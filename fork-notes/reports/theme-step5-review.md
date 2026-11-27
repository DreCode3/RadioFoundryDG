<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->
# QA review: theme step 5 (icons and palettes, panels theme)

Commits `d51930b7..934d80c5` on `feat/gd88-gui` (6 commits). Checked against `fork-notes/gui-plan.md` step 5,
"Decided Nov 26 (user)", and the older rules (step 4 Layout, marks inside the strip pane, reference theme unchanged,
panels=n costs nothing, no RAM on the GD-88). One review, sized to the risk. I didn't read the implementer's report.

## Verdict

No blocking defects. The code matches the brief, and every build and suite I ran passes. The gaps I found are test
gaps for behavior the renders show working, plus one stale comment. Per the Process rules, none of them blocks.

## Built against the brief

| Brief | Found | How checked |
|---|---|---|
| One 24 px family, 20 px live, theme ink, accent on focus, no tints | Yes. One const table, one blitter (`panels_icon`). Tiles use `ui_col.fg` or `panels_accent`. Strip marks use `ui_col.fg`. The old tint table and the gear's fixed black/gray are gone | Code; table rendered to a sheet; uiSim Day/Night, both layouts |
| Drawings as in `icons_revised.png` / `audio_headphones.png` | Yes, all 19 menu pictures plus the monitor and roaming. Audio is headphones, Scan the zigzag (also on the Menu tile), Zones a folder, Receive a dial with its ticks outside, the roaming ring with a center dot | Decoded `panels_icons.h` to a 6x sheet, compared by eye |
| Menu tiles 39 px, 2 px apart | Yes: rows 60-98, 101-139, 142-180 | Measured pixels in a render |
| Privacy key in ink, red slash when it can't decrypt | Yes. Under the slash the key is drawn in `dim` (palette), not `fg` | Renders; code |
| Soft key bar 32 px with a 2 px gap; Chinese labels aligned | Yes: bar at y 204-235. Chinese bar labels and the locked sentence are in 8x16, centered on the bar. Before this change they sat at the bar's bottom edge | Pixels; a zh uiSim I built (`UI_LANG=zh`), before/after |
| Strip words 12x24 where the strip fits, else 8x16; nothing smaller, VOX/RX/A-B included | Yes. VOX, RX, IP, TX and the mode are text in the chosen font. A/B are 8x16 in rounded boxes, the selected one filled. Algorithm labels are 8x16 | Renders (calm and crowded, Day/Night); uiThemeTest's new min-font case |
| Palette fixes | Day strip ink = `fg` (Day and Night). Day `warn`/`strip_warn` = RGB(122,86,0). The key's grays are now `dim`. The night gear is drawn in `fg` and shows | Diff; renders |
| Scan: a moving gap | Yes: 13 steps of 100 ms, the gap moves along the stroke, then the arrowhead | 13 frames rendered |
| Roaming frames inside the strip pane | Yes, all 8 steps, Day and Night | 8 frames rendered; new test |
| Marks inside the pane with 2 px clear | Yes. Old and new pane tests pass | Tests; M5 mutation |
| Reference theme unchanged | 1363 compared, 0 differ (profile left out) | `cc.sh` |
| panels=n doesn't grow; no RAM added | panels=n: 774200 B before and after, 0 B. panels=y: 793452 to 791620 B, -1832 B. RAM 205872 / 205784 B, unchanged | GD-88 builds at the same path length (see note) |

Size note: on this toolchain, flash depends on the length of the source and build paths (assert `__FILE__` strings,
about 10 B per character). A first panels=n head build in `RadioFoundryDG/build/t-qa10-np` read +20 B against a base
built in a worktree. Rebuilt in a worktree with the same path length and the same `BUILD=build/t-bn`, the panels=n
build is byte-identical in size. Future size comparisons need the same path lengths.

## Icon table and blitter

