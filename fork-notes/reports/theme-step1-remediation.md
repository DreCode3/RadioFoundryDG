<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->
<!-- Saved from the cloud session's scratchpad; paths under /tmp/claude-0 no longer exist. -->

# Theme step 1: remediation of the QA review

Three new local commits on `feat/gd88-gui`, on top of `8dd8ce37`. Nothing amended, nothing pushed, tree clean,
`build/t-*` removed, `build/ci` not touched. Trailer kept as before (Claude Fable 5.2).

| Hash | Subject | Findings |
|---|---|---|
| `4db45f54` | UI: theme op calls the stack check can follow | 1, 4, nits (names, the `colors` guard) |
| `3a70ab87` | UI: one source for the list look, bounded geometry, layout through the op | 2, 5, 6, 8, the other nits |
| `3b2fd11c` | tests: uiThemeTest, the theme framework with themes of the test's own | 7 |

## Acceptance, each commit

| Check | `4db45f54` | `3a70ab87` | `3b2fd11c` |
|---|---|---|---|
| Pixel compare against `base` | 2208 compared, 0 differ, 0 missing; layout rc 0 | same | same |
| `run-ui` | rc 0, 0 warnings | rc 0, 0 warnings | rc 0, 0 warnings, now with `uiThemeTest` |
| `uiPrototypeTest.py` | OK | OK | OK |
| gd88, dm1701, md9600, gd88 `CONFIG_UI_PROFILES=n` | 0 warnings | 0 warnings | 0 warnings |
| `stack-check` gd88, dm1701 | every task fits | every task fits | every task fits |

On the final commit also: default build and `stack-check` of db25d, dm1801pro, dm32, gd88b, md380, mduv380, rt10 (0
warnings, every task fits), `check-run`, `check_generic.py`, `check_doc_links.py`, `license_grep.sh` clean, and the
whole host suite, where the only failure is still `run-settings` (field 74, the known `gui_flags` offset).

Not run: `tools/ci.sh`, `reuse lint` (not installed), HIL.

## Finding 1: stack check visibility (`4db45f54`)

**What changed.** `UI_THEME_OP` is gone. Each shared entry point calls its op through a local pointer to the ops table
and names the reference function for the NULL case:

```c
const ui_theme_ops_t *o = ui_theme_cur()->ops;
if (o->top != NULL)
	return o->top(v, st, s, prefix, name);
return reference_top(v, st, s, prefix, name);
```

`tools/stack_check.py` types `o` from its declaration and takes every function any theme stores in that field as a
target; the fallback is a direct call it sees in the call graph. The design stays data plus an ops table with NULL
meaning "the reference theme's". `UI_THEME_SELECT` (0 in every build today) makes the theme in use a variable set by
`ui_theme_use()`; with it 0, `ui_theme_cur()` is still `&ui_theme_reference`.

I chose this over the alternatives because: complete ops tables with no NULL would turn a forgotten op into a call
through NULL on the radio; a switch on a theme id drops the ops table; `stack_check_edges.txt` lines are keyed by
field name alone (`top`, `layout`, `home`) and would have to list every theme's functions by hand.

**Proof, scratch build with a second theme chosen at run time.** Uncommitted: a `ui_theme_scratch` with its own
`colors`, `bottom`, `home`, `list_geom`, `cell` and the other ops NULL, selected by `ui_theme_use()` from a setting, built
with `-DUI_THEME_SELECT=1`. Run on the final commit with `STACK_CHECK_ARGS=-v`:

| Board | Unresolved pointer calls | Of them from `core/ui` | Theme calls resolved "by field name" only | Result |
|---|---|---|---|---|
| dm1701 (Arm, LTO) | 71, as before the theme work | 1: `left`, `ui_scr[...]->leave()` in `ui_app.c` (old) | 0 | every task fits |
| mduv380 (Arm, LTO) | 71 | the same 1 | 0 | every task fits |
| gd88 (C-SKY, no LTO) | 53, as before | the same 1 | 0 | every task fits |

`ui_theme_selected`, `ui_theme_scratch` and `reference_ops` are all in those images, so the calls really are indirect.

Positive check that the ops are counted: with the scratch cell op's buffer made 1200 bytes, the check fails on both
dm1701 and gd88 with the path through the pointer:

```
ui       OVER stack  2064  deepest  2008  free  -140
   ...  ui_draw_quick > reference_quick (pointer) > draw_quick_list > ui_draw_list (168)
        > scratch_cell (1224, core/ui/ui_theme_scratch.c, pointer) > gfx_text > ...
```

Logs: `fw/fscr-stackv-{dm1701,mduv380,gd88}.log` (final commit) and `fw/scr2-stackv-{dm1701,gd88}.log` (oversized op)
under the scratchpad.

