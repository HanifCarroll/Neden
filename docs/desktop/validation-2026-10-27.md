# Desktop validation — October 27, 2026

## Video clicks and language behavior

- Single clicks on the picture now toggle playback. Clicking a paused picture
  with the sidebar open still closes the sidebar and resumes. Subtitle word
  actions remain separate. The single-click handler waits for the system's
  double-click interval so a fullscreen double-click does not also pause.
- On the MacBook Pro, six interaction scenarios passed together at `6f57f9d`,
  including video clicks, word cards, selection and explanation hover. A separate
  native check verified double-clicks during active playback through fullscreen
  entry and exit, waiting 700 ms against the host's 500 ms double-click interval.
  Temporary profiles and synthetic media were used; the normal profile was unchanged.
- Local validation passed all 15 CTest suites, 11 portable UI scenarios, QML lint,
  hosted API type checking and all 29 API tests. The hosted language prompt changes
  were deployed only to staging, version `d86caaa7-b491-47ef-be34-161174c6a6bf`;
  its health endpoint returned HTTP 200. These are structural checks, not a claim
  of linguistic accuracy in every supported language.
- See [the generalization audit](generalization-audit-2026-10-27.md) for changes
  and remaining script/segmentation limitations, and
  [subtitle sources](FREE_SUBTITLE_SOURCES.md) for verified free-tier allowances.

## Rendering investigation

