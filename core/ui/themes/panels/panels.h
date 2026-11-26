// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// The panels theme's own pieces (CONFIG_UI_THEME_PANELS): its palettes, status strip, Home, list heading, rows and
// icons. Private to the theme: only panels.c and the reference ops in ui_screens.c that still hold the panels look's
// branches include it, until those branches are the theme's own ops (docs/spec/ui.md)
#ifndef UI_THEMES_PANELS_H
#define UI_THEMES_PANELS_H

#include "ui/ui_screens.h"

#define PROFILE_BAND_H 29
#define PROFILE_COMMERCIAL_STATUS_H 36
#define PROFILE_HOME_GAP 3
#define PROFILE_HOME_INSET 4
#define PROFILE_FOOTER_H 34

// Its colors outside the shared tokens, set with the palette (its colors op)
extern color_t profile_bar_fg;
extern color_t profile_row_bg, profile_row_focus, profile_home_gutter;

// The footer pane's top, and a rounded pane across the screen inside the gutter
int profile_footer_top(void);
void profile_pane(int y, int h, color_t ink);
// Text centered across the screen, shortened at its end to the panes' inner width
void profile_center(const font_t *f, int y, const char *text, color_t c);

// The strip and the title of a screen (the reference top op's branch)
int draw_profile_top(const radio_view_t *v, radio_state_t st, const ui_state_t *s, const char *prefix,
	const char *name);
// Home: its status regions (returns the y below them), the footer under the banner, the whole frame, and whether a
// channel keeps the reference Home's body (trunked, SFR, a kind this build lacks)
int draw_home_status(const radio_view_t *v, const ui_state_t *s);
void profile_home_footer(const ui_state_t *s, const char *hint);
void draw_profile_home(const radio_view_t *v, const ui_state_t *s);
bool profile_operational_home(const radio_view_t *v);

// The bottom row under a list: the OK group, or a numbered menu's digit range, shown as OK
void compact_menu_hint(char *left, bool numbered);

// Lists: the palette entries the rows and icons need, the heading, a top-level menu's icon and a row's panel
void profile_list_colors(const ui_list_t *l);
int __attribute__((noinline)) profile_list_head(const radio_view_t *v, const ui_state_t *s, const ui_list_t *l,
	bool draw);
void draw_menu_icon(uint8_t icon, int x, int y);
void profile_row(int x, int y, int w, int h, color_t ink);

#endif
