# Optional Neden subscription

Implemented as an invitation-only staging preview, October 26, 2026. It is not
offered for sale. Better Auth owns identity; Cloudflare hosts the API and storage.
See the [deployment runbook](../apps/api/README.md) and
[validation record](hosted/VALIDATION-2026-10-26.md). The pricing below remains a
hypothesis for a future paid launch.

## Experience

Keep the open-source player and bring-your-own-key option. Add an optional
**Neden account — AI included** connection alongside them. A subscriber signs in
through their browser and returns to the desktop app ready to look up words,
explain lines, and ask follow-ups. They never see provider keys or token bills.

The website handles signup, checkout and account management. Video playback stays
local. Only the subtitle context and questions needed for a request reach the
service. This delivers the subscription benefit without first rebuilding the
player for a browser or uploading users' videos.

## Components and ownership

| Component | Responsibility |
| --- | --- |
| Desktop account connection | Browser-based authorization with PKCE, refresh credentials in the OS keychain, sign-out, and reconnect; existing local playback remains independent. |
| Website account pages | Sign-in, plan and allowance, billing portal, account deletion and support. |
| Cloudflare Worker API | Authenticated word lookup, explanation and follow-up endpoints; server-owned prompts, bounded context/output, streaming and cancellation. |
| Account database | Subscription state, plan versions, billing customer IDs, usage ledger and deletion state. |
| Atomic usage reservation | Per-account concurrency and allowance checks before paid work starts; reconcile reservations against actual provider usage afterward. A Durable Object or database transaction can own this boundary. |
| AI adapter | Server-only provider secrets, permitted model/configuration, deadlines and bounded retries. It accepts Neden operations rather than arbitrary provider requests. |
| Stripe Billing | Subscription Checkout and Customer Portal; verified, idempotent webhook processing updates access on payment, renewal, cancellation and failure. |

Prefer an established identity service over custom password handling. A desktop
client is public: it cannot safely hold a shared secret or decide its own paid
status. Validate access on the server for every paid operation. Keep provider
selection behind an interface so model or provider changes do not require clients
to ship secrets or new pricing logic.

Use the existing Cloudflare deployment for the first backend, subject to measuring
streaming latency and storage needs. Isolate account/billing, usage accounting and
AI execution into modules with explicit interfaces. The website and desktop should
use the same versioned API contract.

## Price and cost hypothesis

Start by testing willingness to pay around **US$8–12/month** with a clear monthly
study allowance. This is a hypothesis, not a settled price. Describe allowance in
word lookups and explanations; users should not need to understand tokens. Show
remaining allowance before exhaustion, with no automatic overage charges.

Illustrative monthly AI costs using DeepSeek Flash's October 26, 2026 peak,
uncached prices ($0.30/million input tokens and $1.20/million output tokens):

| Activity | Count | Assumed tokens per request, input / output | Estimated AI cost |
| --- | ---: | ---: | ---: |
| Word lookup | 1,000 | 800 / 120 | $0.384 |
| Line explanation | 200 | 2,000 / 650 | $0.276 |
| Follow-up | 100 | 3,000 / 500 | $0.150 |
| Total | 1,300 | | **$0.81** |

These are assumed request sizes, not measured user usage. Longer conversations,
thinking output, model changes, retries and abuse can raise costs substantially.
The estimate excludes hosting, authentication, payment fees, taxes, refunds and
support. Explicitly configure non-thinking mode for fast lookups. Measure actual
cost distributions, including heavy users, before choosing the allowance and price.

Source: [DeepSeek pricing](https://api-docs.deepseek.com/quick_start/pricing/).

## Cost and privacy controls

- Reserve a bounded maximum cost atomically before each provider call. Use request
  IDs to avoid duplicate billing after retries. Track charged usage even when the
  user disconnects; abort upstream work where possible and reconcile abandoned
  reservations conservatively.
- Apply per-account request/concurrency limits, bounded conversation history and
  output lengths, trial-abuse controls, and a global spending stop. Keep local
  playback available when hosted AI is unavailable or allowance is exhausted.
- Cache only within an account unless explicitly designed and disclosed otherwise.
  Do not put raw subtitles or conversations in routine operational logs. Prefer
  request IDs, timings, token counts and error categories; define retention and
  deletion before inviting customers.
- Verify Stripe webhook signatures and handle duplicate/out-of-order delivery.
  Derive entitlements from server-verified billing state, not the checkout return
  URL. Specify renewal, cancellation, refunds and failed-payment grace periods.

Sources: [Stripe subscription Checkout](https://docs.stripe.com/billing/subscriptions/build-subscriptions),
[subscription webhooks](https://docs.stripe.com/billing/subscriptions/webhooks),
[Customer Portal](https://docs.stripe.com/customer-management).

## Delivery order

1. Implement account sign-in, the server AI adapter and usage ledger behind a
   feature flag. Test with synthetic accounts and mocked provider responses.
2. Integrate Stripe in test mode. Exercise payment authentication, renewal,
   cancellation, failed payment, duplicate webhook and account deletion paths.
3. Run a capped private pilot; measure latency, actual cost per study session,
   failure/retry rates and heavy-user usage. Establish alerts and a support process.
4. Choose the price and allowance from pilot evidence; complete live payment
   onboarding and the service's terms/privacy disclosures; then offer subscriptions.

A browser-only player, media hosting, cloud flashcard sync and mobile apps can be
separate later products. None is required to remove API-key setup from Neden today.
