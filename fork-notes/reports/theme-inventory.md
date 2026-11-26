<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 The RadioFoundry Authors -->
<!-- Saved from the cloud session's scratchpad; paths under /tmp/claude-0 no longer exist. -->

# Theme framework inventory (core/ui, branch feat/gd88-gui)

Read-only analysis. Nothing was built or run; every statement comes from reading the sources. Line numbers are of the
working tree at 023527ad. "Reference" is today's look, "profile" is the look behind `CONFIG_UI_PROFILES`.

Counts: 84 `#if CONFIG_UI_PROFILES` blocks and one expression in 12 files of `core/ui` (ui_screens.c 41, ui_menus.c 14,
ui_app.c 10, ui_screens.h 6, gfx.c 3, gfx.h 3, ui_svc_storage.c 3, one each in ui_svc.h, ui_nav.h, ui_dual.c,
ui_svc_position.c, ui_svc_radio.c), plus about 77 run-time tests inside them. Outside `core/ui`: 6 in `core/position`,
the settings field, 3 build files, and the tests.

The look is selected by six different conditions today, which is the first thing the framework replaces:
`profile_theme()` (palette id, ui_screens.c:41), `s->gui_profiles` (ui_screens.c:1075), `ui_st.gui_profiles`
(ui_app.c, ui_menus.c), `l->roomy` and the other list flags (ui_screens.h:280-284), a local `inset != 0`
(ui_screens.c:1529, 4983), and `bottom != L->hint_y` (ui_screens.c:4409).

## 1. Drawing entry points

All in `core/ui/ui_screens.c` unless a file is named. "Chrome" means `ui_draw_top`, `ui_draw_bottom` / `ui_draw_hint`,
`ui_draw_banner`. Every function reads the globals `ui_col` (ui_screens.c:24) and `ui_layout` (:19).

### Chrome

| Function | Called by | Reads | Profile branch |
|---|---|---|---|
| `draw_top_as` :1072, `ui_draw_top` :1265 (status strip, and the title row or merged title) | every screen with a strip: ui_draw_home :3729, draw_dmr_call :2829, ui_draw_incoming_call :2569, ui_draw_emergency :5076, ui_draw_volume :5151, ui_draw_hotspot :5177, draw_quick_tiles :5453, list_top :4486, ui_draw_text_page :4617, ui_gps.c:473, ui_satellite.c:606, ui_menus.c:2314 (text entry), :5372 (trim) | `v->state/monitor/aprs_tx/rssi_dbm/roam/ch.mode`, `s->battery_pct/no_battery/low_batt_pct/unread/locked/gps/usb_net/scanning/dual`, `ui_strip_fill`, `gfx_bpp`, `UI_TALL_PX` | Yes: :1074 returns `draw_profile_top` :1053 |
| `draw_profile_status` :944 | draw_profile_top :1058, list_top :4479 | as above plus `v->power/vox_on`, `s->gps_fix/gps_clock/gps_utc_*/gui_night/hear_all`, `board.power_count` | profile only |
| `ui_draw_title` :1273 | draw_top_as :1202/:1260, draw_profile_top :1064, ui_draw_incoming_call :2576 | `ui_col.dim` | No |
| `draw_hint` :1469, `ui_draw_hint` :1609 (hint row, soft key bar, toast, two-line toast) | draw_bottom :1628/:1630, ui_draw_list :5062/:5064, draw_dmr_call :2949, ui_draw_emergency :5101, ui_draw_hotspot :5219, draw_quick_tiles :5508-5512, ui_menus.c:2351/:2353 | `L->hint_y/small`, `board.quick_label/context_label/keymap`, `list->roomy/numbered`, `soft_key` | Yes, 7 blocks :1483-1595 |
| `draw_bottom` :1625, `ui_draw_bottom` :4354 | Home variants, ui_draw_volume :5163, ui_draw_text_page :4658, ui_gps.c:486, ui_satellite.c:666, ui_menus.c:5434 | `s->toast/hints` | No (through draw_hint) |
| `draw_banner` :4382, `ui_draw_banner` :4452 | ui_draw_home :3722/:3740, profile_home_footer :3649, ui_draw_home_dual :4349, ui_draw_list :5060, draw_quick_tiles :5506, ui_draw_text_page :4657, ui_gps.c:487, ui_satellite.c:668 | `s->banner`, `s->now_ms`, pixels already drawn (`overlay_top` :1308) | Yes, 3 blocks :4408-4426 |
| `ui_draw_volume_bar` :5120 (overlay in the hint row or the strip) | ui_app_draw (ui_app.c:2872) | `s->volbar`, `L->hint_y/strip_h` | No (draws a plain full-width bar over the profile footer) |

