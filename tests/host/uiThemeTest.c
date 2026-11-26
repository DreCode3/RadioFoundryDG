// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// The theme framework (core/ui/ui_theme.h) with themes of the test's own in use (UI_THEME_SELECT): an op left NULL
// reaches the reference theme's and draws the same frame, a palette without a colors op stays as it is, a theme's own
// ops are the ones called, a strip of the theme's own is measured where it is drawn, and the list frame (ui_draw_list)
// scrolls and places the cells of a geometry the reference theme doesn't have, also when that geometry is out of bounds
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "ui/gfx.h"
#include "ui/ui_screens.h"
#include "ui/ui_theme.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

#define W	160
#define H	120
static void d_init(void) {}
static void d_flush(const void *fb, uint16_t y0, uint16_t y1) { (void)fb; (void)y0; (void)y1; }
static void d_flush_indexed(const void *fb, const uint16_t *pal, uint16_t y0, uint16_t y1)
{
	(void)fb; (void)pal; (void)y0; (void)y1;
}
static void d_backlight(uint8_t p) { (void)p; }
static hal_display_t disp = { .width = W, .height = H, .bpp = 16, .init = d_init, .flush = d_flush,
	.set_backlight = d_backlight };

static const band_t bands[] = { { 136000000, 174000000, true }, { 400000000, 480000000, true } };
static const char *const power_names[] = { "50mW", "250mW", "500mW", "750mW", "1W", "2W", "3W", "4W", "5W" };
const board_t board = {
	.name = "Test radio", .bands = bands, .band_count = 2, .power_count = 9, .power_names = power_names,
	.power_default = 4, .display = &disp,
};

static radio_view_t view;
static ui_state_t state;

static void base(void)
{
	radio_view_t *v = &view;
	memset(v, 0, sizeof *v);
	v->state = RS_IDLE;
	v->zone_pos = 11;
	v->zone_size = 16;
	v->channel_number = 12;
	strcpy(v->zone_name, "Simplex");
	strcpy(v->ch.name, "2M CALLING FREQ");
	v->ch.rx_hz = v->ch.tx_hz = 146520000;
	v->ch.mode = RADIO_MODE_FM;
	v->ch.bw = RADIO_BW_25K;
	v->ch.power = CODEPLUG_POWER_DEFAULT;
	v->power = 4;
	v->squelch = 5;
	v->rssi_dbm = -112;
	v->tot_s = 180;
	memset(&state, 0, sizeof state);
	state.now_ms = 1000 + 14 * 60000;
	state.battery_pct = 84;
	state.brightness = 100;
	state.hints = true;
	state.home_hint = "OK Menu  * Quick";
}

// --- a list for the frame: rows "i0", "i1", ...; one of them may be a text line (never focused)

static int info_row = -1;
static void item(uint16_t i, ui_item_t *it)
{
	snprintf(it->label, sizeof it->label, "i%u", (unsigned)i);
	if (i % 3 == 0)
		strcpy(it->value, "On");
	if ((int)i == info_row)
		it->flags |= UI_ITEM_INFO;
}

static ui_list_t list(uint16_t count, uint16_t focus, uint16_t *top)
{
	ui_list_t l;
	memset(&l, 0, sizeof l);
	l.prefix = "Test";
	l.name = "List";
	l.count = count;
	l.focus = focus;
	l.top = top;
	l.item = item;
	l.empty = "Nothing here";
	l.hint = "OK open  Back";
	return l;
}

// --- frames: one of every shared entry point, to compare what two themes draw

typedef uint16_t shot_t[H][W];

static void take(shot_t out)
{
	for (int y = 0; y < H; y++)
		for (int x = 0; x < W; x++)
			out[y][x] = gfx_get_pixel(x, y);
}

enum { F_HOME, F_HOME_BANNER, F_LIST, F_LIST_BODY, F_LIST_EMPTY, F_QUICK, F_TOP_HINT,
#if CONFIG_DUAL_WATCH
	F_HOME_DUAL,
#endif
	F_COUNT };

