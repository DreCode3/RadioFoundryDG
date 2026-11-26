<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->
<!-- Saved from the cloud session's scratchpad; paths under /tmp/claude-0 no longer exist. -->

# Review: theme descriptor step 1 (48e1e944, 7013200b, 26fe0875)

Range `dc9e29d6..26fe0875`, files `core/ui/ui_screens.c`, `core/ui/ui_theme.h`, `docs/spec/ui.md`.
Line numbers are for the tree at 26fe0875.

## Verdict

I found no pixel or behavior change, no build-configuration bug and no warning. The "pure refactor" claim holds for
everything I could run. What I did find:

- one latent defect in the mechanism itself (the stack check cannot follow `UI_THEME_OP` once the theme stops being a
  compile-time constant), proven by experiment;
- a small flash and stack cost that the commit messages do not state;
- contract gaps and nits a maintainer is likely to raise.

Nothing was checked on the GD-88's own compiler: there is no C-SKY toolchain in this container. All firmware
numbers below are Arm (GCC 13.3) proxies.

## Confirmed by experiment

### 1. The stack check loses every theme op call when the theme is not a constant (latent)

`core/ui/ui_theme.h:85-88`, call sites `core/ui/ui_screens.c:266, 1257, 1606, 3748, 4362, 4472, 4546, 5119, 5136,
5153, 5155, 5626`.

Today `ui_theme_cur()` is `&ui_theme_reference`, GCC folds every op call into a direct call, and the stack check sees
them (verified, see "What I ran"). The header presents `UI_THEME_OP` as the extension point for a second theme. As
soon as `ui_theme_cur()` reads a stored pointer, `tools/stack_check.py` resolves none of the calls.

Experiment (scratch copy, mduv380): `ui_theme_cur()` changed to a plain `const ui_theme_t *` variable, nothing else.

| Macro form | Unresolved pointer calls (no LTO) | UI task result |
|---|---|---|
| As committed, `cur()->ops->op != NULL ? ... : ...` | 14 (baseline 1): all 13 theme call sites | "ok", deepest 1624 through `msgs_item`; the drawing subtree is not counted |
| `ui_theme_cur()->ops->op` (no `?:`, nested macro kept) | 14 | same |
| `ui_theme_sel->ops->op` (the variable named in the macro) | 1 | resolved |

With LTO the list goes from 71 to 84. Unresolved calls do not fail the check (`docs/dev/stack-check.md`), so the
status strip, bottom row, Home, Quick and list ops would leave the UI task's worst case silently while the report
still says "every task fits". The UI task has 80 to 96 bytes free on the Arm boards.

Fix, before step 2: name the pointer variable directly in the macro and resolve the NULL fallback somewhere the tool
can read (fill missing ops when the descriptor is defined, or one small dispatch function per op), or add `ptr` lines
for each op to `tools/stack_check_edges.txt`. Then check the `-v` list holds no `UI_THEME_OP` line.

### 2. Flash grows with every switch off (minor)

| Build | Before | After | Delta |
|---|---|---|---|
| dm1701 default (LTO) | 706200 | 706360 | +160 B (95.78% to 95.81%) |
| md9600 default (LTO, monochrome, no `CONFIG_UI_COLOR`) | 685440 | 685604 | +164 B |
| mduv380 + `CONFIG_UI_PROFILES`, LTO | 725824 | 725856 | +32 B |
| mduv380 + `CONFIG_UI_PROFILES`, no LTO | 693216 | 693512 | +296 B |

The descriptor and ops table are not in the image, as commit 48e1e944 says. The cost is `list_header` as a function
of its own (62 B, `ui_screens.c:4535`), the `ref_banner` wrapper (20 B) and a larger `ui_draw_list` (+48 B). The
monochrome build grows too, although its palette table shrank by three entries. The no-LTO row is the closest proxy
for the GD-88 (C-SKY builds have no LTO). Suggested: state the cost in the PR. `list_header`'s `noinline` solves a
stack path that only exists with the profile heading, so it could follow `CONFIG_UI_PROFILES` (not tested).

### 3. UI task stack: +8 bytes on one board, better with the profile look

| Build | Before | After |
|---|---|---|
| dm1701 default | deepest 1632, free 96 | deepest 1640, free 88 (`ui_task` frame 160 to 168) |
| md9600 default | deepest 1648, free 80 | unchanged |
| mduv380 + profiles, LTO | OVER by 48 (`msg_enter > list_top > draw_top_as > draw_profile_status`) | ok, free 88 |
| mduv380 + profiles, no LTO | OVER by 8 | ok, free 80 |