### Lists (every screen without its own `draw`)

| Function | Called by | Reads | Profile branch |
|---|---|---|---|
| `ui_draw_list` :4842 | list_draw (ui_app.c:1940), draw_quick_list :5440 | `ui_list_t` (prefix, name, header, body, count, focus, `*top`, item, empty, hint, flags), `s->hints/toast/banner/gui_profiles`; writes `*l->top` | Yes, 11 blocks :4848-5035, plus unguarded uses of `two_columns/paginated/main_menu/inset` |
| `list_top` :4459 (strip, title or heading, Find header) | ui_draw_list, ui_list_rows, ui_list_body_fits | `l->prefix/name/header/roomy` | Yes :4462 |
| `list_pitch` :4504, `list_bottom` :4499, `list_body_end` :4738 | ui_draw_list, ui_list_rows, ui_list_body_fits | `L->row_h/hint_y`, `l->roomy` | Yes :4506 |
| `row_text` :4705 (label left, value right, alternates) | ui_draw_list :5039-5052 | font, width | No |
| the two-column grid | inside ui_draw_list: :4871-4884 (rows = 3, pitch, font), :4887 (page-based top), :4913-4955 (page badge, column widths), :4966-4970 (cells), :5028-5035 (digit) | `l->two_columns/main_menu/menu_icons`, `it.icon` | profile only |
| `draw_menu_icon` :4771, `profile_row` :4833 | ui_draw_list | `ui_col.priv/rx/warn/tx`, fixed gear colors | profile only |
| inline value editor | ui_draw_list :4981, :4990, :5043-5048 (`<value>` on the warning color) | `UI_ITEM_EDIT` | focus panel only (:4984) |
| `ui_list_body_fits` :4743 | msg_enter (ui_menus.c:962): decides whether the Message screen gets a "Full message" row | list_top, `L->small` | through `l.roomy` (ui_menus.c:960) |
| `ui_list_rows` :4758 | no caller anywhere (declared ui_screens.h:295) | | through list_pitch |
| `ui_draw_text_page` :4607 | msg_read_draw (ui_menus.c:1115), about_draw (:5657) | `v->state`, `L->tiny` | No |

Pick lists (`SCR_PICKER`, language :8276, display choice :8318) are plain list rows with the value "In use". Toggle
tiles do not exist yet.

### Home

| Function | Called by | Reads | Profile branch |
|---|---|---|---|
| `ui_draw_home` :3704 (dispatch) | ui_home_draw (ui_app.c:1310), gfxTest | `s->dual.on`, `s->gui_profiles`, `dmr_call_screen(v)`, `s->large_text` | Yes :3712, ahead of the DMR call screen |
| reference Home: `draw_place` :1834, `draw_talk` :1920, `draw_talk_dmr` :2503, `draw_chips` :2034 (`_dmr` :2652, `_t3` :2624), `draw_activity` :3027, `draw_last_heard` :2755, `draw_meter` :2134, `draw_home_large` :3228 | ui_draw_home | `L->zone_y/channel_y/talk_y/chips_y/sep_y/activity_*`, the whole view, `s->entry*/label_first/alias_show/dmr_off` | No |
| DMR call screen `draw_dmr_call` :2806, `call_line` :2795 | ui_draw_home :3721 | `v->dmr.call/alias/rssi`, home_soft_key | No (never reached in the profile look unless `profile_operational_home`) |
| `draw_profile_home` :3653 with `draw_home_status` :3632, `profile_home_devices` :3471, `profile_home_operating_marks` :3584, `profile_home_target` :3336, `profile_home_details` :3364, `profile_home_activity` :3406, `profile_home_footer` :3647 | ui_draw_home :3714 | `s->gui_profile` (0 Commercial, 1 Amateur), `s->large_text/entry/gui_night/gps_*`, view | profile only; layout tests at :3428, :3442, :3636, :3661 |
| `ui_draw_home_dual` :4302 with `dual_pane` :3887, `dual_large` :4109, `dual_activity` :4176, `dual_call` :4159, `dual_choose` :4274 | ui_draw_home :3708 | `s->dual`, view | Yes, 9 blocks :3878-4348 |
| `home_hint` :3173, `dual_hint` :4244, `home_soft_key` :1614 (text, not drawing) | the Home variants | view, `s->home_hint/home_soft_key_label` | No |

### Other screens with their own draw

None of these branch on the look. They draw their body with `gfx_*`, `ui_col` and `ui_layout` between the `y` that
`ui_draw_top` returns and `L->hint_y`, at full width from x = 0. That is the implicit contract a theme must keep.

