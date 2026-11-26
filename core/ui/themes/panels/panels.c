// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// The panels theme (CONFIG_UI_THEME_PANELS): one status strip of drawn marks on every screen, rows as rounded panels
// of one height, a soft key bar of two labels, Day and Night palettes, the Commercial and Amateur Home layouts. Its ops
// (ui_theme.h) are at the end; what they share with the reference theme's comes from ui_screens.c
#include "config.h"
#if CONFIG_UI_THEME_PANELS
#include <string.h>
#include "board.h"
#include "ui/ui_screens.h"
#include "ui/ui_theme.h"
#include "ui/fmt.h"
#include "ui/i18n.h"
#include "ui/ui_svc.h"
#include "ui/cjk_font.h"
#include "codeplug/codeplug_modes.h"
#include "panels_icons.h"

#define PANELS_BAND_H 29	// the status strip, the same on every screen
#define PANELS_SCALE 6		// its marks, in quarters of their drawn size
#define PANELS_HOME_GAP 3
#define PANELS_HOME_INSET 4
#define PANELS_FOOTER_H 34
#define PANELS_TITLE_PAD 3	// above and below a title, in its row of the medium font's height
#define PANELS_ROW_GAP 2	// between a row's panel and the next one's
#define PANELS_ACCENT_W 4	// the focus's accent bar
#define PANELS_TEXT_X 10	// a row's text from its left edge: clear of the accent bar
#define PANELS_MODE_H 24	// Home's mode bar

#define PANELS_GEAR_OUTLINE RGB565(0, 0, 0)
#define PANELS_GEAR_FILL RGB565(148, 148, 148)

static color_t panels_bar_fg;	// the bottom row's text
static color_t panels_row_bg, panels_row_focus, panels_accent;
static color_t panels_gutter;	// Home's background, around its panes

// The profile look's Day and Night palettes, and its colors outside the shared tokens (index: night)
typedef struct {
	color_t row_bg, row_focus, accent, bar_fg, gutter;
} panels_extra_colors_t;
static const ui_colors_t panels_palettes[] = {	// by UI_PALETTE_*
	{ .bg = RGB565(220, 234, 244), .fg = RGB565(23, 40, 53), .dim = RGB565(53, 78, 101),
	  .line = RGB565(171, 195, 216), .tx = RGB565(189, 41, 41), .rx = RGB565(20, 101, 59),
	  .warn = RGB565(135, 96, 0), .idle = RGB565(53, 78, 101), .pill_fg = RGB565(255, 255, 255),
	  .pill_dark_fg = RGB565(255, 255, 255), .focus = RGB565(194, 217, 237), .hint_bg = RGB565(36, 79, 136),
	  .priv = RGB565(29, 97, 165), .idle_fg = RGB565(255, 255, 255), .focus_fg = RGB565(23, 40, 53),
	  .chnum = RGB565(53, 78, 101), .strip_bg = RGB565(244, 248, 252), .strip_warn = RGB565(135, 96, 0) },
	{ .bg = RGB565(17, 26, 36), .fg = RGB565(237, 242, 247), .dim = RGB565(180, 194, 209),
	  .line = RGB565(74, 97, 121), .tx = RGB565(255, 119, 119), .rx = RGB565(116, 216, 153),
	  .warn = RGB565(245, 198, 104), .idle = RGB565(180, 194, 209), .pill_fg = RGB565(0, 0, 0),
	  .pill_dark_fg = RGB565(0, 0, 0), .focus = RGB565(38, 61, 85), .hint_bg = RGB565(29, 54, 86),
	  .priv = RGB565(130, 188, 255), .idle_fg = RGB565(0, 0, 0), .focus_fg = RGB565(237, 242, 247),
	  .chnum = RGB565(180, 194, 209), .strip_bg = RGB565(25, 38, 52), .strip_warn = RGB565(245, 198, 104) },
};
static const panels_extra_colors_t panels_extra_colors[] = {
	{ RGB565(205, 223, 237), RGB565(174, 204, 231), RGB565(36, 79, 136), RGB565(255, 255, 255),
	  RGB565(155, 183, 207) },
	{ RGB565(30, 46, 63), RGB565(48, 76, 102), RGB565(152, 191, 231), RGB565(237, 242, 247), RGB565(8, 15, 23) },
};

// The colors outside the shared tokens go with the palette (UI_PALETTE_*), the only option the op takes
static void panels_colors(ui_colors_t *c, uint8_t palette)
{
	(void)c;
	panels_row_bg = panels_extra_colors[palette].row_bg;
	panels_row_focus = panels_extra_colors[palette].row_focus;
	panels_accent = panels_extra_colors[palette].accent;
	panels_bar_fg = panels_extra_colors[palette].bar_fg;
	panels_gutter = panels_extra_colors[palette].gutter;
}

// A color display 240 px high with room across for the status strip's marks and the caller line
static bool panels_fits(void)
{
	return CONFIG_UI_COLOR && gfx_bpp() > 1 && gfx_width() >= 310 && gfx_height() == 240;
}

typedef struct { int x, y, scale; } panels_mark_pos_t;

static unsigned panels_mask_coverage(const uint8_t *mask, int stride, int x, int y)
{
	return (mask[y * stride + x / 4] >> (2 * (x % 4))) & 3u;
}

static void panels_mask(int x, int y, const uint8_t *mask, int w, int h, color_t ink)
{
	int stride = (w + 3) / 4;
	for (int dy = 0; dy < h; dy++)
		for (int dx = 0; dx < w; dx++) {
			unsigned coverage = panels_mask_coverage(mask, stride, dx, dy);
			if (coverage)
				gfx_coverage_pixel(x + dx, y + dy, ink, coverage);
		}
}

static int panels_px(int n, int scale) { return n * scale / 4; }

static void panels_block(panels_mark_pos_t at, int x, int y, int w, int h, color_t ink)
{
	int dx = panels_px(x, at.scale), dy = panels_px(y, at.scale);
	gfx_fill(at.x + dx, at.y + dy, panels_px(x + w, at.scale) - dx, panels_px(y + h, at.scale) - dy, ink);
}

static void panels_round_fill(int x, int y, int w, int h, color_t ink)
{
	gfx_round_fill(x, y, w, h, ink);
	if (w >= 3 && h >= 3) {
		gfx_coverage_pixel(x, y, ink, 1);
		gfx_coverage_pixel(x + w - 1, y, ink, 1);
		gfx_coverage_pixel(x, y + h - 1, ink, 1);
		gfx_coverage_pixel(x + w - 1, y + h - 1, ink, 1);
	}
}

static void panels_pane(int y, int h, color_t ink)
{
	static const uint8_t corner[] = { 5, 3, 2, 1, 1 };
	const int x = PANELS_HOME_INSET, w = gfx_width() - 2 * x;
	const int r = sizeof corner;
	gfx_fill(x, y + r, w, h - 2 * r, ink);
	for (int i = 0; i < r; i++) {
		int inset = corner[i], top = y + i, bottom = y + h - 1 - i;
		gfx_hline(x + inset, top, w - 2 * inset, ink);
		gfx_hline(x + inset, bottom, w - 2 * inset, ink);
		gfx_coverage_pixel(x + inset - 1, top, ink, 1);
		gfx_coverage_pixel(x + w - inset, top, ink, 1);
		gfx_coverage_pixel(x + inset - 1, bottom, ink, 1);
		gfx_coverage_pixel(x + w - inset, bottom, ink, 1);
	}
}

static int panels_footer_top(void)
{
	return gfx_height() - PANELS_HOME_INSET - PANELS_FOOTER_H;
}

static void panels_round_block(panels_mark_pos_t at, int x, int y, int w, int h, color_t ink)
{
	int dx = panels_px(x, at.scale), dy = panels_px(y, at.scale);
	panels_round_fill(at.x + dx, at.y + dy, panels_px(x + w, at.scale) - dx, panels_px(y + h, at.scale) - dy, ink);
}

typedef struct { uint32_t left, right; } panels_subrow_t;

// Round the four subpixels of a mask corner only when the edge turns in both directions.
// Straight stems, isolated points, and the gaps between parallel strokes keep their coverage.
static panels_subrow_t panels_subrow(const uint32_t *rows, int h, int sample)
{
	int y = sample / 2;
	uint32_t above = y > 0 ? rows[y - 1] : 0;
	uint32_t below = y + 1 < h ? rows[y + 1] : 0;
	uint32_t center = rows[y];
	uint32_t left = center >> 1, right = center << 1;
	uint32_t adjacent = (sample & 1) ? below : above;
	uint32_t corners = (above ^ below) & (left ^ right) & (center ^ adjacent);
	return (panels_subrow_t){ center ^ (corners & ~(left ^ adjacent)),
		center ^ (corners & ~(right ^ adjacent)) };
}

static unsigned panels_coverage(panels_subrow_t upper, panels_subrow_t lower, int a, int b, int w)
{
	uint32_t bit_a = 1u << (w - 1 - a / 2), bit_b = 1u << (w - 1 - b / 2);
	unsigned hits = ((a & 1 ? upper.right : upper.left) & bit_a) != 0;
	hits += ((a & 1 ? lower.right : lower.left) & bit_a) != 0;
	hits += ((b & 1 ? upper.right : upper.left) & bit_b) != 0;
	hits += ((b & 1 ? lower.right : lower.left) & bit_b) != 0;
	return (hits * 3 + 2) / 4;
}

static void panels_bitmap(int x, int y, const uint32_t *rows, int w, int h, int width, int height, color_t ink)
{
	for (int dy = 0; dy < height; dy++) {
		panels_subrow_t upper = panels_subrow(rows, h, (4 * dy + 1) * h / (2 * height));
		panels_subrow_t lower = panels_subrow(rows, h, (4 * dy + 3) * h / (2 * height));
		for (int dx = 0; dx < width; dx++) {
			int a = width == w ? 2 * dx : (4 * dx + 1) * w / (2 * width);
			int b = width == w ? 2 * dx + 1 : (4 * dx + 3) * w / (2 * width);
			unsigned coverage = panels_coverage(upper, lower, a, b, w);
			if (coverage)
				gfx_coverage_pixel(x + dx, y + dy, ink, coverage);
		}
	}
}

#define PANELS_HEIGHT(mark) (sizeof(mark) / sizeof((mark)[0]))
#define PANELS_SCALED_PIXELS(x, y, mark, w, width, height, ink) \
	panels_bitmap(x, y, mark, w, PANELS_HEIGHT(mark), width, height, ink)
#define PANELS_PIXELS(x, y, mark, w, ink) \
	PANELS_SCALED_PIXELS(x, y, mark, w, w, PANELS_HEIGHT(mark), ink)
#define PANELS_MARK_AT(at, mark, width, ink) \
	PANELS_SCALED_PIXELS((at).x, (at).y, mark, width, panels_px(width, (at).scale), \
		panels_px(PANELS_HEIGHT(mark), (at).scale), ink)
#define PANELS_MARK(x, y, mark, width, ink) PANELS_PIXELS(x, y, mark, width, ink)
#define PANELS_STATUS_MARK(x, y, mark, width, ink) \
	PANELS_MARK_AT(((panels_mark_pos_t){ x, y, scale }), mark, width, ink)

static color_t panels_ink(const ui_state_t *s)
{
	return s->gui_night ? ui_col.fg : RGB565(0, 0, 0);
}


enum {
	PANELS_VOX_W = 22, PANELS_DUAL_W = 25, PANELS_MAIL_W = 23, PANELS_SPEAKER_W = 19,
	PANELS_DIRECT_W = 19, PANELS_SCAN_W = 22, PANELS_ORBIT_W = 27, PANELS_BT_W = 24,
};

static const uint32_t panels_vox_art[] = {
	0x330C33, 0x330C33, 0x333333, 0x333333, 0x33330C, 0x33330C, 0x333333, 0x333333,
	0x0C0C33, 0x0C0C33,
};
static const uint32_t panels_rx_art[] = {
	0xFF0C3, 0xFF842, 0xC1C66, 0xC1C24, 0xFF83C, 0xFF018, 0xCE018, 0xC6018,
	0xC703C, 0xC3824, 0xC1C66, 0xC0C42, 0xC0EC3, 0x00400,
};

