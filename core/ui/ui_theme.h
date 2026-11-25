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
