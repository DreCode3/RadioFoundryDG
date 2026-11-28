<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->

# Adding a UI theme

A theme changes how the radio's screens look, never what they hold or how they behave. The framework, the reference
theme and the panels theme are specified in [ui.md](../spec/ui.md) section 9; this page says how to write a theme,
make it fit a display, give it icons and test it. The panels theme (`core/ui/themes/panels/panels.c`) is the worked
example throughout.

## Before you start

- A theme draws; the shared code decides. A theme never adds, removes or reorders a menu entry, changes what a key
  does or hides a feature. If your look needs a behavior change (a new value on a tile, a different hint), make it in
  the shared code for every theme, as its own pull request.
- Shared code never asks which theme is in use. Where a theme needs the shared code to act differently, it declares
  an option (`UI_THEME_OPT_*`, [ui.md](../spec/ui.md) 9.4) and the shared code tests the option.
- Every theme multiplies the screens to review and the gallery classes to keep. Open an issue with mock-ups first
  ([Contributing](contributing.md)).
- The UI is designed from first principles ([docs/ux/principles.md](../ux/principles.md)), not from another firmware's
  screens, icons or menus, stock firmware included ([CLEANROOM.md](../CLEANROOM.md)).

## The two layers

### Data: palettes, options and the fit

Most of a look is data, and a theme can be data alone: a descriptor (`ui_theme_t`, `core/ui/ui_theme.h`) with its own
palettes, the options it honors, a fit test and an ops table whose entries are all NULL. Every NULL op is the
reference theme's, so such a theme draws every screen as the reference theme does, in its own colors.
`uiThemeTest` checks exactly that.

```c
static const ui_colors_t sea_palettes[] = {
	{ .bg = RGB565(250, 252, 255), .fg = RGB565(10, 30, 60), .dim = RGB565(80, 100, 130), /* ... every token */ },
};
static const ui_theme_ops_t sea_ops = { 0 };	// every op the reference theme's
const ui_theme_t ui_theme_sea = {
	.palettes = sea_palettes,
	.palette_count = sizeof sea_palettes / sizeof sea_palettes[0],
	.id = UI_THEME_ID_SEA,
	.options = 0,
	.ops = &sea_ops,
	.fits = NULL,	// every display
};
```

| Data | How to give it |
|---|---|
| Colors | `palettes`: every `ui_colors_t` token for each palette. Colors the tokens lack go in your own table, indexed by the palette, and a `colors` op copies them for the palette in use (panels: `panels_colors`) |
| Options | `options`: the `UI_THEME_OPT_*` rows Settings > Display shows for your theme, and the behavior flags (`UI_THEME_OPT_NAME_FIRST`, `UI_THEME_OPT_SOFT_KEYS`). A palette choice: `UI_THEME_OPT_PALETTE` (Colors) or `UI_THEME_OPT_DAY_NIGHT` |
| Fonts and regions | A `layout` op of a few lines: `ui_layout_fonts(L, aa)` for the reference fonts, your changes, then `ui_layout_regions(L, strip_h, hint_y)` (panels: `panels_layout`) |
| Fit | `fits`: whether the theme can draw on this display (below) |

The descriptor has no metrics or icon-set field. Row heights, margins and icons belong to the ops that use them, so a
theme with its own metrics or icons is an ops theme.

### Ops: your own composition

An op draws one element; give the ones whose composition you change and leave the rest NULL. The ops and what each
must do are in [ui.md](../spec/ui.md) 9.3.

| To change | Write |
|---|---|
| The status strip and titles | `top`, and `list_head` if a list's heading differs |
| The bottom row (hints, soft keys, toasts) | `bottom` |
| List rows, grids, the value editor | `list_geom` (where the cells go, how many columns) and `cell` (one cell). Use `ui_list_t.kind` for the Menu and Settings grids |
| The scroll position | `scroll` |
| Home, dual watch's Home, Quick | `home`, `home_dual`, `quick` |
| Marks that move | `animate` |

Rules every op follows:

- **Measure and draw agree.** `top` and `list_head` are called with `draw` false to measure a screen before it opens
  (`ui_list_body_fits`) and with `draw` true later. Both calls must return the same y.
- **16-color panels.** Every color board draws into a 16-entry palette (`GFX_INDEXED`), and anti-aliased edges take
  free entries until four are left. Reserve your solid colors first, in `list_head` and at the start of `home` (`gfx_reserve_colors`), so
  text edges get the nearest color instead of taking yours.
- **Stack.** The UI task has a small stack and the ops are on its deepest paths. Keep buffers small, put a function
  with large buffers out of line (`__attribute__((noinline))`) away from its caller's frame, and run
  `make BOARD=<b> stack-check` ([Stack check](stack-check.md)); `STACK_CHECK_ARGS=-v` lists a theme call it couldn't
  resolve. Call an op only the way `ui_screens.c` does (a local pointer to the ops table, the reference function as the
  fallback), so the tool can follow it.
