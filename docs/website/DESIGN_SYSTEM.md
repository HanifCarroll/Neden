# Website design system

The website is an Astro static site in `apps/website`. Tailwind 4 supplies utilities;
shadcn/ui (Radix Nova) supplies accessible interaction primitives. React is hydrated
only for the FAQ accordion. Product screenshots come from the actual desktop UI.

## Visual specification

Three coordinated concepts define the hero, workflow, and downloads/FAQ sections.
The layout is flat and editorial: open white space, thin rules, one large product
image, and no decorative card grid. IINA's product-first presentation and
asbplayer's concrete workflow informed the information architecture, not the copy.

- Background: true white `#ffffff`; foreground: near-black `#15191e`.
- Primary: blue `#086bd9` for accessible white button text; display accent
  `#0879f9` for large text. Muted text `#586474`; rules `#dde3ea`.
- Typography: self-hosted Inter variable. Display 40–72px, section 32–52px,
  subheading 22px, body 17–20px, controls 16px, captions 13–14px.
- Spacing: 4px base; 8/12/16/24/32/48/64/96/128px scale.
- Content width: 1200px, page gutters 24–48px; reading width 680px.
- Buttons: 48px high, 8px radius, 16px medium text, 20px outline icons.
- Screenshot frame: 12px radius and restrained shadow; never used for body content.
- Icons: Lucide, 1.75px stroke. Platform names remain textual; OS marks are optional.
- Motion: short opacity/color transitions, reduced-motion support, no autoplay media.

## Components and behavior

`SiteLayout` owns metadata, header, footer and global styles. `LinkButton` uses
the shadcn button variants for real anchor links. `Downloads` reads one typed release
manifest; missing verified assets produce an explicit availability label, never a
fabricated download. `Faq` uses shadcn Accordion, with a no-JavaScript fallback.
Screenshots are responsive optimized images with useful alt text and captions.

Mobile stacks the hero actions, workflow and platform list. Navigation remains
visible without a hamburger menu. All focus states are visible and links work
without JavaScript. The only client dependency is progressive FAQ enhancement.

## Intentional concept adaptations

- Actual screenshots replace illustrative concept UI; no invented product controls.
- Platform requirements and availability come from verified release artifacts,
  replacing the concept's provisional requirements and download labels.
- The smaller button blue is darker than the concept to meet contrast requirements.
- A linked installation guide and privacy page complete the download journey.

References: [IINA](https://www.iina.io/), [asbplayer](https://docs.asbplayer.dev/),
[mpv](https://mpv.io/), [shadcn Astro setup](https://ui.shadcn.com/docs/installation/astro).
