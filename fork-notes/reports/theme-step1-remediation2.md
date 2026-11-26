<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->
<!-- Saved from the cloud session's scratchpad; paths under /tmp/claude-0 no longer exist. -->

# Theme step 1: second remediation (QA re-review, open item 1)

One new commit on `feat/gd88-gui`, on top of `e5ba383b`: `2e46cb85` "UI: a theme's strip is measured where it is
drawn". Nothing amended. I did not push; the branch is level with `origin` and has a fork-notes commit (`dcc69de4`)
above mine, both from outside this run. Tree clean, `build/t-*` removed, `build/ci` not touched.

## Open item 1: measuring and drawing the strip agree (`2e46cb85`)

**Choice: QA's option 1.** The `top` op gets the `draw` flag `list_head` already has:

```c
int (*top)(const radio_view_t *v, radio_state_t st, const ui_state_t *s, const char *prefix, const char *name,
	bool draw);
```

`draw` false only measures (no view, no state) and must return the y it returns when it draws. The reference list
head now calls the one op both ways (`theme_top(..., draw)`), so a theme with a strip of its own and no `list_head`
measures its own strip. `ui_draw_top` and every other caller are unchanged.

Why not option 2 (a theme with `top` must give `list_head`, enforced in `ui_theme_use`): it is a rule checked at run
time, every such theme would repeat the reference list head, and the same hole would reopen for the next caller that
measures. Option 1 makes disagreement impossible in the shared code and costs nothing (below).

Why a flag and not a second op (`top_y`): two ops a theme must keep in step, and a NULL `top_y` next to a `top` would
be the same defect again.

**Stack and flash.** The compiler splits `reference_top` into the measuring exit (64 bytes of code) and the drawing
part (`reference_top.part`), on both Arm and C-SKY, so a measuring call does not enter the drawing code or its
frame. Clean tree, against the previous commit (`3b2fd11c`):

| Build | FLASH before | FLASH now | UI task deepest / free |
|---|---|---|---|
| gd88 | 788376 | 788400 (+24) | 1664 / 204, unchanged |
| gd88 `CONFIG_UI_PROFILES=n` | 770884 | 770928 (+44) | not checked separately |
| dm1701 | 706360 | 706312 (-48) | 1640 / 88, unchanged |
| md9600 | 685620 | 685340 (-280) | 1648 / 80, unchanged |
| mduv380 + profile look, LTO | 725936 | 725872 (-64) | 1640 / 88, unchanged |
| mduv380 + profile look, no LTO | 693520 | 693536 (+16) | 1640 / 88 (was 1632 / 96) |

RAM: gd88 205800, dm1701 127512, gd88 without profiles 205720, mduv380 124224 and 119232, all unchanged; md9600
121320 (8 less). The md9600 numbers moved across 275 symbols, most of them outside the UI: that is LTO placing
things differently, not this change. The no-LTO mduv380 run still fails only on `prvIdleTask` (over by 24, as before
the theme work).

Totals against `dc9e29d6`, for the PR: gd88 +432, gd88 without the profile look +460, dm1701 +112, md9600 -116,
mduv380 + profile look +48 (LTO) and +320 (no LTO).

**The run-time selection proof still holds.** Scratch build on `2e46cb85` (`-DUI_THEME_SELECT=1`, a second theme
picked from a setting, now with its own `top` as well):

| Board | Unresolved pointer calls | From `core/ui` | Result |
|---|---|---|---|
| dm1701 (Arm, LTO) | 71, as before | only `left`, `ui_scr[...]->leave()` (old) | every task fits |
| mduv380 (Arm, LTO) | 71 | the same | every task fits |
| gd88 (C-SKY, no LTO) | 53, as before | the same | every task fits |

With the scratch `top` given a 1400-byte buffer, the check fails on dm1701 and gd88, and the path it reports is the
measuring one, through both pointers:

```
ui  OVER  deepest 2708 (gd88)
  ... ui_push > msg_enter (pointer) > ui_list_body_fits > list_top > reference_list_head (pointer)
      > theme_top > scratch_top (1428, pointer) > gfx_text > ...
```

That also shows QA's earlier point in practice: the check counts a theme's whole `top` under every measuring caller.
For the reference theme the split keeps that cheap; a second theme's `top` should keep its measuring exit ahead of
its buffers the same way.

One-theme builds are still folded: no `ui_theme_reference`, `reference_ops` or `ui_theme_selected` in gd88, gd88
without profiles, dm1701 or md9600.

**Test, and that it fails on the unfixed code.** `uiThemeTest` has a theme with `top` only (a strip 60 px high, 50
without a title):

- for messages of 1 to 8 lines above two rows, `ui_list_body_fits` answers from the theme's y (the expected answers
  differ from the reference strip's for some lengths, and fall both ways);
- the op is asked once, with the title, with no view or state, and does not draw;
- drawn, there is nothing of the list above the theme's y and the text starts at it;
- a message said to fit has its last line on screen;
- the reference strip measures what it returns when it draws, with and without a title, on this display and on a
  320x240 one that gives the title its own row.

With the measuring branch of `reference_list_head` put back as it was (`reference_top_y(...)`), the test fails with
10 FAIL lines. Related mutations, all caught:

| Mutation | Result |
|---|---|
| Unfixed: the list head measures the reference strip | 10 FAIL |
| The list head measures without the title | 8 FAIL |
| The reference strip draws when asked to measure | ASan SEGV (no view) |
| The reference strip measures as untitled | 3 FAIL (caught only by the 320x240 part) |

All 23 mutations (the 17 from the last round plus these and the two below) are caught; the list is in
`mutations2.txt` in the scratchpad.

## QA's optional items (same commit)

- **Soft key in the unit test.** A theme with `bottom` only: Home hands it the assigned soft key's label
  (`home_soft_key_label`), not while the keys are locked, not with a toast. QA's mutation (the label not passed) now
  fails `uiThemeTest` (1 FAIL).
- **At least one palette.** `ui_theme.h` states it on `palette_count`. `ui_theme_use` returns `bool` and refuses a
  theme without a palette (the one in use stays). Tested; with the check removed the test fails (2 FAIL). A
  `_Static_assert` cannot see another theme's table, so the check is at run time, in code only multi-theme builds
  have.

## Acceptance

| Check | Result |
|---|---|
| Pixel compare against `base` | 2208 compared, 0 differ, 0 missing; layout rc 0 |
| `run-ui` | rc 0, 0 warnings |
| `uiPrototypeTest.py` | OK |
| gd88, dm1701, md9600, gd88 `CONFIG_UI_PROFILES=n` | 0 warnings |
| `stack-check` gd88, dm1701 (and md9600) | every task fits |
| db25d, dm1801pro, dm32, gd88b, md380, mduv380, rt10: default build and `stack-check` | 0 warnings, every task fits |
| Whole host suite | only the known `run-settings` failure (field 74) |

Not run: `tools/ci.sh`, `reuse lint` (not installed), HIL.

## Not done, as instructed

QA's step-2 notes (the palette in use across `ui_theme_use`, the profile palettes bypassing the theme) and the
simulator's `zone_size` 0 crash in `tests/host/sim/ui_sim.c`.

## Disputed

Nothing. One remark for step 2, not a disagreement: `docs/spec/ui.md` describes `uiThemeTest` in one paragraph that I
did not extend for the new cases; it is still accurate.
