# Neden

A desktop video player for language learning. Watch your own videos with two subtitle tracks, look up words in context, and ask follow-up questions about the dialogue.

Neden includes a standalone **Qt Quick desktop player** and a separate **IINA plugin**.

Visit [neden.hanifcarroll.com](https://neden.hanifcarroll.com) for a walkthrough,
screenshots, and current download availability. Legacy IINA update URLs redirect
to this repository.

## What it does

- Play local videos with audio, picture, subtitle timing, queue, chapter, and seek-preview controls.
- Display primary and secondary subtitles together, search the transcript, and replay individual lines.
- Click a word for its meaning in context, word form, and other senses; explain a line with a translation and word-by-word breakdown.
- Ask follow-up questions and save flashcards locally.
- Resume unfinished videos, browse playback history, and customize keyboard shortcuts.

Bring your own media and subtitles. AI explanations require a configured provider or compatible local endpoint; provider charges may apply. Ordinary local playback does not require an AI account.

## Project status

The desktop player is under active development. **macOS is the primary tested platform.** Preview installers target macOS 13+ on Apple silicon, Windows 10 (1809)+ / 11 on x86-64 (new source builds require 1903+), and Ubuntu 24.04 on x86-64. Windows and Linux remain experimental: installer/startup checks and portable tests do not establish physical GPU, audio, or desktop-integration coverage. See [platform support](docs/desktop/PLATFORMS.md) and the [preview validation record](docs/desktop/releases/0.2.0-preview.1.md).

Downloads, checksums, matching dependency sources, and validation records are published together on [GitHub Releases](https://github.com/HanifCarroll/Neden/releases). The macOS preview is ad-hoc signed and unnotarized; the Windows installer is unsigned. See the [installation guide](https://neden.hanifcarroll.com/installation/) and [dependency and distribution requirements](docs/desktop/DEPENDENCIES.md).

## Build the desktop player on macOS

Requirements: a C++17 compiler, Apple command-line developer tools, **Qt 6.9+**, **CMake 3.24+**, **Python 3.12+**, and libmpv development headers. Qt must include Quick, Quick Controls 2, Qml, Network, OpenGL, WebView, Svg, and Test. Bun is optional for desktop development.

One setup using Homebrew:

```sh
xcode-select --install  # Only if developer tools are not installed
brew install cmake qt mpv python

git clone https://github.com/HanifCarroll/Neden.git neden
cd neden
export CMAKE_PREFIX_PATH="$(brew --prefix qt)"
export PATH="$(brew --prefix qt)/bin:$PATH"
python3 scripts/fetch_macos_runtime.py
sh scripts/desktop.sh test
sh scripts/desktop.sh lint
sh scripts/desktop.sh run
```

The Homebrew mpv package supplies **headers only**. Playback links the pinned, checksum-verified frameworks downloaded into `.cache/playback-frameworks`, rather than the Homebrew library. The build checks the reviewed runtime's configuration. Set `NEDEN_MPV_FRAMEWORKS` to use another copy of that same reviewed runtime. On a custom Qt installation, set `CMAKE_PREFIX_PATH` and `PATH` to its prefix and `bin` directory.

Fresh builds use ad hoc development signing; an Apple developer account is not required. A persistent certificate is optional for stable Keychain approvals. To install locally without opening the app:

```sh
python3 scripts/install_desktop.py --no-launch
# Or install in your own Applications folder:
python3 scripts/install_desktop.py --no-launch --destination ~/Applications/Neden.app
```

Installation gracefully quits and replaces a running copy, so finish playback first. The [installation guide](docs/desktop/INSTALLATION.md) covers signing, shortcuts, profiles, native testing, and recovery. For logic tests without libmpv or Qt WebView, use `sh scripts/desktop.sh core-test` with Qt Core/Gui/Network/Qml/Test installed.

Build scripts use the selected developer toolchain. If needed, select your own tools with `DEVELOPER_DIR`; local overrides belong in your shell or ignored `mise.local.toml`, never in tracked configuration.

## IINA plugin

Read the [plugin usage guide](apps/iina-plugin/README.md) for IINA requirements and installation. To build and validate it on macOS, install Bun 1.3.14+, Python 3.12+, and Apple's Swift command-line compiler, then run from the repository root:

```sh
bun install --frozen-lockfile
bun run release:check
```

This checks the plugin and builds an installable `.iinaplgz` in ignored `dist/`. Packaging does not require IINA at a particular filesystem path, and does not install or enable the plugin. Actual integration checks still require IINA. Root `Info.json` mirrors the plugin metadata for IINA's GitHub update discovery.

## Data and services

Playback history, subtitle associations, settings, and study cards are stored locally. Desktop credentials use QtKeychain with plaintext fallback disabled. AI requests send the selected dialogue and configured context to the chosen provider; subtitle downloads and dictionary navigation contact their respective services. Use synthetic media and mock endpoints for tests. See [security and privacy](SECURITY.md) before sharing logs or profiles.

The legacy desktop bundle identifier and macOS application-support directory (`Neden Qt Lab`) remain stable for existing installations. They do not depend on the maintainer's account, home directory, signing certificate, or credentials.

## Repository map

| Directory | Purpose |
| --- | --- |
| `apps/desktop/` | C++ application modules, QML interface, native platform adapters, and tests |
| `apps/iina-plugin/` | TypeScript IINA integration, web UI, Swift transport helper, and tests |
| `apps/website/` | Astro product website, shared design styles, and verified download links |
| `apps/api/` | Experimental Better Auth, billing, and hosted AI service ([setup and limits](apps/api/README.md)) |
| `shared/` | Fonts, icons, branding, and their license notices |
| `scripts/` | Builds, packaging, runtime checks, and validation |
| `docs/` | Architecture, behavior contracts, setup, and platform limitations |

Start with [development conventions](docs/development.md), [desktop architecture](docs/desktop/ARCHITECTURE.md), or the [plugin specification](docs/iina-plugin/SPEC.md). Contributions are welcome; see [CONTRIBUTING.md](CONTRIBUTING.md). In development builds, Help → Getting started explains setup, Help → Check for updates checks stable releases, and Help → Report a problem provides private-by-default diagnostics to copy. Report reproducible bugs through [GitHub Issues](https://github.com/HanifCarroll/Neden/issues).

## License

Neden's source is [MIT licensed](LICENSE). Fonts, icons, Qt, QtKeychain, libmpv, and other dependencies retain their own licenses; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). No video library or commercial subtitle collection is included in the current source tree. Neden is an independent project and is not affiliated with IINA or the configured service providers.
