# Desktop acceptance pass — October 26, 2026

This records development-build checks, not promotion of the public preview to a
stable release. The website's `v0.2.0-preview.1` downloads remain unchanged.

## Changes delivered

- Getting-started guidance from the home screen and Help menu, with direct links
  to video, subtitle and AI setup.
- A manual stable-release check and release-notes links. No automatic polling,
  download or installation. The check sends no profile or media information.
- Copyable diagnostics with an explicit allowlist, excluding media titles,
  filenames, paths, subtitle text, questions and credentials.
- Explicit accessible names on dropdown entries, plus Help focus/shortcut checks.
- Source-revision records for every platform package and a verifier that rejects
  mixed revisions or versions. GitHub release immutability enabled for future
  published releases. Updated platform documentation and acceptance gates.

## Automated evidence

- [All-platform run at a3efe55](https://github.com/HanifCarroll/Neden/actions/runs/40390381793):
  macOS, Windows and Linux jobs all passed. Each runs 12 CTest suites and ten
  independent QML interaction scenarios using isolated profiles and mock AI.
- Windows Credential Manager and Linux Secret Service integration checks passed:
  save a synthetic key, reconnect through a fresh store object, read it, and
  delete the unique test entry. These do not establish locked-store, reboot or
  upgrade behavior on every desktop.
- [Stronger pixel-check run at aa98f68](https://github.com/HanifCarroll/Neden/actions/runs/40392066095):
  Linux and Windows failed the added pixel assertion; their earlier clock-only
  checks had passed. Repeating with a longer fixture at f65921f still failed on
  Linux and produced a corrupted offscreen capture. An independent OS capture
  of the installed app showed correct white video. The assertion was changed to
  inspect presented screen pixels through `QScreen`, rather than trigger another
  libmpv/Qt render through `QQuickWindow::grabWindow`. This change passed on the
  Pro but did not resolve either CI renderer: Windows still showed black video
  and Linux showed a grid pattern. Local Docker playback alone does not qualify
  those different graphics environments.
- [Final Linux run at 93d0596](https://github.com/HanifCarroll/Neden/actions/runs/40398104994)
  passed all checks, including the installed application's presented white pixels
  in its default rendering mode. Earlier failures remain unexplained; one passing
  rerun does not establish that the intermittent rendering issue is resolved.
- [Final Windows run at b9f417c](https://github.com/HanifCarroll/Neden/actions/runs/40397342981)
  passed all 12 CTest suites, ten QML scenarios and credential checks, but failed
  the installed white-pixel assertion in both default and basic rendering loops
  (exit 4). Switching render loops is therefore not an established fix. Failure
  captures and the exact installer are retained as CI artifacts. These runs use
  the same app source; 93d0596 only adds Linux workflow diagnostics.
- The f65921f macOS packaging run separately hit `hdiutil: Resource busy` while
  creating its disk image, after passing its tests. A fresh packaging run is
  required; that failed run was not accepted as a valid package. The macOS job
  in the subsequent 6432bdd run passed, including disk-image verification.
- Local macOS: 12 CTest suites, ten portable QML scenarios plus ten additional
  independent scenarios, QML lint, 14 Python
  packaging/install/source-record tests, website type/lint/build checks and the
  full plugin release check passed. The plugin release check also ran its 11
  Python archive tests.

## Installed Linux checks on this Mac

Ubuntu 24.04 x86-64 under Docker Desktop on Apple Silicon, Xvfb with Mesa
llvmpipe software OpenGL. Tests ran as a regular non-root user; Chromium sandbox
disabling was not requested. This is emulation, not a physical Linux desktop.

- Installed the published `.deb`, started its actual application, then upgraded
  to the candidate from a3efe55. Startup and playback-clock smoke checks passed.
- Generated an H.264/AAC color-pattern clip. A longer capture visibly displayed
  the pattern with Qt's default threaded renderer. Early captures were black;
  a basic-loop comparison displayed pixels, but the longer default-loop run did
  too. This does not establish a renderer defect. Installer smoke checks now
  have an explicit white-fixture pixel assertion so a clock alone cannot pass.
- Opened Help → Check for updates using mouse/keyboard input. The live GitHub
  request completed and correctly reported that no stable release exists.
  With the container network disconnected, the same action showed a clear
  failure and retry option. The network was restored after the check.
- Removed and reinstalled the package. Both generated profile files remained
  byte-identical across removal and reinstallation. The app executable was
  removed and restored as expected.
- The container has no sound device; audible output was not tested. No Wayland,
  real GPU, physical pointer, desktop screen reader or suspend/resume claim is made.

## MacBook Pro checks

Apple Silicon, macOS 27.2, visible 1920×1080 display at 1×. Temporary profiles,
synthetic media and mock AI were used. The normal installed application and its
credentials were left alone.

The broader native run had 32 scenario passes and five failures. Three failures
exposed stale test assumptions (track-menu entries, collapsed word details, and
missing media setup); those assertions/setup were corrected. A fullscreen/search
focus case passed on its independent rerun. Native pointer injection did not
arrive reliably through the remote environment and remains inconclusive.

Help dialogs were visually reviewed at normal size and 900×540. The guide's lower
content is reachable by scrolling. Focus, Escape dismissal and player-shortcut
blocking/restoration passed through Qt's event path. With the test process set
to `TabFocusAllControls`, Tab reached all actions, Shift+Tab reversed focus,
buttons exposed their focus ring, and Escape closed each dialog. The original
text/list-only focus policy was restored; no system preferences changed.
The original white-fixture pixel
smoke check at aa98f68 passed in 1.54 seconds. The final presented-screen-pixel
check at 6432bdd passed in 1.616 seconds with a new profile and the 30-second
fixture. Screen capture succeeded without bypassing validation.

## Remaining release gates

- macOS Developer ID signing, hardened runtime, notarization/stapling and fresh
  downloaded-file Gatekeeper verification. The available local identity is Apple
  Development, which does not establish public-distribution readiness.
- Resolve the Windows visible-video failure and explain the intermittent Linux
  failures. A running clock and successful decoder test are insufficient. No new
  public release is promoted from this acceptance pass.
- Windows trusted signing and timestamps, plus Windows 11 desktop acceptance.
  Windows Server CI is useful coverage but is not that desktop test.
- Linux Wayland/X11 desktop hardware, audible audio and hardware decoding.
- Screen-reader use, multiple displays/Retina scaling, real pointer behavior,
  sleep/wake, audio unplug and credential migration/locked-store coverage.
- Real-provider AI quality/latency/cost evaluation. Mock tests do not establish
  live provider behavior, and no paid AI requests were made for this pass.

See [release acceptance gates](RELEASING.md) and the
[optional subscription proposal](../HOSTED_PRODUCT.md).
