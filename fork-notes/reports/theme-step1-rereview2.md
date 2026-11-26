<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->
<!-- Saved from the cloud session's scratchpad; paths under /tmp/claude-0 no longer exist. -->

# Re-review 2: theme step 1 after 2e46cb85

Commit `2e46cb85` "UI: a theme's strip is measured where it is drawn", on top of `3b2fd11c`. Line numbers are for
the tree at 2e46cb85 (HEAD `dcc69de4` adds fork notes only).

## Verdict: APPROVED

Open item 1 is fixed, both optional items are done, and I found no defect in the commit: no pixel change, no
warning on either compiler, no stack or build regression. Step 1 has no open item.

Two small test gaps remain. They are not defects and do not block; I list them so they are not lost.

## Open item 1: fixed

| Check | Result |
|---|---|
| My failing scenario from the last review (a theme with `top` only returning 60, a four-line message, two rows) | `ui_list_body_fits` now answers false; the frame draws three lines from 60, as before. Measuring and drawing agree |
| The same scenario with the old measuring branch put back in `reference_list_head` | `ui_list_body_fits` answers true again, and `uiThemeTest` fails with 10 FAIL lines |
| The reference strip keeps its own contract in the profile look | By reading: `draw_profile_top` returns what `reference_top_y` returns whenever the profile look can be on (it needs a 240-row display, above `UI_TALL_PX`) |

The design is the first of the two fixes I suggested: the `top` op takes `draw`, and `reference_list_head` goes
through `theme_top` both ways (`core/ui/ui_screens.c:4588`).

## Optional items: done

- **Soft key.** `test_bottom_only` checks the label reaches a theme's `bottom`, not while locked, not with a toast.
  My mutation from last time now fails `uiThemeTest`.
- **At least one palette.** Stated in `ui_theme.h`; `ui_theme_use` returns false and keeps the theme in use. Three
  mutations of that check (accepted anyway, stored before refusing, wrong return value) are all caught.

## Adversarial review of 2e46cb85

**Stack: measuring callers do not pull the drawing frame.**

| Build | What the image shows |
|---|---|
| gd88 (C-SKY, no LTO) | `reference_top` is 64 bytes with a 16-byte frame; it tests `draw` first and returns the measured y without entering `reference_top.part.0` (136-byte frame, 2090 bytes of code). Checked in the disassembly |
| dm1701 (Arm, LTO) | `msg_enter` has no call edge to the strip at all: the measuring path is folded into it |

UI task, deepest / free: gd88 1664 / 204, dm1701 1640 / 88, md9600 1648 / 80, mduv380 + profiles with LTO 1640 / 88,
all unchanged. mduv380 + profiles without LTO is 1640 / 88, 8 bytes deeper than before this commit.

**Run-time selection proof, repeated with my own scratch theme (`UI_THEME_SELECT=1`, selected from a setting).**

| Board | Unresolved pointer calls | Theme calls among them | Oversized op reported |
|---|---|---|---|
| gd88 | 53, as before | none | all 12 ops, each through `x_<op> (pointer)` |
| dm1701 | 71, as before | none | 6 of 6 tried (`top`, `layout`, `colors`, `cell`, `list_head`, `bottom`) |

With `top` oversized, the reported chain is the measuring one on both:
`msg_enter > ui_list_body_fits > list_top > reference_list_head (pointer) > theme_top > x_top (pointer)`.
One-theme builds still have no descriptor, ops table or selection pointer (gd88, dm1701).

**Flash, bytes, my builds.**

| Build | `dc9e29d6` | `3b2fd11c` | `2e46cb85` | Total |
|---|---|---|---|---|
| gd88 | 787968 | 788368 | 788404 | +436 |
| gd88 `CONFIG_UI_PROFILES=n` | 770472 | 770888 | 770932 | +460 |
| dm1701 | 706200 | 706360 | 706312 | +112 |
| md9600 | 685440 | 685604 | 685340 | -100 |
| mduv380 + profiles, LTO | 725824 | 725936 | 725872 | +48 |
| mduv380 + profiles, no LTO | 693216 | 693528 | 693544 | +328 |

