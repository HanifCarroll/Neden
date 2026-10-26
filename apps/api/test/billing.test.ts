import { afterEach, describe, expect, it, vi } from "vitest";
import { env } from "cloudflare:test";
import { checkout, retrieveEntitlement, stripeClient } from "../src/billing";
import type { Env } from "../src/environment";

const settings: Env = {
  ...env,
  STRIPE_SECRET_KEY: "sk_test_not_a_real_credential",
  STRIPE_PRICE_ID: "price_pilot",
  STRIPE_WEBHOOK_SECRET: "mock-webhook-signing-secret",
};
afterEach(() => vi.restoreAllMocks());
function subscription(status: string, price = "price_pilot") {
  return {
    id: "sub_example",
    status,
    created: 100,
    cancel_at_period_end: true,
    items: {
      data: [
        {
          price: { id: price },
          current_period_start: 100,
          current_period_end: 200,
        },
      ],
    },
  };
}
function subscriptions(data: unknown[]) {
  return vi
    .spyOn(globalThis, "fetch")
    .mockResolvedValue(
      Response.json({ object: "list", data, has_more: false }),
    );
}
describe("billing decisions", () => {
  it("blocks checkout before the hosted service is ready", async () => {
    await expect(checkout(settings, "unused")).rejects.toMatchObject({
      code: "billing_unavailable",
    });
  });
  it("rejects live credentials in a sandbox deployment", () => {
    expect(() =>
      stripeClient({
        ...settings,
        STRIPE_SECRET_KEY: "sk_live_not_a_real_credential",
      }),
    ).toThrow();
  });
  it("allows only active subscriptions for the configured price", async () => {
    subscriptions([
      subscription("active", "price_unrelated"),
      subscription("past_due"),
    ]);
    expect((await retrieveEntitlement(settings, "cus_test")).enabled).toBe(
      false,
    );
  });
  it("preserves access until a period-end cancellation takes effect", async () => {
    subscriptions([subscription("active")]);
    expect(await retrieveEntitlement(settings, "cus_test")).toMatchObject({
      enabled: true,
      periodStart: 100,
      periodEnd: 200,
      cancelAtPeriodEnd: true,
    });
  });
  it("prevents a second subscription while an existing payment needs attention", async () => {
    const id = crypto.randomUUID();
    await settings.DB.prepare(
      "INSERT INTO billing_accounts (account_id, stripe_customer_id) VALUES (?, ?)",
    )
      .bind(id, `cus_${id}`)
      .run();
    const mock = subscriptions([subscription("past_due")]);
    await expect(
      checkout(
        {
          ...settings,
          AI_ENABLED: "true",
          DEEPSEEK_API_KEY: "mock-provider-key",
        },
        id,
      ),
    ).rejects.toMatchObject({ code: "subscription_exists" });
    expect(mock).toHaveBeenCalledTimes(1);
  });
  it("reuses an open checkout instead of starting another payment flow", async () => {
    const id = crypto.randomUUID();
    await settings.DB.prepare(
      "INSERT INTO billing_accounts (account_id, stripe_customer_id) VALUES (?, ?)",
    )
      .bind(id, `cus_${id}`)
      .run();
    const mock = vi
      .spyOn(globalThis, "fetch")
      .mockResolvedValueOnce(
        Response.json({ object: "list", data: [], has_more: false }),
      )
      .mockResolvedValueOnce(
        Response.json({
          object: "list",
          data: [
            {
              mode: "subscription",
              client_reference_id: id,
              url: "https://checkout.stripe.com/example",
            },
          ],
          has_more: false,
        }),
      );
    expect(
      await checkout(
        {
          ...settings,
          AI_ENABLED: "true",
          DEEPSEEK_API_KEY: "mock-provider-key",
        },
        id,
      ),
    ).toEqual({ url: "https://checkout.stripe.com/example" });
    expect(mock).toHaveBeenCalledTimes(2);
  });
});