Frames without LTO: `ui_draw_list` 320 became 168, plus `ref_cell` 152 on the row path (320, equal) or
`ref_list_geom` 144 on the `l->item` path (312). `ref_bottom` is 328 against `draw_hint` 320, and
`profile_home_footer` 24 against 16, so Home paths through the bottom row cost 4 to 8 bytes more. No new static data
(RAM equal on dm1701 and md9600; plus or minus 8 bytes of alignment in the profile builds). No struct is passed by
value.

## Not verifiable here

### 4. Everything specific to the GD-88 build (possible)

The folding of the op calls depends on GCC reading through an `extern const` object defined later in the same file.
Arm GCC 13.3 at `-Os` does it with and without LTO. The GD-88 uses csky-elf-gcc 14.2 without LTO. If it does not
fold there, finding 1 applies today and the ops table is in flash. Check on a GD-88 build:
`csky-elf-nm <elf> | grep -E 'reference_ops|ui_theme_reference'` prints nothing, and
`make BOARD=gd88 stack-check STACK_CHECK_ARGS=-v` lists no `UI_THEME_OP` call. The GD-88 flash and stack deltas are
also unmeasured.

## By reading (not reachable today)

### 5. The profile look is now inferred from three different things

- `ref_list_geom` takes the 7 px inset from `l->roomy` (`ui_screens.c:4910-4911`).
- `ref_cell` takes "profile rows" from `g->x != 0` (`ui_screens.c:4974-4978`).
- `ref_list_head` still reserves the profile colors on `s->gui_profiles` (`ui_screens.c:4523`).

Before, all three came from `s->gui_profiles`. They agree because the only caller sets
`l.roomy = ui_st.gui_profiles` (`core/ui/ui_app.c:1956`); commit 7013200b says so. A list built with the flag and
`roomy` apart would get the profile colors reserved and reference rows with no inset, where the old code drew inset
tiles with a 2 px gap. That `: 2` arm of `gap = l->roomy ? 4 : 2` (`ui_screens.c:4976`) is now dead code. Fix: drop
the arm and state on `ui_list_t.roomy` that it carries the look, or pass the look to the two ops.

### 6. The shared frame trusts the theme's geometry

`ui_draw_list` divides by `g.rows * g.cols` (`ui_screens.c:5123`) and by `g.rows` (`5148`), and indexes
`g.col_w[col]` (`5149`). A `list_geom` op returning `rows == 0`, `cols == 0` or `cols > 2` divides by zero or reads
past `col_w[2]`. `ref_list_geom` never does. The header should state the bounds, or the frame should clamp.

### 7. The new branches have no test and cannot have one

The NULL-op fallback in `UI_THEME_OP` and the `colors == NULL` branch (`ui_screens.c:198`) are dead in every build,
and `ui_theme_cur()` cannot be overridden, so no host test can install a second descriptor. Upstream asks for a host
test with every code change and for tests on the branches that are easy to skip. A test binary built with a small
test descriptor (some ops NULL, a different geometry such as two unpaged columns) would cover the fallback, the
`colors` exception and the frame's scrolling.

### 8. Geometry still decided outside the ops

- `ui_theme_set` calls `layout_regions(&ui_layout)` directly on a palette switch (`ui_screens.c:191, 195`), not the
  theme's `layout` op.
- `ui_list_body_fits` and `ui_list_rows` use the shared `list_pitch` (`l->roomy` adds 10), not `list_geom`.
  `ui_list_rows` (`ui_screens.c:4814`) has no caller anywhere; it was already dead before these commits.
- `ref_list_head`'s measure branch repeats `ref_top`'s height formula.

None changes behavior now. Each is a place a second theme would silently get the reference look's numbers.

## Nits

- `ui_theme.h:48-49`: "the UI task's deepest path runs through here" does not match the stack check. The deepest
  chain is `ui_push > msgs_item > msg_row > ...` on dm1701 and md9600, and `msg_enter > ui_list_body_fits > list_top
  > ref_list_head > profile_list_head` in the profile build. "a frame more" is also wrong: a caller's struct adds
  bytes to the caller's frame.