- **No text comparisons.** Shown text depends on the language. Decide by structure and flags: a hint's groups and the
  key name each starts with ("OK", `HINT_BACK`), `UI_BAR_*`, `ui_list_t` fields, item flags. Never compare a label.
- **Text alternates.** A string may hold `{long|short}` forms (`TEXT_ALT_*`, `font.h`). Walk them with
  `gfx_text_alt(text, level, ...)` and pick the longest that fits; never shrink the font below your theme's minimum
  instead.
- **Reuse the shared helpers.** `ui_theme.h` lists them (`UI_THEME_SHARED`): Home's hint and soft key, the frequency
  being typed, the dual watch panes (`ui_home_dual_body` with your `ui_dual_style_t`), the banner box, `ui_row_text`
  and `ui_row_edit`, `ui_wrap_line`, `ui_fit_end`. For a channel kind your Home doesn't draw, `ui_reference_home` and
  `ui_reference_call_body` draw the reference theme's body under your strip, as panels does for trunked channels.
- **No RTOS, no blocking.** Ops run in the host tests. State lives in `ui_state_t` and `radio_view_t`; a theme keeps
  at most a few statics of its own (panels: the sliding row's position).

## Wiring a theme into the build

1. Write `core/ui/themes/<name>/<name>.c`, with the SPDX header, the whole file under `#if CONFIG_UI_THEME_<NAME>`.
2. Add the switch: its comment and default (`n`) in `config/features.mk`, the name in `FEATURES`, its dependency
   check (a color theme needs `CONFIG_UI_COLOR`, as `UI_THEME_PANELS` does), and its default in `core/config.h`. Extend
   `UI_THEME_SELECT`'s default there to `CONFIG_UI_THEME_PANELS || CONFIG_UI_THEME_<NAME>`.
3. Add the directory to `CORE_DIRS` in the `Makefile`, as the panels line does.
4. Give it the next number in the `UI_THEME_ID_*` enum (`ui_theme.h`). The number is stored in the settings (byte 407,
   [settings-store.md](../spec/settings-store.md)): never reuse or reorder one. `CONFIG_UI_THEME_DEFAULT` then accepts
   it for a board that starts in your theme.
5. Declare `extern const ui_theme_t ui_theme_<name>;` in `ui_theme.h`, add it to `ui_themes[]` in `ui_screens.c`, and to
   `v_gui_theme` and `n_gui_theme` in `ui_menus.c` with its Theme row name (`TRN(SET_VALUE_THEME_<NAME>, "...")`). Run
   `python3 tools/i18n.py --sync` and translate the name ([Translating](translating.md)).
6. Widen the guards your theme needs. The shared pieces only the panels theme reads are guarded by
   `CONFIG_UI_THEME_PANELS` today: the `UI_THEME_SHARED` helpers, `ui_list_t.kind`, `numbered`, `ok_acts`,
   `back_acts`, `ui_list_bar` and the `UI_BAR_*` list flags, `ui_item_t.icon` and the `UI_MENU_ICON_*` names,
   `gfx_coverage_pixel`, `gfx_reserve_colors` and `gfx_text_title`. Change each guard to a condition that covers both
   switches, at the definition and at every caller.
7. If the theme can't draw on some panels, add a size rule to `config/features.mk` beside the panels one, so the
   board's `hardware.gallery` class and insets make the switch `BOARD_UNSUPPORTED` where it can't fit (below).
8. Docs: a row in [build-config.md](../build-config.md) and in the User interface table of
   [feature-matrix.md](../feature-matrix.md), a `[feature.ui_theme_<name>]` entry in `tests/hil/features.toml`, the
   theme in [ui.md](../spec/ui.md) section 9, and its name in the Theme row of the user manual's settings reference.

## The cost rule

A switch that is off costs nothing. Guard every definition with the same condition as its callers, not a narrower
one, and keep shared helpers `static` where only one theme is built (`UI_THEME_SHARED` expands to `static` there, so
the compiler keeps inlining them). With the reference theme alone the ops table folds away and its calls are direct;
keep the call form so it stays that way.

Check it before the pull request:

```
make BOARD=<b> BUILD=build/t-on                                   # your theme on
make BOARD=<b> CONFIG_UI_THEME_<NAME>=n BUILD=build/t-off         # off: must build with zero warnings
```

Compare the `off` image against the same board built from the commit before your change: the two should match, or the
pull request says where every byte went. `tools/ci.sh` builds each default-on switch off alone and reports the sizes
(`no-UI_THEME_<NAME>`); the feature matrix takes its figure from there. Remove the `build/t-*` trees afterwards.

## Making a look fit a display size

The UI draws for a display class, not a radio: the drawable area inside the board's safe-area insets
(`gfx_width`, `gfx_height`), its color depth (`gfx_bpp`) and whether it is color (`CONFIG_UI_COLOR`). Which radios have
which class is in [feature-matrix.md](../feature-matrix.md) and each board's `hardware.gallery`.

1. **Find the smallest area the look holds.** Count what a screen must show at its minimum: the panels theme needs
   three list rows under its strip and title, above its soft key bar, in 8x16 text, which is 158 x 127.
2. **Write `fits()` from the measured area**, after `gfx_init`. A board that finds its panel at start-up sets its size
   in its display's `init`, so one image can meet two sizes.
3. **Use more than one layout where one doesn't stretch.** Panels has a full layout from 220 x 200 and a compact one
   below it, each a table of metrics (`panels_metrics_t`) chosen by the measured size. Derive row counts from the room,
   not from a fixed number.
4. **Leave out what a board can't use.** `GFX_MAX_ROWS` (the frame buffer's rows) and `GFX_MIN_ROWS` (the fewest rows
   the board's panel can have) are build-time constants: a test on them (`PANELS_COMPACT_BUILT`) lets the compiler drop
   a layout and its icons from a board that never shows them.
5. **Fit text by words, not by font size.** Give a long label a shorter form in the shared string (`{Connections|Links}`),
   pick it by measuring, and shorten at the end only as the last step. Keep a minimum text size (8x16 in panels). A
   translation keeps the English's number of `{...}` groups (`tools/i18n.py` checks it); its forms may differ.
6. **Mark where it can't draw.** The `config/features.mk` size rule (step 7 above) keeps the maximal build and the
   feature matrix honest for boards whose panel the theme can't fit.
7. **Check every class you claim.** `tests/host/uiSim --geom <class> --script "<keys>" --out f.png` renders one screen;
   `uiSim --layout <class>` runs the layout check on every gallery screen of that class; `uiSim --ellipsis <class>`
   lists every text still shortened in the middle.

## Icons

The panels icons are drawn in code, from first principles, by `tools/profile_icons.py` and generated into
`core/ui/themes/panels/panels_icons.h`: one family on a 24 px canvas with a 20 px live area and 2 px strokes, each icon
a 2-bit coverage mask trimmed to its ink, plus the same drawings at 0.8x (a 20 px canvas) for the compact layout. The
theme draws every icon, in the strip and on the tiles, with one blitter in one ink (the accent on focus).

To change or add one:

1. Write or edit its drawing function (the `Canvas` primitives: `line`, `arc`, `ring`, `disk`, `poly`, `rrect`).
2. For a new Menu entry or Settings group, add its `UI_MENU_ICON_*` name to `ui_menu_icon_t` (`core/ui/ui_screens.h`),
   to the `ICONS` list in the same order, and set it on the entry (`hub_item`, the `GROUP()` table in `ui_menus.c`).
3. Run `python3 tools/profile_icons.py` (it needs Pillow 13.0, which the script declares) and commit the header. The
   same Pillow version gives the same bytes.
4. Re-render the gallery and look at the icon at 1x, Day and Night.

A theme of your own can generate its icons the same way into its own header (`profile_icons.py <OUT>` writes
elsewhere), or keep data in any form its ops draw.

## Testing a theme

| Test | What to add |
|---|---|
| `uiThemeTest` | A build of it with your switch, as `uiThemeTestPanels` is built in `tests/host/Makefile` (with `GFX_FIT_TRACE`, so the test sees every text drawn and shortened), added to `run-ui`. Check your frames by structure, not pixels: text inside its region, no text under your minimum, no overlap, each layout's row counts, and your options |
| The gallery | A class in `sim_geoms[]` (`tests/host/sim/ui_sim.c`) for each display your theme is drawn on, set to start in your theme as the `-panels` classes are (`sim_geom_t.panels` selects the panels theme; a second theme needs its own field). `make -C tests/host ui-gallery` renders it into `docs/ux/screens/<class>/` |
| The layout check | Nothing more: `uiSim --layout` runs every class in `sim_geoms[]`, in the theme it starts in, with ordinary and worst-case content, and fails on text outside the safe area, overlaps, cut text and middle ellipses |
| Flows | `uiFlowTest` runs the task flows on the reference classes. Your theme doesn't change them; a flow that fails only in your theme is a theme bug |
| Script checks | A `uiSim --script` with `expect:` checks for what structure can't show, as `tests/host/uiPrototypeTest.py` does for panels at 310 x 240 |

**A refactor with no visual change.** Moving drawing behind an op must change no pixel. Render the gallery before and
after: `make -C tests/host ui-gallery` rewrites `docs/ux/screens/`, and `git status --short docs/ux/screens` must list
nothing. The gallery covers every screen in `gallery[]` (`tests/host/uiSim.c`) at every class; states it doesn't reach
need a script render of their own, compared the same way.

A pull request that changes the look attaches the gallery PNGs it changed, before and after
([Contributing](contributing.md)).