static void frame(int which, shot_t out)
{
	static uint16_t top;
	ui_list_t l;
	base();
	top = 0;
	switch (which) {
	case F_HOME:
		ui_draw_home(&view, &state);
		break;
	case F_HOME_BANNER:
		state.banner.kind = UI_BANNER_INFO;
		state.banner.hold = true;
		strcpy(state.banner.title, "Radio check");
		strcpy(state.banner.text, "Answered");
		state.toast = "Saved";
		ui_draw_home(&view, &state);
		break;
	case F_LIST:
		l = list(40, 17, &top);
		l.header = "Find: 2_";
		ui_draw_list(&view, &state, &l);
		break;
	case F_LIST_BODY:
		l = list(3, 1, &top);
		l.body = "A message long enough to take more than one line of this display";
		ui_draw_list(&view, &state, &l);
		break;
	case F_LIST_EMPTY:
		l = list(0, 0, &top);
		ui_draw_list(&view, &state, &l);
		break;
	case F_QUICK:
		state.quick_focus = 4;
		ui_draw_quick(&view, &state);
		break;
	case F_TOP_HINT:
		gfx_clear(ui_col.bg);
		view.state = RS_RX;
		ui_draw_top(&view, &state, "Title", "Name");
		ui_draw_banner(&state);
		ui_draw_hint("OK open  Back", false);
		break;
#if CONFIG_DUAL_WATCH
	case F_HOME_DUAL:
		state.dual.on = true;
		state.dual.rx = state.dual.last = state.dual.back_to = UI_DUAL_NONE;
		for (int i = 0; i < 2; i++) {
			ui_dual_pane_t *p = &state.dual.pane[i];
			p->number = (uint16_t)(3 + 2 * i);
			p->mode = RADIO_MODE_FM;
			p->rx_hz = 146520000u + 25000u * (unsigned)i;
			strcpy(p->name, i ? "CHANNEL B" : "CHANNEL A");
			strcpy(p->zone, "Simplex");
		}
		ui_draw_home(&view, &state);
		break;
#endif
	}
	take(out);
}

static shot_t ref_shots[F_COUNT], shot;
static ui_layout_t ref_layout;
static ui_colors_t ref_colors;

// An op left NULL is the reference theme's: a theme with no op of its own (and the reference theme's palettes and
// colors op, by its descriptor) lays out and draws every frame as the reference theme does
static void test_null_ops(void)
{
	static ui_theme_ops_t ops;	// all NULL
	static ui_theme_t theme;
	ops.colors = ui_theme_reference.ops->colors;
	theme.palettes = ui_theme_reference.palettes;
	theme.palette_count = ui_theme_reference.palette_count;
	theme.ops = &ops;

	memset(&ui_layout, 0, sizeof ui_layout);
	uint32_t strips = ui_strip_draws;
	ui_theme_use(&theme);
	CHECK(ui_theme_cur() == &theme);
	CHECK(memcmp(&ui_layout, &ref_layout, sizeof ui_layout) == 0);	// layout
	CHECK(memcmp(&ui_col, &ref_colors, sizeof ui_col) == 0);
	for (int f = 0; f < F_COUNT; f++) {
		frame(f, shot);
		if (memcmp(shot, ref_shots[f], sizeof shot) != 0) {
			printf("FAIL frame %d differs from the reference theme's\n", f);
			fails++;
		}
	}
	CHECK(ui_strip_draws != strips);	// the reference strip ran
	ui_theme_use(&ui_theme_reference);
}

// --- palettes and the colors op

// Values no palette of the reference theme has, and fields its colors op would rewrite (focus_fg, chnum, the strip's)
static const ui_colors_t own_palettes[2] = {
	{ .bg = 0x0101, .fg = 0x0202, .dim = 0x0303, .line = 0x0404, .tx = 0x0505, .rx = 0x0606, .warn = 0x0707,
	  .idle = 0x0808, .pill_fg = 0x0909, .pill_dark_fg = 0x0A0A, .focus = 0x0B0B, .hint_bg = 0x0C0C, .priv = 0x0D0D,
	  .idle_fg = 0x0E0E, .focus_fg = 0x0F0F, .chnum = 0x1010, .strip_bg = 0x1111, .strip_warn = 0x1212 },
	{ .bg = 0x2121, .fg = 0x2222, .dim = 0x2323, .line = 0x2424, .tx = 0x2525, .rx = 0x2626, .warn = 0x2727,
	  .idle = 0x2828, .pill_fg = 0x2929, .pill_dark_fg = 0x2A2A, .focus = 0x2B2B, .hint_bg = 0x2C2C, .priv = 0x2D2D,
	  .idle_fg = 0x2E2E, .focus_fg = 0x2F2F, .chnum = 0x3030, .strip_bg = 0x3131, .strip_warn = 0x3232 },
};

