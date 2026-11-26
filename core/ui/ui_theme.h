// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// A theme: how the screens look. The shared code owns what a screen holds and how it behaves (the menu tree, the
// keys, scrolling, the text and its translations) and calls the theme in use for geometry and pixels. A theme is a
// const descriptor: its palettes and its drawing ops. It owns no RAM; the colors and metrics in use stay in ui_col and
// ui_layout (ui_screens.h). What Settings > Display calls a theme (UI_THEME_*) is one palette of it
#ifndef UI_THEME_H
#define UI_THEME_H

#include "ui_screens.h"

// 1: the build has more than one theme and the one in use is chosen at run time (ui_theme_use). 0: the reference
// theme only. Set by the theme switches (CONFIG_UI_THEME_PANELS); the host test of the framework sets it for its own
// themes (tests/host/uiThemeTest.c)
#ifndef UI_THEME_SELECT
#define UI_THEME_SELECT	CONFIG_UI_THEME_PANELS
#endif

// A theme's number, as the settings store it: fixed, whichever themes a build has. 0 is the build's default theme
enum { UI_THEME_ID_DEFAULT, UI_THEME_ID_REFERENCE, UI_THEME_ID_PANELS };

// The bottom row's text (ops->bottom). The two list flags carry ui_list_t.roomy and .numbered, fields that exist only
// with CONFIG_UI_THEME_PANELS, to the one piece of code that reads them, which is built only with it. So they follow
// the switch too: without it nothing would set or test them
#define UI_BAR_TOAST	0x01u	// a toast, not key hints (the value of ui_draw_hint's toast)
#if CONFIG_UI_THEME_PANELS
#define UI_BAR_LIST	0x02u	// under a roomy list (ui_list_t.roomy)
#define UI_BAR_NUMBERED	0x04u	// ... whose rows the digit keys pick (ui_list_t.numbered)
#endif

// Where a list's cells go (ops->list_geom). The shared list code (ui_draw_list) picks the cells on screen from it and
// moves the list; the first column is filled first. The op sets every field. rows is at least 1 and cols is 1 to
// UI_LIST_COLS: ui_draw_list brings anything else into that range, so a theme's mistake shows as a wrong layout
#define UI_LIST_COLS	2
typedef struct {
	const font_t *font;	// the rows' text
	int16_t x, y;		// the first cell's corner
	int16_t pitch;		// from one row to the next: a cell's height
	int16_t col_w[UI_LIST_COLS];	// each column's width
	int16_t col_gap;	// between columns
	uint8_t rows, cols;	// cells on screen: rows * cols
	bool paged;		// the list moves a page at a time, not a row
} ui_list_geom_t;

// One cell of a list (ops->cell): its box, and whether it has the focus
typedef struct {
	int16_t x, y, w, h;
	bool focus;
} ui_cell_t;

// An op left NULL is the reference theme's, so a theme gives only what it draws differently. The shared entry points
// in ui_screens.c (ui_draw_top, ui_draw_hint, ui_draw_home, ui_draw_list, ...) make the choice; nothing else calls an op
typedef struct ui_theme_ops {
	// Fonts for the display and the regions, into *L
	void (*layout)(ui_layout_t *L);
	// The theme's appearance options over the palette just copied into *c (the reference theme: the highlight color).
	// NULL: the palette as it is, not the reference theme's op
	void (*colors)(ui_colors_t *c, uint8_t palette);
	// The status strip, and the title when there is one. st: the state its pill shows. Returns the y where the
	// screen's content starts. draw false: only measure, for the same title (v and s are NULL, st means nothing). It
	// must return the y it returns when it draws: a list is measured before a screen opens (ui_list_body_fits) and
	// drawn later, and the two must agree on where its text starts
	int (*top)(const radio_view_t *v, radio_state_t st, const ui_state_t *s, const char *prefix, const char *name,
		bool draw);
	// The bottom row: key hints (groups separated by two spaces, with text alternates) or a toast. soft_key: the label
	// of Home's assigned soft key, NULL without one. Plain arguments, no struct: ui_draw_hint has only the text and
	// the flag, and a struct would be built in the frame of every screen that draws a hint
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
	// The strip and the title; returns the y below them. draw false: only measure (v and s are NULL), with the same
	// result. The reference theme's is the top op both ways, so a theme with only a strip of its own needs none.
	// With draw true it is the first thing drawn after the screen is cleared: a theme whose rows or icons need
	// palette entries of their own on a 16-color display takes them here (gfx_reserve_colors), before the strip's
	// shades use them up
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
	uint8_t palette_count;		// at least 1: a palette number out of range is the first
	uint8_t id;			// UI_THEME_ID_*
	const ui_theme_ops_t *ops;
	// Whether the theme can draw on this display (after gfx_init). NULL: on every display
	bool (*fits)(void);
} ui_theme_t;