- [Diagnostic package run at `1b88bd7`](https://github.com/HanifCarroll/Neden/actions/runs/40503526454)
  passed on Linux and macOS. Windows passed its behavior, UI and credential checks,
  but failed the installed visible-white-video check. Opt-in framebuffer readback
  showed black pixels inside the video's own render target, before presentation,
  with successful mpv return codes and no reported GL error. A running playback
  clock therefore remains insufficient to accept this package.
- [Same-installer Windows driver comparison](https://github.com/HanifCarroll/Neden/actions/runs/40505401546)
  tested the bundled Mesa 11.2.2 and an independently downloaded, checksum-pinned
  Mesa 26.2.4. The bundled driver failed with both Qt render loops. The modern
  driver failed with the threaded loop and passed with the basic loop. One
  successful configuration does not establish a general fix; no replacement
  driver or render-loop override was shipped from this experiment.
- Qt 6.10's RHI changes `GL_CULL_FACE`, while its
  `QQuickOpenGLUtils::resetOpenGLState()` implementation does not restore that
  state. [libmpv's OpenGL contract](https://github.com/mpv-player/mpv/blob/v0.41.0/include/mpv/render_gl.h)
  expects standard defaults. `f0d6090` explicitly disables face culling before
  mpv initialization and each render. A controlled Pro comparison verified both
  renderers were white before injection, and that injected culling reached render
  entry. The fixed renderer produced **12/12 white samples**; its parent produced
  **12/12 black samples**, despite advancing playback. The ordinary smoke test
  also passed. A maintained native regression accompanies the fix.
- The committed regression at `b424684` passed on the Pro, with six white-screen
  samples after injected culling. The same revision passed GitHub Checks and was
  installed locally; its application signature verified successfully. The local
  app was left closed.
- [Post-fix package run at `f0d6090`](https://github.com/HanifCarroll/Neden/actions/runs/40506151346)
  passed the installed video check on Linux and macOS. Windows again passed the
  portable, UI and credential checks but failed the installed white-frame check
  with bundled Mesa 11.2.2, in both default and basic render loops. Frames 60, 120
  and 180 were black inside the video framebuffer, with culling already disabled,
  no GL error and a successful mpv return code. The culling fix therefore resolves
  a demonstrated defect, **not the remaining Windows software-rendering failure**.
  The newer-driver/basic-loop combination remains a diagnostic lead: its other
  tested render-loop configuration failed, and the downloaded binary archive does
  not include the dependency notices needed by the release packaging process.
  It has not replaced the packaged driver or changed Windows render-loop defaults.
- Synchronous playback property calls remain a separate potential source of
  stalls: [libmpv documents](https://github.com/mpv-player/mpv/blob/v0.41.0/include/mpv/render.h)
  that render-thread dependencies on blocking client calls can cause timeouts.
  Their presence alone does not prove they caused these black frames.

Public preview installers and release-readiness claims are unchanged. CI software
graphics checks do not establish physical GPU, sound-device or Wayland coverage.


## Subtitle catalogs and script-aware word selection

- `3d17a4f` adds a source picker for Subvault, SubDL and the existing OpenSubtitles
  integration. Both new providers completed title selection, release selection,
  download and SRT parsing through the actual C++ catalog client. Each returned
  1,429 cues for the same episode; downloaded dialogue and account credentials
  remain outside the repository. The free SubDL account reported 2,000 searches
  and 50 downloads per day. See [source setup](SUBTITLE_SOURCES.md).
- Mock tests cover credential isolation, title IDs, season-pack episode filtering,
  redirects, disallowed download origins, quota/refresh errors, and cancellation.
  All 15 local desktop CTest suites and 12 portable UI scenarios passed. The
  updated language tests exercise Japanese, Chinese, Thai, and UTF-16 offsets
  after emoji, alongside existing space-separated scripts. Native dictionaries
  differ across operating systems; this does not imply perfect segmentation or
  translation accuracy for every language.
- [Package run at `3d17a4f`](https://github.com/HanifCarroll/Neden/actions/runs/40515535153)
  passed Linux and macOS packaging and installed visible-video checks. This also
  exercises Linux ICU linkage and the macOS native word tokenizer.
- A combined UI run exposed test state leakage: the catalog test left its modal
  study drawer open. `142b98c` closes it, including cleanup on assertion failure.
  The combined catalog, Settings, word-card and hover sequence then passed locally
  with the offscreen backend. It also corrects the missing-key guidance to the
  actual **AI & accounts** Settings tab.
- On the MacBook Pro, unmodified `acb342b` passed the requested combined native
  sequence (7 passes including setup/cleanup), and the longer sequence with media
  loading (8 passes including setup/cleanup), with no failures or skips. All 34
  study-test cases passed. The original failed runs and a separate instrumented
  hover diagnostic failure were retained. Native screenshots at 1280×800 and
  900×540 show readable source choices and no horizontal clipping; credential
  controls are reachable by vertical scrolling. Tests used synthetic media and
  isolated profiles, with no live AI calls or changes to the normal profile.

## Windows software fallback follow-up

- `3543705` packages a checksum-pinned Mesa 26.2.4 fallback and selects Qt's basic
  render loop on Windows. Its corresponding dependency sources, build recipes and
  notices are collected with the package. The system OpenGL driver remains the
  normal hardware path; the bundled DLL uses Qt's `opengl32sw.dll` fallback name.
- [Installed check at `3543705`](https://github.com/HanifCarroll/Neden/actions/runs/40514340321)
  revealed a further distinction: Mesa selected D3D12 with Microsoft's Basic
  Render Driver. Frames 60, 120 and 180 were white inside mpv's framebuffer, but
  the presented window was black and the visible-video check correctly failed.
- `acb342b` selects Mesa's LLVMpipe CPU rasterizer by default on Windows, preserving
  explicit diagnostic overrides. Vendor OpenGL drivers ignore this Mesa-specific
  setting. This follows [Mesa's driver selection documentation](https://docs.mesa3d.org/envvars.html)
  and [LLVMpipe documentation](https://docs.mesa3d.org/drivers/llvmpipe.html).
  The installed-package check clears driver and render-loop overrides, forces only
  Qt's software fallback, and requires three successful fresh starts using actual
  screen pixels. Internal framebuffer readback cannot substitute for this check.
- [Package run at `acb342b`](https://github.com/HanifCarroll/Neden/actions/runs/40517026640)
  passed all three installed white-frame launches, portable/UI/credential tests,
  source collection, DLL dependency checks, installation and uninstallation.
  However, separate driver comparisons at `3d17a4f` produced mixed pass/fail
  results with LLVMpipe in both render loops. In a retained failed image the UI
  was visible while the video and its framebuffer were black. This is distinct
  from the earlier D3D12 whole-window presentation failure. Three passing starts
  alone do not establish that the intermittent failure is resolved.
- The diagnostic workflow now accepts either successful package candidates or
  failed installers, repeats fresh launches, retains every result, and fails
  when **any** tested configuration fails. Previously its exit status considered
  only the explicitly loaded comparison DLL, even when the bundled path failed.


## Repeated Windows launches and upstream research

The following tests reuse the **same installer from `acb342b`**, with fresh
profiles and processes. A successful launch must display actual white video;
the playback clock or a successful rendering return value is insufficient.

| Comparison | Bundled/threaded | Bundled/basic | Explicit driver/threaded | Explicit driver/basic |
| --- | --- | --- | --- | --- |
| [Mesa 26.2.4, native GL](https://github.com/HanifCarroll/Neden/actions/runs/40519855661) | 4/5 | 2/5 | 3/5 | 3/5 |
| [Mesa 25.2.8 comparison](https://github.com/HanifCarroll/Neden/actions/runs/40520103521) | 2/5 | 3/5 | 5/5 | 4/5 |
| [Mesa 26.2.4, GL 3.3 override](https://github.com/HanifCarroll/Neden/actions/runs/40521007954) | 4/5 | 3/5 | 3/5 | 3/5 |

The bundled columns always use the installer's Mesa 26.2.4. The explicit columns
use `QT_OPENGL_DLL` to select the comparison driver. These are diagnostic samples,
not estimates of failure rates on users' hardware. Neither an older driver nor
limiting the advertised OpenGL version eliminates the observed failure. The
Windows software-rendering issue remains **unresolved**.

On the MacBook Pro, ten fresh processes with isolated profiles all passed the
presented-white-frame check (1.53–2.07 seconds). Successful starts also logged
mpv's startup render-timeout warning. That warning alone therefore cannot identify
the cause of black Windows frames.

Relevant primary sources and their implications:

- [mpv's OpenGL contract](https://github.com/mpv-player/mpv/blob/v0.41.0/include/mpv/render_gl.h)
  requires the same current context and standard graphics state, with documented
  exceptions. [Qt's renderer contract](https://doc.qt.io/qt-6/qquickframebufferobject-renderer.html)
  explicitly warns that the scene graph shares and modifies that context. Our
  already-demonstrated culling fix addresses one violation; it is not proof that
  every remaining failure is another state leak.
- [mpv issue 14577](https://github.com/mpv-player/mpv/issues/14577) reports
  intermittent black/corrupted LLVMpipe video while overlays remain intact. It is
  an unresolved Linux report, not proof that Neden has the identical bug.
- [mpv issue 18259](https://github.com/mpv-player/mpv/issues/18259) involves Nvidia
  hardware decoding, HDR and GL errors. Our white fixture uses software decoding
  and reports no GL error, so its proposed cause does not match this reproduction.
- The [official SDL render example](https://github.com/mpv-player/mpv-examples/blob/master/libmpv/sdl/main.c)
  separates update notifications from drawing on the thread owning the graphics
  context. The standalone Windows diagnostic follows that structure with Win32,
  the installed mpv DLL and Mesa, without Qt or Neden's playback/settings code.
  It renders into a framebuffer, presents it, and requires ten consecutive white
  framebuffer **and screen** samples. It is an isolation test, not an app-test
  replacement or a production rendering fallback.

### Standalone reproduction

[The controlled standalone run](https://github.com/HanifCarroll/Neden/actions/runs/40523139534)
passed its own white-window presentation and capture control on all 20 launches.
Only 13/20 launches subsequently displayed white video through libmpv. Failed
launches repeatedly read approximately `(1, 1, 1)` from both the video framebuffer
and the screen, while successful launches read `(255, 255, 255)`. This reproduces
the symptom without Qt, application settings, or synchronous playback property
queries. Those components are therefore not necessary to trigger this failure.
It does not yet distinguish an mpv defect from a Mesa defect or their interaction
with this Windows runner. The earlier probe without a separate capture control
also failed intermittently (12/20 passed), but the controlled run is stronger
evidence.

The probe additionally offers two diagnostic modes based on
[mpv's documented options](https://mpv.io/manual/stable/): `rgb` moves conversion
to RGBA into the CPU video filter; `simple` requests the restricted, single-pass
GPU mode. These are separate experiments, not production defaults. Both still
require real white framebuffer and screen pixels. Neither disables the installed
application's acceptance check.

- [CPU RGBA conversion](https://github.com/HanifCarroll/Neden/actions/runs/40524016339):
  **20/20 passed** with the same installed libraries and white fixture.
- [Restricted rendering path](https://github.com/HanifCarroll/Neden/actions/runs/40524023092):
  **20/20 passed** with the same libraries and fixture.

These results point toward processing bypassed by those modes; they are not yet
proof of a specific faulty shader or a production-quality solution. The CPU
conversion changes video format/precision and workload; the restricted renderer
disables advanced processing. Neither has been enabled in the application.
Reading mpv 0.41's `video/out/gpu/video.c` identifies chroma scaling as one stage
both modes bypass, motivating a separate `cscale=bilinear` experiment. A separate
`fbo-format=rgba32f` experiment checks whether intermediate half-float buffers are
responsible without simplifying the scalers.

- [Bilinear chroma only](https://github.com/HanifCarroll/Neden/actions/runs/40524480983):
  **20/20 passed**. Luma scaling and the rest of the normal rendering pipeline
  remain enabled.
- [32-bit float intermediate buffers](https://github.com/HanifCarroll/Neden/actions/runs/40524488081):
  **16/20 passed**, so increasing buffer precision does not resolve the symptom.

`b1ff51f` consequently selects bilinear chroma scaling only when the current
Windows OpenGL renderer identifies itself as LLVMpipe. It sets the option before
creating the libmpv render context and loading media, and reports configuration
errors. Other platforms and hardware OpenGL renderers retain their defaults.
This is a narrowly scoped compatibility workaround supported by a controlled
reproducer, not a claim to have repaired or conclusively identified an upstream
Mesa/mpv defect. Bilinear chroma reconstruction can make color edges softer than
the default Lanczos reconstruction; luma sharpness and the normal processing
features are preserved. Installed-application validation is recorded below.