#if CONFIG_DUAL_WATCH
static const uint32_t panels_dual_art[] = {
	0x0FF83FE, 0x1FFC7FF, 0x180C603, 0x180C603, 0x180C603, 0x188C663, 0x194C653, 0x19CC663,
	0x194C653, 0x194C663, 0x180C603, 0x180C603, 0x180C603, 0x1FFC7FF, 0x0FF83FE,
};
#endif

#ifdef GFX_FIT_TRACE
static const uint32_t panels_direct_art[] = {
	0x3F0C0, 0x3F0C0, 0x0C330, 0x0C330, 0x0C3F0, 0x0C3F0, 0x0C330, 0x0C330,
	0x0C330, 0x0C330, 0x00000, 0x00000, 0x00010, 0x00018, 0x0001E, 0x7FFFF,
	0x7FFFE, 0x00018, 0x00010,
};
static const uint32_t panels_bt_art[] = {
	0x0380, 0x03C0, 0x03F0, 0x03B8, 0x439E, 0xE387, 0x7B9E, 0x1FB8,
	0x0FF0, 0x07E0, 0x03C0, 0x07E0, 0x0FF0, 0x1FB8, 0x7B9E, 0xE387,
	0x439E, 0x03B8, 0x03F0, 0x03C0, 0x0380,
};
static const uint32_t panels_record_ring[] = {
	0x007C0, 0x01FF0, 0x03838, 0x0600C, 0x0C006, 0x0C006, 0x18003, 0x18003,
	0x18003, 0x18003, 0x18003, 0x0C006, 0x0C006, 0x0600C, 0x03838, 0x01FF0,
	0x007C0,
};
static const uint32_t panels_record_center[] = {
	0x038, 0x0FE, 0x0FE, 0x1FF, 0x1FF, 0x1FF, 0x0FE, 0x0FE,
	0x038,
};

static void panels_recording(int x, int cy, int size, const ui_state_t *s)
{
	int y = cy - size / 2;
	PANELS_SCALED_PIXELS(x, y, panels_record_ring, 17, size, size, ui_col.tx);
	if (!(s->now_ms / 400u % 2u)) {
		int inset = 4 * size / 17;
		int dot_size = 13 * size / 17 - inset;
		PANELS_SCALED_PIXELS(x + inset, y + inset, panels_record_center, 9, dot_size, dot_size, ui_col.tx);
	}
}

static void panels_bluetooth(int x, int cy, int height, const ui_state_t *s)
{
	color_t ink = s->gui_sim_bt == 2 ? ui_col.priv : ui_col.rx;
	PANELS_SCALED_PIXELS(x + 4, cy - height / 2, panels_bt_art, 16, 16, height, ink);
	if (s->gui_sim_bt == 2) {
		panels_round_fill(x + 1, cy - 1, 3, 3, ink);
		panels_round_fill(x + 21, cy - 1, 3, 3, ink);
	}
}
#endif


static void panels_scan(int x, int cy, uint32_t frame, color_t ink, color_t step)
{
	// The silhouette stays fixed while the highlight walks the three legs, then fills the arrowhead.
	for (int dy = 0; dy < 17; dy++)
		for (int dx = 0; dx < PANELS_SCAN_W; dx++) {
			unsigned coverage = panels_mask_coverage(panels_scan_22, (PANELS_SCAN_W + 3) / 4, dx, dy);
			if (!coverage)
				continue;
			unsigned segment;
			if (dx >= 16 && dy >= 7)
				segment = 12;
			else if (dy < 4)
				segment = (unsigned)dx / 4u;
			else if (dy < 11)
				segment = 4u + (unsigned)(dy - 4) / 2u;
			else
				segment = 8u + (unsigned)dx / 4u;
			gfx_coverage_pixel(x + dx, cy - 8 + dy, frame == segment ? step : ink, coverage);
		}
}

#if CONFIG_DMR_ROAMING

static void panels_roaming(int x, int cy, uint32_t frame, color_t ink)
{
	static const uint32_t letter[] = { 0x3C, 0x3C, 0x33, 0x33, 0x3C, 0x3C, 0x33, 0x33, 0x33, 0x33 };
	panels_mask(x, cy - 12, panels_orbit_frames[frame % 8u], PANELS_ORBIT_W, 25, ink);
	PANELS_MARK(x + 10, cy - 5, letter, 6, ink);
}
#endif

static bool panels_clock(const ui_state_t *s, char *out, size_t cap)
{
	uint32_t date = s->gps_utc_ymd, time = s->gps_utc_hms;
	uint32_t month = date / 100u % 100u, day = date % 100u;
	if (!s->gps_clock || !s->gps_fix || date == 0 || month == 0 || month > 12 || day == 0 || day > 31 ||
	    time / 10000u > 23 || time / 100u % 100u > 59 || time % 100u > 60)
		return false;
	static const tr_id_t months[] = {
		TRN(CLOCK_MONTH_JAN, "Jan"), TRN(CLOCK_MONTH_FEB, "Feb"), TRN(CLOCK_MONTH_MAR, "Mar"),
		TRN(CLOCK_MONTH_APR, "Apr"), TRN(CLOCK_MONTH_MAY, "May"), TRN(CLOCK_MONTH_JUN, "Jun"),
		TRN(CLOCK_MONTH_JUL, "Jul"), TRN(CLOCK_MONTH_AUG, "Aug"), TRN(CLOCK_MONTH_SEP, "Sep"),
		TRN(CLOCK_MONTH_OCT, "Oct"), TRN(CLOCK_MONTH_NOV, "Nov"), TRN(CLOCK_MONTH_DEC, "Dec"),
	};
	sbuf_t b;
	sb_init(&b, out, cap);
	sb_u32(&b, day, 1);
	sb_chr(&b, ' ');
	sb_str(&b, tr(months[month - 1]));
	sb_chr(&b, ' ');
	sb_u32(&b, time / 10000u, 2);
	sb_chr(&b, ':');
	sb_u32(&b, time / 100u % 100u, 2);
	sb_chr(&b, 'Z');
	return true;
}

static void panels_battery(const radio_view_t *v, const ui_state_t *s, panels_mark_pos_t at, color_t ink)
{
	bool low = v->battery != RADIO_BATT_OK || (s->battery_pct &&
		s->battery_pct <= (s->low_batt_pct ? s->low_batt_pct : 15));
	color_t c = low ? ui_col.tx : ink;
	panels_round_block(at, 3, -5, 5, 3, c);
	panels_round_block(at, 0, -3, 11, 16, c);
	panels_round_block(at, 1, -2, 9, 14, ui_col.strip_bg);
	unsigned segments = (s->battery_pct + 33u) / 34u;
	for (int i = 0; i < 3; i++)
		panels_block(at, 2, 8 - i * 4, 7, 3, (unsigned)i < segments ? c : ui_col.line);
}

static unsigned panels_power_level(const radio_view_t *v)
{
	unsigned level = v->power * 3u / board.power_count;
	return level < 3 ? level : 2;
}

// The status strip's items in the order they get room, the first first (panels_draw_status): what a crowded strip
// gives up goes last (gui-plan.md, decided Nov 26): battery, TX/NO TX, signal, mode, lock, privacy, scan, monitor and
// unread, then the GPS clock, then GPS, the USB network, power, receive-only, VOX, dual watch and roaming
enum {
	PANELS_SI_BATTERY, PANELS_SI_ACTIVITY, PANELS_SI_SIGNAL, PANELS_SI_MODE, PANELS_SI_LOCK, PANELS_SI_KEY,
	PANELS_SI_SCAN, PANELS_SI_MONITOR, PANELS_SI_UNREAD, PANELS_SI_CLOCK, PANELS_SI_GPS, PANELS_SI_IP,
	PANELS_SI_POWER, PANELS_SI_RX_ONLY, PANELS_SI_VOX, PANELS_SI_DUAL, PANELS_SI_ROAM,
#ifdef GFX_FIT_TRACE
	PANELS_SI_RECORDING, PANELS_SI_TALKAROUND, PANELS_SI_BT,	// the simulator's examples
#endif
	PANELS_SI_COUNT
};
#define PANELS_STRIP_X 10	// the strip's marks from the screen's edges: inside its pane's rounded ends

// What the strip shows at a frame: its states, worked out once for the measure and the draw
typedef struct {
	const char *mode, *activity, *algorithm;
	color_t activity_ink;
	const font_t *mode_font;
	char clock[24];
	uint8_t strength;
	bool encrypted, warning;
} panels_strip_t;

// The privacy key: green while DMR privacy is configured (with AES on an AES channel), a red X on encrypted receive
// activity without a usable key
static void panels_strip_key(const radio_view_t *v, const ui_state_t *s, panels_strip_t *t)
{
#if CONFIG_DMR
	if (v->ch.mode == RADIO_MODE_DMR) {
		t->encrypted = v->dmr.privacy;
		bool aes = CONFIG_DMR_AES && v->ch.kind == CODEPLUG_KIND_DMR_AES;
		bool receiving = v->state == RS_RX &&
			(v->dmr.state == RDMR_RX || v->dmr.state == RDMR_RX_OTHER);
		t->warning = receiving && v->dmr.call.encrypted &&
			(!t->encrypted || (v->dmr.call.pi_header && !aes) || v->dmr.call.privacy_blocked);
		if (t->encrypted && aes && !t->warning)
			t->algorithm = "AES";
	}
#else
	(void)v;
#endif
#ifdef GFX_FIT_TRACE
	if (s->gui_sim_crypt && !t->warning) {
		t->algorithm = s->gui_sim_crypt == 1 ? "ARC4" : "AES";
		t->encrypted = true;
	}
#else
	(void)s;
#endif
}

static const char *panels_status_mode(const radio_view_t *v, const ui_state_t *s)
{
	const char *mode = ui_mode_word(v);
#if CONFIG_DUAL_WATCH
	if (s->dual.on)
		mode = ui_dual_mode_word(&s->dual.pane[s->dual.sel]);
#endif
#ifdef GFX_FIT_TRACE
	if (s->gui_sim_dstar)
		mode = "D-STAR";
#endif
	if (v->aprs_tx)
		mode = "FM";
	return mode;
}

static const char *panels_status_activity(const radio_view_t *v, radio_state_t st, color_t *color)
{
	const char *activity = NULL;
	*color = ui_col.dim;
	if (st == RS_TX) {
		activity = v->aprs_tx ? "TX APRS" : "TX";
		*color = ui_col.tx;
	} else if (st == RS_TX_DENIED) {
		activity = TR(TOP_STATE_NO_TX, "NO TX");
		*color = ui_col.warn;
	}
	return activity;
}

static uint8_t panels_signal_bars(const radio_view_t *v, const ui_state_t *s)
{
	if (v->state == RS_TX || v->state == RS_TX_DENIED)
		return 0;
#if CONFIG_DUAL_WATCH
	if (s->dual.on && s->dual.rx < 2) {
		const ui_dual_pane_t *pane = &s->dual.pane[s->dual.rx];
		return pane->rx && pane->rssi_dbm ? ui_rssi_bars(pane->rssi_dbm) : 0;
	}
#else
	(void)s;
#endif
	// The receiver samples background RSSI even after the squelch closes.
	return v->state == RS_RX && v->rssi_dbm ? ui_rssi_bars(v->rssi_dbm) : 0;
}

static bool panels_scanning(const ui_state_t *s)
{
#if CONFIG_DUAL_WATCH
	return s->scanning && !s->dual.on;
#else
	return s->scanning;
#endif
}

#define PANELS_STRIP_GAP 4	// between two items of the strip

