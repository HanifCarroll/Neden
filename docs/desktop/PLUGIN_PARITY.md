# Desktop and IINA plugin behavior

The desktop player preserves the plugin’s language-learning workflow. `apps/iina-plugin` remains the reference implementation and is not modified by desktop updates.

| Capability | Desktop behavior |
| --- | --- |
| Phrase explanations | Double-click a word, finish a text selection, or use the primary subtitle’s hover **Explain line** action. The first answer includes Natural meaning, Breakdown, and optional Grammar. |
| Conversation context | Freeze the selected phrase, full source cue, three cues before and after, overlapping secondary cues when enabled, and language choices for the conversation. |
| Follow-ups | Ask in Meaning; Enter sends, Shift–Enter adds a line. Completed history accompanies the question. Partial or failed answers are displayed but excluded from future request context. |
| Streaming, Stop, Retry | Show incremental text, distinguish failed/incomplete/completed turns, and retry only on an explicit action. Retry replaces the interrupted turn. |
| Close and reopen | Stop an active answer, retain the conversation in the current window, and resume playback only when that interaction originally paused it. Opening another media file or changing tracks clears obsolete context. |
| Replay and full cue | Meaning includes **Replay line** and a full-subtitle disclosure. Replay uses the frozen cue and follows the desktop requirement to play it once and pause. |
| Limits | 2,000 characters per follow-up, 20 turns, 32 KB initial context, 100 KB full request, and 64 KiB per answer. Reject oversized input; never silently truncate context. |
| Subtitle files and roles | Load UTF-8 SRT/WebVTT into either role. Reuse loaded files through role selectors or choose None. Both tracks have independent timing. Each video remembers its chosen files. |
| Subtitle appearance | Independent sizes/colors, spacing, order, bottom margin and relative bottom offset; outline, weight, compact background. Preview in the video, apply to save, or close Settings to restore saved appearance. |
| Overlay interaction | Disable selectable subtitles and explanation controls while retaining plain subtitles. This stops pending work and clears the conversation. |
| AI settings | Set API base URL/model, source/explanation languages, secondary-context inclusion, or a key-free endpoint. Save new keys in native Keychain or explicitly connect existing entries. Saved keys are never returned to the UI. |
| Optional provider fallback | Choose configured AI or GPT-6 Luna first. An explicitly enabled fallback waits five seconds without an answer before trying the other provider once. It remains off by default, never switches after text starts, and warns that both attempts may be billed. |
| Quick word cards | Optional single-click Google Cloud Translation or Microsoft Translator lookup; off by default. Show plain meanings and alternatives. Close restores eligible playback; a new selection/track/media/cue cancels obsolete work. |
| Dictionary and study | Keep configurable dictionary URLs/WebView, flashcards/review/export, and OpenSubtitles search/download. |

## Deliberate differences

The desktop uses its own libmpv adapter and subtitle timeline. IINA-only bridge methods, sidebar attachment, native-subtitle restoration, and plugin teardown hooks do not apply. Desktop playback and subtitle navigation retain the user’s specified shortcuts and play-once/pause behavior. Subtitle defaults retain the chosen desktop design, with primary above secondary and no broad background strip.

Microsoft’s empty dictionary response offers an explicit **Try word translation** action. It does not silently issue a second billable request. AI fallback likewise requires the user’s explicit opt-in. Word-service language settings accept the common language names supported by the plugin or ISO codes.

Conversations are window-local and not saved to disk. Recent-video history stores media position and chosen subtitle file paths, not chat text. Secure credential storage is implemented on macOS; other platforms continue to fail closed until an OS credential adapter is verified.

## Verification

Pure tests cover conversation context/history, retry and limits; Turkish and multilingual text; SRT/WebVTT; track reuse/removal; appearance preview/cancel/save; non-secret settings; and bounded word responses. Controlled local services exercise incremental AI output, cancellation, explicit retry, opted-in fallback, and word-lookup cancellation without paid calls.

The optional native test target exercises the actual QML/libmpv window on a Mac: file pickers and dual subtitles, gestures/fullscreen, controls and focus, phrase submission, and follow-up composition. Keep visible checks on the Pro when the Air is in use. Mock tests do not establish that a provider accepts a real account’s credentials or that macOS will grant a new Keychain approval.
