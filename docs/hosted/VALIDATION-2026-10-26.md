# Hosted preview validation — 2026-10-26

Environment: invitation-only Cloudflare staging, Stripe sandbox, real DeepSeek.
No live charge or public subscription launch. Test identities and credentials are
kept in ignored local audit files, not in this repository.

## Completed

- Verified signup email delivered through the configured sender; browser verification
  and signed-in account page worked in background Chrome.
- Actual sandbox card Checkout created a subscription. Signed webhook delivery
  granted the configured 1,000 word and 300 explanation/follow-up allowances.
- Stripe Portal cancellation preserved access until the period end. Renewing and
  canceling again exercised state updates. The final test subscription is canceled
  at period end. Flexible-billing `cancel_at` is covered by a regression test.
- Real hosted word and streamed explanation requests succeeded. Reusing a request
  ID returned 409. Failed provider requests exposed a Workers redirect incompatibility;
  manual redirect handling now rejects redirects without forwarding credentials.
- A headless native check used the actual StudyServices, word-card parser and
  explanation session: browser PKCE authorization, saved-token refresh, word lookup,
  streamed explanation and contextual follow-up all passed. Sign-out requested
  token revocation. Repeating the complete flow from a signed-out browser also
  passed. The check uses an in-memory credential store, not the OS keychain.
- Thirteen desktop test suites and QML lint passed. API tests cover identity,
  allowances and billing with mocked external services; the real checks above
  supplement them. Unit tests isolate local secret files from the test runtime.

## Reproduction

Run `bun run check:api`, `bun run check:website` and `bun run test:desktop`.
`hosted_service_check` is an opt-in headless executable built with the desktop tests.
Pass an ignored owner-only output path for its authorization link; open that link
in a browser with an invited, verified, subscribed staging account. It performs real
provider requests and consumes test allowance. It never prints tokens or passwords.
Its exit code is zero on success. It is deliberately excluded from unattended CI.

## Native runtime finding

The Pro launch check caught a read-only `Flow.implicitHeight` assignment in the
new settings component. Unit tests and QML lint did not catch it. Commit `14ca4d4`
removes that assignment so Qt supplies the flow layout height. Actual native launch
is therefore required in addition to the checks above.

## Remaining launch gates

Native GUI and real OS-keychain persistence need platform validation beyond the
headless check. Production requires separate live Stripe configuration, a confirmed
price and allowance, public service terms/privacy, support, error/spend monitoring
and a recovery drill. Recurring renewal and payment-failure behavior have automated
coverage, but have not been exercised through a real accelerated Stripe test clock.
The pricing proposal is not an approved public offer.
