<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# Review: theme step 2, the panels theme (d9dfd89b, 6590540a..70a91cc3)

Range reviewed: `git diff 3f7ca184..70a91cc3` (3f7ca184 is the upstream merge the step starts from; the docs commits
006528e5 and f6198ac6 skipped) plus d9dfd89b. Line numbers are for the tree at 70a91cc3. Independent QA: I did not
write this code, and I took no claim from the commit messages or the hand-off as proof.

## Verdict

**APPROVED.** I found no defect and no blocking latent risk. Every frame I could render is the same before and after
(2208 baseline frames, 950 random walks on four geometries, 221 targeted scripts), every switch combination I built
has zero warnings, the switch costs nothing when off (12 bytes less on the GD-88 and the DM-1701), and the stack check
resolves every theme call with the second theme built. What follows are test gaps and nits, none blocking.

## Findings

| # | Severity | Where | What |
|---|---|---|---|
| 1 | Not blocking (test gap) | `core/ui/themes/panels/panels.c:1437`, `:1528` | Drawing order and fit of the copied code are guarded only by the pixel baseline, which is outside the host suite; one mutation passes everything |
| 2 | Not blocking (test gap) | `core/ui/ui_app.c:2565` | The GPS clock tick's option guard is untested (benign) |
| 3 | Not blocking (latent nit) | `core/ui/ui_screens.c:2768-2779`, `:3209` | `dual_style` is a file-static pointer set by `ui_home_dual_body` and never restored |
| 4 | Not blocking (latent nit) | `core/ui/ui_screens.h:210-214`, `core/ui/ui_gps.c:43` | Palette numbers now overlap between themes (panels Night is 1, the reference theme's Light) |
| 5 | Info | `core/ui/ui_svc_storage.c:521`, `ui_app.c:1996` | A radio with theme 1 stored has no way back to panels on the radio |
| 6 | Info (not from this step) | `docs/feature-matrix.md` | `tools/boards.py docs --check` fails, on HIL status cells of unrelated features |

### 1. The copies' drawing order rests on the pixel baseline only (test gap)

The host suite (`make -C tests/host`) never draws the panels look and compares pixels: `uiFlowTest` is built without
`CONFIG_UI_THEME_PANELS`, `uiThemeTestPanels` checks the pick and the reference frames, and `uiPrototypeTest.py`
checks texts, focus and screens. The pixel baseline (`fork-notes/tools/snapshot.py` + `compare.py`) is the per-commit
check, but it is not in the suite or in `tools/ci.sh`.

- M10, `panels_scroll` drawing the grid's page badge after the cells instead of before (`panels.c:1437`, `over` test
  inverted): every host test and `uiPrototypeTest` pass; only the baseline sees it (8 frames differ: hub and
  settings frames of the profile variants). This is exactly the
  16-color palette-order rule the hand-off warns about.
- M17, `panels_bottom`'s room for the hint groups 6 px wider (`panels.c:1528`, `w - 12` to `w - 6`): no test and not
  the baseline either (2208/0/0). No English hint lands in those 6 px; a longer translation would.

Suggested, for a later step: a `uiThemeTestPanels` frame or a checksum over a few panels frames (a paginated grid,
a list with a two-line toast, the bottom row at its font fallback), so the suite carries the order rule without the
external baseline.

### 2. GPS clock tick under every theme (test gap, benign)

M16 removes `if (!ui_theme_has(UI_THEME_OPT_GPS_CLOCK)) return;` from `gps_clock_tick` (`ui_app.c:2565`). Nothing
catches it, the baseline included. The effect would be redraws on GPS fix and time changes while the reference theme
is in use in a panels build: no wrong pixels, a little more drawing. Logged, not worth a test on its own.

### 3. `dual_style` outlives the call that sets it (latent nit)

