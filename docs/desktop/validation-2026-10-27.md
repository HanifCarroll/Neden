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
