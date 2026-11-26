// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// The panels theme (CONFIG_UI_THEME_PANELS): rows and Home regions as rounded panels, a status strip of drawn marks,
// Day and Night palettes, the Commercial and Amateur Home layouts. Its ops (ui_theme.h) are at the end; what they
// share with the reference theme's comes from ui_screens.c
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

#define PROFILE_BAND_H 29
#define PROFILE_COMMERCIAL_STATUS_H 36
#define PROFILE_HOME_GAP 3
#define PROFILE_HOME_INSET 4
#define PROFILE_FOOTER_H 34

#define PROFILE_GEAR_OUTLINE RGB565(0, 0, 0)
#define PROFILE_GEAR_FILL RGB565(148, 148, 148)

static color_t profile_bar_fg;	// the bottom row's text
static color_t profile_home_gutter;
static color_t profile_row_bg, profile_row_focus;

// The profile look's Day and Night palettes, and its colors outside the shared tokens (index: night)
typedef struct {
	color_t row_bg, row_focus, home_gutter, bar_fg;
} profile_colors_t;
static const ui_colors_t profile_palettes[] = {	// by UI_PANELS_*
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
static const profile_colors_t profile_colors[] = {
	{ RGB565(205, 223, 237), RGB565(174, 204, 231), RGB565(155, 183, 207), RGB565(255, 255, 255) },
	{ RGB565(30, 46, 63), RGB565(48, 76, 102), RGB565(8, 15, 23), RGB565(237, 242, 247) },
};

// The colors outside the shared tokens go with the palette (UI_PANELS_*). The theme has no appearance options
static void panels_colors(ui_colors_t *c, uint8_t palette)
{
	(void)c;
	profile_row_bg = profile_colors[palette].row_bg;
	profile_row_focus = profile_colors[palette].row_focus;
	profile_home_gutter = profile_colors[palette].home_gutter;
	profile_bar_fg = profile_colors[palette].bar_fg;
}

// A color display 240 px high with room across for the status strip's marks and the caller line
static bool panels_fits(void)
{
	return CONFIG_UI_COLOR && gfx_bpp() > 1 && gfx_width() >= 310 && gfx_height() == 240;
}

typedef struct { int x, y, scale; } profile_mark_pos_t;

static unsigned profile_mask_coverage(const uint8_t *mask, int stride, int x, int y)
{
	return (mask[y * stride + x / 4] >> (2 * (x % 4))) & 3u;
}

static void profile_mask(int x, int y, const uint8_t *mask, int w, int h, color_t ink)
{
	int stride = (w + 3) / 4;
	for (int dy = 0; dy < h; dy++)
		for (int dx = 0; dx < w; dx++) {
			unsigned coverage = profile_mask_coverage(mask, stride, dx, dy);
			if (coverage)
				gfx_coverage_pixel(x + dx, y + dy, ink, coverage);
		}
}

static int profile_px(int n, int scale) { return n * scale / 4; }

static void profile_block(profile_mark_pos_t at, int x, int y, int w, int h, color_t ink)
{
	int dx = profile_px(x, at.scale), dy = profile_px(y, at.scale);
	gfx_fill(at.x + dx, at.y + dy, profile_px(x + w, at.scale) - dx, profile_px(y + h, at.scale) - dy, ink);
}

static void profile_round_fill(int x, int y, int w, int h, color_t ink)
{
	gfx_round_fill(x, y, w, h, ink);
	if (w >= 3 && h >= 3) {
		gfx_coverage_pixel(x, y, ink, 1);
		gfx_coverage_pixel(x + w - 1, y, ink, 1);
		gfx_coverage_pixel(x, y + h - 1, ink, 1);
		gfx_coverage_pixel(x + w - 1, y + h - 1, ink, 1);
	}
}

static void profile_pane(int y, int h, color_t ink)
{
	static const uint8_t corner[] = { 5, 3, 2, 1, 1 };
	const int x = PROFILE_HOME_INSET, w = gfx_width() - 2 * x;
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

static int profile_footer_top(void)
{
	return gfx_height() - PROFILE_HOME_INSET - PROFILE_FOOTER_H;
}

static void profile_round_block(profile_mark_pos_t at, int x, int y, int w, int h, color_t ink)
{
	int dx = profile_px(x, at.scale), dy = profile_px(y, at.scale);
	profile_round_fill(at.x + dx, at.y + dy, profile_px(x + w, at.scale) - dx, profile_px(y + h, at.scale) - dy, ink);
}

typedef struct { uint32_t left, right; } profile_subrow_t;

// Round the four subpixels of a mask corner only when the edge turns in both directions.
// Straight stems, isolated points, and the gaps between parallel strokes keep their coverage.
static profile_subrow_t profile_subrow(const uint32_t *rows, int h, int sample)
{
	int y = sample / 2;
	uint32_t above = y > 0 ? rows[y - 1] : 0;
	uint32_t below = y + 1 < h ? rows[y + 1] : 0;
	uint32_t center = rows[y];
	uint32_t left = center >> 1, right = center << 1;
	uint32_t adjacent = (sample & 1) ? below : above;
	uint32_t corners = (above ^ below) & (left ^ right) & (center ^ adjacent);
	return (profile_subrow_t){ center ^ (corners & ~(left ^ adjacent)),
		center ^ (corners & ~(right ^ adjacent)) };
}

static unsigned profile_coverage(profile_subrow_t upper, profile_subrow_t lower, int a, int b, int w)
{
	uint32_t bit_a = 1u << (w - 1 - a / 2), bit_b = 1u << (w - 1 - b / 2);
	unsigned hits = ((a & 1 ? upper.right : upper.left) & bit_a) != 0;
	hits += ((a & 1 ? lower.right : lower.left) & bit_a) != 0;
	hits += ((b & 1 ? upper.right : upper.left) & bit_b) != 0;
	hits += ((b & 1 ? lower.right : lower.left) & bit_b) != 0;
	return (hits * 3 + 2) / 4;
}

static void profile_bitmap(int x, int y, const uint32_t *rows, int w, int h, int width, int height, color_t ink)
{
	for (int dy = 0; dy < height; dy++) {
		profile_subrow_t upper = profile_subrow(rows, h, (4 * dy + 1) * h / (2 * height));
		profile_subrow_t lower = profile_subrow(rows, h, (4 * dy + 3) * h / (2 * height));
		for (int dx = 0; dx < width; dx++) {
			int a = width == w ? 2 * dx : (4 * dx + 1) * w / (2 * width);
			int b = width == w ? 2 * dx + 1 : (4 * dx + 3) * w / (2 * width);
			unsigned coverage = profile_coverage(upper, lower, a, b, w);
			if (coverage)
				gfx_coverage_pixel(x + dx, y + dy, ink, coverage);
		}
	}
}

#define PROFILE_HEIGHT(mark) (sizeof(mark) / sizeof((mark)[0]))
#define PROFILE_SCALED_PIXELS(x, y, mark, w, width, height, ink) \
	profile_bitmap(x, y, mark, w, PROFILE_HEIGHT(mark), width, height, ink)
#define PROFILE_PIXELS(x, y, mark, w, ink) \
	PROFILE_SCALED_PIXELS(x, y, mark, w, w, PROFILE_HEIGHT(mark), ink)
#define PROFILE_MARK_AT(at, mark, width, ink) \
	PROFILE_SCALED_PIXELS((at).x, (at).y, mark, width, profile_px(width, (at).scale), \
		profile_px(PROFILE_HEIGHT(mark), (at).scale), ink)
#define PROFILE_MARK(x, y, mark, width, ink) PROFILE_PIXELS(x, y, mark, width, ink)
#define PROFILE_STATUS_MARK(x, y, mark, width, ink) \
	PROFILE_MARK_AT(((profile_mark_pos_t){ x, y, scale }), mark, width, ink)

static color_t profile_ink(const ui_state_t *s)
{
	return s->gui_night ? ui_col.fg : RGB565(0, 0, 0);
}


enum {
	PROFILE_VOX_W = 22, PROFILE_DUAL_W = 25, PROFILE_MAIL_W = 23, PROFILE_SPEAKER_W = 19,
	PROFILE_DIRECT_W = 19, PROFILE_SCAN_W = 22, PROFILE_ORBIT_W = 27, PROFILE_BT_W = 24,
};

static const uint32_t profile_vox_art[] = {
	0x330C33, 0x330C33, 0x333333, 0x333333, 0x33330C, 0x33330C, 0x333333, 0x333333,
	0x0C0C33, 0x0C0C33,
};
static const uint32_t profile_rx_art[] = {
	0xFF0C3, 0xFF842, 0xC1C66, 0xC1C24, 0xFF83C, 0xFF018, 0xCE018, 0xC6018,
	0xC703C, 0xC3824, 0xC1C66, 0xC0C42, 0xC0EC3, 0x00400,
};

#if CONFIG_DUAL_WATCH
static const uint32_t profile_dual_art[] = {
	0x0FF83FE, 0x1FFC7FF, 0x180C603, 0x180C603, 0x180C603, 0x188C663, 0x194C653, 0x19CC663,
	0x194C653, 0x194C663, 0x180C603, 0x180C603, 0x180C603, 0x1FFC7FF, 0x0FF83FE,
};
#endif

#ifdef GFX_FIT_TRACE
static const uint32_t profile_direct_art[] = {
	0x3F0C0, 0x3F0C0, 0x0C330, 0x0C330, 0x0C3F0, 0x0C3F0, 0x0C330, 0x0C330,
	0x0C330, 0x0C330, 0x00000, 0x00000, 0x00010, 0x00018, 0x0001E, 0x7FFFF,
	0x7FFFE, 0x00018, 0x00010,
};
static const uint32_t profile_bt_art[] = {
	0x0380, 0x03C0, 0x03F0, 0x03B8, 0x439E, 0xE387, 0x7B9E, 0x1FB8,
	0x0FF0, 0x07E0, 0x03C0, 0x07E0, 0x0FF0, 0x1FB8, 0x7B9E, 0xE387,
	0x439E, 0x03B8, 0x03F0, 0x03C0, 0x0380,
};
static const uint32_t profile_record_ring[] = {
	0x007C0, 0x01FF0, 0x03838, 0x0600C, 0x0C006, 0x0C006, 0x18003, 0x18003,
	0x18003, 0x18003, 0x18003, 0x0C006, 0x0C006, 0x0600C, 0x03838, 0x01FF0,
	0x007C0,
};
static const uint32_t profile_record_center[] = {
	0x038, 0x0FE, 0x0FE, 0x1FF, 0x1FF, 0x1FF, 0x0FE, 0x0FE,
	0x038,
};

static void profile_recording(int x, int cy, int size, const ui_state_t *s)
{
	int y = cy - size / 2;
	PROFILE_SCALED_PIXELS(x, y, profile_record_ring, 17, size, size, ui_col.tx);
	if (!(s->now_ms / 400u % 2u)) {
		int inset = 4 * size / 17;
		int dot_size = 13 * size / 17 - inset;
		PROFILE_SCALED_PIXELS(x + inset, y + inset, profile_record_center, 9, dot_size, dot_size, ui_col.tx);
	}
}

static void profile_bluetooth(int x, int cy, int height, const ui_state_t *s)
{
	color_t ink = s->gui_sim_bt == 2 ? ui_col.priv : ui_col.rx;
	PROFILE_SCALED_PIXELS(x + 4, cy - height / 2, profile_bt_art, 16, 16, height, ink);
	if (s->gui_sim_bt == 2) {
		profile_round_fill(x + 1, cy - 1, 3, 3, ink);
		profile_round_fill(x + 21, cy - 1, 3, 3, ink);
	}
}
#endif


static void profile_scan(int x, int cy, uint32_t frame, color_t ink, color_t step)
{
	// The silhouette stays fixed while the highlight walks the three legs, then fills the arrowhead.
	for (int dy = 0; dy < 17; dy++)
		for (int dx = 0; dx < PROFILE_SCAN_W; dx++) {
			unsigned coverage = profile_mask_coverage(profile_scan_22, (PROFILE_SCAN_W + 3) / 4, dx, dy);
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

static void profile_roaming(int x, int cy, uint32_t frame, color_t ink)
{
	static const uint32_t letter[] = { 0x3C, 0x3C, 0x33, 0x33, 0x3C, 0x3C, 0x33, 0x33, 0x33, 0x33 };
	profile_mask(x, cy - 12, profile_orbit_frames[frame % 8u], PROFILE_ORBIT_W, 25, ink);
	PROFILE_MARK(x + 10, cy - 5, letter, 6, ink);
}
#endif

static bool profile_clock(const ui_state_t *s, char *out, size_t cap)
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

static void profile_battery(const radio_view_t *v, const ui_state_t *s, profile_mark_pos_t at, color_t ink)
{
	bool low = v->battery != RADIO_BATT_OK || (s->battery_pct &&
		s->battery_pct <= (s->low_batt_pct ? s->low_batt_pct : 15));
	color_t c = low ? ui_col.tx : ink;
	profile_round_block(at, 3, -5, 5, 3, c);
	profile_round_block(at, 0, -3, 11, 16, c);
	profile_round_block(at, 1, -2, 9, 14, ui_col.strip_bg);
	unsigned segments = (s->battery_pct + 33u) / 34u;
	for (int i = 0; i < 3; i++)
		profile_block(at, 2, 8 - i * 4, 7, 3, (unsigned)i < segments ? c : ui_col.line);
}

static unsigned profile_power_level(const radio_view_t *v)
{
	unsigned level = v->power * 3u / board.power_count;
	return level < 3 ? level : 2;
}

static void profile_power(const radio_view_t *v, profile_mark_pos_t at, color_t ink)
{
	// Tall, rounded L/M/H share one slot even when the level changes.
	static const uint32_t power[][10] = {
		{ 0x080, 0x180, 0x180, 0x180, 0x180, 0x180, 0x180, 0x180, 0x1FE, 0x0FC },
		{ 0x082, 0x1C7, 0x1C7, 0x1EF, 0x1BB, 0x193, 0x183, 0x183, 0x183, 0x082 },
		{ 0x082, 0x183, 0x183, 0x183, 0x1FF, 0x1FF, 0x183, 0x183, 0x183, 0x082 },
	};
	PROFILE_MARK_AT(at, power[profile_power_level(v)], 9, ink);
}

typedef struct {
	int x;
	bool reverse;
	bool measure;
	bool tight;
} profile_row_cursor_t;

static int profile_slot(profile_row_cursor_t *row, int width)
{
	int x = row->reverse ? row->x - width : row->x;
	row->x += row->reverse ? -width : width;
	return x;
}

static void profile_status_key(const radio_view_t *v, const ui_state_t *s, int y, int scale,
	profile_row_cursor_t *row)
{
	bool encrypted = false;
	bool warning = false;
	const char *algorithm = NULL;
#if CONFIG_DMR
	if (v->ch.mode == RADIO_MODE_DMR) {
		encrypted = v->dmr.privacy;
		bool aes = CONFIG_DMR_AES && v->ch.kind == CODEPLUG_KIND_DMR_AES;
		bool receiving = v->state == RS_RX &&
			(v->dmr.state == RDMR_RX || v->dmr.state == RDMR_RX_OTHER);
		warning = receiving && v->dmr.call.encrypted &&
			(!encrypted || (v->dmr.call.pi_header && !aes) || v->dmr.call.privacy_blocked);
		if (encrypted && aes && !warning)
			algorithm = "AES";
	}
#else
	(void)v;
#endif
#ifdef GFX_FIT_TRACE
	if (s->gui_sim_crypt && !warning) {
		algorithm = s->gui_sim_crypt == 1 ? "ARC4" : "AES";
		encrypted = true;
	}
#endif
	if (!encrypted && !warning)
		return;
	const font_t *algo_font = scale > 4 ? &font_aa_6x12 : &font_5x8;
	// gfx_text_title adds one column of weight per character at native height.
	int key_w = algorithm ? (int)strlen(algorithm) * (algo_font->w + 1) : profile_px(15, scale);
	if (!row->reverse)
		profile_slot(row, 2);
	int key_x = profile_slot(row, key_w + (row->reverse ? (scale > 4 ? 3 : 5) : 5));
	if (row->measure)
		return;
	int kx = key_x + (key_w - profile_px(13, scale)) / 2;
	color_t key_color = !warning ? ui_col.rx : s->gui_night ? RGB565(160, 168, 178) : RGB565(105, 114, 124);
	profile_mask(kx, algorithm ? y - profile_px(4, scale) : y + 1,
		scale == 6 ? profile_key_19 : scale == 5 ? profile_key_16 : profile_key_13,
		profile_px(13, scale), scale == 6 ? 10 : scale == 5 ? 9 : 7, key_color);
	if (algorithm)
		gfx_text_title(algo_font, key_x + key_w / 2, y + profile_px(5, scale) + algo_font->ascent,
			key_w, algorithm, ui_col.rx, 4);
	if (warning) {
		static const uint32_t cross[] = { 0x1001, 0x0802, 0x040C, 0x0310, 0x00A0, 0x0040, 0x00A0, 0x0310, 0x040C, 0x0802, 0x1001 };
		PROFILE_STATUS_MARK(kx, y - profile_px(1, scale), cross, 13, ui_col.tx);
	}
}

static int profile_operating_marks(const radio_view_t *v, const ui_state_t *s, int y, int scale,
	color_t ink, profile_row_cursor_t row)
{
	if (!row.reverse)
		profile_status_key(v, s, y, scale, &row);
#ifdef GFX_FIT_TRACE
	if (row.reverse && s->gui_sim_recording) {
		int rx = profile_slot(&row, 20);
		if (!row.measure)
			profile_recording(rx, y + 5, 17, s);
	}
#endif
	if (s->unread) {
		int rx = profile_slot(&row, PROFILE_MAIL_W + (row.tight ? 2 : 3));
		if (!row.measure)
			profile_mask(rx, y, profile_mail_23, PROFILE_MAIL_W, 15, ink);
	}
	if (row.reverse)
		profile_status_key(v, s, y, scale, &row);
#ifdef GFX_FIT_TRACE
	if (s->gui_sim_talkaround) {
		int rx = profile_slot(&row, PROFILE_DIRECT_W + (row.tight ? 2 : 3));
		if (!row.measure)
			PROFILE_PIXELS(rx, y - 3, profile_direct_art, PROFILE_DIRECT_W, ink);
	}
#endif
	bool scanning = s->scanning;
#if CONFIG_DUAL_WATCH
	scanning = scanning && !s->dual.on;
#endif
	if (scanning) {
		int rx = profile_slot(&row, PROFILE_SCAN_W + (row.tight ? 2 : 3));
		if (!row.measure)
			profile_scan(rx, y + 7, s->now_ms / 100u % 13u, ink, ui_col.dim);
	}
	return row.x;
}

static int profile_status_right(const radio_view_t *v, const ui_state_t *s, int y, int scale, color_t ink)
{
	// Grow the marks within their slots where possible to keep space for the clock and the feature icons.
	int rx = gfx_width() - (scale > 4 ? 5 : 6);
	if (!s->no_battery) {
		rx -= profile_px(11, scale);
		profile_battery(v, s, (profile_mark_pos_t){ rx, y, scale }, ink);
		rx -= 3;
	}
#ifdef GFX_FIT_TRACE
	if (s->gui_sim_bt) {
		rx -= PROFILE_BT_W + 2;
		profile_bluetooth(rx, y + 4, 21, s);
	}
#endif
	if (board.power_count) {
		rx -= scale == 6 ? 17 : scale > 4 ? 13 : 12;
		if (scale == 6) {
			char level[] = { "LMH"[profile_power_level(v)], 0 };
			gfx_text(&font_aa_12x24, rx, 2, level, ink);
		} else {
			profile_power(v, (profile_mark_pos_t){ rx, y, scale }, ink);
		}
	}
	if (s->locked) {
		rx -= profile_px(10, scale) + 4;
		profile_mask(rx, y - 1, scale == 6 ? profile_lock_15 : scale > 4 ? profile_lock_12 : profile_lock_10,
			profile_px(10, scale), profile_px(11, scale), ink);
	}
	if (s->gps_fix) {
		rx -= profile_px(11, scale) + 3;
		profile_mask(rx, y - 1, scale == 6 ? profile_pin_16 : scale > 4 ? profile_pin_13 : profile_pin_11,
			profile_px(11, scale), profile_px(11, scale), ink);
	}
	profile_row_cursor_t row = { .x = rx, .reverse = true };
	return profile_operating_marks(v, s, y, scale, ink, row) - (scale > 4 ? 2 : 3);
}

static const char *profile_status_mode(const radio_view_t *v, const ui_state_t *s)
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

static const char *profile_status_activity(const radio_view_t *v, radio_state_t st, const ui_state_t *s,
	bool titled, char clock[32], color_t *color)
{
	const char *activity = NULL;
	*color = ui_col.dim;
	if (st == RS_TX) {
		activity = v->aprs_tx ? "TX APRS" : "TX";
		*color = ui_col.tx;
	} else if (st == RS_TX_DENIED) {
		activity = TR(TOP_STATE_NO_TX, "NO TX");
		*color = ui_col.warn;
	} else if (st == RS_IDLE && !titled && profile_clock(s, clock, 32)) {
		activity = clock;
	}
	return activity;
}

static uint8_t profile_signal_bars(const radio_view_t *v, const ui_state_t *s)
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

static void draw_profile_status(const radio_view_t *v, radio_state_t st, const ui_state_t *s, bool titled, int strip_h)
{
	ui_strip_draws++;
	const font_t *f = &font_6x12;
#if CONFIG_LANG_ZH
	if (lang_wide(lang_current()))
		f = &font_8x16;
#endif
	int w = gfx_width();
	int scale = titled ? 6 : 4;
	int iy = (strip_h - profile_px(9, scale)) / 2;
	color_t ink = profile_ink(s);
	gfx_fill(0, 0, w, strip_h, ui_col.strip_bg);
	gfx_hline(0, strip_h - 1, w, ui_col.line);
	int rx = profile_status_right(v, s, iy, scale, ink);
	const char *mode = profile_status_mode(v, s);

	char clock[32];
	color_t c;
	const char *activity = profile_status_activity(v, st, s, titled, clock, &c);
	bool monitor = (v->monitor || s->hear_all) && !v->aprs_tx;
	bool dual = false;
#if CONFIG_DUAL_WATCH
	dual = s->dual.on;
#endif

	bool functions = monitor || dual || v->vox_on;
	if (titled && !functions)
		f = &font_aa_8x16;
	int features_w = (monitor ? PROFILE_SPEAKER_W + 3 : 0) +
		(dual ? PROFILE_DUAL_W + 3 : 0) + (v->vox_on ? PROFILE_VOX_W : 0);
	int roaming_w = 0;
#if CONFIG_DMR_ROAMING
	if (v->roam.state != RROAM_STATE_OFF && !v->aprs_tx)
		roaming_w = PROFILE_ORBIT_W + 3;
#endif
	int strength = profile_signal_bars(v, s);
	int signal_w = strength || !titled ? profile_px(18, scale) : 0;
	int mode_offset = signal_w ? signal_w + 4 : 0;
	int gap = scale > 4 ? 3 : 5;
	int x = 5;
	bool above_roam = scale > 4 && roaming_w && f == &font_6x12;
	int activity_w = activity != NULL ?
		gfx_text_width(scale > 4 && f == &font_6x12 ? &font_5x8 : f, activity) : 0;
	// Put the mode beside the bars when both rows of the status strip still fit. Crowded combinations
	// use the two-line badge so the clock and active feature marks have the same space as before.
	int room = rx - x - gap - roaming_w - features_w;
	if (activity_w) {
		int activity_room = rx - x - gap - (above_roam ? 0 : roaming_w) - activity_w;
		if (activity_room < room)
			room = activity_room;
	}
	const font_t *mode_font = titled ? &font_aa_12x24 : &font_aa_6x12;
	if (titled && mode_offset + gfx_text_width(mode_font, mode) > room)
		mode_font = &font_aa_8x16;
	if (mode_offset + gfx_text_width(mode_font, mode) > room)
		mode_font = &font_aa_6x12;
	int mode_w = gfx_text_width(mode_font, mode);
	int inline_w = mode_offset + mode_w;
	bool side = inline_w <= room || !signal_w;
	int badge_w = side ? inline_w : profile_px(24, scale);
	if (!side && mode_w + 4 > badge_w)
		badge_w = mode_w + 4;
	int signal_x = side ? x : x + (badge_w - signal_w) / 2;
	int signal_y = side ? (strip_h - profile_px(9, scale)) / 2 - profile_px(4, scale) : 0;
	profile_mark_pos_t signal = { signal_x, signal_y, scale };
	for (int bar = 0; bar < strength; bar++) {
		int height = 3 + bar * 2;
		profile_round_block(signal, bar * 5, 13 - height, 3, height, ink);
	}
	int mode_x = side ? x + mode_offset : x + (badge_w - mode_w) / 2;
	int mode_y = side ? (strip_h - mode_font->h) / 2 : strip_h - mode_font->h - 2;
	gfx_text(mode_font, mode_x, mode_y, mode, ink);

	x += badge_w + gap;
	int activity_x = x;
#if CONFIG_DMR_ROAMING
	if (roaming_w) {
		bool moving = v->roam.state == RROAM_STATE_SEARCH || v->roam.state == RROAM_STATE_SURVEY;
		int cy = above_roam ? strip_h - 13 : strip_h / 2;
		profile_roaming(x, cy, moving ? s->now_ms / 125u : 0, ink);
		x += roaming_w;
	}
#endif
	if (!above_roam)
		activity_x = x;
	if (scale > 4 && activity != NULL && f == &font_6x12 &&
	    (above_roam || activity_x + gfx_text_width(f, activity) > rx))
		f = &font_5x8;
	if (functions && activity != NULL && !above_roam && f->h + 17 >= strip_h)
		f = &font_5x8;
	bool show_activity = activity != NULL && activity_x + gfx_text_width(f, activity) <= rx;
	if (show_activity)
		gfx_text(f, activity_x, functions || above_roam ? 0 : (strip_h - f->h) / 2, activity, c);
	int fy = show_activity && !above_roam ? f->h + 1 : iy;
	if (monitor && x + PROFILE_SPEAKER_W <= rx) {
		profile_mask(x, fy, profile_speaker_19, PROFILE_SPEAKER_W, 16, ink);
		x += PROFILE_SPEAKER_W + 3;
	}
#if CONFIG_DUAL_WATCH
	if (dual && x + PROFILE_DUAL_W <= rx) {
		PROFILE_PIXELS(x, fy, profile_dual_art, PROFILE_DUAL_W, ink);
		x += PROFILE_DUAL_W + 3;
	}
#endif
	if (v->vox_on && x + PROFILE_VOX_W <= rx)
		PROFILE_PIXELS(x, fy + 2, profile_vox_art, PROFILE_VOX_W, ink);
}

static int draw_profile_top(const radio_view_t *v, radio_state_t st, const ui_state_t *s,
	const char *prefix, const char *name)
{
	const ui_layout_t *L = &ui_layout;
	bool titled = prefix != NULL || name != NULL;
	draw_profile_status(v, st, s, titled, L->strip_h);
	if (!titled)
		return L->strip_h;
	int w = gfx_width();
	int y = L->strip_h;
	gfx_fill(0, y, w, L->small->h + 2, ui_col.bg);
	ui_draw_title(L->small, 0, y + 1, w, prefix, name, ui_col.fg);
	gfx_hline(0, y + L->small->h + 1, w, ui_col.line);
	return y + L->small->h + 2;
}

// Its strip, and the title on a row of its own below it. Measuring returns before the strip's drawing and its buffers
static int panels_top(const radio_view_t *v, radio_state_t st, const ui_state_t *s, const char *prefix,
	const char *name, bool draw)
{
	if (!draw)
		return ui_layout.strip_h + (prefix != NULL || name != NULL ? ui_layout.small->h + 2 : 0);
	return draw_profile_top(v, st, s, prefix, name);
}

static void compact_menu_hint(char *left, bool numbered)
{
	for (char *group = left; *group;) {
		char *end = strstr(group, "  ");
		bool ok = strncmp(group, "OK ", 3) == 0;
		bool shortcut = numbered && group[0] >= '0' && group[0] <= '9' &&
			group[1] == '-' && group[2] >= '0' && group[2] <= '9' && group[3] == ' ';
		if (ok || shortcut) {
			group[0] = 'O';
			group[1] = 'K';
			if (end)
				memmove(group + 2, end, strlen(end) + 1);
			else
				group[2] = 0;
			return;
		}
		if (!end)
			break;
		group = end + 2;
	}
}

static void profile_center(const font_t *f, int y, const char *text, color_t c)
{
	char fit[64];
	int w = gfx_width();
	ui_fit_end(f, text, w - 30, fit, sizeof fit);
	gfx_text(f, (w - gfx_text_width(f, fit)) / 2, y, fit, c);
}

static void profile_frequency(sbuf_t *b, uint32_t hz)
{
	if (hz) {
		sb_freq(b, hz);
		sb_str(b, " MHz");
	}
}

static uint32_t profile_talk_id(const radio_view_t *v, bool *private_call, const char **label)
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

static void profile_target(sbuf_t *b, uint32_t id, bool private_call)
{
	if (id == 0xFFFFFFu) {
		sb_str(b, TR(VALUE_ALL_CALL, "All call"));
	} else {
		sb_str(b, private_call ? "ID " : "TG ");
		sb_u32(b, id, 1);
	}
}

static const char *profile_home_target(const radio_view_t *v, const ui_state_t *s, char *out, size_t cap,
	uint32_t id, bool private_call, const char *label, color_t *c)
{
	sbuf_t b;
	sb_init(&b, out, cap);
	if (s->entry) {
		ui_home_entry_text(s, out, cap);
		*c = ui_col.warn;
		return out;
	}
	if (v->vfo) {
		bool tx = v->state == RS_TX && !v->aprs_tx;
		profile_frequency(&b, tx ? v->ch.tx_hz : v->ch.rx_hz);
		*c = tx ? ui_col.tx : ui_col.fg;
		return out;
	}
	if (id && !v->ch.rx_only) {
		if (id != 0xFFFFFFu && label[0] && !ui_name_is_id(label, id, private_call))
			return label;
		profile_target(&b, id, private_call);
		return out;
	}
	if (v->ch.name[0])
		return v->ch.name;
	profile_frequency(&b, v->ch.rx_hz);
	return out;
}

static void profile_home_details(const radio_view_t *v, const ui_state_t *s, int y, uint32_t id, bool private_call)
{
	const font_t *f = ui_layout.small;
	sbuf_t b;
	char buf[64];
	sb_init(&b, buf, sizeof buf);
	if (!v->vfo)
		sb_str(&b, v->ch.name);
	if (v->temporary)
		sb_str(&b, GLYPH_DOT);
	if (id && !v->ch.rx_only) {
		if (buf[0])
			sb_str(&b, " " GLYPH_DOT " ");
		profile_target(&b, id, private_call);
	}
#if CONFIG_DMR
	else if (v->ch.mode == RADIO_MODE_DMR && v->dmr.active && !v->ch.rx_only) {
		if (buf[0])
			sb_str(&b, " " GLYPH_DOT " ");
		sb_str(&b, TR(HOME_STATE_NO_TARGET, "No talk target"));
	}
#endif
	if (buf[0])
		profile_center(f, y, buf, ui_col.dim);
	sb_init(&b, buf, sizeof buf);
	profile_frequency(&b, v->ch.rx_hz);
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
		profile_center(f, y + f->h + 1, buf, ui_col.dim);
}

static void profile_home_activity(const radio_view_t *v, const ui_state_t *s)
{
	const char *text = NULL;
	const font_t *f = ui_layout.medium;
	color_t c = ui_col.warn;
	char buf[72];
	sbuf_t b;
	if (v->state == RS_TX_DENIED) {
		text = ui_deny_text(v, v->tx_deny, buf, sizeof buf);
	} else if (v->state == RS_TX) {
		if (!v->aprs_tx && !ui_sending_data(v) && v->tot_s) {
			sb_init(&b, buf, sizeof buf);
			uint32_t elapsed = v->tx_elapsed_ms / 1000u;
			sb_mmss(&b, elapsed < v->tot_s ? v->tot_s - elapsed : 0);
			text = buf;
			c = v->tot_warning ? ui_col.warn : ui_col.tx;
		}
#if CONFIG_DMR
	} else if (v->state == RS_RX && v->ch.mode == RADIO_MODE_DMR && v->dmr.active) {
		if (v->dmr.state == RDMR_RX && s->alias_show && v->dmr.alias[0]) {
			text = v->dmr.alias;
			c = ui_col.priv;
			if (!s->gui_profile && gfx_text_width(ui_layout.large, text) <= gfx_width() - 30)
				f = ui_layout.large;
		}
#endif
	} else if (board_hwopt_pending() != NULL) {
		sb_init(&b, buf, sizeof buf);
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
	if (text != NULL) {
		int y = profile_footer_top() - PROFILE_HOME_GAP - f->h - 5;
		gfx_hline(15, y - 4, gfx_width() - 30, ui_col.line);
		profile_center(f, y, text, c);
	}
}

#define PROFILE_HOME_OPERATING_H 28

// Center the visible capitals inside the strip, allowing for the font's blank rows.
static int profile_native_caps(const font_t *font, int x, int cy, const char *label, color_t ink)
{
	int top, rows;
	font_glyph(font, 'M', &top, &rows);
	int y = cy - rows / 2 - top;
	return gfx_text(font, x, y > 0 ? y : 0, label, ink);
}

// Keep each device mark centered in its reserved width.
static int profile_device_slot(int *right, int slot_w, int mark_w)
{
	*right -= slot_w;
	return *right + (slot_w - mark_w) / 2;
}

static void profile_home_devices(const radio_view_t *v, const ui_state_t *s, int strip_h)
{
	int cy = strip_h / 2;
	int right = gfx_width() - 6;
	bool tall = strip_h == PROFILE_COMMERCIAL_STATUS_H;
	bool tx = v->state == RS_TX || v->state == RS_TX_DENIED;
	int slot_w = tx ? 24 : 28;
	int scale = tall ? 9 : 8;
#if CONFIG_DUAL_WATCH || defined(GFX_FIT_TRACE)
	int mark_h = tall ? 24 : 22;
#endif
	color_t ink = profile_ink(s);
	profile_pane(2, strip_h - 2, ui_col.strip_bg);

	if (!s->no_battery) {
		int battery_scale = tall ? 6 : 5;
		int x = profile_device_slot(&right, slot_w, profile_px(11, battery_scale));
		profile_battery(v, s, (profile_mark_pos_t){ x, cy - profile_px(4, battery_scale), battery_scale }, ink);
	}
#ifdef GFX_FIT_TRACE
	if (s->gui_sim_bt) {
		int x = profile_device_slot(&right, slot_w, PROFILE_BT_W);
		profile_bluetooth(x, cy, strip_h - 7, s);
	}
#endif
	if (board.power_count) {
		char level[] = { "LMH"[profile_power_level(v)], 0 };
		const font_t *font = tall ? FONT_AA_LARGEST : &font_aa_12x24;
		int x = profile_device_slot(&right, slot_w, font->w);
		profile_native_caps(font, x, cy, level, ink);
	}
	if (s->locked) {
		int x = profile_device_slot(&right, slot_w, profile_px(10, scale));
		profile_mask(x, cy - profile_px(11, scale) / 2, tall ? profile_lock_22 : profile_lock_20,
			profile_px(10, scale), profile_px(11, scale), ink);
	}
	if (s->gps_fix) {
		int x = profile_device_slot(&right, slot_w, profile_px(11, scale));
		profile_mask(x, cy - profile_px(11, scale) / 2, tall ? profile_pin_24 : profile_pin_22,
			profile_px(11, scale), profile_px(11, scale), ink);
	}
#if CONFIG_DUAL_WATCH
	if (s->dual.on) {
		const int pane_w = 16, gap = 4;
		const font_t *font = &font_aa_12x24;
		int x = profile_device_slot(&right, tx ? 36 : 40, 2 * pane_w + gap);
		int y = cy - mark_h / 2;
		for (int pane = 0; pane < 2; pane++) {
			const char *letter = pane == 0 ? "A" : "B";
			profile_round_fill(x, y, pane_w, mark_h, ink);
			profile_round_fill(x + 2, y + 2, pane_w - 4, mark_h - 4, ui_col.strip_bg);
			profile_native_caps(font, x + (pane_w - font->w) / 2, cy, letter, ink);
			x += pane_w + gap;
		}
	}
#endif
#ifdef GFX_FIT_TRACE
	if (s->gui_sim_recording) {
		int x = profile_device_slot(&right, slot_w, mark_h);
		profile_recording(x, cy, mark_h, s);
	}
#endif
	if (v->vox_on) {
		const font_t *font = &font_aa_12x24;
		int x = profile_device_slot(&right, tx ? 36 : 42, 3 * font->w);
		profile_native_caps(font, x, cy, "VOX", ink);
	}
	int strength = profile_signal_bars(v, s);
	for (int bar = 0; bar < strength; bar++) {
		int height = (bar + 1) * (strip_h - 8) / 4;
		profile_round_fill(15 + bar * 6, strip_h - 4 - height, 5, height, ink);
	}
	if (tx) {
		char clock[32], fit[32];
		color_t activity_ink;
		const char *activity = profile_status_activity(v, v->state, s, false, clock, &activity_ink);
		const font_t *font = &font_aa_8x16;
		const int left = 42;
		int room = right - left - 3;
		if (gfx_text_width(font, activity) > room)
			font = &font_aa_6x12;
		gfx_text_fit(font, activity, room, fit, sizeof fit);
		gfx_text(font, left, (strip_h - font->h) / 2, fit, activity_ink);
	}
}

static int profile_home_marks(const radio_view_t *v, const ui_state_t *s, int top, color_t ink,
	profile_row_cursor_t row)
{
	const int y = top + 7;
#if CONFIG_DMR_ROAMING
	if (v->roam.state != RROAM_STATE_OFF && !v->aprs_tx) {
		int x = profile_slot(&row, PROFILE_ORBIT_W + (row.tight ? 2 : 5));
		if (!row.measure) {
			bool moving = v->roam.state == RROAM_STATE_SEARCH || v->roam.state == RROAM_STATE_SURVEY;
			profile_roaming(x, top + 14, moving ? s->now_ms / 125u : 0, ink);
		}
	}
#endif
	row.x = profile_operating_marks(v, s, y, 6, ink, row);
	if ((v->monitor || s->hear_all) && !v->aprs_tx) {
		int x = profile_slot(&row, PROFILE_SPEAKER_W + (row.tight ? 2 : 3));
		if (!row.measure)
			profile_mask(x, y - 1, profile_speaker_19, PROFILE_SPEAKER_W, 16, ink);
	}
	if (v->ch.rx_only || v->am || v->bcast) {
		int x = profile_slot(&row, 24) + 4;
		if (!row.measure)
			PROFILE_PIXELS(x, top + 7, profile_rx_art, 20, ink);
	}
	return row.x;
}

static void profile_home_operating_marks(const radio_view_t *v, const ui_state_t *s, int top)
{
	char date[32];
	char *time = NULL;
	const font_t *date_font = &font_aa_6x12;
#if CONFIG_LANG_ZH
	if (lang_wide(lang_current()))
		date_font = &font_aa_8x16;
#endif
	const font_t *time_font = &font_aa_6x12;
	int right = gfx_width() - 12;
	int marks_right = right;
	if (v->state == RS_IDLE && profile_clock(s, date, sizeof date)) {
		time = strrchr(date, ' ');
		*time++ = '\0';
		int date_w = gfx_text_width(date_font, date);
		int time_w = gfx_text_width(time_font, time);
		marks_right -= (date_w > time_w ? date_w : time_w) + 5;
	}

	const char *mode = profile_status_mode(v, s);
	color_t ink = profile_ink(s);
	const font_t *font = FONT_AA_LARGEST;
	const int left = 15, gap = 5;
	// Keep a full-size mode label where possible. Pack the gaps before stepping down a native font size.
	profile_row_cursor_t row = { .measure = true };
	int marks_w = profile_home_marks(v, s, top, ink, row);
	int mode_room = marks_right - left - gap - marks_w;
	if (gfx_text_width(font, mode) > mode_room) {
		row.tight = true;
		marks_w = profile_home_marks(v, s, top, ink, row);
		mode_room = marks_right - left - gap - marks_w;
		if (gfx_text_width(font, mode) > mode_room)
			font = &font_aa_12x24;
		if (gfx_text_width(font, mode) > mode_room)
			font = &font_aa_8x16;
	}
	row.measure = false;
	row.x = profile_native_caps(font, left, top + PROFILE_HOME_OPERATING_H / 2, mode, ui_col.fg) + gap;
	profile_home_marks(v, s, top, ink, row);

	if (time) {
		int y = top + (PROFILE_HOME_OPERATING_H - date_font->h - time_font->h) / 2;
		gfx_text(date_font, right - gfx_text_width(date_font, date), y, date, ui_col.dim);
		gfx_text(time_font, right - gfx_text_width(time_font, time), y + date_font->h, time, ui_col.dim);
	}
}

static int draw_home_status(const radio_view_t *v, const ui_state_t *s)
{
	int strip_h = s->gui_profile ? ui_layout.strip_h : PROFILE_COMMERCIAL_STATUS_H;
	int operating_top = strip_h + PROFILE_HOME_GAP;
	ui_strip_draws++;
	profile_home_devices(v, s, strip_h);
	profile_pane(operating_top, PROFILE_HOME_OPERATING_H, profile_row_bg);
	profile_home_operating_marks(v, s, operating_top);
	return operating_top + PROFILE_HOME_OPERATING_H + PROFILE_HOME_GAP;
}

static void profile_home_footer(const ui_state_t *s, const char *hint)
{
	// Above the footer pane; what it reads of the screen under it stays clear of the panes' rounded corners
	ui_banner_box(s, profile_footer_top() - PROFILE_HOME_GAP, PROFILE_HOME_INSET + 5, PROFILE_HOME_INSET + 5,
		ui_col.strip_bg);
	ui_draw_bottom_key(s, hint, ui_home_soft_key(s));
}

// noinline: inlined in panels_home, its buffers would be in the frame the reference Home is drawn from
static void __attribute__((noinline)) draw_profile_home(const radio_view_t *v, const ui_state_t *s)
{
	const ui_layout_t *L = &ui_layout;
	gfx_clear(profile_home_gutter);
	int channel_top = draw_home_status(v, s);
	int channel_bottom = profile_footer_top() - PROFILE_HOME_GAP;
	profile_pane(channel_top, channel_bottom - channel_top, ui_col.bg);

	bool commercial = s->gui_profile == 0;
	bool amateur = !commercial && !s->large_text;
	const char *label;
	bool private_call;
	uint32_t id = profile_talk_id(v, &private_call, &label);
	char buf[64];
	color_t c = ui_col.fg;
	const char *target = profile_home_target(v, s, buf, sizeof buf, id, private_call, label, &c);
	int center = (channel_top + channel_bottom) / 2 - 12;
	int y = center - (amateur ? 48 : 26);
	const char *zone = v->vfo ? "VFO" : v->zone_name;
	profile_center(commercial ? L->large : L->medium, commercial ? center - 47 : y - 4, zone, ui_col.dim);
	y += L->medium->h + 5;
	int baseline = commercial ? center + 26 : y + L->large->ascent;
	int height_quarters = commercial ? 6 : 5;
	int w = gfx_width();
	if (!gfx_text_title(L->large, w / 2, baseline, w - 30, target, c, height_quarters) &&
	    !gfx_text_title(L->medium, w / 2, baseline, w - 30, target, c, height_quarters))
		profile_center(L->medium, baseline - L->medium->ascent, target, c);
	if (amateur && !s->entry)
		profile_home_details(v, s, y + L->large->h, id, private_call);

	profile_home_activity(v, s);
	profile_home_footer(s, ui_home_hint(v, s));
}

static bool profile_operational_home(const radio_view_t *v)
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

// The palette entries the profile look's rows and icons need, taken before anything else is drawn
static void profile_list_colors(const ui_list_t *l)
{
	const color_t row_colors[] = { profile_row_bg, profile_row_focus, ui_col.fg };
	const color_t icon_colors[] = { ui_col.priv, ui_col.rx, ui_col.warn, ui_col.tx,
		PROFILE_GEAR_FILL, PROFILE_GEAR_OUTLINE };
	gfx_reserve_colors(row_colors, sizeof row_colors / sizeof row_colors[0]);
	if (l->kind != UI_LIST_PLAIN)
		gfx_reserve_colors(icon_colors, sizeof icon_colors / sizeof icon_colors[0]);
}

// The profile look's list heading. noinline, as list_header in ui_screens.c: its buffers stay out of the frame of
// panels_list_head, which also leads to the theme's top op
static int __attribute__((noinline)) profile_list_head(const radio_view_t *v, const ui_state_t *s, const ui_list_t *l,
	bool draw)
{
	char title[64];
	sbuf_t b;
	sb_init(&b, title, sizeof title);
	const char *prefix = l->kind == UI_LIST_HUB ? TR(FIND_CHIP_MENU, "Menu") : l->prefix;	// the main menu: Menu
	if (prefix)
		sb_str(&b, prefix);
	if (prefix && l->name)
		sb_chr(&b, ' ');
	if (l->name)
		sb_str(&b, l->name);
	const font_t *f = ui_layout.large;
	if (gfx_text_width(f, title) > gfx_width() - 30)
		f = ui_layout.medium;
	int y = ui_layout.strip_h + 5;
	if (draw) {
		char fit[48];
		draw_profile_status(v, v->state, s, true, ui_layout.strip_h);
		gfx_text_fit(f, title, gfx_width() - 30, fit, sizeof fit);
		gfx_text(f, 15, y, fit, ui_col.fg);
	}
	return y + f->h + 5;
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
		PROFILE_MARK(x, y, outline, 13, PROFILE_GEAR_OUTLINE);
		PROFILE_MARK(x, y, fill, 13, PROFILE_GEAR_FILL);
		return;
	}
	if (icon == UI_MENU_ICON_PRIVACY) {
		profile_mask(x, y + 3, profile_key_13, 13, 7, ui_col.warn);
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
	PROFILE_MARK(x, y, marks[icon].rows, 13, colors[marks[icon].tint]);
}

static void profile_row(int x, int y, int w, int h, color_t ink)
{
	// Three stepped bands round off the corners cleanly at native resolution.
	gfx_fill(x + 3, y, w - 6, h, ink);
	gfx_fill(x + 1, y + 1, w - 2, h - 2, ink);
	gfx_fill(x, y + 3, w, h - 6, ink);
}

// --- The theme ---

// Home. A trunked or single-frequency repeater channel, or one of a kind this build lacks, keeps the reference theme's
// Home body, drawn in this theme's strip, regions and footer (its top and bottom ops)
static void panels_home(const radio_view_t *v, const ui_state_t *s)
{
	if (profile_operational_home(v))
		ui_reference_home(v, s);
	else
		draw_profile_home(v, s);
}

#if CONFIG_DUAL_WATCH
// Dual watch's Home in the theme's regions: its panes inset in the channel pane, no list of calls (the alias is a live
// receiving indication only), a refused PTT's reason centered. Commercial names the talk target where the channel
// number would be
static const ui_dual_style_t panels_dual[] = {
	{ .inset = 12, .deny_margin = 15, .target_label = true },	// Commercial
	{ .inset = 12, .deny_margin = 15 },				// Amateur
};

static void panels_home_dual(const radio_view_t *v, const ui_state_t *s)
{
	gfx_clear(profile_home_gutter);
	int top = draw_home_status(v, s);
	int bottom = profile_footer_top() - PROFILE_HOME_GAP;
	profile_pane(top, bottom - top, ui_col.bg);
	ui_home_dual_body(v, s, top + 4, bottom - 4, &panels_dual[s->gui_profile != 0]);
	profile_home_footer(s, ui_dual_hint(v, s));
}
#endif

// A list's palette entries, then its heading: a larger left-aligned title (the main menu's is Menu), or the strip
// alone for a list without a title
static int panels_list_head(const radio_view_t *v, const ui_state_t *s, const ui_list_t *l, bool draw)
{
	if (draw)
		profile_list_colors(l);
	if (l->prefix || l->name || l->kind == UI_LIST_HUB)
		return profile_list_head(v, s, l, draw);
	return panels_top(v, draw ? v->state : RS_IDLE, s, NULL, NULL, draw);
}

// Taller rows inset from the edges; the first level of a menu as a grid of two columns and three rows, a page at a time
static void panels_list_geom(const ui_list_t *l, int y, int y_end, ui_list_geom_t *g)
{
	bool grid = l->kind != UI_LIST_PLAIN;
	g->font = ui_layout.small;
	g->x = 7;
	g->y = (int16_t)y;
	g->pitch = (int16_t)(ui_layout.row_h + 10);
	int rows = (y_end - y) / g->pitch;
	if (rows < 1)
		rows = 1;
	g->cols = 1;
	g->col_w[1] = 0;
	g->col_gap = 0;
	g->paged = false;
	if (grid) {
		rows = 3;
		g->pitch = (int16_t)((y_end - y) / rows);
		if (l->kind == UI_LIST_HUB)
			g->font = &font_aa_12x24;
		g->cols = 2;
		g->col_gap = 8;
		g->paged = true;
	}
	g->rows = (uint8_t)rows;
	int inner_w = gfx_width() - 2 * g->x;
	g->col_w[0] = (int16_t)inner_w;
	if (grid) {
		int visible = 2 * rows;
		bool paginated = l->count > visible;
		int column_w = (inner_w - g->col_gap) / 2;
		if (l->kind != UI_LIST_HUB) {
			// Give a column extra width for longer translations before shortening a label. Measured over every page,
			// so the columns keep one split as the pages turn
			int minimum[2] = { 0, 0 };
			for (uint16_t i = 0; i < l->count; i++) {
				ui_item_t it = { 0 };
				l->item(i, &it);
				int need = (paginated ? 8 : 10) + gfx_text_width(g->font, it.label);
				if (it.icon != UI_MENU_ICON_NONE)
					need += 16 + (it.num ? 0 : g->font->w + 3);
				if (it.num)
					need += g->font->w + 3;
				int column = i % visible >= rows;
				if (need > minimum[column])
					minimum[column] = need;
			}
			int max_left = inner_w - g->col_gap - minimum[1];
			if (minimum[0] <= max_left) {
				if (column_w < minimum[0])
					column_w = minimum[0];
				if (column_w > max_left)
					column_w = max_left;
			}
		}
		g->col_w[0] = (int16_t)column_w;
		g->col_w[1] = (int16_t)(inner_w - g->col_gap - column_w);
	}
}

// Each row a rounded panel with an accent bar on the focus, an icon on the first level of a menu, and in the grid (the
// geometry's two columns) the digit centered and a long label on two lines. The main menu's tiles show no values
static void panels_cell(const ui_list_t *l, const ui_list_geom_t *g, const ui_cell_t *c, const ui_item_t *it)
{
	const ui_layout_t *L = &ui_layout;
	const font_t *f = g->font;
	int pitch = c->h;
	bool two_columns = g->cols > 1;
	bool paginated = two_columns && l->count > g->rows * g->cols;
	bool main_menu = l->kind == UI_LIST_HUB;
	const int gap = 4;
	color_t disabled = ui_col.dim;
	uint8_t flags = main_menu ? it->flags & ~UI_ITEM_WARN : it->flags;
	const char *value = main_menu ? "" : it->value;
	bool focus = c->focus;
	color_t fg = (flags & UI_ITEM_DIM) ? disabled : (flags & UI_ITEM_WARN) ? ui_col.warn : ui_col.fg;
	if (flags & UI_ITEM_INFO)
		fg = ui_col.dim;
	color_t vfg = (flags & UI_ITEM_DIM) ? disabled : ui_col.dim;
	if (flags & UI_ITEM_TEMP)
		vfg = ui_col.warn;
	if (focus) {
		profile_row(c->x, c->y + gap, c->w, pitch - 2 * gap, flags & UI_ITEM_EDIT ? ui_col.warn : profile_row_focus);
		gfx_fill(c->x + 2, c->y + gap + 5, 2, pitch - 2 * gap - 10, ui_col.fg);
		fg = (flags & UI_ITEM_EDIT) ? ui_col.pill_dark_fg : ui_col.focus_fg;
		vfg = fg;
	} else if (!(flags & UI_ITEM_INFO)) {
		profile_row(c->x, c->y + gap, c->w, pitch - 2 * gap, profile_row_bg);
	}
	int x = c->x + 5;
	if (l->kind != UI_LIST_PLAIN && it->icon != UI_MENU_ICON_NONE) {
		draw_menu_icon(it->icon, x, c->y + (pitch - 13) / 2);
		x += two_columns ? 16 : 19;
		if (!it->num)
			x += two_columns ? f->w + 3 : 2 * f->w;
	}
	int right = c->x + c->w - (paginated ? 3 : 5);
	const font_t *row_font = f;
	int digit_gap = main_menu ? 5 : 3;
	if (main_menu && gfx_text_width(f, it->label) + (it->num ? f->w + digit_gap : 0) > right - x) {
		row_font = L->small;
		if (gfx_text_width(row_font, it->label) + (it->num ? row_font->w + digit_gap : 0) > right - x &&
		    gfx_text_width(L->tiny, it->label) + (it->num ? L->tiny->w + digit_gap : 0) <= right - x)
			row_font = L->tiny;
	}
	int pad = (pitch - row_font->h) / 2;
	int label_room = right - x - (it->num ? row_font->w + digit_gap : 0);
	char val[48];
	const char *rest = NULL;
	if (paginated && !value[0] && pitch >= 2 * row_font->h && gfx_text_width(row_font, it->label) > label_room)
		rest = ui_wrap_line(row_font, it->label, label_room, val, sizeof val);
	int ty = c->y + (rest && *rest ? (pitch - 2 * row_font->h) / 2 : pad);
	if (it->num) {
		char n[3] = { it->num, ' ', 0 };
		color_t num_ink = focus ? fg : ui_col.dim;
		if (two_columns) {
			n[1] = '\0';
			int digit_w = row_font->w + 1;
			gfx_text_title(row_font, x + digit_w / 2, c->y + pad + row_font->ascent, digit_w, n, num_ink, 4);
			x += row_font->w + digit_gap;
		} else {
			x = gfx_text(row_font, x, ty, n, num_ink);
		}
	}
	if (rest && *rest) {
		ui_row_text(row_font, x, ty, right, val, "", fg, vfg, false);
		ui_row_text(row_font, x, ty + row_font->h, right, rest, "", fg, vfg, false);
		return;
	}
	if (flags & UI_ITEM_EDIT) {
		sbuf_t b;
		sb_init(&b, val, sizeof val);
		sb_chr(&b, '<');
		sb_str(&b, value);
		sb_chr(&b, '>');
	} else {
		str_copy(val, sizeof val, value);
	}
	ui_row_text(row_font, x, ty, right, it->label, val, fg, vfg, (flags & UI_ITEM_PREVIEW) != 0);
}

// The grid's page in the title, before the cells: its shades take their palette entries ahead of the cells' on a
// 16-color display (gfx.c). A single column: the reference theme's marks over the rows, after them
static void panels_scroll(const ui_list_geom_t *g, uint16_t top, uint16_t count, bool over)
{
	int w = gfx_width();
	int visible = g->rows * g->cols;
	if (g->cols > 1) {
		if (over || count <= visible)
			return;
		char page[16];
		sbuf_t b;
		sb_init(&b, page, sizeof page);
		sb_u32(&b, top / visible + 1, 1);
		sb_chr(&b, '/');
		sb_u32(&b, (count + visible - 1) / visible, 1);
		const font_t *badge = &font_aa_8x16;
		int bx = w - g->x - gfx_text_width(badge, page), by = g->y - badge->h - 4;
		gfx_text(badge, bx, by, page, ui_col.dim);
		for (int dx = -2; dx <= 2; dx++) {
			int slope = dx < 0 ? -dx : dx;
			if (top > 0)
				gfx_pixel(bx - 7 + dx, by + 4 + slope, ui_col.fg);
			if (top + visible < count)
				gfx_pixel(bx - 7 + dx, by + 12 - slope, ui_col.fg);
		}
		return;
	}
	if (!over)
		return;
	if (top > 0)
		gfx_fill(w - 3, g->y, 3, 2, ui_col.dim);
	if (top + visible < count)
		gfx_fill(w - 3, g->y + g->rows * g->pitch - 2, 3, 2, ui_col.dim);
}

// The bottom row, a rounded pane above the screen's lower edge: key hints in the largest font that fits them (groups
// separated by two spaces, with text alternates; a list's OK group shortened to OK), or a toast in the small font,
// centered, with a second line above it when it is too long for one. soft_key: Home's assigned soft key, its label on
// the right. The buffers are the reference theme's (ui_screens.c): the UI task's deepest chain runs through here
static void panels_bottom(const char *text, uint8_t flags, const char *soft_key)
{
#if CONFIG_VOLUME_BAR
	ui_hint_draws++;
#endif
	bool toast = flags & UI_BAR_TOAST;
	const ui_layout_t *L = &ui_layout;
	const int inset = PROFILE_HOME_INSET;
	int top = profile_footer_top();
	const int h = PROFILE_FOOTER_H;
	const font_t *f = toast ? L->small : FONT_AA_LARGEST;
	char fit[64];
	color_t bg = toast ? ui_col.fg : ui_col.hint_bg;
	color_t fg = toast ? ui_col.bg : profile_bar_fg;
	int text_y = top + (h - f->h) / 2;
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
		int y = text_y;
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
		profile_pane(top, gfx_height() - PROFILE_HOME_INSET - top, bg);
		gfx_text(f, inset + (w - gfx_text_width(f, fit)) / 2, y, fit, fg);
		if (second[0]) {
			gfx_text_fit(f, second, w - 2, fit, sizeof fit);
			gfx_text(f, inset + (w - gfx_text_width(f, fit)) / 2, y + f->h, fit, fg);
		}
		return;
	}
	profile_pane(top, h, bg);
	// The longest alternate whose groups fit side by side in the largest font, then in the next ones
	char form[64], left[64], right[24];
	bool more = true;
	int lw = 0, rw = 0;
	int room = w - 12;
	for (int level = 0; more; level++) {
		gfx_text_alt(text, level, form, sizeof form, &more);
		if (flags & UI_BAR_LIST)
			compact_menu_hint(form, flags & UI_BAR_NUMBERED);
		ui_hint_split(ui_key_names(form, fit, sizeof fit, true), left, sizeof left, right, sizeof right);
		ui_secondary_hint_split(left, right, sizeof right, soft_key != NULL);
		lw = gfx_text_width(f, left);
		rw = gfx_text_width(f, soft_key != NULL ? soft_key : right);
		if (lw + (rw && lw ? f->w : 0) + rw <= room)
			break;
		if (!more && f != L->small) {
			f = f == FONT_AA_LARGEST && f != &font_aa_12x24 ? &font_aa_12x24 : L->small;
			text_y = top + (h - f->h) / 2;
			level = -1;
			more = true;
		}
	}
	// Still too wide: the last groups on the left go, whole, before anything is shortened
	while (lw + (rw && lw ? f->w : 0) + rw > w) {
		char *cut = NULL;
		for (char *p = strstr(left, "  "); p != NULL; p = strstr(p + 1, "  "))
			cut = p;
		if (cut == NULL)
			break;
		*cut = 0;
		lw = gfx_text_width(f, left);
	}
	int margin = lw + (rw && lw ? f->w : 0) + rw <= w - 30 ? 15 - inset : 6;
	if (rw > w - 2 * margin)
		rw = w - 2 * margin;
	const char *right_text = soft_key != NULL ? soft_key : right;
	if (*right_text) {
		gfx_text_fit(f, right_text, rw, fit, sizeof fit);
		gfx_text(f, inset + w - margin - gfx_text_width(f, fit), text_y, fit, fg);
	}
	if (left[0]) {
		gfx_text_fit(f, left, w - 2 * margin - (*right_text ? rw + f->w : 0), fit, sizeof fit);
		gfx_text(f, inset + margin, text_y, fit, fg);
	}
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

// The reference theme's fonts, its own strip and footer
static void panels_layout(ui_layout_t *L)
{
	ui_layout_fonts(L);
	ui_layout_regions(L, PROFILE_BAND_H, profile_footer_top());
}

// The ops left NULL are the reference theme's, which still draw this look where ui_state_t.gui_profiles (and a list's
// roomy flag) says this theme is in use
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
	.list_head = panels_list_head,
	.list_geom = panels_list_geom,
	.cell = panels_cell,
	.scroll = panels_scroll,
};

const ui_theme_t ui_theme_panels = {
	.palettes = profile_palettes,
	.palette_count = sizeof profile_palettes / sizeof profile_palettes[0],
	.id = UI_THEME_ID_PANELS,
	.ops = &panels_ops,
	.fits = panels_fits,
};
#endif
