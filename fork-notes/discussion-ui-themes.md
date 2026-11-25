<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->
<!-- Draft for the upstream GitHub Discussions board (Ideas). Fork-only: post the text below the line, not this file. -->

---

**Title:** Proposal: a theme framework for the UI, and a menu layout based on commercial radios

## Summary

I'd like to propose two related changes and get your feedback before I open any pull requests:

1. **A theme framework.** Every radio keeps one shared UI stack for behavior: menus, settings, keys, navigation,
   Find and Quick. A theme changes only how the screens look. Adding a new look then means writing a theme, not
   adding `#if` branches across the UI.
2. **A reorganized menu tree** that follows the conventions commercial radios share, so that common settings are
   easier to find and each settings group covers one topic.

I've been building a second look for the GD-88 (the Commercial and Amateur display profiles on my fork) and have
used it to find where the current code makes a new look hard. The theme framework is the part I'd like to agree on
first, because the rest builds on it.

## Why a framework

The display profile work today adds about 110 `CONFIG_UI_PROFILES` and `gui_profiles` branches across 12 files in
`core/ui`. It works, but every new look would multiply those branches, and every screen change would have to be made
once per look. The pieces a framework needs already exist: `ui_colors_t` is a set of color tokens, `ui_layout_t` holds
the fonts and region positions, and the Dark, Light, High contrast and Night themes are palettes.

## What is shared and what a theme provides

| Shared by every theme (one stack) | A theme provides (presentation only) |
|---|---|
| The menu tree, settings, key meanings, navigation, Find, Quick, translations, the debug link views | Palettes (day and night variants), fonts, metrics (row height, insets, grid), an icon set, and how these are drawn: list rows, grid tiles, toggle tiles, pick lists, value editors, the soft key bar, the status strip, banners, Quick settings, Home |

A theme never adds, removes or reorders menu items, and never changes what a key does. That keeps the UI tests that
check behavior (the task flows and press budgets) valid for every theme.

## A possible shape

A theme is a descriptor, in the style of `board_t` and the RF driver ops tables:

```c
typedef struct {
	tr_id_t name;				// shown in Settings > Display
	const ui_colors_t *day, *night;		// palettes
	const ui_metrics_t *metrics;		// fonts, row pitch, insets, grid columns
	const ui_icon_set_t *icons;		// NULL: text only
	const ui_theme_ops_t *ops;		// drawing; a NULL entry falls back to the reference theme
	bool (*fits)(int w, int h, bool color);	// the display classes it supports
} ui_theme_t;
```

Themes would come in two layers. Most themes need only data: palettes, fonts, sizes and an icon set, which anyone
can write without touching drawing code. Contributors who want a different composition (their own Home, status strip
or list cells) fill in drawing ops as well. The ops layer also matters for new radios: a radio with a new resolution
or new features can get a look that fits it without reworking the shared UI.

`ui_theme_ops_t` would have one entry for each drawn element (status strip, title, list row, grid tile, toggle tile,
pick list row, value editor, soft key bar, banner, Quick grid, Home). A theme fills in what it changes and inherits the
rest. Each theme sits in its own directory under `core/ui/themes/`, behind its own `CONFIG_UI_THEME_<NAME>` switch, so a
radio pays flash only for the themes it builds. The user picks a theme in Settings > Display, among those that fit the
display.

## Proposed order of pull requests

1. **Refactor, no visual change.** Move today's drawing behind the descriptor as the reference theme. The UI gallery
   must come out identical, which proves nothing changed.
2. **A second theme.** Move the GD-88 profile look onto the framework and remove its `#if` branches from the shared
   code.
3. **Theme tests.** Every theme draws every gallery screen on every display class it supports, and the layout checks
   in ui.md section 7 run for each one. The gallery gets one folder per theme.
4. **The menu tree** (below), as its own pull request, since it changes every radio's menus.

## The menu tree

I've used two-way radios for about 30 years, mostly commercial ones. I compared the menus of current commercial
radios (Motorola MOTOTRBO, Hytera, Kenwood NX, Tait, Icom and L3Harris) with amateur DMR radios. The commercial
vendors agree on a few conventions:

- Communication comes first, configuration last: Contacts, Messages and Call log, then Zone and Scan, then Settings.
- Scan is a top-level item next to Zone.
- Settings is split into a few groups, each about one topic. A long flat list (MOTOTRBO's Radio Settings,
  Anytone's Radio Set) is the pattern users complain about.
- No setting is more than three levels down, and Radio info is read-only and comes last.

Proposed main menu (a 3x3 grid that matches the keypad):

| # | Item | Notes |
|---|---|---|
| 1 | Contacts | |
| 2 | Messages | Zone offers move here |
| 3 | Call log | Today's Recent |
| 4 | Zones | |
| 5 | Scan | With Roaming |
| 6 | Location | GPS status, APRS, Satellite, and the location settings |
| 7 | Connections | Hotspot, USB network, SFR, data services |
| 8 | Settings | |
| 9 | Free | Later: a user-chosen favorite |

Proposed Settings groups: Display, Tones & alerts, Audio, Call settings, Receive & VFO, Privacy, Security & keypad
lock, Buttons, Programming (channel and zone editing, the VFO and the service items), and Radio info. Today's Talk &
listen group, which holds 14 unrelated settings, is split between Call settings, Audio, Receive & VFO and
Connections. Tools is replaced by Scan, Location, Connections and Programming.

No feature is removed and no menu item is hidden. The numbered key paths change, so the pull request would update
the task flows, press budgets, information-architecture.md and the translations together.

## Questions for the maintainers

1. Is the shared-versus-theme boundary above the one you have in mind?
2. Are you comfortable with the two layers, data for most themes and drawing ops for the advanced ones? Or would you
   rather start with data only and add ops later?
3. Naming: "Theme" today means a palette (Dark, Light, High contrast, Night). Should the whole look be a "theme"
   and those become "palettes", or do you prefer another word for the look?
4. Would you accept the menu tree change for all radios, and should it wait until the framework is merged?
5. The display profiles depend on the GD-88 bring-up branch. Should the framework pull requests go into `main` first,
   with the GD-88 theme following once the bring-up is merged?

Screenshots of the current GD-88 profiles are attached (Commercial and Amateur Home, Menu, Settings in Night).
