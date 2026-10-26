# Neden website

Static Astro site with Tailwind 4 and shadcn/ui (Radix). Run commands from the
repository root:

```sh
bun install --frozen-lockfile
bun run dev:website
bun run check:website
bun run build:website
```

`src/styles/global.css` contains typography, colors, spacing, and shared layout
styles. `src/components/ui` contains the shadcn primitives. The FAQ is the only
hydrated React component; primary navigation, images, downloads, and a native FAQ
fallback work without JavaScript. Fonts are self-hosted.

## Releases

`src/data/releases.ts` is the single source of truth for platform download links.
Add a URL only after validating the exact published package, its platform
requirements, checksums, and dependency/source notices. Never substitute a source
archive for a button that promises an installer.

## Deployment

Authenticate Wrangler to the Cloudflare account that manages `hanifcarroll.com`,
then run `bun run --cwd apps/website deploy`. The checked-in configuration serves
`dist` as static assets at `neden.hanifcarroll.com`; no server, database, or secrets
are needed in the site. Account credentials stay outside Git. Verify the live
homepage, platform links, privacy/installation pages, and 404 after deployment.

Screenshot provenance is in `docs/website/sample-media/README.md`; the show imagery
is not included under the application source's MIT license. Design decisions are
in `docs/website/DESIGN_SYSTEM.md`.