| Function | Caller |
|---|---|
| `ui_draw_quick` :5515: `draw_quick_tiles` :5443 (3x3 grid, inline `<value>` editor :5486), `draw_quick_list` :5429 (under 150 px, through ui_draw_list) | ui_quick_draw (ui_app.c:1702) |
| `ui_draw_splash` :1718 | ui.c:154 |
| `ui_draw_incoming_call` :2554, `ui_draw_emergency` :5068, `ui_draw_volume` :5144, `ui_draw_diag` :5535 | ui_app.c:1768, :1783, :1734, :1709 |
| `ui_draw_hotspot` :5168 | draw_screen (ui_app.c:2882) |
| `text_draw` ui_menus.c:2283 (text entry), `trim_draw` :5364 (slider editor), `bt_draw` :5786 | screen table |
| `gps_draw` ui_gps.c:469 (`draw_status/sky/signal/grid`); `sys_color` :43 tests `ui_theme_get() == UI_THEME_NIGHT` | screen table |
| `satellite_draw` ui_satellite.c:577 | screen table |

### Palette, layout, primitives

| Function | Notes |
|---|---|
| `ui_theme_set` :133, `accent_apply` :79, `ui_accent_set` :220, `ui_strip_set` :226 | Palettes are code, not tables. `ui_col` is a 36-byte RAM struct that the accent rewrites. Called from ui_apply_setting (ui_app.c:2125-2146), gfxTest |
| `profile_palette` :46 | Day and Night, plus four colors outside `ui_colors_t` (:36-37) |
| `ui_layout_init` :271, `layout_regions` :232 | Fonts by display class, regions. Called from ui.c:149 and on a language change (ui_app.c:2152); the profile overrides `strip_h` and `hint_y` (:241) |
| gfx.c: `gfx_coverage_pixel` :296, `gfx_reserve_colors` :408, `gfx_text_title` :770 | Generic primitives, built only with the profile switch |

## 2. Every profile site

P presentation, B behavior, S choosing the look (settings, storage, build), T test or simulator, D shared data that is
only fetched for the profile look. **B!** marks behavior that differs between looks today.

### core/ui/ui_screens.c

| Lines | Class | What |
|---|---|---|
| 30-70 | P | Metrics `PROFILE_*`, `profile_bar_fg/row_bg/row_focus/home_gutter`, `profile_theme()`, `profile_palette()` |
| 136-138 | S | `ui_theme_set` accepts `UI_THEME_PROFILE_DAY/NIGHT` |
| 141-143, 147-156 | P | Loads the profile palette, re-runs `layout_regions` when the look changes |
| 241-246 | P | `strip_h` 29, `hint_y` at the footer |
| 475-1068 | P | Icon header, mask and bitmap blitters, `profile_pane`, status marks, clock, battery, power, key, `draw_profile_status`, `draw_profile_top` |
| 624-665, 789-794, 824-830, 838-844, 866-871, 902-905 | T | States only the simulator can set (`gui_sim_*`: recording, Bluetooth, D-STAR, talkaround, ARC4), inside `GFX_FIT_TRACE` |
| 1074-1077 | P | `draw_top_as` hands over to the profile strip |
| 1357-1394, 1562-1565 | **B!** | `secondary_hint_split`: with the profile footer the Quick group (or the last group) becomes the right-hand label. Shared with `CONFIG_KEY_ASSIGN` |
| 1405-1427, 1557-1560 | **B!** | `compact_menu_hint`: in profile lists "OK open" and "1-7 open" become "OK". It matches the English "OK " in a translated string (:1410) |
| 1483-1495, 1529-1533, 1542-1546, 1570-1577, 1590-1595 | P | Footer pane, font, color, margins |
| 3273-3702 | P | Profile Home. Content differences are listed below |
| 3712-3717 | P, **B!** | Profile Home is chosen before the DMR call screen |
| 3878-3883 | P | Dual watch inset 12 |
| 3897-3899, 4025-4037 | P | Commercial dual pane: target label or zone, caller only as the talker alias |
| 4186-4191 | P | Dual: refused transmission centered |
| 4234-4237 | P | Dual: no call history lines |
| 4306-4318, 4343-4348 | P | Dual chrome: gutter, status, pane, footer |
| 4325-4327 | P | Commercial: `dual_choose` skips fits with a detail row |
| 4408-4411, 4412-4415, 4423-4426 | P | Banner: overlay inset, x0 past the dual tags, background color |
| 4462-4485 | P | List heading: large, left-aligned, under the profile strip |
| 4506-4509 | P | Row pitch + 10 |
| 4767-4840 | P | `draw_menu_icon`, `profile_row` |
| 4848-4857 | P | Reserves palette slots for row and icon colors |
| 4874-4884, 4887-4888 | P | Grid: 3 rows, 2 columns, page-based `top` (:4887 is unguarded) |
| 4903-4909 | P | Inset 7, gap, disabled rows in `dim` instead of `line` |
| 4913-4955 | P | "1/2" page badge with arrows, column widths from the visible page's labels |
| 4962-4964 | P | `it.icon` initialized |
| 4982-4988, 4992-4995 | P | Row panels, focus bar |
| 4998-5005, 5028-5035 | P | Icon, grid digit |

