# Website visual review — 2026-10-26

Browser plugin, local built Astro preview, 1440×1000 desktop and 390×844 phone.
Reference: generated workflow concept `exec-53119dff-4002-4c34-8cb2-89456f974323.png`.
Rendered review: `/tmp/neden-website-qa/workflow.png`; hero/full page, downloads,
and phone captures are in the same temporary QA directory. These paths identify
local review artifacts, not build dependencies.

The reference and rendered workflow were opened together with the image viewer.

| Area | Result |
| --- | --- |
| Layout | Preserved the open two-column workflow and numbered steps. The full real app fits within the content width. |
| Typography | Inter, strong two-line heading, readable body copy, quiet numbered labels. Reference type was scaled to the 1200px content grid. |
| Palette | White, near-black, muted slate, blue accents. Button blue is darker for contrast. |
| Spacing | Shared 8px scale, 64px column gap, thin separators, generous section padding. |
| Copy | Concrete open / look up / replay workflow retained; no generated UI labels presented as real features. |
| Assets | Replaced the illustrative ferry and invented player chrome with real Neden captures of Leyla ile Mecnun, as requested. |
| Responsive behavior | Single-column phone layout, no horizontal overflow, enlarged screenshot links, wrapped feature rail. |

Verified navigation to walkthrough/downloads and installation/privacy pages,
FAQ expansion, all image loads, and website type/lint/build checks. Download
buttons are supplied by the release manifest only after their exact platform
artifacts pass installer validation.
