// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// Scratch only (never committed): a second theme with some ops of its own and some NULL, selected at run time, to
// prove the stack check follows every theme op call
#include <string.h>
#include "ui_theme.h"

static void scratch_bottom(const char *text, uint8_t flags, const char *soft_key)
{
	char fit[96];	// a frame the report must show under ui_draw_hint
	gfx_text_fit(ui_layout.small, text, gfx_width(), fit, sizeof fit);
	gfx_fill(0, ui_layout.hint_y, gfx_width(), gfx_height() - ui_layout.hint_y, ui_col.hint_bg);
	gfx_text(ui_layout.small, 0, ui_layout.hint_y, fit, (flags & UI_BAR_TOAST) ? ui_col.warn : ui_col.dim);
	if (soft_key != NULL)
		gfx_text(ui_layout.small, gfx_width() / 2, ui_layout.hint_y, soft_key, ui_col.fg);
}

static void scratch_geom(const ui_list_t *l, int y, int y_end, ui_list_geom_t *g)
{
	(void)l;
	g->font = ui_layout.small;
	g->x = 0;
	g->y = (int16_t)y;
	g->pitch = (int16_t)(ui_layout.row_h + 4);
	g->rows = (uint8_t)((y_end - y) / g->pitch > 0 ? (y_end - y) / g->pitch : 1);
	g->cols = 2;
	g->col_gap = 2;
	g->col_w[0] = g->col_w[1] = (int16_t)((gfx_width() - 2) / 2);
	g->paged = false;
}

static void scratch_cell(const ui_list_t *l, const ui_list_geom_t *g, const ui_cell_t *c, const ui_item_t *it)
{
	char fit[80];
	(void)l;
	if (c->focus)
		gfx_fill(c->x, c->y, c->w, c->h, ui_col.focus);
	gfx_text_fit(g->font, it->label, c->w, fit, sizeof fit);
	gfx_text(g->font, c->x, c->y, fit, c->focus ? ui_col.focus_fg : ui_col.fg);
}

static void scratch_colors(ui_colors_t *c, uint8_t palette)
{
	(void)palette;
	c->focus_fg = c->bg;
}

static void scratch_home(const radio_view_t *v, const ui_state_t *s)
{
	gfx_clear(ui_col.bg);
	ui_draw_top(v, s, NULL, NULL);
	gfx_text(ui_layout.large, 0, ui_layout.strip_h, v->ch.name, ui_col.fg);
	ui_draw_banner(s);
	ui_draw_bottom(s, s->home_hint);
}

static int scratch_top(const radio_view_t *v, radio_state_t st, const ui_state_t *s, const char *prefix, const char *name,
	bool draw)
{
	char fit[64];
	int y = ui_layout.strip_h + 6;
	(void)v; (void)st; (void)s; (void)prefix;
	if (draw) {
		ui_strip_draws++;
		gfx_fill(0, 0, gfx_width(), y, ui_col.hint_bg);
		gfx_text_fit(ui_layout.small, name != NULL ? name : "", gfx_width(), fit, sizeof fit);
		gfx_text(ui_layout.small, 0, 0, fit, ui_col.fg);
	}
	return y;
}

static const ui_colors_t scratch_palettes[1] = {
	{ .bg = RGB565(0, 0, 40), .fg = RGB565(255, 255, 255), .dim = RGB565(160, 160, 200), .line = RGB565(60, 60, 90),
	  .tx = RGB565(224, 32, 32), .rx = RGB565(40, 184, 72), .warn = RGB565(240, 168, 0), .idle = RGB565(96, 96, 96),
	  .pill_fg = RGB565(255, 255, 255), .focus = RGB565(32, 96, 200), .hint_bg = RGB565(20, 20, 60) },
};

// banner, home_dual, quick, layout, list_head and scroll are the reference theme's
static const ui_theme_ops_t scratch_ops = {
	.colors = scratch_colors,
	.top = scratch_top,
	.bottom = scratch_bottom,
	.home = scratch_home,
	.list_geom = scratch_geom,
	.cell = scratch_cell,
};

const ui_theme_t ui_theme_scratch = {
	.palettes = scratch_palettes,
	.palette_count = 1,
	.ops = &scratch_ops,
};

void ui_theme_scratch_pick(bool on)
{
	ui_theme_use(on ? &ui_theme_scratch : &ui_theme_reference);
}
