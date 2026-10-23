# Third-party materials

Neden's original source is covered by the root [MIT license](LICENSE). Dependency licenses apply separately.

| Material | Use | License and provenance |
| --- | --- | --- |
| Inter 4.1 | Bundled desktop fonts | SIL OFL 1.1; [full notice](shared/fonts/LICENSE.txt), [upstream release](https://github.com/rsms/inter/releases/tag/v4.1) |
| Lucide icons | Bundled desktop controls | ISC; [full notice](shared/icons/lucide/LICENSE), [source commit and adaptations](shared/icons/lucide/SOURCE.md) |
| Neden branding | App icon | Generated for this project; [provenance](shared/branding/README.md) |
| markdown-it 15.0.2 and dependencies | Plugin Markdown rendering | MIT and dependency notices; pinned by `bun.lock`; `scripts/copy_plugin_notices.ts` copies the package notices into plugin builds |
| Qt | Desktop UI and networking | Dynamically linked; see [module and distribution notes](docs/desktop/DEPENDENCIES.md) |
| QtKeychain 0.17.0 | Desktop credential storage | Modified BSD; pinned upstream source and COPYING are included by the build |
| libmpv, FFmpeg, and supporting libraries | Desktop playback | Reviewed dynamic runtime; exact versions, build configuration, notices/source requirements in [the dependency guide](docs/desktop/DEPENDENCIES.md) |

Development tools and transitive dependencies keep their own licenses. The current checkout does not vendor the playback binaries or Qt. Local installation downloads/copies dependencies and is a separate step from source publication. Complete public-binary redistribution materials are still outstanding; do not treat this index as a replacement for those materials.
