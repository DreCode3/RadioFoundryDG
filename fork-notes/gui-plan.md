<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# GUI plan and decisions (fork-only)

Decisions made with the user for the GD-88 GUI work. Upstream docs (`docs/ux/`, `docs/spec/ui.md`) are updated
as each piece is built; this file is the working record until then.

## Goal

A shared theme framework: one stack for behavior, themes for presentation, so a developer can add a new look
without touching menus or behavior. The maintainer has wanted this; the proposal goes to the upstream Discussions
board first ([draft](discussion-ui-themes.md)).

| Shared by every theme | A theme provides (presentation only) |
|---|---|
| Menu tree, settings, keys, navigation, Find, Quick, translations | Palettes (day and night), fonts, metrics, icon set, how lists, grids, toggle tiles, pickers, the soft key bar, the status strip and Home are drawn |

Themes never change the menu structure or behavior. The current look becomes the reference theme ("Original
RadioFoundry"); the user's look is the second theme, with Commercial and Amateur as its two Home layouts. No feature
or menu item is hidden by theme or layout.

### Behavior every theme shares (decided)

The two looks differed in these places; each is now shared behavior, and a theme only decides how it looks:

1. During a DMR call Home shows the caller (name or ID), the destination talkgroup and the signal.
2. The status strip shows the same set of states in every theme: TX/RX, signal, battery, lock, scan, monitor, unread
   messages, GPS (including searching), USB network, privacy. Icon or word is the theme's choice.
3. Large text works in every layout.
4. "Talkgroup name first" (key and setting) works in every theme.
5. Menu tiles show their values (unread count, current zone, scanning), as a badge or a second line.
6. Dual watch keeps its call history and the caller's contact name.
7. Each theme declares its own appearance options; Settings > Display shows the active theme's rows (the reference
   theme: Theme, Highlight color, Filled status bar; ours: Day/Night). Appearance options are not features.

Themes are layered (decided): data (palettes, fonts, metrics, icons) that most users can write, and an ops table
for elements whose composition differs (Home, status strip, list cell, soft key bar) for advanced contributors. The
ops table also lets a new radio with a new resolution or new features get a fitting look without reworking the
shared UI.

Technical choices: per-cell list drawing ops; icons as a data table drawn by one shared blitter; the reference code
stays in `ui_screens.c` until upstream agrees on the shape; the theme id goes in the spare settings byte with 0 = the
build's default. Work goes ahead without waiting for the maintainers' reply to the discussion.

## Menu tree (shared by all radios, commercial conventions first)

Main Menu, a 3x3 grid matching the keypad:

| # | Item | Contents |
|---|---|---|
| 1 | Contacts | Talkgroups, People, Listen lists, Manual dial |
| 2 | Messages | Inbox, New, Quick text, Sent, Drafts, Zone offers |
| 3 | Call log | Missed, Incoming, Outgoing, last heard |
| 4 | Zones | Zone picker, the zone's channels |
| 5 | Scan | Scan on/off, scan lists, Roaming |
| 6 | Location | GPS status, APRS, Satellite, Location settings last |
| 7 | Connections | Hotspot, USB network, SFR (with its settings), Data services, PC and network settings |
| 8 | Settings | Below |
| 9 | (empty) | Planned: a user-chosen favorite |

Settings (Settings > group > item, never deeper):

| # | Group | Contents |
|---|---|---|
| 1 | Display | Display profile (Commercial / Amateur), Day/Night, GPS clock, Brightness, Backlight, Large text, Hint row, Talkgroup name first, Language, Battery saver, Auto power off, Low battery warning |
| 2 | Tones & alerts | Tone volume, Key tones, Voice prompts, Vibrate; sub-pages Alert tones (the on/off tones) and Signaling tones |
| 3 | Audio | Mic gain, Mic AGC, Mic noise reduction |
| 4 | Call settings | Reply window, TX timeout, Show talker alias, Digital monitor at start, Answer radio checks, Answer call alerts, Message format, Accept zone shares |
| 5 | Receive & VFO | Default squelch, VFO step, FM broadcast step, Dual watch talk back, Dual watch delay |
| 6 | Privacy | Encryption: key type, key |
| 7 | Security & keypad lock | Auto lock, PTT when locked, Knob when locked (later: a PIN) |
| 8 | Buttons | Long press time, Key repeat, Up/Down on Home, programmable keys |
| 9 | Programming | Channels (edit, new), zone editing, VFO, Diagnostics, Crash log, Freq trim, RX past bands, Hardware, Factory reset (last) |
| - | Radio info | Read-only: battery, DMR ID and name, versions, GPS, codeplug. No digit; reached by Up/Down |

0 stays Find inside Settings. Channels leaves the main Menu (Programming has it). Tools is gone (Connections,
Scan, Location and Programming take its items).

## Naming

Call log, Keypad lock, Radio info, Tones & alerts, Call settings, Talkgroup / Contact (not "Talk to"),
Connections. Sentence case everywhere; a screen's title is the text of the row that opens it. Full words, except
common abbreviations (VFO, SFR, DMR, GPS, APRS, TG, CTCSS, DCS).

## Behavior of settings rows

- On/off: OK flips it, the row shows the new state.
- Several values: a pick list with a check mark on the current value.
- Numbers: a value editor with left and right markers, in the theme's accent color. Amber only for warnings.

## Layout (the second theme, judged on the GD-88)