### core/ui/ui_app.c

| Lines | Class | What |
|---|---|---|
| 1955-1963 | P set by B | `list_draw` sets `roomy/menu_icons/two_columns/main_menu` from `gui_profiles` and the screen id; `numbered` from `SF_NUMBERED`. :1961 gives the hub the title "Menu" in the profile look only |
| 1999-2003 | S | `gui_profiles` = color, `UI_FEAT_GUI_PROFILES`, bpp > 1, width >= 310, height == 240 |
| 2027-2029 | S | Settings applied at start |
| 2121-2139 | S | Theme, Highlight color and Filled status bar are ignored while the profile look is on |
| 2141-2148 | S | Layout, Day/Night, GPS clock applied |
| 2347-2350 | **B!** | The "Talkgroup name first" side key does nothing in the profile look |
| 2561-2590, 2602-2604 | P, D, T | `profile_tick`: redraw cadence for the scan and roaming animations (P), GPS fix and clock fetch (D), simulated recording (T, :2573) |

### core/ui/ui_menus.c

| Lines | Class | What |
|---|---|---|
| 106-116 | P, **B!** | Hub icon ids (P data). :112-115 drop the row values in the profile look: unread count with its warning flag, zone name, "Scanning" |
| 959-961 | P feeding B | `l.roomy` changes `ui_list_body_fits`, so the Message screen's row set depends on the look |
| 6600-6602, 7549-7551, 7751-7755 | **B!** | `ST_CHOICE`: OK opens a pick list. `ST_LIST` rows cycle in place |
| 6634-6636, 7130-7134, 7271-7273 | P | Settings group icon ids |
| 6658-6666, 6925-6929 | S | Display Profile, Day/Night, GPS clock rows (GPS clock is a feature, not a look) |
| 7179-7185 | S, **B!** | Rows hidden by look: the three choice rows without it; Theme, Highlight color, Filled status bar and Talkgroup name first with it. Row count and digit paths in Settings > Display differ |
| 7511-7514 | **B!** | Key assignment skips "Talkgroup name first" in the profile look |
| 8308-8336, 8414-8416 | B | `SCR_DISPLAY_CHOICE` pick screen |

### Other core/ui files, core/, boards/

| Site | Class | What |
|---|---|---|
| ui_screens.h:62-64, ui_dual.c:247-252 | D | `ui_dual_pane_t.target_label` |
| ui_screens.h:142-149 | S, D | `gui_profiles`, `gui_profile`, `gui_night` (S); `gps_clock`, `gps_fix`, `gps_utc_*` (D) |
| ui_screens.h:150-154 | T | `gui_sim_*` |
| ui_screens.h:213-215 | S | Palette ids past `UI_THEME_COUNT` |
| ui_screens.h:247-255, 262-264 | P | `ui_menu_icon_t`, `ui_item_t.icon` |
| ui_screens.h:279-285 | P, B | List flags; `numbered` is behavior information |
| ui_nav.h:52-54 | B | `SCR_DISPLAY_CHOICE` |
| gfx.h:47-50, 72-75, 104-107; gfx.c:296-312, 408-419, 770 on | P | The three primitives |
| ui_svc.h:57, ui_svc_radio.c:220 | S | `UI_FEAT_GUI_PROFILES` (bit 24 of the feature word) |
| ui_svc.h:238-241, ui_svc_position.c:31-34 | D | `utc_ymd`, `utc_clock_hms` |
| ui_svc.h:467 | S | `UI_SET_GUI_PROFILE/NIGHT/GPS_CLOCK`, not guarded |
| ui_svc_storage.c:82-91, 329-334, 507-518 | S | `gui_flags` get and set |
| core/settings/settings.h:256, 263-265 | S | `gui_flags` in the first byte of the old `reserved11`, not guarded |
| core/position/position.c:33, 344, 399, 457, 486; position.h:104 | D | The GPS date and clock, guarded by the look's switch |
| config/features.mk:210, :358; core/config.h:239; boards/gd88/config.mk:20 | S | The switch. No board lists it as unsupported, so every maximal build turns it on |