**One-theme build still folded.** `nm` on gd88, gd88 `CONFIG_UI_PROFILES=n`, dm1701 and md9600 at the final commit:
no `ui_theme_reference`, `reference_ops` or `ui_theme_selected`. Sizes did not move with this commit.

## Finding 4: the GD-88 build (no change needed)

The C-SKY toolchain is installed; QA did not have it. On gd88 at every commit: the descriptor and table are not in
the ELF, the disassembly of `ui_draw_list` calls the reference ops directly, and the unresolved list has no theme call.
GD-88 flash and stack are in the tables below.

## Findings 2 and 3: cost, final numbers for the PR (`3a70ab87`)

`list_header` is `noinline` only with `CONFIG_UI_PROFILES`. Without the switch the UI task is at the same depth with
or without it (dm1701 1640, md9600 1648), so the 62 bytes went. The bounds of finding 6 cost 36 to 44 bytes, so the
remediation as a whole is flash neutral.

Flash, clean tree, bytes:

| Build | `dc9e29d6` (before the theme work) | `26fe0875` (step 1 as reviewed) | `3b2fd11c` (now) | Total |
|---|---|---|---|---|
| gd88 (C-SKY, no LTO, profile look on) | 787968 | 788372 | 788376 | +408 |
| gd88 `CONFIG_UI_PROFILES=n` | 770468 | 770888 | 770884 | +416 |
| dm1701 (LTO) | 706200 | 706360 | 706360 | +160 |
| md9600 (LTO, monochrome) | 685456 | 685604 | 685620 | +164 |
| mduv380 + profile look, LTO (QA's configuration) | 725824 | 725856 | 725936 | +112 |
| mduv380 + profile look, no LTO (QA's configuration) | 693216 | 693512 | 693520 | +304 |

The first two mduv380 columns are QA's measurements; mine use the same switches (`CONFIG_UI_PROFILES=y
CONFIG_FONT_AA=y CONFIG_FONT_XL=y CONFIG_NET_HTTP=n`, plus `LTO=0 CONFIG_APRS=n CONFIG_SATELLITE_TLE=n`). Two builds of
one commit differ by up to 16 bytes on md9600.

RAM is unchanged in every build (gd88 205800, dm1701 127512, md9600 121328, mduv380 + profiles 124224 and 119232).

UI task, `stack-check` (deepest / free beyond the 128-byte margin):

| Build | Before the theme work | Now |
|---|---|---|
| gd88 | 1664 / 204 | 1664 / 204 |
| dm1701 | 1632 / 96 | 1640 / 88 |
| md9600 | 1648 / 80 | 1648 / 80 |
| mduv380 + profile look, LTO | 1776, over by 48 (QA) | 1640 / 88 |
| mduv380 + profile look, no LTO | 1736, over by 8 (QA) | 1632 / 96 |

The no-LTO mduv380 run still exits with an error, as it did before the theme work: `prvIdleTask` is over by 24 in
that configuration (QA's `sc-old-nolto.log` has the same line). It is not a UI path.

## Finding 5: one source for the list look (`3a70ab87`)

`reference_list_head` (palette entries), `reference_list_geom` (inset, pitch) and `reference_cell` (panels) all read
`l->roomy`. `ui_screens.h` documents it as the field the list drawing takes the look from, set by the screen from
`ui_state_t.gui_profiles`. The dead `: 2` gap is removed. The grid branches follow the geometry's column count, which
is the geometry op's own output, and the comments say so. Evidence: pixel baseline identical.

## Finding 6: geometry bounds (`3a70ab87`)

`ui_theme.h` states that the op sets every field, `rows >= 1` and `1 <= cols <= UI_LIST_COLS` (the size of `col_w`).
`ui_draw_list` clamps both before use. `uiThemeTest` feeds it 0 rows, 0 columns, 7 and 255 columns.

## Finding 7: host test (`3b2fd11c`)

`tests/host/uiThemeTest.c`, built with `-DUI_THEME_SELECT=1` under AddressSanitizer, in the `run-ui` group (it tests
`core/ui`), listed in `docs/dev/testing.md` and `docs/spec/ui.md`. It checks:

- a theme with every op NULL lays out and draws eight frames pixel for pixel as the reference theme (Home, banner
  and toast, lists with header, body and empty, Quick, titled strip with hint, dual watch Home);
- the `colors` exception: no op leaves the palette exactly as written, a palette past the theme's count is its
  first, and an op runs after the copy on the colors in use;
- a theme's `layout`, `top`, `bottom`, `banner`, `home`, `home_dual`, `quick` and `list_head` are the ones called,
  with their arguments, and a measuring caller asks the head not to draw;
- the frame over two unpaged columns, the same paged, and one column: empty list, count below the cells on screen,
  focus on the last cell, one past it, on the last item, above the view, a stale `top`, a part-filled last page, a
  text line under the focus, `l->top` NULL, the Find header, the order of the four ops, each cell's box;
- the bounds above.

It fails without the code it covers. 17 one-line mutations of `ui_screens.c`, each rebuilt and run, all caught:

| Mutation | Result |
|---|---|
| NULL fallback dropped for `cell` / `bottom` / `layout` | 2 FAIL / 7 FAIL / ASan SEGV |
| NULL `colors` given the reference op | 6 FAIL |
| Palette not checked against the theme's count | ASan global-buffer-overflow |
| The theme's `top` / `list_head` ignored | 3 FAIL / 34 FAIL |
| Row scrolling off by one; no clamp at the list's end; paging by rows | 2 / 1 / 6 FAIL |
| Second column at the first column's x; a text line takes the focus | 21 / 1 FAIL |
| `scroll` not called before the cells | 30 FAIL |
| Rows / columns not bounded below | ASan FPE (division by zero), both |
| Columns not bounded above | ASan stack-buffer-overflow |
| `ui_theme_use` not re-running layout and colors | ASan SEGV |

Not covered by it: the test builds without `CONFIG_UI_PROFILES`, so the profile branches inside the reference ops and
the two list bar flags are still covered only by the pixel baseline and `uiPrototypeTest`.

## Finding 8: geometry through the theme (`3a70ab87`)

- `ui_theme_set` re-runs the theme's `layout` op on a look change, not `layout_regions`.
- `ui_list_rows` and its declaration are deleted (no caller), and `list_pitch` with it: the row pitch is in the
  geometry op only.
- `reference_top_y()` is the one formula for the y under the reference strip, used by `reference_top` and by the
  list head's measuring branch.
- `ui_list_body_fits`: see "Disputed".

## Nits

| Nit | Done |
|---|---|
| Wrong reason on the `bottom` op's comment | Rewritten: `ui_draw_hint` has only the text and the flag, and a struct would be built in every caller's frame |
| `UI_BAR_LIST`, `UI_BAR_NUMBERED` guard | Kept under `CONFIG_UI_PROFILES`, and the header says why: the `ui_list_t` fields they carry and the only code that reads them exist only with the switch, so unguarded nothing could set or test them |
| `list_head`'s hidden duty | Documented on the op: with `draw` true it is the first thing drawn after the clear, and where a theme takes palette entries on a 16-color display |
| `ref_` prefix | `reference_<op>` for all twelve; `accent_apply` is `reference_colors` |
| `ui_theme` static next to `ui_theme_t` | The static is `ui_palette` |
| Reference-only comments on ops that hold profile code | `reference_list_head`, `reference_list_geom`, `reference_cell`, `reference_scroll`, `reference_banner` say which profile branches they still hold |
| `palettes[]` bound | `_Static_assert` on its size against `UI_THEME_COUNT` under `CONFIG_UI_COLOR` |
| `UI_THEME_OP(colors)` misuse | The macro no longer exists; `colors` is called in one place, and `uiThemeTest` checks the exception |

## Disputed, for the user to decide

1. **`ui_list_body_fits` is not routed through `list_geom`.** The review says it uses the shared `list_pitch`. It did
   not: it uses the head's height, the frame's own rule for the body (half the room) and the small font, and never a
   row pitch. It already asks the theme for its head. I left the code and added a comment saying the body is the
   frame's, the same in every theme. If a theme should be able to change the body's font or share of the room, that
   is a new op, which I did not add.
2. **`ui_theme_set` / `ui_theme_get` keep their names**, although they set a palette. They are public, used by the
   settings code, the simulator and gfxTest, and the rename is question 3 of the upstream discussion. The header says
   that what Settings calls a theme is one palette of a `ui_theme_t`.
3. **`UI_THEME_SELECT` is set by no firmware build**, so the run-time path is compiled only in `uiThemeTest` and in
   my scratch build. CI's stack check will cover it from the first build that has a second theme; until then the
   proof above is a one-off. I did not commit a second firmware theme as a fixture, because it would cost flash and a
   build configuration for nothing a radio uses.

## Surprises

- QA's two mduv380 configurations are not `CONFIG_UI_PROFILES=y` alone: the image does not fit without also turning
  off HTTP (and APRS and satellite TLE without LTO). I took the switch sets from QA's build logs.
- Under LTO a selection variable that nothing writes is folded back to a constant, so a scratch proof needs a real
  store reachable at run time. Mine calls `ui_theme_use()` from a setting.
- The stack tool does expand a leading macro, but it cannot read a call whose callee is a `?:` expression, which is
  why the fallback had to become statements.
