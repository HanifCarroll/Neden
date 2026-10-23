# Contributing to Neden

For a small bug fix, open a pull request with a reproduction and the relevant check results. For a substantial feature or architectural change, open an issue first so we can agree on scope. Keep contributions focused and preserve existing playback and study behavior.

## Getting started

Follow the [README](README.md) for your target. The desktop player and IINA plugin have separate runtime contracts; read [development conventions](docs/development.md) and the relevant architecture/specification before changing either. Development does not require the maintainer's accounts, certificates, media, or private test machines.

Use descriptive names, straightforward control flow, and explicit ownership. Keep platform-specific code in adapters and UI state separate from playback, persistence, and requests. Prefer an existing dependency or abstraction when it fits; explain new dependencies and preserve their licenses.

## Checks

- Desktop logic: `sh scripts/desktop.sh core-test`.
- Full desktop build/tests: `sh scripts/desktop.sh test`, then `sh scripts/desktop.sh lint`.
- Build and packaging helpers: `python3 -m unittest discover -s scripts -p 'test_*.py'`.
- IINA plugin: `bun install --frozen-lockfile`, then `bun run release:check` on macOS.
- Secrets: `gitleaks git --redact --log-opts="--all"` with Gitleaks 8.30.1 or newer.

Add a regression check when fixing behavior that can recur. Report what you tested and what remains unverified. Headless checks do not prove GPU rendering, native fullscreen, hardware audio changes, or OS credential permissions. Use the [isolated native checks](docs/desktop/INSTALLATION.md#isolated-native-checks) on a dedicated display when a change needs them. Routine checks must not use saved credentials or billable endpoints.

## Pull requests

Explain the problem, resulting behavior, and relevant validation. Include screenshots for visible changes using synthetic or openly licensed media. Describe the license and provenance of any new asset or adapted code. Keep unrelated formatting and generated outputs out of the patch.

Never commit API keys, private certificates, personal profiles, complete commercial subtitles, media collections, or local benchmark logs. Inspect `git diff --cached` before committing. Removing a secret in a later commit does not remove it from history; follow [SECURITY.md](SECURITY.md) if that happens.

By submitting a contribution, you agree to license your original contribution under this project's MIT license. Third-party materials retain their own licenses and must be identified. Be respectful and keep discussion focused on the work.