- Grids: one fixed column split for the whole list, not per page.
- One row height everywhere; lists show 5 rows on 310 x 240.
- Lists of only on/off rows with short labels: 2-column toggle tiles with a state chip.
- Focus: filled highlight with a drawn accent bar, no "|" character.
- Scroll position: an "n/N" badge in the title.
- Soft key bar: exactly two labels, one per soft key.
- Quick settings in the theme's style, full words.
- The status strip is the same on every screen.
- One 24 px icon family in the theme ink, accent on focus; color only where it means something (TX red, RX green,
  warnings amber).
- Home: the CPS channel name is the main line, zone above it. Commercial shows the talk target only when it
  differs from the channel's own (temporary target, reply window, a call). Amateur always shows it with the details
  row. Commercial soft keys: Menu | Quick.
- A GD-88 starts in the Commercial layout (`gui_flags` 0).

## Process (decided)

Each step is QA-clean before the next starts. An implementer subagent does the work; an independent adversarial
reviewer (QA) reviews it without the implementer's conclusions; findings go back to the implementer and the loop
repeats until QA approves. Work is pushed as it is committed, but a step counts as done, and the next step starts, only
after QA approval. A disagreement between the implementer, QA or
Claude goes to the user, with both positions, and the user decides.

Testing is sized to the risk, not exhaustive (the user wants safe, not past diminishing returns):

| When | Checks |
|---|---|
| Each commit (~5 min) | uiSim builds with zero warnings; pixel comparison against the baseline; `uiThemeTest`, `uiPrototypeTest`; one GD-88 build with zero warnings |
| End of a step | Whole host suite; GD-88, DM-1701, MD-9600 and the relevant switch-off builds; stack check GD-88 and DM-1701; sizes. Other boards only when shared code outside `core/ui` changed |
| QA, once a step (~30 min target) | Read the diff; experiments where the risk is; old-versus-new frames on the baseline set, targeted scripts and one random walk; about a dozen mutations aimed at new code; switch combinations only for switches the step touched |
| QA re-review | The fix and the lines it touched, not a full re-run |
| Milestones only | Full `tools/ci.sh` matrix: at the GD-88 freeze and before an upstream pull request |

Only defects and real latent risks block approval. Test gaps and nits are logged and carried into the next step.

## Order of work

1. Discussion post (draft ready).
2. Framework: extract the reference theme's drawing behind a theme descriptor with no visual change (gallery
   identical), then move the profile look onto it as the second theme.
   - Step 1 (descriptor, list frame, banner box, stack-check-safe dispatch, `uiThemeTest`): QA-approved at
     `2e46cb85`. Cost against `dc9e29d6`: GD-88 +436 B, DM-1701 +112 B, MD-9600 -100 B flash; RAM unchanged.
   - Step 2 starts with two `uiThemeTest` cases QA listed (not blocking step 1): a theme with `top` NULL measures
     through the reference strip (`theme_top`'s fallback passes `draw`), and a list's strip shows the view's state
     while receiving (`reference_list_head` passes `v->state`). Then QA's step-2 notes: the palette in use across
     `ui_theme_use`, and the profile palettes bypassing the theme's palettes and `colors` op.
   - The first firmware build with a second theme is checked with `STACK_CHECK_ARGS=-v` for unresolved theme calls.
   - Decided for step 2: the second theme is named **panels** (`core/ui/themes/panels/`, `CONFIG_UI_THEME_PANELS`).
     The theme id is stored in its **own spare settings byte** (the remaining `reserved11` byte, offset 407; 0 = the
     build's default theme), with `gui_flags` left as it is.
   - Decided for step 2 (Nov 25): the behavior differences that stay until step 4 are **declared theme options**,
     never a test of the theme in shared code. The Display settings rows a theme shows and the talkgroup-name-first
     key are options on the descriptor; "hub tiles show no values, hub titled Menu" moves into the panels list ops.
   - Items 1-4 (rename, move to `core/ui/themes/panels/`, descriptor and pick, `gui_theme` at offset 407) are in
     `6590540a`..`83ec470d`. Theme numbers: 0 the build's default, 1 reference, 2 panels; a stored theme that is
     missing or doesn't fit falls back to the default, then the reference theme.
   - Step 2 (the panels theme: items 1-7, every op moved, appearance options, `UI_THEME_PANELS` only where the
     drawable area is color, >= 310 px wide and 240 px high): QA-approved at `70a91cc3`
     ([review](reports/theme-step2-review.md)). Cost against `3f7ca184`: GD-88 +1912 B flash, +8 B RAM; the switch
     off costs nothing; UI stack unchanged (GD-88 1664 deepest, 204 free).
   - Step 3 starts with what step 2's QA logged (not blocking): a few panels frames or checksums in
     `uiThemeTestPanels`, so the host suite holds the drawing-order rule without the external pixel baseline (finding
     1); restore or pass down `dual_style` instead of a file-static pointer left set (3); a warning at the `UI_THEME_*`
     enum that those palette numbers are the reference theme's only (4). For step 4: no Settings row sets the theme
     yet, so a radio with theme 1 stored can't get back to panels (5).
3. Layout fixes in the second theme.
4. Menu tree, settings row behavior, naming. First: fix the UI simulator's division by zero on an empty zone
   (`tests/host/sim/ui_sim.c` near the zone stub's `zone_size`; found by QA in step 1, pre-existing, test code only),
   with a test that reproduces it, since step 4 exercises zone and channel editing. (shared; updates task flows, press budgets, IA and translations).
5. Icons and palettes.
6. Freeze on the GD-88, then port to other display classes.

## Planned, not now

- A user-chosen favorite in main Menu slot 9.
- A PIN for Programming and settings.
- Menu scope (restricted menus) only if a purely commercial radio is supported.
