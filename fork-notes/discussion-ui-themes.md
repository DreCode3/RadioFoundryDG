<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->
<!-- Final text for the upstream GitHub Discussions board (Ideas). Fork-only: post the text below the line, not this file.
     Facts as of feat/gd88-gui at 43ffb323 plus the docs commits after it (Nov 27, 2026); sizes from the CI size report at e5104c5e
     (../rf-work.noindex/ci-e5104c5e/size-report.md). Attach the images listed at the end; copies with a README are in
     ../rf-port-out/discussion/. The PR series in detail: upstream-pr-plan.md. -->

---

**Title:** Proposal: a UI theme framework, a commercial-style menu tree, and a second look built on them

## Summary

I'd like your feedback before I open pull requests. On my fork I have built, and tested on the host and in CI:

1. **A theme framework.** Every radio keeps one shared UI stack for behavior: the menu tree, settings, keys,
   navigation, Find and Quick. A theme only decides how screens look. Today's look becomes the reference theme
   ("Original") with no visual change.
2. **A reorganized menu tree** that follows the conventions commercial radios share: a 3x3 Menu that matches the
   keypad, and Settings in nine groups of one topic each.
3. **A second theme, "panels"**: rounded panes, a two-label soft key bar, Day and Night palettes, and a Commercial and
   an Amateur Home layout. It fits color displays from 158 x 127 up, in a full layout from 220 x 200 and a compact one
   below that.

The framework is the part I'd most like to agree on, because the rest builds on it.

## The problem

Before this work, a second look meant `#if` branches across `core/ui`: the first version of the GD-88 profiles added
about 110 of them in 12 files. Every further look would multiply them, and every screen change would have to be made
once per look. The pieces a framework needs already existed: `ui_colors_t` is a set of color tokens, `ui_layout_t`
holds the fonts and regions, and Dark, Light, High contrast and Night are palettes.

## The design

### What is shared and what a theme draws

| Shared by every theme (one stack) | A theme provides |
|---|---|
| The menu tree, settings, key meanings, navigation, Find, Quick's tiles and values, the text and its translations, the hints' words, the debug link's view keys | Palettes, fonts, metrics, icons, and how the status strip, titles, the bottom row, banners, list rows and grid tiles, Quick, Home and dual watch's Home are drawn |

A theme never adds, removes or reorders a menu entry, never changes what a key does, and never hides a feature. So
the task flows and press budgets (`uiFlowTest`) hold for every theme. Shared code never asks which theme is in use;
it tests an option the theme declares.

### The descriptor

As built (`core/ui/ui_theme.h`), in the style of `board_t` and the driver ops tables:

```c
typedef struct ui_theme {
	const ui_colors_t *palettes;	// by palette, before ops->colors
	uint8_t palette_count;		// at least 1
	uint8_t id;			// UI_THEME_ID_*: 1 reference, 2 panels (stored in the settings)
	uint8_t options;		// UI_THEME_OPT_*: the appearance options it honors
	const ui_theme_ops_t *ops;	// a NULL op is the reference theme's
	bool (*fits)(void);		// can it draw on this display (NULL: every display)
} ui_theme_t;
```

The ops are `layout`, `colors`, `top` (strip and title), `bottom` (hints, soft keys, toasts), `banner`, `home`,
`home_dual`, `quick`, the list ops `list_head`, `list_geom`, `cell` and `scroll`, and `animate` for marks that move.
`ui_draw_list` keeps a list screen's frame and scrolling for every theme; grids and value editors are lists drawn by
the cell op. Each theme lives in `core/ui/themes/<name>/` behind `CONFIG_UI_THEME_<NAME>`.

### Two layers

- **Data**, which most contributors can write: a descriptor with palettes, the options it honors and a fit test, and
  every op NULL. It draws every screen as the reference theme does, in its own colors (the host test proves exactly
  that). Fonts and regions take a `layout` op of a few lines.
- **Ops**, for a composition of its own: its own strip, Home, list cells or soft key bar. This also lets a radio with
  a new resolution get a look that fits without reworking the shared UI. Panels gives every op its own.

One difference from my first sketch: there is no shared metrics or icon-set type in the descriptor. Metrics and icons
belong to the ops that draw them; panels keeps its own tables.

### Options, storage, fit

- **Appearance options** the theme declares (`UI_THEME_OPT_*`). Settings > Display shows the rows of the theme in use
  only: Original has Colors, Highlight color and Filled status bar; panels has Display profile, Day/Night and GPS
  clock. Two options are behavior flags: a radio that starts in panels starts with Talkgroup name first on, and the
  soft key bar stays when Hint row is off.
