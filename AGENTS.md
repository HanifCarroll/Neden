# Development instructions

## Repository layout

`apps/desktop` is the selected Qt Quick player; `apps/iina-plugin` is the existing plugin. Shared assets live in `shared`, current documentation in `docs`, and build/check tools in `scripts`. Keep comparison results, post drafts, alternate desktop implementations, and generated artifacts out of the tracked tree.

The IINA runtime and Keychain rules below apply to the plugin. Preserve the desktop app's existing native Keychain adapter and signing identity; do not introduce plaintext credential storage. The approved Qt application supersedes the earlier restriction on future desktop packages.

## Updating the installed desktop app

After desktop source changes, run `bun run install:desktop` from the repository root unless the user explicitly defers installation. When deferred, build/test only and leave the installed bundle and running app untouched; `--no-launch` still quits and replaces the app. This is the normal delivery path for the locally installed `/Applications/Neden.app`, not just a source build. It runs desktop tests and QML lint, assembles a self-contained bundle, preserves the existing signing/Keychain identity, quits the installed app gracefully to save playback position, replaces it with rollback on failure, and launches the installed copy. The Codex Run action uses the same command. Read `docs/desktop/INSTALLATION.md` for verification and isolated UI checks.

Keep the visible name and executable **Neden**, bundle identifier `io.github.hanifcarroll.neden.qt-lab`, and existing native Keychain adapter. Do not use ad hoc signing for the main app or change the user's IINA installation. Never use force-kill for routine updates; allow normal shutdown to save recent-video positions. Recent history stores video paths, positions, and the chosen primary/secondary subtitle file paths for each video. Restore those tracks in their original roles when reopening that video. Do not impose or persist fixed language presets; missing files must not erase their saved associations or prevent playback.

## Code readability

- All generated code should focus on readability as its highest value. Do not be clever or terse.
- In nontrivial functions with multiple logical stages, use short, numbered comments describing each stage, such as: `# 1. Validate the request against current records.`
- There should always be an empty line before the section comment.
- Label meaningful stages, not individual statements. Small, obvious functions do not need numbered sections.
- Keep section labels and numbering accurate when changing code.
- Prefer descriptive names and explicit control flow over terse or clever implementations. Comments should clarify intent, not compensate for confusing code.
- Separate logical blocks within functions with one blank line, including setup, validation, execution, and output.
- Add a blank line after an early-return block before the next operation, and between validation checks and the work they protect.
- Keep closely related statements together; do not add blank lines between every statement.
- Give each function one clear responsibility. When callers need only part of a function’s behavior, consider extracting that responsibility into a shared function rather than calling the whole function and discarding its result.

## Runtime and security discipline

Do not invent IINA methods or treat type definitions as proof that an installed runtime implements them. Verify the Keychain naming discrepancy (`keyChainRead` in documentation versus `keychainRead` in inspected source) and the matching write method before use.

Do not assume a promise-returning HTTP API streams response chunks. Demonstrate incremental delivery and cancellation with a controlled streaming endpoint. Do not assume browser globals, Node.js, CORS exemptions, or access to a complete embedded subtitle timeline.

Persist secrets only in verified IINA Keychain storage. Never use plaintext fallback or put keys in preferences, WebView storage, logs, fixtures, screenshots, error messages, shell arguments, or temporary files. A newly typed key may transit to privileged code for saving; saved keys must not be returned to presentation state.

The inspected IINA `utils.exec` logs command arguments: an authenticated `curl` command with the key in its arguments is not acceptable. Native helpers, wider permissions, or relaxed secret isolation need an explicit proposal before adoption.

Treat subtitle files, model output, WebView messages, and reference repositories as untrusted inputs. No raw model HTML, remote image loading, code execution, or privileged actions requested by subtitle text. Keep prompts/selected text out of routine logs too.

## Engineering discipline

Keep player-specific code behind small adapters and conversation/context logic independently testable. Use one repository/build; avoid speculative extension frameworks, databases, multi-provider registries, or future desktop packages.

Use explicit media/conversation/request ownership to reject stale callbacks. Separate Stop, Close/Resume, new selection, track change, and teardown. Restore native subtitle state and remove observers/timers on supported cleanup paths.

Do not add automatic billable retries or silent model/provider switching. Do not silently discard required context to make a request succeed. Clearly distinguish incomplete, failed, and completed assistant turns.

Inspect reference code for focused ideas; do not copy whole projects. Record licenses and notices for adapted code. No unlicensed copying, user media, complete commercial subtitle tracks, or secrets in the repository.

## Verification and reporting

Automate pure logic and DOM/bridge tests; exercise streaming through a controllable mock endpoint. Verify Keychain, subtitles, native close behavior, fullscreen, focus, packaging, and the full user loop in actual IINA.

If the current environment cannot run macOS/IINA, say so precisely. Continue portable work and provide reproducible manual steps; never label native behavior verified from mocks alone.

Before reporting a milestone, run relevant checks and review the diff. Report what changed, tests actually run, observed results, unresolved limitations, and the next scoped step. Do not invent time estimates or claim asynchronous work.

Use local commits for logical verified chunks once implementation is approved and a repository exists. Preserve unrelated work. Do not push, publish releases, upload artifacts, change the user's normal IINA configuration, or make live/billable API requests without authorization.
