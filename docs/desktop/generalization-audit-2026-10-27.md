# Language and machine portability audit — 2026-10-27

Scope: the shipped Qt desktop application, its hosted study API, subtitle pipeline, and build/package configuration. The separate legacy IINA plugin is not included in desktop installers.

## Verified behavior

- New desktop profiles use automatic source-language detection and the operating system's language for explanations. English is only the fallback for an unrecognized system locale. Existing, explicitly saved choices are preserved.
- Source, explanation, subtitle-search and track-translation languages use validated lists. Provider-specific language identifiers stay in `LanguageCatalog`.
- Subtitle search uses the chosen language and video fingerprint or explicit title/season/episode. Automatic lookup skips unspecified languages. There is no show-specific lookup, episode number, media folder or Turkish download preference in the shipped application.
- Local and embedded subtitles, timing repair, and track translation retain Unicode text and original timing. Files are selected by the user or discovered alongside their video, rather than from a particular home directory.
- Profiles and caches use platform data directories. Cloud origins and playback runtime locations are configurable. The repository owner in release URLs and application identifiers identifies the product; it does not select a user's account, files or learning language.
- The optional external dictionary still defaults to English Wiktionary, which contains multilingual headwords. Its URL is editable. This is separate from the contextual AI word card. The interface itself is currently English; language-neutral learning is not a claim of fully translated interface strings.

## Improvements

- Desktop and hosted prompts now use examples from multiple languages instead of examples tied to one Turkish show. Explanation headings follow the selected explanation language. Guidance covers lexical words in scripts without spaces and language-appropriate grammar, rather than assuming all teaching is suffix analysis.
- Word clicks and hosted validation preserve internal apostrophes, hyphens and Unicode joiners. This fixes truncated French elisions, English contractions, compounds and Persian joined forms. Outer quotation marks and dialogue dashes remain outside the looked-up word.
- The case-folding mode is named Turkic, matching its existing use for Turkish and Azerbaijani. General Unicode case folding remains the default.
- Added cross-script request and extraction checks for Arabic, Chinese/Japanese, Hindi, Persian, Cyrillic and Latin text, including different explanation languages.

## Remaining boundaries

- Word hit testing now uses Core Foundation dictionary tokenization on macOS and ICU on Windows/Linux for Chinese, Japanese, Thai, Lao, Khmer and Myanmar script runs. A bounded last-sentence cache avoids repeating segmentation on every hover. Tests cover Chinese, Japanese and Thai lexical units, mixed scripts and UTF-16 offsets after emoji. Dictionaries can split compounds differently between platforms; phrase selection and whole-line explanation remain available. This is not a claim of perfect linguistic segmentation.
- Automated tests verify transport, selection, settings and Unicode preservation; they do not certify linguistic accuracy for every language. Native right-to-left and complex-script visual acceptance needs a broader human-reviewed corpus.
- The legacy IINA plugin retains older language settings and provider integrations. It is separate from the desktop application and should not be presented as having the desktop's current setup experience.