### Tests and simulator

| Site | Class | What |
|---|---|---|
| tests/host/Makefile:322, :759 | T | radioCoreTest and uiSim build with the switch on; uiFlowTest builds with it off (:740) |
| tests/host/sim/ui_sim.h:24, ui_sim.c:327-330 | T | Geometry `320x240iR10` is the only one with `gui_profiles`; it is not in `sim_geoms[]`, so the gallery and `uiSim --layout` never draw the profile look |
| ui_sim.c:1801, 1827 | T | Masks `UI_FEAT_GUI_PROFILES` |
| ui_sim.c:2327, 2344-2348, 2543 | T | GPS position, clock, satellites differ on that geometry |
| ui_sim.c:2706-2735, 2810-2811, 2859-2862 | T | On that geometry the sim also adds the soft key, a GPS, P25 and NXDN: "profile look" and "a board like the GD-88" are one fixture |
| ui_sim.c:3203-3222 | T | `gui:original\|prototype` switches the look at run time (goes Home first because the Display rows differ) |
| ui_sim.c:3223-3252, 3253, 3268 | T | `prototype:*`, `vox:`, `rxonly:`, `roam:` tokens |
| tests/host/uiPrototypeTest.py | T | Checks by pixel coordinates (:40, :66-70, :135-148, :233-238) and `expect:layout=0`; no stored baseline |
| tests/host/radioCoreTest.c:4535, 4541, 4609 | T | GPS clock |
| radioCoreTest.c:6538-6561, 12034 | T | `gui_flags` round trip |

### Behavior and content that differs between looks today

1. The "Talkgroup name first" key, its key assignment value and its setting are disabled (ui_app.c:2347, ui_menus.c:7511, :7182).
2. Settings > Display has different rows, so different counts and digit paths (ui_menus.c:7179-7185).
3. Two-value settings open a pick list in one look and would cycle in the other (`ST_CHOICE`).
4. Hint wording and which group is the right-hand label (ui_screens.c:1557-1565).
5. Hub rows lose their values, including the unread count; the hub gains a title (ui_menus.c:112, ui_app.c:1961).
6. Receiving a DMR call: the reference shows the call screen (caller from the contacts, destination, alias, signal
   meter, the hint "PTT replies"). The profile Home shows the channel's talk target and the caller only as a talker
   alias when "Show talker alias" is on (ui_screens.c:3305, :3425), with the idle hint.
7. Dual watch: no call history, Commercial shows no contact name for the caller (ui_screens.c:4025-4031, :4234).
8. Large text does nothing in Commercial and only hides the details row in Amateur (ui_screens.c:3662).
9. Status strip content: the profile strip has no USB network indicator, no battery percent, no unread count digit,
   and shows the GPS only with a fix (ui_screens.c:857-893 against :1142-1254).
10. The Message screen's "Full message" row depends on row pitch (ui_menus.c:959).

Not a difference: focus movement. `list_move` (ui_app.c:1845) is linear in both looks; Left and Right are not list
keys. The grid only changes how `top` is derived when drawn.

## 3. Proposed descriptor

Principles: the shared code owns the screen skeletons, scrolling, text content and counters; a theme owns geometry
and pixels. Shared helpers take explicit parameters, and a theme's op is a thin caller. Everything is `const`; the only
RAM is the current selection.

```c
// core/ui/ui_theme.h
typedef struct {
	tr_id_t name;			// "Dark", "Day"
	ui_colors_t c;			// the shared tokens, before the highlight color
	uint8_t flags;			// UI_PAL_ONE_HUE: no hue cues (ui_gps.c:43 tests this instead of an id)
} ui_palette_t;

typedef struct {			// where the shared list skeleton puts cells
	const font_t *font;
	int16_t x, y, pitch, col_w[3], col_gap;
	uint8_t rows, cols;
	uint8_t cell;			// UI_CELL_ROW, _TILE, _TOGGLE, _PICK
	bool paged, col_major;		// top moves a page at a time; fill columns first
} ui_list_geom_t;

typedef struct { int16_t x, y, w, h; uint16_t index; bool focus; } ui_cell_t;

typedef struct {
	const char *text;		// hint with alternates, or the toast
	const char *soft_right;		// Home's assigned soft key label, or NULL
	bool toast, numbered;
} ui_bar_t;

typedef struct ui_theme {
	uint8_t id;			// stored in settings: never renumbered
	tr_id_t name;
	const ui_palette_t *palettes;	uint8_t palette_count;
	const tr_id_t *layouts;		uint8_t layout_count;	// Home layouts ("Commercial", "Amateur"); 0: one
	const struct ui_icon_set *icons;	// NULL: text and line icons only
	const struct ui_theme_ops *ops;	// a NULL entry falls back to the reference op
	bool (*fits)(int w, int h, int bpp);
} ui_theme_t;
```