- **The theme is stored** in the last spare byte of the old `reserved11` (`gui_theme`, page 1 offset 407), 0 meaning
  the build's default, so older records read as before. A board can name its default (`CONFIG_UI_THEME_DEFAULT`). A
  stored theme the build lacks or the display doesn't fit falls back to the default, then to Original.
- **Settings > Display > Theme** appears only where at least two themes fit the display.

### The cost rule

A theme switch that is off costs nothing: every definition only a theme uses is guarded like its callers. With the
reference theme alone `ui_theme_cur()` is a constant, so every op call resolves at build time; a prototype with direct
calls instead gave an image of byte-identical size. The split itself costs about 200 B against the code it replaced.

## What is built and tested

| Piece | State |
|---|---|
| Reference theme behind the descriptor | Done with no visual change: every frame identical, compared with the fork's pixel-baseline scripts. Upstream's tools show the same: `make -C tests/host ui-gallery` leaves `docs/ux/screens` unchanged |
| Menu tree, settings groups, naming | Done: 3x3 Menu (Contacts, Messages, Call log, Zones, Scan, Location, Connections, Settings, 9 free), nine Settings groups plus Radio info, a page's title is the row that opens it. Task flows, press budgets, information architecture, HIL tests and translations updated |
| Panels theme | Done: full layout (310 x 240 and 220 x 310), compact layout (158 x 127 to 210 x 176), 23 icons drawn from first principles by a generator script, Day and Night |
| Text that fits | Text alternates (`{long|short}`) for narrow screens, whole-word short forms in English, Spanish, Italian, French and Chinese, never a font under 8x16 in panels |
| Host tests | `uiThemeTest` (the framework with test themes), `uiThemeTestPanels` and `uiThemeTestPanelsOriginal` (panels frames checked by structure), `uiPrototypeTest.py` (panels at 310 x 240), the layout check (`uiSim --layout`) over every gallery class including the panels ones, `uiFlowTest` (every task flow and press budget on four display and key map pairs, and the crawler, updated for the new menu tree; it runs the reference theme) |
| CI | `tools/ci.sh` on every board at `e5104c5e` (Nov 27): all 802 firmware configurations built with zero warnings. The board registry check failed on a comment that named radios; `cee9bbcd` fixed it, and the checks step re-run at `43ffb323` passed. The fork's pixel-baseline scripts (not in upstream) compared 2,755 rendered frames with 0 unintended changes; upstream's equivalent is the gallery diff and `uiSim --layout` |
| Not done | HIL and on-air checks of the new UI; native-speaker review of the French, Italian, Spanish and Chinese wording |

Docs on the branch: a Themes section in `docs/spec/ui.md`, `docs/dev/ui-themes.md` (how to add a theme),
`docs/ux/panels-theme.md` (the design rationale), and the user manual.

## Radios and display sizes

The fit rule is by display class, not by radio: color, at least 158 x 127 drawable inside the insets, the full layout
from 220 x 200. `config/features.mk` derives "unsupported" from each board's gallery class, so nothing is listed by
hand.

| Radio | Drawable area | Panels |
|---|---|---|
| DM-32UV | 220 x 310 | Default (full layout); Original selectable |
| GD-88 | 310 x 240, or 210 x 176 with the smaller panel | Default (full, or compact on the small panel); Original selectable. Needs the bring-up branch |
| MD-380 | 158 x 127 | Selectable (compact); starts in Original |
| DB25-D PRO | 160 x 128 | Selectable (compact); starts in Original |
| MD-UV380 | 158 x 127 | Fits, but off: the default image has no room for it |
| DM-1701 | 150 x 120 | Too small: Original only |
| DM-1801 Pro, MD-9600 | 128 x 64 mono | Original only |

## Measured cost