// The width an item's mark or text takes in the strip, 0 where it has nothing to show
static int panels_strip_mark_w(int item, const radio_view_t *v, const ui_state_t *s, const panels_strip_t *t)
{
	const int scale = PANELS_SCALE;
	const font_t *f = &font_aa_8x16;
	switch (item) {
	case PANELS_SI_BATTERY: return s->no_battery ? 0 : panels_px(11, scale);
	case PANELS_SI_ACTIVITY: return t->activity ? gfx_text_width(f, t->activity) : 0;
	case PANELS_SI_SIGNAL: return t->strength ? panels_px(18, scale) : 0;
	case PANELS_SI_MODE: return t->mode != NULL ? gfx_text_width(t->mode_font, t->mode) : 0;
	case PANELS_SI_LOCK: return s->locked ? panels_px(10, scale) : 0;
	case PANELS_SI_KEY:
		if (!t->encrypted && !t->warning)
			return 0;
		return t->algorithm ? gfx_text_width(f, t->algorithm) : panels_px(15, scale);
	case PANELS_SI_SCAN: return panels_scanning(s) ? PANELS_SCAN_W : 0;
	case PANELS_SI_MONITOR: return (v->monitor || s->hear_all) && !v->aprs_tx ? PANELS_SPEAKER_W : 0;
	case PANELS_SI_UNREAD: return s->unread ? PANELS_MAIL_W : 0;
	case PANELS_SI_GPS: return s->gps_fix || s->gps == UI_GPS_SEARCH ? panels_px(11, scale) : 0;
	case PANELS_SI_IP: return s->usb_net != UI_USB_NET_OFF ? gfx_text_width(f, "IP") : 0;
	case PANELS_SI_POWER: return board.power_count ? font_aa_12x24.w : 0;
	case PANELS_SI_RX_ONLY: return v->ch.rx_only || v->am || v->bcast ? 20 : 0;
	case PANELS_SI_VOX: return v->vox_on ? PANELS_VOX_W : 0;
	case PANELS_SI_CLOCK: return t->clock[0] ? gfx_text_width(f, t->clock) + 8 : 0;	// kept apart from both sides
#if CONFIG_DUAL_WATCH
	case PANELS_SI_DUAL: return s->dual.on ? PANELS_DUAL_W : 0;
#endif
#if CONFIG_DMR_ROAMING
	case PANELS_SI_ROAM: return v->roam.state != RROAM_STATE_OFF && !v->aprs_tx ? PANELS_ORBIT_W : 0;
#endif
#ifdef GFX_FIT_TRACE
	case PANELS_SI_RECORDING: return s->gui_sim_recording ? 17 : 0;
	case PANELS_SI_TALKAROUND: return s->gui_sim_talkaround ? PANELS_DIRECT_W : 0;
	case PANELS_SI_BT: return s->gui_sim_bt ? PANELS_BT_W : 0;
#endif
	default: return 0;
	}
}

// ... with the gap that follows it
static int panels_strip_w(int item, const radio_view_t *v, const ui_state_t *s, const panels_strip_t *t)
{
	int w = panels_strip_mark_w(item, v, s, t);
	return w ? w + PANELS_STRIP_GAP : 0;
}

// The items that get room, in their order: each while what is taken so far leaves room for it, never over another
static uint32_t panels_strip_fit(const radio_view_t *v, const ui_state_t *s, const panels_strip_t *t, int room)
{
	uint32_t shown = 0;
	for (int i = 0; i < PANELS_SI_COUNT; i++) {
		int w = panels_strip_w(i, v, s, t);
		if (w && w <= room) {
			shown |= 1u << i;
			room -= w;
		}
	}
	return shown;
}

// One item at x (its left edge), the strip's marks centered on cy
static void panels_strip_item(int item, int x, int cy, const radio_view_t *v, const ui_state_t *s,
	const panels_strip_t *t, color_t ink)
{
	const int scale = PANELS_SCALE;
	int y = cy - panels_px(9, scale) / 2;	// the top of a mark drawn at the strip's scale
	switch (item) {
	case PANELS_SI_BATTERY:
		panels_battery(v, s, (panels_mark_pos_t){ x, y, scale }, ink);
		break;
	case PANELS_SI_ACTIVITY:
		gfx_text(&font_aa_8x16, x, cy - font_aa_8x16.h / 2, t->activity, t->activity_ink);
		break;
	case PANELS_SI_SIGNAL: {
		panels_mark_pos_t signal = { x, y - panels_px(4, scale), scale };
		for (int bar = 0; bar < t->strength; bar++) {
			int height = 3 + bar * 2;
			panels_round_block(signal, bar * 5, 13 - height, 3, height, ink);
		}
		break;
	}
	case PANELS_SI_MODE:
		gfx_text(t->mode_font, x, cy - t->mode_font->h / 2, t->mode, ink);
		break;
	case PANELS_SI_LOCK:
		panels_mask(x, y - 1, panels_lock_15, panels_px(10, scale), panels_px(11, scale), ink);
		break;
	case PANELS_SI_KEY: {
		// The key over its algorithm's name (8x16, the strip's smallest text) where it has one
		const font_t *f = &font_aa_8x16;
		int key_w = t->algorithm ? (int)strlen(t->algorithm) * f->w : panels_px(15, scale);
		int kx = x + (key_w - panels_px(13, scale)) / 2;
		color_t key_color = !t->warning ? ui_col.rx : s->gui_night ? RGB565(160, 168, 178) : RGB565(105, 114, 124);
		panels_mask(kx, t->algorithm ? cy - 11 : y + 1, panels_key_19, panels_px(13, scale), 10, key_color);
		if (t->algorithm)
			gfx_text(f, x + (key_w - gfx_text_width(f, t->algorithm)) / 2, cy - 4, t->algorithm, ui_col.rx);
		if (t->warning) {
			static const uint32_t cross[] = { 0x1001, 0x0802, 0x040C, 0x0310, 0x00A0, 0x0040, 0x00A0, 0x0310,
				0x040C, 0x0802, 0x1001 };
			PANELS_STATUS_MARK(kx, y - panels_px(1, scale), cross, 13, ui_col.tx);
		}
		break;
	}
	case PANELS_SI_SCAN:
		panels_scan(x, cy, s->now_ms / 100u % 13u, ink, ui_col.dim);
		break;
	case PANELS_SI_MONITOR:
		panels_mask(x, cy - 8, panels_speaker_19, PANELS_SPEAKER_W, 16, ink);
		break;
	case PANELS_SI_UNREAD:
		panels_mask(x, cy - 7, panels_mail_23, PANELS_MAIL_W, 15, ink);
		break;
	case PANELS_SI_GPS:
		panels_mask(x, y - 1, panels_pin_16, panels_px(11, scale), panels_px(11, scale), s->gps_fix ? ink : ui_col.dim);
		break;
	case PANELS_SI_IP:
		gfx_text(&font_aa_8x16, x, cy - font_aa_8x16.h / 2, "IP", s->usb_net == UI_USB_NET_UP ? ink : ui_col.dim);
		break;
	case PANELS_SI_POWER: {
		char level[] = { "LMH"[panels_power_level(v)], 0 };
		gfx_text(&font_aa_12x24, x, cy - font_aa_12x24.h / 2, level, ink);
		break;
	}
	case PANELS_SI_RX_ONLY:
		PANELS_PIXELS(x, cy - 7, panels_rx_art, 20, ink);
		break;
	case PANELS_SI_VOX:
		PANELS_PIXELS(x, cy - 5, panels_vox_art, PANELS_VOX_W, ink);
		break;
	case PANELS_SI_CLOCK:
		gfx_text(&font_aa_8x16, x + 4, cy - font_aa_8x16.h / 2, t->clock, ui_col.dim);
		break;
#if CONFIG_DUAL_WATCH
	case PANELS_SI_DUAL:
		PANELS_PIXELS(x, cy - 7, panels_dual_art, PANELS_DUAL_W, ink);
		break;
#endif
#if CONFIG_DMR_ROAMING
	case PANELS_SI_ROAM: {
		bool moving = v->roam.state == RROAM_STATE_SEARCH || v->roam.state == RROAM_STATE_SURVEY;
		panels_roaming(x, cy, moving ? s->now_ms / 125u : 0, ink);
		break;
	}
#endif
#ifdef GFX_FIT_TRACE
	case PANELS_SI_RECORDING:
		panels_recording(x, cy, 17, s);
		break;
	case PANELS_SI_TALKAROUND:
		PANELS_PIXELS(x, cy - 10, panels_direct_art, PANELS_DIRECT_W, ink);
		break;
	case PANELS_SI_BT:
		panels_bluetooth(x, cy, 21, s);
		break;
#endif
	default:
		break;
	}
}

// Where the items go: from the left, signal and mode first; from the right edge, battery first
static const uint8_t panels_strip_left[] = {
	PANELS_SI_SIGNAL, PANELS_SI_MODE, PANELS_SI_ROAM, PANELS_SI_ACTIVITY, PANELS_SI_MONITOR, PANELS_SI_DUAL,
	PANELS_SI_VOX,
};
static const uint8_t panels_strip_right[] = {
	PANELS_SI_BATTERY,
#ifdef GFX_FIT_TRACE
	PANELS_SI_BT,
#endif
	PANELS_SI_POWER, PANELS_SI_LOCK, PANELS_SI_GPS, PANELS_SI_IP,
#ifdef GFX_FIT_TRACE
	PANELS_SI_RECORDING,
#endif
	PANELS_SI_UNREAD, PANELS_SI_KEY,
#ifdef GFX_FIT_TRACE
	PANELS_SI_TALKAROUND,
#endif
	PANELS_SI_RX_ONLY, PANELS_SI_SCAN,
};

// The status strip, a rounded pane at the top of every screen, the same marks at the same size on every one: signal
// bars, the mode, roaming, TX or NO TX and the monitor, dual watch and VOX marks from the left; battery, power, lock,
// GPS, the USB network and the operating marks (unread mail, the privacy key, a receive-only channel, scanning) from the
// right; the GPS clock in the room between. Home has no mode word in it: its mode bar has the mode. Nothing overlaps:
// a crowded strip gives up items in the reverse of their order (PANELS_SI_*), and the mode takes the 8x16 font before
// anything else goes. under: the screen's color around the pane. Returns whether the GPS clock is due but had no room
// (Home then has it in its mode bar)
static bool panels_draw_status(const radio_view_t *v, radio_state_t st, const ui_state_t *s, color_t under, bool home)
{
	ui_strip_draws++;
	int w = gfx_width(), h = ui_layout.strip_h;
	color_t ink = panels_ink(s);
	gfx_fill(0, 0, w, h, under);
	panels_pane(2, h - 2, ui_col.strip_bg);
	panels_strip_t t = { 0 };
	t.mode = home ? NULL : panels_status_mode(v, s);
	t.activity = panels_status_activity(v, st, &t.activity_ink);
	t.strength = panels_signal_bars(v, s);
	panels_strip_key(v, s, &t);
	int room = w - 2 * PANELS_STRIP_X;
	if (!panels_clock(s, t.clock, sizeof t.clock))
		t.clock[0] = 0;
	// The mode in the large font where every item, the clock included, fits beside it
	t.mode_font = &font_aa_12x24;
	int need = 0;
	for (int i = 0; i < PANELS_SI_COUNT; i++)
		need += panels_strip_w(i, v, s, &t);
	if (need > room)
		t.mode_font = &font_aa_8x16;
	uint32_t shown = panels_strip_fit(v, s, &t, room);
	int cy = (h - 1) / 2;	// a 24 px cell (the mode) from y 2, clear of Home's mode bar below
	int left = PANELS_STRIP_X, right = w - PANELS_STRIP_X;
	for (unsigned i = 0; i < sizeof panels_strip_left; i++) {
		int item = panels_strip_left[i];
		if (shown & (1u << item)) {
			panels_strip_item(item, left, cy, v, s, &t, ink);
			left += panels_strip_w(item, v, s, &t);
		}
	}
	for (unsigned i = 0; i < sizeof panels_strip_right; i++) {
		int item = panels_strip_right[i];
		if (shown & (1u << item)) {
			right -= panels_strip_w(item, v, s, &t);
			panels_strip_item(item, right + PANELS_STRIP_GAP, cy, v, s, &t, ink);
		}
	}
	if (shown & (1u << PANELS_SI_CLOCK)) {
		int cw = panels_strip_mark_w(PANELS_SI_CLOCK, v, s, &t);
		panels_strip_item(PANELS_SI_CLOCK, left + (right + PANELS_STRIP_GAP - left - cw) / 2, cy, v, s, &t, ink);
	}
	return t.clock[0] && !(shown & (1u << PANELS_SI_CLOCK));
}

