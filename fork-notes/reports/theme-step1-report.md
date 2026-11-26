<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->
<!-- Saved from the cloud session's scratchpad; paths under /tmp/claude-0 no longer exist. -->

# Theme framework, step 1 (inventory steps 1.2, 1.3, 1.4): report

Branch `feat/gd88-gui`, three local commits on top of `dc9e29d6`. Nothing pushed, no branch created, tree clean,
`build/t-*` removed.

## Commits

| Hash | Subject | Inventory step |
|---|---|---|
| `48e1e944` | UI: theme descriptor, the reference look behind it | 1.2 |
| `7013200b` | UI: list screens as a shared frame and four theme ops | 1.3 |
| `26fe0875` | UI: the banner box takes its place and background from its caller | 1.4 (banner only) |

Files: `core/ui/ui_theme.h` (new), `core/ui/ui_screens.c`, one row in the file table of `docs/spec/ui.md`.

The commit trailer is `Co-Authored-By: Claude Fable 5.2 <noreply@anthropic.com>` plus the session line, not the
"Claude Opus 5.5" line in the task. This session's own attribution instruction names the model that wrote the commits
and says only the user's instructions override it. The commits are local, so `git rebase`/amend can change the line if
the user wants the other one.

## Checks, each step

| Check | 1.2 | 1.3 | 1.4 |
|---|---|---|---|
| uiSim builds, 0 warnings | yes | yes | yes |
| Pixel compare against `base` | 2208 compared, 0 differ, 0 missing; layout rc 0 | same | same |
| `make -s -C tests/host run-ui` | rc 0, 0 warnings | rc 0, 0 warnings | rc 0, 0 warnings |
| `python3 tests/host/uiPrototypeTest.py` | OK | OK | OK |
| gd88, dm1701, md9600, gd88 `CONFIG_UI_PROFILES=n` | 0 warnings | 0 warnings | 0 warnings |
| `stack-check` gd88, dm1701 | every task fits | every task fits | every task fits |