`ui_screens.c:2771-2775` keeps the dual watch style in a file-static pointer, `ui_home_dual_body` sets it
(`:3209`) and nothing restores it. Today every reader (`dual_pane`, `dual_large`, `dual_activity` through
`DUAL_INSET()`, `DUAL_TARGET_LABEL()`, `DUAL_CALLS()`, `dual_style->deny_margin`) runs only beneath
`ui_home_dual_body`, so the value is always the current frame's; M12 (the assignment removed) is caught by
`uiPrototypeTest`. A future caller of those helpers outside it would draw in whichever theme's style ran last.
Suggested: pass the style down, or restore `&dual_reference` on return. Not blocking.

### 4. Palette numbers overlap between themes (latent nit)

`ui_theme_get()` returns the palette number of the theme in use. Before the step the profile palettes were 4 and 5,
outside the reference theme's `UI_THEME_*` (0-3); now panels Day and Night are 0 and 1. The only shared reader,
`ui_gps.c:43` (`ui_theme_get() == UI_THEME_NIGHT`, 3), behaves as before under panels (never true, as with 4/5), and
`ui_menus.c:7662` reads the stored setting, not the palette. A future shared test such as
`ui_theme_get() == UI_THEME_LIGHT` would however be true under panels Night. Suggest a one-line warning at the
`UI_THEME_*` enum (`ui_screens.h:210`) that these numbers mean the reference theme's palettes only, or a theme option
for "GNSS colors" when step 5 revisits palettes.

### 5. No way back from a stored reference theme (info)

`gui_theme` is written only by the simulator; there is no Settings row for it yet. A GD-88 whose byte 407 reads 1
(from a settings tool) starts in the reference theme and Settings > Display shows the reference theme's rows, so the
user can't return to panels on the radio. The spec says the byte is read only at start and the picker is not in this
step, so this is expected; noting it so step 4 picks it up.

### 6. `tools/boards.py docs --check` (info, not this step)

In rf-qa at 70a91cc3 the check reports `docs/feature-matrix.md` out of date. Regenerating changes 10 lines, all HIL
status cells ("needs re-run" or not) of unrelated features (`dmr_emergency`, `gps`, `task_stacks`, `watchdog`, ...),
none of the `ui_theme_panels` rows the step edited. Likely a git-history-dependent cell; I did not check whether
3f7ca184 fails the same way. File restored after the check.

## Checked and found correct

- **Copied bottom and list cell.** `panels_bottom` against the old `reference_bottom` profile branches: the same
  sequence (toast with two-line wrap and `ui_overlay_top`, pane, compact menu hint, secondary split, font fallback to
  12x24 then small, the 15 px / 6 px margin rule); the dropped `gfx_bpp() == 1` branches are unreachable since
  `fits()` needs bpp > 1. `panels_cell`, `panels_list_geom`, `panels_list_head` and `panels_scroll` against the old
  `roomy` branches: same. The hub's empty values and missing warn flag moved from `hub_item` to the cell (`value = ""`,
  `UI_ITEM_WARN` masked); `hub_item` sets no other flag, so nothing else changed. "Menu" moved into
  `panels_list_title`.
- **Lists that never set `roomy`.** The old Quick list (`draw_quick_list`) had `roomy` false; under panels Quick is
  drawn as tiles (width >= 150), so the list frame never meets it. `msg_enter`'s `ui_list_body_fits` now measures
  through the theme head, as before.
- **Trunked and SFR Home.** `panels_home` checks `panels_operational_home` before anything else and calls
  `ui_reference_home`, which takes the DMR call screen path, as the old order did; the strip, footer and regions are the
  theme's (`panels_layout` redoes the regions with `PANELS_BAND_H` and the footer top). Baseline variants and my
  targeted t3, t3 ring, t3 PTT deny, SFR, SFR receiving and SFR large text scripts in all four variants: identical.
