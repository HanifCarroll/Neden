# Development

## Repository and supported runtimes

`apps/desktop` is the selected Qt Quick player, `apps/iina-plugin` is the existing IINA plugin, `shared` holds shared assets, and `scripts` holds build and validation tools. The [desktop architecture](desktop/ARCHITECTURE.md) and [plugin specification](iina-plugin/SPEC.md) describe their separate runtime boundaries. Player-specific adapters keep conversation and context logic independently testable. Desktop native code is grouped into app, playback, subtitles, study, and platform modules, with checks under tests and QML under ui. See the desktop architecture for module contracts before extending a workflow.

The desktop app retains its existing native Keychain adapter and signing identity. Its normal local delivery path is `bun run install:desktop`, which runs tests and QML lint, packages the app, gracefully quits the installed copy to save position, replaces it with rollback on failure, and launches it. Build-only work does not update `/Applications/Neden.app`. Deferred installation means leaving the installed bundle and running app untouched; `--no-launch` still quits and replaces it. The [installation guide](desktop/INSTALLATION.md) covers the stable bundle identity, subtitle/history restoration, and isolated native checks. The desktop installer does not change the user's IINA installation.

## C++ editor support

The desktop CMake configuration exports `build/desktop/compile_commands.json`. The root `.clangd` points Zed and other clangd clients at that database so they use the same Qt/mpv include paths, generated headers, and compiler flags as the app. Run `bun run build:desktop` after a fresh checkout or build configuration change. If an already-open editor retains old diagnostics, run Zed's **Restart Language Server** action. Do not add machine-specific include paths to editor settings.

## Plugin runtime and credentials

IINA runtime behavior is established from the installed runtime and inspected source, not type declarations alone. The [plugin specification](iina-plugin/SPEC.md) records the case-sensitive `keyChainRead`/`keychainRead` discrepancy, the verified write method, supported host APIs, subtitle acquisition, and native teardown limits. Browser globals, Node.js facilities, CORS exemptions, complete embedded subtitle timelines, and incremental HTTP delivery cannot be assumed.

Stored credentials remain in the verified native Keychain adapter, with no plaintext fallback. Keys stay out of presentation state, preferences, WebView storage, logs, fixtures, screenshots, errors, subprocess arguments, and ordinary temporary files. A newly entered key may pass to privileged code for saving; a saved key does not return to presentation state. IINA's `utils.exec` logs arguments, so authenticated `curl` arguments would expose credentials. The approved packaged Swift transport and its permissions are documented in the specification; expanded native helpers, permissions, or weaker credential isolation require a separate design decision.

Subtitle files, model output, WebView messages, and reference code are untrusted inputs. Model HTML, remote images, executable content, and privileged actions derived from subtitle text are outside the rendering contract. Prompts and selected text stay out of routine logs. Adapted reference code retains its license and notices; user media, complete commercial subtitle tracks, and secrets stay out of the repository.

Window/media, conversation, and request ownership reject stale callbacks. Stop, Hide/Resume, new selections, track changes, and teardown have distinct semantics, including observer/timer cleanup and native subtitle restoration where the host supports them. Required context is not silently discarded. Failed, incomplete, and completed turns remain distinguishable. The specification defines the explicitly enabled fallback behavior; retries and provider changes follow that contract rather than introducing undisclosed billable requests.

## Verification

The [root README](../README.md) lists desktop build/test commands and the plugin's `bun run release:check` gate. Pure logic and DOM/bridge checks use controlled mocks; streaming tests demonstrate incremental delivery and cancellation against controlled endpoints. Actual IINA is needed to establish Keychain behavior, subtitle coexistence/restoration, native close, fullscreen, focus, packaging, and the full selection/conversation/resume loop. Mocks do not establish native behavior. Record tested runtimes, observed results, and checks that could not run separately from portable verification.

Generated artifacts, comparison results, alternate desktop implementations, and post drafts stay outside the tracked source tree. Local installation, source verification, public releases, artifact uploads, and live/billable API calls are distinct delivery steps; a local check does not establish a published release. [Release documentation](iina-plugin/RELEASE.md) and [the eval plan](iina-plugin/EVAL_PLAN.md) cover plugin distribution and language-quality evidence.