These match the implementer's numbers within 16 bytes. RAM is unchanged except md9600, 8 bytes less.

**Mutations.** 50 one-line mutations of `ui_screens.c` against `uiThemeTest`: the 35 from last time and 15 new ones
for this commit. 48 are caught. The two that pass are the gaps below.

## Not blocking: two test gaps

Both are on lines this commit added. The code is correct today; no test in `run-ui` would notice if either line
were changed later.

| Where | Mutation that passes | What it would do | A test that closes it |
|---|---|---|---|
| `core/ui/ui_screens.c:1290`, the fallback in `theme_top` | `reference_top(..., true)` instead of `draw` | A theme that leaves `top` NULL would draw, with no view, when a message is measured: a crash on opening a message. Dead code in one-theme builds | A theme with `top` NULL (the bottom-only one will do): `ui_list_body_fits` gives the reference theme's answer |
| `core/ui/ui_screens.c:4588`, `reference_list_head` | `RS_IDLE` instead of `draw ? v->state : RS_IDLE` | Every list screen's strip would show IDLE during a call. Not caught by `uiThemeTest`, `uiFlowTest`, `gfxTest` or `uiPrototypeTest` | The top-only theme records `st`: draw a list with the view receiving and check it |

I suggest adding them with the first test change of step 2. They need no re-review of step 1.

## What I ran

| Check | Result |
|---|---|
| `uiSim --gallery`, `dc9e29d6` against `2e46cb85`, 7 classes, 3 highlight colors | 1264 PNGs, 0 differ |
| Step-by-step frame comparison, same driver and scripts, one new seed | 0 differences in about 89,000 frames |
| ... gallery and audit scripts at `320x240iR10`, 6 variants | 1440 runs, 9324 steps |
| ... 58 targeted scripts, 10 geometries; again in Spanish, Italian, French | 8690 + 3 x 3476 steps |
| ... random walks | 600 runs at `320x240iR10`, 840 over the 7 other classes |
| Firmware, default build and stack check: gd88, gd88 `CONFIG_UI_PROFILES=n`, gd88b, db25d, dm1801pro, dm32, rt10, dm1701, md380, md9600, mduv380 | 0 warnings, every task fits |
| mduv380 + profiles, LTO and no LTO | 0 warnings; the UI task fits. The no-LTO run still fails on `prvIdleTask` only, as before the theme work |
| `ui_screens.c`, `ui_app.c`, `ui_menus.c` over 21 switch combinations on gd88 (C-SKY) and mduv380 (Arm) | 42 builds, 0 warnings, 0 errors |
| `uiThemeTest` built with `CONFIG_UI_PROFILES=1`, `CONFIG_DUAL_WATCH=0`, `CONFIG_KEY_ASSIGN=0`, and all three together | passes |
| `make -C tests/host -k` | only the known `run-settings` failure (field 74) |
| `make -C tests/host SANITIZE=1 run-ui` | passes, no sanitizer report |
| `check-run`, `check_doc_links.py`, `check_generic.py`, `license_grep.sh`, `i18n.py --check`, `stack_check.py --self-test`, `git diff --check` | clean |

The driver now waits for a crashed simulator before reading its exit status, so the false difference of last time
(both simulators crashing in the simulator's own stub) no longer appears.

Not run: `tools/ci.sh`, `reuse lint` (not installed), HIL. Left out as instructed: my step-2 notes and the
simulator's `zone_size` crash.

Scratch worktrees and `build/rv-*` trees are removed; the main checkout has no changes. Scripts and logs are in
`/tmp/claude-0/-home-user-RadioFoundryDG/5cf815ea-6a4b-5648-86c7-8189112daa08/scratchpad/review/` (`walk.py`,
`targeted.txt`, `mutate.py`, `ui_theme_x2.c.orig`, logs in `round3/`).