Image sizes from `tools/ci.sh` at `e5104c5e` (default builds; "panels off" is ci's `no-UI_THEME_PANELS` build):

| Radio | Default image | Free | Panels off | Panels costs |
|---|---:|---:|---:|---|
| DM-32UV | 804,532 | 96,588 | 785,668 | 18,864 B (full layout) |
| GD-88 | 798,552 | 98,472 | 775,160 | 23,392 B (both layouts) |
| MD-380 | 618,660 | 249,692 | 583,876 | 34,784 B, of which 17,348 B are the anti-aliased fonts panels draws (the original look keeps the bitmap ones) |
| DB25-D PRO | 768,408 | 128,616 | 733,568 | 34,840 B, of which 17,404 B fonts |
| DM-1701 | 710,952 | 26,328 | not built | 0 |
| MD-UV380 | 712,284 | 24,996 | not built | 0 |

On the radios without panels, what the branch adds over upstream `main` is shared behavior only. Measured on the
DM-1701 and MD-UV380 maximal builds (LTO, at `fa8e74cd`): about +7.6 KB, of which 2.6 KB is the bring-up branch and the
rest the menu tree, settings rows, text alternates, fit helpers and translations; none of it is panels code. Three
changes then recovered space without changing a frame: language tables with an offset every 16 strings (448 B),
20-byte Settings rows (400 B), and LTO for the third-party code in those two boards' maximal builds (about 3.7 KB).
Default images came out 384 to 720 B smaller than before them. That was enough for the DM-1701 (1,016 B free in its
maximal build) but left the MD-UV380's 272 B short, so its maximal build also leaves `FONT_AA` out (6,080 B; its
default image draws no anti-aliased font): 5,816 B free. The cost is that the maximal build no longer covers `FONT_AA`
on that board; the MD-380, DB25-D, DM-32UV and GD-88 builds still do.

## Dependency on `feature/gd88_bringup`

My branch sits on your `feature/gd88_bringup` (`c13059ce`) plus upstream `main` merges (5 commits behind `main` today).
Only the GD-88's own board change needs the bring-up branch: the framework, the menu tree and the panels theme touch
`core/ui`, the settings store and other boards, and can be cut onto `main` and go in first. I'd propose that order,
with the GD-88 change after the bring-up merges.

## Proposed pull requests

Each one topic, each with its host tests, its gallery PNGs before and after, and its translations:

1. **Space first:** the three size changes above, one small PR each, so later PRs don't fail the 720 KB boards'
   maximal builds. The MD-UV380's `FONT_AA` skip goes with the PR that needs it, not here: on `main` its maximal build
   fits with `FONT_AA`.
2. **UI: the reference look behind a theme descriptor.** No visual change; `uiThemeTest`.
3. **UI: the menu tree, Settings groups and names.** Changes every radio's menus; updates the task flows, press
   budgets, HIL tests, information architecture and translations.
4. **UI: text that fits narrow screens.** Short forms and whole-word translations, the wrap fix.
5. **UI: the panels theme, full layout,** with the DM-32UV as its first default; `uiThemeTestPanels`,
   `uiPrototypeTest.py`.
6. **UI: the panels theme's compact layout.** Its own gallery classes.
7. **Per radio:** MD-380 and DB25-D selectable, MD-UV380 kept off, and the GD-88 after the bring-up.
8. Optionally, the UI simulator in a browser (`tests/host/uiLive.py`).

## Questions for the maintainers

1. Is the shared-versus-theme boundary the one you have in mind, with options declared on the descriptor rather than
   tests of the theme in shared code?
2. Are you comfortable with the two layers as built (data: palettes, options, fit; ops for the rest), or would you
   want shared metrics and icon types in the descriptor?
3. Naming: the whole look is now "Theme" (Original, Panels) and the reference palette row is "Colors". The code still
   calls the palettes `UI_THEME_DARK` ... and the Colors setting `UI_SET_THEME`. Rename in a follow-up?
4. Would you take the menu tree change for every radio, and before or after the framework?
5. Main first and the GD-88 after the bring-up, as above?
6. The few shared fields only panels reads (`ui_list_t.kind`, the hint row's OK and Back flags, menu icon ids) are
   guarded by `CONFIG_UI_THEME_PANELS`. Widen them to "any second theme" now, or when a third theme comes?
7. The default per radio: panels by default on the DM-32UV (and GD-88), selectable on the MD-380 and DB25-D. Agreed?
8. On the 160 x 128 radios panels brings the anti-aliased fonts beside the bitmap ones (about 17 KB). Is that worth it,
   or should panels draw the bitmap fonts there?
9. The gallery gains three panels classes (189 screens each) and the DM-32UV class switches to panels. Is that the
   review load you want, and should the palette variants (which panels ignores) be skipped in those classes?
10. On small screens the focused list row slides a long label sideways (never on Home). Acceptable under P5?

## Images

Attached: Commercial and Amateur Home during a call, the 3x3 Menu, Settings in Night (310 x 240); Home, Quick and
Settings > Display on the 220 x 310 screen; Home, a call and the Menu on the 158 x 127 screen; Home on the 210 x 176
screen; and the same Home in the Original look for comparison.
