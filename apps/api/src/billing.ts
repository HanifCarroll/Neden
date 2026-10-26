import Stripe from "stripe";
import type { Env } from "./environment";
import type { Entitlement } from "./contracts";
import { ServiceError } from "./contracts";

export function stripeClient(env: Env) {
  const key = env.STRIPE_SECRET_KEY;
  if (!key || (env.BILLING_LIVE !== "true" && !/^(sk|rk)_test_/.test(key)))
    throw new ServiceError(
      503,
      "billing_unavailable",
      "Billing is not available in this environment yet.",
    );
  return new Stripe(key, {
    httpClient: Stripe.createFetchHttpClient(),
    maxNetworkRetries: 1,
    timeout: 10000,
  });
}
export async function customerForAccount(env: Env, accountId: string) {
  const account = await env.DB.prepare(
    "SELECT stripe_customer_id, deletion_requested_at FROM billing_accounts WHERE account_id = ?",
  )
    .bind(accountId)
    .first<{
      stripe_customer_id: string | null;
      deletion_requested_at: number | null;
    }>();
  if (account?.deletion_requested_at)
    throw new ServiceError(
      403,
      "account_deleted",
      "This account is being deleted.",
    );
  if (account?.stripe_customer_id) return account.stripe_customer_id;
  const customer = await stripeClient(env).customers.create(
    { metadata: { nedenAccount: accountId } },
    { idempotencyKey: `customer-${accountId}` },
  );
  await env.DB.prepare(
    "INSERT INTO billing_accounts (account_id, stripe_customer_id) VALUES (?, ?) ON CONFLICT(account_id) DO UPDATE SET stripe_customer_id = excluded.stripe_customer_id",
  )
    .bind(accountId, customer.id)
    .run();
  return customer.id;
}
export async function retrieveEntitlement(
  env: Env,
  customerId: string,
): Promise<Entitlement> {
  let subscription: Stripe.Subscription | undefined;
  // Stripe returns newest first. Iterate pages so old active grants remain visible.
  for await (const candidate of stripeClient(env).subscriptions.list({
    customer: customerId,
    status: "all",
    limit: 100,
  })) {
    if (
      candidate.status === "active" &&
      candidate.items.data.some((item) => item.price.id === env.STRIPE_PRICE_ID)
    ) {
      subscription = candidate;
      break;
    }
  }
  const item = subscription?.items.data.find(
    (item) => item.price.id === env.STRIPE_PRICE_ID,
  );
  return {
    subscriptionId: subscription?.id ?? "",
    enabled: !!subscription && !!item,
    periodStart: item?.current_period_start ?? 0,
    periodEnd: Math.min(
      item?.current_period_end ?? 0,
      subscription?.cancel_at ?? Number.POSITIVE_INFINITY,
    ),
    // Flexible billing uses cancel_at even for a portal period-end cancellation.
    cancelAtPeriodEnd: !!(
      subscription?.cancel_at_period_end || subscription?.cancel_at
    ),
    wordLimit: Number(env.MONTHLY_WORD_LIMIT),
    explanationLimit: Number(env.MONTHLY_EXPLANATION_LIMIT),
  };
}
export async function checkout(env: Env, accountId: string) {
  if (
    env.AI_ENABLED !== "true" ||
    !env.DEEPSEEK_API_KEY ||
    !env.STRIPE_WEBHOOK_SECRET ||
    !env.STRIPE_PRICE_ID
  )
    throw new ServiceError(
      503,
      "billing_unavailable",
      "Subscriptions are not open yet.",
    );
  const stripe = stripeClient(env);
  const customer = await customerForAccount(env, accountId);
  for await (const existing of stripe.subscriptions.list({
    customer,
    status: "all",
    limit: 100,
  })) {
    if (
      [
        "active",
        "trialing",
        "past_due",
        "unpaid",
        "incomplete",
        "paused",
      ].includes(existing.status)
    )
      throw new ServiceError(
        409,
        "subscription_exists",
        "Manage your existing subscription in Billing.",
      );
  }
  const pending = await stripe.checkout.sessions.list({
    customer,
    status: "open",
    limit: 10,
  });
  const reusable = pending.data.find(
    (session) =>
      session.mode === "subscription" &&
      session.client_reference_id === accountId,
  );
  if (reusable?.url) return { url: reusable.url };
  const session = await stripe.checkout.sessions.create(
    {
      mode: "subscription",
      customer,
      line_items: [{ price: env.STRIPE_PRICE_ID, quantity: 1 }],
      success_url: `${env.WEBSITE_ORIGIN}/account?checkout=complete`,
      cancel_url: `${env.WEBSITE_ORIGIN}/account`,
      client_reference_id: accountId,
      subscription_data: { metadata: { nedenAccount: accountId } },
    },
    {
      idempotencyKey: `checkout-${accountId}-${Math.floor(Date.now() / 1800000)}`,
    },
  );
  return { url: session.url };
}
export async function billingPortal(env: Env, accountId: string) {
  const account = await env.DB.prepare(
    "SELECT stripe_customer_id FROM billing_accounts WHERE account_id = ? AND deletion_requested_at IS NULL",
  )
    .bind(accountId)
    .first<{ stripe_customer_id: string | null }>();
  if (!account?.stripe_customer_id)
    throw new ServiceError(
      404,
      "billing_account_missing",
      "No billing account exists yet.",
    );
  const session = await stripeClient(env).billingPortal.sessions.create({
    customer: account.stripe_customer_id,
    configuration: env.STRIPE_PORTAL_CONFIGURATION_ID,
    return_url: `${env.WEBSITE_ORIGIN}/account`,
  });
  return { url: session.url };
}
