import { DurableObject } from "cloudflare:workers";
import type { Env } from "./environment";
import type { Entitlement, Operation } from "./contracts";
import { ServiceError } from "./contracts";
import { retrieveEntitlement, checkout, stripeClient } from "./billing";

interface UsageRow extends Record<string, string | number> {
  request_id: string;
  bucket: string;
  period_start: number;
  state: string;
  expires_at: number;
}

// This object alone owns an account's spending decisions. D1 never authorizes AI.
export class AccountUsage extends DurableObject<Env> {
  constructor(ctx: DurableObjectState, env: Env) {
    super(ctx, env);
    ctx.storage.sql.exec(`CREATE TABLE IF NOT EXISTS reservations (
      request_id TEXT PRIMARY KEY, bucket TEXT NOT NULL, period_start INTEGER NOT NULL,
      state TEXT NOT NULL, expires_at INTEGER NOT NULL, input_tokens INTEGER, output_tokens INTEGER)`);
  }
  async synchronizeBilling(customerId: string) {
    return this.ctx.blockConcurrencyWhile(async () => {
      if (await this.ctx.storage.get("closed")) return;
      await this.updateEntitlement(
        await retrieveEntitlement(this.env, customerId),
      );
    });
  }
  async createCheckout(accountId: string) {
    return this.ctx.blockConcurrencyWhile(async () => {
      return checkout(this.env, accountId);
    });
  }
  async globalBudget() {
    return this.ctx.storage.transaction(async () => {
      const day = Math.floor(Date.now() / 86400000);
      const previous = await this.ctx.storage.get<{
        day: number;
        count: number;
      }>("global-budget");
      const count = previous?.day === day ? previous.count : 0;
      const limit = Number(this.env.GLOBAL_DAILY_REQUEST_LIMIT);
      if (!Number.isSafeInteger(limit) || limit < 1 || count >= limit)
        return false;
      await this.ctx.storage.put("global-budget", { day, count: count + 1 });
      return true;
    });
  }
  async updateEntitlement(entitlement: Entitlement) {
    if (await this.ctx.storage.get("closed")) return;
    await this.ctx.storage.put("entitlement", entitlement);
  }
  async summary() {
    const entitlement = await this.ctx.storage.get<Entitlement>("entitlement");
    const counts = this.counts(entitlement?.periodStart ?? 0);
    return {
      subscribed:
        !!entitlement?.enabled && entitlement.periodEnd > Date.now() / 1000,
      periodEnd: entitlement?.periodEnd ?? null,
      cancelAtPeriodEnd: entitlement?.cancelAtPeriodEnd ?? false,
      words: { used: counts.word, limit: entitlement?.wordLimit ?? 0 },
      explanations: {
        used: counts.explanation,
        limit: entitlement?.explanationLimit ?? 0,
      },
    };
  }
  private counts(periodStart: number) {
    const result = { word: 0, explanation: 0 };
    for (const row of this.ctx.storage.sql.exec<{
      bucket: "word" | "explanation";
      count: number;
    }>(
      "SELECT bucket, COUNT(*) AS count FROM reservations WHERE period_start = ? AND state != 'released' GROUP BY bucket",
      periodStart,
    ))
      result[row.bucket] = row.count;
    return result;
  }
  async reserve(requestId: string, operation: Operation) {
    try {
      await this.ctx.storage.transaction(async () => {
        const entitlement =
          await this.ctx.storage.get<Entitlement>("entitlement");
        if (!entitlement?.enabled || entitlement.periodEnd <= Date.now() / 1000)
          throw new ServiceError(
            403,
            "subscription_required",
            "An active subscription is required.",
          );
        const prior = this.ctx.storage.sql
          .exec<UsageRow>(
            "SELECT * FROM reservations WHERE request_id = ?",
            requestId,
          )
          .toArray()[0];
        // Never resend the same request to a paid provider, even after a disconnect.
        if (prior)
          throw new ServiceError(
            409,
            "duplicate_request",
            "This request was already submitted.",
          );
        const active = this.ctx.storage.sql
          .exec<{ count: number }>(
            "SELECT COUNT(*) AS count FROM reservations WHERE state = 'reserved' AND expires_at > ?",
            Date.now(),
          )
          .one().count;
        if (active >= 2)
          throw new ServiceError(
            429,
            "busy",
            "Wait for an active answer to finish.",
          );
        const bucket = operation === "word" ? "word" : "explanation";
        const limit =
          bucket === "word"
            ? entitlement.wordLimit
            : entitlement.explanationLimit;
        if (
          !Number.isSafeInteger(limit) ||
          limit < 1 ||
          this.counts(entitlement.periodStart)[bucket] >= limit
        )
          throw new ServiceError(
            429,
            "allowance_exhausted",
            "Your monthly allowance has been used.",
          );
        this.ctx.storage.sql.exec(
          "INSERT INTO reservations VALUES (?, ?, ?, ?, ?, NULL, NULL)",
          requestId,
          bucket,
          entitlement.periodStart,
          "reserved",
          Date.now() + 180000,
        );
        await this.ctx.storage.setAlarm(Date.now() + 240000);
      });
      return { ok: true as const };
    } catch (error) {
      if (error instanceof ServiceError)
        return {
          ok: false as const,
          status: error.status,
          code: error.code,
          message: error.message,
        };
      throw error;
    }
  }
  async finish(
    requestId: string,
    inputTokens: number | null,
    outputTokens: number | null,
    release = false,
  ) {
    this.ctx.storage.sql.exec(
      "UPDATE reservations SET state = ?, input_tokens = ?, output_tokens = ? WHERE request_id = ? AND state = 'reserved'",
      release ? "released" : "charged",
      inputTokens,
      outputTokens,
      requestId,
    );
  }
  async alarm() {
    // Unknown provider outcomes consume the reserved operation; never refund on a crash.
    this.ctx.storage.sql.exec(
      "UPDATE reservations SET state = 'charged' WHERE state = 'reserved' AND expires_at <= ?",
      Date.now(),
    );
    this.ctx.storage.sql.exec(
      "DELETE FROM reservations WHERE expires_at < ?",
      Date.now() - 100 * 86400000,
    );
    const pending = this.ctx.storage.sql
      .exec<{ count: number }>(
        "SELECT COUNT(*) AS count FROM reservations WHERE state = 'reserved'",
      )
      .one().count;
    if (pending) await this.ctx.storage.setAlarm(Date.now() + 240000);
    else {
      const oldest = this.ctx.storage.sql
        .exec<{ expires_at: number }>(
          "SELECT expires_at FROM reservations ORDER BY expires_at LIMIT 1",
        )
        .toArray()[0];
      if (oldest)
        await this.ctx.storage.setAlarm(oldest.expires_at + 100 * 86400000 + 1);
    }
  }
  async closeAccount(accountId: string) {
    return this.ctx.blockConcurrencyWhile(async () => {
      const account = await this.env.DB.prepare(
        "SELECT stripe_customer_id FROM billing_accounts WHERE account_id = ?",
      )
        .bind(accountId)
        .first<{ stripe_customer_id: string | null }>();
      if (account?.stripe_customer_id) {
        const stripe = stripeClient(this.env);
        for await (const subscription of stripe.subscriptions.list({
          customer: account.stripe_customer_id,
          status: "all",
          limit: 100,
        })) {
          if (!["canceled", "incomplete_expired"].includes(subscription.status))
            await stripe.subscriptions.cancel(subscription.id);
        }
      }
      // Tombstone prevents delayed webhooks or queued requests reactivating this account.
      await this.env.DB.prepare(
        `INSERT INTO billing_accounts (account_id, deletion_requested_at) VALUES (?, unixepoch())
        ON CONFLICT(account_id) DO UPDATE SET deletion_requested_at = unixepoch()`,
      )
        .bind(accountId)
        .run();
      await this.ctx.storage.put("closed", true);
      await this.erase();
    });
  }
  async erase() {
    this.ctx.storage.sql.exec("DELETE FROM reservations");
    await this.ctx.storage.delete(["entitlement", "checkout"]);
    await this.ctx.storage.deleteAlarm();
  }
}
