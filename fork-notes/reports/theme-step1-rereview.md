<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->
<!-- Saved from the cloud session's scratchpad; paths under /tmp/claude-0 no longer exist. -->

# Re-review: theme step 1 after remediation (4db45f54, 3a70ab87, 3b2fd11c)

Range `8dd8ce37..3b2fd11c`. Line numbers are for the tree at 3b2fd11c (HEAD `e5ba383b` adds fork notes only).

## Verdict: NOT APPROVED, one open item

Seven of the eight findings and every nit are closed, and I found no pixel change, warning, stack or build
regression in the remediation. One part of finding 8 is still open, and I can show it fail.

### Open item 1 (required): a theme with its own `top` and no `list_head` measures one strip and draws another

- **Where:** `core/ui/ui_screens.c:4575` (`reference_list_head`), contract in `core/ui/ui_theme.h:48` and `:58`.
- **What:** when drawing, the reference list head calls `ui_draw_top`, which goes to the theme's `top` op. When
  measuring (`draw` false), it returns `reference_top_y`, the reference strip's height. The remediation gave that
  height one formula, but the measuring branch still does not ask the theme in use.
- **Failure scenario:** a theme gives `top` (a taller strip) and leaves `list_head` NULL, which the header invites
  ("a theme gives only what it draws differently"). `msg_enter` (`core/ui/ui_menus.c:962`) calls
  `ui_list_body_fits`, which measures from the reference y and says the message fits, so no "Full message" row is
  added. `ui_draw_list` then starts the body at the theme's y and shortens the text. The user cannot read the whole
  message.
- **Confidence:** confirmed by experiment. Scratch theme with `top` returning 60 on the test's 160x120 display, a
  four-line body, two rows: `ui_list_body_fits` returned true, the body started at 60 and three lines were drawn.
  No build has such a theme today, so nothing on a radio is wrong yet.
- **Suggested fix (either):**
  1. Let the strip be measured through the theme: a way to ask the `top` op for its y without drawing, used by
     `reference_list_head` when `draw` is false.
  2. State in `ui_theme.h` that a theme with its own `top` must give `list_head`, and refuse or fix up a theme that
     does not in `ui_theme_use`.

  Either way `uiThemeTest` needs a case that fails without it: a theme with `top` only, where measuring and drawing
  must agree on where the body starts.

### Optional, not blocking

- `tests/host/uiThemeTest.c:353` only ever sees a NULL soft key. My mutation "the `bottom` op gets no soft key"
  (`ui_screens.c:1636`) passed `uiThemeTest`. `uiPrototypeTest` catches it (2 failures), so the code is protected
  by `run-ui` as a whole. A case with a theme that has `bottom` only, `home_soft_key_label` set and Home drawn would
  close it in the unit test.
- `ui_theme.h:89`: a theme needs at least one palette (`ui_theme_set` reads `palettes[0]`). The header does not say.
- For step 2, not step 1: `ui_theme_use` on a theme with fewer palettes overwrites the palette in use, so going back
  to the first theme shows its first palette until the setting is applied again. The profile palettes also bypass
  the selected theme's palettes and `colors` op while the profile look lives in the reference theme.

## Findings from the first review

| # | Finding | Status | How I checked |
|---|---|---|---|
| 1 | Stack check loses theme op calls at run-time selection | Fixed | My own scratch theme, selected by `ui_theme_use` from a setting, `UI_THEME_SELECT=1`. GD-88 (C-SKY, no LTO): unresolved calls 53 as before, none a theme call; each of the 12 ops (`colors` and `layout` included) given a 1400-byte frame in turn makes the UI task OVER through `x_<op> (pointer)`. dm1701 (Arm, LTO): 71 as before, none a theme call; 6 of 6 ops tried are reported the same way |
| 2 | Flash cost | Accepted, now stated | Table below; my numbers match the implementer's within 8 bytes |
| 3 | UI task stack | Accepted | GD-88 unchanged at 1664 / 204 free; dm1701 keeps its +8 (1640 / 88); md9600 unchanged |
| 4 | GD-88 build unverified | Closed | GD-88 at 3b2fd11c: no `reference_ops`, `ui_theme_reference`, `ui_theme_selected` or `ui_theme_use` symbol, 0 warnings, every task fits |
| 5 | Look inferred from three things | Fixed | All three ops read `l->roomy`; dead gap arm gone; pixels identical |
| 6 | Frame trusts the geometry | Fixed | Clamp in `list_geom`; bounds in the header; my mutations of each clamp value are caught |
| 7 | No test for the new branches | Fixed | 35 mutations of my own: `uiThemeTest` catches 34, `uiPrototypeTest` the other |
| 8 | Geometry decided outside the ops | Partly | Layout goes through the op, `ui_list_rows` and `list_pitch` are gone, one formula for the strip. The measuring branch is open item 1 |

Nits: all done, or declined with a reason I accept (the two list bar flags stay under `CONFIG_UI_PROFILES`;
`uiPrototypeTest` catches all three mutations of them).