static int colors_calls, colors_palette;
static bool colors_copied;
static void probe_colors(ui_colors_t *c, uint8_t palette)
{
	colors_calls++;
	colors_palette = palette;
	// the palette is in *c, which is the colors in use, before the op runs
	colors_copied = c == &ui_col && palette < 2 && memcmp(c, &own_palettes[palette], sizeof *c) == 0;
	c->warn = 0x7777;
}

static void test_colors(void)
{
	// No colors op: the palette as it is. This NULL is not the reference theme's op (its highlight color)
	static const ui_theme_ops_t plain_ops = { .colors = NULL };
	static const ui_theme_t plain = { .palettes = own_palettes, .palette_count = 2, .ops = &plain_ops };
	ui_theme_set(0);
	ui_theme_use(&plain);
	CHECK(memcmp(&ui_col, &own_palettes[0], sizeof ui_col) == 0);
	ui_theme_set(1);
	CHECK(ui_theme_get() == 1 && memcmp(&ui_col, &own_palettes[1], sizeof ui_col) == 0);
	ui_theme_set(2);	// the theme has two palettes: its first
	CHECK(ui_theme_get() == 0 && memcmp(&ui_col, &own_palettes[0], sizeof ui_col) == 0);
	// The reference theme's own setters still re-apply through the theme in use
	ui_accent_set(3);
	CHECK(memcmp(&ui_col, &own_palettes[0], sizeof ui_col) == 0);
	ui_accent_set(0);

	// A colors op: after the copy, on the colors in use, with the palette's number
	static const ui_theme_ops_t tinted_ops = { .colors = probe_colors };
	static const ui_theme_t tinted = { .palettes = own_palettes, .palette_count = 2, .ops = &tinted_ops };
	ui_theme_set(1);
	colors_calls = 0;
	ui_theme_use(&tinted);
	CHECK(colors_calls == 1 && colors_palette == 1 && colors_copied);
	ui_colors_t want = own_palettes[1];
	want.warn = 0x7777;
	CHECK(memcmp(&ui_col, &want, sizeof ui_col) == 0);

	// A palette the reference theme has and this one doesn't, kept over a change of theme, is brought into range
	ui_theme_use(&ui_theme_reference);
	ui_theme_set(UI_THEME_NIGHT);
	CHECK(ui_theme_get() == UI_THEME_NIGHT);
	ui_theme_use(&plain);
	CHECK(ui_theme_get() == 0 && memcmp(&ui_col, &own_palettes[0], sizeof ui_col) == 0);
	ui_theme_use(&ui_theme_reference);
}

// --- a theme's own ops are the ones the shared entry points call

static struct {
	int layout, top, bottom, banner, home, home_dual, quick, head;
	bool top_draw;
	radio_state_t top_st;
	const char *top_prefix, *top_name, *bottom_text, *bottom_soft;
	uint8_t bottom_flags;
	bool head_draw, head_null;
} got;

static void probe_layout(ui_layout_t *L)
{
	got.layout++;
	*L = ref_layout;
	L->row_h = ref_layout.row_h + 2;
}

static int probe_top(const radio_view_t *v, radio_state_t st, const ui_state_t *s, const char *prefix, const char *name,
	bool draw)
{
	(void)v;
	(void)s;
	got.top++;
	got.top_draw = draw;
	got.top_st = st;
	got.top_prefix = prefix;
	got.top_name = name;
	return 33;
}

static void probe_bottom(const char *text, uint8_t flags, const char *soft_key)
{
	got.bottom++;
	got.bottom_text = text;
	got.bottom_flags = flags;
	got.bottom_soft = soft_key;
}

static void probe_banner(const ui_state_t *s) { (void)s; got.banner++; }
static void probe_home(const radio_view_t *v, const ui_state_t *s) { (void)v; (void)s; got.home++; }
#if CONFIG_DUAL_WATCH
static void probe_home_dual(const radio_view_t *v, const ui_state_t *s) { (void)v; (void)s; got.home_dual++; }
#endif
static void probe_quick(const radio_view_t *v, const ui_state_t *s) { (void)v; (void)s; got.quick++; }

