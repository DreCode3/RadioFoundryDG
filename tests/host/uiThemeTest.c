// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// The theme framework (core/ui/ui_theme.h) with themes of the test's own in use (UI_THEME_SELECT): an op left NULL
// reaches the reference theme's and draws the same frame, a palette without a colors op stays as it is, a theme's own
// ops are the ones called, a strip of the theme's own is measured where it is drawn, and the list frame (ui_draw_list)
// scrolls and places the cells of a geometry the reference theme doesn't have, also when that geometry is out of bounds,
// and the theme picked for a display (ui_theme_pick). Built a second time with the panels theme (uiThemeTestPanels),
// which also checks the panels theme's own frames by their structure: the scroll position drawn before the cells and
// the soft key bar's two labels
#include <stdio.h>
#include <string.h>
#include "board.h"
#include "ui/gfx.h"
#include "ui/ui_screens.h"
#include "ui/ui_theme.h"
#include "codeplug/codeplug_modes.h"

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

static void probe_bottom(const char *text, unsigned flags, const char *soft_key)
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
static radio_state_t tall_st;
static int tall_top(const radio_view_t *v, radio_state_t st, const ui_state_t *s, const char *prefix, const char *name,
	bool draw)
{
	if (draw) {
		tall_draws++;
		tall_st = st;
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
	// A list's strip shows what the radio is doing: the view's state, as on every other screen
	l.body = "L1";
	view.state = RS_RX;
	ui_draw_list(&view, &state, &l);
	CHECK(tall_st == RS_RX);
	view.state = RS_TX;
	ui_draw_list(&view, &state, &l);
	CHECK(tall_st == RS_TX);
	view.state = RS_IDLE;
	ui_draw_list(&view, &state, &l);
	CHECK(tall_st == RS_IDLE);
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
	// This theme has no strip of its own: a list is measured through the reference strip (without a view: nothing is
	// drawn), with the reference theme's answer for every length of message
	uint16_t top = 0;
	ui_list_t l = list(2, 0, &top);
	static const char body[] = "L1\nL2\nL3\nL4\nL5\nL6\nL7\nL8";
	bool with_theme[8], some_fit = false, some_not = false;
	uint32_t strips = ui_strip_draws;
	for (int n = 1; n <= 8; n++) {
		char text[sizeof body];
		memcpy(text, body, (size_t)(3 * n - 1));
		text[3 * n - 1] = 0;
		l.body = text;
		with_theme[n - 1] = ui_list_body_fits(&state, &l);
	}
	CHECK(ui_strip_draws == strips);
	l.body = NULL;
	ui_theme_use(&ui_theme_reference);
	for (int n = 1; n <= 8; n++) {
		char text[sizeof body];
		memcpy(text, body, (size_t)(3 * n - 1));
		text[3 * n - 1] = 0;
		l.body = text;
		bool fits = ui_list_body_fits(&state, &l);
		CHECK(fits == with_theme[n - 1]);
		some_fit = some_fit || fits;
		some_not = some_not || !fits;
	}
	CHECK(some_fit && some_not);
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

static struct { uint8_t rows, cols; bool paged, across; } grid = { 3, 2, false, false };
#define GX	4
#define GPITCH	10
#define GGAP	6
static const int16_t gw[UI_LIST_COLS] = { 70, 60, 50 };
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
	for (int i = 0; i < UI_LIST_COLS; i++)
		g->col_w[i] = gw[i];
	g->col_gap = GGAP;
	g->rows = grid.rows;
	g->cols = grid.cols;
	g->paged = grid.paged;
	g->across = grid.across;
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

static struct { const ui_list_t *l; uint16_t top, count; uint8_t rows, cols; bool across; } scrolled[2];
static void probe_scroll(const ui_list_t *l, const ui_list_geom_t *g, uint16_t top, uint16_t count, bool over)
{
	note(over ? 'S' : 's');
	scrolled[over].l = l;
	scrolled[over].top = top;
	scrolled[over].count = count;
	scrolled[over].rows = g->rows;
	scrolled[over].cols = g->cols;
	scrolled[over].across = g->across;
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
		scrolled[1].count == count && scrolled[0].l == &l && scrolled[1].l == &l;	// the list, for its focus
	int cols = grid.cols < 1 ? 1 : grid.cols > UI_LIST_COLS ? UI_LIST_COLS : grid.cols;
	for (int r = 0; ok && r < n; r++) {
		int col = grid.across ? r % cols : r / rows, row = grid.across ? r / cols : r % rows, x = GX;
		for (int k = 0; k < col; k++)
			x += gw[k] + GGAP;
		char label[16];
		snprintf(label, sizeof label, "i%d", first + r);
		ok = strcmp(cells[r].label, label) == 0 && cells[r].x == x && cells[r].w == gw[col] &&
			cells[r].y == head_y + row * GPITCH && cells[r].h == GPITCH &&
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

	// Three columns in a keypad's order (across): a row is filled before the next, a page at a time
	grid.rows = 3; grid.cols = 3; grid.paged = true; grid.across = true;
	EXPECT(8, 0, 0, 0, 8, 3);	// 1 2 3 / 4 5 6 / 7 8: the last row part filled
	EXPECT(8, 7, 0, 0, 8, 3);
	EXPECT(20, 9, 0, 9, 9, 3);	// the second page
	EXPECT(20, 19, 0, 18, 2, 3);
	grid.across = false;

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

// Stack bytes under the caller's next frames set to ones, so a field left unset there reads as set
static void __attribute__((noinline)) scribble_stack(void)
{
	volatile uint8_t junk[4096];
	for (size_t i = 0; i < sizeof junk; i++)
		junk[i] = 0xFF;
}

// The reference theme's geometry sets every field: across false (its rows go down one column), whatever the stack held
static void test_reference_geometry_fields(void)
{
	static const ui_theme_ops_t ops = { .list_head = probe_head, .cell = probe_cell, .scroll = probe_scroll };
	static ui_theme_t theme = { .ops = &ops };
	theme.palettes = ui_theme_reference.palettes;
	theme.palette_count = ui_theme_reference.palette_count;
	ui_theme_use(&theme);
	uint16_t top = 0;
	ui_list_t l = list(5, 0, &top);
	scrolled[0].across = scrolled[1].across = true;
	scribble_stack();
	ui_draw_list(&view, &state, &l);
	CHECK(!scrolled[0].across && !scrolled[1].across && scrolled[1].cols == 1);
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

// The theme for a display: the one numbered where the build has it and it fits, else the build's default where that
// fits, else the reference theme, which fits every display. The test's own themes are in no build's list
static bool picks_on(hal_display_t d, uint8_t id, const ui_theme_t *want)
{
	d.flush_indexed = d_flush_indexed;
	bool ok = gfx_init(&d) && ui_theme_pick(id) == want;
	CHECK(gfx_init(&disp));
	return ok;
}

static bool picks(int w, int inset_right, int h, uint8_t bpp, uint8_t id, const ui_theme_t *want)
{
	hal_display_t d = disp;
	d.width = (uint16_t)w; d.height = (uint16_t)h; d.inset_right = (uint8_t)inset_right; d.bpp = bpp;
	return picks_on(d, id, want);
}

#if CONFIG_UI_THEME_PANELS
// A panel behind a bezel that hides lr px left and right and tb px top and bottom
static hal_display_t inset_panel(int w, int h, int lr, int tb, uint8_t bpp)
{
	hal_display_t d = disp;
	d.width = (uint16_t)w; d.height = (uint16_t)h; d.bpp = bpp;
	d.inset_left = d.inset_right = (uint8_t)lr;
	d.inset_top = d.inset_bottom = (uint8_t)tb;
	return d;
}
#endif

#if CONFIG_UI_THEME_PANELS
// Talkgroup name first's start on this display: the theme a reset radio starts in declares it or not
static bool name_first_on(int w, int inset_right, int h, uint8_t bpp)
{
	hal_display_t d = disp;
	d.width = (uint16_t)w; d.height = (uint16_t)h; d.inset_right = (uint8_t)inset_right; d.bpp = bpp;
	d.flush_indexed = d_flush_indexed;
	bool on = gfx_init(&d) && ui_theme_starts_name_first();
	CHECK(gfx_init(&disp));
	return on;
}
#endif

static void test_pick(void)
{
	const ui_theme_t *ref = &ui_theme_reference;
	CHECK(picks(W, 0, H, 16, UI_THEME_ID_DEFAULT, ref));
	CHECK(picks(W, 0, H, 16, UI_THEME_ID_REFERENCE, ref));
	CHECK(picks(W, 0, H, 16, UI_THEME_ID_PANELS, ref));	// doesn't fit, or isn't built
	CHECK(picks(W, 0, H, 16, 200, ref));			// a number from a later build
	CHECK(picks(320, 10, 240, 4, UI_THEME_ID_REFERENCE, ref));
#if CONFIG_UI_THEME_PANELS
	// A color display with at least 158 x 127 to draw on (the panel less its insets). The build's default: panels, or
	// the original theme where the build starts in it (CONFIG_UI_THEME_DEFAULT, uiThemeTestPanelsOriginal)
	const ui_theme_t *panels = &ui_theme_panels;
	const ui_theme_t *def = CONFIG_UI_THEME_DEFAULT == UI_THEME_ID_REFERENCE ? ref : panels;
	CHECK(picks(320, 10, 240, 4, UI_THEME_ID_DEFAULT, def));	// the build's default
	CHECK(picks(320, 10, 240, 4, UI_THEME_ID_PANELS, panels));
	CHECK(picks(320, 10, 240, 4, 200, def));
	CHECK(picks(320, 0, 240, 4, UI_THEME_ID_DEFAULT, def));
	CHECK(picks_on(inset_panel(240, 320, 10, 5, 4), UI_THEME_ID_DEFAULT, def));	// 220 x 310
	CHECK(picks_on(inset_panel(240, 320, 10, 5, 4), UI_THEME_ID_PANELS, panels));
	// 160 x 128, the compact layout, where the build has it (a panel that may be under 200 rows: GFX_MIN_ROWS); and
	// any display narrower than 220 px, however high: the full layout's tiles need that width
	const ui_theme_t *compact = GFX_MIN_ROWS < UI_TALL_PX ? panels : ref;
	CHECK(picks(170, 12, 240, 4, UI_THEME_ID_PANELS, compact));	// 158 across
	CHECK(picks(170, 13, 240, 4, UI_THEME_ID_DEFAULT, ref));	// 157 across
	CHECK(picks(170, 13, 240, 4, UI_THEME_ID_PANELS, ref));
	CHECK(picks(230, 11, 240, 4, UI_THEME_ID_PANELS, compact));	// 219 across
	CHECK(picks(230, 10, 240, 4, UI_THEME_ID_PANELS, panels));	// 220 across: the full layout
	CHECK(picks(220, 0, 199, 4, UI_THEME_ID_PANELS, compact));	// 199 high
	CHECK(picks(160, 0, 128, 4, UI_THEME_ID_PANELS, compact));
	CHECK(picks(160, 0, 128, 4, UI_THEME_ID_DEFAULT, GFX_MIN_ROWS < UI_TALL_PX ? def : ref));
	CHECK(picks(220, 0, 176, 4, UI_THEME_ID_PANELS, compact));
	CHECK(picks_on(inset_panel(160, 128, 1, 0, 4), UI_THEME_ID_PANELS, compact));	// 158 x 128
	CHECK(picks(160, 0, 127, 4, UI_THEME_ID_PANELS, compact));	// 127 high
	CHECK(picks(160, 0, 126, 4, UI_THEME_ID_PANELS, ref));		// 126 high
	CHECK(picks(160, 3, 128, 4, UI_THEME_ID_PANELS, ref));		// 157 across
	CHECK(picks_on(inset_panel(160, 120, 5, 0, 4), UI_THEME_ID_PANELS, ref));	// 150 x 120
	CHECK(picks(320, 0, 200, 4, UI_THEME_ID_PANELS, panels));	// any height from 128 (200: the full layout)
	CHECK(picks(220, 0, 200, 4, UI_THEME_ID_PANELS, panels));
#if GFX_MIN_ROWS < UI_TALL_PX
	// Which layout: the full one from 220 x 200, the compact one (its 18 px strip) on a narrower display however high
	static const struct { int w, h, strip; } layouts[] = {
		{ 220, 200, 29 }, { 320, 240, 29 }, { 219, 240, 18 }, { 158, 240, 18 }, { 220, 199, 18 }, { 158, 127, 18 },
	};
	for (unsigned i = 0; i < sizeof layouts / sizeof layouts[0]; i++) {
		hal_display_t d = disp;
		d.width = (uint16_t)layouts[i].w;
		d.height = (uint16_t)layouts[i].h;
		d.bpp = 4;
		d.flush_indexed = d_flush_indexed;
		CHECK(gfx_init(&d) && ui_theme_use(&ui_theme_panels) && ui_layout.strip_h == layouts[i].strip);
		ui_theme_use(&ui_theme_reference);
	}
	CHECK(gfx_init(&disp));
#endif
	CHECK(picks(320, 0, 240, 1, UI_THEME_ID_PANELS, ref));		// monochrome
	CHECK(picks_on(inset_panel(240, 320, 10, 5, 1), UI_THEME_ID_PANELS, ref));
	CHECK(picks(128, 0, 64, 1, UI_THEME_ID_PANELS, ref));
	// Its palettes are Day and Night, with nothing past them: a palette out of range is Day
	CHECK(ui_theme_panels.palette_count == 2 && ui_theme_panels.id == UI_THEME_ID_PANELS);
	// Its appearance options: Day/Night, the Home layout and the GPS clock, none of the reference theme's; and the
	// talkgroup's name first for a radio that starts in it; a bottom row of soft keys, kept with Hint row off
	CHECK(ui_theme_panels.options == (UI_THEME_OPT_DAY_NIGHT | UI_THEME_OPT_LAYOUT | UI_THEME_OPT_GPS_CLOCK |
					  UI_THEME_OPT_NAME_FIRST | UI_THEME_OPT_SOFT_KEYS));
	CHECK(!(ui_theme_reference.options & UI_THEME_OPT_SOFT_KEYS));
	CHECK(!(ui_theme_reference.options & UI_THEME_OPT_NAME_FIRST));
	// So a radio starts with it on where its display picks panels for the build's default, off where it picks the
	// reference theme, panels built in either way
	bool starts = CONFIG_UI_THEME_DEFAULT != UI_THEME_ID_REFERENCE;
	CHECK(name_first_on(320, 10, 240, 4) == starts && name_first_on(320, 0, 240, 4) == starts &&
	      name_first_on(220, 0, 310, 4) == starts &&
	      name_first_on(220, 0, 176, 4) == (starts && GFX_MIN_ROWS < UI_TALL_PX) &&
	      name_first_on(160, 0, 128, 4) == (starts && GFX_MIN_ROWS < UI_TALL_PX));
	CHECK(!name_first_on(157, 0, 240, 4) && !name_first_on(160, 0, 126, 4) && !name_first_on(320, 0, 240, 1));
	CHECK(ui_theme_use(&ui_theme_panels) && !ui_theme_has(UI_THEME_OPT_PALETTE) &&
	      ui_theme_has(UI_THEME_OPT_DAY_NIGHT));
	ui_theme_use(&ui_theme_reference);
	// Its scan mark moves every 100 ms while a scan runs (not paused: scan 3); nothing moves otherwise. The reference
	// theme has nothing that moves
	const ui_theme_ops_t *po = ui_theme_panels.ops;
	base();
	state.now_ms = 1000;
	view.scan = 1;
	CHECK(po->animate != NULL && !po->animate(&view, &state, 1099) && po->animate(&view, &state, 1100));
	view.scan = 3;
	CHECK(!po->animate(&view, &state, 1100));
	view.scan = 0;
	CHECK(!po->animate(&view, &state, 5000));
#else
	CHECK(picks(320, 10, 240, 4, UI_THEME_ID_DEFAULT, ref));
	CHECK(picks(320, 10, 240, 4, UI_THEME_ID_PANELS, ref));
#endif
	CHECK(ui_theme_reference.id == UI_THEME_ID_REFERENCE && ui_theme_reference.fits == NULL &&
	      ui_theme_reference.ops->animate == NULL);
	// The reference theme's appearance options: Theme, Highlight color, Filled status bar (Talkgroup name first is no
	// theme's option: it works in every theme)
	CHECK(ui_theme_reference.options == (UI_THEME_OPT_PALETTE | UI_THEME_OPT_ACCENT | UI_THEME_OPT_STRIP));
	CHECK(ui_theme_cur() == ref && ui_theme_has(UI_THEME_OPT_ACCENT) && !ui_theme_has(UI_THEME_OPT_LAYOUT));
}

#if CONFIG_UI_THEME_PANELS
// --- the panels theme's own frames on the display it is for: structure, not pixels, so the checks hold through layout
// changes. Built with GFX_FIT_TRACE (uiThemeTestPanels): every text drawn and every text shortened is seen here

static struct { int x, y, w, h; char s[48]; } texts[64];
static int ntexts, nshortened;

void gfx_fit_trace(const char *src, const char *out)
{
	(void)src;
	(void)out;
	nshortened++;
}

void gfx_draw_trace(gfx_trace_kind_t kind, int x, int y, int w, int h, const int clip[4], const char *s)
{
	(void)clip;
	if (kind == GFX_TRACE_CLEAR)
		ntexts = 0;
	if (kind != GFX_TRACE_TEXT || ntexts >= (int)(sizeof texts / sizeof texts[0]))
		return;
	texts[ntexts].x = x;
	texts[ntexts].y = y;
	texts[ntexts].w = w;
	texts[ntexts].h = h;
	snprintf(texts[ntexts].s, sizeof texts[0].s, "%s", s);
	ntexts++;
}

static int find_text(const char *s)
{
	for (int i = 0; i < ntexts; i++)
		if (strcmp(texts[i].s, s) == 0)
			return i;
	return -1;
}

// The panel the theme is for: 320 x 240 in 16 colors, its right edge 10 px behind a bezel
static hal_display_t panel = { .width = 320, .height = 240, .inset_right = 10, .bpp = 4, .init = d_init,
	.flush = d_flush, .flush_indexed = d_flush_indexed, .set_backlight = d_backlight };

// The first level of a menu with more entries than a screen: an icon, a digit and a short label each
static void menu_item(uint16_t i, ui_item_t *it)
{
	snprintf(it->label, sizeof it->label, "Entry %u", (unsigned)i);
	it->num = (char)('0' + (i + 1) % 10);
	it->icon = (uint8_t)(UI_MENU_ICON_AUDIO + i % (UI_MENU_ICON_COUNT - UI_MENU_ICON_AUDIO));
}

// The scroll position is drawn before the cells (the scroll op's first call), so on a 16-color display its color and its
// anti-aliased edges take palette entries ahead of the cells' (theme step 2 review, finding 1); the second call draws
// nothing. In a whole frame it is in the title's row, in its own color
static void test_panels_scroll_position(void)
{
	uint16_t top = 0;
	ui_list_t l = list(14, 3, &top);
	l.kind = UI_LIST_MENU;
	l.numbered = true;
	l.item = menu_item;
	base();
	const ui_theme_ops_t *o = ui_theme_panels.ops;
	int rows_y = o->list_head(NULL, NULL, &l, false);
	ui_list_geom_t g;
	o->list_geom(&l, rows_y, ui_layout.hint_y, &g);
	CHECK(g.rows * g.cols < l.count);
	gfx_clear(ui_col.bg);
	ntexts = 0;
	o->scroll(&l, &g, 0, l.count, false);
	CHECK(ntexts == 1 && find_text("4/14") == 0);
	ntexts = 0;
	o->scroll(&l, &g, 0, l.count, true);
	CHECK(ntexts == 0);

	ui_draw_list(&view, &state, &l);
	int b = find_text("4/14");
	CHECK(b >= 0);
	if (b < 0)
		return;
	CHECK(texts[b].y >= ui_layout.strip_h && texts[b].y + texts[b].h <= rows_y);
	int exact = 0;
	for (int y = texts[b].y; y < texts[b].y + texts[b].h; y++)
		for (int x = texts[b].x; x < texts[b].x + texts[b].w; x++)
			exact += gfx_get_pixel(x, y) == ui_col.dim;
	CHECK(exact > 0);
	// A list that fits on its screen has none
	l = list(3, 1, &top);
	ui_draw_list(&view, &state, &l);
	CHECK(find_text("2/3") < 0);
}

// The soft key bar: one label over each soft key, OK on the left and Back (or Home's soft key) on the right where the
// hint declares the key by structure (a group that starts with its name), never a label a translation chose; inside
// the bar's pane with room at its ends and between them, none shortened. The hint row above it in the 8x16 font: the
// groups the labels don't say (other keys, sentences, what OK and Back do), whole
static void bar(int line, const char *hint, unsigned flags, const char *soft_key, const char *want_left,
	const char *want_right, const char *want_row)
{
	gfx_clear(ui_col.bg);
	ntexts = 0;
	nshortened = 0;
	ui_theme_panels.ops->bottom(hint, flags, soft_key);
	int l = want_left != NULL ? find_text(want_left) : -1, r = want_right != NULL ? find_text(want_right) : -1;
	int w = want_row != NULL ? find_text(want_row) : -1;
	int want = (want_left != NULL) + (want_right != NULL) + (want_row != NULL);
	int bar_y = ui_layout.hint_y + ui_layout.small->h;	// the bar's pane, below the hint row
	bool ok = ntexts == want && nshortened == 0 && (want_left == NULL || l >= 0) && (want_right == NULL || r >= 0) &&
		(want_row == NULL || w >= 0);
	for (int i = 0; ok && i < ntexts; i++)
		ok = texts[i].x >= 10 && texts[i].x + texts[i].w <= gfx_width() - 10 && texts[i].h >= 16 &&
			texts[i].y >= (i == w ? ui_layout.hint_y : bar_y - 2) && texts[i].y + texts[i].h <= gfx_height() &&
			(i != w || texts[i].y + texts[i].h <= bar_y + 2);
	if (ok && l >= 0 && r >= 0)
		ok = texts[l].x + texts[l].w + 8 <= texts[r].x;
	if (!ok) {
		printf("FAIL %s:%d: bar \"%s\": %d texts, %d shortened:", __FILE__, line, hint, ntexts, nshortened);
		for (int i = 0; i < ntexts; i++)
			printf(" \"%s\" %d+%d %d+%d", texts[i].s, texts[i].x, texts[i].w, texts[i].y, texts[i].h);
		printf("\n");
		fails++;
	}
}
#define BAR(hint, flags, soft_key, left, right, row) bar(__LINE__, hint, flags, soft_key, left, right, row)

static void test_panels_bar(void)
{
	base();
	// In a list OK opens or picks the row and Back leaves: those groups say nothing more, only the other keys' do
	BAR("OK open  # reply  Back", UI_BAR_LIST, NULL, "OK", "Back", "# reply");
	BAR("OK change  Hold OK save  Back", UI_BAR_LIST, NULL, "OK", "Back", "Hold OK save");
	BAR("0-9 find  OK talk  # more", UI_BAR_LIST, NULL, "OK", "Back", "# more");	// Back leaves every list
	BAR("OK choose  Back", UI_BAR_LIST, NULL, "OK", "Back", NULL);			// nothing left: the row is empty
	// ... unless the focused row's value editor is open: OK and Back do its jobs then
	BAR("OK save  Back cancel", UI_BAR_LIST | UI_BAR_OK_ACTS | UI_BAR_BACK_ACTS, NULL, "OK", "Back",
	    "OK save  Back cancel");
	// ... or the list says its OK does more (send, go, add, use): its OK and Back groups stay
	BAR("OK send  Back", UI_BAR_LIST | UI_BAR_OK_ACTS, NULL, "OK", "Back", "OK send");
	// ... a key at a time: Back that keeps editing stays, the plain OK choose beside it goes
	BAR("OK choose  Back keep editing", UI_BAR_LIST | UI_BAR_BACK_ACTS, NULL, "OK", "Back", "Back keep editing");
	BAR("OK send  Back cancel", UI_BAR_LIST | UI_BAR_OK_ACTS, NULL, "OK", "Back", "OK send");
	// A row too long for the title's width (280 px) uses the panes' (298 px) from x 6 before it loses a group
	gfx_clear(ui_col.bg);
	ntexts = nshortened = 0;
	ui_theme_panels.ops->bottom("Back delete  # ABC/123/abc  OK finish", 0, NULL);
	int wide = find_text("Back delete  # ABC/123/abc  OK finish");
	CHECK(wide >= 0 && nshortened == 0 && texts[wide].x == 6 && texts[wide].w > gfx_width() - 30 &&
	      texts[wide].x + texts[wide].w <= gfx_width() - 6);
	// Hint row off: the two labels stay, the row goes
	BAR("OK open  # reply  Back", UI_BAR_LIST | UI_BAR_NO_HINTS, NULL, "OK", "Back", NULL);
	BAR("# skip  Back stop scan", UI_BAR_NO_HINTS, "Back", NULL, "Back", NULL);
	BAR("1-7 open  Back", UI_BAR_LIST | UI_BAR_NUMBERED, NULL, "OK", "Back", NULL);	// (no digits on this radio)
	BAR("OK save  Back cancel", 0, NULL, "OK", "Back", "OK save  Back cancel");	// the value editor
	BAR("OK  Back", 0, NULL, "OK", "Back", NULL);				// bare: the labels say it all
	BAR("OK acknowledge  Back hide", 0, NULL, "OK", "Back", "OK acknowledge  Back hide");
	BAR("0 All settings  Hold OK save", 0, NULL, NULL, NULL, "Hold OK save");	// hold OK: not a soft key
	BAR("# caller  Back end reply", 0, "Back", NULL, "Back", "# caller  Back end reply");	// Home's soft key
	BAR("0-9 freq  * Quick  Up/Dn", 0, "Quick", NULL, "Quick", "* Quick  Up/Dn");
	BAR("# skip  Back stop scan", 0, "", NULL, NULL, "# skip  Back stop scan");	// a soft key with no function
	BAR("PTT talk  # more  Back", UI_BAR_LIST, NULL, NULL, "Back", "PTT talk  # more");
	BAR("Back: channel's talkgroup", 0, NULL, NULL, "Back", "Back: channel's talkgroup");
	BAR("Keys locked: hold Back to unlock", 0, NULL, NULL, NULL, "Keys locked: hold Back to unlock");
	BAR("Release PTT to stop", 0, NULL, NULL, NULL, "Release PTT to stop");
	// The longest form whose row fits
	BAR(TEXT_ALT_OPEN "# find or ID  OK send now  Back from here to there and back" TEXT_ALT_NEXT
	    "# or ID  OK send  Back" TEXT_ALT_CLOSE, 0, NULL, "OK", "Back", "# or ID  OK send");
	// Home: its idle hint's first group is the menu's (OK opens it), Menu on the left; the rest in the hint row
	view.ch.name[0] = 0;
	state.home_hint = "OK Menu  * Quick";
	ui_draw_home(&view, &state);
	CHECK(find_text("Menu") >= 0 && find_text("* Quick") >= 0 && find_text("OK Menu") < 0);
	state.home_hint = "Menu  # Joe";
	ui_draw_home(&view, &state);
	int m = find_text("Menu"), j = find_text("# Joe");
	CHECK(m >= 0 && j >= 0 && texts[j].y < texts[m].y);
	// OK opens the menu on Home whatever the hint says (scanning, the reply window, dual watch): Menu on the left; the
	// state's own groups in the hint row
	state.scanning = true;
	ui_draw_home(&view, &state);
	CHECK(find_text("Menu") >= 0 && find_text("# skip  Back stop scan") >= 0);
	state.scanning = false;
	// Typing a frequency: OK tunes it
	state.entry = true;
	ui_draw_home(&view, &state);
	CHECK(find_text("Menu") < 0 && find_text("OK") >= 0);
	state.entry = false;
	// Keys locked: the bar has what unlocks them, in large text, in place of the labels; Hint row off too
	state.locked = true;
	for (int hints = 0; hints < 2; hints++) {
		state.hints = hints;
		ui_draw_home(&view, &state);
		int k = -1;
		for (int i = 0; i < ntexts; i++)
			if (strstr(texts[i].s, "unlock") != NULL)
				k = i;
		CHECK(k >= 0 && texts[k].h >= 24 && texts[k].y >= ui_layout.hint_y + 16 && find_text("Menu") < 0);
	}
	state.locked = false;
	// Hint row off: Home's bar keeps its two labels, the hint row goes
	state.hints = false;
	state.home_hint = "OK Menu  * Quick";
	ui_draw_home(&view, &state);
	CHECK(find_text("Menu") >= 0 && find_text("* Quick") < 0);
	state.hints = true;
	base();
}

// Text no smaller than 8x16 on any of the theme's frames, and a status strip whose items never overlap however many
// there are (the order they get room in: panels_draw_status)
static void test_panels_min_font(void)
{
	base();
	uint16_t top = 0;
	ui_list_t l = list(14, 3, &top);
	for (int frame = 0; frame < 8; frame++) {
		base();
		if (gfx_height() < UI_TALL_PX) {
			// A name the compact Home's channel font holds enlarged (its 12x24 capitals are 14 px of ink at 1x),
			// with the room for it: no volume bar
			strcpy(view.ch.name, "2M CALL");
			state.volbar = -1;
		}
		if (frame == 3 || frame == 4) {	// a crowded strip: TX, monitor, VOX, unread, lock, USB, GPS, scan, AES
			view.state = RS_TX;
			view.monitor = view.vox_on = true;
			state.unread = 2;
			state.locked = state.scanning = state.gps_fix = state.gps_clock = true;
			state.gps_utc_ymd = 20261126;
			state.gps_utc_hms = 140800;
			state.usb_net = UI_USB_NET_UP;
			state.gui_sim_crypt = 2;
			view.ch.rx_only = frame == 4;
		}
		if (frame == 6 || frame == 7) {	// Quick, its TX frequency tile editing in the VFO (frame 7)
			view.vfo = frame == 7;
			state.quick_focus = 4;
			state.quick_edit = frame == 7;
			ui_draw_quick(&view, &state);
		} else if (frame == 0 || frame == 3)
			ui_draw_home(&view, &state);
		else if (frame == 4)
			ui_draw_list(&view, &state, &l);
		else {
			l = list(14, 3, &top);
			l.kind = frame == 1 ? UI_LIST_HUB : frame == 2 ? UI_LIST_MENU : UI_LIST_PLAIN;
			l.item = menu_item;
			l.numbered = true;
			ui_draw_list(&view, &state, &l);
		}
		bool ok = true;
		for (int i = 0; i < ntexts; i++)
			if (texts[i].h < 16) {
				printf("FAIL %s:%d: frame %d: \"%s\" %d px high\n", __FILE__, __LINE__, frame, texts[i].s, texts[i].h);
				ok = false;
			}
		for (int i = 0; i < ntexts; i++)
			for (int k = i + 1; k < ntexts; k++)
				if (texts[i].y + texts[i].h / 2 < ui_layout.strip_h && texts[k].y + texts[k].h / 2 < ui_layout.strip_h &&
				    texts[i].x < texts[k].x + texts[k].w && texts[k].x < texts[i].x + texts[i].w) {
					printf("FAIL %s:%d: frame %d: \"%s\" over \"%s\" in the strip\n", __FILE__, __LINE__, frame,
					       texts[i].s, texts[k].s);
					ok = false;
				}
		if (!ok)
			fails++;
	}
	// The strip's words, VOX and RX among them, and dual watch's A and B are text, none smaller than 8x16 (until step 5
	// they were bitmap letters 10 to 14 px high)
	base();
	view.vox_on = view.ch.rx_only = true;
#if CONFIG_DUAL_WATCH
	state.dual.on = true;
#endif
	l = list(14, 3, &top);
	ui_draw_list(&view, &state, &l);
	CHECK(find_text("VOX") >= 0 && find_text("RX") >= 0);
#if CONFIG_DUAL_WATCH
	CHECK(find_text("A") >= 0 && find_text("B") >= 0);
#endif
	for (int i = 0; i < ntexts; i++)
		if (texts[i].h < 16) {
			printf("FAIL %s:%d: \"%s\" %d px high\n", __FILE__, __LINE__, texts[i].s, texts[i].h);
			fails++;
		}
	base();
}

// Quick's TX frequency tile shows the digits typed so far ("146.5" then the places left), not a word for them (the
// first layout attempt showed "TX"); the hint row has them too. On the narrow screen, where the places don't fit
// beside the digit, the tile has the typed digits alone
static void test_panels_quick_entry(bool narrow)
{
	base();
	view.vfo = true;
	state.quick_focus = 4;	// tile 5: TX frequency in the VFO
	state.quick_edit = true;
	state.entry_len = 4;
	memcpy(state.entry_digits, "1465", 5);
	ui_draw_quick(&view, &state);
	int tile = find_text(narrow ? "146.5" : "146.5____"), row = -1;
	for (int i = 0; i < ntexts; i++)
		if (strstr(texts[i].s, "TX 146.5") != NULL)
			row = i;
	CHECK(tile >= 0 && texts[tile].y < ui_layout.hint_y && texts[tile].h >= 16);
	CHECK(!narrow || find_text("146.5____") < 0);
	CHECK(row >= 0 && texts[row].y >= ui_layout.hint_y);
	base();
}

// Lists show five rows of one height between the title and the bar; the main menu is a grid of three columns in the
// keypad's order (1 2 3 across), the first level of Settings one of two columns down, a page of three rows each, one
// split for the whole list
static void test_panels_geometry(void)
{
	base();
	uint16_t top = 0;
	ui_list_t l = list(30, 0, &top);
	const ui_theme_ops_t *o = ui_theme_panels.ops;
	ui_list_geom_t g;
	int y = o->list_head(NULL, NULL, &l, false);
	o->list_geom(&l, y, ui_layout.hint_y, &g);
	CHECK(g.rows == 5 && g.cols == 1 && !g.paged && g.font->h >= 16 && y + 5 * g.pitch <= ui_layout.hint_y + 4);
	l.kind = UI_LIST_HUB;
	o->list_geom(&l, y, ui_layout.hint_y, &g);
	CHECK(g.rows == 3 && g.cols == 3 && g.across && g.paged && g.col_w[0] == g.col_w[1] &&
	      g.y + 3 * g.pitch <= ui_layout.hint_y && g.pitch >= 2 * g.font->h);
	l.kind = UI_LIST_MENU;
	o->list_geom(&l, y, ui_layout.hint_y, &g);
	CHECK(g.rows == 3 && g.cols == 2 && !g.across && g.paged && g.pitch >= 2 * g.font->h);
	// Without the bar (Hint row off) a list runs to the screen's foot
	l.kind = UI_LIST_PLAIN;
	o->list_geom(&l, y, gfx_height(), &g);
	CHECK(g.rows == (gfx_height() - y) / g.pitch);
}

// The main menu's tiles: one with a label of two forms, one with a value (Scan's "Off")
// Settings' groups: an icon, a digit and a label of up to two lines' words
static void group_tile_item(uint16_t i, ui_item_t *it)
{
	static const char *const labels[] = { "Display", "Tones & alerts", "Audio", "Call settings", "Receive & VFO",
		"Privacy", "Security & keypad lock", "Buttons", "Programming" };
	it->num = (char)('1' + i);
	it->icon = UI_MENU_ICON_SCAN;
	strcpy(it->label, labels[i % 9]);
}

// The main menu with the Zones tile first, its value the zone's name
static void zones_tile_item(uint16_t i, ui_item_t *it)
{
	it->num = (char)('1' + i);
	it->icon = i == 0 ? UI_MENU_ICON_ZONES : UI_MENU_ICON_SCAN;
	snprintf(it->label, sizeof it->label, i == 0 ? "Zones" : "Entry %u", (unsigned)i);
	if (i == 0)
		strcpy(it->value, "Club");
}

static void hub_tile_item(uint16_t i, ui_item_t *it)
{
	it->num = (char)('1' + i);
	it->icon = UI_MENU_ICON_SCAN;
	if (i == 0) {
		strcpy(it->label, TEXT_ALT_OPEN "Connections" TEXT_ALT_NEXT "Links" TEXT_ALT_CLOSE);
	} else if (i == 1) {
		strcpy(it->label, "Scan");
		strcpy(it->value, "Off");
	} else {
		snprintf(it->label, sizeof it->label, "Entry %u", (unsigned)i);
	}
}

// The grids' tiles and the rows from the screen's size. On the narrow screen (220 px across): tiles 2 px from the edges
// and apart, the label of two forms in its short one, a value that doesn't fit beside the digit on a third line under
// the label, seven list rows. On the wide one: the full margins, the long form, the value beside the digit
static void test_panels_tiles(bool narrow)
{
	base();
	uint16_t top = 0;
	ui_list_t l = list(9, 4, &top);
	l.kind = UI_LIST_HUB;
	l.numbered = true;
	l.item = hub_tile_item;
	l.prefix = "Menu";
	l.name = NULL;
	nshortened = 0;
	ui_draw_list(&view, &state, &l);
	int links = find_text("Links"), conn = find_text("Connections");
	CHECK(narrow ? links >= 0 && conn < 0 : conn >= 0 && links < 0);
	int scan = find_text("Scan"), off = find_text("Off"), digit = find_text("2 ");
	CHECK(scan >= 0 && off >= 0 && digit >= 0 && nshortened == 0);
	if (scan >= 0 && off >= 0 && digit >= 0) {
		if (narrow)
			CHECK(texts[off].x == texts[scan].x && texts[off].y == texts[scan].y + texts[scan].h &&
			      texts[off].y + texts[off].h <= ui_layout.hint_y);
		else
			CHECK(texts[off].y == texts[digit].y && texts[off].y < texts[scan].y);
	}
	const ui_theme_ops_t *o = ui_theme_panels.ops;
	ui_list_geom_t g;
	int y = o->list_head(NULL, NULL, &l, false);
	o->list_geom(&l, y, ui_layout.hint_y, &g);
	if (narrow)
		CHECK(g.x == 2 && g.col_gap == 2 && g.col_w[0] == 70 && g.pitch == 65);
	else
		CHECK(g.x == 5 && g.col_gap == 3 && g.col_w[0] == 98);
	// The focused tile's label starts 1 px clear of the accent bar (4 px), its room kept; the others' where they were
	int col1 = g.x + g.col_w[0] + g.col_gap;
	l.focus = 0;
	nshortened = 0;
	ui_draw_list(&view, &state, &l);
	links = find_text(narrow ? "Links" : "Connections");
	scan = find_text("Scan");
	CHECK(links >= 0 && scan >= 0 && nshortened == 0);
	if (links >= 0 && scan >= 0)
		CHECK(texts[links].x == g.x + (narrow ? 5 : 7) && texts[scan].x == col1 + (narrow ? 3 : 7));
	// Settings' group tiles: on the narrow screen the icon and digit above the label, which takes the tile's width on
	// up to two lines, so every group's name is whole (the wide screen keeps the label beside the icon)
	for (uint16_t f = 0; narrow && f < 9; f += 6) {
		l = list(9, f, &top);
		l.kind = UI_LIST_MENU;
		l.numbered = true;
		l.item = group_tile_item;
		l.prefix = "Settings";
		l.name = NULL;
		top = f;
		nshortened = 0;
		ui_draw_list(&view, &state, &l);
		CHECK(nshortened == 0);
		if (f == 0) {
			int call = find_text("Call"), dig = find_text("4");
			CHECK(call >= 0 && find_text("settings") >= 0 && find_text("Tones &") >= 0 && find_text("alerts") >= 0 &&
			      find_text("Receive &") >= 0 && find_text("VFO") >= 0 && dig >= 0);
			if (call >= 0 && dig >= 0)
				CHECK(texts[call].y > texts[dig].y && texts[call].x < texts[dig].x);
		} else {
			CHECK(find_text("Security &") >= 0 && find_text("keypad lock") >= 0 && find_text("Programming") >= 0);
		}
	}
	// Lists: rows of 26 px, seven on the narrow screen in 8x16 (its rows hold too few characters of the small font),
	// five on the wide one in the small font
	l = list(30, 0, &top);
	o->list_geom(&l, o->list_head(NULL, NULL, &l, false), ui_layout.hint_y, &g);
	CHECK(g.rows == (narrow ? 7 : 5) && g.pitch == 26 && g.font->h == (narrow ? 16 : ui_layout.small->h));
}

// Text that falls back to 8x16 on the narrow screen where its larger font doesn't hold it whole: the keys-locked
// sentence in the bar, Amateur Home's details row
static void test_panels_narrow_text(bool narrow)
{
	base();
	state.locked = true;
	ui_draw_home(&view, &state);
	int k = -1;
	for (int i = 0; i < ntexts; i++)
		if (strstr(texts[i].s, "to unlock") != NULL)
			k = i;
	CHECK(k >= 0 && strchr(texts[k].s, GLYPH_ELLIPSIS[0]) == NULL && (narrow ? texts[k].h == 16 : texts[k].h >= 24));
#if CONFIG_DMR
	base();
	state.gui_profile = 1;
	view.ch.mode = RADIO_MODE_DMR;
	view.ch.rx_hz = view.ch.tx_hz = 453625000;
	view.ch.timeslot = 1;
	view.ch.color_code = 1;
	ui_draw_home(&view, &state);
	int d = -1;
	for (int i = 0; i < ntexts; i++)
		if (strstr(texts[i].s, "453.625 MHz") != NULL)
			d = i;
	CHECK(ui_layout.small->h > 16);
	CHECK(d >= 0 && strstr(texts[d].s, "CC1") != NULL && texts[d].h == (narrow ? 16 : ui_layout.small->h));
#endif
	base();
}

// A list with a body: its title keeps room for the scroll position, which shows once the body has taken rows (on the
// narrow screen a long title ran under it)
static void test_panels_badge_room(void)
{
	base();
	uint16_t top = 0;
	ui_list_t l = list(5, 0, &top);
	l.prefix = "Message 1 of 1";
	l.name = NULL;
	l.body = "From Q1ABC Joe, 0s ago. Your special configuration is: active SMS long SMS, delay 2.5 s, retries 3, "
		 "language English";
	ui_draw_list(&view, &state, &l);
	int b = find_text("1/5"), t = -1;
	for (int i = 0; i < ntexts; i++)
		if (strncmp(texts[i].s, "Message", 7) == 0)
			t = i;
	CHECK(b >= 0 && t >= 0);
	if (b >= 0 && t >= 0)
		CHECK(texts[t].x + texts[t].w <= texts[b].x);
	base();
}

// --- the compact layout (a display under 200 px high: 160 x 128) against the full one (310 x 240)

// The rows a plain list of 30 shows under the bottom row's flags (ui_list_bar, as ui_draw_list sets them)
static int list_rows(const char *hint, unsigned flags)
{
	uint16_t top = 0;
	ui_list_t l = list(30, 0, &top);
	l.hint = hint;
	ui_list_bar = flags;
	const ui_theme_ops_t *o = ui_theme_panels.ops;
	ui_list_geom_t g;
	o->list_geom(&l, o->list_head(NULL, NULL, &l, false), ui_layout.hint_y, &g);
	ui_list_bar = 0;
	return g.rows;
}

static bool edit_focus;
static void edit_item(uint16_t i, ui_item_t *it)
{
	item(i, it);
	if (edit_focus && i == 0)
		it->flags |= UI_ITEM_EDIT;
}

// The rows ui_draw_list draws: i0, i1, ... on screen
static int drawn_rows(const char *hint)
{
	uint16_t top = 0;
	ui_list_t l = list(30, 0, &top);
	l.hint = hint;
	l.item = edit_item;
	ui_draw_list(&view, &state, &l);
	int n = 0;
	char name[8];
	for (int i = 0; i < 30; i++) {
		snprintf(name, sizeof name, "i%d", i);
		n += find_text(name) >= 0;
	}
	return n;
}

// The texts of the last frame, for a failed check
static void dump_texts(int line)
{
	printf("  texts at line %d (%d x %d):", line, gfx_width(), gfx_height());
	for (int i = 0; i < ntexts; i++)
		printf(" \"%s\" %d,%d+%d", texts[i].s, texts[i].x, texts[i].y, texts[i].h);
	printf("\n");
}
#define CHECKT(c) do { if (!(c)) { CHECK(c); dump_texts(__LINE__); } } while (0)

static int text_with(const char *part)
{
	for (int i = 0; i < ntexts; i++)
		if (strstr(texts[i].s, part) != NULL)
			return i;
	return -1;
}

// A hidden or empty hint row gives the compact layout's list its room: 4 rows instead of 3. The full layout keeps 5
// rows either way, as frozen
static void test_compact_rows(bool compact)
{
	base();
	int few = compact ? 3 : 5, more = compact ? 4 : 5;
	CHECKT(list_rows("OK change  Hold OK save  Back", UI_BAR_LIST) == few);	// the row has "Hold OK save"
	CHECKT(list_rows("OK choose  Back", UI_BAR_LIST) == more);		// the bar says both groups: empty
	CHECKT(list_rows("OK change  Hold OK save  Back", UI_BAR_LIST | UI_BAR_NO_HINTS) == more);	// Hint row off
	CHECKT(list_rows(NULL, UI_BAR_LIST) == more);
	CHECKT(list_rows("OK save  Back cancel", UI_BAR_LIST | UI_BAR_OK_ACTS | UI_BAR_BACK_ACTS) == few);	// an editor
	// ui_draw_list sets the flags before the head and the rows: Hint row off, and the focused row's open value editor,
	// whose hint has a row
	CHECKT(drawn_rows("OK choose  Back") == more);
	state.hints = false;
	CHECKT(drawn_rows("OK change  Hold OK save  Back") == more);
	state.hints = true;
	edit_focus = true;
	CHECKT(drawn_rows("OK save  Back cancel") == few);
	edit_focus = false;
	base();
}

// The compact layout: Home's mode word in the strip (no mode bar), its strip's marks inside the strip (the 16 px
// icons), no GPS clock; the keys-locked bar's short form; a list label and a title shortened at their end. The full
// layout: the mode in its mode bar, the clock in the strip
static void test_compact_home(bool compact)
{
	base();
	state.hints = false;
	state.locked = true;
	state.gps_fix = state.gps_clock = true;
	state.gps_utc_ymd = 20261127;
	state.gps_utc_hms = 140500;
	ui_draw_home(&view, &state);
	int fm = find_text("FM"), clock = text_with("Nov"), k = text_with("unlock");
	CHECKT(fm >= 0 && (compact ? texts[fm].y + texts[fm].h <= ui_layout.strip_h :
			   texts[fm].y + texts[fm].h / 2 > ui_layout.strip_h));
	CHECKT(compact ? clock < 0 : clock >= 0);
	CHECKT(k >= 0 && strcmp(texts[k].s, compact ? "Hold * to unlock" : "Locked: hold * to unlock") == 0);
	if (compact) {
		// The row under the strip is Home's gutter only: no mark (the lock) reaches past the strip's pane
		color_t gutter = gfx_get_pixel(0, ui_layout.strip_h);
		bool clear = true;
		for (int x = 0; x < gfx_width(); x++)
			clear = clear && gfx_get_pixel(x, ui_layout.strip_h) == gutter;
		CHECKT(clear && ui_layout.strip_h == 18);
	}
	base();
	uint16_t top = 0;
	ui_list_t l = list(5, 0, &top);
	l.prefix = "Incoming private call to";
	l.name = NULL;
	ui_draw_list(&view, &state, &l);
	int t = text_with("Incoming");
	CHECKT(t >= 0 && (compact ? strcmp(texts[t].s + strlen(texts[t].s) - 1, GLYPH_ELLIPSIS) == 0 :
			 strcmp(texts[t].s, "Incoming private call to") == 0));
	// A body's last line, shortened over a line break, has a space there, not a missing glyph
	l = list(5, 0, &top);
	l.body = "Site 1 of 3: COUNTY DMR\nSignal -92 dBm\nBeacons: learning, more of them here\nThe rest";
	ui_draw_list(&view, &state, &l);
	bool breaks = false;
	for (int i = 0; i < ntexts; i++)
		breaks = breaks || strchr(texts[i].s, '\n') != NULL;
	CHECKT(!breaks && text_with("Site 1") >= 0);
	base();
}

#if CONFIG_DMR
// Amateur Home on DMR: the zone, the channel, the target and the details row while idle; during a call the compact
// layout's call lines take the room of the target and details rows, which go, and the zone and the channel stay. The
// full layout keeps the lines' room always, and its target during a call
static void test_compact_call(bool compact)
{
	base();
	state.volbar = -1;	// no volume bar: the pane runs to the bar
	// Hint row on: the compact Home has no hint line ("* Quick"), so the layouts keep their room; the full one has it
	ui_draw_home(&view, &state);
	CHECKT(compact ? find_text("* Quick") < 0 && find_text("Menu") >= 0 : find_text("* Quick") >= 0);
	state.gui_profile = 1;
	view.ch.mode = RADIO_MODE_DMR;
	view.ch.timeslot = 1;
	view.ch.color_code = 1;
	view.dmr.active = true;
	view.dmr.target_id = 3100;
	strcpy(view.dmr.target_name, "USA");
	ui_draw_home(&view, &state);
	CHECKT(find_text("Simplex") >= 0 && text_with("3100") >= 0 && text_with("146.520 MHz") >= 0);
	view.state = RS_RX;
	view.dmr.state = RDMR_RX;
	view.dmr.call.group = true;
	view.dmr.call.dst = 91;
	strcpy(view.dmr.call.src_name, "Joe");
	ui_draw_home(&view, &state);
	int joe = find_text("Joe"), zone = find_text("Simplex"), target = text_with("3100");
	CHECKT(joe >= 0 && zone >= 0 && text_with("TG 91") >= 0);
	CHECKT(compact ? target < 0 && text_with("146.520 MHz") < 0 : target >= 0);
	if (joe >= 0 && zone >= 0)
		CHECKT(texts[zone].y + texts[zone].h <= texts[joe].y);
	base();
}

// No two texts drawn share a pixel
static bool texts_apart(void)
{
	for (int i = 0; i < ntexts; i++)
		for (int j = i + 1; j < ntexts; j++)
			if (texts[i].x < texts[j].x + texts[j].w && texts[j].x < texts[i].x + texts[i].w &&
			    texts[i].y < texts[j].y + texts[j].h && texts[j].y < texts[i].y + texts[i].h)
				return false;
	return true;
}

// Home on a channel whose body is the reference theme's (an SFR repeater here; trunked and unbuilt kinds go the same
// way): its lines apart, the activity line under them. The compact layout has no hint line there either, its bar
// "Menu", and a zone name too long beside the position is cut at its end; a call has the reference call screen there
static void test_compact_operational_home(bool compact)
{
	base();
	state.volbar = -1;
	view.ch.mode = RADIO_MODE_DMR;
	view.dmr.codec = true;
	view.dmr.sfr_rpt = true;
	strcpy(view.zone_name, "Local repeaters");
	ui_draw_home(&view, &state);
	int act = find_text("No activity yet"), sfr = text_with("SFR repeater");
	CHECKT(texts_apart() && act >= 0 && sfr >= 0 && texts[sfr].y < texts[act].y);
	CHECKT(compact ? text_with("Quick") < 0 && find_text("Menu") >= 0 : text_with("* Quick") >= 0);
	int zone = text_with("Local");
	CHECKT(zone >= 0 && (strstr(texts[zone].s, GLYPH_ELLIPSIS) == NULL ||
			     strcmp(texts[zone].s + strlen(texts[zone].s) - 1, GLYPH_ELLIPSIS) == 0));
	view.dmr.sfr_rpt = false;
	view.dmr.active = true;
	view.state = RS_RX;
	view.dmr.state = RDMR_RX;
	view.dmr.call.group = true;
	view.dmr.call.dst = 91;
	strcpy(view.dmr.call.src_name, "Joe");
	view.ch.kind = CODEPLUG_KIND_DMR_TRUNK;
	ui_draw_home(&view, &state);
	CHECKT(texts_apart() && find_text("Joe") >= 0 && text_with("TG 91") >= 0 &&
	       (compact ? find_text("Menu") >= 0 && text_with("PTT") < 0 : true));
	// Keying it: the call screen's transmit states (waiting for the grant, admitted) stay, under the bar
	view.state = RS_TX;
	view.dmr.target_id = 3100;
	for (int k = 0; k < 2; k++) {
		view.dmr.state = k ? RDMR_TX_PERMIT : RDMR_TX_WAIT;
		ui_draw_home(&view, &state);
		CHECKT(texts_apart() && find_text(k ? "ADMITTED" : "WAITING") >= 0 && text_with("3100") >= 0 &&
		       (compact ? find_text("Menu") >= 0 && text_with("Release") < 0 : text_with("Release") >= 0));
	}
	base();
}
#endif

// The compact grids: the Menu's tiles have the icon and digit, the title row the focused tile's label and value; Quick's
// tiles their value, the title row the focused tile's name, the hint row without its last group. The full ones have
// the labels and names in the tiles
static void test_compact_grids(bool compact)
{
	base();
	uint16_t top = 0;
	ui_list_t l = list(9, 1, &top);
	l.kind = UI_LIST_HUB;
	l.numbered = true;
	l.item = hub_tile_item;
	l.prefix = "Menu";
	l.name = NULL;
	ui_draw_list(&view, &state, &l);
	int scan = find_text("Scan"), off = find_text("Off"), entry = find_text("Entry 2"), menu = find_text("Menu");
	CHECKT(scan >= 0 && off >= 0);
	if (scan >= 0 && off >= 0) {
		int title_y = ui_layout.strip_h;
		CHECKT(compact ? texts[scan].y < title_y + 17 && texts[off].y == texts[scan].y && entry < 0 &&
		      (menu < 0 || texts[menu].y >= ui_layout.hint_y) : texts[scan].y > title_y + 30 && entry >= 0);
	}
	base();
	ui_draw_quick(&view, &state);
	int power = text_with("Power");
	CHECKT(power >= 0 && (compact ? texts[power].y < ui_layout.strip_h + 17 : texts[power].y > ui_layout.strip_h + 30));
#if CONFIG_SUBTONE
	// A focused tile's value too long for it beside the accent bar (a TX-only tone, "T100.0"): shortened at its end
	// in the compact tile, whole after the name in the title row
	view.ch.tx_tone.type = TONE_CTCSS;
	view.ch.tx_tone.value = 1000;
	state.quick_focus = 2;
	ui_draw_quick(&view, &state);
	int whole = find_text("T100.0");
	CHECKT(whole >= 0 && (compact ? texts[whole].y < ui_layout.strip_h + 17 && text_with("T100" GLYPH_ELLIPSIS) >= 0 :
			      texts[whole].y > ui_layout.strip_h + 30));
	state.quick_focus = 0;
#endif
	// The hint row drops its last groups, whole, where it runs out of room
	gfx_clear(ui_col.bg);
	ntexts = 0;
	ui_theme_panels.ops->bottom("# reply  PTT talk  Hold OK save", 0, NULL);
	CHECKT(find_text(compact ? "# reply  PTT talk" : "# reply  PTT talk  Hold OK save") >= 0);
	if (!compact) {
		base();
		return;
	}
	// A toast of three forms whose first form two lines can't hold (a longer translation) takes its shortest, on one
	// line ("Hold * to unlock", the compact bar's)
	gfx_clear(ui_col.bg);
	ntexts = 0;
	ui_theme_panels.ops->bottom(TEXT_ALT_OPEN "Keys are locked now: hold the star key down to unlock them" TEXT_ALT_NEXT
				    "Locked: hold * to unlock" TEXT_ALT_NEXT "Hold * to unlock" TEXT_ALT_CLOSE, UI_BAR_TOAST, NULL);
	CHECKT(find_text("Hold * to unlock") >= 0 && text_with("Keys") < 0 && ntexts == 1);
	// ... and only there: English's first form fits two lines, so it keeps its words
	gfx_clear(ui_col.bg);
	ntexts = 0;
	ui_theme_panels.ops->bottom(TEXT_ALT_OPEN "Keys locked: hold * to unlock" TEXT_ALT_NEXT "Locked: hold * to unlock"
				    TEXT_ALT_NEXT "Hold * to unlock" TEXT_ALT_CLOSE, UI_BAR_TOAST, NULL);
	CHECKT(find_text("Keys locked: hold") >= 0 && find_text("* to unlock") >= 0 && ntexts == 2);
	// The Zones tile's value (the zone's name) is not in the title row when it is focused; Home shows the zone
	l = list(9, 0, &top);
	l.kind = UI_LIST_HUB;
	l.numbered = true;
	l.item = zones_tile_item;
	l.prefix = "Menu";
	l.name = NULL;
	ui_draw_list(&view, &state, &l);
	CHECKT(find_text("Zones") >= 0 && text_with("Club") < 0);
	// Quick's tiles take the hint row's room with Hint row off: its last row lower than with it on
	base();
	int low[2];
	for (int hints_off = 0; hints_off < 2; hints_off++) {
		state.hints = !hints_off;
		ui_draw_quick(&view, &state);
		low[hints_off] = 0;
		for (int i = 0; i < ntexts; i++)
			if (texts[i].y + texts[i].h <= gfx_height() - 20 && texts[i].y > low[hints_off] &&	// above the bar
			    (hints_off || texts[i].y + texts[i].h <= ui_layout.hint_y))
				low[hints_off] = texts[i].y;
	}
	CHECKT(low[1] > low[0]);
	base();
}

// A list whose rows have long labels: "Timeout warning tone" and the like, each with "On"
static void long_item(uint16_t i, ui_item_t *it)
{
	snprintf(it->label, sizeof it->label, "Timeout warning tone %u", (unsigned)i);
	strcpy(it->value, "On");
}

#if GFX_MIN_ROWS < UI_TALL_PX
// A row whose label has a shorter form, beside a value that takes most of the row (Settings > Display > Profile)
static void alt_item(uint16_t i, ui_item_t *it)
{
	(void)i;
	strcpy(it->label, TEXT_ALT_OPEN "Display profile" TEXT_ALT_NEXT "Profile" TEXT_ALT_CLOSE);
	strcpy(it->value, "Commercial");
}

// No text drawn is shortened in the middle: an ellipsis only at its end
static bool no_middle_cut(void)
{
	for (int i = 0; i < ntexts; i++) {
		const char *e = strstr(texts[i].s, GLYPH_ELLIPSIS);
		if (e != NULL && e[1] != 0)
			return false;
	}
	return true;
}
#endif

// The compact layout's focused row whose label is cut slides it sideways: shortened at its end for a second, then 2 px
// every 100 ms, the animate op asking for a frame at each step, until its end shows; the other rows stay shortened.
// The full layout has room for it
static void test_compact_marquee(bool compact)
{
	base();
	uint16_t top = 0;
	ui_list_t l = list(5, 1, &top);
	l.item = long_item;
	uint32_t t0 = state.now_ms;
	ui_draw_list(&view, &state, &l);
	int k = text_with("Timeout"), whole = find_text("Timeout warning tone 1");
	CHECKT(compact ? k >= 0 && whole < 0 && strstr(texts[k].s, GLYPH_ELLIPSIS) != NULL : whole >= 0);
	if (!compact) {
		base();
		return;
	}
	const ui_theme_ops_t *o = ui_theme_panels.ops;
	CHECK(!o->animate(&view, &state, t0 + 999) && o->animate(&view, &state, t0 + 1000));
	state.now_ms = t0 + 1550;	// 5 steps in: 10 px
	ui_draw_list(&view, &state, &l);
	whole = find_text("Timeout warning tone 1");
	int x0 = 3 + 8;	// the row's text: the list 3 px from the edge, the text 8 px into the row
	// The value stays on the sliding row: an "On" on its line, not only on the other rows'
	bool on_row = false;
	for (int i = 0; whole >= 0 && i < ntexts; i++)
		on_row = on_row || (strcmp(texts[i].s, "On") == 0 && texts[i].y == texts[whole].y);
	CHECKT(whole >= 0 && texts[whole].x == x0 - 10 && on_row);
	CHECKT(find_text("Timeout warning tone 0") < 0 && find_text("Timeout warning tone 2") < 0);	// shortened
	CHECK(!o->animate(&view, &state, t0 + 1599) && o->animate(&view, &state, t0 + 1600));
	// Its end shows and rests: "Timeout warning tone 1" is 176 px in a room of 120
	state.now_ms = t0 + 1000 + 28 * 100 + 500;
	ui_draw_list(&view, &state, &l);
	whole = find_text("Timeout warning tone 1");
	CHECKT(whole >= 0 && texts[whole].x == x0 - (176 - 120));
	// A new focus starts again, shortened
	l.focus = 2;
	ui_draw_list(&view, &state, &l);
	CHECKT(find_text("Timeout warning tone 2") < 0);
	base();
}

// The bottom row's flags a theme reads (ui_list_bar) are the list's own when it is only measured, not the last list
// drawn's: here a list drawn with Hint row off, then another measured with it on and its own flags
static void test_list_bar_measured(void)
{
	base();
	uint16_t top = 0;
	ui_list_t l = list(5, 0, &top);
	state.hints = false;
	ui_draw_list(&view, &state, &l);
	CHECK(ui_list_bar & UI_BAR_NO_HINTS);
	state.hints = true;
	l.numbered = true;
	l.ok_acts = true;
	l.body = "A line";
	CHECK(ui_list_body_fits(&state, &l));
	CHECK(ui_list_bar == (UI_BAR_LIST | UI_BAR_NUMBERED | UI_BAR_OK_ACTS));
	base();
}

static void test_panels_frames(void)
{
	CHECK(gfx_init(&panel));
	CHECK(ui_theme_use(&ui_theme_panels));
	ui_theme_set(UI_PALETTE_DAY);
	test_panels_scroll_position();
	test_panels_bar();
	test_panels_min_font();
	test_panels_quick_entry(false);
	test_panels_geometry();
	test_panels_tiles(false);
	test_panels_narrow_text(false);
	test_panels_badge_room();
	// The 240 x 320 panel behind a bezel that hides 10 px left and right and 5 top and bottom: 220 x 310
	hal_display_t narrow = inset_panel(240, 320, 10, 5, 4);
	narrow.flush_indexed = d_flush_indexed;
	CHECK(gfx_init(&narrow));
	CHECK(ui_theme_use(&ui_theme_panels));
	ui_theme_set(UI_PALETTE_DAY);
	CHECK(gfx_width() == 220 && gfx_height() == 310);
	test_panels_tiles(true);
	test_panels_narrow_text(true);
	test_panels_quick_entry(true);
	test_panels_badge_room();
	// The full layout where the compact one differs
	CHECK(gfx_init(&panel));
	CHECK(ui_theme_use(&ui_theme_panels));
	ui_theme_set(UI_PALETTE_DAY);
	test_compact_rows(false);
	test_compact_home(false);
#if CONFIG_DMR
	test_compact_call(false);
	test_compact_operational_home(false);
#endif
	test_compact_grids(false);
	test_compact_marquee(false);
	test_list_bar_measured();
#if GFX_MIN_ROWS < UI_TALL_PX
	// The 160 x 128 panel: the compact layout
	hal_display_t compact = { .width = 160, .height = 128, .bpp = 4, .init = d_init, .flush = d_flush,
		.flush_indexed = d_flush_indexed, .set_backlight = d_backlight };
	CHECK(gfx_init(&compact));
	CHECK(ui_theme_use(&ui_theme_panels));
	ui_theme_set(UI_PALETTE_DAY);
	test_compact_rows(true);
	test_compact_home(true);
#if CONFIG_DMR
	test_compact_call(true);
	test_compact_operational_home(true);
#endif
	test_compact_grids(true);
	test_compact_marquee(true);
	test_panels_min_font();
	// 158 x 127 (a 160 x 128 panel behind a bezel that hides a row and two columns): 3 list rows, the hint row empty
	// or not (4 need 128 px)
	hal_display_t bezel = compact;
	bezel.inset_left = 2;
	bezel.inset_top = 1;
	CHECK(gfx_init(&bezel));
	CHECK(ui_theme_use(&ui_theme_panels));
	CHECK(gfx_width() == 158 && gfx_height() == 127 && ui_layout.strip_h == 18);
	CHECK(list_rows("OK choose  Back", UI_BAR_LIST) == 3 && list_rows("OK change  Hold OK save  Back", UI_BAR_LIST) == 3);
	ui_theme_set(UI_PALETTE_DAY);
	// A label with a shorter form where not even that fits ("Profile" beside "Commercial" is 2 px too wide here):
	// cut at its end as any label, never in the middle, and the focused row's slides to show it whole
	base();
	uint16_t top = 0;
	ui_list_t l = list(3, 1, &top);
	l.item = alt_item;
	uint32_t t0 = state.now_ms;
	ui_draw_list(&view, &state, &l);
	CHECKT(no_middle_cut() && find_text("Profi" GLYPH_ELLIPSIS) >= 0 && find_text("Profile") < 0 &&
	       text_with("Display") < 0 && find_text("Commercial") >= 0);
	state.now_ms = t0 + 1550;
	ui_draw_list(&view, &state, &l);
	int whole = find_text("Profile");
	CHECKT(no_middle_cut() && whole >= 0 && texts[whole].x < 3 + 8 && find_text("Profi" GLYPH_ELLIPSIS) >= 0);
	base();
#if CONFIG_DMR
	test_compact_operational_home(true);
#endif
	// No GPS clock in the compact layout, even where a quiet strip would have room for it (220 x 176)
	compact.width = 220;
	compact.height = 176;
	CHECK(gfx_init(&compact));
	CHECK(ui_theme_use(&ui_theme_panels));
	base();
	state.gps_fix = state.gps_clock = true;
	state.gps_utc_ymd = 20261127;
	state.gps_utc_hms = 140500;
	ui_draw_home(&view, &state);
	CHECK(find_text("FM") >= 0 && text_with("Nov") < 0);
	base();
#endif
	ui_theme_use(&ui_theme_reference);
	CHECK(gfx_init(&disp));
	ui_layout_init();
}
#endif

#if CONFIG_UI_THEME_PANELS && defined(FONT_AA_THEME_ONLY)
// A board whose original look keeps the bitmap fonts while panels draws the anti-aliased ones (font.h
// FONT_AA_THEME_ONLY, uiThemeTestPanelsOriginal): on one display, each theme's layout has its own set
static void test_theme_fonts(void)
{
	CHECK(gfx_init(&panel));
	CHECK(ui_theme_use(&ui_theme_reference));
	CHECK(ui_layout.small->bpp == 1 && ui_layout.medium->bpp == 1 && ui_layout.large->bpp == 1);
	CHECK(ui_theme_use(&ui_theme_panels));
	CHECK(ui_layout.small->bpp == 2 && ui_layout.medium == &font_aa_12x24 && font_aa_12x24.bpp == 2);
	ui_theme_use(&ui_theme_reference);
	CHECK(gfx_init(&disp));
	ui_layout_init();
}
#endif

#if CONFIG_UI_THEME_PANELS	// ui_wrap_line is external only with a theme of its own (UI_THEME_SHARED)
// Word wrap never starts a line with the ':', ';', '?' or '!' French writes after a space: the word before it moves down
// too. Any other space still breaks, and a line still fills to its width
static void test_wrap_keeps_punctuation_on_its_word(void)
{
	static const struct { const char *text; int cols; const char *first, *rest; } cases[] = {
		{ "Codeplug impossible : non", 20, "Codeplug", "impossible : non" },
		{ "Supprimer ce canal ?", 19, "Supprimer ce", "canal ?" },
		{ "Envoyer ; puis attendre !", 24, "Envoyer ; puis", "attendre !" },
		{ "Couldn't write it: not saved", 19, "Couldn't write it:", "not saved" },
		{ "abc def", 5, "abc", "def" },
	};
	for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
		char line[64];
		const char *rest = ui_wrap_line(&font_6x12, cases[i].text, cases[i].cols * font_6x12.w, line, sizeof line);
		CHECK(strcmp(line, cases[i].first) == 0);
		CHECK(strcmp(rest, cases[i].rest) == 0);
	}
}
#endif

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
	test_reference_geometry_fields();
	test_pick();
#if CONFIG_UI_THEME_PANELS
	test_panels_frames();
	test_wrap_keeps_punctuation_on_its_word();
#endif
#if CONFIG_UI_THEME_PANELS && defined(FONT_AA_THEME_ONLY)
	test_theme_fonts();
#endif
	ui_layout_init();

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
