# Neden

Language learning while watching local video, with selectable subtitles, contextual explanations, and dictionary lookup.

- `apps/desktop/` — Qt Quick desktop player: dual SRT subtitles, Turkish-aware transcript search, sentence replay, dictionary WebView, streamed AI explanations/translations, study cards, subtitle downloads, and settings.
- `apps/iina-plugin/` — existing macOS IINA integration, with its own [usage guide](apps/iina-plugin/README.md).
- `shared/` — shared assets and their required notices.
- `docs/` — current architecture, platform limits, dependency notes, and plugin specification.
- `scripts/` — build, packaging, and validation tools.

## Desktop development

Use Qt 6.8 or newer with Quick, Quick Controls 2, Qml, Network, OpenGL, WebView, and Test; CMake 3.21 or newer; Python 3.12 or newer; a C++17 compiler; and libmpv development headers. The current Mac build uses Qt 6.12. macOS builds also require command-line developer tools and the existing Apple Development signing identity.

Prepare the reviewed macOS playback frameworks with `python3 scripts/fetch_macos_runtime.py`, then:

```sh
bun run build:desktop
bun run test:desktop
bun run run:desktop
```

The launcher defaults to `.cache/playback-frameworks`; set `NEDEN_MPV_FRAMEWORKS` to use another reviewed copy. It deliberately refuses an unreviewed system mpv. Use `CMAKE_PREFIX_PATH` for a Qt installation outside the compiler's normal search path. `bun run test:desktop:core` checks portable logic without a playback runtime. Desktop commands can also be run directly through `scripts/desktop.sh` without Bun.

Open your own video and source/translation SRT files through the player. No media or commercial subtitle tracks are bundled. Settings opens with Command–comma on macOS or Ctrl–comma elsewhere. Instant fullscreen is the Mac default; native fullscreen is optional. Existing bundle identifiers and saved-data locations are retained so prior Keychain approvals and study data remain usable.

See [desktop architecture](docs/desktop/ARCHITECTURE.md), [platform limits](docs/desktop/PLATFORMS.md), and [dependency and distribution requirements](docs/desktop/DEPENDENCIES.md). These are developer builds, not redistributable installers.

## IINA plugin development

From the repository root:

```sh
bun install --frozen-lockfile
bun run release:check
```

The check builds an installable package in ignored `dist/`, verifies its contents, and exercises streaming against controlled local endpoints. It does not install or enable the plugin in IINA. The root `Info.json` is required by IINA's GitHub update discovery; it mirrors `apps/iina-plugin/Info.json` and is checked during packaging.

The project source is MIT licensed; dependency licenses still apply. Generated builds, caches, and test output are ignored. Previous implementations are available through Git history.