static int head_y = 20;
static char order[96];	// the list ops in the order the frame called them
static void note(char c)
{
	size_t n = strlen(order);
	if (n + 1 < sizeof order) {
		order[n] = c;
		order[n + 1] = 0;
	}
}

static int probe_head(const radio_view_t *v, const ui_state_t *s, const ui_list_t *l, bool draw)
{
	(void)l;
	got.head++;
	got.head_draw = draw;
	got.head_null = v == NULL && s == NULL;
	if (draw)
		note('H');
	return head_y;
}

static void test_own_ops(void)
{
	static const ui_theme_ops_t ops = {
		.layout = probe_layout, .top = probe_top, .bottom = probe_bottom, .banner = probe_banner,
		.home = probe_home,
#if CONFIG_DUAL_WATCH
		.home_dual = probe_home_dual,
#endif
		.quick = probe_quick, .list_head = probe_head,
	};
	static ui_theme_t theme = { .ops = &ops };
	theme.palettes = ui_theme_reference.palettes;
	theme.palette_count = ui_theme_reference.palette_count;

	memset(&got, 0, sizeof got);
	ui_theme_use(&theme);
	CHECK(got.layout == 1 && ui_layout.row_h == ref_layout.row_h + 2);
	// colors is NULL here: the reference palette without the reference theme's highlight color
	CHECK(memcmp(&ui_col, &ui_theme_reference.palettes[UI_THEME_DARK], sizeof ui_col) == 0);

	base();
	uint32_t strips = ui_strip_draws;
	view.state = RS_TX;
	CHECK(ui_draw_top(&view, &state, "P", "N") == 33);
	CHECK(got.top == 1 && got.top_draw && got.top_st == RS_TX && strcmp(got.top_prefix, "P") == 0 &&
	      strcmp(got.top_name, "N") == 0);
	CHECK(ui_strip_draws == strips);	// not the reference strip

	ui_draw_hint("toast", true);
	CHECK(got.bottom == 1 && strcmp(got.bottom_text, "toast") == 0 && got.bottom_flags == UI_BAR_TOAST &&
	      got.bottom_soft == NULL);
	ui_draw_hint("hint", false);
	CHECK(got.bottom == 2 && got.bottom_flags == 0);
	ui_draw_bottom(&state, "keys");
	CHECK(got.bottom == 3 && strcmp(got.bottom_text, "keys") == 0 && got.bottom_flags == 0);
	state.toast = "done";
	ui_draw_bottom(&state, "keys");
	CHECK(got.bottom == 4 && strcmp(got.bottom_text, "done") == 0 && got.bottom_flags == UI_BAR_TOAST);
	state.toast = NULL;
	state.hints = false;
	ui_draw_bottom(&state, "keys");
	CHECK(got.bottom == 4);		// hints off and no toast: no bottom row
	state.hints = true;

	ui_draw_banner(&state);
	CHECK(got.banner == 1);
	ui_draw_home(&view, &state);
	CHECK(got.home == 1 && got.home_dual == 0);
#if CONFIG_DUAL_WATCH
	state.dual.on = true;
	ui_draw_home(&view, &state);
	CHECK(got.home == 1 && got.home_dual == 1);
	state.dual.on = false;
#endif
	ui_draw_quick(&view, &state);
	CHECK(got.quick == 1);

	// The list frame asks the theme's head to draw, a caller that only measures asks it not to (and has no view)
	uint16_t top = 0;
	ui_list_t l = list(5, 0, &top);
	order[0] = 0;
	ui_draw_list(&view, &state, &l);
	CHECK(got.head == 1 && got.head_draw && !got.head_null);
	CHECK(got.banner == 2 && got.bottom == 5 && strcmp(got.bottom_text, "OK open  Back") == 0);
	l.body = "Short";
	CHECK(ui_list_body_fits(&state, &l));
	CHECK(got.head == 2 && !got.head_draw && got.head_null);
	head_y = H;	// a head that leaves no room: the body doesn't fit
	CHECK(!ui_list_body_fits(&state, &l));
	head_y = 20;
	ui_theme_use(&ui_theme_reference);
}

// --- a strip of the theme's own, and nothing else: lists are measured where they are drawn