// A screen's title below the strip, in the medium font (8x16 where it is too long), left-aligned, reserve pixels left
// free at its right for the scroll position. Returns the y below its row, the same whichever font it took. The strip
// is drawn before, by the caller: under this function's buffers it would deepen the UI task's deepest path
static int panels_title(const char *prefix, const char *name, int reserve, bool draw)
{
	const ui_layout_t *L = &ui_layout;
	int y = L->strip_h + PANELS_TITLE_PAD;
	if (draw) {
		char title[48], fit[40];	// on the UI task's deepest path (make stack-check): no larger
		sbuf_t b;
		sb_init(&b, title, sizeof title);
		if (prefix)
			sb_str(&b, prefix);
		if (prefix && name)
			sb_chr(&b, ' ');
		if (name)
			sb_str(&b, name);
		int room = gfx_width() - 30 - reserve;
		const font_t *f = gfx_text_width(L->medium, title) <= room ? L->medium : &font_aa_8x16;
		gfx_text_fit(f, title, room, fit, sizeof fit);
		gfx_text(f, 15, y + (L->medium->h - f->h) / 2, fit, ui_col.fg);
	}
	return y + L->medium->h + PANELS_TITLE_PAD;
}

// Its strip, and a title as a list's. Measuring returns before the strip's drawing and its buffers
static int panels_top(const radio_view_t *v, radio_state_t st, const ui_state_t *s, const char *prefix,
	const char *name, bool draw)
{
	if (draw)
		panels_draw_status(v, st, s, ui_col.bg, false);
	if (prefix == NULL && name == NULL)
		return ui_layout.strip_h;
	if (draw) {
		// The title row in the screen's background: a screen drawn without clearing it first (a page over a page)
		gfx_fill(0, ui_layout.strip_h, gfx_width(), ui_layout.medium->h + 2 * PANELS_TITLE_PAD, ui_col.bg);
	}
	return panels_title(prefix, name, 0, draw);
}

// The bottom of the screen: the soft key bar, a rounded pane with two labels, one over each soft key, and the hint row
// above it in the 8x16 font. A hint (groups separated by two spaces, ui_hint_split) declares its keys by structure:
// a group starting with the OK key's name ("OK", which no catalog translates) is the OK key's, one starting with the
// Back key's name (HINT_BACK, as ui_hint_split finds it) the Back key's. The bar names the two soft keys, OK and
// Back (Home: Menu and its assigned key), where the hint declares them; the bar stays with Hint row off
// (UI_THEME_OPT_SOFT_KEYS). The hint row has only what the bar can't say: the other keys' groups (#, *, the digits,
// PTT, Up/Down, a key held), sentences, and what OK and Back do where it isn't their usual job. In a list OK opens or
// picks the focused row and Back leaves, so its OK and Back groups are left out, unless the row's value editor is open
// (UI_BAR_EDIT: "OK save  Back cancel"); elsewhere they stay ("Back undo", "Back stop scan"). Nothing compares text
// a translation chose, so the bar and the row are the same in every language
#define PANELS_BAR_MENU 0x20u	// the theme's own flags: Home, where OK opens the menu (ui_home_ok_menu): Menu on the left
#define PANELS_BAR_KEYS 0x40u	// ... a screen of the theme's own where OK and Back both act (Quick)
#define PANELS_BAR_IDLE 0x80u	// ... Home's idle hint, whose first group is the menu's (the label says it)

static bool panels_group_ok(const char *g, size_t n)
{
	return n >= 2 && g[0] == 'O' && g[1] == 'K' && (n == 2 || g[2] == ' ');
}

static bool panels_group_back(const char *g, size_t n)
{
	const char *back = TR(HINT_BACK, "Back");
	size_t bn = strlen(back);
	return n >= bn && strncmp(g, back, bn) == 0;
}

// One form of a hint (the key names in) taken apart: the soft keys it declares (bit 0 OK, bit 1 Back), and into row
// (NULL: only the keys) the groups the bar can't say (see above). Returns the keys, and 4 when the row has a group
static unsigned panels_hint_parts(const char *form, uint8_t flags, char *row, size_t cap)
{
	unsigned keys = 0;
	size_t rn = 0, bn = strlen(TR(HINT_BACK, "Back"));
	bool first = true;
	for (const char *g = form; *g;) {
		const char *end = strstr(g, "  ");
		size_t n = end != NULL ? (size_t)(end - g) : strlen(g);
		bool ok = panels_group_ok(g, n), back = panels_group_back(g, n);
		keys |= (ok ? 1u : 0u) | (back ? 2u : 0u);
		bool usual = (ok || back) && (flags & UI_BAR_LIST) && !(flags & UI_BAR_EDIT);
		bool said = (ok && n == 2) || (back && n == bn) || usual || ((flags & PANELS_BAR_IDLE) && first) ||
			(flags & UI_BAR_NO_HINTS);
		if (!said && n) {
			keys |= 4u;
			if (row != NULL && rn + n + 3 < cap) {
				if (rn) {
					row[rn++] = ' ';
					row[rn++] = ' ';
				}
				memcpy(row + rn, g, n);
				rn += n;
			}
		}
		first = false;
		g += n;
		while (*g == ' ')
			g++;
	}
	if (row != NULL)
		row[rn] = 0;
	return keys;
}

static int panels_hint_top(void)
{
	return panels_footer_top() - font_aa_8x16.h;
}

// The hint has a group for the hint row (for Home, which ends its pane above the row then)
static bool panels_hint_has_row(const char *text, uint8_t flags)
{
	char form[64], names[64];
	bool more;
	gfx_text_alt(text, 0, form, sizeof form, &more);
	return (panels_hint_parts(ui_key_names(form, names, sizeof names, true), flags, NULL, 0) & 4u) != 0;
}

// The bar's text in the largest font that fits both labels, left at the left soft key, right at the right one
static void panels_bar_labels(const char *left, const char *right, int top, color_t fg)
{
	const ui_layout_t *L = &ui_layout;
	const int inset = PANELS_HOME_INSET, h = PANELS_FOOTER_H, margin = 15 - inset;
	int w = gfx_width() - 2 * inset;
	const font_t *fonts[] = { FONT_AA_LARGEST, &font_aa_12x24, L->small };
	const font_t *f = L->small;
	for (unsigned i = 0; i < sizeof fonts / sizeof fonts[0]; i++) {
		int lw = left != NULL ? gfx_text_width(fonts[i], left) : 0, rw = right != NULL ? gfx_text_width(fonts[i], right) : 0;
		if (lw + rw + fonts[i]->w <= w - 2 * margin) {
			f = fonts[i];
			break;
		}
	}
	char fit[32];
	int text_y = top + (h - f->h) / 2, rw = 0;
	if (right != NULL) {
		gfx_text_fit(f, right, w - 2 * margin, fit, sizeof fit);
		rw = gfx_text_width(f, fit);
		gfx_text(f, inset + w - margin - rw, text_y, fit, fg);
	}
	if (left != NULL) {
		gfx_text_fit(f, left, w - 2 * margin - (rw ? rw + f->w : 0), fit, sizeof fit);
		gfx_text(f, inset + margin, text_y, fit, fg);
	}
}

// The bottom row: the soft key bar and the hint row (above), or a toast in the small font, centered on the bar, with a
// second line above it when it is too long for one. soft_key: Home's assigned soft key, its label on the right. The
// buffers are the size of the reference theme's (ui_screens.c): the UI task's deepest chain runs through here
static void panels_bottom(const char *text, uint8_t flags, const char *soft_key)
{
#if CONFIG_VOLUME_BAR
	ui_hint_draws++;
#endif
	bool toast = flags & UI_BAR_TOAST;
	const ui_layout_t *L = &ui_layout;
	const int inset = PANELS_HOME_INSET;
	int top = panels_footer_top();
	const int h = PANELS_FOOTER_H;
	const font_t *f = L->small;
	char fit[64];
	int w = gfx_width() - 2 * inset;
	if (toast) {
		char first[64], second[64];	// second holds the raw alternate until the key names are in first
		const char *line = first;	// the alternate with the key names: first, or second when there were none
		bool more = true;
		// One line in the longest alternate that fits it, else two lines of the first
		for (int level = 0; more; level++) {
			gfx_text_alt(text, level, second, sizeof second, &more);
			line = ui_key_names(second, first, sizeof first, false);
			if (gfx_text_width(f, line) <= w - 2)
				break;
		}
		if (gfx_text_width(f, line) > w - 2) {
			gfx_text_alt(text, 0, second, sizeof second, &more);
			line = ui_key_names(second, first, sizeof first, false);
		}
		if (line != first) {
			memcpy(first, second, sizeof first);	// the rest below may point into it, and second is reused
		}
		const char *rest = ui_wrap_line(f, first, w - 2, fit, sizeof fit);
		int y = top + (h - f->h) / 2;
		second[0] = 0;
		if (*rest) {
			// Two lines: the first ends at a word, the rest goes below it
			size_t n = strlen(rest);
			n = n < sizeof second - 1 ? n : sizeof second - 1;
			memcpy(second, rest, n);
			second[n] = 0;
			y -= f->h;
			top = ui_overlay_top(y, inset + 5, inset + 5);
		}
		panels_pane(top, gfx_height() - PANELS_HOME_INSET - top, ui_col.fg);
		gfx_text(f, inset + (w - gfx_text_width(f, fit)) / 2, y, fit, ui_col.bg);
		if (second[0]) {
			gfx_text_fit(f, second, w - 2, fit, sizeof fit);
			gfx_text(f, inset + (w - gfx_text_width(f, fit)) / 2, y + f->h, fit, ui_col.bg);
		}
		return;
	}
	panels_pane(top, h, ui_col.hint_bg);
	// The hint row in the longest form that fits it; its last groups go, whole, before it is shortened
	char form[64], row[64];
	const font_t *rf = &font_aa_8x16;
	int room = gfx_width() - 30;
	unsigned keys = 0;
	bool more = true;
	for (int level = 0; more; level++) {
		gfx_text_alt(text, level, form, sizeof form, &more);
		unsigned k = panels_hint_parts(ui_key_names(form, fit, sizeof fit, true), flags, row, sizeof row);
		if (level == 0)
			keys = k;	// the keys the whole hint declares; a shorter form may leave one out
		if (gfx_text_width(rf, row) <= room)
			break;
	}
	while (gfx_text_width(rf, row) > room) {
		char *cut = NULL;
		for (char *p = strstr(row, "  "); p != NULL; p = strstr(p + 1, "  "))
			cut = p;
		if (cut == NULL)
			break;
		*cut = 0;
	}
	if (row[0]) {
		gfx_text_fit(rf, row, room, fit, sizeof fit);
		gfx_text(rf, 15, panels_hint_top(), fit, ui_col.dim);
	}
	const char *left = (flags & (PANELS_BAR_MENU | PANELS_BAR_IDLE)) ? TR(FIND_CHIP_MENU, "Menu") :
		(keys & 1u) || (flags & PANELS_BAR_KEYS) || ((flags & UI_BAR_LIST) && (flags & UI_BAR_NUMBERED)) ? "OK" : NULL;
	const char *right = soft_key != NULL ? soft_key :
		(keys & 2u) || (flags & (UI_BAR_LIST | PANELS_BAR_KEYS)) ? TR(HINT_BACK, "Back") : NULL;
	panels_bar_labels(left, right != NULL && right[0] ? right : NULL, top, panels_bar_fg);
}

