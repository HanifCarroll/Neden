# Preparing a desktop release

A preview is a support/validation designation, not an installer format. Promote
platforms independently; signing does not substitute for behavioral acceptance.

## Build and publish one immutable revision

1. Commit the complete source and version. Dispatch `Desktop packages` with `all`
   at that commit. All jobs must succeed; do not combine artifacts from different runs.
2. Download each platform candidate and matching sources. Each candidate includes
   `release-record.json`. Run `python3 scripts/release/verify_candidate_records.py
   COMMIT VERSION MAC_DIRECTORY WINDOWS_DIRECTORY LINUX_DIRECTORY`.
3. Check installed playback, UI, credentials, upgrades and accessibility against
   the exact downloaded packages; record environment, skips and failures.
4. Verify source archives/notices, installer checksums, signatures, and macOS
   notarization where applicable. Create a versioned validation document.
5. Create a draft release pointing to the tested commit, upload all installers,
   corresponding sources, checksums and validation evidence, then verify their
   remote hashes. Publish only when complete. Repository release immutability is
   enabled: published assets and tags are locked; corrections need a new version.
6. Update the website manifest to that release's exact asset URLs. Keep support
   labels specific to the environments actually tested.

## Acceptance gates

- Clean install, restart, upgrade over the previous release, uninstall without
  losing user-owned videos or silently destroying study data.
- Real video and audible audio; common codecs/containers; hardware decoding and
  software fallback; seek/replay/frame step; chapters; queue; sleep/wake and unplug.
- Dual subtitles, Turkish search, word cards, line explanation, streaming follow-up,
  cancellation/retry/failure, and durable flashcards/history.
- Fullscreen, one-pixel cursor wake, idle controls, multiple displays and scaling.
- Real OS credential backend: save, fresh-process reconnect, locked/missing store,
  and deletion. No plaintext fallback. Use synthetic keys and isolated accounts.
- Keyboard-only setup/playback/study flows, visible focus, screen-reader names,
  larger display scale, reduced motion, and 900×540 minimum window layout.
- macOS: Developer ID + hardened runtime + notarization/stapling, downloaded-file
  Gatekeeper check, physical pointer/audio, Keychain approval and upgrade continuity.
- Windows: trusted signing and timestamps, Windows 11 desktop/WebView2/credentials;
  Windows 10 only if separately tested and still claimed. CI Server is not desktop coverage.
- Linux: initially Ubuntu 24.04 x64, Wayland and X11 sessions, Secret Service,
  browser sandbox enabled, GPU/audio and installed dependency closure. Docker and
  Xvfb results must not be described as physical desktop verification.

## Automated coverage

`support_tests` exercises update URL/version validation and diagnostics privacy.
`check_desktop_ui.py` runs independent real-QML UI scenarios against mock AI,
using software rendering and disposable profiles. Native credential integration
checks write/read/delete one unique synthetic account only in disposable CI sessions.
Packaged `--smoke-test --profile TEMP VIDEO` waits for decoded video dimensions,
advancing playback and a rendered window frame, not just process creation.

Signing certificates, paid accounts and physical hardware are external prerequisites;
absence must remain explicit in the release record. The app's manual checker offers
stable releases only; users can deliberately visit GitHub for previews.
