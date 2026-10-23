# Security and privacy

Neden is under active development. Security fixes target the current `main` branch; there is no separately maintained stable release series.

## Reporting a vulnerability

Send a private report to **hanif@hanifcarroll.com** with the affected revision, reproduction, impact, and a minimal synthetic example. Do not include real API keys or personal media. Please report vulnerabilities privately before opening a public issue. Ordinary bugs without sensitive details can use GitHub Issues.

If a real credential was committed or exposed, revoke or rotate it at the provider first. Rewriting history does not revoke credentials or erase existing clones and caches. Coordinate any history rewrite with the maintainer; see [GitHub's removal guidance](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/removing-sensitive-data-from-a-repository).

## What stays local and what leaves the device

- Video playback and local subtitle parsing use your files. History contains paths, positions, and playback choices; study cards and settings also remain on disk.
- AI explanations and word lookups send selected dialogue and configured context to the provider endpoint you choose. Follow-up requests include conversation context. Providers may charge for requests.
- Subtitle search/download and dictionary navigation contact external services when used. Check their terms and privacy policies.
- Desktop credentials use QtKeychain with insecure fallback disabled. The plugin uses IINA's native Keychain integration. Credentials must not be copied into preferences, screenshots, logs, fixtures, subprocess arguments, or reports.

Use a separate `--profile` directory and mock services for testing. Profiles, playback history, subtitle downloads, screenshots, and crash logs can contain private information even when they contain no API key. Inspect and redact them before sharing. Treat subtitle text, model output, media metadata, and external web content as untrusted input.

## Dependency and release boundaries

Dependencies fetched by build scripts are pinned and checked where supported. The playback-runtime guard checks the chosen runtime configuration; it is not a comprehensive security or license audit. Do not substitute an arbitrary runtime without reviewing the build and distribution consequences.

Local macOS builds may use ad hoc signing. Public binary distribution requires additional dependency materials, signing, and platform testing described in [the dependency guide](docs/desktop/DEPENDENCIES.md). A passing source test suite is not a notarization or production security guarantee.