// The bar with one sentence in place of the two labels (Home while the keys are locked: what unlocks them), in the
// largest font one of its forms fits, whether Hint row is on or off
static void panels_bar_sentence(const char *text)
{
	const ui_layout_t *L = &ui_layout;
	const int inset = PANELS_HOME_INSET, margin = 4;	// a sentence spans the bar: "Locked: hold * to unlock" in 12x24
	int top = panels_footer_top(), room = gfx_width() - 2 * inset - 2 * margin;
	char form[64], names[64];
	const font_t *fonts[] = { FONT_AA_LARGEST, &font_aa_12x24, L->small };
	const char *line = NULL;
	const font_t *f = L->small;
#if CONFIG_VOLUME_BAR
	ui_hint_draws++;
#endif
	panels_pane(top, PANELS_FOOTER_H, ui_col.hint_bg);
	for (unsigned i = 0; i < sizeof fonts / sizeof fonts[0] && line == NULL; i++) {
		bool more = true;
		for (int level = 0; more; level++) {
			gfx_text_alt(text, level, form, sizeof form, &more);
			const char *named = ui_key_names(form, names, sizeof names, false);
			if (gfx_text_width(fonts[i], named) <= room) {
				line = named;
				f = fonts[i];
				break;
			}
		}
	}
	if (line == NULL)
		line = ui_key_names(form, names, sizeof names, false);	// the shortest form, shortened
	char fit[64];
	gfx_text_fit(f, line, room, fit, sizeof fit);
	gfx_text(f, inset + margin, top + (PANELS_FOOTER_H - f->h) / 2, fit, panels_bar_fg);
}

static void panels_center(const font_t *f, int y, const char *text, color_t c)
{
	char fit[64];
	int w = gfx_width();
	ui_fit_end(f, text, w - 30, fit, sizeof fit);
	gfx_text(f, (w - gfx_text_width(f, fit)) / 2, y, fit, c);
}

static void panels_frequency(sbuf_t *b, uint32_t hz)
{
	if (hz) {
		sb_freq(b, hz);
		sb_str(b, " MHz");
	}
}

static uint32_t panels_talk_id(const radio_view_t *v, bool *private_call, const char **label)
{
	*label = "";
	*private_call = false;
#if CONFIG_DMR
	if (v->ch.mode == RADIO_MODE_DMR && v->dmr.active) {
		const radio_dmr_view_t *d = &v->dmr;
		if (v->state == RS_TX && d->call.dst != 0 && !v->aprs_tx && !ui_sending_data(v)) {
			*label = !d->call.dst_name[0] && d->call.dst == d->target_id && d->call.group != d->target_private ?
				 d->target_name : d->call.dst_name;
			*private_call = !d->call.group;
			return d->call.dst;
		}
		*label = d->target_name;
		*private_call = d->target_private;
		return d->target_id;
	}
#endif
#if CONFIG_P25
	if (v->ch.mode == RADIO_MODE_P25 && !v->ch.rx_only) {
		*private_call = (v->p25.flags & RADIO_P25_TALK_UNIT) != 0;
		return v->p25.talk;
	}
#endif
#if CONFIG_NXDN
	if (v->ch.mode == RADIO_MODE_NXDN && !v->ch.rx_only) {
		*private_call = (v->nxdn.flags & RADIO_NXDN_TALK_UNIT) != 0;
		return v->nxdn.talk;
	}
#endif
	(void)v;
	return 0;
}

static void panels_target(sbuf_t *b, uint32_t id, bool private_call)
{
	if (id == 0xFFFFFFu) {
		sb_str(b, TR(VALUE_ALL_CALL, "All call"));
	} else {
		sb_str(b, private_call ? "ID " : "TG ");
		sb_u32(b, id, 1);
	}
}

// The talk target as Home names it. Talkgroup name first (Settings > Display, a side key; name_first): its name, with
// the number after it where with_number asks; else, as the reference theme's talk line reads, TG or ID and the number,
// then the name. Without a name, the number alone
static void panels_target_text(sbuf_t *b, uint32_t id, bool private_call, const char *label, bool with_number,
	bool name_first)
{
	bool named = id != 0xFFFFFFu && label[0] && !ui_name_is_id(label, id, private_call);
	if (named && !name_first) {
		panels_target(b, id, private_call);
		sb_str(b, " " GLYPH_DOT " ");
		sb_str(b, label);
		return;
	}
	if (named)
		sb_str(b, label);
	if (named && !with_number)
		return;
	if (named)
		sb_str(b, " " GLYPH_DOT " ");
	panels_target(b, id, private_call);
}

// Home's main line: the frequency being typed, the VFO's frequency (the TX one, red, while it transmits), else the
// channel's name from the codeplug, or its frequency when it has none
static const char *panels_home_main(const radio_view_t *v, const ui_state_t *s, char *out, size_t cap, color_t *c)
{
	sbuf_t b;
	sb_init(&b, out, cap);
	*c = ui_col.fg;
	if (s->entry) {
		ui_home_entry_text(s, out, cap);
		*c = ui_col.warn;
		return out;
	}
	if (v->vfo) {
		bool tx = v->state == RS_TX && !v->aprs_tx;
		panels_frequency(&b, tx ? v->ch.tx_hz : v->ch.rx_hz);
		*c = tx ? ui_col.tx : ui_col.fg;
		return out;
	}
	if (v->ch.name[0])
		return v->ch.name;
	panels_frequency(&b, v->ch.rx_hz);
	return out;
}

// Commercial shows the talk target only where it isn't the channel's own: one chosen for now, the reply window's, a
// one-shot call's. In the VFO the main line is a frequency, which names no target, so the VFO's is shown too
static bool panels_target_differs(const radio_view_t *v)
{
#if CONFIG_DMR
	const radio_dmr_view_t *d = &v->dmr;
	return v->ch.mode == RADIO_MODE_DMR && d->active &&
		(v->vfo || d->target_manual || d->reply_active || d->one_shot);
#else
	(void)v;
	return false;
#endif
}

// Amateur's details: the frequency, the timeslot and color code, a VFO's split, a dot for a temporary change
static void panels_home_details(const radio_view_t *v, const ui_state_t *s, int y)
{
	const font_t *f = ui_layout.small;
	sbuf_t b;
	char buf[64];
	sb_init(&b, buf, sizeof buf);
	if (v->temporary)
		sb_str(&b, GLYPH_DOT " ");
	panels_frequency(&b, v->ch.rx_hz);
#if CONFIG_DMR
	if (v->ch.mode == RADIO_MODE_DMR) {
		sb_str(&b, " " GLYPH_DOT " TS");
		sb_u32(&b, v->dmr.ts ? v->dmr.ts : v->ch.timeslot, 1);
		sb_str(&b, " " GLYPH_DOT " CC");
		sb_u32(&b, v->dmr.active ? v->dmr.cc : v->ch.color_code, 1);
	}
#endif
	if (v->vfo && !s->entry && v->ch.tx_hz && v->ch.tx_hz != v->ch.rx_hz && !v->ch.rx_only) {
		sb_str(&b, " " GLYPH_DOT " TX ");
		sb_freq(&b, v->ch.tx_hz);
	}
	if (buf[0])
		panels_center(f, y, buf, ui_col.dim);
}

// From one line of Home's text to the next in font f: a whole cell, so no line's box overlaps another's
static int panels_pitch(const font_t *f)
{
	return f->h;
}

// Where the lines at the foot of Home's channel pane start (its rule), the pane ending at bottom, the first line in
// font f
static int panels_home_lines_top(int bottom, const font_t *f)
{
	return bottom - 1 - ui_layout.small->h - panels_pitch(f) - 2;
}

// The two lines at the bottom of Home's channel area, under a rule: during a DMR call who is calling (the talker alias,
// the contact's name or the ID) and to which talkgroup, while transmitting where PTT talks to and the time left;
// otherwise one line of what needs attention (a refused PTT, a timed transmission, a missing frequency or target, ...).
// The signal is the strip's bars. The lines keep their place, so nothing above them
// moves when a call comes in
static void panels_home_lines(const radio_view_t *v, const ui_state_t *s, int bottom, const font_t *f)
{
	const ui_layout_t *L = &ui_layout;
	const font_t *f2 = L->small;
	const char *text = NULL, *text2 = NULL;
	color_t c = ui_col.warn, c2 = ui_col.dim;
	char buf[72], buf2[48];
	sbuf_t b, b2;
	sb_init(&b, buf, sizeof buf);
	sb_init(&b2, buf2, sizeof buf2);
	if (v->state == RS_TX_DENIED) {
		text = ui_deny_text(v, v->tx_deny, buf, sizeof buf);
	} else if (v->state == RS_TX) {
		bool voice = !v->aprs_tx && !ui_sending_data(v);
		bool private_call;
		const char *label;
		uint32_t id = voice ? panels_talk_id(v, &private_call, &label) : 0;
		if (id && v->ch.mode == RADIO_MODE_DMR) {
			panels_target_text(&b, id, private_call, label, false, s->label_first);
			text = buf;
			c = ui_col.fg;
		}
		if (voice && v->tot_s) {
			uint32_t elapsed = v->tx_elapsed_ms / 1000u;
			sbuf_t *t = text != NULL ? &b2 : &b;
			sb_mmss(t, elapsed < v->tot_s ? v->tot_s - elapsed : 0);
			color_t tc = v->tot_warning ? ui_col.warn : ui_col.tx;
			if (text != NULL) {
				text2 = buf2;
				c2 = tc;
			} else {
				text = buf;
				c = tc;
			}
		}
#if CONFIG_DMR
	} else if (v->state == RS_RX && v->ch.mode == RADIO_MODE_DMR && v->dmr.active &&
		   (v->dmr.state == RDMR_RX || v->dmr.state == RDMR_RX_OTHER)) {
		const radio_dmr_call_t *call = &v->dmr.call;
		if (s->alias_show && v->dmr.alias[0] && v->dmr.state == RDMR_RX) {
			sb_str(&b, v->dmr.alias);
		} else if (call->src_name[0]) {
			sb_str(&b, call->src_name);
		} else {
			sb_str(&b, "ID ");
			sb_u32(&b, call->src, 1);
		}
		text = buf;
		c = ui_col.priv;
		if (call->group)
			panels_target_text(&b2, call->dst, false, call->dst_name, true, true);
		else
			sb_str(&b2, call->to_us ? TR(ACT_ROW_PRIVATE_CALL, "Private call") : "");
		text2 = buf2[0] ? buf2 : NULL;
#endif
	} else if (board_hwopt_pending() != NULL) {
		ui_hwopt_needed(&b);
		text = buf;
	} else if (v->ch.rx_hz && !v->ch.tx_hz && !v->ch.rx_only && !v->am && !v->bcast) {
		text = TR(HOME_STATE_NO_TX_FREQ, "No TX frequency");
#if CONFIG_DMR
	} else if (v->ch.mode == RADIO_MODE_DMR && !v->dmr.active && s->dmr_off) {
		text = TR(HOME_STATE_DMR_MODEM_OFF, "DMR modem off");
	} else if (v->ch.mode == RADIO_MODE_DMR && !v->vfo && v->dmr.active && !v->dmr.target_id &&
		   !v->ch.rx_only && !s->gui_profile) {
		text = TR(HOME_STATE_NO_TARGET, "No talk target");
#endif
	}
	int y2 = bottom - 1 - f2->h, y = y2 - panels_pitch(f);
	if (text != NULL) {
		gfx_hline(15, y - 2, gfx_width() - 30, ui_col.line);
		panels_center(f, y, text, c);
	}
	if (text2 != NULL)
		panels_center(f2, y2, text2, c2);
}

// A line in the larger fonts, enlarged by quarters where it fits (gfx_text_title), else in the medium font
static void panels_home_title(const font_t *f, int top, int quarters, const char *text, color_t c)
{
	const ui_layout_t *L = &ui_layout;
	int w = gfx_width();
	if (!gfx_text_title(f, w / 2, top + f->ascent * quarters / 4, w - 30, text, c, quarters) &&
	    !gfx_text_title(L->medium, w / 2, top + L->medium->ascent * quarters / 4, w - 30, text, c, quarters))
		panels_center(L->medium, top, text, c);
}

