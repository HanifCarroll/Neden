# Maintainer checklist

## Repository metadata

The original repository name is retained for IINA update compatibility. The intended GitHub About description is:

> Neden: a desktop video player for language learning, with dual subtitles, contextual word meanings, AI explanations, and an IINA plugin.

An authenticated repository administrator can apply it with:

```sh
gh repo edit HanifCarroll/iina-language-learning-plugin \
  --description 'Neden: a desktop video player for language learning, with dual subtitles, contextual word meanings, AI explanations, and an IINA plugin.'
```

Do not change visibility, rename the repository, or publish binaries as a side effect of a code update. Renaming requires checking both copies of `Info.json` and existing IINA update behavior.

## Checks and dependencies

The GitHub workflow runs portable desktop tests and the plugin's full package check on macOS, plus Python helper checks and a redacted history secret scan. Actions are pinned to commit hashes; Dependabot proposes updates. These jobs do not launch the desktop app, install over a user's app, use saved credentials, or contact paid model endpoints. Build tools and dependencies are downloaded from their normal public sources.

Full desktop playback tests, QML lint, and native checks remain separate local gates. Follow [CONTRIBUTING.md](../CONTRIBUTING.md) and the [installation guide](desktop/INSTALLATION.md). Before changing a pinned runtime or QtKeychain archive, verify the upstream release, checksum, license and platform behavior, then update the dependency documentation. Bun dependencies are locked by `bun.lock`; use `bun install --frozen-lockfile` in checks.

Use synthetic or redistributable test media and a temporary profile. Keep raw logs, benchmarks, screenshots with private content, and machine-specific tool settings out of tracked files. Review attribution and licenses for any new asset or copied implementation.

## Publication and history

A source publication check should cover every branch/tag being published, commit messages, deleted files in history, asset provenance, and current setup instructions. A secret scanner alone cannot assess private screenshots, copyright, or whether all platform features actually work. Record the checked revision and remaining limits separately from a release claim.

For any history cleanup, keep a verified local backup bundle, prepare the rewritten history in a separate clone, compare the current source trees, scan the candidate again, and coordinate with contributors before updating remote refs. Use an expected-old-commit guard for the push. Other clones need to resynchronize; merging an old branch can reintroduce removed content. GitHub caches and existing clones are outside the scope of a local rewrite. See [GitHub's history-removal guidance](https://docs.github.com/en/authentication/keeping-your-account-and-data-secure/removing-sensitive-data-from-a-repository).

Source publication and binary releases are separate. The [dependency guide](desktop/DEPENDENCIES.md) lists the still-required notices, corresponding library sources, and replacement/relinking information for desktop binary distribution. A local signed installation is not a public release package.