#define TALL_Y	60	// under a title; 10 less without one
static int tall_draws, tall_measures;
static bool tall_measure_bare, tall_titled;
static int tall_top(const radio_view_t *v, radio_state_t st, const ui_state_t *s, const char *prefix, const char *name,
	bool draw)
{
	(void)st;
	if (draw) {
		tall_draws++;
	} else {
		tall_measures++;
		tall_measure_bare = v == NULL && s == NULL;
	}
	tall_titled = prefix != NULL || name != NULL;
	return tall_titled ? TALL_Y : TALL_Y - 10;
}

// Ink (anything but the background) in the rows y0 .. y1 - 1
static bool ink(int y0, int y1)
{
	for (int y = y0; y < y1; y++)
		for (int x = 0; x < W; x++)
			if (gfx_get_pixel(x, y) != ui_col.bg)
				return true;
	return false;
}

static void test_top_only(void)
{
	static const ui_theme_ops_t ops = { .top = tall_top };
	static ui_theme_t theme = { .ops = &ops };
	theme.palettes = ui_theme_reference.palettes;
	theme.palette_count = ui_theme_reference.palette_count;
	const ui_theme_ops_t *ref = ui_theme_reference.ops;

	// The reference strip measures what it returns when it draws, with a title and without: on a display that gives
	// the title a row of its own (16 colors, the test is built for 240 rows) and on this one, where it is in the strip
	base();
	hal_display_t tall = disp;
	tall.width = 320; tall.height = 240; tall.bpp = 4; tall.flush_indexed = d_flush_indexed;
	CHECK(gfx_init(&tall));
	ui_layout_init();
	gfx_clear(ui_col.bg);
	int titled = ref->top(NULL, RS_IDLE, NULL, "Test", "List", false);
	int bare = ref->top(NULL, RS_IDLE, NULL, NULL, NULL, false);
	CHECK(titled > bare && bare == ui_layout.strip_h);
	CHECK(titled == ref->top(&view, RS_IDLE, &state, "Test", "List", true));
	CHECK(titled == ref->top(&view, RS_IDLE, &state, NULL, "List", true));
	CHECK(bare == ref->top(&view, RS_IDLE, &state, NULL, NULL, true));
	CHECK(gfx_init(&disp));
	ui_layout_init();
	gfx_clear(ui_col.bg);
	int ref_y = ref->top(NULL, RS_IDLE, NULL, "Test", "List", false);
	CHECK(ref_y == ref->top(&view, RS_IDLE, &state, "Test", "List", true));
	CHECK(ref->top(NULL, RS_IDLE, NULL, NULL, NULL, false) == ref->top(&view, RS_IDLE, &state, NULL, NULL, true));
	CHECK(ref_y < TALL_Y);

	CHECK(ui_theme_use(&theme));
	const int fh = ui_layout.small->h;
	uint16_t top = 0;
	ui_list_t l = list(2, 0, &top);
	// A message of n lines above two rows: it fits when it would from the theme's y, not from the reference strip's.
	// The room is the frame's rule (half of what is left, and a line)
	static const char body[] = "L1\nL2\nL3\nL4\nL5\nL6\nL7\nL8";
	bool differs = false, some_fit = false, some_not = false;
	for (int n = 1; n <= 8; n++) {
		char text[sizeof body];
		memcpy(text, body, (size_t)(3 * n - 1));
		text[3 * n - 1] = 0;
		l.body = text;
		bool want = TALL_Y + n * fh <= TALL_Y + (ui_layout.hint_y - TALL_Y) / 2 + fh;
		bool from_ref = ref_y + n * fh <= ref_y + (ui_layout.hint_y - ref_y) / 2 + fh;
		tall_draws = tall_measures = 0;
		bool fits = ui_list_body_fits(&state, &l);
		CHECK(fits == want);
		CHECK(tall_measures == 1 && tall_draws == 0 && tall_measure_bare && tall_titled);	// asked, with the title, not drawn
		differs = differs || want != from_ref;
		some_fit = some_fit || want;
		some_not = some_not || !want;
		// Drawn: nothing of the list above the theme's y, the text from there, and all of it when it was said to fit
		tall_draws = 0;
		ui_draw_list(&view, &state, &l);
		CHECK(tall_draws == 1);
		CHECK(!ink(0, TALL_Y) && ink(TALL_Y, TALL_Y + fh));
		if (fits)
			CHECK(ink(TALL_Y + (n - 1) * fh, TALL_Y + n * fh));
	}
	CHECK(differs && some_fit && some_not);	// the bodies tell the two strips apart, and fall both ways
	// Without a title the theme is asked so, and the list starts where it says
	l.prefix = l.name = NULL;
	l.body = "L1";
	CHECK(ui_list_body_fits(&state, &l) && !tall_titled);
	ui_draw_list(&view, &state, &l);
	CHECK(!ink(0, TALL_Y - 10) && ink(TALL_Y - 10, TALL_Y - 10 + fh));
	ui_theme_use(&ui_theme_reference);
}