// Text whose capitals are centered on cy, allowing for the font's blank rows
static void panels_native_caps(const font_t *font, int x, int cy, const char *label, color_t ink)
{
	int top, rows;
	font_glyph(font, 'M', &top, &rows);
	int y = cy - rows / 2 - top;
	gfx_text(font, x, y > 0 ? y : 0, label, ink);
}

// Home's frame on the gutter: the strip (no mode word), the mode bar under it (the mode in the largest font, and the GPS
// clock where the strip had no room for it) and the channel pane down to bottom. Returns the pane's top
static int panels_home_frame(const radio_view_t *v, const ui_state_t *s, int bottom)
{
	// The panes' and the bar's colors first: on a display with a 16-color palette the text's shades take what is left
	const color_t colors[] = { panels_gutter, ui_col.strip_bg, panels_row_bg, ui_col.bg, ui_col.fg, ui_col.hint_bg,
		panels_bar_fg };
	gfx_clear(panels_gutter);
	gfx_reserve_colors(colors, sizeof colors / sizeof colors[0]);
	bool clock = panels_draw_status(v, v->state, s, panels_gutter, true);
	int y = ui_layout.strip_h + 1;
	panels_pane(y, PANELS_MODE_H, panels_row_bg);
	panels_native_caps(FONT_AA_LARGEST, 15, y + PANELS_MODE_H / 2, panels_status_mode(v, s), ui_col.fg);
	char time[24];
	if (clock && panels_clock(s, time, sizeof time)) {
		// The GPS clock the crowded strip had no room for, at the mode bar's right end (as in step 2)
		const font_t *f = &font_aa_8x16;
		gfx_text(f, gfx_width() - 15 - gfx_text_width(f, time), y + (PANELS_MODE_H - f->h) / 2, time, ui_col.dim);
	}
	y += PANELS_MODE_H + 2;
	panels_pane(y, bottom - y, ui_col.bg);
	return y;
}

// The flags of Home's bar: Menu on the left where OK opens it (ui_home_ok_menu), and its idle hint (s->home_hint) starts
// with the menu's group; Hint row off leaves out the hints
static uint8_t panels_home_bar(const ui_state_t *s, const char *hint)
{
	return (uint8_t)((ui_home_ok_menu(s) ? PANELS_BAR_MENU : 0) | (hint != NULL && hint == s->home_hint ? PANELS_BAR_IDLE : 0) |
		(s->hints ? 0 : UI_BAR_NO_HINTS));
}

// Where Home's channel pane ends: above the hint row where the bar has one, or the knob's volume bar while it shows
// (ui_draw_volume_bar, in the rows from the hint row's top)
static int panels_home_bottom(const ui_state_t *s, const char *hint)
{
	bool row = s->volbar >= 0 || (s->toast == NULL && s->hints && !s->locked && hint != NULL &&
		panels_hint_has_row(hint, panels_home_bar(s, hint)));
	return row ? panels_hint_top() - 2 : panels_footer_top() - PANELS_HOME_GAP;
}

static void panels_home_footer(const ui_state_t *s, const char *hint, int bottom)
{
	// Above the pane's lower edge; what it reads of the screen under it stays clear of the panes' rounded corners
	ui_banner_box(s, bottom, PANELS_HOME_INSET + 5, PANELS_HOME_INSET + 5, ui_col.strip_bg);
	if (s->toast != NULL)
		ui_draw_hint(s->toast, true);
	else if (s->locked && hint != NULL)
		panels_bar_sentence(hint);	// what unlocks the keys, in large text (as in step 2), Hint row on or off
	else if (hint != NULL)
		panels_bottom(hint, panels_home_bar(s, hint), ui_home_soft_key(s));	// the soft keys stay with Hint row off
}

// Home: the frame, then in the channel pane the zone, the channel's name as the main line (larger in Commercial), the
// talk target where the layout shows it (Commercial: only one that isn't the channel's own; Amateur: always) and
// Amateur's details, with the two lines of a call or of what needs attention at the pane's foot. Large text keeps the
// zone, the main line and the target, larger, without the details. Where the pane is short (the hint row above the bar) the main line takes fewer
// quarters of enlargement, then the details row and the zone line go, before anything overlaps. noinline: inlined in
// panels_home, its buffers would be in the frame the reference Home is drawn from
static void __attribute__((noinline)) panels_draw_home(const radio_view_t *v, const ui_state_t *s)
{
	const ui_layout_t *L = &ui_layout;
	const char *hint = ui_home_hint(v, s);
	int bottom = panels_home_bottom(s, hint);
	int top = panels_home_frame(v, s, bottom) + 2;	// the zone's cell clear of the mode's
	bool commercial = s->gui_profile == 0, large = s->large_text;
	char buf[64], target[48];
	color_t c;
	const char *main_line = panels_home_main(v, s, buf, sizeof buf, &c);
	bool private_call;
	const char *label;
	uint32_t id = panels_talk_id(v, &private_call, &label);
	sbuf_t b;
	sb_init(&b, target, sizeof target);
	if (s->entry || v->ch.rx_only || (commercial && !panels_target_differs(v))) {
		// no target line
	} else if (id) {
		panels_target_text(&b, id, private_call, label, !commercial && !large, s->label_first);
#if CONFIG_DMR
	} else if (!commercial && v->ch.mode == RADIO_MODE_DMR && v->dmr.active) {
		sb_str(&b, TR(HOME_STATE_NO_TARGET, "No talk target"));
#endif
	}
	// The lines at the foot keep their place: what is above fits over them. Where it doesn't, in this order: the main
	// line takes fewer quarters of enlargement, the target, the zone and the foot's first line take the small font,
	// then the details row and the zone line go
	const font_t *tf = large ? L->large : L->medium, *zf = L->medium, *lf = L->medium;
	bool zone = true, details = !commercial && !large && !s->entry;
	int quarters = commercial || large ? 6 : 4;
	for (;;) {
		int need = (zone ? panels_pitch(zf) : 0) + L->large->h * quarters / 4 +
			(target[0] ? panels_pitch(tf) : 0) + (details ? L->small->h : 0);
		if (need <= panels_home_lines_top(bottom, lf) - 1 - top)
			break;
		if (quarters > 4)
			quarters--;
		else if (target[0] && tf == L->medium)
			tf = L->small;
		else if (zone && zf == L->medium)
			zf = L->small;
		else if (lf == L->medium)
			lf = L->small;
		else if (details)
			details = false;
		else if (zone)
			zone = false;
		else
			break;
	}
	int y = top;
	if (zone) {
		panels_center(zf, y, v->vfo ? "VFO" : v->zone_name, ui_col.dim);
		y += panels_pitch(zf);
	}
	panels_home_title(L->large, y, quarters, main_line, c);
	y += L->large->h * quarters / 4;
	if (target[0]) {
		if (large)
			panels_home_title(L->large, y, 4, target, ui_col.fg);
		else
			panels_center(tf, y, target, ui_col.fg);
		y += panels_pitch(tf);
	}
	if (details)
		panels_home_details(v, s, y);

	panels_home_lines(v, s, bottom, lf);
	panels_home_footer(s, hint, bottom);
}

static bool panels_operational_home(const radio_view_t *v)
{
#if CONFIG_DMR_TRUNK
	if (v->t3.sys != 0)
		return true;
#endif
	if (v->ch.kind == CODEPLUG_KIND_DMR_TRUNK || v->ch.kind == CODEPLUG_KIND_P25_TRUNK ||
	    (v->ch.kind != CODEPLUG_KIND_NONE && !codeplug_kind_built(v->ch.kind)))
		return true;
#if CONFIG_DMR
	return v->ch.mode == RADIO_MODE_DMR && v->dmr.sfr_rpt;
#else
	return v->ch.mode == RADIO_MODE_DMR;
#endif
}
static void draw_menu_icon(uint8_t icon, int x, int y)
{
	if (icon == UI_MENU_ICON_SETTINGS) {
		static const uint32_t outline[] = {
			0x00F0, 0x00F0, 0x07FC, 0x0FFE, 0x1F1F, 0x0E0E, 0x0E0E,
			0x0E0F, 0x1F1F, 0x0FFE, 0x07FC, 0x00F0, 0x00F0,
		};
		static const uint32_t fill[] = {
			0x0000, 0x0060, 0x00F0, 0x071C, 0x0E0E, 0x0404, 0x0404,
			0x0406, 0x0E0E, 0x071C, 0x00F0, 0x0060, 0x0000,
		};
		PANELS_MARK(x, y, outline, 13, PANELS_GEAR_OUTLINE);
		PANELS_MARK(x, y, fill, 13, PANELS_GEAR_FILL);
		return;
	}
	if (icon == UI_MENU_ICON_PRIVACY) {
		panels_mask(x, y + 3, panels_key_13, 13, 7, ui_col.warn);
		return;
	}

	static const struct { uint8_t tint; uint32_t rows[13]; } marks[UI_MENU_ICON_COUNT] = {
		[UI_MENU_ICON_SOUND] = { 0, { 0x0000, 0x0080, 0x0188, 0x0384, 0x0FB2, 0x1F8A, 0x1F8A,
			0x1F8A, 0x0FB2, 0x0384, 0x0188, 0x0080, 0x0000 } },
		[UI_MENU_ICON_DISPLAY] = { 0, { 0x0FFE, 0x1803, 0x1803, 0x1803, 0x1803, 0x1803, 0x1803,
			0x1803, 0x0FFE, 0x00E0, 0x00E0, 0x03F8, 0x0000 } },
		[UI_MENU_ICON_TALK] = { 1, { 0x00E0, 0x01F0, 0x01F0, 0x01F0, 0x05F4, 0x05F4, 0x05F4,
			0x060C, 0x03F8, 0x0040, 0x0040, 0x01F0, 0x0000 } },
		[UI_MENU_ICON_LOCATION] = { 3, { 0x01F0, 0x071C, 0x060C, 0x0C46, 0x0CA6, 0x0C46, 0x060C,
			0x060C, 0x0318, 0x01B0, 0x00E0, 0x0040, 0x0000 } },
		[UI_MENU_ICON_CONNECTIONS] = { 0, { 0x0000, 0x0110, 0x0110, 0x0110, 0x01F0, 0x03F8, 0x03F8,
			0x03F8, 0x01F0, 0x0060, 0x0060, 0x0060, 0x0060 } },
		[UI_MENU_ICON_POWER] = { 1, { 0x00E0, 0x00E0, 0x07FC, 0x060C, 0x064C, 0x06CC, 0x06EC,
			0x066C, 0x064C, 0x060C, 0x07FC, 0x0000, 0x0000 } },
		[UI_MENU_ICON_KEYS] = { 0, { 0x07FC, 0x0C06, 0x0D56, 0x0C06, 0x0D56, 0x0C06, 0x0D56,
			0x0C06, 0x0D56, 0x0C06, 0x07FC, 0x0000, 0x0000 } },
		[UI_MENU_ICON_LOCK] = { 2, { 0x01F0, 0x0318, 0x0318, 0x0318, 0x07FC, 0x0C06, 0x0C46,
			0x0CE6, 0x0C46, 0x0C46, 0x0C06, 0x07FC, 0x0000 } },
		[UI_MENU_ICON_RADIO] = { 0, { 0x0000, 0x0300, 0x0300, 0x03F8, 0x07FC, 0x07FC, 0x060C,
			0x060C, 0x07FC, 0x060C, 0x07FC, 0x07FC, 0x03F8 } },
		[UI_MENU_ICON_TONES] = { 1, { 0x0000, 0x0306, 0x03FE, 0x0306, 0x0306, 0x0306, 0x0306,
			0x0306, 0x0F1E, 0x1F3E, 0x1E3C, 0x0000, 0x0000 } },
		[UI_MENU_ICON_ADVANCED] = { 2, { 0x0000, 0x0300, 0x1FFF, 0x1FFF, 0x0300, 0x0018, 0x1FFF,
			0x1FFF, 0x0018, 0x00C0, 0x1FFF, 0x1FFF, 0x00C0 } },
		[UI_MENU_ICON_CALLER] = { 1, { 0x0E00, 0x1E00, 0x1F00, 0x0F00, 0x0F00, 0x0700, 0x0780,
			0x038C, 0x03DE, 0x01FF, 0x00FF, 0x003E, 0x000C } },
		[UI_MENU_ICON_CONTACTS] = { 0, { 0x00E0, 0x01F0, 0x01F0, 0x01F0, 0x01F0, 0x00E0, 0x0000,
			0x03F8, 0x07FC, 0x0FFE, 0x0FFE, 0x07FC, 0x03F8 } },
		[UI_MENU_ICON_MESSAGES] = { 0, { 0x0000, 0x0FFE, 0x1FFF, 0x1803, 0x1C07, 0x1A0B, 0x1913,
			0x18E3, 0x1803, 0x1803, 0x1FFF, 0x0FFE, 0x0000 } },
		[UI_MENU_ICON_RECENT] = { 1, { 0x01F0, 0x060C, 0x0C06, 0x0842, 0x1843, 0x1843, 0x1873,
			0x1803, 0x1803, 0x0802, 0x0C06, 0x060C, 0x01F0 } },
		[UI_MENU_ICON_ZONES] = { 0, { 0x0000, 0x0000, 0x0000, 0x0FC0, 0x0FC0, 0x0FFE, 0x0FFF,
			0x0C03, 0x0FFF, 0x0FFF, 0x0FFF, 0x0FFF, 0x0000 } },
		[UI_MENU_ICON_CHANNELS] = { 1, { 0x0000, 0x0000, 0x0DFE, 0x01FE, 0x0000, 0x0000, 0x0DFE,
			0x01FE, 0x0000, 0x0000, 0x0DFE, 0x01FE, 0x0000 } },
		[UI_MENU_ICON_TOOLS] = { 2, { 0x0001, 0x0007, 0x000F, 0x001E, 0x0038, 0x01F0, 0x03E0,
			0x0770, 0x0EF0, 0x1DE0, 0x1BC0, 0x0F80, 0x0700 } },
	};
	const color_t colors[] = { ui_col.priv, ui_col.rx, ui_col.warn, ui_col.tx };
	PANELS_MARK(x, y, marks[icon].rows, 13, colors[marks[icon].tint]);
}

