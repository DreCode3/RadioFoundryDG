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

#define PANELS_GEAR_OUTLINE RGB565(0, 0, 0)
#define PANELS_GEAR_FILL RGB565(148, 148, 148)

static color_t panels_bar_fg;	// the bottom row's text
static color_t panels_row_bg, panels_row_focus, panels_accent;

// The profile look's Day and Night palettes, and its colors outside the shared tokens (index: night)
typedef struct {
	color_t row_bg, row_focus, accent, bar_fg;
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
	{ RGB565(205, 223, 237), RGB565(174, 204, 231), RGB565(36, 79, 136), RGB565(255, 255, 255) },
	{ RGB565(30, 46, 63), RGB565(48, 76, 102), RGB565(152, 191, 231), RGB565(237, 242, 247) },
};

// The colors outside the shared tokens go with the palette (UI_PALETTE_*), the only option the op takes
static void panels_colors(ui_colors_t *c, uint8_t palette)
{
	(void)c;
	panels_row_bg = panels_extra_colors[palette].row_bg;
	panels_row_focus = panels_extra_colors[palette].row_focus;
	panels_accent = panels_extra_colors[palette].accent;
	panels_bar_fg = panels_extra_colors[palette].bar_fg;
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

typedef struct {
	int x;
	bool reverse;
	bool measure;
	bool tight;
} panels_row_cursor_t;

static int panels_slot(panels_row_cursor_t *row, int width)
{
	int x = row->reverse ? row->x - width : row->x;
	row->x += row->reverse ? -width : width;
	return x;
}

static void panels_status_key(const radio_view_t *v, const ui_state_t *s, int y, panels_row_cursor_t *row)
{
	const int scale = PANELS_SCALE;
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
	const font_t *algo_font = &font_aa_6x12;
	// gfx_text_title adds one column of weight per character at native height.
	int key_w = algorithm ? (int)strlen(algorithm) * (algo_font->w + 1) : panels_px(15, scale);
	if (!row->reverse)
		panels_slot(row, 2);
	int key_x = panels_slot(row, key_w + (row->reverse ? 3 : 5));
	if (row->measure)
		return;
	int kx = key_x + (key_w - panels_px(13, scale)) / 2;
	color_t key_color = !warning ? ui_col.rx : s->gui_night ? RGB565(160, 168, 178) : RGB565(105, 114, 124);
	panels_mask(kx, algorithm ? y - panels_px(4, scale) : y + 1, panels_key_19, panels_px(13, scale), 10, key_color);
	if (algorithm)
		gfx_text_title(algo_font, key_x + key_w / 2, y + panels_px(5, scale) + algo_font->ascent,
			key_w, algorithm, ui_col.rx, 4);
	if (warning) {
		static const uint32_t cross[] = { 0x1001, 0x0802, 0x040C, 0x0310, 0x00A0, 0x0040, 0x00A0, 0x0310, 0x040C, 0x0802, 0x1001 };
		PANELS_STATUS_MARK(kx, y - panels_px(1, scale), cross, 13, ui_col.tx);
	}
}

// The marks for what the radio is doing, right to left from row.x: the privacy key, unread messages, a receive-only
// channel, scanning (and the simulator's recording and talkaround examples)
static int panels_operating_marks(const radio_view_t *v, const ui_state_t *s, int y, color_t ink,
	panels_row_cursor_t row)
{
#ifdef GFX_FIT_TRACE
	if (s->gui_sim_recording) {
		int rx = panels_slot(&row, 20);
		panels_recording(rx, y + 5, 17, s);
	}
#endif
	if (s->unread) {
		int rx = panels_slot(&row, PANELS_MAIL_W + 3);
		panels_mask(rx, y, panels_mail_23, PANELS_MAIL_W, 15, ink);
	}
	panels_status_key(v, s, y, &row);
#ifdef GFX_FIT_TRACE
	if (s->gui_sim_talkaround) {
		int rx = panels_slot(&row, PANELS_DIRECT_W + 3);
		PANELS_PIXELS(rx, y - 3, panels_direct_art, PANELS_DIRECT_W, ink);
	}
#endif
	// A channel that can't transmit here (receive only, the air band, broadcast FM): its RX mark
	if (v->ch.rx_only || v->am || v->bcast) {
		int rx = panels_slot(&row, 24) + 2;
		PANELS_PIXELS(rx, y - 1, panels_rx_art, 20, ink);
	}
	bool scanning = s->scanning;
#if CONFIG_DUAL_WATCH
	scanning = scanning && !s->dual.on;
#endif
	if (scanning) {
		int rx = panels_slot(&row, PANELS_SCAN_W + 3);
		panels_scan(rx, y + 7, s->now_ms / 100u % 13u, ink, ui_col.dim);
	}
	return row.x;
}

// The strip's right end, right to left: battery, power, lock, GPS (dimmed while it searches), the USB network, then the
// operating marks. Returns where they end
static int panels_status_right(const radio_view_t *v, const ui_state_t *s, int y, color_t ink)
{
	const int scale = PANELS_SCALE;
	int rx = gfx_width() - 5;
	if (!s->no_battery) {
		rx -= panels_px(11, scale);
		panels_battery(v, s, (panels_mark_pos_t){ rx, y, scale }, ink);
		rx -= 3;
	}
#ifdef GFX_FIT_TRACE
	if (s->gui_sim_bt) {
		rx -= PANELS_BT_W + 2;
		panels_bluetooth(rx, y + 4, 21, s);
	}
#endif
	if (board.power_count) {
		rx -= 17;
		char level[] = { "LMH"[panels_power_level(v)], 0 };
		gfx_text(&font_aa_12x24, rx, 2, level, ink);
	}
	if (s->locked) {
		rx -= panels_px(10, scale) + 4;
		panels_mask(rx, y - 1, panels_lock_15, panels_px(10, scale), panels_px(11, scale), ink);
	}
	if (s->gps_fix || s->gps == UI_GPS_SEARCH) {
		rx -= panels_px(11, scale) + 3;
		panels_mask(rx, y - 1, panels_pin_16, panels_px(11, scale), panels_px(11, scale),
			s->gps_fix ? ink : ui_col.dim);
	}
	if (s->usb_net != UI_USB_NET_OFF) {
		const font_t *f = &font_aa_8x16;
		rx -= gfx_text_width(f, "IP") + 4;
		gfx_text(f, rx, (PANELS_BAND_H - f->h) / 2, "IP", s->usb_net == UI_USB_NET_UP ? ink : ui_col.dim);
	}
	panels_row_cursor_t row = { .x = rx, .reverse = true };
	return panels_operating_marks(v, s, y, ink, row) - 2;
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

// The status strip, the same marks at the same size on every screen: signal bars and the mode on the left, then
// roaming, TX or NO TX and the monitor, dual watch and VOX marks; the device and operating marks on the right. Home
// shows the GPS clock in its own lines, so the strip has nothing that only one screen shows
static void panels_draw_status(const radio_view_t *v, radio_state_t st, const ui_state_t *s, int strip_h)
{
	ui_strip_draws++;
	const int scale = PANELS_SCALE;
	const font_t *f = &font_6x12;
#if CONFIG_LANG_ZH
	if (lang_wide(lang_current()))
		f = &font_8x16;
#endif
	int w = gfx_width();
	int iy = (strip_h - panels_px(9, scale)) / 2;
	color_t ink = panels_ink(s);
	gfx_fill(0, 0, w, strip_h, ui_col.strip_bg);
	gfx_hline(0, strip_h - 1, w, ui_col.line);
	int rx = panels_status_right(v, s, iy, ink);
	const char *mode = panels_status_mode(v, s);

	color_t c;
	const char *activity = panels_status_activity(v, st, &c);
	bool monitor = (v->monitor || s->hear_all) && !v->aprs_tx;
	bool dual = false;
#if CONFIG_DUAL_WATCH
	dual = s->dual.on;
#endif

	bool functions = monitor || dual || v->vox_on;
	if (!functions)
		f = &font_aa_8x16;
	int features_w = (monitor ? PANELS_SPEAKER_W + 3 : 0) +
		(dual ? PANELS_DUAL_W + 3 : 0) + (v->vox_on ? PANELS_VOX_W : 0);
	int roaming_w = 0;
#if CONFIG_DMR_ROAMING
	if (v->roam.state != RROAM_STATE_OFF && !v->aprs_tx)
		roaming_w = PANELS_ORBIT_W + 3;
#endif
	int strength = panels_signal_bars(v, s);
	int signal_w = strength ? panels_px(18, scale) : 0;
	int mode_offset = signal_w ? signal_w + 4 : 0;
	int gap = 3;
	int x = 5;
	bool above_roam = roaming_w && f == &font_6x12;
	int activity_w = activity != NULL ? gfx_text_width(f == &font_6x12 ? &font_5x8 : f, activity) : 0;
	// Put the mode beside the bars when both rows of the status strip still fit. Crowded combinations
	// use the two-line badge so the clock and active feature marks have the same space as before.
	int room = rx - x - gap - roaming_w - features_w;
	if (activity_w) {
		int activity_room = rx - x - gap - (above_roam ? 0 : roaming_w) - activity_w;
		if (activity_room < room)
			room = activity_room;
	}
	const font_t *mode_font = &font_aa_12x24;
	if (mode_offset + gfx_text_width(mode_font, mode) > room)
		mode_font = &font_aa_8x16;
	if (mode_offset + gfx_text_width(mode_font, mode) > room)
		mode_font = &font_aa_6x12;
	int mode_w = gfx_text_width(mode_font, mode);
	int inline_w = mode_offset + mode_w;
	bool side = inline_w <= room || !signal_w;
	int badge_w = side ? inline_w : panels_px(24, scale);
	if (!side && mode_w + 4 > badge_w)
		badge_w = mode_w + 4;
	int signal_x = side ? x : x + (badge_w - signal_w) / 2;
	int signal_y = side ? (strip_h - panels_px(9, scale)) / 2 - panels_px(4, scale) : 0;
	panels_mark_pos_t signal = { signal_x, signal_y, scale };
	for (int bar = 0; bar < strength; bar++) {
		int height = 3 + bar * 2;
		panels_round_block(signal, bar * 5, 13 - height, 3, height, ink);
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
		panels_roaming(x, cy, moving ? s->now_ms / 125u : 0, ink);
		x += roaming_w;
	}
#endif
	if (!above_roam)
		activity_x = x;
	if (activity != NULL && f == &font_6x12 && (above_roam || activity_x + gfx_text_width(f, activity) > rx))
		f = &font_5x8;
	if (functions && activity != NULL && !above_roam && f->h + 17 >= strip_h)
		f = &font_5x8;
	bool show_activity = activity != NULL && activity_x + gfx_text_width(f, activity) <= rx;
	if (show_activity)
		gfx_text(f, activity_x, functions || above_roam ? 0 : (strip_h - f->h) / 2, activity, c);
	int fy = show_activity && !above_roam ? f->h + 1 : iy;
	if (monitor && x + PANELS_SPEAKER_W <= rx) {
		panels_mask(x, fy, panels_speaker_19, PANELS_SPEAKER_W, 16, ink);
		x += PANELS_SPEAKER_W + 3;
	}
#if CONFIG_DUAL_WATCH
	if (dual && x + PANELS_DUAL_W <= rx) {
		PANELS_PIXELS(x, fy, panels_dual_art, PANELS_DUAL_W, ink);
		x += PANELS_DUAL_W + 3;
	}
#endif
	if (v->vox_on && x + PANELS_VOX_W <= rx)
		PANELS_PIXELS(x, fy + 2, panels_vox_art, PANELS_VOX_W, ink);
}

// A screen's title below the strip, in the medium font (the small one where it is too long), left-aligned, reserve
// pixels left free at its right for the scroll position. Returns the y below its row, the same whichever font it took
static int panels_title(const radio_view_t *v, radio_state_t st, const ui_state_t *s, const char *prefix,
	const char *name, int reserve, bool draw)
{
	const ui_layout_t *L = &ui_layout;
	int y = L->strip_h + PANELS_TITLE_PAD;
	if (draw) {
		char title[64], fit[48];
		sbuf_t b;
		sb_init(&b, title, sizeof title);
		if (prefix)
			sb_str(&b, prefix);
		if (prefix && name)
			sb_chr(&b, ' ');
		if (name)
			sb_str(&b, name);
		int room = gfx_width() - 30 - reserve;
		const font_t *f = gfx_text_width(L->medium, title) <= room ? L->medium : L->small;
		panels_draw_status(v, st, s, L->strip_h);
		gfx_text_fit(f, title, room, fit, sizeof fit);
		gfx_text(f, 15, y + (L->medium->h - f->h) / 2, fit, ui_col.fg);
	}
	return y + L->medium->h + PANELS_TITLE_PAD;
}

// Its strip, and a title as a list's. Measuring returns before the strip's drawing and its buffers
static int panels_top(const radio_view_t *v, radio_state_t st, const ui_state_t *s, const char *prefix,
	const char *name, bool draw)
{
	bool titled = prefix != NULL || name != NULL;
	if (!titled) {
		if (draw)
			panels_draw_status(v, st, s, ui_layout.strip_h);
		return ui_layout.strip_h;
	}
	if (draw) {
		// The title row in the screen's background: a screen drawn without clearing it first (a page over a page)
		gfx_fill(0, ui_layout.strip_h, gfx_width(), ui_layout.medium->h + 2 * PANELS_TITLE_PAD, ui_col.bg);
	}
	return panels_title(v, st, s, prefix, name, 0, draw);
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

// The talk target as Home names it: its name, else TG or ID and the number; with_number puts the number after a name
static void panels_target_text(sbuf_t *b, uint32_t id, bool private_call, const char *label, bool with_number)
{
	bool named = id != 0xFFFFFFu && label[0] && !ui_name_is_id(label, id, private_call);
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

// The two lines at the bottom of Home's channel area, under a rule: during a DMR call who is calling (the talker alias,
// the contact's name or the ID) and to which talkgroup, while transmitting where PTT talks to and the time left;
// otherwise one line of what needs attention (a refused PTT, a timed transmission, a missing frequency or target, ...)
// and, while idle, the GPS clock. The signal is the strip's bars. The lines keep their place, so nothing above them
// moves when a call comes in
static void panels_home_lines(const radio_view_t *v, const ui_state_t *s, int bottom)
{
	const ui_layout_t *L = &ui_layout;
	const font_t *f = L->medium, *f2 = L->small;
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
			panels_target_text(&b, id, private_call, label, false);
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
			panels_target_text(&b2, call->dst, false, call->dst_name, true);
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
	// The GPS clock (UTC) on the second line while idle (GPS clock, Settings > Display)
	char clock[32];
	if (text2 == NULL && v->state == RS_IDLE && panels_clock(s, clock, sizeof clock)) {
		text2 = clock;
		c2 = ui_col.dim;
	}
	int y2 = bottom - 3 - f2->h, y = y2 - f->h;
	if (text != NULL) {
		gfx_hline(15, y - 4, gfx_width() - 30, ui_col.line);
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

static void panels_home_footer(const ui_state_t *s, const char *hint)
{
	// Above the footer pane; what it reads of the screen under it stays clear of the pane's rounded corners
	ui_banner_box(s, panels_footer_top() - PANELS_HOME_GAP, PANELS_HOME_INSET + 5, PANELS_HOME_INSET + 5,
		ui_col.strip_bg);
	ui_draw_bottom_key(s, hint, ui_home_soft_key(s));
}

// Home under the strip: the zone, then the channel's name as the main line (larger in Commercial), the talk target where
// the layout shows it (Commercial: only one that isn't the channel's own; Amateur: always) and Amateur's details;
// large text keeps the main line and the target, larger. noinline: inlined in panels_home, its buffers would be in the
// frame the reference Home is drawn from
static void __attribute__((noinline)) panels_draw_home(const radio_view_t *v, const ui_state_t *s)
{
	const ui_layout_t *L = &ui_layout;
	gfx_clear(ui_col.bg);
	panels_draw_status(v, v->state, s, L->strip_h);
	int bottom = panels_footer_top() - PANELS_HOME_GAP;
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
		panels_target_text(&b, id, private_call, label, !commercial && !large);
#if CONFIG_DMR
	} else if (!commercial && v->ch.mode == RADIO_MODE_DMR && v->dmr.active) {
		sb_str(&b, TR(HOME_STATE_NO_TARGET, "No talk target"));
#endif
	}
	int y = L->strip_h + 6;
	if (!large) {
		panels_center(L->medium, y, v->vfo ? "VFO" : v->zone_name, ui_col.dim);
		y += L->medium->h + 3;
	}
	int quarters = commercial || large ? 6 : 4;
	panels_home_title(L->large, y, quarters, main_line, c);
	y += L->large->h * quarters / 4 + 3;
	if (target[0]) {
		if (large)
			panels_home_title(L->large, y, 4, target, ui_col.fg);
		else
			panels_center(L->medium, y, target, ui_col.fg);
		y += (large ? L->large->h : L->medium->h) + 2;
	}
	if (!commercial && !large && !s->entry)
		panels_home_details(v, s, y);

	panels_home_lines(v, s, bottom);
	panels_home_footer(s, ui_home_hint(v, s));
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
	panels_row(x, y, w, h, edit ? ui_col.warn : panels_row_focus);
	if (!edit)
		panels_accent_bar(x, y, h);
}

// One row height for every list and grid: the small font and its padding
static int panels_row_h(void)
{
	return ui_layout.small->h + 10;
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
// Dual watch's Home under the strip: its panes inset from the edges, no list of calls (the alias is a live receiving
// indication only), a refused PTT's reason centered. Commercial names the talk target where the channel number would be
static const ui_dual_style_t panels_dual[] = {
	{ .inset = 12, .deny_margin = 15, .target_label = true },	// Commercial
	{ .inset = 12, .deny_margin = 15 },				// Amateur
};

static void panels_home_dual(const radio_view_t *v, const ui_state_t *s)
{
	gfx_clear(ui_col.bg);
	panels_draw_status(v, v->state, s, ui_layout.strip_h);
	int top = ui_layout.strip_h + PANELS_HOME_GAP;
	int bottom = panels_footer_top() - PANELS_HOME_GAP;
	// The GPS clock on a line of its own under the panes, as on Home
	char clock[32];
	if (v->state == RS_IDLE && panels_clock(s, clock, sizeof clock)) {
		const font_t *f = ui_layout.small;
		bottom -= 3 + f->h;
		panels_center(f, bottom, clock, ui_col.dim);
	}
	ui_home_dual_body(v, s, top + 4, bottom - 4, &panels_dual[s->gui_profile != 0]);
	panels_home_footer(s, ui_dual_hint(v, s));
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
	const color_t row_colors[] = { panels_row_bg, panels_row_focus, panels_accent, ui_col.fg };
	const color_t icon_colors[] = { ui_col.priv, ui_col.rx, ui_col.warn, ui_col.tx,
		PANELS_GEAR_FILL, PANELS_GEAR_OUTLINE };
	gfx_reserve_colors(row_colors, sizeof row_colors / sizeof row_colors[0]);
	if (icons)
		gfx_reserve_colors(icon_colors, sizeof icon_colors / sizeof icon_colors[0]);
}

static bool panels_titled(const ui_list_t *l)
{
	return l->prefix || l->name || l->kind == UI_LIST_HUB;
}

// The scroll position, "n/N" in the tiny font: the n-th entry of the list's count. Returns its width
static int panels_badge(const ui_list_t *l, unsigned n, char *out, size_t cap)
{
	sbuf_t b;
	sb_init(&b, out, cap);
	sb_u32(&b, n, 1);
	sb_chr(&b, '/');
	sb_u32(&b, l->count, 1);
	return gfx_text_width(ui_layout.tiny, out);
}

// A list's palette entries, then its heading: the strip and the title (the main menu's is Menu), with room kept at its
// right for the scroll position, or the strip alone for a list without a title
static int panels_list_head(const radio_view_t *v, const ui_state_t *s, const ui_list_t *l, bool draw)
{
	if (draw)
		panels_list_colors(l->kind != UI_LIST_PLAIN);
	if (!panels_titled(l))
		return panels_top(v, draw ? v->state : RS_IDLE, s, NULL, NULL, draw);
	char badge[16];
	int reserve = l->count > 1 ? panels_badge(l, l->count, badge, sizeof badge) + 8 : 0;
	const char *prefix = l->kind == UI_LIST_HUB ? TR(FIND_CHIP_MENU, "Menu") : l->prefix;	// the main menu: Menu
	return panels_title(v, draw ? v->state : RS_IDLE, s, prefix, l->name, reserve, draw);
}

// Rows of one height, inset from the edges, as many as fit; the first level of a menu as a grid of two equal columns
// (one split for the whole list), a page at a time
static void panels_list_geom(const ui_list_t *l, int y, int y_end, ui_list_geom_t *g)
{
	bool grid = l->kind != UI_LIST_PLAIN;
	g->font = ui_layout.small;
	g->x = 7;
	g->y = (int16_t)y;
	g->pitch = (int16_t)panels_row_h();
	int rows = (y_end - y) / g->pitch;
	g->rows = (uint8_t)(rows < 1 ? 1 : rows);
	int inner_w = gfx_width() - 2 * g->x;
	g->cols = grid ? 2 : 1;
	g->col_gap = grid ? 8 : 0;
	g->paged = grid;
	g->col_w[0] = (int16_t)(grid ? (inner_w - g->col_gap) / 2 : inner_w);
	g->col_w[1] = (int16_t)(grid ? inner_w - g->col_gap - g->col_w[0] : 0);
}

// Each row a rounded panel, the focus filled with its accent bar, an icon on the first level of a menu, the key's
// digit, the label and the value. A label too long for its grid column takes the tiny font, never a second line. The
// main menu's tiles show no values
static void panels_cell(const ui_list_t *l, const ui_list_geom_t *g, const ui_cell_t *c, const ui_item_t *it)
{
	bool main_menu = l->kind == UI_LIST_HUB;
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
	if (!(flags & UI_ITEM_INFO) || focus)
		panels_cell_panel(c->x, c->y + PANELS_ROW_GAP, c->w, c->h - 2 * PANELS_ROW_GAP, focus,
			(flags & UI_ITEM_EDIT) != 0);
	if (focus) {
		fg = (flags & UI_ITEM_EDIT) ? ui_col.pill_dark_fg : ui_col.focus_fg;
		vfg = fg;
	}
	int x = c->x + PANELS_TEXT_X;
	if (l->kind != UI_LIST_PLAIN && it->icon != UI_MENU_ICON_NONE) {
		draw_menu_icon(it->icon, x, c->y + (c->h - 13) / 2);
		x += 17;
	}
	int right = c->x + c->w - 5;
	const font_t *f = g->font;
	// A grid column's label: the tiny font, then the small print, before it is shortened
	if (g->cols > 1 && !value[0] && gfx_text_width(f, it->label) + (it->num ? 2 * f->w : 0) > right - x)
		f = ui_layout.tiny;
	if (g->cols > 1 && !value[0] && gfx_text_width(f, it->label) + (it->num ? 2 * f->w : 0) > right - x)
		f = &font_aa_6x12;
	int ty = c->y + (c->h - f->h) / 2;
	if (it->num) {
		char n[3] = { it->num, ' ', 0 };
		x = gfx_text(f, x, ty, n, focus ? fg : ui_col.dim);
	}
	char val[48];
	if (flags & UI_ITEM_EDIT) {
		sbuf_t b;
		sb_init(&b, val, sizeof val);
		sb_chr(&b, '<');
		sb_str(&b, value);
		sb_chr(&b, '>');
	} else {
		str_copy(val, sizeof val, value);
	}
	ui_row_text(f, x, ty, right, it->label, val, fg, vfg, (flags & UI_ITEM_PREVIEW) != 0);
}

// The scroll position in the title's row, "n/N" for the focused entry, where the list has more entries than its screen
// shows. Before the cells: its shades take their palette entries ahead of the cells' on a 16-color display (gfx.c)
static void panels_scroll(const ui_list_t *l, const ui_list_geom_t *g, uint16_t top, uint16_t count, bool over)
{
	(void)top;
	if (over || count <= g->rows * g->cols || !panels_titled(l))
		return;
	char badge[16];
	const font_t *f = ui_layout.tiny;
	int w = panels_badge(l, l->focus < count ? l->focus + 1u : count, badge, sizeof badge);
	gfx_text(f, gfx_width() - 15 - w, ui_layout.strip_h + PANELS_TITLE_PAD + (ui_layout.medium->h - f->h) / 2, badge,
		ui_col.dim);
}

// A Quick tile's name in whole words: the first of its text alternates (the others are a narrow tile's abbreviations),
// and the theme's word where Quick's own name is a short one
static const char *panels_quick_word(uint8_t kind, const char *name, char *out, size_t cap)
{
	switch (kind) {
	case QT_BRIGHTNESS:
		return TR(SET_LABEL_BRIGHTNESS, "Brightness");
	case QT_CC:
		return TR(CHAN_LABEL_COLOR_CODE, "Color code");
	case QT_BANDWIDTH:
		return TR(CHAN_LABEL_BANDWIDTH, "Bandwidth");
	case QT_SPLIT:
		return TR(QUICK_LABEL_TX_FREQUENCY, "TX frequency");
	default: {
		bool more;
		gfx_text_alt(name, 0, out, cap, &more);
		return out;
	}
	}
}

// Quick settings: the tiles 1-9 as rounded panels in three rows of three under the title, the focus as a list's, each
// with its key's digit, its name in whole words and its value. One font for the nine names: the tiny one where each fits
// on a line, else two lines of the small print, with the values in the tiny font so both fit a tile. The bottom row as
// the reference theme's Quick has it
static void panels_quick(const radio_view_t *v, const ui_state_t *s)
{
	const ui_layout_t *L = &ui_layout;
	char label[24], word[24], value[16], fit[24];
	const char *split;
	gfx_clear(ui_col.bg);
	panels_list_colors(false);
	int top = panels_title(v, v->state, s, TR(QUICK_TITLE, "Quick"), v->vfo ? "VFO" : v->ch.name, 0, true);
	int bottom = panels_footer_top() - PANELS_HOME_GAP;
	const int x0 = 7, gap = 6;
	int tw = (gfx_width() - 2 * x0 - 2 * gap) / 3, th = (bottom - top) / 3;
	int room = tw - PANELS_TEXT_X - 4 - 2 * L->tiny->w;	// beside the digit
	const font_t *lf = L->tiny, *vf = L->small;
	for (int i = 0; i < UI_QUICK_TILES && lf == L->tiny; i++) {
		ui_quick_tile(i, v, s, label, sizeof label, value, sizeof value);
		if (gfx_text_width(lf, panels_quick_word(ui_quick_kind(i, v), label + 1, word, sizeof word)) > room)
			lf = &font_aa_6x12;
	}
	if (lf != L->tiny)
		vf = L->tiny;
	room = tw - PANELS_TEXT_X - 4 - 2 * lf->w;
	for (int i = 0; i < UI_QUICK_TILES; i++) {
		int x = x0 + (i % 3) * (tw + gap), y = top + (i / 3) * th;
		bool ok = ui_quick_tile(i, v, s, label, sizeof label, value, sizeof value);
		bool focus = i == s->quick_focus, edit = focus && s->quick_edit;
		color_t fg = focus ? (edit ? ui_col.pill_dark_fg : ui_col.focus_fg) : ok ? ui_col.fg : ui_col.dim;
		panels_cell_panel(x, y + PANELS_ROW_GAP, tw, th - 2 * PANELS_ROW_GAP, focus, edit);
		int tx = x + PANELS_TEXT_X, ty = y + PANELS_ROW_GAP + (lf == L->tiny ? 3 : 1);
		char key[3] = { label[0], ' ', 0 };
		tx = gfx_text(lf, tx, ty, key, focus ? fg : ui_col.dim);
		const char *name = panels_quick_word(ui_quick_kind(i, v), label + 1, word, sizeof word);
		const char *rest = ui_wrap_line(lf, name, room, fit, sizeof fit);
		gfx_text(lf, tx, ty, fit, focus ? fg : ui_col.dim);
		if (*rest) {
			gfx_text_fit(lf, rest, room, fit, sizeof fit);
			gfx_text(lf, tx, ty + lf->h, fit, focus ? fg : ui_col.dim);
		}
		if (edit) {
			// Editing: arrows around the value say Up/Dn changes it
			sbuf_t b;
			sb_init(&b, fit, sizeof fit);
			sb_chr(&b, '<');
			sb_str(&b, value);
			sb_chr(&b, '>');
			str_copy(value, sizeof value, fit);
		}
		gfx_text_fit(vf, value, tw - 8, fit, sizeof fit);
		gfx_text(vf, x + (tw - gfx_text_width(vf, fit)) / 2, y + th - PANELS_ROW_GAP - (lf == L->tiny ? 3 : 1) - vf->h,
			fit, fg);
	}
	ui_draw_banner(s);
	if (s->toast != NULL)
		ui_draw_hint(s->toast, true);
	else if ((split = ui_quick_split_hint(v, s, label, sizeof label)) != NULL)
		ui_draw_hint(split, false);
	else
		ui_draw_hint(TR(QUICK_HINT_ALL_SETTINGS, "0 All settings  Hold OK save"), false);
}

// A group of a hint for the OK key: it starts with OK (first), or it names OK anywhere, as "Hold OK save" does
static bool panels_ok_group(const char *g, size_t n, bool first)
{
	for (size_t i = 0; i + 2 <= n; i++) {
		if ((i == 0 || g[i - 1] == ' ') && g[i] == 'O' && g[i + 1] == 'K' && (i + 2 == n || g[i + 2] == ' '))
			return true;
		if (first)
			break;
	}
	return false;
}

// A group of a hint that starts with a key other than a soft key: # or *, a digit, PTT, Up or Down, a key held, or the
// key names of a radio without a keypad (board.quick_label, context_label: ui_key_names put them in)
static bool panels_key_group(const char *g, size_t n)
{
	static const char *const keys[] = { "PTT", "Up", "Down", "Up/Dn", "Hold" };
	if (n == 0 || g[0] == '#' || g[0] == '*' || (g[0] >= '0' && g[0] <= '9'))
		return true;
	size_t w = 0;
	while (w < n && g[w] != ' ')
		w++;
	for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++)
		if (strlen(keys[i]) == w && strncmp(g, keys[i], w) == 0)
			return true;
	const char *board_keys[] = { board.quick_label, board.context_label };
	for (size_t i = 0; i < 2; i++)
		if (board_keys[i] != NULL && strlen(board_keys[i]) == w && strncmp(g, board_keys[i], w) == 0)
			return true;
	return false;
}

// The two soft keys' labels from a hint's groups (separated by two spaces): on the right Home's assigned soft key or
// the Back group (in a list, Back where the hint has none: Back leaves every list), on the left the OK group, else the
// first group that names no other key (Home's Menu, a sentence such as "Release PTT to stop"). The groups for keys that
// aren't soft keys stay off the bar. left holds the other groups on entry and the one kept on return
static void panels_soft_labels(char *left, char *right, size_t rcap, uint8_t flags, const char *soft_key)
{
	if (soft_key != NULL)
		str_copy(right, rcap, soft_key);
	else if (!right[0] && (flags & UI_BAR_LIST))
		str_copy(right, rcap, TR(HINT_BACK, "Back"));
	const char *keep = NULL;
	size_t keep_n = 0;
	for (int pass = 0; pass < 3 && keep == NULL; pass++) {
		for (const char *g = left; *g;) {
			const char *end = strstr(g, "  ");
			size_t n = end != NULL ? (size_t)(end - g) : strlen(g);
			if (pass == 2 ? !panels_key_group(g, n) : panels_ok_group(g, n, pass == 0)) {
				keep = g;
				keep_n = n;
				break;
			}
			g = end != NULL ? end + 2 : g + n;
		}
	}
	if (keep != NULL)
		memmove(left, keep, keep_n);
	left[keep != NULL ? keep_n : 0] = 0;
}

// The bottom row, a rounded pane above the screen's lower edge: two labels, one over each soft key (left and right),
// in the largest font that fits them (a list's OK group shortened to OK), or a toast in the small font, centered, with a
// second line above it when it is too long for one. soft_key: Home's assigned soft key, its label on the right. The
// buffers are the reference theme's (ui_screens.c): the UI task's deepest chain runs through here
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
	const font_t *f = toast ? L->small : FONT_AA_LARGEST;
	char fit[64];
	color_t bg = toast ? ui_col.fg : ui_col.hint_bg;
	color_t fg = toast ? ui_col.bg : panels_bar_fg;
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
		panels_pane(top, gfx_height() - PANELS_HOME_INSET - top, bg);
		gfx_text(f, inset + (w - gfx_text_width(f, fit)) / 2, y, fit, fg);
		if (second[0]) {
			gfx_text_fit(f, second, w - 2, fit, sizeof fit);
			gfx_text(f, inset + (w - gfx_text_width(f, fit)) / 2, y + f->h, fit, fg);
		}
		return;
	}
	panels_pane(top, h, bg);
	// The longest alternate whose two labels fit side by side in the largest font, then in the next ones
	char form[64], left[64], right[24];
	bool more = true;
	int lw = 0, rw = 0;
	int room = w - 12;
	for (int level = 0; more; level++) {
		gfx_text_alt(text, level, form, sizeof form, &more);
		if (flags & UI_BAR_LIST)
			compact_menu_hint(form, flags & UI_BAR_NUMBERED);
		ui_hint_split(ui_key_names(form, fit, sizeof fit, true), left, sizeof left, right, sizeof right);
		panels_soft_labels(left, right, sizeof right, flags, soft_key);
		lw = gfx_text_width(f, left);
		rw = gfx_text_width(f, right);
		if (lw + (rw && lw ? f->w : 0) + rw <= room)
			break;
		if (!more && f != L->small) {
			f = f == FONT_AA_LARGEST && f != &font_aa_12x24 ? &font_aa_12x24 : L->small;
			text_y = top + (h - f->h) / 2;
			level = -1;
			more = true;
		}
	}
	int margin = lw + (rw && lw ? f->w : 0) + rw <= w - 30 ? 15 - inset : 6;
	if (rw > w - 2 * margin)
		rw = w - 2 * margin;
	if (right[0]) {
		gfx_text_fit(f, right, rw, fit, sizeof fit);
		gfx_text(f, inset + w - margin - gfx_text_width(f, fit), text_y, fit, fg);
	}
	if (left[0]) {
		gfx_text_fit(f, left, w - 2 * margin - (right[0] ? rw + f->w : 0), fit, sizeof fit);
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
	ui_layout_regions(L, PANELS_BAND_H, panels_footer_top());
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
	.options = UI_THEME_OPT_DAY_NIGHT | UI_THEME_OPT_LAYOUT | UI_THEME_OPT_GPS_CLOCK,
	.ops = &panels_ops,
	.fits = panels_fits,
};
#endif