// --- a bottom row of the theme's own, and nothing else: Home hands it the assigned soft key's label

static void test_bottom_only(void)
{
	static const ui_theme_ops_t ops = { .bottom = probe_bottom };
	static ui_theme_t theme = { .ops = &ops };
	theme.palettes = ui_theme_reference.palettes;
	theme.palette_count = ui_theme_reference.palette_count;
	CHECK(ui_theme_use(&theme));
	base();
	memset(&got, 0, sizeof got);
	ui_draw_home(&view, &state);
	CHECK(got.bottom == 1 && got.bottom_flags == 0 && got.bottom_text != NULL && got.bottom_soft == NULL);
#if CONFIG_KEY_ASSIGN
	static const char label[] = "Scan";
	state.home_soft_key_label = label;
	ui_draw_home(&view, &state);
	CHECK(got.bottom == 2 && got.bottom_flags == 0 && got.bottom_soft == label);
	state.locked = true;	// the keys are locked: the soft key does nothing, so it has no label
	ui_draw_home(&view, &state);
	CHECK(got.bottom == 3 && got.bottom_soft == NULL);
	state.locked = false;
	state.toast = "Saved";	// a toast is not the soft key's row
	ui_draw_home(&view, &state);
	CHECK(got.bottom == 4 && got.bottom_flags == UI_BAR_TOAST && got.bottom_soft == NULL);
#endif
	ui_theme_use(&ui_theme_reference);
}

// A theme without a palette is refused: the one in use stays, with its colors
static void test_no_palette(void)
{
	static const ui_theme_ops_t ops = { .bottom = probe_bottom };
	static const ui_theme_t theme = { .palettes = own_palettes, .palette_count = 0, .ops = &ops };
	ui_colors_t before = ui_col;
	CHECK(!ui_theme_use(&theme));
	CHECK(ui_theme_cur() == &ui_theme_reference && memcmp(&ui_col, &before, sizeof ui_col) == 0);
}

// --- the list frame over a theme's geometry: which cells, where, and *l->top

static struct { uint8_t rows, cols; bool paged; } grid = { 3, 2, false };
#define GX	4
#define GPITCH	10
#define GGAP	6
static const int16_t gw[UI_LIST_COLS] = { 70, 60 };
static int geom_y;

static void grid_geom(const ui_list_t *l, int y, int y_end, ui_list_geom_t *g)
{
	(void)l;
	(void)y_end;
	note('G');
	geom_y = y;
	g->font = ui_layout.small;
	g->x = GX;
	g->y = (int16_t)y;
	g->pitch = GPITCH;
	g->col_w[0] = gw[0];
	g->col_w[1] = gw[1];
	g->col_gap = GGAP;
	g->rows = grid.rows;
	g->cols = grid.cols;
	g->paged = grid.paged;
}

static struct { char label[sizeof ((ui_item_t *)0)->label]; int x, y, w, h; bool focus; } cells[32];
static int ncells;
static void probe_cell(const ui_list_t *l, const ui_list_geom_t *g, const ui_cell_t *c, const ui_item_t *it)
{
	(void)l;
	(void)g;
	note('C');
	if (ncells < (int)(sizeof cells / sizeof cells[0])) {
		strcpy(cells[ncells].label, it->label);
		cells[ncells].x = c->x;
		cells[ncells].y = c->y;
		cells[ncells].w = c->w;
		cells[ncells].h = c->h;
		cells[ncells].focus = c->focus;
	}
	ncells++;
}

