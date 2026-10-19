# IINA plugin packaging and release

Keep the plugin identifier `io.github.hanifcarroll.iina-language-learning` unchanged to preserve installed settings and Keychain entries. Canonical metadata lives in `apps/iina-plugin/Info.json`; mirror it to root `Info.json`, which IINA reads from the GitHub main branch for update discovery. Packaging checks that the two agree.

From the repository root, run `bun install --frozen-lockfile` and `bun run release:check`. Output goes to ignored `dist/`. The check covers lint, formatting, tests, TypeScript, Swift compilation, package integrity/required notices, and controlled streaming-helper endpoints. The source checkout itself is not an installable plugin.

Before publishing, inspect the archive and history for secrets or user media, verify installation and the full subtitle → explanation → follow-up → resume loop in the actual supported IINA version, and exercise Keychain approval, Stop, close, fullscreen, focus, and subtitle restoration. Native behavior is not established by mocks. Keep the raw-disable limitation documented in the plugin README.

IINA's tested GitHub installer prefers a `.iinaplgz` asset in the repository's latest release. Publish only a separately authorized, verified package, incrementing `version` and integer `ghVersion` together with the update metadata. A source-only GitHub install cannot load the workspace tree. No push, release, installation into the user's normal IINA, or community-list submission is part of a local build check.
