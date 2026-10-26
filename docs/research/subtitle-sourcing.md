# Subtitles included with Neden Cloud

Research checked 2026-10-26. This is a proposal, not an enabled Cloud feature.

## Recommendation

Use a commercial subtitle API through Neden's authenticated Cloud service. Start by comparing OpenSubtitles Pro and SubDL Pro on representative Turkish/English episodes and movies. Select the provider on coverage, matching and timing quality, not just advertised catalogue size. Add a second provider only where it improves coverage. Offer generated subtitles later as an explicit fallback.

The app should ask for preferred subtitle languages once, then check embedded tracks and adjacent files before searching online. Load a confident release/hash match automatically. For ambiguous matches, show a short choice with release and language. Keep manual loading and timing adjustment available.

Call this **subtitles included**, rather than promise unlimited availability. Two subtitle languages mean two downloads per episode. For example, 100 users watching five episodes daily would consume 1,000 downloads before retries. Provider allowances are a shared service budget, not automatically a separate allowance for each Neden subscriber.

## Options

- **OpenSubtitles Pro:** Designed for applications whose users do not supply OpenSubtitles credentials. The API overview describes unlimited professional access, but the newer Pro page specifies dedicated quotas and tiered limits. Confirm the app's allowance and pricing before buying; the linked price table was not readable in this research session. A personal free account's 20 downloads/day and VIP's 1,000/day are different from the commercial app arrangement.
  - https://opensubtitles.tawk.help/article/about-the-api
  - https://opensubtitles.tawk.help/article/pro-packages
  - https://opensubtitles.tawk.help/article/user-levels-basic-vip
  - https://opensubtitles.stoplight.io/docs/opensubtitles-api/fcgiyz3p7sqn9-api-subscription-prices
- **SubDL:** Its published Pro plan is $5/month with 30,000 API requests and 2,000 API downloads/day. Max lists $10/month with 60,000 requests and 5,000 downloads/day. Confirm the billing interval and checkout price when subscribing. Its terms explicitly permit commercial API use with an active paid plan and prohibit a shared free key powering a commercial service. Website “unlimited downloads” does not mean unlimited API downloads. This is the strongest low-cost alternative to test first.
  - https://subdl.com/pro
  - https://subdl.com/developers
  - https://subdl.com/terms
- **SubSource:** Documents search/download APIs, 7,200 requests/day on free keys and 100,000/day on Pro, with custom higher-volume plans. These are request limits, not a verified download allowance. Commercial terms, download accounting, pricing and Turkish coverage still need confirmation before choosing it for Neden.
  - https://subsource.net/api-docs
- **Local transcription with whisper.cpp:** Can generate timed subtitles without a subtitle-catalogue quota, using the user's hardware. It supports Apple Silicon and other desktop platforms. Requires a model download and processing time; overlapping dialogue, music and names can produce errors. This is a fallback, not a guarantee of transcript accuracy or a translated second track. Cloud transcription instead creates a cost per audio duration and requires explicit audio-upload consent.
  - https://github.com/ggml-org/whisper.cpp

Commercial API access does not itself grant ownership of subtitle files. Confirm permitted caching, retention and distribution for the intended service before introducing a shared subtitle cache. Do not pool personal keys to evade allowances.

## Implementation boundary

The current desktop SubtitleService handles OpenSubtitles credentials, hash/title search and downloads directly. Automatic lookup currently requests Turkish; manual lookup accepts Turkish or English. Cloud currently covers AI explanations and word lookup only.

A future Cloud subtitle implementation should:

1. Keep provider credentials in Worker secrets. Use the existing Neden session and subscription checks for a narrow search/download API; never expose an arbitrary fetch proxy.
2. Use provider adapters behind one search-result model: provider ID, subtitle ID, language, release, match evidence, format and hearing-impaired status. Match the actual release, not only the episode title.
3. Respect provider rate limits and account quotas, deduplicate concurrent downloads and retry only appropriate transient failures. Local reuse and any provider-approved shared caching should avoid charging for identical retries.
4. Send only the video fingerprint and necessary title/release metadata for catalogue search. Make audio upload an explicit separate action for hosted transcription.
5. Keep generated, translated and human-authored subtitles distinguishable. Validate formats, file sizes, encoding and archive contents before loading; protect existing user files.
6. Replace the fixed Turkish/English acquisition restriction with supported language codes and saved language preferences. Keep explanation language independent from subtitle-search language.

## Settings shipped with this investigation

AI & accounts shows one connection mode at a time. Cloud sign-in is separate from personal provider credentials. Advanced provider controls, languages/context and OpenSubtitles setup are expandable; subtitle appearance keeps its most common controls visible. No provider subscription or new hosted subtitle capability was purchased or enabled.