Also run on the final commit: default build and `stack-check` of every other board (db25d, dm1801pro, dm32, gd88b,
md380, mduv380, rt10) and `stack-check` of md9600, all with 0 warnings and "every task fits"; `check_generic.py`,
`check_doc_links.py`, `license_grep.sh` clean; the whole host suite (`make -k -C tests/host`), where the only failure
is `run-settings` ("settings_t field 74 at 407, layout 1 has it at 406"), the `gui_flags` offset the inventory and
CLAUDE.md already list as a CI blocker (these commits don't touch settings).

Not run: `tools/ci.sh` (maximal builds, the switch matrix), `reuse lint` (not installed here), the gallery make target
(the pixel compare renders the same gallery), HIL.

For 1.4 the baseline has no banner with dual watch on, so 70 extra frames (banners over dual watch Home, over the
hub, Settings and Quick, in the profile look's four variants and at 320x240, 160x128 and 128x64m) were rendered with
the simulator before and after the commit: identical.

## Sizes (clean tree at each commit, `--print-memory-usage`, bytes)

| Build | Baseline `dc9e29d6` | 1.2 `48e1e944` | 1.3 `7013200b` | 1.4 `26fe0875` | Total |
|---|---|---|---|---|---|
| gd88 FLASH | 787968 | 788008 (+40) | 788384 (+376) | 788372 (-12) | +404 |
| gd88 RAM | 205800 | 205800 | 205800 | 205800 | 0 |
| dm1701 FLASH | 706200 | 706264 (+64) | 706360 (+96) | 706360 (0) | +160 |
| dm1701 RAM | 127512 | 127512 | 127512 | 127512 | 0 |
| md9600 FLASH | 685456 | 685492 (+36) | 685604 (+112) | 685604 (0) | +148 |
| md9600 RAM | 121328 | 121328 | 121328 | 121328 | 0 |
| gd88 `CONFIG_UI_PROFILES=n` FLASH | 770468 | 770532 (+64) | 770872 (+340) | 770888 (+16) | +420 |
| gd88 `CONFIG_UI_PROFILES=n` RAM | 205720 | 205720 | 205720 | 205720 | 0 |

The baseline numbers in the task (787992, 706244, 685484) were of a dirty tree: `-dirty` in the version string and its
alignment add 24 to 44 bytes. Two builds of the same commit also differ by up to 16 bytes on md9600 (685456 and
685440 for `dc9e29d6`), so differences that small are noise.

Where the growth comes from:

- **The indirection itself compiles away.** `ui_theme_cur()` is `&ui_theme_reference`; every `UI_THEME_OP(...)` call
  is a direct call in the image, and neither `ui_theme_reference` nor the ops table is in any ELF (checked with `nm`
  and in the disassembly of `ui_draw_list`). No pointer is stored or followed.
- **1.2, +36 to +64.** On gd88 (C-SKY, built without LTO) a function whose address is in the ops table is no longer
  inlined into its one caller, so `ref_layout`, `accent_apply`, `ref_quick`, `ref_banner` and `ref_home` are separate
  functions behind small stubs (about 44 bytes). On the LTO boards the bodies are inlined as before; the rest is
  register allocation moving in callers (`ui_task`, `handle`, `ui_home_draw`), because the ops are no longer
  specialized (`draw_hint.isra` became plain `ref_bottom`).
- **1.3, +96 to +376.** Four functions with their own prologues and a geometry and a cell struct passed by pointer,
  where one function held everything in registers. On the LTO boards they are inlined back and the cost is the struct
  (`ui_draw_list` +92). On gd88 they stay separate: `ui_draw_list` 2434 to 528, plus `ref_cell` 1376,
  `ref_list_geom` 398, `ref_scroll` 284, `ref_list_head` 118, `profile_list_head` 172, `list_header` 54. This is the
  largest step and at the top of "a few hundred bytes". It could be taken back in a one-theme build by forcing the
  reference ops inline, but the GD-88 will build two themes, where the calls are real, so that was not done.
- **1.4, -12 to +16.** Noise.

RAM (`.data` + `.bss`) is unchanged on every board. Everything added is `const` or on the stack.

## Stack (`make stack-check`, UI task, bytes)

| Board | Baseline deepest / free | After 1.2 | After 1.3 | After 1.4 |
|---|---|---|---|---|
| gd88 | 1664 / 204 | 1664 / 204 | 1664 / 204 | 1664 / 204 |
| dm1701 | 1632 / 96 | 1640 / 88 | 1640 / 88 | 1640 / 88 |

"Every task fits" on both at every step; the least free is unchanged (gd88 `start` 68, dm1701 `prvIdleTask` 24).

- dm1701 +8: `ui_task`'s own frame went from 160 to 168 in 1.2. Its deepest path (`ui_push > msgs_item > ...`) does
  not pass through the changed code; the frame holds everything LTO inlines into the task, and the allocation moved.
- gd88 during 1.3: the first version of the split made a new deepest path, 1728 (free 140):
  `msg_enter > ui_list_body_fits > list_top > ref_list_head > ui_draw_top > ref_top > draw_profile_status > ...`. The
  path is not real (a measuring call never draws) but the checker follows it, and the split had put one more frame
  on it. Fixed in the same commit: the profile heading and the Find header each have their own `noinline` frame, so
  the path is shorter than before the split and the UI task is back at 1664.
- Final commit, other boards (no baseline measured for them): UI free db25d 204, dm1801pro 272, dm32 272, md380 188,
  mduv380 88, rt10 360, md9600 80; all pass.

## The descriptor as built (`core/ui/ui_theme.h`)

```c
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// A theme: how the screens look. The shared code owns what a screen holds and how it behaves (the menu tree, the
// keys, scrolling, the text and its translations) and calls the theme in use for geometry and pixels. A theme is a
// const descriptor: its palettes and its drawing ops. It owns no RAM; the colors and metrics in use stay in ui_col and
// ui_layout (ui_screens.h). What Settings > Display calls a theme (UI_THEME_*) is one palette of it
#ifndef UI_THEME_H
#define UI_THEME_H

#include "ui_screens.h"

// The bottom row's text (ops->bottom)
#define UI_BAR_TOAST	0x01u	// a toast, not key hints (the value of ui_draw_hint's toast)
#if CONFIG_UI_PROFILES
#define UI_BAR_LIST	0x02u	// under a roomy list (ui_list_t.roomy)
#define UI_BAR_NUMBERED	0x04u	// ... whose rows the digit keys pick (ui_list_t.numbered)
#endif

// Where a list's cells go (ops->list_geom). The shared list code (ui_draw_list) picks the cells on screen from it and
// moves the list; the first column is filled first
typedef struct {
	const font_t *font;	// the rows' text
	int16_t x, y;		// the first cell's corner
	int16_t pitch;		// from one row to the next: a cell's height
	int16_t col_w[2];	// each column's width
	int16_t col_gap;	// between columns
	uint8_t rows, cols;	// cells on screen: rows * cols
	bool paged;		// the list moves a page at a time, not a row
} ui_list_geom_t;

// One cell of a list (ops->cell): its box, and whether it has the focus
typedef struct {
	int16_t x, y, w, h;
	bool focus;
} ui_cell_t;

// An op left NULL is the reference theme's (UI_THEME_OP), so a theme gives only what it draws differently
typedef struct ui_theme_ops {
	// Fonts for the display and the regions, into *L
	void (*layout)(ui_layout_t *L);
	// The theme's appearance options over the palette just copied into *c (the reference theme: the highlight color).
	// NULL: the palette as it is, not the reference theme's op
	void (*colors)(ui_colors_t *c, uint8_t palette);
	// The status strip, and the title when there is one. st: the state its pill shows. Returns the y where the
	// screen's content starts
	int (*top)(const radio_view_t *v, radio_state_t st, const ui_state_t *s, const char *prefix, const char *name);
	// The bottom row: key hints (groups separated by two spaces, with text alternates) or a toast. soft_key: the label
	// of Home's assigned soft key, NULL without one. Plain arguments, no struct: the UI task's deepest path runs
	// through here (docs/dev/stack-check.md), and a caller's struct would be a frame more on it
	void (*bottom)(const char *text, uint8_t flags, const char *soft_key);
	// The banner, if one is up, over the lower part of the screen
	void (*banner)(const ui_state_t *s);
	// Home, the whole frame
	void (*home)(const radio_view_t *v, const ui_state_t *s);
#if CONFIG_DUAL_WATCH
	void (*home_dual)(const radio_view_t *v, const ui_state_t *s);	// ... while dual watch runs
#endif
	// Quick settings, the whole frame
	void (*quick)(const radio_view_t *v, const ui_state_t *s);
	// A list screen. ui_draw_list owns its frame (the body text, which cells are on screen, the banner, the bottom row)
	// and calls these four.
	// The strip and the title; returns the y below them. draw false: only measure (v and s are NULL)
	int (*list_head)(const radio_view_t *v, const ui_state_t *s, const ui_list_t *l, bool draw);
	// Where the cells go, between y and y_end
	void (*list_geom)(const ui_list_t *l, int y, int y_end, ui_list_geom_t *g);
	// One cell: its background and focus, the key's digit, the label and the value, an open editor
	void (*cell)(const ui_list_t *l, const ui_list_geom_t *g, const ui_cell_t *c, const ui_item_t *it);
	// That the list has more before top or after the cells on screen. Called before the cells are drawn (over false)
	// and after them (over true); a theme draws in one of the two. Marks on the cells need the second. What is drawn
	// first gets its colors first on a display with a 16-color palette, which is what the first is for
	void (*scroll)(const ui_list_geom_t *g, uint16_t top, uint16_t count, bool over);
} ui_theme_ops_t;

typedef struct ui_theme {
	const ui_colors_t *palettes;	// by palette (ui_theme_set), before ops->colors
	uint8_t palette_count;
	const ui_theme_ops_t *ops;
} ui_theme_t;

// Today's look (ui_screens.c)
extern const ui_theme_t ui_theme_reference;

// The theme in use. One theme is built, so it is a constant: the compiler resolves every op call and no pointer is
// stored or followed
#define ui_theme_cur()	(&ui_theme_reference)

// The op of the theme in use, the reference theme's where it has none
#define UI_THEME_OP(op)	(ui_theme_cur()->ops->op != NULL ? ui_theme_cur()->ops->op : ui_theme_reference.ops->op)

#endif
```

The ops table and the descriptor are at the end of `ui_screens.c` (`reference_ops`, `ui_theme_reference`).

| Op | Reference function | Shared entry point that calls it |
|---|---|---|
| `layout` | `ref_layout` (fonts by display class, then `layout_regions`) | `ui_layout_init` |
| `colors` | `accent_apply(c, palette)` | `ui_theme_set` (after copying `palettes[theme]`) |
| `top` | `ref_top` (was the body of `draw_top_as`) | `draw_top_as`, `ui_draw_top` |
| `bottom` | `ref_bottom` (was `draw_hint`) | `draw_hint`, `ui_draw_hint`, `ui_draw_bottom` |
| `banner` | `ref_banner` | `ui_draw_banner` |
| `home` | `ref_home` | `ui_draw_home` (which first sends dual watch to `ui_draw_home_dual`) |
| `home_dual` | `ref_home_dual` | `ui_draw_home_dual` |
| `quick` | `ref_quick` | `ui_draw_quick` |
| `list_head`, `list_geom`, `cell`, `scroll` | `ref_list_head`, `ref_list_geom`, `ref_cell`, `ref_scroll` | `ui_draw_list` (and `list_top` for the measuring callers) |

Every public function the tests use is unchanged in name and signature.

Differences from inventory section 3, and why:

| Proposed | Built | Why |
|---|---|---|
| `bottom(s, const ui_bar_t *)` | `bottom(text, flags, soft_key)` | `ui_draw_hint(text, toast)` has no `ui_state_t`, and a struct in the caller is one more frame on the UI task's deepest path. Flags: `UI_BAR_TOAST`, and under `CONFIG_UI_PROFILES` `UI_BAR_LIST` and `UI_BAR_NUMBERED`, which carry `ui_list_t.roomy` and `.numbered` until the list flags go |
| `scroll(g, top, count)` | `scroll(g, top, count, over)`, called before the cells and after them | See "Surprises", the 16-color palette |
| `colors`: NULL copies the table | The shared code always copies `palettes[palette]`; `colors` then applies the theme's own options. NULL means no options, not the reference op | A theme without a highlight color must not get the reference one. The one exception to the NULL fallback |
| `ui_list_geom_t` with `col_w[3]`, `cell` kind, `col_major` | `col_w[2]`, no kind, columns always filled first | Nothing uses three columns, a cell kind or row-major order yet |
| `ui_cell_t.index` | left out | No op reads it |
| `ui_theme_t`: `id`, `name`, `layouts`, `icons`, `fits`; `ui_palette_t` with name and flags | `palettes` (plain `ui_colors_t`), `palette_count`, `ops` | The rest belongs to selection (step 2): nothing reads it yet, and a name needs a translation key |
| `animate` | not added | It lives in `ui_app.c` (`profile_tick`) and belongs with the second theme (step 3) |
| `ui_strip_draws` and `ui_hint_draws` counted in the dispatchers | left where they were | The profile strip is drawn outside the `top` op in two places (list heading, profile Home), so moving the counter now would need those to count too |

## Surprises

1. **Draw order changes pixels on the 16-color panel.** Drawing the profile page badge ("1/2") after the cells
   instead of before changed 119 pixels on the hub and 44 on Settings: the palette's 16 entries go to whoever draws
   first, and the first label's edge shade lost its entry. So `scroll` is called twice, before and after the cells:
   the reference marks sit on the rows and need the second call, the badge needs the first. Any later reordering of
   drawing in the second theme will hit the same effect.
2. **The stack check follows measuring calls into drawing code** (above). It will matter again when ops are added:
   an op that both measures and draws puts its drawing path under every measuring caller.
3. **gd88 builds without LTO**, so there the "compiles away" result is direct calls but separate functions. The
   STM32 boards with `LTO=1` inline them back.
4. **Without `CONFIG_UI_COLOR` the palette table was 4 entries with 3 empty.** Read through the descriptor it was no
   longer folded into immediates and md9600 carried the whole 144-byte table; it is now sized by the build (one
   palette there, 36 bytes).
5. **`ui_list_geom` has no `ui_state_t`.** The profile inset was `s->gui_profiles ? 7 : 0`; the geometry op uses
   `l->roomy`, which `list_draw` sets from the same flag. The two can only differ for `draw_quick_list` on a profile
   display, and that list is only drawn under 150 px wide, where the profile look is never on.
6. **In `draw_banner` the test `bottom != L->hint_y` was always true for the profile footer** (41 px against 38 px
   or a text line), so the footer now passes the inset as a constant.
7. Two builds of one commit differ by up to 16 bytes, and a dirty tree by 24 to 44, so sizes were taken on the
   committed tree each time.

## Not done, and why

- **`ui_hint_fit` and `ui_dual_body`** (the other two helpers of 1.4). Neither was needed for the split. What differs
  between the looks in the hint row ("OK open" as "OK", which group goes right) and in dual watch (call history, the
  caller's name, the detail row) is behavior that gui-plan decides is shared, to be made the same in step 4. A
  helper's parameters written now would be the ones that step removes. The hint code is also on the UI task's
  deepest path. Only the dual watch inset (12 px) is pure geometry.
- **No new host test.** Every UI test and the gallery already run through the dispatchers; a test of the NULL-op
  fallback cannot exist while one theme is built. An upstream PR will need to say so in its Testing table.
- **`ui_list_rows`** still uses `list_pitch` directly; it has no caller and step 5 removes it.

## What remains

Step 2 (selection):

- Replace the look conditions by the current theme, palette and layout index. Five of the six are left:
  `profile_theme()`, `s->gui_profiles`, `ui_st.gui_profiles`, the `ui_list_t` flags, and the inset test (now
  `g->x != 0` in `ref_cell` and `inset` in `ref_bottom`). The sixth, `bottom != hint_y`, went in 1.4.
  `ui_theme_cur()` becomes a stored pointer when more than one theme is built.
- Add `id`, `name`, `layouts`, `fits` to `ui_theme_t`; palettes get names and the one-hue flag (`ui_gps.c` tests
  `UI_THEME_NIGHT`); the theme id in the spare settings byte with 0 as the build's default, with `settingsTest.c`
  and the spec table (the offset test already fails for `gui_flags`).
- `ui_theme_set` still validates the profile palette ids and re-runs `layout_regions` itself under `#if`; that
  becomes "the theme changed: call its `layout`".
- `ui_list_t.kind` set from the screen definition replaces `roomy/menu_icons/two_columns/main_menu`; `numbered` and
  the `UI_BAR_*` list flags then need no `#if`.
- The simulator selects a theme directly; `UI_FEAT_GUI_PROFILES` goes; separate the board fixture from the theme.

Step 3 (second theme, one op per commit into `core/ui/themes/<name>/`), in this order of readiness:

- `colors` and palettes: `profile_palettes`, `profile_colors` as a private const table indexed by palette (the four
  `profile_*` statics are 8 bytes of RAM today).
- `layout`: the `strip_h` / `hint_y` override in `layout_regions`.
- `top`: `draw_profile_top`, `draw_profile_status`; then the `ui_strip_draws` counter can move to the dispatcher.
- `banner`: `banner_box` is ready; move the `#if` out of `ref_banner`, export `banner_box`.
- List ops: `profile_list_head` and `profile_list_colors` are already separate functions; `ref_list_geom`,
  `ref_cell` and `ref_scroll` each still hold the profile branches (two columns, panels, icons, badge), to be split
  out. Icons as a data table with one blitter.
- `bottom`: the seven profile blocks in `ref_bottom`, after step 4 decides the hint policy.
- `home`, `home_dual`: `draw_profile_home` and the nine dual watch blocks; `profile_operational_home` falls back to
  the reference Home, which a theme can do by calling the reference op.
- `animate`: the cadence part of `profile_tick` in `ui_app.c`.
- Then the flash and maximal-build blockers CLAUDE.md lists, and the theme tests (gallery and `uiSim --layout` per
  theme).