- **Dual watch Home.** `ui_dual_style_t` reproduces inset 12, the target label for Commercial only, no call list and
  the centered deny text (`dual_deny` with margin 15 equals the old `profile_center`'s `w - 30`). Targeted: TX denied
  (rx-only channel), timeout, banner, toast, large text receiving, heard calls, in all four variants and on five other
  geometries: identical.
- **Theme pick and fallback.** `ui_theme_pick`: 0 maps to the build's default; an id the build lacks or that doesn't
  fit falls to the default, then the reference theme. `ui_app_init` maps a value outside 0-255 to the default, and
  the setter refuses one. Stored 0, 1, 2, 200 and out-of-range covered by `uiThemeTestPanels` (`test_pick`, every
  bound of `fits()`), `radioCoreTest` (get, set -1/256/255/1/0) and `uiPrototypeTest` (reference theme on `320x240`).
  Only one caller of `ui_app_init` (`ui.c:157`), after the splash and `ui_layout_init`; with no frame buffer
  `fits()` fails and the reference theme stays.
- **Settings.** `gui_theme` at 407 in the old `reserved11`, `sizeof(settings_t)` still 412, `settingsTest` and
  `settings-store.md` updated together; 0 reads as the build's default, which is panels on the GD-88 where it fits,
  as before. No other user of the byte or of the removed `UI_FEAT_GUI_PROFILES` bit.
- **No theme identity in shared code.** No test of `ui_theme_panels`, a theme id or a palette id outside
  `ui_screens.c`'s registry. The remaining differences are options on the descriptor (`UI_THEME_OPT_*`), and the
  `CONFIG_UI_THEME_PANELS` blocks left in shared code are build guards for fields and tables that exist only with the
  switch. `set_hidden`'s option mapping reproduces the old row rules, GPS clock still hidden without a GPS.
- **`#if` guards.** Declarations in `ui_theme.h` under the switch, definitions `UI_THEME_SHARED` (static when off);
  `ui_secondary_hint_split` under `PANELS || KEY_ASSIGN`; dual watch pieces under `DUAL_WATCH`; the Makefile adds
  `core/ui/themes/panels` only for `y` with `core/ui` built, matching `feature_on`. Builds below.
- **Stack tool.** `o->animate(...)` in `ui_app_tick` goes through a typed local. With the second theme built,
  `stack-check -v` on DM-1701 lists the same 71 unresolved pointer calls as without it (none a theme op), and the
  deepest UI path runs through `panels_list_head` resolved. GD-88 UI stack 1664 / 204 free with and without the
  switch, the same as 3f7ca184.

## What I ran

### Frames, old (3f7ca184) against new (70a91cc3)

| Check | Result |
|---|---|
| uiSim at 70a91cc3 | builds, 0 warnings |
| `snapshot.py` + `compare.py` against `rf-baseline` | 2208 compared, 0 differ, 0 missing; layout rc 0 |
| Random walks (`rf-qa-out/scripts/walk.py`, keys plus 25% radio events, 3-30 steps from 8 start states), uiSim built at each commit | `320x240iR10` 400, `320x240` 250, `160x128` 150, `240x320i10x5` 150: 0 differ, 0 errors |
| Targeted scripts (`rf-qa-out/scripts/targeted.txt`): dual watch deny / timeout / banner / toast / large text / heard, list and hub toasts, Trunked and SFR Home, the talk label key, `gui:original` (reference theme in a panels build, its Display rows, hub, round trip), the scan, roaming and recording marks at 6 timings each, the GPS clock, the Display rows; four variants on `320x240iR10`, nine scripts on five other geometries | 221 compared, 0 differ, 0 errors |

### Host suite

Linux image (`local/rf-host-tests`), full `make -k -C tests/host -j16` on a copy of 70a91cc3: rc 0, all passed,
`uiThemeTest` and `uiThemeTestPanels` included. The one compiler warning (`pppTest.c:363`) is in a file the step
didn't touch.

### Mutations

17 mutations in a scratch copy, each run against `uiThemeTest`, `uiThemeTestPanels`, `radioCoreTest`,
`settingsTest`, `uiFlowTest`, `uiFlowTestNoColor`, `gfxTest` (Linux image) and `uiPrototypeTest.py` (native uiSim);
the pixel baseline only where those missed it. All reverted.

| # | Mutation | Caught by |
|---|---|---|
| M01 | pick: fallback to the reference theme instead of the build default | uiThemeTestPanels |
| M02 | `fits()`: width `>= 310` to `> 310` | uiThemeTestPanels, uiPrototypeTest |
| M03 | `ui_app_init` ignores the stored id | uiPrototypeTest |
| M04 | Filled status bar not an appearance option | uiPrototypeTest |
| M05 | label-first option test inverted in `set_key_next` | uiPrototypeTest |
| M06 | Settings drawn as a plain list (list kind) | uiPrototypeTest |
| M07 | stored byte always read 0 | radioCoreTest |
| M08 | stored 255 refused | radioCoreTest |
| M09 | Settings' icon colors not reserved first (palette order) | uiPrototypeTest |
| M10 | grid page badge drawn after the cells (drawing order) | pixel baseline only (8 frames); finding 1 |
| M11 | Commercial dual watch without the target label | uiPrototypeTest |
| M12 | `dual_style` never set | uiPrototypeTest |
| M13 | scan mark every 200 ms | uiThemeTestPanels |
| M14 | `UI_BAR_LIST` not passed | uiPrototypeTest |
| M15 | palette bound off by one | uiThemeTest, uiThemeTestPanels, gfxTest |
| M16 | GPS clock tick under every theme | nothing (baseline 0 differ); finding 2 |
| M17 | bottom row's room 6 px wider | nothing (baseline 0 differ); finding 1 |

### Firmware, switch combinations

Built in rf-qa at 70a91cc3 with `BUILD=build/t-<name>`, trees removed afterwards. All rc 0, 0 warnings.

| Build | FLASH | RAM | Stack check |
|---|---|---|---|
| dm1701 | 706300 | 127512 | ui 1640 deepest, 88 free; every task fits |
| dm1701 `CONFIG_UI_THEME_PANELS=y` (never fits; both themes built) | 725316 | 127552 | ui 1660, 68 free; `-v`: same unresolved list as dm1701 |
| dm1701 panels, `CONFIG_DUAL_WATCH=n` | 704172 | 122600 | not run |
| dm1701 `CONFIG_DUAL_WATCH=n` | 685728 | 122544 | not run |
| dm1701 panels, `CONFIG_GPS=n` | 715836 | 126904 | not run |
| gd88 | 790324 | 205808 | ui 1664, 204 free; every task fits (`-v`) |
| gd88 `CONFIG_UI_THEME_PANELS=n` | 770932 | 205720 | ui 1664, 204 free |
| gd88 `CONFIG_DUAL_WATCH=n` | 766988 | 200664 | not run |
| gd88 `CONFIG_GPS=n` | 779260 | 205176 | not run |
| gd88 `CONFIG_KEY_ASSIGN=n` | 786408 | 205384 | not run |
| gd88 `CONFIG_UI_COLOR=n` (panels follows it off) | 769816 | 205720 | not run |
| gd88 `CONFIG_DMR=n` | 405384 | 130560 | not run |

Cost, both trees plain `git archive` copies so the version strings match:

| Build | 3f7ca184 | 70a91cc3 | Delta |
|---|---|---|---|
| gd88, panels on | 788244 / 205800 | 790156 / 205808 | +1912 B flash, +8 B RAM (as the commit messages add up) |
| gd88, panels off | 770780 / 205720 | 770768 / 205720 | -12 B |
| dm1701 default (off) | sections | sections | -12 B (`.rodata`, the off-switch list); every symbol change is a rename |

GD-88 UI stack 1664 deepest / 204 free in all four.

### Not run

- MD-9600 and the other boards: the step changed shared code only under `core/ui` and the position and settings
  headers, and the end-of-step builds are the implementer's.
- `tools/ci.sh`; `uiSim --layout` beyond the snapshot's own run (rc 0).
- No radio, debug link or service touched. The worktree is clean at 70a91cc3 (`git status` empty).

Scratch output (scripts, logs, renders): `../rf-qa-out`.
