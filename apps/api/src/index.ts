import { Hono } from "hono";
import { cors } from "hono/cors";
import { ServiceError } from "./contracts";
import { authenticate, hasSandboxAccess, requireSandboxAccess } from "./auth";
import { identity, desktopClientId } from "./identity";
import { billingPortal, stripeClient } from "./billing";
import { boundedText } from "./http";
import { study } from "./study";
import type { Env } from "./environment";
import Stripe from "stripe";
export { AccountUsage } from "./usage";

const app = new Hono<{
  Bindings: Env;
  Variables: { account: { id: string; subject: string } };
}>();
app.use("*", async (c, next) => {
  c.header("Cache-Control", "no-store");
  c.header("X-Content-Type-Options", "nosniff");
  c.header("Referrer-Policy", "no-referrer");
  await next();
});
app.use("*", (c, next) =>
  cors({
    origin: c.env.WEBSITE_ORIGIN,
    credentials: true,
    allowMethods: ["GET", "POST", "DELETE"],
    allowHeaders: ["Content-Type", "Authorization", "Idempotency-Key"],
    maxAge: 600,
  })(c, next),
);
app.get("/health", (c) => c.json({ service: "neden-api", version: 1 }));
app.on(["GET", "POST"], "/api/auth/*", (c) =>
  identity(c.env).handler(c.req.raw),
);
app.get("/v1/config", (c) =>
  c.json({
    issuer: `${c.env.API_ORIGIN}/api/auth`,
    audience: c.env.API_ORIGIN,
    desktopClientId,
    website: c.env.WEBSITE_ORIGIN,
    billingAvailable:
      !!c.env.STRIPE_PRICE_ID &&
      !!c.env.STRIPE_SECRET_KEY &&
      !!c.env.DEEPSEEK_API_KEY &&
      !!c.env.STRIPE_WEBHOOK_SECRET &&
      c.env.AI_ENABLED === "true",
    sandbox: c.env.BILLING_LIVE !== "true",
  }),
);
app.post("/v1/billing/webhook", async (c) => {
  if (!c.env.STRIPE_WEBHOOK_SECRET)
    throw new ServiceError(503, "not_configured", "Webhook is not configured.");
  const body = await boundedText(c.req.raw, 256000);
  let event: Stripe.Event;
  try {
    event = await stripeClient(c.env).webhooks.constructEventAsync(
      body,
      c.req.header("stripe-signature") ?? "",
      c.env.STRIPE_WEBHOOK_SECRET,
      undefined,
      Stripe.createSubtleCryptoProvider(),
    );
  } catch {
    throw new ServiceError(
      400,
      "invalid_signature",
      "Invalid webhook signature.",
    );
  }
  if (event.livemode !== (c.env.BILLING_LIVE === "true"))
    throw new ServiceError(
      400,
      "wrong_environment",
      "Wrong billing environment.",
    );
  if (
    await c.env.DB.prepare(
      "SELECT event_id FROM billing_events WHERE event_id = ?",
    )
      .bind(event.id)
      .first()
  )
    return c.json({ received: true });
  const object = event.data.object as unknown as {
    customer?: string | { id: string };
  };
  const customer =
    typeof object.customer === "string" ? object.customer : object.customer?.id;
  if (customer) {
    const account = await c.env.DB.prepare(
      "SELECT account_id, deletion_requested_at FROM billing_accounts WHERE stripe_customer_id = ?",
    )
      .bind(customer)
      .first<{ account_id: string; deletion_requested_at: number | null }>();
    if (account && !account.deletion_requested_at) {
      const usage = c.env.ACCOUNT_USAGE.get(
        c.env.ACCOUNT_USAGE.idFromName(account.account_id),
      );
      // Fetch current Stripe state, rather than trusting event delivery order.
      await usage.synchronizeBilling(customer);
    }
  }
  await c.env.DB.prepare(
    "INSERT OR IGNORE INTO billing_events (event_id) VALUES (?)",
  )
    .bind(event.id)
    .run();
  return c.json({ received: true });
});
app.use("/v1/*", async (c, next) => {
  c.set("account", await authenticate(c.req.raw, c.env));
  await next();
});
app.get("/v1/account", async (c) => {
  const accountId = c.get("account").id;
  const usage = c.env.ACCOUNT_USAGE.get(
    c.env.ACCOUNT_USAGE.idFromName(accountId),
  );
  const billing = await c.env.DB.prepare(
    "SELECT stripe_customer_id FROM billing_accounts WHERE account_id = ?",
  )
    .bind(accountId)
    .first<{ stripe_customer_id: string | null }>();
  return c.json({
    ...(await usage.summary()),
    hasBillingAccount: !!billing?.stripe_customer_id,
    canSubscribe: hasSandboxAccess(c.env, accountId),
  });
});
app.post("/v1/study", (c) => {
  requireSandboxAccess(c.env, c.get("account").id);
  return study(c.req.raw, c.env, c.get("account").id, c.executionCtx);
});
app.post("/v1/billing/checkout", async (c) => {
  const id = c.get("account").id;
  requireSandboxAccess(c.env, id);
  return c.json(
    await c.env.ACCOUNT_USAGE.get(
      c.env.ACCOUNT_USAGE.idFromName(id),
    ).createCheckout(id),
  );
});
app.post("/v1/billing/portal", async (c) =>
  c.json(await billingPortal(c.env, c.get("account").id)),
);
app.onError((error, c) => {
  if (error instanceof ServiceError)
    return c.json(
      { error: { code: error.code, message: error.message } },
      error.status as 400,
    );
  if (error instanceof SyntaxError)
    return c.json(
      { error: { code: "invalid_json", message: "Invalid JSON." } },
      400,
    );
  // Raw SDK errors can contain private data; return and log only a correlation ID.
  const requestId = crypto.randomUUID();
  console.error(JSON.stringify({ event: "request_failed", requestId }));
  return c.json(
    {
      error: {
        code: "service_error",
        message: "The service could not complete this request.",
        requestId,
      },
    },
    503,
  );
});
app.notFound((c) =>
  c.json({ error: { code: "not_found", message: "Unknown endpoint." } }, 404),
);
export default app;