static void panels_row(int x, int y, int w, int h, color_t ink)
{
	// Three stepped bands round off the corners cleanly at native resolution.
	gfx_fill(x + 3, y, w - 6, h, ink);
	gfx_fill(x + 1, y + 1, w - 2, h - 2, ink);
	gfx_fill(x, y + 3, w, h - 6, ink);
}

// The focus's accent bar: the highlight's rounded left end in the accent color, the row's full height, so it reads as
// the edge of the highlight and not as a character before the text
static void panels_accent_bar(int x, int y, int h)
{
	gfx_fill(x, y + 3, 1, h - 6, panels_accent);
	gfx_fill(x + 1, y + 1, PANELS_ACCENT_W - 2, h - 2, panels_accent);
	gfx_fill(x + PANELS_ACCENT_W - 1, y, 1, h, panels_accent);
}

// A row or tile: its panel, the focus as the filled highlight with its accent bar (amber while its editor is open)
static void panels_cell_panel(int x, int y, int w, int h, bool focus, bool edit)
{
	if (!focus) {
		panels_row(x, y, w, h, panels_row_bg);
		return;
	}
	(void)edit;	// the value editor is a focused row; its markers carry the accent (panels_cell)
	panels_row(x, y, w, h, panels_row_focus);
	panels_accent_bar(x, y, h);
}

// One row height for every list: five rows between the title and the bar on 310 x 240, the small font (9x18 where the
// build has it) with its padding
static int panels_row_h(void)
{
	return 26;
}

// --- The theme ---

// Home. A trunked or single-frequency repeater channel, or one of a kind this build lacks, keeps the reference theme's
// Home body, drawn in this theme's strip, regions and footer (its top and bottom ops)
static void panels_home(const radio_view_t *v, const ui_state_t *s)
{
	if (panels_operational_home(v))
		ui_reference_home(v, s);
	else
		panels_draw_home(v, s);
}

#if CONFIG_DUAL_WATCH
// Dual watch's Home in Home's frame: its panes inset in the channel pane, no list of calls (the alias is a live receiving
// indication only), a refused PTT's reason centered. Commercial names the talk target where the channel number would be
static const ui_dual_style_t panels_dual[] = {
	{ .inset = 12, .deny_margin = 15, .target_label = true },	// Commercial
	{ .inset = 12, .deny_margin = 15 },				// Amateur
};

static void panels_home_dual(const radio_view_t *v, const ui_state_t *s)
{
	const char *hint = ui_dual_hint(v, s);
	int bottom = panels_home_bottom(s, hint);
	int top = panels_home_frame(v, s, bottom);
	ui_home_dual_body(v, s, top + 4, bottom - 4, &panels_dual[s->gui_profile != 0]);
	panels_home_footer(s, hint, bottom);
}
#endif

// The marks that move: the scan mark's highlight every 100 ms, the roaming mark every 125 ms while it searches or
// surveys (and the simulator's recording dot)
static bool panels_animate(const radio_view_t *v, const ui_state_t *s, uint32_t now_ms)
{
	if (v->scan != 0 && v->scan != 3 && now_ms / 100u != s->now_ms / 100u)
		return true;
#if CONFIG_DMR_ROAMING
	if ((v->roam.state == RROAM_STATE_SEARCH || v->roam.state == RROAM_STATE_SURVEY) &&
	    now_ms / 125u != s->now_ms / 125u)
		return true;
#endif
#ifdef GFX_FIT_TRACE
	if (s->gui_sim_recording && now_ms / 400u != s->now_ms / 400u)
		return true;
#endif
	return false;
}

// The palette entries the rows and icons need, taken before anything else is drawn
static void panels_list_colors(bool icons)
{
	const color_t row_colors[] = { panels_row_bg, panels_row_focus, panels_accent, ui_col.fg, ui_col.hint_bg,
		panels_bar_fg };
	const color_t icon_colors[] = { ui_col.priv, ui_col.rx, ui_col.warn, ui_col.tx,
		PANELS_GEAR_FILL, PANELS_GEAR_OUTLINE };
	gfx_reserve_colors(row_colors, sizeof row_colors / sizeof row_colors[0]);
	if (icons)
		gfx_reserve_colors(icon_colors, sizeof icon_colors / sizeof icon_colors[0]);
}

static bool panels_titled(const ui_list_t *l)
{
	return l->prefix || l->name;
}

// The scroll position, "n/N" in the 8x16 font: the n-th entry of the list's count. Returns its width
static int panels_badge(const ui_list_t *l, unsigned n, char *out, size_t cap)
{
	sbuf_t b;
	sb_init(&b, out, cap);
	sb_u32(&b, n, 1);
	sb_chr(&b, '/');
	sb_u32(&b, l->count, 1);
	return gfx_text_width(&font_aa_8x16, out);
}

#define PANELS_GRID_ROWS 3	// a grid's rows of tiles, each two lines of text high

// Where a list's cells end: a row's panel may run under the hint row's text where the row's text ends above it; a grid's
// tiles end above the hint row. Without the bar (Hint row off) at y_end
static int panels_list_end(const ui_list_t *l, int y_end)
{
	if (y_end != ui_layout.hint_y)
		return y_end;
	// The last row's text ends above the hint row's; its panel may reach under the row's blank top
	int row_text_end = panels_hint_top() + (panels_row_h() - ui_layout.small->h) / 2;
	return l->kind == UI_LIST_PLAIN ? row_text_end : panels_hint_top() - 2;
}

// The entries a screen of the list shows (for the room the title keeps for the scroll position)
static int panels_list_page(const ui_list_t *l, int y)
{
	if (l->kind == UI_LIST_HUB)
		return 3 * PANELS_GRID_ROWS;
	if (l->kind == UI_LIST_MENU)
		return 2 * PANELS_GRID_ROWS;
	int rows = (panels_list_end(l, ui_layout.hint_y) - y) / panels_row_h();
	return rows < 1 ? 1 : rows;
}

// A list's palette entries, then its heading: the strip and the title, with room kept at its right for the scroll
// position where the list has more entries than a screen shows, or the strip alone for a list without a title
static int panels_list_head(const radio_view_t *v, const ui_state_t *s, const ui_list_t *l, bool draw)
{
	if (draw) {
		panels_list_colors(l->kind != UI_LIST_PLAIN);
		// The strip straight from here, not through the top op: its title's frame would join the deepest path of
		// the UI task (make stack-check)
		panels_draw_status(v, v->state, s, ui_col.bg, false);
	}
	if (!panels_titled(l))
		return ui_layout.strip_h;
	char badge[16];
	int y = panels_title(NULL, NULL, 0, false);
	int reserve = l->count > panels_list_page(l, y) ? panels_badge(l, l->count, badge, sizeof badge) + 8 : 0;
	return panels_title(l->prefix, l->name, reserve, draw);
}

// Rows of one height, inset from the edges, as many as fit. The main menu as a grid of three columns in the keypad's
// order (1 2 3 across), the first level of Settings as one of two columns (1 2 3 down); a grid's tiles are two lines
// of the 8x16 font high, one fixed split of the columns for the whole list, a page at a time
static void panels_list_geom(const ui_list_t *l, int y, int y_end, ui_list_geom_t *g)
{
	g->y = (int16_t)y;
	y_end = panels_list_end(l, y_end);
	int cols = l->kind == UI_LIST_HUB ? 3 : l->kind == UI_LIST_MENU ? 2 : 1;
	g->x = cols == 3 ? 5 : 7;
	g->cols = (uint8_t)cols;
	g->col_gap = (int8_t)(cols == 3 ? 3 : cols == 2 ? 8 : 0);
	g->paged = cols > 1;
	g->across = cols == 3;
	g->font = cols > 1 ? &font_aa_8x16 : ui_layout.small;
	int rows = cols > 1 ? PANELS_GRID_ROWS : (y_end - y) / panels_row_h();
	g->rows = (uint8_t)(rows < 1 ? 1 : rows);
	g->pitch = (int16_t)(cols > 1 ? (y_end - y) / PANELS_GRID_ROWS : panels_row_h());
	int inner_w = gfx_width() - 2 * g->x, col_w = (inner_w - (cols - 1) * g->col_gap) / cols;
	for (int i = 0; i < UI_LIST_COLS; i++)
		g->col_w[i] = (int16_t)(i < cols - 1 ? col_w : i == cols - 1 ? inner_w - (cols - 1) * (col_w + g->col_gap) : 0);
}

