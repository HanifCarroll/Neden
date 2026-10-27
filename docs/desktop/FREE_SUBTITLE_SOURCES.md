# Free subtitle source checks — 2026-10-26

Paid subtitle API subscriptions are deferred. No new paid plan or provider account was created, and no new subtitle service was silently enabled in Neden.

## Subvault / Gestdown

The project's [API documentation](https://github.com/Belphemur/AddictedProxy/blob/dev/docs/api-surface.md) describes show search, episode search and SRT downloads at `api.gestdown.info`. The source combines Addic7ed and SuperSubtitles results. Its current API returns opaque base62 IDs; clients must use returned identifiers, not construct them from titles.

A live, unauthenticated check found **Leyla and Mecnun (Leyla ile Mecnun)** and available seasons 1, 2 and 3. Two returned SuperSubtitles NF.WEBRip entries were downloaded and parsed using Neden's actual SRT parser:

| Request | HTTP result | File size | Parsed cues |
| --- | --- | --- | --- |
| Season 1, episode 1, English | 200 | 94,041 bytes | 1,188 |
| Season 2, episode 17, Turkish | 200 | 126,235 bytes | 1,429 |

Both files were readable UTF-8 SRT. This verifies search, download and parsing for those samples, not subtitle accuracy, synchronization with every release, complete show coverage, unlimited access, or suitability for commercial redistribution. Downloaded dialogue was kept outside the repository and is not a test fixture. An initial Python HTTP client received 403, while a normal curl client succeeded; availability/client compatibility needs further assessment before shipping an integration. The documented API includes throttling and temporary refresh responses, which a future provider must handle.

## Other free tiers

[SubSource's official documentation](https://subsource.net/api-docs) lists a free key tier of 60 requests/minute, 1,800/hour and 7,200/day. It requires a key created in an account dashboard. No key was available, so its download path has **not** been validated. These are request limits, not a promise of that many successful subtitle downloads or unrestricted redistribution.

## Recommendation

Subvault is a promising free TV source for a later explicit integration, but the samples above are not enough to make it the sole automatic provider. Keep matching release/cut information visible, normalize provider IDs behind a common adapter, respect throttling, and require a successful local parse before replacing a track. Embedded text, user-provided SRT/VTT, timing repair and translation work independently of a subtitle download subscription.

## Free API allowance recheck — 2026-10-27

- **SubDL v2:** the [current developer documentation](https://subdl.com/developers) lists **2,000 searches/day and 50 downloads/day** on a free account API key. Its `/api/v2/me` endpoint reports each allowance separately. The older v1 documentation has different figures; use the current v2 limits when planning a new integration. These are published limits, not a newly validated account allocation. A shared Neden key would need an allocation suitable for all its users; this is not a per-customer unlimited-download plan.
- **Subvault / Gestdown:** [official API documentation](https://github.com/Belphemur/AddictedProxy/blob/dev/docs/api-surface.md) says every endpoint is public without authentication and throttling is applied at the proxy/infrastructure layer. It documents 429 responses but does not publish a guaranteed numeric daily allowance. The prior unauthenticated sample checks demonstrate free access, not unlimited capacity or an SLA. No paid plan is required for those documented endpoints.

Neither result justifies promising unlimited hosted subtitle downloads. Keep provider requests bounded, respect retry responses, and retain local/embedded subtitle paths when a community source is unavailable.