- **Bounds and clipping:** every entry fits its 24 px canvas (checked with a decoder). Only TONES reaches row 23
  (live area 2..22: the bell's clapper is 1 px past it). That's cosmetic and inside the canvas. Pixels go through
  `gfx_coverage_pixel`, which clips to the clip rectangle. Offsets (`at`) and strides match the data. The table is
  1842 B and fits `uint16_t at`. `panels_scan_steps` is 19 x 10 nibble pairs, matching the 20 x 19 scan mask.
  Roaming's halo plus the farthest dot offset (±7) stays inside 0..23.
- **16-color palette:** icons use `fg` and the accent, both reserved in `panels_list_colors` before the cells are
  drawn. Strip marks use `fg`, `dim`, `tx` and `strip_bg`. All 38 `after/` renders use 11 to 15 colors. The tile test
  (blends only between the panel and `fg`/accent) passes Day and Night on the indexed `iR10` simulator.
- **Const, not RAM:** `static const`, included only by `panels.c`. GD-88 RAM is unchanged.
- **Generator:** `python3 tools/profile_icons.py OUT` with Pillow 13.0.0 (also `uv run`) reproduces the committed
  `panels_icons.h` byte for byte.

## Mutations (14, plus probes)

| # | Mutation | Caught by |
|---|---|---|
| M1 | Menu tiles 4 px apart | test_tile_icons_stay_inside_their_tiles_in_the_theme_ink |
| M2 | Tile icon tinted (`rx`) | test_tile_icons…, test_settings_grid_icons_digits_and_pages |
| M2b | Single-column list row icon tinted | **none** (path not reached on 310 x 240; see log) |
| M3 | Strip key green again | 3 tests incl. test_strip_privacy_key_in_the_theme_ink |
| M4 | No red slash | test_crossed_key_is_only_a_receive_warning, test_strip_privacy_key… |
| M5 | Roaming 2 px up | test_strip_marks_stay_inside_the_pane_while_they_move |
| M6 | Scan without its gap | **none** |
| M7 | Bar 34 px | 5 tests |
| M8 | No gap above the bar | test_tile_icons… |
| M9 | Chinese not detected (bar labels) | **none** (no test runs a zh build of the panels bar) |
| M10 | Words always 8x16 | test_menu_status_uses_larger_mode_and_device_marks, test_dstar_preview… |
| M10b | Words always 12x24 (never shrink in a crowd) | **none** |
| M11 | Dual A/B in 6x12 | uiThemeTestPanels min-font case (2 failures) |
| M12 | Day ink black again | 3 tests |
| M13 | 934d80c5 reverted (12x24 word cell unclamped) | **none** in the suites. `pttdown expect:layout=0` fails with it ("DMR" and "TX" overlap) |
| M14 | Strip icons ignore their canvas offset `dx` | **none** |

The new tests in this step (pane while moving, tile icons, privacy key, min-font strip words) each fail on the
mutation of their change.

## Expectations changed in older tests

- Black-ink checks became `DAY_INK` (palette `fg`): on purpose (Day strip ink = fg).
- Tinted-icon checks became ink checks; the gear check became "accent, no black": on purpose.
- Receive-only "R stem / blank column" became "RX is shown": RX is now text, so the bitmap shape no longer exists.
  On purpose.
- Monitor offsets +10/+12/+14 became +8/+10/+12: they follow the new 16 px speaker. M12 still fails this test.
- `test_home_clock_moves_to_the_mode_bar_in_a_crowd`: `assertTrue(clock_in_mode_bar(crowd))` became `assertFalse`.
  The narrower marks really do let Home's strip hold the clock beside that crowd (rendered), so this hides no defect.
  But the suite no longer has a case where the clock moves into the mode bar. The path still works: with
  `rxonly:on pttdown` (NO TX) or `aprstx` added to the crowd, the clock is drawn at the mode bar's right end
  (rendered). Logged as a test gap below.

## Logged, not blocking

1. **Test gap: mode-bar clock.** No test now covers the clock moving into Home's mode bar. Smallest fix: in
   `test_home_clock_moves_to_the_mode_bar_in_a_crowd`, add `assertTrue(clock_in_mode_bar(crowd + "rxonly:on pttdown "))`
   (the plain frame needs the same suffix).
2. **Test gap: 934d80c5 has no test.** Reverting it passes every suite. Smallest fix: run
   `pttdown expect:layout=0` in both layouts in uiPrototypeTest (both fail with the clamp removed).
3. **Test gaps (M6, M9, M10b, M14):**
   - M6: the scan gap's movement;
   - M9: Chinese bar labels at 8x16 (only a `UI_LANG=zh` simulator shows them);
   - M10b: words shrinking to 8x16 before items drop;
   - M14: the strip icons' canvas offsets.

   The renders show all four working.
4. **M2b:** the single-column icon-row branch of `panels_cell` (`g->cols == 1` with icons) isn't reached on the
   GD-88 screens I tried (Settings and Menu are grids; Large text doesn't change them). It's harmless, but it's
   untested and, on this display, possibly dead.
5. **Stale comment:** `panels.c:383` still says "green while DMR privacy is configured … a red X".
6. **Scan step 12 leaves two faint anti-aliased specks** where the arrowhead was: edge pixels assigned to a stroke
   step. Visible only at zoom.
7. **TONES (bell)** reaches canvas row 23, 1 px past the 20 px live area. Cosmetic.
8. **pppTest.c:363 `-Wsign-compare`** in the host suite: older than this step and untouched by it.

## For the user to settle (visible, not defects)

- **The warning key is drawn in `dim` under the red slash, not in the full ink.** The brief says "theme ink, with a
  red slash". The commit dims the key so the slash reads. `dim` is a palette entry, so it agrees with "the key's grays
  come from the palette". Spec and user docs say "dimmed key with a red slash".
- **Scan's last step blanks the whole arrowhead** (a "Z" frame), as in the approved "gap" option row of
  `icons_revised.png`. The plan's wording "then the arrowhead lit" could be read as the opposite.
- **Chinese locked sentence** is left-aligned in 8x16 on the bar (the English one fills the bar). This placement is
  older than step 5; only the font and centering changed.

## Checks run

- `fork-notes/tools/cc.sh --except profile ../rf-base-s5 ../rf-qa10-work` (log `cc.log`): CC OK.
  - uiSim: 0 warnings.
  - Gallery and 940 profile renders: no failures. Layout rc 0.
  - 1363 compared, 0 differ.
  - uiThemeTest and uiThemeTestPanels: passed.
  - uiPrototypeTest: OK.
  - GD-88: 0 warnings. UI stack 1664 deepest, 204 free (unchanged).
- Full host suite in Docker (`make -C tests/host clean` before and after): rc 0 (`host.log`).
- GD-88 builds, all with 0 warnings:
  - default `make BOARD=gd88` (`gd88.log`): 791640 B / 205872 B;
  - panels=n in `build/t-qa10-np`: 774220 B, then removed;
  - base and head at the same path lengths, panels on and off (`base-gd88*.log`, `head-gd88*-samepath.log`);
    worktrees removed.
- `tools/profile_icons.py` regeneration: identical.
- 14 mutations in a `tools/worktree.sh` worktree against uiPrototypeTest, `uiSim --layout` and, for the survivors,
  uiThemeTest/uiThemeTestPanels in Docker (`mutations.log`). Worktree removed.
- Renders, Day and Night, Commercial and Amateur:
  - crowded strips; TX, NO TX and TX APRS crowds;
  - the dual watch strip;
  - 13 scan frames and 8 roaming frames;
  - Menu and Settings tiles;
  - a zh simulator (Home, Settings, list, locked), before and after.

## Checks not run

- `tools/ci.sh` matrix, DM-1701, MD-9600 and other boards: shared code outside `core/ui` isn't touched.
- Stack check beyond the one in `cc.sh`, `reuse lint`, HIL, on air. I didn't touch a radio.
- Translation audits beyond the zh bar. The es/fr/it layout pass wasn't run.
- A random walk and the old-versus-new frame comparison of the whole profile set: I reviewed `before.png`/`after.png`
  and my own targeted renders instead.

Note: the main checkout shows `M fork-notes/gui-plan.md`, uncommitted, not made by this review. `build/gd88` from the
requested default GD-88 build is left in place.

**APPROVED**
