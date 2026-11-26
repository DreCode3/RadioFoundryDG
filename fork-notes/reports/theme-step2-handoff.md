<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->
<!-- Saved from the cloud session. Tool paths it names under /tmp/claude-0 are now fork-notes/tools/ (cc.sh, snapshot.py, compare.py, scratch.sh); the pixel baseline is rendered locally (fork-notes/local-setup.md). -->

# Theme step 2: hand-off

Stopped as asked. Tree clean at my last commit, nothing uncommitted, nothing pushed by me, `build/t-*` removed.

## Commit made

| Hash | Subject |
|---|---|
| `d9dfd89b` | tests: uiThemeTest, a NULL top measured through the reference strip, a list's strip state |

Done and verified: the two carry-over `uiThemeTest` cases. Each fails on the mutation QA named (fallback passing
`true`: ASan SEGV; `RS_IDLE` for a list's strip: 2 FAIL). Per-commit check passed: uiSim 0 warnings, pixels
2208/0/0 and layout rc 0, uiThemeTest, uiPrototypeTest, gd88 build 0 warnings (FLASH 788416, RAM 205800, UI stack
1664 / 204).

Nothing else of step 2 is written. The rest below is the plan I had worked out from reading the code.

## Next commit, then the rest in order

1. **Rename the switch** `CONFIG_UI_PROFILES` to `CONFIG_UI_THEME_PANELS` everywhere (mechanical).
2. **Move the profile look's own code** (icons, marks, panes, strip, Home, list heading, about 1100 lines) to
   `core/ui/themes/panels/`, no logic change. The reference ops keep their branches and call into it through a
   theme-private header. Export the shared helpers it needs from `ui_screens.c` (`mode_word`, `rssi_bars`,
   `fit_end`, `home_hint`, `home_soft_key`, `draw_bottom`, `banner_box`, `key_names`, `hint_split`, `wrap_line`,
   `row_text`, ...) with a `ui_` prefix.
3. **Descriptor and selection**: `ui_theme_panels` with palettes [Day, Night]; `UI_THEME_SELECT` derived from the
   switch; a registry and a pick by `fits()` (color, width >= 310, height 240) in `ui_app_init`, after the splash.
   `ui_theme_set` loses the palette ids 4 and 5. Keep `ui_st.gui_profiles` as "panels in use" until the ops are out.
4. **Stored theme id** in `reserved11` (offset 407), 0 = the build's default; fix `settingsTest.c` (`gui_flags` at
   406) and `settings-store.md` together. The simulator sets it; `UI_FEAT_GUI_PROFILES` goes.
5. **One op per commit**, deleting the branch in the reference op each time: layout, top, banner, list ops (with
   `ui_list_t.kind` replacing the four look flags), bottom, home, home_dual, animate (from `profile_tick`).
6. **Appearance options** on the descriptor (which Display settings a theme honors) replace `ui_st.gui_profiles` in
   `ui_app.c` and `ui_menus.c`; then remove the field.
7. Width rule in `config/features.mk`, the five switch places, docs, two-theme `stack-check -v`, end-of-step checks.

## Open questions for the user

Answered on Nov 26: the name is "panels", and the theme id goes in its own spare settings byte (`reserved11`), with `gui_flags` unchanged.

- **Theme name.** I planned "panels" (rows and Home regions are rounded panels): neutral, says what it looks like.
- **Settings byte.** `reserved11` for the theme id, leaving `gui_flags` as it is, or fold it into `gui_flags`?
- **Behavior differences that must stay until step 4**: is one transitional test in shared code acceptable, or
  should each be a declared theme option? I planned options for the Display settings and the talkgroup-name-first
  key, and to move "hub tiles show no values, hub titled Menu" into the panels list ops.

## Gotchas

- **Simulator default.** The gallery's plain `320x240` class also passes `fits()`. Today only `320x240iR10` sets the
  profile feature bit. The simulator must store the reference theme for every other geometry, or the reference
  gallery changes.
- **Trunked and SFR Home.** The profile look falls back to the reference Home body for these, drawn with the profile
  strip, footer and regions. The panels `home` op must call the reference op, and its layout op must redo the Home
  regions after setting `strip_h` and `hint_y`.
- **Interleaved code.** `reference_bottom`, `reference_cell`, `reference_list_geom`, `reference_scroll` and the dual
  watch functions hold profile branches inline. Cell and bottom need a copy in the theme; dual watch needs a style
  parameter (inset 12, target label, no history, centered deny text), not a copy.
- **Palette order on 16 colors.** Any reordering of drawing changes pixels; the badge-before-cells rule is why
  `scroll` is called twice.
- **gd88 has no LTO.** Moving code across files changes inlining there; check `stack-check` on gd88 after each move.
  A measuring op must return before its buffers.
- **Stack tool.** It reads calls of the form `o->op(...)` through a typed local; a `?:` callee or a nested macro is
  unresolved.
- **GPS clock data** in `core/position` is guarded by the look's switch; it follows the rename.
- Session tools: `tools/cc.sh` (per-commit check), `snapshot.py`, `compare.py`, `mutate*.py`, `scratch.sh` (two-theme
  proof), all under the scratchpad; `base/` is the pixel reference and must not be overwritten.