| Op | Signature | Replaces | Reference implementation |
|---|---|---|---|
| `layout` | `void (*)(ui_layout_t *L)` | `ui_layout_init` :271, `layout_regions` :232, the override at :241 | the two functions, unchanged |
| `colors` | `void (*)(ui_colors_t *out, uint8_t palette)`; NULL copies the table | `ui_theme_set` :133, `profile_palette` :46 | table copy plus `accent_apply` :79 (highlight color and filled strip are reference options) |
| `top` | `int (*)(const radio_view_t *v, radio_state_t st, const ui_state_t *s, const char *prefix, const char *name)` | the branch at :1074, `draw_profile_top` | body of `draw_top_as` :1078-1262 |
| `list_head` | `int (*)(const radio_view_t *v, const ui_state_t *s, const ui_list_t *l, bool draw)` | `list_top` :4462-4487 (measure and draw) | `ui_draw_top` and the measure formula :4486 |
| `list_geom` | `void (*)(const ui_list_t *l, int y, int y_end, ui_list_geom_t *g)` | `list_pitch` :4504, :4867-4884, :4901-4912, the `roomy/two_columns/main_menu` flags | one column, `pitch = L->row_h` |
| `cell` | `void (*)(const ui_list_t *l, const ui_list_geom_t *g, const ui_cell_t *c, const ui_item_t *it)` | the loop body :4966-5053: panel, focus, icon, digit, label and value, inline editor | that body with `row_text` :4705 |
| `scroll` | `void (*)(const ui_list_geom_t *g, uint16_t top, uint16_t count)` | scroll marks :5056-5059, page badge :4914-4931 | the two 3 x 2 marks |
| `bottom` | `void (*)(const ui_state_t *s, const ui_bar_t *b)` | `draw_hint` :1469 | `draw_hint` without its 7 profile blocks |
| `banner` | `void (*)(const ui_state_t *s)` | `ui_draw_banner` :4452 and the 3 blocks | shared `ui_banner_box(s, bottom, x0, right_inset, bg)` (today's `draw_banner`) at `L->hint_y`, `hint_bg` |
| `home` | `void (*)(const radio_view_t *v, const ui_state_t *s)` | `ui_draw_home` :3712-3742 | call screen, large text, place / talk / chips / activity |
| `home_dual` | same | `ui_draw_home_dual` :4302 | shared `ui_dual_body(v, s, const ui_dual_style_t *)` with inset 0, details and history on |
| `quick` | same | `ui_draw_quick` :5515 | `draw_quick_tiles`, `draw_quick_list` |
| `animate` | `bool (*)(const radio_view_t *v, const ui_state_t *s, uint32_t prev_ms, uint32_t now_ms)`; NULL: never | the cadence part of `profile_tick` (ui_app.c:2565-2577) | NULL |

Thirteen pointers, 52 bytes of flash a theme. The other screens (text entry, trim, GPS, satellite, volume, emergency,
hotspot, incoming call, diagnostics, text page, splash) need no op: they use `top`, `bottom`, `banner`, the palette
and the layout.

What the shared code keeps:

- `ui_draw_list` becomes the skeleton: clear, `list_head`, Find header, body, `list_geom`, the `top` rule (scroll or
  page, from `g.paged`), the item loop calling `cell`, `scroll`, banner, bottom. It alone writes `*l->top`.
- The list's kind is semantic and set from the screen definition, never from the look: `ui_list_t.kind` =
  `UI_LIST_PLAIN`, `_MENU` (hub), `_GROUPS` (Settings), `_PICK`, `_TOGGLES`. A `kind` byte fits in the padding after
  `ui_scr_def_t.flags` (ui_nav.h). The theme maps a kind to a geometry and a cell kind. The five flags at
  ui_screens.h:280-284 go; `numbered` moves to `ui_bar_t`.
- Hint text: one shared `ui_hint_fit(bar, font, room, left, right)` holds alternates, `key_names`, `hint_split` and
  the soft key rule. Whether the bar is hint groups or two soft key labels is one policy flag the theme declares, not
  string surgery in the drawer.
- `ui_strip_draws` and `ui_hint_draws` are counted in the shared dispatchers, so a theme cannot break the crawler's
  strip rule or the volume bar's placement (ui_app.c:2867-2875).
- Item content is the same for every theme: the hub always fills its values, and the cell decides what to draw.

Existing structs that become theme data:

| Today | Becomes |
|---|---|
| `ui_colors_t` filled by code | One `const ui_palette_t` per palette (dark, light, high contrast, night; day, night; mono). `ui_col` stays the 36-byte resolved copy in RAM, so no pointer chase and no RAM change. Light, high contrast and night tables stay under `CONFIG_UI_COLOR` |
| `profile_row_bg/row_focus/home_gutter/bar_fg`, `profile_ink`, the grays at :806 and :4768 | A `const` color struct private to the second theme, indexed by palette. Not added to `ui_colors_t`: that would cost RAM on every radio |
| `ui_layout_t` | Unchanged in step 1 (`ops->layout` fills it). It is the shared metrics struct: four fonts, `strip_h`, `hint_y`, `row_h`. The Home regions (`zone_y` to `activity_h`) are the reference Home's and can move into the reference theme later |
| `PROFILE_*` constants, pitch + 10, inset 7 | Private `const` metrics of the second theme, visible to shared code only through `ui_list_geom_t` |
| `ui_menu_icon_t`, ui_profile_icons.h, the 13-row marks at :4791 | `ui_icon_set_t`: a `const` table from icon id to a coverage mask and size, drawn by one shared blitter (`profile_mask` :485 moved to gfx). The id in `ui_item_t` stays behind a build capability (`UI_ICONS`, derived from the theme switches) so it costs a reference-only build nothing |
| `ui_theme`, `gui_profiles`, `gui_profile`, `gui_night` | One selection: `const ui_theme_t *` plus palette and layout index, 6 bytes, against 12 today in a profile build |

Cost in a reference-only build: when one theme is built, `ui_theme_cur()` is a macro for `&ui_theme_reference`; with
`-flto=auto` (Makefile:334) the calls resolve at build time and no pointer is stored. This must be measured, not
assumed. No theme owns a buffer.

## 4. Refactor plan

Each commit ends with: `make -C tests/host run-ui`, `make -C tests/host ui-gallery` with `git status docs/ux/screens`
clean, the profile baseline of step 0 identical, `make BOARD=gd88`, `make BOARD=gd88 CONFIG_UI_PROFILES=n BUILD=build/t-x`,
one 160 px and the mono board, `stack-check` on each, and the size of `.text/.data/.bss` against the previous commit.

0. **Baseline (tests only).** The profile look has no pixel reference. Render the `gallery[]` scripts of uiSim.c and
   the uiPrototypeTest scripts at `320x240iR10` in Commercial and Amateur, Day and Night, and store their hashes.
   Record GD-88 flash and RAM, the size of a board with little flash left, and the UI task's stack margin.
1. **Reference look behind the descriptor, no pixel change in either look.**
   1. Palettes as `const` tables; `ui_theme_set` copies and applies the accent. gfxTest:473-531 checks the values.
   2. Add `ui_theme.h`, `ui_theme_reference` and the dispatchers; `top`, `bottom`, `banner`, `home`, `home_dual`,
      `quick`, `layout`, `colors` point at the existing functions. The profile `#if` blocks stay inside them for now.
   3. Split `ui_draw_list` into the skeleton and `list_head`, `list_geom`, `cell`, `scroll`. Before this, land the
      approved "one fixed column split for the whole list" (gui-plan, Layout) in the old code, so `list_geom` does
      not need `top`; that one commit changes profile pixels and renews the baseline.
   4. Extract the helpers with explicit parameters: `ui_hint_fit`, `ui_banner_box`, `ui_dual_body`.
2. **Selection.** Replace the six conditions by the current theme, palette and layout. `ui_app_init` picks the theme
   with `fits()`. The simulator selects a theme directly; `UI_FEAT_GUI_PROFILES` goes.
3. **Second theme, one op per commit**, into `core/ui/themes/<name>/` behind `CONFIG_UI_THEME_<NAME>`: palettes and
   private colors, `layout`, `top`, `bottom`, `banner`, list ops, `home`, `home_dual`, `animate`. Each commit moves the
   profile side out of the reference function and deletes the branch. The baseline stays identical.
4. **Behavior made the same in both looks** (each is a visible change, one commit each, tests with it): the
   talkgroup-name-first key and setting, hub values and title, hint policy, the Display rows, pick lists for every
   multi-value setting, the call information on Home. The GPS clock data moves from the look's switch to its own.
5. **Remove** `CONFIG_UI_PROFILES`, the `gui_sim_*` fields from `ui_state_t` (to a simulator hook), `ui_list_rows`;
   list the theme switch in the five places a new switch needs.
6. **Theme tests** (the discussion's PR 3): gallery folder per theme, `uiSim --layout` per theme, uiFlowTest per
   theme.

Upstream has none of the profile code, so its PR 1 is the diff of steps 1 and 2 without the theme directory, squashed
onto a branch from `upstream/main` and proved there with the gallery.

Risky spots:

- **UI task stack.** `draw_hint` is on the deepest path with three 64-byte buffers (:1479, :1500, :1551), and
  the comment at :5118 gives 8 bytes to spare on the tightest board (ui_app.c:1863 guards the same chain). An op call and a split helper add
  frames. `tools/stack_check.py` takes every function stored in an ops field, so each theme's worst case counts on a
  board that builds it. Keep the `noinline` and `always_inline` attributes.
- **`ui_draw_list`** is 220 lines with the looks interleaved, a mono path (:5016) and the two-line tile label
  (:5021). It is the commit most likely to move a pixel.
- **`overlay_top`** reads back pixels to place banners and two-line toasts (:1308), so the result depends on what the
  theme drew underneath and on the x0 and inset it passes.
- **16-color frame buffer.** A theme with panels plus anti-aliased text can run out of the 16 entries;
  `gfx_reserve_colors` exists only for that (:4853). ui.md 5 states a 9-color limit for the reference look.
- **Tests bound to the current API.** gfxTest reads `ui_col`, `ui_layout` and calls `ui_theme_set(2)`,
  `ui_accent_set`, `ui_strip_set` (gfxTest.c:242, 400-540): keep them as wrappers. `uiSim --layout` reads
  `ui_layout.row_h` and `strip_h` for the row pitch rule of ui.md 7 (uiSim.c:361), which does not see a theme's own
  pitch. uiPrototypeTest's pixel coordinates break on any intended layout change.
- **Press counts.** uiFlowTest builds without the switch and at four geometries without the look, so steps 1 to 3
  cannot change it; step 4 can (Display rows, pick lists), and the Message screen's row set depends on the theme's
  row pitch.
- **Flash with the theme off.** `ui_item_t.icon`, the group icons, `ST_CHOICE` and the pick screen are guarded
  today; after step 4 the pick list is shared code and costs every board. `uiFlowTestNoColor` (`CONFIG_UI_COLOR=0`)
  must still drop the extra palettes.
- **Maximal builds.** `ci.sh` turns every switch on for every board. A board whose panel the theme cannot fit needs
  the theme in `BOARD_UNSUPPORTED` with that reason, or its maximal build pays for a theme `fits()` never selects.
- **Settings.** tests/host/settingsTest.c:931 still expects `reserved11` at offset 406, where `gui_flags` now is, and
  settings-store.md:177 and :179 disagree. Any byte used for the theme id has to fix the test and the table together.
- **Simulator fixture.** The profile geometry also changes keys, GPS, P25 and NXDN (ui_sim.c:2728-2735, 2810).
  Separate the board fixture from the theme before comparing looks on the same script.

## 5. Open questions

1. **Stored selection.** Theme id in the remaining `reserved11` byte, with 0 meaning the build's default theme, so
   old records and upstream radios keep what they have? Should Day/Night be one setting shared by all themes or a
   palette index per theme?
2. **Settings rows.** The plan says no item is hidden by theme. Are Highlight color and Filled status bar shown
   dimmed with a reason in a theme without them, or may a theme option row be absent?
3. **Home content.** Is "the call screen during a DMR call" behavior (every theme must show caller, destination and
   signal) or presentation (a Home layout may reduce it to the alias)? The same for dual watch history and Large text.
4. **Status strip parity.** Must every theme show the same set of states (USB network, battery percent, unread
   count, GPS searching)? A written list of required indications would make it testable.
5. **Hint row or soft key bar.** May a theme show two labels where the reference shows hint groups, with both built
   from the same hint text?
6. **Grid navigation.** Should Left and Right move between columns? That is behavior, so it would apply to every
   theme and need the column count from `list_geom`.
7. **Granularity.** Per-cell list ops as proposed, or one op for the whole list body (simpler, but each theme then
   carries its own scrolling)? Should Quick become a list kind drawn through `cell`?
8. **Inheritance.** NULL falls back to the reference op. May a theme also call a reference function for cases it has
   not designed (the profile Home does this for trunking and SFR at :3687-3701)?
9. **Icons.** A data table with one shared blitter, or drawing left to each theme until the 24 px family exists?
10. **Names.** The second theme's name and switch; whether today's "Theme" row and `UI_THEME_*` are renamed to
    palette (the discussion's question 3).
11. **Where the reference code lives.** Stay in ui_screens.c for a small upstream diff, or move to
    `core/ui/themes/reference/` in a later mechanical commit?
12. **GPS clock.** Its own switch, and does the reference strip show it?