Two corrections to my first review. The C-SKY toolchain was installed (`~/toolchains/csky-elf`, not on `PATH`); I
was wrong to say it was not. And `ui_list_body_fits` never used `list_pitch`; only `ui_list_rows` did.

Flash, bytes, my builds:

| Build | `dc9e29d6` | `26fe0875` | `3b2fd11c` | Total |
|---|---|---|---|---|
| gd88 | 787968 | 788376 | 788368 | +400 |
| gd88 `CONFIG_UI_PROFILES=n` | 770472 | not built | 770888 | +416 |
| dm1701 | 706200 | 706360 | 706360 | +160 |
| md9600 | 685440 | 685604 | 685604 | +164 |
| mduv380 + profiles, LTO | 725824 | 725856 | 725936 | +112 |
| mduv380 + profiles, no LTO | 693216 | 693512 | 693528 | +312 |

## Rulings on the three disputed points

No point needs the user: I agree with the implementer on all three.

1. **`ui_list_body_fits` not routed through `list_geom`: agree, resolved.** The implementer is right on the fact
   (see my correction above) and on the design: the body's font, width and share of the room are the frame's. Open
   item 1 is a different matter: where the body starts, which that function's new comment says is the theme's.
2. **`ui_theme_set` / `ui_theme_get` keep their names: agree, resolved.** They are public, renaming them is not a
   step-1 change, and the name is an open question in the upstream discussion. The header explains the meaning. Note
   for step 2: `ui_theme_use` (a theme) and `ui_theme_set` (a palette) will sit side by side.
3. **`UI_THEME_SELECT` set by no firmware build: agree, resolved.** A fixture theme would cost flash and a build
   configuration for nothing a radio uses. The run-time path is compiled and tested on the host under
   AddressSanitizer, and I reproduced the stack-check proof on both toolchains. One condition for step 2: unresolved
   pointer calls do not fail the stack check, so the first build with a second theme must be checked with
   `STACK_CHECK_ARGS=-v` for theme calls (or gated with `--max-unresolved`).

## What I ran

| Check | Result |
|---|---|
| `uiSim --gallery`, `dc9e29d6` against `3b2fd11c`, 7 classes, 3 highlight colors | 1264 PNGs, 0 differ |
| Step-by-step frame comparison, same driver and scripts as the first review plus one new seed | 0 pixel differences in about 89,000 frames |
| ... gallery and audit scripts at `320x240iR10`, 6 variants | 1440 runs, 9324 steps |
| ... 58 targeted scripts, 10 geometries; again in Spanish, Italian, French | 8690 + 3 x 3476 steps |
| ... random walks | 600 runs at `320x240iR10`, 840 over the 7 other classes |
| ... each intermediate commit (4db45f54, 3a70ab87) | 0 differences, 0 warnings |
| Firmware at 3b2fd11c, default build and stack check: gd88, gd88 `CONFIG_UI_PROFILES=n`, gd88b, db25d, dm1801pro, dm32, rt10, dm1701, md380, md9600, mduv380 | 0 warnings, every task fits |
| mduv380 + profiles, LTO and no LTO | 0 warnings; UI task fits (free 88 and 96). The no-LTO run still fails on `prvIdleTask`, as before the theme work |
| `ui_screens.c`, `ui_app.c`, `ui_menus.c` compiled over 19 switch combinations on gd88 (C-SKY) and mduv380 (Arm); 5 combinations at each intermediate commit | 0 warnings, 0 errors |
| Run-time theme proof, gd88 and dm1701 | see finding 1 |
| `uiThemeTest` built with `CONFIG_UI_PROFILES=1`, `CONFIG_DUAL_WATCH=0`, `CONFIG_KEY_ASSIGN=0` | passes. With `CONFIG_UI_COLOR=0` it reports 4 failures, all its own assumption that a palette can be chosen; it is not built that way |
| `make -C tests/host -k` | only the known `run-settings` failure (field 74); `uiThemeTest` passes |
| `make -C tests/host SANITIZE=1 run-ui` | passes, no sanitizer report |
| `check-run`, `check_doc_links.py`, `check_generic.py`, `license_grep.sh`, `stack_check.py --self-test`, `git diff --check` | clean |

One walk reported a difference that is not one: both simulators crash at the same step with identical frames. It is
a division by zero in the simulator's own stub (`tests/host/sim/ui_sim.c:1512`, `zone_size` 0), present before the
theme work and outside these commits. The script is in `t-crash.txt`.

Not run: `tools/ci.sh`, `reuse lint` (not installed), HIL.

Scratch worktrees and `build/rv-*` trees are removed; the main checkout has no changes. Scripts and logs are in
`/tmp/claude-0/-home-user-RadioFoundryDG/5cf815ea-6a4b-5648-86c7-8189112daa08/scratchpad/review/` (`walk.py`,
`targeted.txt`, `mutate.py`, `ui_theme_x.c.orig`, logs in `round2/`).