// A grid tile's text, two lines of the 8x16 font. The main menu's: its icon, its digit and its value (shortened to
// the room) on the first line, its label on the second. Settings': its icon, its digit and its label, the label going
// on to the second line at a word where it is too long for one. A label two lines can't hold is shortened
static void panels_tile_text(const ui_list_t *l, const ui_cell_t *c, const ui_item_t *it, color_t fg, color_t vfg,
	color_t num_ink)
{
	const font_t *f = &font_aa_8x16;
	char line[40], fit[40];
	bool hub = l->kind == UI_LIST_HUB;
	int x = c->x + (hub ? 7 : PANELS_TEXT_X), right = c->x + c->w - 3;
	int label_x = x, text_x = x + (it->icon != UI_MENU_ICON_NONE ? 17 : 0) + (it->num ? 2 * f->w : 0);
	const char *rest = "";
	if (!hub) {
		// Broken at a space; a first word too long for the room beside the icon and the digit goes whole to the
		// second line, which starts under the icon
		rest = ui_wrap_line(f, it->label, right - text_x, line, sizeof line);
		const char *sp = strchr(it->label, ' ');
		size_t first = sp != NULL ? (size_t)(sp - it->label) : strlen(it->label);
		if (*rest && strlen(line) < first) {
			line[0] = 0;
			rest = it->label;
		}
	}
	// Two lines, or one centered in the tile (a label that fits beside the icon and the digit)
	int y1 = !hub && !*rest ? c->y + (c->h - f->h) / 2 + 1 : c->y + c->h / 2 - f->h, y2 = y1 + f->h;
	if (it->icon != UI_MENU_ICON_NONE) {
		draw_menu_icon(it->icon, x, y1 + (f->h - 13) / 2);
		x += 17;
	}
	if (it->num) {
		char n[3] = { it->num, ' ', 0 };
		x = gfx_text(f, x, y1, n, num_ink);
	}
	if (hub) {
		// The value's longest form that fits whole, else its first shortened at the end: a zone's name has no
		// shorter form
		int room = right - x - f->w;
		bool more = true, whole = false;
		for (int level = 0; more && !whole; level++) {
			gfx_text_alt(it->value, level, line, sizeof line, &more);
			whole = line[0] && gfx_text_width(f, line) <= room;
		}
		if (!whole)
			gfx_text_alt(it->value, 0, line, sizeof line, &more);
		if (line[0]) {
			ui_fit_end(f, line, room, fit, sizeof fit);
			gfx_text(f, right - gfx_text_width(f, fit), y1, fit, vfg);
		}
		ui_fit_end(f, it->label, right - label_x, fit, sizeof fit);
		gfx_text(f, label_x, y2, fit, fg);
		return;
	}
	gfx_text(f, x, y1, line, fg);
	if (*rest) {
		int rx = line[0] ? x : label_x;
		ui_fit_end(f, rest, right - rx, fit, sizeof fit);
		gfx_text(f, rx, y2, fit, fg);
	}
}

// Each row or tile a rounded panel, the focus filled with its accent bar; a row has the key's digit, the label and the
// value in the 8x16 font, shortened where it is too long, never in a smaller font; a tile as panels_tile_text
static void panels_cell(const ui_list_t *l, const ui_list_geom_t *g, const ui_cell_t *c, const ui_item_t *it)
{
	color_t disabled = ui_col.dim;
	uint8_t flags = it->flags;
	const char *value = it->value;
	bool focus = c->focus;
	color_t fg = (flags & UI_ITEM_DIM) ? disabled : (flags & UI_ITEM_WARN) ? ui_col.warn : ui_col.fg;
	if (flags & UI_ITEM_INFO)
		fg = ui_col.dim;
	color_t vfg = (flags & UI_ITEM_DIM) ? disabled : ui_col.dim;
	if (flags & UI_ITEM_TEMP)
		vfg = ui_col.warn;
	if (!(flags & UI_ITEM_INFO) || focus)
		panels_cell_panel(c->x, c->y + PANELS_ROW_GAP, c->w, c->h - 2 * PANELS_ROW_GAP, focus,
			(flags & UI_ITEM_EDIT) != 0);
	if (focus) {
		fg = ui_col.focus_fg;
		vfg = fg;
	}
	if (g->cols > 1) {
		panels_tile_text(l, c, it, fg, vfg, focus ? fg : ui_col.dim);
		return;
	}
	int x = c->x + PANELS_TEXT_X;
	if (l->kind != UI_LIST_PLAIN && it->icon != UI_MENU_ICON_NONE) {
		draw_menu_icon(it->icon, x, c->y + (c->h - 13) / 2);
		x += 17;
	}
	int right = c->x + c->w - 5;
	const font_t *f = g->font;
	int ty = c->y + (c->h - f->h) / 2;
	if (it->num) {
		char n[3] = { it->num, ' ', 0 };
		x = gfx_text(f, x, ty, n, focus ? fg : ui_col.dim);
	}
	if (focus && (flags & UI_ITEM_EDIT)) {
		ui_row_edit(f, x, ty, right, it->label, value, fg, panels_accent);
		return;
	}
	ui_row_text(f, x, ty, right, it->label, value, fg, vfg, (flags & UI_ITEM_PREVIEW) != 0);
}

// The scroll position in the title's row, "n/N" for the focused entry, where the list has more entries than its screen
// shows. Before the cells: its shades take their palette entries ahead of the cells' on a 16-color display (gfx.c)
static void panels_scroll(const ui_list_t *l, const ui_list_geom_t *g, uint16_t top, uint16_t count, bool over)
{
	(void)top;
	if (over || count <= g->rows * g->cols || !panels_titled(l))
		return;
	char badge[16];
	const font_t *f = &font_aa_8x16;
	int w = panels_badge(l, l->focus < count ? l->focus + 1u : count, badge, sizeof badge);
	gfx_text(f, gfx_width() - 15 - w, ui_layout.strip_h + PANELS_TITLE_PAD + (ui_layout.medium->h - f->h) / 2, badge,
		ui_col.dim);
}

// A Quick tile's name: the theme's whole word where Quick's own name is a short one, else the tile's name, each in
// the longest of its text alternates that fits room (the others are a narrow tile's shorter words)
static const char *panels_quick_word(uint8_t kind, const char *name, int room, char *out, size_t cap)
{
	const font_t *f = &font_aa_8x16;
	const char *word = NULL;
	switch (kind) {
	case QT_BRIGHTNESS:
		word = TR(SET_LABEL_BRIGHTNESS, "Brightness");
		break;
	case QT_CC:
		word = TR(CHAN_LABEL_COLOR_CODE, "Color code");
		break;
	case QT_BANDWIDTH:
		word = TR(CHAN_LABEL_BANDWIDTH, "Bandwidth");
		break;
	case QT_SPLIT:
		word = TR(QUICK_LABEL_TX_FREQUENCY, "TX frequency");
		break;
	default:
		break;
	}
	if (word != NULL && gfx_text_width(f, word) <= room)
		return word;
	bool more = true;
	for (int level = 0; more; level++) {
		gfx_text_alt(name, level, out, cap, &more);
		if (gfx_text_width(f, out) <= room)
			break;
	}
	return out;
}

// The typed TX frequency, in the tile that edits it ("146.52_._"), as the hint row has it
static void panels_quick_entry(const ui_state_t *s, char *out, size_t cap)
{
	sbuf_t b;
	sb_init(&b, out, cap);
	for (uint8_t i = 0; i < UI_ENTRY_MAX; i++) {
		if (i == UI_ENTRY_MHZ)
			sb_chr(&b, '.');
		sb_chr(&b, i < s->entry_len ? s->entry_digits[i] : '_');
	}
}

// Quick settings: the tiles 1-9 as rounded panels in three rows of three under the title, the focus as a list's. Each
// has two lines of the 8x16 font: its name in whole words (panels_quick_word), then its key's digit and its value,
// between arrows while it edits (the TX frequency's typed digits). The bottom row as the reference theme's Quick has it
static void panels_quick(const radio_view_t *v, const ui_state_t *s)
{
	const font_t *f = &font_aa_8x16;
	char label[24], word[24], value[16], fit[24];
	const char *split;
	gfx_clear(ui_col.bg);
	panels_list_colors(false);
	panels_draw_status(v, v->state, s, ui_col.bg, false);
	int top = panels_title(TR(QUICK_TITLE, "Quick"), v->vfo ? "VFO" : v->ch.name, 0, true);
	int bottom = panels_hint_top() - 2;	// Quick always has its bottom row, as the reference theme's
	const int x0 = 5, gap = 3;
	int tw = (gfx_width() - 2 * x0 - 2 * gap) / 3, th = (bottom - top) / 3;
	int room = tw - 10;
	for (int i = 0; i < UI_QUICK_TILES; i++) {
		int x = x0 + (i % 3) * (tw + gap), y = top + (i / 3) * th;
		bool ok = ui_quick_tile(i, v, s, label, sizeof label, value, sizeof value);
		uint8_t kind = ui_quick_kind(i, v);
		bool focus = i == s->quick_focus, edit = focus && s->quick_edit;
		color_t fg = focus ? ui_col.focus_fg : ok ? ui_col.fg : ui_col.dim;
		panels_cell_panel(x, y + PANELS_ROW_GAP, tw, th - 2 * PANELS_ROW_GAP, focus, edit);
		int y1 = y + th / 2 - f->h, y2 = y1 + f->h;
		const char *name = panels_quick_word(kind, label + 1, room, word, sizeof word);
		gfx_text_fit(f, name, room, fit, sizeof fit);
		gfx_text(f, x + 7, y1, fit, focus ? fg : ui_col.dim);
		char key[2] = { label[0], 0 };
		int vx = gfx_text(f, x + 7, y2, key, focus ? fg : ui_col.dim) + 4;
		bool typed = edit && kind == QT_SPLIT && s->entry_len > 0;
		if (typed)
			panels_quick_entry(s, value, sizeof value);	// the digits say it edits: no arrows, room for all
		if (edit && !typed) {
			// Editing: arrows around the value say Up/Dn changes it
			sbuf_t b;
			sb_init(&b, fit, sizeof fit);
			sb_chr(&b, '<');
			sb_str(&b, value);
			sb_chr(&b, '>');
			str_copy(value, sizeof value, fit);
		}
		int right = x + tw - 4;
		gfx_text_fit(f, value, right - vx, fit, sizeof fit);
		int fw = gfx_text_width(f, fit), cx = x + (tw - fw) / 2;
		gfx_text(f, cx > vx ? cx : vx, y2, fit, edit ? panels_accent : fg);
	}
	ui_draw_banner(s);
	if (s->toast != NULL)
		ui_draw_hint(s->toast, true);
	else if ((split = ui_quick_split_hint(v, s, label, sizeof label)) != NULL)
		panels_bottom(split, PANELS_BAR_KEYS | (s->hints ? 0 : UI_BAR_NO_HINTS), NULL);
	else
		panels_bottom(TR(QUICK_HINT_ALL_SETTINGS, "0 All settings  Hold OK save"),
			PANELS_BAR_KEYS | (s->hints ? 0 : UI_BAR_NO_HINTS), NULL);
}

// The banner on the strip's color. While dual watch runs its screen is read for a gap right of the pane tags, which
// span several lines of text
static void panels_banner(const ui_state_t *s)
{
	int x0 = 0;
#if CONFIG_DUAL_WATCH
	if (s->dual.on)
		x0 = ui_dual_tag_w(ui_layout.large);
#endif
	ui_banner_box(s, ui_layout.hint_y, x0, 0, ui_col.strip_bg);
}

// The reference theme's fonts, none smaller than 8x16 (the screens the shared code draws use tiny for their smallest
// text), its own strip, and the bottom row from the hint row's top: what a screen lays out stays clear of both
static void panels_layout(ui_layout_t *L)
{
	ui_layout_fonts(L);
	if (L->tiny->h < L->small->h)
		L->tiny = L->small;
	ui_layout_regions(L, PANELS_BAND_H, panels_hint_top());
}

// The ops left NULL are the reference theme's
static const ui_theme_ops_t panels_ops = {
	.layout = panels_layout,
	.colors = panels_colors,
	.top = panels_top,
	.bottom = panels_bottom,
	.banner = panels_banner,
	.home = panels_home,
#if CONFIG_DUAL_WATCH
	.home_dual = panels_home_dual,
#endif
	.quick = panels_quick,
	.list_head = panels_list_head,
	.list_geom = panels_list_geom,
	.cell = panels_cell,
	.scroll = panels_scroll,
	.animate = panels_animate,
};

const ui_theme_t ui_theme_panels = {
	.palettes = panels_palettes,
	.palette_count = sizeof panels_palettes / sizeof panels_palettes[0],
	.id = UI_THEME_ID_PANELS,
	// Its talk line leads with a talkgroup's name ("USA · TG 3100"), so a radio that starts in it starts so
	// Its bottom row names the soft keys, so it stays with Hint row off (only the hint line goes)
	.options = UI_THEME_OPT_DAY_NIGHT | UI_THEME_OPT_LAYOUT | UI_THEME_OPT_GPS_CLOCK | UI_THEME_OPT_NAME_FIRST |
		UI_THEME_OPT_SOFT_KEYS,
	.ops = &panels_ops,
	.fits = panels_fits,
};
#endif
