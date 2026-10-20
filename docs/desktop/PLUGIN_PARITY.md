# Desktop and IINA plugin behavior

The desktop player preserves the plugin’s language-learning workflow. `apps/iina-plugin` remains the reference implementation and is not modified by desktop updates.

| Capability | Desktop behavior |
| --- | --- |
| Phrase explanations | Double-click a word, finish a text selection, or use the primary subtitle’s hover **Explain line** action. The desktop first answer uses Translation and Breakdown. Each breakdown entry combines an expression’s contextual meaning with its useful grammar or usage, teaching each point once; follow-ups provide deeper detail. |
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
| Quick word cards | Single-click a word for a floating contextual AI card: a short scene-specific gloss, normally one explanatory sentence (at most 35 words/220 characters), part of speech, dictionary form, and zero to two brief alternative senses. The prompt keeps alternatives within the same dictionary entry and part of speech. The popover has no extra explanation action; Explain line (or E) opens the full subtitle explanation in the sidebar. Uses the configured AI endpoint/model and saved key (DeepSeek by default). Contextual AI and Off are the available choices. Retired Google/Microsoft choices migrate to contextual AI while preserving the other learning settings. Close restores eligible playback; a new selection/track/media/cue cancels obsolete work. |
| Dictionary and study | Keep configurable dictionary URLs/WebView, flashcards/review/export, and OpenSubtitles search/download. |

The dictionary panel has a search field (Enter submits), a back arrow only when web history exists, and Retry only after a failed load. A quiet loading indicator disappears when ready. It has no duplicate Close, misleading flashcard Save, permanent Reload, or Loaded status; the inspector tabs and close button handle navigation, and flashcards remain in Meaning/Study.

## Deliberate differences

The desktop uses its own libmpv adapter and subtitle timeline. IINA-only bridge methods, sidebar attachment, native-subtitle restoration, and plugin teardown hooks do not apply. Desktop playback and subtitle navigation retain the user’s specified shortcuts and play-once/pause behavior. Subtitle defaults retain the chosen desktop design, with primary above secondary and no broad background strip.

AI fallback likewise requires the user’s explicit opt-in. Quick cards make one bounded JSON Chat Completions request after the system double-click interval, so phrase selection does not also charge for a word card. The request freezes the clicked subtitle, primary source cue, three neighboring cues on each side, optional secondary context, and language choices. No retry or provider fallback happens automatically; malformed/truncated/empty answers show an actionable error with an explicit Try again. The model may be wrong; alternative meanings are generated suggestions, not a vetted dictionary. Language settings accept descriptive language names or codes for the AI prompt.

Each conversation owns a frozen study target. Saving a flashcard or replaying its line uses that target after the playhead moves; subtitle downloads, settings actions and deck updates have independent messages. Settings drafts commit atomically to the existing profile.

Conversations are window-local and not saved to disk. Recent-video history stores media position and chosen subtitle file paths, not chat text. Secure credential storage is implemented on macOS; other platforms continue to fail closed until an OS credential adapter is verified.

## Verification

Pure tests cover conversation context/history, retry and limits; Turkish and multilingual text; SRT/WebVTT; track reuse/removal; appearance preview/cancel/save; non-secret settings; and bounded word responses. Controlled local services exercise incremental AI output, cancellation, explicit retry, opted-in fallback, and word-lookup cancellation without paid calls.

The optional native test target exercises the actual QML/libmpv window on a Mac: file pickers and dual subtitles, gestures/fullscreen, controls and focus, phrase submission, and follow-up composition. Keep visible checks on the Pro when the Air is in use. Mock tests do not establish that a provider accepts a real account’s credentials or that macOS will grant a new Keychain approval.
