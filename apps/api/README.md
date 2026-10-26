# Optional hosted Neden service

Experimental Cloudflare Worker for Better Auth accounts, Stripe subscriptions,
and metered DeepSeek requests. The desktop player remains free and independent.
**This is an invitation-only staging preview, not a launched subscription service.**

## Current coverage

Implemented: verified email/password sign-in, recovery, sign-out, account deletion,
OAuth authorization code with PKCE for a registered native client, sandbox billing
routes, bounded AI requests, streaming responses, and atomic allowances.

Validated against the deployed staging service: verification email delivery,
Stripe sandbox Checkout and Portal, signed subscription/cancellation webhooks,
hosted DeepSeek word lookups, streamed explanations and follow-ups, and the native
client's browser authorization and refresh-token flow. See
[the validation record](../../docs/hosted/VALIDATION-2026-10-26.md) for limits.

Sandbox accounts require an explicit `SANDBOX_ACCOUNT_IDS` invitation for checkout
and AI. Fake payments never grant arbitrary accounts access to real provider spend.
`AI_ENABLED=false` blocks provider calls and new subscriptions; missing credentials
also fail closed. The desktop connection is optional; local playback and existing
API-key mode remain independent.

## Architecture

- `identity.ts` owns Better Auth and email delivery. D1 stores accounts, hashed
  passwords, sessions, verification records, OAuth grants, and signing keys.
- `auth.ts` verifies cookies or short-lived RS256 OAuth tokens with the expected
  issuer, audience, and `study` scope. Password recovery revokes refresh tokens and
  existing access tokens. Deleted users cannot access the API.
- `billing.ts` maps account IDs to Stripe customers. Only an active subscription
  with the configured price grants access. Checkout reuses an open session and
  rejects an existing subscription, including one awaiting payment.
- `usage.ts` owns each account's allowance in a SQLite Durable Object. It serializes
  reservations, limits concurrent requests, and rejects reused request IDs. A
  separate object caps daily requests across the deployment.
- `study.ts`, `contracts.ts`, and `provider.ts` own request validation and the paid
  request lifecycle. The server chooses the provider, model, prompts, and output
  limits. Definitive provider rejection restores allowance; uncertain outcomes,
  including disconnects, remain charged to prevent retries from bypassing limits.
- `prompts.ts` contains a snapshot of the desktop teaching instructions. Review
  both copies when changing prompts; they are not automatically synchronized.

No prompt, answer, video, or subtitle file is persisted by the service. Operation
IDs and token counts support allowance enforcement. Usage retention is 100 days;
alarms also clean up inactive accounts. Deletion retains a minimal billing tombstone
and event IDs so delayed webhooks cannot reactivate the account. Payment records
remain with Stripe. Cloudflare infrastructure metadata and provider retention are
separate from application storage.

## Local checks

From the repository root:

```sh
bun install --frozen-lockfile
bun run check:api
bun run check:website
```

Tests run in Cloudflare's local runtime with isolated D1 and Durable Objects.
Email and provider/payment calls are mocked. No credentials or paid calls are
needed for the tests. Tests exercise verification, recovery, deletion, PKCE,
code reuse, scope boundaries, quota concurrency, and billing state decisions.

For interactive local development, copy `.env.example` to ignored `.env` or `.dev.vars`,
set a fresh local auth secret, and run `bun run dev` here. Local email needs a
configured delivery binding or a development-only mail adapter. Do not return
verification links from public endpoints or log their contents.

## Deployment

The checked-in staging environment is the maintainer's deployment. Other operators
must supply their own domains, Cloudflare D1 IDs, email sender, and Stripe price.
None of those public identifiers is a credential. Never copy production data or
secrets into test fixtures.

1. Create D1 and the email sending subdomain. Verify DNS and bind `EMAIL` to an
   allowed sender. Disable email body preview/logging.
2. Set matching `API_ORIGIN` and `WEBSITE_ORIGIN` values; add the API custom domain.
   Keep `BILLING_LIVE=false` and `AI_ENABLED=false` for initial setup.
3. Apply D1 migrations with `bunx wrangler d1 migrations apply neden-staging --remote --env staging`.
4. Store `BETTER_AUTH_SECRET` (at least 32 random characters), `STRIPE_SECRET_KEY`,
   `STRIPE_WEBHOOK_SECRET`, and `DEEPSEEK_API_KEY` using Worker secrets. Never put
   them in tracked configuration or browser environment variables.
5. Register the desktop client with `bun scripts/register-desktop.ts staging`
   from this directory. This trusted deployment command is idempotent. It is a public native client with PKCE, no client secret,
   and only the loopback callback `http://127.0.0.1/oauth/callback`. Better Auth
   permits an ephemeral loopback port. Registration is not a public API route.
   Set `SANDBOX_ACCOUNT_IDS` as a Worker secret containing invited account IDs
   separated by commas. Keep the invitation list out of source control.
6. Deploy with `bun run deploy:staging`. Build the website with `PUBLIC_API_URL`
   pointing to that API; `bun run build:staging` in `apps/website` uses the checked-in
   staging origin. Then deploy its `staging` environment.
7. Configure the sandbox Stripe Billing Portal and a signed webhook at
   `/v1/billing/webhook`. Subscribe to `customer.subscription.created`,
   `customer.subscription.updated`, `customer.subscription.deleted`,
   `checkout.session.completed`, `invoice.paid`, and `invoice.payment_failed`.
   The handler retrieves current Stripe state instead of trusting event ordering.
8. Validate email, billing, cancellation, webhook retries, hosted AI, and the native
   login/refresh/logout lifecycle before opening subscriptions. The sandbox price
   is an experiment, not an approved public offer. Live launch needs separate live
   secrets and prices, accurate public privacy/terms, monitoring, recovery drills,
   and native credential-store and platform release validation.

Worker logs deliberately exclude request payloads and raw SDK errors. Unexpected
failures expose a correlation ID only. Add error-rate monitoring before production;
staging currently has general Worker observability disabled.

## API contract

`GET /v1/config` is public and contains only origins, client ID, sandbox state, and
whether checkout is available. `/api/auth/*` is handled by Better Auth.

Authenticated routes:

- `GET /v1/account`: subscription and allowance counts, plus billing-account presence.
- `POST /v1/billing/checkout`: sandbox subscription Checkout URL.
- `POST /v1/billing/portal`: customer Portal URL, including when payment is overdue.
- `POST /v1/study`: `operation` is `word`, `explain`, or `followup`, with bounded
  subtitle context; follow-ups include completed history. Every request supplies a
  unique `Idempotency-Key`. A reused key returns 409 rather than spending again.

Web requests use credentialed cookies and an exact allowed Origin for writes.
Native clients use an OAuth bearer token; tokens never enter application preferences.
Refresh tokens are saved through QtKeychain with insecure fallback disabled. Access
tokens stay in memory. Each API origin has a separate credential-store namespace.

## Staging setup record

On 2026-10-26 the D1 schema, email sender, API and account website were deployed.
Stripe sandbox billing uses a dedicated restricted key, signed webhook and configured
Customer Portal. The provider key and billing secrets are Worker secrets; ignored
local copies have owner-only permissions. No live payment was made.

The test subscription was canceled through Stripe Portal. Its webhook preserved
access through the paid test period and correctly changed the account's renewal
label to an end date. Flexible billing can use `cancel_at` without setting
`cancel_at_period_end`; the entitlement code and regression test cover both.