static struct { uint16_t top, count; uint8_t rows, cols; } scrolled[2];
static void probe_scroll(const ui_list_geom_t *g, uint16_t top, uint16_t count, bool over)
{
	note(over ? 'S' : 's');
	scrolled[over].top = top;
	scrolled[over].count = count;
	scrolled[over].rows = g->rows;
	scrolled[over].cols = g->cols;
}

// Draws the list and checks the frame showed the cells first .. first + n - 1, column by column from the geometry's
// corner, the focus on the focused row only, and left *top at first
static void expect(int line, uint16_t count, uint16_t focus, uint16_t top_in, uint16_t first, int n, int rows)
{
	uint16_t top = top_in;
	ui_list_t l = list(count, focus, &top);
	char want[96];
	bool ok = true;
	base();
	order[0] = 0;
	ncells = 0;
	ui_draw_list(&view, &state, &l);
	snprintf(want, sizeof want, "HGs%.*sS", n, "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC");
	ok = ok && strcmp(order, want) == 0 && top == first && ncells == n && geom_y == head_y;
	ok = ok && scrolled[0].top == first && scrolled[0].count == count && scrolled[1].top == first &&
		scrolled[1].count == count;
	for (int r = 0; ok && r < n; r++) {
		int col = r / rows;
		char label[16];
		snprintf(label, sizeof label, "i%d", first + r);
		ok = strcmp(cells[r].label, label) == 0 && cells[r].x == GX + col * (gw[0] + GGAP) && cells[r].w == gw[col] &&
			cells[r].y == head_y + (r % rows) * GPITCH && cells[r].h == GPITCH &&
			cells[r].focus == (first + r == focus && first + r != info_row);
	}
	if (!ok) {
		printf("FAIL %s:%d: count %u focus %u top %u: order %s (want %s), top %u (want %u), %d cells\n", __FILE__, line,
		       (unsigned)count, (unsigned)focus, (unsigned)top_in, order, want, (unsigned)top, (unsigned)first, ncells);
		fails++;
	}
}
#define EXPECT(count, focus, top_in, first, n, rows) expect(__LINE__, count, focus, top_in, first, n, rows)

static void test_list_frame(void)
{
	static const ui_theme_ops_t ops = { .list_head = probe_head, .list_geom = grid_geom, .cell = probe_cell,
		.scroll = probe_scroll };
	static ui_theme_t theme = { .ops = &ops };
	theme.palettes = ui_theme_reference.palettes;
	theme.palette_count = ui_theme_reference.palette_count;
	ui_theme_use(&theme);

	// Two columns of three rows, moved a row at a time
	grid.rows = 3; grid.cols = 2; grid.paged = false;
	EXPECT(0, 0, 0, 0, 0, 3);	// empty: no cell
	EXPECT(0, 0, 5, 0, 0, 3);	// ... and a stale top goes back to 0
	EXPECT(4, 3, 0, 0, 4, 3);	// fewer rows than cells: all of them, the second column part filled
	EXPECT(4, 0, 2, 0, 4, 3);	// ... from the first, wherever top was
	EXPECT(6, 5, 0, 0, 6, 3);	// exactly the cells on screen
	EXPECT(20, 0, 0, 0, 6, 3);
	EXPECT(20, 5, 0, 0, 6, 3);	// the last cell on screen: nothing moves
	EXPECT(20, 6, 0, 1, 6, 3);	// one past it: one row on
	EXPECT(20, 19, 0, 14, 6, 3);	// the last row: the last six
	EXPECT(20, 19, 14, 14, 6, 3);
	EXPECT(20, 8, 5, 5, 6, 3);	// in view: top stays
	EXPECT(20, 7, 14, 7, 6, 3);	// above the view: it becomes the first
	EXPECT(20, 0, 14, 0, 6, 3);
	EXPECT(7, 2, 5, 1, 6, 3);	// a top past the end (the list got shorter): the last six
	info_row = 4;			// a text line has no focus, though the list's focus is on it
	EXPECT(20, 4, 0, 0, 6, 3);
	info_row = -1;

	// The same cells a page at a time: top is the focused row's page, whatever it was
	grid.paged = true;
	EXPECT(0, 0, 3, 0, 0, 3);
	EXPECT(4, 3, 2, 0, 4, 3);
	EXPECT(20, 5, 12, 0, 6, 3);
	EXPECT(20, 6, 0, 6, 6, 3);
	EXPECT(20, 17, 0, 12, 6, 3);
	EXPECT(20, 18, 0, 18, 2, 3);	// the last page, part filled
	EXPECT(20, 19, 6, 18, 2, 3);
	EXPECT(18, 17, 0, 12, 6, 3);	// a full last page

	// One column, and no place to keep top (l->top NULL): the frame starts from 0 each time
	grid.rows = 4; grid.cols = 1; grid.paged = false;
	EXPECT(10, 9, 0, 6, 4, 4);
	{
		ui_list_t l = list(10, 9, NULL);
		ncells = 0;
		ui_draw_list(&view, &state, &l);
		CHECK(ncells == 4 && strcmp(cells[0].label, "i6") == 0 && cells[3].focus);
	}

	// The Find header is the frame's: the cells start a line of the small font below the theme's head
	{
		uint16_t top = 0;
		ui_list_t l = list(3, 0, &top);
		l.header = "Find: 2_";
		ncells = 0;
		ui_draw_list(&view, &state, &l);
		CHECK(geom_y == head_y + ui_layout.small->h && ncells == 3 && cells[0].y == geom_y);
	}
	ui_theme_use(&ui_theme_reference);
}

