# Finding subtitles

Open a video, then choose **Subtitles → Find subtitles…**. Pick a source and
language, enter the title, and supply the season/episode for TV. Choose the
matching title, then a release. Neden downloads an individual SRT, validates it,
saves a private local copy, and loads it as the primary track. Check timing against
the dialogue; different edits of a film or episode can need adjustment.

| Source | Setup | Search |
| --- | --- | --- |
| Subvault | No account or key | TV episodes; choose title, season, episode and language |
| SubDL | Free account key in Settings → AI & accounts → Subtitle downloads | Movies and TV; individual files within season packs |
| OpenSubtitles | Existing API key and download account | Video fingerprint or title; optional automatic matching |

SubDL's live free-account API reported 2,000 searches and 50 downloads per day on
2026-10-27. A free-key search and file download succeeded. Limits are controlled by
the provider and may change. Subvault exposes public endpoints without a key; it
can throttle requests or temporarily lock a show while refreshing it. Neden shows
these errors and lets the user retry or select another source. It does not rotate
accounts, bypass quotas, or promise unlimited downloads.

Keys are saved in the operating system's credential store, never in profiles,
result labels or installers. The SubDL key is sent in a header only to its API
and download origin. Provider URLs containing a key are stripped of their query
before entering the result model. Redirects are refused. Switching videos or
starting another search cancels pending work. Downloads are bounded to 8 MiB;
ZIP archives, external download origins and invalid/non-UTF-8 SRT data are rejected.

Provider references:
- [SubDL API](https://subdl.com/developers)
- [Subvault API surface](https://github.com/Belphemur/AddictedProxy/blob/dev/docs/api-surface.md)

## Developer checks

The regular `study_tests` suite uses loopback servers to exercise title selection,
pack filtering, download bytes, credential isolation, cancellation, redirects and
quota/refresh errors without spending provider allowance. The desktop UI check
verifies the source and language pickers.

An opt-in live check downloads **one** subtitle and prints only status/cue counts:

```sh
cmake --build build/desktop-core --target subtitle_catalog_check
./build/desktop-core/subtitle_catalog_check subvault 'Show title' en 1 1
# Supply SUBDL_API_KEY in the process environment for the SubDL variant:
./build/desktop-core/subtitle_catalog_check subdl 'Movie title' en 0 0
```

A local `.env` may hold the development key (see `.env.example`); it is ignored.
The installed app does not load checkout secrets. Use its credential controls to
save a personal key on each device. No maintainer credential is distributed.