- `ui_theme.h:14-17`: `UI_BAR_LIST` and `UI_BAR_NUMBERED` exist only with `CONFIG_UI_PROFILES`, and the shared frame
  computes them under the same switch (`ui_screens.c:5160-5163`). Consistent with their users, but they describe
  shared facts and cost nothing unguarded.
- `list_head` has a hidden duty: the reference op reserves palette entries because it is the first thing drawn after
  the clear. The header does not say so.
- Names: `struct ui_theme` and `ui_theme_t` next to `static uint8_t ui_theme` (`ui_screens.c:26`) and
  `ui_theme_set/get`, which mean a palette. The `ref_` prefix is also used by the reference-trim code (`ref_trim`,
  `ref_here`, `ref_same`). `.colors = accent_apply` (`ui_screens.c:5704`) is the one op without the prefix.
- Comments on `ref_list_geom` ("one column of them from edge to edge"), `ref_cell` and `ref_banner` describe the
  reference look only, while the bodies still hold the profile grid and colors.
- `palettes[]` lost its `[UI_THEME_COUNT]` bound (`ui_screens.c:143`). A `_Static_assert` on its size under
  `CONFIG_UI_COLOR` would keep the Settings list and the table in step.
- The `colors` exception to the fallback is honored at its one use (`ui_screens.c:197-199`), but only a comment
  stops `UI_THEME_OP(colors)`.

## What I ran

| Check | Result |
|---|---|
| `uiSim --gallery`, old against new, 7 display classes, 3 highlight colors | 1264 PNGs, 0 differ |
| Step-by-step frame comparison, old and new simulators fed the same tokens, frame compared after every token | 0 differences in about 80,000 frames at the final commit |
| ... every gallery and audit script at `320x240iR10` (the second look), plain and with `gui:original`, worst-case content, hints off, full contacts and channels | 1440 runs, 9324 steps |
| ... random walks (keys, calls, messages, alerts, zone offers, themes, highlight colors, look switch), seeded | 400 runs x 40 steps at `320x240iR10`; 840 runs x 40 steps over the 7 other classes |
| ... 58 targeted scripts: every focus of the Hub and Settings grids both ways, partial last page, Find header and no match, long and short message, banners and toasts over lists, Quick, Home and dual watch in both looks, two receivers, large text, emergency, volume | 10 geometries including `128x64` (1 bpp), `120x160`, `140x128` (Quick as a list); again in Spanish, Italian and French |
| ... each intermediate commit (48e1e944, 7013200b) against the base | 0 differences, 0 warnings |
| Harness sensitivity: page badge drawn after the cells in a scratch copy | 110 differences reported, so palette order changes are detected |
| Firmware builds old and new: dm1701 default, md9600 default, mduv380 with `CONFIG_UI_PROFILES` with and without LTO | 0 warnings; sizes and stack check above |
| `ui_screens.c` compiled for mduv380 over 18 switch combinations, LTO on and off (`UI_PROFILES`, `DUAL_WATCH`, `UI_COLOR`, `DMR`, `KEY_ASSIGN`, `SCAN`, `VOLUME_BAR`, `FM`, `VFO`, `DMR_TRUNK`, `DIAG`, `GPS`, `DMR_ROAMING`) | 0 warnings, 0 errors |
| Host gcc 13 at `-Os` and `-O2`, clang at `-Os` | 0 warnings |
| Disassembly of `ui_screens.o` without LTO | the only indirect calls are `l->item`; no `ui_theme_reference` or `reference_ops` symbol in any image |
| `make -C tests/host -k` at 26fe0875 | only the known `run-settings` failure (field 74) |
| `tools/check_doc_links.py`, `tools/check_generic.py`, `tools/license_grep.sh` | clean |

Not run: any C-SKY build (GD-88, the target of this work), `reuse lint` (not installed), `tools/ci.sh`, HIL.
The concurrent `build/ci` run was not touched; its `dm1701-maximal` failure is the flash overflow of 15.7 KB that
predates these commits.

Scratch worktrees and `build/rv-*` trees are removed. The comparison driver, the targeted scripts and the logs are in
`/tmp/claude-0/-home-user-RadioFoundryDG/5cf815ea-6a4b-5648-86c7-8189112daa08/scratchpad/review/` (`walk.py`,
`targeted.txt`, `cmp.py`, `ci.py`, `sc-*.log`, `fw-*.log`).