// A geometry out of the bounds ui_theme.h gives is brought into them: no division by zero, no width read past col_w
static void test_geometry_bounds(void)
{
	static const ui_theme_ops_t ops = { .list_head = probe_head, .list_geom = grid_geom, .cell = probe_cell,
		.scroll = probe_scroll };
	static ui_theme_t theme = { .ops = &ops };
	theme.palettes = ui_theme_reference.palettes;
	theme.palette_count = ui_theme_reference.palette_count;
	ui_theme_use(&theme);

	grid.rows = 0; grid.cols = 0; grid.paged = false;	// as one row, one column
	EXPECT(5, 3, 0, 3, 1, 1);
	CHECK(scrolled[1].rows == 1 && scrolled[1].cols == 1);	// the ops see the geometry the frame used
	grid.paged = true;
	EXPECT(5, 3, 0, 3, 1, 1);
	EXPECT(0, 0, 0, 0, 0, 1);
	grid.rows = 2; grid.cols = 7; grid.paged = false;	// as UI_LIST_COLS columns
	EXPECT(20, 0, 0, 0, 2 * UI_LIST_COLS, 2);
	EXPECT(20, 19, 0, 20 - 2 * UI_LIST_COLS, 2 * UI_LIST_COLS, 2);
	CHECK(scrolled[1].rows == 2 && scrolled[1].cols == UI_LIST_COLS);
	grid.rows = 0; grid.cols = 255; grid.paged = true;
	EXPECT(20, 7, 0, 6, UI_LIST_COLS, 1);
	grid.rows = 3; grid.cols = 2; grid.paged = false;
	ui_theme_use(&ui_theme_reference);
}

int main(void)
{
	CHECK(gfx_init(&disp));
	ui_layout_init();
	CHECK(ui_theme_cur() == &ui_theme_reference);
	ref_layout = ui_layout;
	ref_colors = ui_col;
	for (int f = 0; f < F_COUNT; f++)
		frame(f, ref_shots[f]);
	// The frames differ from each other, so comparing them means something
	for (int f = 1; f < F_COUNT; f++)
		CHECK(memcmp(ref_shots[f], ref_shots[0], sizeof shot) != 0);

	test_null_ops();
	test_colors();
	test_own_ops();
	test_top_only();
	test_bottom_only();
	test_no_palette();
	test_list_frame();
	test_geometry_bounds();

	// Back on the reference theme: its layout, its colors and its frames again
	ui_theme_set(UI_THEME_DARK);
	CHECK(ui_theme_cur() == &ui_theme_reference);
	CHECK(memcmp(&ui_layout, &ref_layout, sizeof ui_layout) == 0);
	CHECK(memcmp(&ui_col, &ref_colors, sizeof ui_col) == 0);
	for (int f = 0; f < F_COUNT; f++) {
		frame(f, shot);
		CHECK(memcmp(shot, ref_shots[f], sizeof shot) == 0);
	}

	printf("uiThemeTest: %s (%d failures)\n", fails ? "FAILED" : "passed", fails);
	return fails != 0;
}