// Today's look (ui_screens.c)
extern const ui_theme_t ui_theme_reference;
#if CONFIG_UI_THEME_PANELS
// Rounded panels, Day and Night, the Commercial and Amateur Home layouts (themes/panels/panels.c). Its palettes:
extern const ui_theme_t ui_theme_panels;
enum { UI_PANELS_DAY, UI_PANELS_NIGHT };
#endif

// Pieces of ui_screens.c that a theme of its own draws with. External only in a build with such a theme: with the
// reference theme alone they stay static there, so the compiler keeps inlining them where no LTO runs
#if CONFIG_UI_THEME_PANELS
#define UI_THEME_SHARED
void ui_layout_fonts(ui_layout_t *L);			// the reference theme's fonts by display class
// Home's regions and the list rows under a strip strip_h high, above the bottom row at hint_y
void ui_layout_regions(ui_layout_t *L, int strip_h, int hint_y);
uint8_t ui_rssi_bars(int16_t dbm);			// signal bars, 0-4
const char *ui_mode_word(const radio_view_t *v);	// the mode word: AM, a channel kind's short name, the mode
#if CONFIG_DUAL_WATCH
const char *ui_dual_mode_word(const ui_dual_pane_t *p);	// ... of a dual watch pane
int ui_dual_tag_w(const font_t *nf);			// the width of a dual watch pane's letter tag
#endif
bool ui_sending_data(const radio_view_t *v);		// on the air without PTT: a data burst
void ui_hwopt_needed(sbuf_t *b);			// what a pending hardware option needs from a person
void ui_home_entry_text(const ui_state_t *s, char *buf, size_t len);	// the frequency being typed on Home
const char *ui_home_hint(const radio_view_t *v, const ui_state_t *s);	// Home's key hints
const char *ui_home_soft_key(const ui_state_t *s);	// the label of Home's assigned soft key, NULL without one
// The bottom row: a toast if one is up, else the hint (with the soft key's label) when hints are on
void ui_draw_bottom_key(const ui_state_t *s, const char *hint, const char *soft_key);
// Text shortened at its end to max_w
void ui_fit_end(const font_t *f, const char *s, int max_w, char *out, size_t len);
// The banner, if one is up, its lower edge at bottom (ui_screens.c has the details)
void ui_banner_box(const ui_state_t *s, int bottom, int x0, int right_inset, color_t bg);
#else
#define UI_THEME_SHARED	static
#endif

// The theme in use. With one theme built it is a constant: the compiler resolves every op call, and neither the
// descriptor nor a pointer to it is in the image
#if UI_THEME_SELECT
extern const ui_theme_t *ui_theme_selected;
#define ui_theme_cur()	ui_theme_selected
// Puts a theme in use, with its layout and the palette in use (its first where it has fewer). After gfx_init.
// false: the theme has no palette, and the one in use stays
bool ui_theme_use(const ui_theme_t *t);
// The theme with number id (UI_THEME_ID_*) where this build has it and it fits the display, else the build's default
// theme where that fits, else the reference theme. After gfx_init
const ui_theme_t *ui_theme_pick(uint8_t id);
#else
#define ui_theme_cur()	(&ui_theme_reference)
#endif

#endif
