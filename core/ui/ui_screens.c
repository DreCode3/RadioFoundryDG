// SPDX-License-Identifier: MIT
// Copyright (c) 2026 The RadioFoundry Authors
// Home, quick settings and the boot splash, laid out from the display size (docs/ux/information-architecture.md 3, 5;
// docs/ux/wireframes). Regions top to bottom: status strip, place, talk target, activity, hint row.
#include <string.h>
#include "board.h"
#include "ui_screens.h"
#include "ui_theme.h"
#include "fmt.h"
#include "i18n.h"
#include "ui_svc.h"
#include "ui_svc_aprs.h"
#include "cjk_font.h"	// CJK_FONT_H: the line height a CJK language needs (CONFIG_LANG_ZH)
#include "config.h"
#include "codeplug/codeplug_modes.h"	// channel kinds: the mode word
#if CONFIG_DMR_TRUNK
#include "dmr/trunk/t3_ms.h"	// the engine's states, for the trunked status words
#endif

ui_layout_t ui_layout;
static const font_t *font_largest;	// the large text setting's font (large_font)

// Colors carry meaning only (P5); every color cue also has a word. On 1 bpp panels everything is ink on paper
// and pills are inverse video
ui_colors_t ui_col;
static uint8_t ui_theme;

static uint8_t ui_accent;	// UI_ACCENT_*
static bool ui_strip_fill;

#if CONFIG_UI_PROFILES
#define PROFILE_BAND_H 29
#define PROFILE_COMMERCIAL_STATUS_H 36
#define PROFILE_HOME_GAP 3
#define PROFILE_HOME_INSET 4
#define PROFILE_FOOTER_H 34
static color_t profile_bar_fg;
static color_t profile_row_bg, profile_row_focus, profile_home_gutter;
static void layout_regions(ui_layout_t *L);
static int profile_footer_top(void);

static bool profile_theme(void)
{
	return ui_theme == UI_THEME_PROFILE_DAY || ui_theme == UI_THEME_PROFILE_NIGHT;
}

// The profile look's Day and Night palettes, and its colors outside the shared tokens (index: night)
typedef struct {
	color_t row_bg, row_focus, home_gutter, bar_fg;
} profile_colors_t;
static const ui_colors_t profile_palettes[2] = {
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
static const profile_colors_t profile_colors[2] = {
	{ RGB565(205, 223, 237), RGB565(174, 204, 231), RGB565(155, 183, 207), RGB565(255, 255, 255) },
	{ RGB565(30, 46, 63), RGB565(48, 76, 102), RGB565(8, 15, 23), RGB565(237, 242, 247) },
};

static void profile_palette(bool night)
{
	ui_col = profile_palettes[night];
	profile_row_bg = profile_colors[night].row_bg;
	profile_row_focus = profile_colors[night].row_focus;
	profile_home_gutter = profile_colors[night].home_gutter;
	profile_bar_fg = profile_colors[night].bar_fg;
}
#endif

// The highlight color over the theme's own (Settings > Display), one shade per color for both the filled status strip
// (reverse video: the color as background, text and icons black) and the focus highlight (black text), so the two
// match; also Home's channel number. All are light shades: black on each is at least 7.5:1 after RGB565 rounding.
// The red is lighter and pinker than TX's (the TX pill also says TX); there is no green, which would hide the RX
// pill. Default keeps the theme's blue focus; its filled strip is light gray (white in high contrast). Built without CONFIG_UI_COLOR (radios
// with only a monochrome screen) the theme's own colors stay. Night keeps its brightest red for all of them whatever the
// highlight color, so nothing but red reaches the eye
static void accent_apply(ui_colors_t *c, uint8_t palette)
{
	c->focus_fg = c->fg;
	c->chnum = c->dim;
	if (!CONFIG_UI_COLOR)
		return;
	if (palette == UI_THEME_NIGHT) {
		c->strip_bg = c->focus = c->chnum = c->fg;
		c->strip_warn = c->focus_fg = c->bg;
		return;
	}
	static const color_t acc[UI_ACCENT_COUNT] = {
		[UI_ACCENT_DEFAULT] = RGB565(216, 216, 216),
		[UI_ACCENT_RED] = RGB565(240, 112, 112),
		[UI_ACCENT_ORANGE] = RGB565(255, 160, 64),
		[UI_ACCENT_YELLOW] = RGB565(255, 208, 0),
		[UI_ACCENT_TEAL] = RGB565(72, 210, 210),
		[UI_ACCENT_BLUE] = RGB565(104, 176, 255),
		[UI_ACCENT_PURPLE] = RGB565(184, 150, 255),
		[UI_ACCENT_PINK] = RGB565(255, 136, 200),
	};
	// Warnings on the filled strip (SCAN, MON, ROAM, unread, the lock, a low battery): dark amber where it reads, dark
	// blue on the warm strips where amber can't; at least 5:1 on its strip after RGB565 rounding
	static const color_t warn[UI_ACCENT_COUNT] = {
		[UI_ACCENT_DEFAULT] = RGB565(136, 68, 0),
		[UI_ACCENT_RED] = RGB565(0, 32, 120),
		[UI_ACCENT_ORANGE] = RGB565(0, 48, 168),
		[UI_ACCENT_YELLOW] = RGB565(128, 64, 0),
		[UI_ACCENT_TEAL] = RGB565(112, 56, 0),
		[UI_ACCENT_BLUE] = RGB565(88, 44, 0),
		[UI_ACCENT_PURPLE] = RGB565(80, 40, 0),
		[UI_ACCENT_PINK] = RGB565(0, 40, 152),
	};
	uint8_t a = ui_accent < UI_ACCENT_COUNT ? ui_accent : UI_ACCENT_DEFAULT;
	color_t shade = acc[a];
	if (a == UI_ACCENT_DEFAULT && palette == UI_THEME_HIGH_CONTRAST)
		shade = RGB565(255, 255, 255);	// high contrast has no gray: its default strip is white
	c->strip_bg = shade;
	c->strip_warn = warn[a];
	if (a != UI_ACCENT_DEFAULT) {
		c->focus = shade;
		c->focus_fg = RGB565(0, 0, 0);
		c->chnum = palette == UI_THEME_LIGHT ? c->fg : shade;	// light shades don't read as digits on white
	}
}

// Night's four reds, brightest first (red only: green and blue are 0, black is the background). Text is only ever the
// two brightest; the darkest is a background under the brightest (the idle pill, the hint row), never a text color.
// Pure red on black reaches 5.2:1 at most, so what must be read at a glance is the brightest on black or black on it
#define NIGHT_R4	RGB565(255, 0, 0)
#define NIGHT_R3	RGB565(176, 0, 0)
#define NIGHT_R2	RGB565(112, 0, 0)
#define NIGHT_R1	RGB565(56, 0, 0)

// The palettes (Settings > Display > Theme), by UI_THEME_*: the colors before the highlight color (accent_apply), which
// sets focus_fg, chnum and on color builds the strip and focus colors. Built without CONFIG_UI_COLOR only Dark is there
static const ui_colors_t palettes[] = {
	[UI_THEME_DARK] = { .bg = RGB565(0, 0, 0), .fg = RGB565(255, 255, 255), .dim = RGB565(168, 168, 168),
		.line = RGB565(72, 72, 72), .tx = RGB565(224, 32, 32), .rx = RGB565(40, 184, 72),
		.warn = RGB565(240, 168, 0), .idle = RGB565(96, 96, 96), .pill_fg = RGB565(255, 255, 255),
		.pill_dark_fg = RGB565(0, 0, 0), .focus = RGB565(32, 96, 200), .hint_bg = RGB565(32, 32, 32),
		.priv = RGB565(64, 140, 255), .idle_fg = RGB565(255, 255, 255) },
#if CONFIG_UI_COLOR
	// Light: dark text on white; the meaning colors darker so they read on white
	[UI_THEME_LIGHT] = { .bg = RGB565(255, 255, 255), .fg = RGB565(0, 0, 0), .dim = RGB565(88, 88, 88),
		.line = RGB565(176, 176, 176), .tx = RGB565(200, 0, 0), .rx = RGB565(0, 136, 40),
		.warn = RGB565(176, 96, 0), .idle = RGB565(128, 128, 128), .pill_fg = RGB565(255, 255, 255),
		.pill_dark_fg = RGB565(0, 0, 0), .focus = RGB565(64, 128, 232), .hint_bg = RGB565(224, 224, 224),
		.priv = RGB565(0, 72, 200), .idle_fg = RGB565(255, 255, 255) },
	// High contrast: pure black and white, saturated meaning colors, nothing gray
	[UI_THEME_HIGH_CONTRAST] = { .bg = RGB565(0, 0, 0), .fg = RGB565(255, 255, 255),
		.dim = RGB565(255, 255, 255), .line = RGB565(255, 255, 255), .tx = RGB565(255, 0, 0),
		.rx = RGB565(0, 255, 0), .warn = RGB565(255, 255, 0), .idle = RGB565(255, 255, 255),
		.pill_fg = RGB565(255, 255, 255), .pill_dark_fg = RGB565(0, 0, 0), .focus = RGB565(0, 0, 255),
		.hint_bg = RGB565(0, 0, 0), .priv = RGB565(0, 255, 255), .idle_fg = RGB565(255, 255, 255) },
	// Night: the meanings that were hues become brightness and the word (P5): TX and warnings brightest, RX and
	// privacy one step down, the idle pill dark with a bright word, black words on the brightest red. The highlight
	// and focus are its brightest red too (accent_apply)
	[UI_THEME_NIGHT] = { .bg = RGB565(0, 0, 0), .fg = NIGHT_R4, .dim = NIGHT_R3, .line = NIGHT_R2,
		.tx = NIGHT_R4, .rx = NIGHT_R3, .warn = NIGHT_R4, .idle = NIGHT_R1, .pill_fg = RGB565(0, 0, 0),
		.pill_dark_fg = RGB565(0, 0, 0), .focus = NIGHT_R4, .hint_bg = NIGHT_R1, .priv = NIGHT_R3,
		.idle_fg = NIGHT_R4 },
#endif
};

void ui_theme_set(uint8_t theme)
{
	const ui_theme_t *t = ui_theme_cur();
	if (!CONFIG_UI_COLOR || (theme >= t->palette_count
#if CONFIG_UI_PROFILES
	    && theme != UI_THEME_PROFILE_DAY && theme != UI_THEME_PROFILE_NIGHT
#endif
	   ))
		theme = UI_THEME_DARK;	// without color settings: the dark theme only
#if CONFIG_UI_PROFILES
	bool was_profile = profile_theme();
#endif
	ui_theme = theme;
	if (gfx_bpp() == 1)
		return;		// ink on paper whatever the theme
#if CONFIG_UI_PROFILES
	if (profile_theme()) {
		profile_palette(theme == UI_THEME_PROFILE_NIGHT);
		if (!was_profile && ui_layout.small != NULL)
			layout_regions(&ui_layout);
		return;
	}
	if (was_profile && ui_layout.small != NULL)
		layout_regions(&ui_layout);
#endif
	ui_col = t->palettes[theme];
	if (t->ops->colors != NULL)
		t->ops->colors(&ui_col, theme);
}

uint8_t ui_theme_get(void)
{
	return ui_theme;
}

void ui_accent_set(uint8_t accent)
{
	ui_accent = accent;
	ui_theme_set(ui_theme);
}

void ui_strip_set(bool filled)
{
	ui_strip_fill = filled;
	ui_theme_set(ui_theme);
}

static void layout_regions(ui_layout_t *L)
{
	int w = gfx_width(), h = gfx_height();
	// A pixel between the status strip and what is below it, and a pixel between list rows on displays 100 px and
	// higher (3 on tall ones, 6 in the 240 px class): easy to read comes before one more row
	bool tall = h >= UI_TALL_PX;
	L->strip_h = L->small->h + 1;
	L->hint_y = h - L->small->h;
#if CONFIG_UI_PROFILES
	if (profile_theme()) {
		L->strip_h = PROFILE_BAND_H;
		L->hint_y = profile_footer_top();
	}
#endif
	int y = L->strip_h;
	// Rows that are dropped when the display is short: the zone line, then the separator
	int need = L->small->h + L->large->h + L->medium->h + L->small->h + 2 + L->small->h;
	bool zone = L->strip_h + need + L->small->h <= h;
	L->zone_y = zone ? y : -1;
	if (zone)
		y += L->small->h;
	L->channel_y = y;
	y += L->large->h;
	L->talk_y = y;
	y += L->medium->h;
	L->chips_y = y;
	y += L->small->h;
	if (L->hint_y - y >= L->small->h + 2) {
		L->sep_y = y;
		y += 2;
	} else {
		L->sep_y = -1;
	}
	L->activity_y = y;
	L->activity_h = L->hint_y - y;
	L->row_h = L->small->h + (w >= 220 && tall ? 6 : tall ? 3 : h >= 100 ? 1 : 0);
}

void ui_layout_init(void)
{
	if (gfx_bpp() == 1) {
		ui_col.bg = 0; ui_col.fg = ui_col.dim = ui_col.line = ui_col.tx = ui_col.rx = ui_col.warn = ui_col.idle = ui_col.focus = ui_col.priv = 1;
		ui_col.pill_fg = ui_col.pill_dark_fg = ui_col.idle_fg = 0;
		ui_col.hint_bg = 0;
	}

	ui_theme_set(ui_theme);
	UI_THEME_OP(layout)(&ui_layout);
}

// The reference theme's fonts by display class, and its regions
static void ref_layout(ui_layout_t *L)
{
	int w = gfx_width(), h = gfx_height();

	// Display classes (P10): about 128 px wide, about 160 px wide, 240 px and wider. A narrow portrait panel (120x160)
	// keeps the narrow class's small font, so lists and labels keep their 24 characters a line, and has the rows for
	// the larger channel and talk target fonts of the 160 px class. The 240 px class also needs UI_TALL_PX rows: a
	// wide but short panel (220x176) has large pixels and the rows of the 160 px class, so it takes that class's fonts
	// Anti-aliased fonts on color panels (CONFIG_FONT_AA, font.h); a mono panel keeps the bitmap ones
	bool aa = gfx_bpp() != 1;
#if CONFIG_FONT_AA && defined(FONT_BOTH)
	aa = aa && font_aa_on;
#endif
	const font_t *f6 = aa ? &font_aa_6x12 : &font_6x12, *f8 = aa ? &font_aa_8x16 : &font_8x16;
	const font_t *f12 = aa ? &font_aa_12x24 : &font_12x24;
	font_largest = aa ? FONT_AA_LARGEST : FONT_LARGEST;
	bool wide = w >= 220 && h >= UI_TALL_PX;
	if (wide) {
		L->tiny = f6; L->small = f8; L->medium = f12; L->large = font_largest;
#if CONFIG_FONT_BIG
		// One size up for lists, labels and the status strip (CONFIG_FONT_BIG): 9 px characters instead of 8
		bool big = aa;
#ifdef FONT_BOTH
		big = big && font_big_on;
#endif
		if (big) {
			L->tiny = f8; L->small = &font_aa_9x18;
		}
#endif
	} else if (w >= 150) {
		L->tiny = &font_5x8; L->small = f6; L->medium = f8; L->large = f12;
	} else if (h >= 150) {
		L->tiny = &font_5x8; L->small = &font_5x8; L->medium = f8; L->large = f12;
	} else {
		L->tiny = &font_5x8; L->small = &font_5x8; L->medium = f6; L->large = f8;
	}

#if CONFIG_LANG_ZH
	// A CJK character is 16 px tall (core/ui/cjk_font.h), so a line that may hold Chinese needs a 16 px font, and only
	// while the language in use is a wide one: English and Spanish keep the tighter layout in the same build. Where the
	// layout already draws its words in one (the 240 px class), only the smallest font, which text pages use, moves up to
	// it. Where it draws them in the 6x12 font (the 160 px class), those words move up to the 8x16 one, which costs Home
	// its zone line and a list two rows on a 160x120 panel (docs/spec/ui.md 2.5). A 128x64 panel has no room for a 16 px
	// line at all: its layout stays as it is and the board must not build the language in (BOARD_UNSUPPORTED,
	// docs/build-config.md)
	if (lang_wide(lang_current())) {
		if (L->small->h >= CJK_FONT_H) {
			if (L->tiny->h < CJK_FONT_H)
				L->tiny = L->small;
		} else if (w >= 150) {		// the 160 px class: 8x16 for lists, labels, the strip and the hints
			L->small = f8;
			L->tiny = f8;
		}
	}
#endif

	layout_regions(L);
}

// --- shared pieces ---

const char *ui_power_name(uint8_t p, uint32_t tx_hz)
{
	return board_power_name(p, tx_hz);
}

static uint8_t channel_power(const radio_view_t *v)
{
	return v->ch.power == CODEPLUG_POWER_DEFAULT ? board.power_default : v->ch.power;
}

void ui_bands_text(char *out, size_t len, bool tx)
{
	sbuf_t b;
	sb_init(&b, out, len);
	for (int i = 0; i < board_band_count(); i++) {
		if (tx && !board.bands[i].tx)
			continue;	// a TX refusal lists only where it can transmit, not the receive-only ranges
		if (b.p != out)
			sb_chr(&b, ' ');
		sb_u32(&b, board.bands[i].lo_hz / 1000000u, 1);
		sb_chr(&b, '-');
		sb_u32(&b, board.bands[i].hi_hz / 1000000u, 1);
	}
}

static uint8_t rssi_bars(int16_t dbm)
{
	if (dbm >= -88) return 4;
	if (dbm >= -98) return 3;
	if (dbm >= -108) return 2;
	if (dbm >= -118) return 1;
	return 0;
}

// The mode word: AM where an FM channel or the VFO is received in AM (the air band); a channel of another kind
// (docs/spec/channel-modes.md) its kind's short name
static const char *mode_word(const radio_view_t *v)
{
	if (v->ch.kind != CODEPLUG_KIND_NONE) {
		return codeplug_kind_name(v->ch.kind);
	}
	return v->ch.mode == RADIO_MODE_FM && v->am ? "AM" : ui_mode_name(v->ch.mode);
}

#if CONFIG_DUAL_WATCH
// The same for a dual watch pane, from the channel picked for it (the receiver may be on the other one)
static const char *dual_mode_word(const ui_dual_pane_t *p)
{
	return p->am ? "AM" : ui_mode_name(p->mode);
}
#endif

// Status strip (P2): state pill, mode, monitor, lock, unread messages, signal, battery.
// A titled screen (prefix and / or name) on a display under UI_TALL_PX high merges its title into the strip
// (wireframes 160x128 and 128x64): the pill shows only while not idle, the signal bars and the battery give way to the
// title (Home shows the battery; the low battery banner warns on any screen). The prefix ("Quick") is kept whole; only
// the name is shortened, from the middle. On tall displays the strip stays whole and the title gets its own row below
// it. Either way the title is centered. Returns the y where content starts.
uint32_t ui_strip_draws;
uint32_t ui_hint_draws;

// The GPS icon: a satellite dish, its rim up and to the right. Hollow while the GPS is off, solid while it searches;
// with a fix the feed arm and two waves join it. The shape, not a color, tells them apart on every panel. Drawn by
// hand at the heights of the strip's signal bars, since a dish drawn from lines muddies below 13 pixels. The width
// keeps the waves' room, so the strip doesn't move when a fix comes
#if CONFIG_GPS
// The rows, bit x of a row is pixel x: each dish's bowl (h rows), then its feed arm and waves from the top row down,
// only as many rows as they reach
static const uint32_t gps_rows[] = {
	// 6 px: bowl, feed, waves
	0x0, 0x1, 0x3, 0x7, 0xf, 0x1e,
	0x20, 0x10, 0x8,
	0x280, 0x500, 0x500, 0x280,
	// 8 px: bowl, feed, waves
	0x0, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7e, 0x3c,
	0xc0, 0x20, 0x10,
	0xa00, 0x500, 0xa00, 0xa00, 0x500,
	// 11 px: bowl, feed, waves
	0x0, 0x0, 0x2, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xfe, 0xfc, 0x30,
	0x600, 0x300, 0x80, 0x40, 0x20,
	0x9000, 0x12000, 0x12000, 0x12000, 0x9000,
	// 13 px: bowl, feed, waves
	0x0, 0x0, 0x0, 0x2, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff, 0x1ff, 0x3fe, 0x1fc,
	0x0, 0xc00, 0xe00, 0x300, 0x80, 0x40, 0x20,
	0x0, 0x12000, 0x24000, 0x24000, 0x48000, 0x24000, 0x24000, 0x12000,
};
typedef struct {
	uint8_t h, w;		// height; width, the waves' room kept
	uint8_t feed, waves;	// rows of each in gps_rows after the bowl
	uint8_t at;		// the bowl's first row in gps_rows
} gps_glyph_t;
static const gps_glyph_t gps_glyphs[] = {
	{ 6, 11, 3, 4, 0 },
	{ 8, 12, 3, 5, 13 },
	{ 11, 17, 5, 5, 29 },
	{ 13, 19, 7, 8, 50 },
};

// The tallest dish no taller than h, the bars' height, so it keeps the strip's top margin (the smallest below that)
static const gps_glyph_t *gps_glyph(int h)
{
	const gps_glyph_t *g = &gps_glyphs[0];
	for (size_t i = 1; i < sizeof gps_glyphs / sizeof gps_glyphs[0]; i++)
		if (gps_glyphs[i].h <= h)
			g = &gps_glyphs[i];
	return g;
}

// Hollow: only the bowl's edge, the pixels with a side open
static void gps_icon(int x, int y, const gps_glyph_t *g, uint8_t state, color_t fg)
{
	const uint32_t *bowl = &gps_rows[g->at], *feed = bowl + g->h, *waves = feed + g->feed;
	for (int r = 0; r < g->h; r++) {
		uint32_t row = bowl[r];
		if (state == UI_GPS_OFF) {
			uint32_t up = r > 0 ? bowl[r - 1] : 0, down = r + 1 < g->h ? bowl[r + 1] : 0;
			row &= ~(up & down & (row << 1) & (row >> 1));
		}
		if (state == UI_GPS_FIX)
			row |= (r < g->feed ? feed[r] : 0) | (r < g->waves ? waves[r] : 0);
		for (int c = 0; c < g->w; c++)
			if (row & (1u << c))
				gfx_pixel(x + c, y + r, fg);
	}
}
#endif

// A word after the mode, with a space either side, where the strip has room for it: measured, since a translation may
// be longer than the English
static bool strip_fits(const font_t *f, int lx, int rx, const char *word)
{
	return lx + gfx_text_width(f, word) + 2 * f->w <= rx;
}

#if CONFIG_UI_PROFILES
#include "ui_profile_icons.h"

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
	const char *mode = mode_word(v);
#if CONFIG_DUAL_WATCH
	if (s->dual.on)
		mode = dual_mode_word(&s->dual.pane[s->dual.sel]);
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
		return pane->rx && pane->rssi_dbm ? rssi_bars(pane->rssi_dbm) : 0;
	}
#else
	(void)s;
#endif
	// The receiver samples background RSSI even after the squelch closes.
	return v->state == RS_RX && v->rssi_dbm ? rssi_bars(v->rssi_dbm) : 0;
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
#endif

static int ref_top(const radio_view_t *v, radio_state_t st, const ui_state_t *s, const char *prefix, const char *name)
{
#if CONFIG_UI_PROFILES
	if (s->gui_profiles)
		return draw_profile_top(v, st, s, prefix, name);
#endif
	ui_strip_draws++;
	const ui_layout_t *L = &ui_layout;
	const font_t *f = L->small;
	int w = gfx_width();
	int ph = f->h;		// the pill: one text cell high
	bool titled = prefix != NULL || name != NULL;
	bool merged = titled && gfx_height() < UI_TALL_PX;
	const char *word = TR(TOP_STATE_IDLE, "IDLE");
	color_t pbg = ui_col.idle, pfg = ui_col.idle_fg;
	// FM with the monitor holding the squelch open shows RX, as the green LED does (radio_sm.c outputs()); MON stays
	// beside the mode
	if (st == RS_IDLE && v->monitor && v->ch.mode != RADIO_MODE_DMR)
		st = RS_RX;
#if CONFIG_DUAL_WATCH
	if (st == RS_IDLE && s->dual.on && s->dual.rx != UI_DUAL_NONE)
		st = RS_RX;	// a channel's own receiver (two receivers) hears a call the view's one doesn't show
#endif
	switch (st) {
	case RS_RX: word = "RX"; pbg = ui_col.rx; pfg = ui_col.pill_dark_fg; break;
	case RS_TX: word = v->aprs_tx ? "TX APRS" : "TX"; pbg = ui_col.tx; break;
	case RS_TX_DENIED: word = TR(TOP_STATE_NO_TX, "NO TX"); pbg = ui_col.warn; pfg = ui_col.pill_dark_fg; break;
	default: break;
	}
	if (gfx_bpp() == 1)
		pfg = ui_col.pill_fg;
	else if (pfg == pbg)	// high contrast: the idle pill is white, so its word is black
		pfg = ui_col.pill_dark_fg;
	// A filled strip (Settings > Display) draws on its own background in black, warnings in a dark shade that reads on
	// it (accent_apply); the idle pill turns inverse on it, RX / TX / NO TX keep their colors and words (P5)
	bool filled = CONFIG_UI_COLOR && ui_strip_fill && gfx_bpp() != 1;
	color_t sbg = filled ? ui_col.strip_bg : ui_col.bg, sfg = filled ? ui_col.pill_dark_fg : ui_col.fg;
	color_t swarn = filled ? ui_col.strip_warn : ui_col.warn;
	if (filled && st == RS_IDLE) {
		pbg = sfg;
		pfg = sbg;
	}
	gfx_fill(0, 0, w, L->strip_h, sbg);
	int x = 0;
	if (!merged || st != RS_IDLE) {
		if (filled) {
			// On the filled strip the pill is the strip's full height (a one-cell pill leaves a line of the
			// strip's color under it) against the bezel, its corners rounded by two pixels. Left of its middle the
			// strip is the screen's background, so no strip color shows beside the left end. On the right a black
			// edge follows the curve: RX, TX and NO TX are close in lightness to every strip color (NO TX on orange
			// nearly equal)
			int pad = f->w >= 8 ? 4 : 3, h = L->strip_h;
			int pw = gfx_text_width(f, word) + 2 * pad;
			gfx_fill(0, 0, pw / 2, h, ui_col.bg);
			gfx_fill(2, 0, pw - 4, h, pbg);
			for (int e = 0; e < 2; e++) {	// the rounded ends, and the edge: the two corner steps and the end
				gfx_vline(1 - e, 1 + e, h - 2 - 2 * e, pbg);
				gfx_vline(pw - 2 + e, 1 + e, h - 2 - 2 * e, pbg);
				gfx_pixel(pw - 2 + e, e, sfg);
				gfx_pixel(pw - 2 + e, h - 1 - e, sfg);
			}
			gfx_vline(pw, 2, h - 4, sfg);
			gfx_text(f, pad, (ph - f->h + 1) / 2, word, pfg);
			x = pw + 1 + f->w / 2;
		} else {
			x = gfx_pill(f, 0, 0, ph, word, pfg, pbg) + f->w / 2;
		}
	}
	int ty = (ph - f->h + 1) / 2;

	// Right side, right to left: battery %, battery icon, signal bars, unread messages, lock
	sbuf_t b;
	int rx = w;
	if (!merged) {
		color_t bc = sfg;
		if (!s->no_battery) {
			char pct[6];
			sb_init(&b, pct, sizeof pct);
			sb_u32(&b, s->battery_pct, 1);
			sb_chr(&b, '%');
			rx -= gfx_text_width(f, pct);
			// Settings > Power > Low battery warning
			uint8_t low = s->low_batt_pct ? s->low_batt_pct : 15;
			bc = s->battery_pct <= low ? swarn : sfg;
			gfx_text(f, rx, ty, pct, bc);
		}
		int bh = f->ascent - 2 < 5 ? 5 : f->ascent - 2;
		int bw = bh * 2;
		if (!s->no_battery) {
			rx -= bw + 2;
			gfx_icon_battery(rx, ty + f->ascent - bh, bw, bh, s->battery_pct, sfg, bc);
		}
		int sw = bh + 4;
		rx -= sw + f->w / 2;
		gfx_icon_bars(rx, ty + f->ascent - bh - 1, sw, bh + 1, 4, rssi_bars(v->rssi_dbm), sfg, filled ? ui_col.idle : ui_col.line);
#if CONFIG_GPS
		if (s->gps != UI_GPS_NONE) {
			const gps_glyph_t *g = gps_glyph(bh + 1);
			rx -= g->w + f->w / 2;
			gps_icon(rx, ty + f->ascent - g->h, g, s->gps, sfg);
		}
#endif
	}
	// Icons one cell high, drawn from lines: an envelope with the unread count, a padlock while the keys are locked
	int ih = f->ascent - 2 < 5 ? 5 : f->ascent - 2, iy = ty + f->ascent - ih;
	if (s->unread) {
		char m[4];
		sb_init(&b, m, sizeof m);
		sb_u32(&b, s->unread > 9 ? 9 : s->unread, 1);
		rx -= gfx_text_width(f, m) + f->w / 2;
		gfx_text(f, rx, ty, m, swarn);
		int ew = ih * 3 / 2;
		rx -= ew + 1;
		gfx_rect(rx, iy, ew, ih, swarn);
		for (int i = 1; i <= ew / 2; i++) {	// the flap: two diagonals meeting in the middle
			gfx_pixel(rx + i, iy + i * (ih - 2) / (ew / 2), swarn);
			gfx_pixel(rx + ew - 1 - i, iy + i * (ih - 2) / (ew / 2), swarn);
		}
	}
	if (s->locked) {
		int lw = ih - ih / 4;
		rx -= lw + f->w / 2;
		gfx_fill(rx, iy + ih / 2, lw, ih - ih / 2, swarn);		// body
		gfx_rect(rx + 1, iy, lw - 2, ih / 2 + 1, swarn);		// shackle
	}

	if (merged) {
		color_t dim = ui_col.dim;
		if (filled)
			ui_col.dim = sfg;	// the title's prefix, normally dim, in the strip's one text color
		ui_draw_title(f, x, ty, (rx < w ? rx - f->w / 2 : w) - x, prefix, name, sfg);
		ui_col.dim = dim;
		return L->strip_h;
	}
	// Left side: the mode, and one mode word when there is room for it with a space either side. In dual watch the
	// mode is the selected channel's (the tuned one alternates) and the word is DUAL
	const char *mw = mode_word(v);
#if CONFIG_DUAL_WATCH
	if (s->dual.on && !v->aprs_tx)
		mw = dual_mode_word(&s->dual.pane[s->dual.sel]);
#endif
	if (v->aprs_tx)
		mw = "FM";
	int lx = gfx_text(f, x, ty, mw, sfg);
#if CONFIG_DUAL_WATCH
	if (s->dual.on && !v->aprs_tx && strip_fits(f, lx, rx, TR(TOP_CHIP_DUAL, "DUAL")))
		lx = gfx_text(f, lx + f->w, ty, TR(TOP_CHIP_DUAL, "DUAL"), swarn);
	else
#endif
	if (s->scanning && strip_fits(f, lx, rx, TR(TOP_CHIP_SCAN, "SCAN")))
		lx = gfx_text(f, lx + f->w, ty, TR(TOP_CHIP_SCAN, "SCAN"), swarn);
#if CONFIG_DMR_ROAMING
	else if (v->roam.state == RROAM_STATE_SEARCH && strip_fits(f, lx, rx, TR(TOP_CHIP_ROAM, "ROAM")))
		lx = gfx_text(f, lx + f->w, ty, TR(TOP_CHIP_ROAM, "ROAM"), swarn);	// searching the zone's sites
#endif
	else if (v->monitor && !v->aprs_tx && strip_fits(f, lx, rx, "MON"))
		lx = gfx_text(f, lx + f->w, ty, "MON", swarn);
#if CONFIG_USB_NETWORK && CONFIG_HOSTLINK
	// USB as a network adapter (Tools > USB network): the serial tools are off. IP, and once the PC has the link up an
	// up and a down arrow right after it, as a phone shows its data link. The arrows stand close, each head beside the
	// other's shaft, so the shafts stop short of the other arrow's head: 3 * hw + 3 columns, at least 6 rows (on the
	// smallest font a shorter shaft reads as a cross). Room as for the other words, a space either side
	else if (s->usb_net != UI_USB_NET_OFF) {
		int hw = f->w >= 8 ? f->w / 4 : 1;	// a barb's width
		int aw = s->usb_net == UI_USB_NET_UP ? 3 * hw + 3 : 0;	// the arrows; the glyph cell's own margin before them
		if (lx + gfx_text_width(f, "IP") + aw + 2 * f->w <= rx) {
			lx = gfx_text(f, lx + f->w, ty, "IP", swarn);
			if (aw) {
				int ah = ih < 6 ? 6 : ih, ay = iy + ih - ah;	// the arrows' height and top, on the icons' baseline
				int ax = lx + hw, bx = ax + hw + 2, sh = ah - hw - 1;	// the shafts, up and down; shaft length
				gfx_vline(ax, ay, sh, swarn);
				gfx_vline(bx, ay + ah - sh, sh, swarn);
				for (int i = 1; i <= hw; i++) {
					gfx_pixel(ax - i, ay + i, swarn);
					gfx_pixel(ax + i, ay + i, swarn);
					gfx_pixel(bx - i, ay + ah - 1 - i, swarn);
					gfx_pixel(bx + i, ay + ah - 1 - i, swarn);
				}
				lx += aw;
			}
		}
	}
#endif
	if (!titled)
		return L->strip_h;
	// Tall display: the title on its own row, underlined
	int y = L->strip_h;
	gfx_fill(0, y, w, f->h + 2, ui_col.bg);
	ui_draw_title(f, 0, y + 1, w, prefix, name, ui_col.fg);
	gfx_hline(0, y + f->h + 1, w, ui_col.line);
	return y + f->h + 2;
}

// The pill shows st instead of v->state: the hotspot screen shows what the hotspot does without a copy of the whole
// view on the UI task's stack (the UI task's deepest path, stack check)
static int draw_top_as(const radio_view_t *v, radio_state_t st, const ui_state_t *s, const char *prefix, const char *name)
{
	return UI_THEME_OP(top)(v, st, s, prefix, name);
}

int ui_draw_top(const radio_view_t *v, const ui_state_t *s, const char *prefix, const char *name)
{
	return draw_top_as(v, v->state, s, prefix, name);
}

// The prefix (dim when a name follows) and the name, centered on the screen and moved as little as needed to stay
// within x .. x + max_w (the pill, the lock and the unread count share the merged strip). Both are fitted first, so a
// shortened title fills its space
void ui_draw_title(const font_t *f, int x, int y, int max_w, const char *prefix, const char *name, color_t c)
{
	char pfit[48] = "", nfit[48] = "";
	int pw = 0, gap = 0, nw = 0, room = max_w;
	if (prefix != NULL) {
		gfx_text_fit(f, prefix, room, pfit, sizeof pfit);
		pw = gfx_text_width(f, pfit);
		room -= pw + f->w;
		if (room < 3 * f->w)
			name = NULL;	// no room for the name after the prefix
		else if (name != NULL)
			gap = f->w;
	}
	if (name != NULL) {
		gfx_text_fit(f, name, room, nfit, sizeof nfit);
		nw = gfx_text_width(f, nfit);
	}
	int tw = pw + gap + nw;
	int tx = (gfx_width() - tw) / 2;
	if (tx + tw > x + max_w)
		tx = x + max_w - tw;
	if (tx < x)
		tx = x;
	if (prefix != NULL)
		tx = gfx_text(f, tx, y, pfit, name != NULL ? ui_col.dim : c) + gap;
	if (name != NULL)
		gfx_text(f, tx, y, nfit, c);
}

static const char *wrap_line(const font_t *f, const char *t, int max_w, char *line, size_t cap);
static int wrap_lines(const font_t *f, const char *t, int max_w, int max, char lines[][64], bool preview);

// An overlay's top edge (banner, two-line toast) moved up to a gap between lines of what is under it, so it never
// cuts a line of text in half: the first row at or above y whose row above is all one color (background, or the
// edge of a focus bar), at most one line up
static int overlay_top(int y, int x0, int right_inset)
{
	int w = gfx_width() - right_inset;
	for (int t = y; t > 0 && t > y - ui_layout.large->h - 2; t--) {
		color_t c = gfx_get_pixel(x0, t - 1);
		int x = x0 + 1;
		while (x < w && gfx_get_pixel(x, t - 1) == c)
			x++;
		if (x == w)
			return t;
	}
	return y;
}

// Split a hint into the Back group (it starts with the Back key's name, "Back" or its translation) and the rest,
// joined by two spaces
static void hint_split(const char *text, char *left, size_t lcap, char *right, size_t rcap)
{
	size_t ln = 0, rn = 0;
	const char *back = TR(HINT_BACK, "Back");
	size_t bn = strlen(back);
	right[0] = 0;
	for (const char *g = text; *g;) {
		const char *end = strstr(g, "  ");
		size_t n = end != NULL ? (size_t)(end - g) : strlen(g);
		if (rn == 0 && n >= bn && strncmp(g, back, bn) == 0 && n < rcap) {
			memcpy(right, g, n);
			right[n] = 0;
			rn = n;
		} else if (ln + n + 3 < lcap) {
			if (ln) {
				left[ln++] = ' ';
				left[ln++] = ' ';
			}
			memcpy(left + ln, g, n);
			ln += n;
		}
		g += n;
		while (*g == ' ')
			g++;
	}
	left[ln] = 0;
}

static bool alt_mark(char c)
{
	return c == TEXT_ALT_OPEN[0] || c == TEXT_ALT_NEXT[0] || c == TEXT_ALT_CLOSE[0];
}

#if CONFIG_UI_PROFILES || CONFIG_KEY_ASSIGN
static void secondary_hint_split(char *left, char *right, size_t cap, bool quick_only)
{
	// Use Quick before the last-caller shortcut. With an assigned soft key, only remove Quick
	// so other groups stay on the left.
	if (right[0] && !quick_only)
		return;
	const char *quick = board.quick_label != NULL ? board.quick_label : "*";
	size_t quick_len = strlen(quick);
	char *chosen = NULL;
	for (char *sep = strstr(left, "  "); sep != NULL; sep = strstr(sep + 2, "  ")) {
		const char *group = sep + 2;
		const char *end = strstr(group, "  ");
		size_t n = end != NULL ? (size_t)(end - group) : strlen(group);
		if (n == 0 || (!quick_only && n >= cap))
			continue;
		if (n > quick_len && group[quick_len] == ' ' && strncmp(group, quick, quick_len) == 0) {
			chosen = sep;
			break;
		}
		if (!quick_only)
			chosen = sep;
	}
	if (chosen == NULL)
		return;
	const char *group = chosen + 2;
	const char *end = strstr(group, "  ");
	size_t n = end != NULL ? (size_t)(end - group) : strlen(group);
	if (!quick_only) {
		memcpy(right, group, n);
		right[n] = 0;
	}
	if (end != NULL)
		memmove(chosen, end, strlen(end) + 1);
	else
		*chosen = 0;
}
#endif

static bool board_has_digits(void)
{
	for (uint8_t i = 0; board.keymap != NULL && i < board.keymap_len; i++) {
		if (board.keymap[i].short_action >= ACT_DIGIT_0 && board.keymap[i].short_action <= ACT_DIGIT_9)
			return true;
	}
	return false;
}

#if CONFIG_UI_PROFILES
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
#endif

// Hints and toasts are written for a keypad: "*" and "#" name the QUICK and CONTEXT keys, and a hint group may start
// with a digit ("0-9 find"). On a radio without them (ui.md 1) a "*" or "#" standing alone becomes the board's own
// key name (board.quick_label / context_label: "P1 Quick"), and in a hint (not a toast, whose numbers are values) a
// group that starts with a digit goes, so no hint names a key the radio doesn't have. Text for a keypad radio comes
// back unchanged
static const char *key_names(const char *text, char *out, size_t cap, bool hint)
{
	bool digits = board_has_digits() || !hint;
	if (text == NULL || cap == 0 || (digits && board.quick_label == NULL && board.context_label == NULL))
		return text;
	size_t n = 0;
	for (const char *p = text; *p && n + 1 < cap; p++) {
		bool start = p == text || alt_mark(p[-1]) || (p - text >= 2 && p[-1] == ' ' && p[-2] == ' ');
		if (start && !digits && *p >= '0' && *p <= '9') {
			// Drop the group and one of the two-space separators around it
			while (p[1] && !alt_mark(p[1]) && !(p[1] == ' ' && p[2] == ' '))
				p++;
			if (n >= 2 && out[n - 1] == ' ' && out[n - 2] == ' ')
				n -= 2;
			else if (p[1] == ' ' && p[2] == ' ')
				p += 2;
			continue;
		}
		bool alone = (p == text || p[-1] == ' ' || alt_mark(p[-1])) && (p[1] == 0 || p[1] == ' ' || alt_mark(p[1]));
		const char *sub = !alone ? NULL : *p == '*' ? board.quick_label : *p == '#' ? board.context_label : NULL;
		if (sub == NULL) {
			out[n++] = *p;
			continue;
		}
		while (*sub && n + 1 < cap)
			out[n++] = *sub++;
	}
	out[n] = 0;
	return out;
}

// The hint row. Hints are groups separated by two spaces ("OK open  # more  Back"); the group for the Back key
// ("Back", "Back keeps", ...) is pinned to the right edge, under the key, and the others run from the left. A hint may
// hold text alternates (font.h): the longest form that fits is used. A toast is centered; one too long for the row
// takes a second line above it, so it is never shortened
static void ref_bottom(const char *text, uint8_t flags, const char *soft_key)
{
#if CONFIG_VOLUME_BAR
	ui_hint_draws++;
#endif
	bool toast = flags & UI_BAR_TOAST;
	// The key names go into each alternate once it is picked, in buffers this function has anyway: no static (RAM is
	// tight on some radios) and no more stack (the UI task's deepest chain runs through here, docs/dev/stack-check.md)
	const ui_layout_t *L = &ui_layout;
	const font_t *f = L->small;
	int inset = 0, top = L->hint_y, h = gfx_height() - top;
	char fit[64];
	color_t bg = toast ? ui_col.fg : ui_col.hint_bg;
	color_t fg = toast ? ui_col.bg : ui_col.dim;
	int text_y = L->hint_y;
#if CONFIG_UI_PROFILES
	if (profile_theme()) {
		inset = PROFILE_HOME_INSET;
		top = profile_footer_top();
		h = PROFILE_FOOTER_H;
		f = toast ? L->small : FONT_AA_LARGEST;
		text_y = top + (h - f->h) / 2;
		if (!toast)
			fg = profile_bar_fg;
	}
#endif
	int w = gfx_width() - 2 * inset;
	if (gfx_bpp() == 1 && !toast)
		fg = ui_col.fg;
	if (toast) {
		char first[64], second[64];	// second holds the raw alternate until the key names are in first
		const char *line = first;	// the alternate with the key names: first, or second when there were none
		bool more = true;
		// One line in the longest alternate that fits it, else two lines of the first
		for (int level = 0; more; level++) {
			gfx_text_alt(text, level, second, sizeof second, &more);
			line = key_names(second, first, sizeof first, false);
			if (gfx_text_width(f, line) <= w - 2)
				break;
		}
		if (gfx_text_width(f, line) > w - 2) {
			gfx_text_alt(text, 0, second, sizeof second, &more);
			line = key_names(second, first, sizeof first, false);
		}
		if (line != first) {
			memcpy(first, second, sizeof first);	// the rest below may point into it, and second is reused
		}
		const char *rest = wrap_line(f, first, w - 2, fit, sizeof fit);
		int y = text_y;
		second[0] = 0;
		if (*rest) {
			// Two lines: the first ends at a word, the rest goes below it
			size_t n = strlen(rest);
			n = n < sizeof second - 1 ? n : sizeof second - 1;
			memcpy(second, rest, n);
			second[n] = 0;
			y -= f->h;
			top = overlay_top(y, inset ? inset + 5 : 0, inset ? inset + 5 : 0);
		}
#if CONFIG_UI_PROFILES
		if (inset)
			profile_pane(top, gfx_height() - PROFILE_HOME_INSET - top, bg);
		else
#endif
			gfx_fill(0, top, w, gfx_height() - top, bg);
		gfx_text(f, inset + (w - gfx_text_width(f, fit)) / 2, y, fit, fg);
		if (second[0]) {
			gfx_text_fit(f, second, w - 2, fit, sizeof fit);
			gfx_text(f, inset + (w - gfx_text_width(f, fit)) / 2, y + f->h, fit, fg);
		}
		return;
	}
#if CONFIG_UI_PROFILES
	if (inset)
		profile_pane(top, h, bg);
	else
#endif
		gfx_fill(0, top, w, h, bg);
	if (gfx_bpp() == 1)
		gfx_hline(0, L->hint_y, w, ui_col.line);
	// The longest alternate whose groups fit side by side (or the shortest there is)
	char form[64], left[64], right[24];
	bool more = true;
	int lw = 0, rw = 0;
	int room = w - (inset ? 12 : 0);
	for (int level = 0; more; level++) {
		gfx_text_alt(text, level, form, sizeof form, &more);
#if CONFIG_UI_PROFILES
		if (flags & UI_BAR_LIST)
			compact_menu_hint(form, flags & UI_BAR_NUMBERED);
#endif
		hint_split(key_names(form, fit, sizeof fit, true), left, sizeof left, right, sizeof right);
#if CONFIG_UI_PROFILES || CONFIG_KEY_ASSIGN
		if (inset || soft_key != NULL)
			secondary_hint_split(left, right, sizeof right, soft_key != NULL);
#endif
		lw = gfx_text_width(f, left);
		rw = gfx_text_width(f, soft_key != NULL ? soft_key : right);
		if (lw + (rw && lw ? f->w : 0) + rw <= room)
			break;
#if CONFIG_UI_PROFILES
		if (!more && inset && f != L->small) {
			f = f == FONT_AA_LARGEST && f != &font_aa_12x24 ? &font_aa_12x24 : L->small;
			text_y = top + (h - f->h) / 2;
			level = -1;
			more = true;
		}
#endif
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
	int margin = lw + (rw && lw ? f->w : 0) + rw <= w - 2 ? 1 : 0;
#if CONFIG_UI_PROFILES
	if (profile_theme() && lw + (rw && lw ? f->w : 0) + rw <= w - 30)
		margin = 15 - inset;
	else if (inset)
		margin = 6;
#endif
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

// The bottom row as the theme in use draws it
static void draw_hint(const char *text, uint8_t flags, const char *soft_key)
{
	UI_THEME_OP(bottom)(text, flags, soft_key);
}

void ui_draw_hint(const char *text, bool toast)
{
	draw_hint(text, toast ? UI_BAR_TOAST : 0, NULL);
}

static const char *home_soft_key(const ui_state_t *s)
{
#if CONFIG_KEY_ASSIGN
	if (!s->entry && !s->locked)
		return s->home_soft_key_label;
#else
	(void)s;
#endif
	return NULL;
}

static void draw_bottom(const ui_state_t *s, const char *hint, const char *soft_key)
{
	if (s->toast != NULL)
		ui_draw_hint(s->toast, true);
	else if (s->hints && hint != NULL)
		draw_hint(hint, 0, soft_key);
}

// --- boot splash ---

// The mark: a hexagon with a skyline behind a handheld radio, signal arcs either side of its antenna. It is drawn
// per pixel from shapes laid out on a 1024 grid (hex radius 482, center 512,512), so it scales to every display class
// with no bitmap. Six flat colors fit the 16-entry palette of a 4 bpp panel. On a 1 bpp panel the hexagon is solid ink
// with the radio and arcs cut out of it, and a small mark has one arc a side, so the strokes stay a pixel or more

static const color_t ORANGE = RGB565(242, 96, 31), SKY = RGB565(203, 74, 24), CREAM = RGB565(255, 243, 220),
	BROWN = RGB565(59, 23, 9), AMBER = RGB565(255, 179, 71), PEACH = RGB565(255, 217, 168);

static bool in_rrect(int px, int py, int x, int y, int w, int h, int r)
{
	if (px < x || px >= x + w || py < y || py >= y + h)
		return false;
	int cx = px < x + r ? x + r : px >= x + w - r ? x + w - r - 1 : px;
	int cy = py < y + r ? y + r : py >= y + h - r ? y + h - r - 1 : py;
	return (px - cx) * (px - cx) + (py - cy) * (py - cy) <= r * r;
}

// The color at one point of the 1024 grid, or -1 outside the hexagon
static int mark_color(int X, int Y, bool mono, bool small)
{
	color_t ink = ui_col.fg, paper = ui_col.bg;
	int ax = X > 512 ? X - 512 : 512 - X, ay = Y > 512 ? Y - 512 : 512 - Y;
	if (ay * 1000 + ax * 577 > 482000 || ax > 417)
		return -1;
	// Radio: antenna and side knob, body, display, speaker slits and the key
	color_t body = mono ? paper : CREAM, det = mono ? ink : BROWN, scr = mono ? ink : AMBER;
	if (in_rrect(X, Y, 434, 193, 37, 311, 18) || in_rrect(X, Y, 553, 441, 46, 74, 14) || in_rrect(X, Y, 397, 482, 230, 370, 46)) {
		if (in_rrect(X, Y, 448, 532, 129, 63, 7))
			return scr;
		if (in_rrect(X, Y, 436, 519, 152, 89, 11))
			return det;
		for (int i = 0; i < 3; i++)
			if (in_rrect(X, Y, 436, 652 + 37 * i, 152, 15, 7))
				return det;
		if ((X - 512) * (X - 512) + (Y - 800) * (Y - 800) <= 23 * 23)
			return det;
		return body;
	}
	// Signal arcs, within 42 degrees of horizontal either side of the antenna, centered 452,286
	static const short ring[2][3][2] = { { { 76, 22 }, { 122, 20 }, { 168, 18 } }, { { 120, 60 }, { 0, 0 }, { 0, 0 } } };
	int dx = X - 452, dy = Y - 286, d2 = dx * dx + dy * dy, ady = dy < 0 ? -dy : dy, adx = dx < 0 ? -dx : dx;
	if (ady * 100 <= adx * 90) {
		for (int i = 0; i < 3; i++) {
			int r = ring[small][i][0], hw = ring[small][i][1] / 2;
			if (r && d2 >= (r - hw) * (r - hw) && d2 <= (r + hw) * (r + hw))
				return mono ? paper : PEACH;
		}
	}
	if (mono)
		return ink;
	// Skyline: a sawtooth roof, two chimneys, in a darker orange than the hexagon
	if (X >= 30 && X < 994) {
		int roof = 490 - 120 * ((X - 30) % 241) / 241;
		if (Y >= roof || (X >= 678 && X < 762 && Y >= 230 && Y < 260) || (X >= 690 && X < 750 && Y >= 250) ||
		    (X >= 223 && X < 267 && Y >= 270))
			return SKY;
	}
	return ORANGE;
}

// r = the hexagon's radius in pixels (the mark is 1.73 r wide and 2 r tall), centered on cx, cy
static void draw_mark(int cx, int cy, int r)
{
	bool mono = gfx_bpp() == 1, small = mono && r < 26;
	for (int dy = -r; dy < r; dy++) {
		for (int dx = -r; dx < r; dx++) {
			int c = mark_color(512 + (2 * dx + 1) * 241 / r, 512 + (2 * dy + 1) * 241 / r, mono, small);
			if (c >= 0)
				gfx_pixel(cx + dx, cy + dy, (color_t)c);
		}
	}
}

// The wordmark: "Radio" in the accent color, "Foundry" in the text color, centered; returns its height
static int draw_wordmark(const font_t *f, int y)
{
	int w1 = gfx_text_width(f, "Radio") + 1, w = w1 + gfx_text_width(f, "Foundry");
	int x = (gfx_width() - w) / 2;
	gfx_text(f, x, y, "Radio", gfx_bpp() == 1 ? ui_col.fg : AMBER);
	gfx_text(f, x + w1, y, "Foundry", ui_col.fg);
	return f->h;
}

void ui_draw_splash(const char *board_name, const char *version, const char *line1, const char *line2)
{
	int w = gfx_width(), h = gfx_height();
	const ui_layout_t *L = &ui_layout;
	gfx_clear(ui_col.bg);

	const char *word = "RadioFoundry";
	const font_t *wf = gfx_text_width(L->large, word) <= w - 4 ? L->large :
		gfx_text_width(L->medium, word) <= w - 4 ? L->medium : L->small;
	// The owner's lines (the CPS boot text): as large as fits, never dropped
	const char *lines[2] = { line1 != NULL ? line1 : "", line2 != NULL ? line2 : "" };
	int n_lines = (lines[0][0] != '\0') + (lines[1][0] != '\0');
	const font_t *lf = L->medium;
	for (int i = 0; i < 2; i++)
		if (gfx_text_width(lf, lines[i]) > w - 2)
			lf = L->small;

	// Footer: board and version, small and dim along the bottom. Too wide (a beta's name on a 128 px display): one
	// space between them, then the version alone
	const font_t *ff = L->tiny;
	char foot[48];
	static const char *const seps[] = { "  ", " ", NULL };
	for (int i = 0; i < 3; i++) {
		sbuf_t b;
		sb_init(&b, foot, sizeof foot);
		if (seps[i] != NULL) {
			sb_str(&b, board_name);
			sb_str(&b, seps[i]);
		}
		sb_str(&b, version);
		if (gfx_text_width(ff, foot) <= w)
			break;
	}
	int foot_y = h - ff->h;

	// Stack: mark, wordmark, rule, lines; centered in the space above the footer. The mark takes what is left
	int gap = h >= 200 ? 8 : h >= 100 ? 4 : 2;
	int text_h = wf->h + gap + (n_lines > 0 ? gap + n_lines * lf->h : 0);
	int avail = foot_y - gap - text_h - gap;
	int r = avail / 2;				// the mark is 2 r tall
	if (r > w * 3 / 10)
		r = w * 3 / 10;
	if (r > h * 3 / 10)
		r = h * 3 / 10;
	bool mark = r >= 20;		// smaller than this the radio is a smudge
	// Too short for the mark above the name (128x64): the mark left of the name instead, at the smallest size that
	// stays clear, with the name in the largest font that leaves it room. The owner's lines keep their size
	int side_r = 0, side_w = 0;
	if (!mark) {
		// The largest mark (14 px radius down to 10) whose row, with the name beside it, fits above the footer
		for (int sr = 14; sr >= 10 && side_r == 0; sr--) {
			int sw = sr * 173 / 100 + 4;
			const font_t *sf = gfx_text_width(L->medium, word) <= w - 4 - sw ? L->medium :
				gfx_text_width(L->small, word) <= w - 4 - sw ? L->small : NULL;
			int row_h = 2 * sr > (sf != NULL ? sf->h : 0) ? 2 * sr : (sf != NULL ? sf->h : 0);
			if (sf != NULL && text_h - wf->h + row_h <= foot_y - gap) {
				text_h += row_h - wf->h;
				side_r = sr;
				side_w = sw;
				wf = sf;
			}
		}
	}
	int mark_h = mark ? 2 * r : 0;
	int total = mark_h + (mark ? gap : 0) + text_h;
	int y = (foot_y - total) / 2;
	if (y < 0)
		y = 0;
	if (mark) {
		draw_mark(w / 2, y + r, r);
		y += mark_h + gap;
	}
	if (side_r > 0) {
		int sw = side_w, ww = gfx_text_width(wf, word);
		int x0 = (w - sw - ww) / 2, row_h = 2 * side_r > wf->h ? 2 * side_r : wf->h;
		draw_mark(x0 + (sw - 4) / 2, y + row_h / 2, side_r);
		gfx_text(wf, x0 + sw, y + (row_h - wf->h) / 2, "Radio", gfx_bpp() == 1 ? ui_col.fg : AMBER);
		gfx_text(wf, x0 + sw + gfx_text_width(wf, "Radio") + 1, y + (row_h - wf->h) / 2, "Foundry", ui_col.fg);
		y += row_h;
	} else
		y += draw_wordmark(wf, y);
	gfx_hline(w / 2 - w / 6, y + gap / 2, w / 3, gfx_bpp() == 1 ? ui_col.fg : ORANGE);
	y += gap;
	if (n_lines > 0) {
		y += gap;
		for (int i = 0; i < 2; i++) {
			if (lines[i][0] == '\0')
				continue;
			gfx_text(lf, (w - gfx_text_width(lf, lines[i])) / 2, y, lines[i], ui_col.fg);
			y += lf->h;
		}
	}
	gfx_text(ff, (w - gfx_text_width(ff, foot)) / 2, foot_y, foot, ui_col.dim);
}

// --- Home ---

// The frequency being typed on Home: MHz digits, then decimals, with blanks for the next digits.
static void home_entry_text(const ui_state_t *s, char *buf, size_t len)
{
	sbuf_t b;
	sb_init(&b, buf, len);
	int dec = s->entry_len > UI_ENTRY_MHZ ? s->entry_len - UI_ENTRY_MHZ : 0;
	int show = dec < 3 ? 3 : dec;
	for (int i = 0; i < UI_ENTRY_MHZ + show; i++) {
		if (i == UI_ENTRY_MHZ)
			sb_chr(&b, '.');
		sb_chr(&b, i < s->entry_len ? s->entry_digits[i] : '_');
	}
}

#if CONFIG_DMR
static void draw_talk_dmr(const radio_view_t *v, bool label_first);
static void draw_chips_dmr(const radio_view_t *v, const ui_state_t *s);
#endif

static void draw_place(const radio_view_t *v, const ui_state_t *s)
{
	const ui_layout_t *L = &ui_layout;
	int w = gfx_width();
	char buf[40], fit[40];
	sbuf_t b;

	if (L->zone_y >= 0) {
		// Zone (or VFO and its step) left, position in the zone right
		char right[16];
		sb_init(&b, right, sizeof right);
		if (v->vfo) {
			sb_str(&b, TR(HOME_LABEL_STEP, "Step "));
			uint32_t st = v->vfo_step_hz;
			sb_u32(&b, st / 1000u, 1);
			if (st % 1000u) {
				sb_chr(&b, '.');
				sb_u32(&b, (st % 1000u) / 10u, 2);
				// trim a trailing zero: 12.50 -> 12.5
				if (b.p[-1] == '0') { b.p[-1] = 0; b.p--; }
			}
			sb_chr(&b, 'k');
		} else if (v->zone_size) {
			sb_u32(&b, v->zone_pos + 1u, 1);
			sb_chr(&b, '/');
			sb_u32(&b, v->zone_size, 1);
		}
		int rw = gfx_text_width(L->small, right);
		const char *left = v->vfo ? "VFO" : v->zone_name;
		if (v->vfo && v->ch.mode == RADIO_MODE_DMR && !v->ch.rx_only && v->ch.tx_hz != v->ch.rx_hz) {
			// A split VFO on DMR: the talk row names the target, so the TX frequency goes here (FM's talk row
			// already shows it). A frequency is never cut short: the step gives way, then "VFO"
			sb_init(&b, buf, sizeof buf);
			sb_str(&b, "VFO  TX ");
			sb_freq(&b, v->ch.tx_hz);
			left = buf;
			if (gfx_text_width(L->small, buf) + L->small->w + rw > w)
				rw = 0;
			if (gfx_text_width(L->small, buf) > w)
				left = buf + 5;	// "TX 433.5375"
		}
		if (rw)
			gfx_text(L->small, w - rw, L->zone_y, right, ui_col.dim);
		gfx_text_fit(L->small, left, w - rw - L->small->w, fit, sizeof fit);
		gfx_text(L->small, 0, L->zone_y, fit, v->vfo ? ui_col.warn : ui_col.fg);
	}

	int y = L->channel_y;
	int base = y + L->large->ascent;
	if (v->vfo || s->entry) {
		// The frequency large, the TX frequency in red while transmitting; while typing, the digits so far and
		// blanks for the rest
		bool tx = v->state == RS_TX && v->vfo;
		sb_init(&b, buf, sizeof buf);
		if (s->entry)
			home_entry_text(s, buf, sizeof buf);
		else
			sb_freq(&b, tx ? v->ch.tx_hz : v->ch.rx_hz);
		int x = gfx_text(L->large, 0, y, buf, s->entry ? ui_col.warn : tx ? ui_col.tx : ui_col.fg);
		if (x + L->small->w / 2 + gfx_text_width(L->small, "MHz") <= w)	// 5 decimals leave no room on 120 px
			gfx_text(L->small, x + L->small->w / 2, base - L->small->ascent, "MHz", ui_col.dim);
		if (L->zone_y < 0 && !s->entry)
			gfx_text(L->small, w - 3 * L->small->w, base - L->small->ascent, "VFO", ui_col.dim);
		return;
	}

	// Channel number (medium, dim) and name, baselines aligned. The name uses the largest font it fits in; only a name
	// too long even for the small font is shortened from the middle
	sb_init(&b, buf, sizeof buf);
	sb_u32(&b, v->channel_number, 1);
	int x = gfx_text(L->medium, 0, base - L->medium->ascent, buf, gfx_bpp() == 1 ? ui_col.dim : ui_col.chnum) +
		L->medium->w / 2;
	int avail = w - x;
	if (v->temporary)
		avail -= L->medium->w;
	const font_t *nf = L->large;
	if (gfx_text_width(nf, v->ch.name) > avail)
		nf = L->medium;
	if (gfx_text_width(nf, v->ch.name) > avail)
		nf = L->small;
	gfx_text_fit(nf, v->ch.name, avail, fit, sizeof fit);
	x = gfx_text(nf, x, nf == L->large ? y : base - nf->ascent, fit, ui_col.fg);
	if (v->temporary)
		gfx_text(L->medium, x, base - L->medium->ascent, GLYPH_DOT, ui_col.warn);
}

static void draw_talk(const radio_view_t *v, bool label_first)
{
#if CONFIG_DMR
	if (v->ch.mode == RADIO_MODE_DMR) {
		draw_talk_dmr(v, label_first);
		return;
	}
#else
	(void)label_first;
#endif
	const ui_layout_t *L = &ui_layout;
	const font_t *f = L->medium;
	char buf[60];	// a channel's three forms: up to 57 bytes ("1296.94375 MHz +300.125M" ...)
	sbuf_t b;
	sb_init(&b, buf, sizeof buf);
	color_t c = ui_col.fg;
	bool arrow = true;
	if (v->ch.mode == RADIO_MODE_DMR) {
		sb_str(&b, TR(WHY_DMR_NOT_IN_BUILD, "DMR: not in this build"));
		c = ui_col.warn;
#if CONFIG_P25_TRUNK
	} else if (v->ch.mode == RADIO_MODE_P25 && v->ch.kind == CODEPLUG_KIND_P25_TRUNK &&
		   v->p25.trunk != RADIO_P25_TRUNK_OFF) {
		// What the follower does, and with the transmit side on, how far it is from talking: registering, the
		// talk group once PTT can ask for it, a request's state
		bool txs = (v->p25.flags & RADIO_P25_TRUNK_TX) != 0;
		if (v->p25.trunk == RADIO_P25_TRUNK_HUNT) {
			sb_str(&b, TR(HOME_STATE_P25T_HUNT, "Finding system"));
			c = ui_col.dim;
		} else if (v->p25.trunk == RADIO_P25_TRUNK_CONTROL && v->p25.treg == P25_REG_BLOCKED) {
			sb_str(&b, TR(HOME_STATE_P25T_BLOCKED, "Blocked system"));
			c = ui_col.warn;
		} else if (v->p25.trunk == RADIO_P25_TRUNK_CONTROL && txs && v->p25.treq == P25_REQ_WAIT) {
			sb_str(&b, TR(HOME_STATE_P25T_WAIT, "Wait"));
		} else if (v->p25.trunk == RADIO_P25_TRUNK_CONTROL && txs && v->p25.treq == P25_REQ_QUEUED) {
			sb_str(&b, TR(HOME_STATE_P25T_QUEUED, "Queued"));
		} else if (v->p25.trunk == RADIO_P25_TRUNK_CONTROL && txs && v->p25.treq == P25_REQ_DENIED) {
			sb_str(&b, TR(HOME_STATE_P25T_DENIED, "Denied"));
			c = ui_col.warn;
		} else if (v->p25.trunk == RADIO_P25_TRUNK_CONTROL && txs && v->p25.treg == P25_REG_DENIED) {
			sb_str(&b, TR(HOME_STATE_P25T_REG_DENIED, "Reg. denied"));
			c = ui_col.warn;
		} else if (v->p25.trunk == RADIO_P25_TRUNK_CONTROL && txs && v->p25.treg != P25_REG_READY &&
			   v->p25.treg != P25_REG_OFF && v->p25.treg != P25_REG_NO_TX) {
			sb_str(&b, TR(HOME_STATE_P25T_REG, "Registering"));
			c = ui_col.dim;
		} else if (v->p25.trunk == RADIO_P25_TRUNK_CONTROL && txs && v->p25.treg != P25_REG_NO_TX) {
			sb_str(&b, "TG ");
			sb_u32(&b, v->p25.talk, 1);
		} else if (v->p25.trunk == RADIO_P25_TRUNK_CONTROL) {
			sb_str(&b, TR(HOME_STATE_P25T_CONTROL, "Control channel"));
		} else if (v->p25.treq == P25_REQ_TALK) {
			sb_str(&b, "TG ");
			sb_u32(&b, v->p25.talk, 1);
		} else {
			sb_str(&b, TR(HOME_STATE_P25T_FOLLOW, "Following"));
			sb_str(&b, (v->p25.flags & RADIO_P25_UNIT) != 0 ? " ID " : " TG ");
			sb_u32(&b, v->p25.dst, 1);
		}
#endif
	} else if (v->ch.rx_only) {
		sb_str(&b, TR(WORD_RX_ONLY, "RX only"));
		c = ui_col.warn;
	} else if (v->ch.tx_hz == 0) {
		sb_str(&b, TR(HOME_STATE_NO_TX_FREQ, "No TX frequency"));
		c = ui_col.warn;
#if CONFIG_P25
	} else if (v->ch.mode == RADIO_MODE_P25) {
		// What PTT calls, as on DMR's talk line: "TG 1", or the unit's ID
		if ((v->p25.flags & RADIO_P25_TALK_UNIT) == 0)
			sb_str(&b, "TG ");
		sb_u32(&b, v->p25.talk, 1);
#endif
#if CONFIG_NXDN
	} else if (v->ch.mode == RADIO_MODE_NXDN) {
		// The same for NXDN: the group, or the unit's ID
		if ((v->nxdn.flags & RADIO_NXDN_TALK_UNIT) == 0)
			sb_str(&b, "TG ");
		sb_u32(&b, v->nxdn.talk, 1);
#endif
	} else {
		// A channel shows the frequency it listens on, as dual watch does, and a repeater's offset after it: no
		// other row has either (#1031). Where PTT goes, with the arrow, only while PTT is held (transmitting, or
		// refused: an out-of-band refusal needs the TX frequency in view) and in the VFO, whose receive frequency
		// is the large one above. " MHz", then the offset, go before the frequency is cut (font.h alternates)
		arrow = v->vfo || v->state == RS_TX || v->state == RS_TX_DENIED;
		uint32_t hz = arrow ? v->ch.tx_hz : v->ch.rx_hz;
		sb_str(&b, TEXT_ALT_OPEN);
		sb_freq(&b, hz);
		sb_str(&b, " MHz");
		if (!arrow && v->ch.tx_hz != v->ch.rx_hz) {
			sb_chr(&b, ' ');
			sb_split(&b, v->ch.rx_hz, v->ch.tx_hz);
			sb_str(&b, TEXT_ALT_NEXT);
			sb_freq(&b, hz);
			sb_chr(&b, ' ');
			sb_split(&b, v->ch.rx_hz, v->ch.tx_hz);
		}
		sb_str(&b, TEXT_ALT_NEXT);
		sb_freq(&b, hz);
		sb_str(&b, TEXT_ALT_CLOSE);
	}
	// Too wide: shorter text, never a smaller font
	int x = arrow ? gfx_text(f, 0, L->talk_y, GLYPH_ARROW, ui_col.dim) : 0;	// no space, on purpose: as on DMR's talk line
	char fit[40];
	gfx_text_fit(f, buf, gfx_width() - x, fit, sizeof fit);
	gfx_text(f, x, L->talk_y, fit, c);
}

// Chips: tone, bandwidth, power, squelch. A dot marks a temporary change
#if CONFIG_DMR_TRUNK
static void draw_chips_t3(const radio_view_t *v);
#endif

static void draw_chips(const radio_view_t *v, const ui_state_t *s)
{
	const ui_layout_t *L = &ui_layout;
	const font_t *f = L->small;
	int w = gfx_width();
	char buf[48];
	sbuf_t b;
	int x = 0, y = L->chips_y;

#if CONFIG_DMR_TRUNK
	if (v->t3.sys != 0) {
		draw_chips_t3(v);
		return;
	}
#endif
#if CONFIG_DMR
	if (v->ch.mode == RADIO_MODE_DMR) {
		draw_chips_dmr(v, s);
		return;
	}
#endif
	if (v->am || v->bcast) {
		// No tone, width or power here: the receiver is fixed. The air band squelch is real, the broadcast one isn't used
		x = gfx_text(f, x, y, v->am ? TR(HOME_CHIP_AIR_BAND_AM, "Air Band AM") :
			TR(HOME_CHIP_BROADCAST_FM, "Broadcast FM"), ui_col.fg) + f->w;
		if (v->am) {
			sb_init(&b, buf, sizeof buf);
			sb_str(&b, "SQ");
			sb_u32(&b, v->squelch, 1);
			if (x + gfx_text_width(f, buf) <= w)
				gfx_text(f, x, y, buf, ui_col.fg);
		}
		return;
	}
	bool pw_temp = v->power != channel_power(v);
	if (v->ch.mode == RADIO_MODE_P25) {
		// No sub-audio tone, always 12.5 kHz, no FM squelch: the power, then the NAC heard (F7E: any)
		if (pw_temp)
			x = gfx_text(f, x, y, GLYPH_DOT, ui_col.warn);
		x = gfx_text(f, x, y, ui_power_name(v->power, v->ch.tx_hz), pw_temp ? ui_col.warn : ui_col.fg) + f->w;
#if CONFIG_P25
		sb_init(&b, buf, sizeof buf);
		sb_str(&b, "NAC ");
		sb_hex(&b, v->p25.rx_nac, 3);
		if (x + gfx_text_width(f, buf) <= w)
			gfx_text(f, x, y, buf, ui_col.fg);
#endif
		return;
	}
	if (v->ch.mode == RADIO_MODE_NXDN) {
		// The same for NXDN: the power, the RAN sent and the rate
		if (pw_temp)
			x = gfx_text(f, x, y, GLYPH_DOT, ui_col.warn);
		x = gfx_text(f, x, y, ui_power_name(v->power, v->ch.tx_hz), pw_temp ? ui_col.warn : ui_col.fg) + f->w;
#if CONFIG_NXDN
		sb_init(&b, buf, sizeof buf);
		sb_str(&b, "RAN ");
		sb_u32(&b, v->nxdn.tx_ran, 1);
		sb_str(&b, (v->nxdn.flags & RADIO_NXDN_9600) != 0 ? " 9600" : " 4800");
		if (x + gfx_text_width(f, buf) <= w)
			gfx_text(f, x, y, buf, ui_col.fg);
#endif
		return;
	}
	sb_init(&b, buf, sizeof buf);
	bool tone_temp = (v->temp & (RADIO_TEMP_TX_TONE | RADIO_TEMP_RX_TONE)) != 0;
	if (tone_temp)
		x = gfx_text(f, x, y, GLYPH_DOT, ui_col.warn);
	if (v->ch.tx_tone.type == TONE_NONE && v->ch.rx_tone.type == TONE_NONE) {
		sb_str(&b, TR(CHIP_NO_TONE, "No tone"));
	} else {
		if (v->ch.tx_tone.type != TONE_NONE) {
			if (v->ch.tx_tone.type == TONE_CTCSS)
				sb_chr(&b, 'T');
			sb_tone(&b, &v->ch.tx_tone);
		}
		bool differ = v->ch.rx_tone.type != v->ch.tx_tone.type || v->ch.rx_tone.value != v->ch.tx_tone.value;
		if (v->ch.rx_tone.type != TONE_NONE && differ) {
			if (v->ch.tx_tone.type != TONE_NONE)
				sb_chr(&b, ' ');
			sb_chr(&b, 'R');
			sb_tone(&b, &v->ch.rx_tone);
		}
	}
	x = gfx_text(f, x, y, buf, tone_temp ? ui_col.warn : ui_col.fg) + f->w;
	bool bw_temp = (v->temp & RADIO_TEMP_BW) != 0;
	if (bw_temp)
		x = gfx_text(f, x, y, GLYPH_DOT, ui_col.warn);
	x = gfx_text(f, x, y, v->ch.bw == RADIO_BW_25K ? "25k" : "12.5k", bw_temp ? ui_col.warn : ui_col.fg) + f->w;
	if (pw_temp)
		x = gfx_text(f, x, y, GLYPH_DOT, ui_col.warn);
	x = gfx_text(f, x, y, ui_power_name(v->power, v->ch.tx_hz), pw_temp ? ui_col.warn : ui_col.fg) + f->w;
	sb_init(&b, buf, sizeof buf);
	sb_str(&b, "SQ");
	sb_u32(&b, v->squelch, 1);
	if (x + gfx_text_width(f, buf) <= w)
		gfx_text(f, x, y, buf, ui_col.fg);
}

// RX signal meter: segments for -120 .. -60 dBm
static void draw_meter(int x, int y, int w, int h, int16_t dbm)
{
	int seg = 10;
	int pitch = w / seg;
	int lit = (dbm + 120) * seg / 60;
	if (lit < 0) lit = 0;
	if (lit > seg) lit = seg;
	if (dbm > -120 && lit == 0)
		lit = 1;
	for (int i = 0; i < seg; i++) {
		if (i < lit)
			gfx_fill(x + i * pitch, y, pitch - 1, h, ui_col.rx);
		else if (gfx_bpp() == 1)
			gfx_rect(x + i * pitch, y, pitch - 1, h, ui_col.line);
		else
			gfx_fill(x + i * pitch, y, pitch - 1, h, ui_col.line);
	}
}

// A hardware option the radio still needs from a person (board_hwopt_pending, hal_hwopt.h): what to do, on the home
// screen and when PTT is refused
static void hwopt_needed(sbuf_t *b)
{
	const hal_hwopt_t *o = board_hwopt_pending();
	if (o == NULL) {
		return;
	}
	if (hwopt_state(o) == HWOPT_ST_RESTART) {
		sb_str(b, TR(HW_BODY_RESTART_TO_APPLY, "Restart to apply"));
		return;
	}
	sb_str(b, TR(HW_BODY_SET, "Set: "));
	sb_str(b, o->name);
}

static const char *hwopt_action(void)
{
	const hal_hwopt_t *o = board_hwopt_pending();
	if (o != NULL && hwopt_state(o) == HWOPT_ST_RESTART) {
		return TR(HW_FIX_POWER_CYCLE, "Turn the radio off and on");
	}
	return TR(HW_FIX_SETTINGS_PATH, "Settings>Advanced>Hardware");
}

#if CONFIG_P25_TRUNK || CONFIG_DMR_TRUNK
// RADIO_TRUNK_WHY_* of a refused trunked PTT: the P25 follower's or the Tier III system's
static uint8_t trunk_why(const radio_view_t *v)
{
#if CONFIG_DMR_TRUNK
	if (v->t3.on)
		return v->t3.why;
#endif
#if CONFIG_P25_TRUNK
	return v->p25.twhy;
#else
	return RADIO_TRUNK_WHY_NONE;
#endif
}
#endif

const char *ui_deny_text(const radio_view_t *v, uint8_t why, char *buf, size_t len)
{
	sbuf_t b;
	sb_init(&b, buf, len);
	switch (why) {
	case TXDENY_RX_ONLY: sb_str(&b, TR(DENY_WHY_RX_ONLY, "This channel is RX only")); break;
	case TXDENY_BAND: {
		char bands[32];
		ui_bands_text(bands, sizeof bands, true);
		sb_str(&b, TR(DENY_WHY_BANDS, "Bands "));
		sb_str(&b, bands);
		break;
	}
	case TXDENY_TIMEOUT: sb_str(&b, TR(DENY_WHY_TIMEOUT, "TX time-out reached")); break;
	case TXDENY_CEASED: sb_str(&b, TR(DENY_WHY_SYSTEM_STOPPED, "System stopped TX")); break;
	case TXDENY_NO_FREQ: sb_str(&b, TR(DENY_WHY_NO_TX_FREQ, "No TX frequency set")); break;
	case TXDENY_AES_KEY: sb_str(&b, TR(WHY_AES_KEY, "AES key or TX disabled")); break;
	case TXDENY_AES_WAIT: sb_str(&b, TR(WHY_AES_WAIT, "AES: saving, try again")); break;
	case TXDENY_MODE: sb_str(&b, TR(WHY_MODE_NOT_IN_BUILD, "Mode: not in this build")); break;
	case TXDENY_NO_CODEC: sb_str(&b, TR(WHY_NO_CODEC, "No codec: voice off")); break;
	case TXDENY_NO_DMR: sb_str(&b, TR(DENY_WHY_MODEM_NOT_RUNNING, "DMR modem not running")); break;
	case TXDENY_BUSY: sb_str(&b, TR(DENY_WHY_CHANNEL_BUSY, "Channel busy")); break;
	case TXDENY_NO_REPEATER: sb_str(&b, TR(DENY_WHY_NO_REPEATER, "No repeater answered")); break;
	case TXDENY_REJECTED: sb_str(&b, TR(DENY_WHY_MODEM_REFUSED, "Modem refused to send")); break;
	case TXDENY_HOTSPOT: sb_str(&b, TR(DENY_WHY_HOTSPOT_MODE, "Hotspot mode: PTT off")); break;
	case TXDENY_NO_ID: sb_str(&b, TR(DENY_WHY_NO_DMR_ID, "No DMR ID in codeplug")); break;
	case TXDENY_AM: sb_str(&b, TR(WHY_AIR_BAND_RX_ONLY, "Air band: receive only")); break;
	case TXDENY_BCAST: sb_str(&b, TR(WHY_BROADCAST_RX_ONLY, "Broadcast: receive only")); break;
	case TXDENY_SFR_RPT: sb_str(&b, TR(DENY_WHY_REPEATER_CHANNEL, "Repeater channel: PTT off")); break;
	case TXDENY_HWOPT: hwopt_needed(&b); break;
	case TXDENY_REFUSED: sb_str(&b, TR(DENY_WHY_RADIO_REFUSED, "Radio refused to send")); break;
	case TXDENY_BENCH: sb_str(&b, TR(DENY_WHY_AIR_BENCH, "Air bench: PTT off")); break;
	case TXDENY_AIR_NO_TX: sb_str(&b, TR(DENY_WHY_AIR_NO_TX, "This mode: receive only")); break;
	case TXDENY_NOT_DMR: sb_str(&b, TR(SHARE_BANNER_TRY_ON_DMR, "Try again on a DMR channel")); break;
	case TXDENY_DATA_ENCRYPTED:
		sb_str(&b, TR(DENY_WHY_DATA_ENCRYPTED, "Encrypted channels can't send text"));
		break;
#if CONFIG_P25_TRUNK || CONFIG_DMR_TRUNK
	case TXDENY_TRUNK:
		switch (trunk_why(v)) {
		case RADIO_TRUNK_WHY_NO_CC: sb_str(&b, TR(DENY_WHY_P25T_NO_CC, "No control channel")); break;
		case RADIO_TRUNK_WHY_NOT_REG: sb_str(&b, TR(DENY_WHY_P25T_NOT_REG, "Not registered yet")); break;
		case RADIO_TRUNK_WHY_DENIED: sb_str(&b, TR(DENY_WHY_P25T_DENIED, "System denied the call")); break;
		case RADIO_TRUNK_WHY_NO_ANSWER: sb_str(&b, TR(DENY_WHY_P25T_NO_ANSWER, "No answer from system")); break;
		case RADIO_TRUNK_WHY_NO_CHAN: sb_str(&b, TR(DENY_WHY_P25T_NO_CHAN, "Voice channel unusable")); break;
		case RADIO_TRUNK_WHY_BLOCKED: sb_str(&b, TR(DENY_WHY_P25T_WACN_BLOCKED, "Blocked system: no TX")); break;
		case RADIO_TRUNK_WHY_OTHER_SYS: sb_str(&b, TR(DENY_WHY_TRUNK_OTHER_SYS, "Another system heard")); break;
		case RADIO_TRUNK_WHY_OTHER_KIND: sb_str(&b, TR(DENY_WHY_TRUNK_OTHER_KIND, "Incompatible system")); break;
		case RADIO_TRUNK_WHY_REG_REFUSED: sb_str(&b, TR(DENY_WHY_TRUNK_REG_REFUSED, "Registration refused")); break;
#if CONFIG_DMR_TRUNK
		case RADIO_TRUNK_WHY_UNMAPPED:
			// The channel number the grant named, as the codeplug's System channels table needs it
			sb_str(&b, TR(DENY_WHY_TRUNK_UNMAPPED, "Unknown channel "));
			sb_u32(&b, v->t3.unmapped, 1);
			break;
#endif
		default: sb_str(&b, TR(DENY_WHY_RX_ONLY, "This channel is RX only")); break;
		}
		break;
#endif
	default: sb_str(&b, TR(DENY_WHY_NOT_ALLOWED, "Not allowed here")); break;
	}
	return buf;
}

static const char *deny_action(const radio_view_t *v)
{
	switch (v->tx_deny) {
	case TXDENY_BAND: return v->vfo ? TR(DENY_FIX_TUNE_INTO_BAND, "Tune into a band, then PTT") :
		TR(DENY_FIX_PICK_ANOTHER_CHANNEL, "Pick another channel");
	case TXDENY_TIMEOUT:
	case TXDENY_CEASED: return TR(DENY_FIX_RELEASE_PTT, "Release PTT to talk again");
	case TXDENY_NO_CODEC: return TR(DENY_FIX_ADD_CODEC, "Add codec with rfoundry");
	case TXDENY_BUSY: return TR(DENY_FIX_WAIT_FOR_CALL_END, "Wait for the call to end");
	case TXDENY_NO_REPEATER: return TR(DENY_FIX_CHECK_CC_AND_RANGE, "Check CC and range");
	case TXDENY_REJECTED:
	case TXDENY_NO_DMR: return TR(DENY_FIX_RELEASE_AND_RETRY, "Release PTT, try again");
	case TXDENY_HOTSPOT: return TR(HOTSPOT_HINT_OK_STOPS, "OK stop hotspot");
	case TXDENY_NO_ID: return TR(DENY_FIX_SET_DMR_ID, "Set your DMR ID in qdmr");
	case TXDENY_AM:
	case TXDENY_BCAST: return v->vfo ? TR(DENY_FIX_TUNE_OUT_OF_BAND, "Tune out of the band") :
		TR(DENY_FIX_PICK_ANOTHER_CHANNEL, "Pick another channel");
	case TXDENY_SFR_RPT: return TR(DENY_FIX_PICK_USER_CHANNEL, "Pick a user channel to talk");
	case TXDENY_HWOPT: return hwopt_action();
	case TXDENY_REFUSED: return TR(DENY_FIX_RELEASE_AND_RETRY, "Release PTT, try again");
	case TXDENY_BENCH: return TR(DENY_FIX_END_BENCH_ON_PC, "End the bench on the PC");
#if CONFIG_P25_TRUNK || CONFIG_DMR_TRUNK
	case TXDENY_TRUNK:
		if (trunk_why(v) == RADIO_TRUNK_WHY_OTHER_SYS || trunk_why(v) == RADIO_TRUNK_WHY_UNMAPPED)
			return TR(DENY_FIX_CHECK_SYSTEM, "Check system settings");
		if (trunk_why(v) == RADIO_TRUNK_WHY_REG_REFUSED)
			return TR(DENY_FIX_ASK_TO_ADD_ID, "Ask to add your ID");	// the system doesn't know it
		if (trunk_why(v) == RADIO_TRUNK_WHY_OTHER_KIND)
			return TR(DENY_FIX_PICK_ANOTHER_CHANNEL, "Pick another channel");	// no setting helps
		return TR(DENY_FIX_RELEASE_AND_RETRY, "Release PTT, try again");
#endif
	default: return TR(DENY_FIX_PICK_ANOTHER_CHANNEL, "Pick another channel");
	}
}

// On the air without PTT: the DMR stack sends a data burst list (ARS, SMS, a position, the IP gateway, a roaming probe)
static bool sending_data(const radio_view_t *v)
{
#if CONFIG_DMR
	return v->state == RS_TX && v->ch.mode == RADIO_MODE_DMR && v->dmr.state == RDMR_TX_DATA;
#else
	(void)v;
	return false;
#endif
}

// Outside the DMR block: the event banners call it in every build
bool ui_sb_gw_caller(sbuf_t *b, uint32_t src)
{
#if CONFIG_DMR_TRUNK_GW
	char num[24];
	if (ui_svc_t3_cli(src, num, sizeof num)) {
		sb_str(b, num[0] != '\0' ? num : TR(TRUNK_WORD_PHONE_LINE, "Phone line"));
		return true;
	}
#else
	(void)b;
	(void)src;
#endif
	return false;
}

// --- DMR (docs/ux/wireframes: Home DMR, Receiving, Transmitting) ---
#if CONFIG_DMR
// "TG 123 Main Ops", "Joe 9991234", "All call"; a private call without a name shows the ID only
// A contact name that only repeats the ID ("TG 123", "TG123", "123" for talkgroup 123; "9996543" for that radio):
// codeplugs often name talkgroups that way, and Home would read "TG 123 TG 123"
bool ui_name_is_id(const char *name, uint32_t id, bool private_call)
{
	const char *p = name;
	while (*p == ' ')
		p++;
	if (!private_call && (p[0] == 'T' || p[0] == 't') && (p[1] == 'G' || p[1] == 'g'))
		p += 2;
	while (*p == ' ')
		p++;
	if (*p < '0' || *p > '9')
		return false;
	uint32_t v = 0;
	for (; *p >= '0' && *p <= '9'; p++) {
		if (v > 0xFFFFFFu)
			return false;
		v = v * 10u + (uint32_t)(*p - '0');
	}
	while (*p == ' ')
		p++;
	return *p == '\0' && v == id;
}

static void sb_target(sbuf_t *b, uint32_t id, bool private_call, const char *name)
{
	if (ui_name_is_id(name, id, private_call))
		name = "";
	if (id == 0xFFFFFFu) {
		sb_str(b, TR(VALUE_ALL_CALL, "All call"));
		return;
	}
	if (private_call) {
		if (name[0] != '\0') {
			sb_str(b, name);
			sb_chr(b, ' ');
		}
		sb_u32(b, id, 1);
		return;
	}
	sb_str(b, "TG ");
	sb_u32(b, id, 1);
	if (name[0] != '\0') {
		sb_chr(b, ' ');
		sb_str(b, name);
	}
}

// The target's short form, for when the whole doesn't fit: a talkgroup keeps its number ("TG 123"), a private contact
// its name ("Joe"), since the name is what the user picked and the ID alone says nothing at a glance
static void sb_target_short(sbuf_t *b, uint32_t id, bool private_call, const char *name)
{
	if (private_call && id != 0xFFFFFFu && name[0] != '\0' && !ui_name_is_id(name, id, private_call))
		sb_str(b, name);
	else
		sb_target(b, id, private_call, "");
}

// The target and its short form as text alternates (font.h), each followed by tail ("TG 91 World (9s)",
// "TG 91 (9s)"), and when tail isn't empty last the short form without it. Without a name, the one form
static void sb_target_alts(sbuf_t *b, uint32_t id, bool private_call, const char *name, const char *tail)
{
	bool named = name[0] != '\0' && id != 0xFFFFFFu && !ui_name_is_id(name, id, private_call);
	if (named || tail[0])
		sb_str(b, TEXT_ALT_OPEN);
	sb_target(b, id, private_call, name);
	sb_str(b, tail);
	if (named) {
		sb_str(b, TEXT_ALT_NEXT);
		sb_target_short(b, id, private_call, name);
		sb_str(b, tail);
	}
	if (tail[0]) {
		sb_str(b, TEXT_ALT_NEXT);
		sb_target_short(b, id, private_call, name);
	}
	if (named || tail[0])
		sb_str(b, TEXT_ALT_CLOSE);
}

// For the talk line: a named talkgroup as text alternates, longest first ("TG 201 Main Ops (8s)", "TG 201 Main Ops",
// "TG 201 Main O…" cut to room cells, "TG 201"), so the reply seconds go before the label and the label is cut before
// it goes. The cut form only when two letters of the label fit. label_first (Settings > Display, a side key) puts the
// name first and drops the number before the name. Anything else: sb_target_alts
static void sb_talk_alts(sbuf_t *b, uint32_t id, bool private_call, const char *name, const char *tail, size_t room,
			 bool label_first)
{
	bool named = name[0] != '\0' && id != 0xFFFFFFu && !ui_name_is_id(name, id, private_call);
	if (private_call || !named) {
		sb_target_alts(b, id, private_call, name, tail);
		return;
	}
	char num[16];
	sbuf_t nb;
	sb_init(&nb, num, sizeof num);
	sb_target(&nb, id, false, "");
	size_t cells = ui_text_cells(name);
	sb_str(b, TEXT_ALT_OPEN);
	if (label_first) {
		// "Main Ops TG 201 (9s)", "Main Ops TG 201", "Main Ops", "Main O…": the name is what the user reads
		if (tail[0]) {
			sb_str(b, name);
			sb_chr(b, ' ');
			sb_str(b, num);
			sb_str(b, tail);
			sb_str(b, TEXT_ALT_NEXT);
		}
		sb_str(b, name);
		sb_chr(b, ' ');
		sb_str(b, num);
		sb_str(b, TEXT_ALT_NEXT);
		sb_str(b, name);
		if (room >= 2 && cells > room - 1) {
			sb_str(b, TEXT_ALT_NEXT);
			sb_cut_text(b, name, room - 1);
			sb_str(b, GLYPH_ELLIPSIS);
		}
		sb_str(b, TEXT_ALT_CLOSE);
		return;
	}
	size_t lead = strlen(num) + 1;	// "TG 201 "
	if (tail[0]) {
		sb_target(b, id, false, name);
		sb_str(b, tail);
		sb_str(b, TEXT_ALT_NEXT);
	}
	sb_target(b, id, false, name);
	if (room >= lead + 3 && cells > room - lead - 1) {
		sb_str(b, TEXT_ALT_NEXT);
		sb_str(b, num);
		sb_chr(b, ' ');
		sb_cut_text(b, name, room - lead - 1);
		sb_str(b, GLYPH_ELLIPSIS);
	}
	sb_str(b, TEXT_ALT_NEXT);
	sb_str(b, num);
	sb_str(b, TEXT_ALT_CLOSE);
}

// The caller as the big line: the contact name, else the ID
static void sb_caller(sbuf_t *b, const radio_dmr_call_t *c)
{
	if (c->src_name[0] == '\0' && ui_sb_gw_caller(b, c->src))
		return;
	if (c->src_name[0] != '\0')
		sb_str(b, c->src_name);
	else
		sb_u32(b, c->src, 1);
}

// Where the call went: "TG 123 Main Ops", "Private to you", "Private to 9991234"
static void sb_call_dst(sbuf_t *b, const radio_dmr_call_t *c)
{
	if (c->group) {
		sb_target(b, c->dst, false, c->dst_name);
	} else if (c->to_us) {
		sb_str(b, TR(CALL_VALUE_PRIVATE_TO_YOU, "Private to you"));
	} else {
		sb_str(b, TR(CALL_VALUE_PRIVATE_TO, "Private to "));
		if (c->dst_name[0] != '\0')
			sb_str(b, c->dst_name);
		else
			sb_u32(b, c->dst, 1);
	}
}

static bool dmr_call_screen(const radio_view_t *v)
{
	if (v->aprs_tx || v->ch.mode != RADIO_MODE_DMR || !v->dmr.active || v->hotspot)
		return false;
	if (v->state == RS_TX)
		return !sending_data(v);	// data has no talk target to show: Home stays, its activity line says so
	return v->state == RS_RX && (v->dmr.state == RDMR_RX || v->dmr.state == RDMR_RX_OTHER);
}

// Talk target line on Home: an arrow and "TG 201 Main Ops", or an inverted return arrow and "TG 9 (8s)" in warn color
// during the reply window. Icons, not words, so a small display keeps room for the label (#336)
static bool ui_st_dmr_off;		// ui_state_t.dmr_off of the frame being drawn

static void draw_talk_dmr(const radio_view_t *v, bool label_first)
{
	const ui_layout_t *L = &ui_layout;
	const font_t *f = L->medium;
	const radio_dmr_view_t *d = &v->dmr;
	char buf[88], fit[48];
	sbuf_t b;
	color_t c = ui_col.fg;

	int w = gfx_width();
	// the reply window's icon inverted, so it stands out on a monochrome display as well. No space after either icon,
	// on purpose: the glyph's own blank column (or the inverted cell) sets it apart, and the space is one more
	// character of the label on a small display
	int x = d->reply_active ? gfx_text_bg(f, 0, L->talk_y, GLYPH_REPLY, ui_col.bg, ui_col.warn)
		: gfx_text(f, 0, L->talk_y, GLYPH_ARROW, ui_col.dim);
	size_t room = x < w ? (size_t)((w - x) / f->w) : 0;
	sb_init(&b, buf, sizeof buf);
	if (v->ch.kind != CODEPLUG_KIND_NONE && !codeplug_kind_built(v->ch.kind)) {
		// a mode this build doesn't run: its record's DMR fields are only a safe stand-in
		sb_str(&b, TR(WHY_MODE_NOT_IN_BUILD, "Mode: not in this build"));
		c = ui_col.warn;
	} else if (v->ch.rx_only) {
		sb_str(&b, TR(WORD_RX_ONLY, "RX only"));
		c = ui_col.warn;
	} else if (d->sfr_rpt) {
		// the channel's repeater role has the modem
		sb_str(&b, d->sfr_cw ? TR(HOME_STATE_SFR_SENDING_ID, "Sending ID") : d->sfr_busy ?
			TR(HOME_STATE_SFR_REPEATING, "Repeating") : TR(HOME_STATE_SFR_REPEATER, "SFR repeater"));
	} else if (!d->active && ui_st_dmr_off) {
		sb_str(&b, TR(HOME_STATE_DMR_MODEM_OFF, "DMR modem off"));
		c = ui_col.warn;
	} else {
		// The target and its shorter forms: "TG 91 World (9s)", "TG 91 World", "TG 91 Wo…", "TG 91"; "Joe 3101234", "Joe"
		char secs[8] = "";
		if (d->reply_active) {
			sbuf_t sb;
			sb_init(&sb, secs, sizeof secs);
			sb_str(&sb, " (");
			sb_u32(&sb, d->reply_s, 1);
			sb_str(&sb, "s)");
			c = ui_col.warn;
		}
		// and last without the reply seconds (the hint still says the reply is on)
		sb_talk_alts(&b, d->target_id, d->target_private, d->target_name, secs, room, label_first);
	}
	// Too wide: the shorter alternate (a talkgroup's label cut, then dropped; a contact its ID), never a smaller font
	gfx_text_fit(f, buf, w - x, fit, sizeof fit);
	gfx_text(f, x, L->talk_y, fit, c);
}

#if CONFIG_DMR_TRUNK
void ui_draw_incoming_call(const radio_view_t *v, const ui_state_t *s, const char *name)
{
	const ui_layout_t *L = &ui_layout;
	int w = gfx_width();
	char id[12], prompt[2][64];
	static char fit[CODEPLUG_NAME_LEN + 1];
	bool named = name[0] != 0;
	sbuf_t b;
	sb_init(&b, id, sizeof id);
	sb_u32(&b, v->t3.ring_src, 1);
	const char *title = TR(TRUNK_TITLE_INCOMING_CALL, "Incoming private call");
	const font_t *title_font = gfx_text_width(L->medium, title) <= w - 4 ? L->medium : L->small;
	int lines = wrap_lines(L->small, TR(TRUNK_HINT_PTT_ANSWER, "Press PTT to answer"),
			       w - 4, 2, prompt, false);

	gfx_clear(ui_col.bg);
	int top = ui_draw_top(v, s, NULL, NULL);
	int labels = (lines + named) * L->small->h;
	int height = title_font->h + L->large->h + labels;
	const font_t *id_font = height + 4 <= gfx_height() - top ? L->large : L->medium;
	height = title_font->h + id_font->h + labels;
	int gap = (gfx_height() - top - height) / 4;
	int y = top + gap;
	ui_draw_title(title_font, 2, y, w - 4, title, NULL, ui_col.fg);
	y += title_font->h + gap;
	if (named) {
		gfx_text_fit(L->small, name, w - 4, fit, sizeof fit);
		gfx_text(L->small, (w - gfx_text_width(L->small, fit)) / 2, y, fit, ui_col.fg);
		y += L->small->h;
	}
	gfx_text(id_font, (w - gfx_text_width(id_font, id)) / 2, y, id, ui_col.fg);
	y += id_font->h + gap;
	for (int i = 0; i < lines; i++, y += L->small->h)
		gfx_text(L->small, (w - gfx_text_width(L->small, prompt[i])) / 2, y, prompt[i], ui_col.fg);
}

// What the radio is doing on the trunked channel's system (dmr-trunking.md 12.4); *warn: something the user should see
static const char *t3_state_word(const radio_view_t *v, bool *warn)
{
	*warn = true;
	if (v->t3.unavail)
		return TR(TRUNK_WHY_SYSTEM_NOT_SET_UP, "System not set up");
	if (v->t3.fallback != 0)	// on the system's fallback channel, or looking for the system from it
		return TR(TRUNK_STATE_FALLBACK, "Fallback");
	if (v->t3.on && v->t3.state == T3_HUNT && (v->t3.other != 0 || v->t3.other_fid != 0))
		return TR(TRUNK_STATE_OTHER_SYSTEM, "Other system");	// Home's activity says which (draw_t3_other)
	if (v->t3.on && v->t3.refused)	// hunting for another TSCC, or registering again where it was refused
		return TR(TRUNK_STATE_REG_REFUSED, "Reg. refused");
	if (!v->t3.on || v->t3.state == T3_HUNT)
		return TR(TRUNK_STATE_SEARCHING, "Searching");
	if (v->t3.ringing)
		return TR(ACT_ROW_PRIVATE_CALL, "Private call");
	if (v->t3.state == T3_FOLLOW) {
		*warn = false;
		return TR(TRUNK_STATE_IN_CALL, "In call");
	}
	if (v->t3.alerting) {
		*warn = false;
		return TR(TRUNK_STATE_CALLING, "Calling");
	}
	if (v->t3.reg == T3_REG_DENIED)
		return TR(TRUNK_STATE_REFUSED, "Refused");
	if (v->t3.reg == T3_REG_NEEDED || v->t3.reg == T3_REG_UNKNOWN)
		return TR(TRUNK_STATE_REGISTERING, "Registering");
	*warn = false;
	return TR(TRUNK_STATE_READY, "Ready");
}

// Chips on a trunked channel: the system's name, what the radio does on it, power. Slot and color code are the
// site's, not the channel's. On the fallback channel the name and frequency rows are that channel's
static void draw_chips_t3(const radio_view_t *v)
{
	const ui_layout_t *L = &ui_layout;
	const font_t *f = L->small;
	static char name[CODEPLUG_NAME_LEN + 1];
	static uint8_t named;		// the system name holds (n + 1; 0 none): read once, not every frame
	if (named != v->t3.sys) {
		named = v->t3.sys;
		if (!ui_svc_t3_name((uint8_t)(v->t3.sys - 1u), name) || name[0] == '\0')
			str_copy(name, sizeof name, TR(TRUNK_WORD_SYSTEM, "System"));
	}
	bool warn;
	const char *st = t3_state_word(v, &warn);
	const char *pw = ui_power_name(v->power, v->ch.tx_hz);
	int room = gfx_width() - gfx_text_width(f, st) - gfx_text_width(f, pw) - 2 * f->w;
	char fit[CODEPLUG_NAME_LEN + 4];
	int x = 0;
	if (room > 0) {
		gfx_text_fit(f, name, room, fit, sizeof fit);
		x = gfx_text(f, x, L->chips_y, fit, ui_col.fg) + f->w;
	}
	x = gfx_text(f, x, L->chips_y, st, warn ? ui_col.warn : ui_col.fg) + f->w;
	if (x + gfx_text_width(f, pw) <= gfx_width())
		gfx_text(f, x, L->chips_y, pw, ui_col.fg);
}
#endif

// Chips: timeslot, color code, repeater / direct, roaming, power, hear all
static void draw_chips_dmr(const radio_view_t *v, const ui_state_t *s)
{
	const ui_layout_t *L = &ui_layout;
	const font_t *f = L->small;
	char buf[40];
	sbuf_t b;
	// Timeslot and color code, each with the dot when it is a temporary change
	int x = 0;
	for (int k = 0; k < 2; k++) {
		bool temp = (v->temp & (k ? RADIO_TEMP_CC : RADIO_TEMP_TS)) != 0;
		sb_init(&b, buf, sizeof buf);
		if (temp)
			sb_str(&b, GLYPH_DOT);
		sb_str(&b, k ? "CC" : "TS");
		sb_u32(&b, k ? (v->dmr.active ? v->dmr.cc : v->ch.color_code) : (v->dmr.ts ? v->dmr.ts : v->ch.timeslot), 1);
		x = gfx_text(f, x, L->chips_y, buf, temp ? ui_col.warn : ui_col.fg) + f->w;
	}
	if (v->dmr.rmo)
		x = gfx_text(f, x, L->chips_y, "Rpt", ui_col.fg) + f->w;	// a repeater channel; the signal bars show it heard
#if CONFIG_DMR_ROAMING
	// Roaming here: Roam, Lock when locked to the site, amber Roam? when the site is weak (dmr-roaming.md 6.2)
	if (v->roam.state != RROAM_STATE_OFF) {
		bool lock = v->roam.state == RROAM_STATE_LOCKED;
		x = gfx_text(f, x, L->chips_y, lock ? TR(HOME_CHIP_ROAM_LOCK, "Lock") : v->roam.weak ?
			TR(HOME_CHIP_ROAM_WEAK, "Roam?") : TR(HOME_CHIP_ROAM, "Roam"),
			     v->roam.weak ? ui_col.warn : ui_col.fg) + f->w;
	}
#endif
	bool pw_temp = v->power != channel_power(v);
	if (pw_temp)
		x = gfx_text(f, x, L->chips_y, GLYPH_DOT, ui_col.warn);
	x = gfx_text(f, x, L->chips_y, ui_power_name(v->power, v->ch.tx_hz), pw_temp ? ui_col.warn : ui_col.fg) + f->w;
	if (L->zone_y < 0 && v->vfo && !v->ch.rx_only && v->ch.tx_hz != v->ch.rx_hz) {
		// A split VFO with no top row for its TX frequency: the offset here ("+600k")
		sb_init(&b, buf, sizeof buf);
		sb_split(&b, v->ch.rx_hz, v->ch.tx_hz);
		if (x + gfx_text_width(f, buf) <= gfx_width())
			x = gfx_text(f, x, L->chips_y, buf, ui_col.warn) + f->w;
	}
	// Measured, not counted: a translation may be longer
	const char *dmon = TR(HOME_CHIP_DMON, "DMon"), *priv = TR(HOME_CHIP_PRIVACY, "Priv");
	if ((v->dmr.hear_all || s->hear_all) && x + gfx_text_width(f, dmon) <= gfx_width())
		x = gfx_text(f, x, L->chips_y, dmon, ui_col.warn) + f->w;	// digital monitor (FM's is "Mon")
	if ((s->privacy || v->dmr.privacy) && x + gfx_text_width(f, priv) <= gfx_width())
		gfx_text(f, x, L->chips_y, priv, ui_col.priv);
}

#if CONFIG_DMR_TRUNK
// Activity on Home while the radio searches a trunked system and hears only another one: what it heard, to compare
// with the system's settings (dmr-trunking.md 12.4): "Heard: Huge, net 0" / "site 83" (and the category when the
// site takes one only), or the first line alone where there is room for one. A system of another kind (no C_SYScode):
// "Incompatible system" / "Cannot join"
static void draw_t3_other(const radio_view_t *v, int y)
{
	const font_t *sm = ui_layout.small;
	int w = gfx_width();
	char a[48], c[40], fit[48];
	sbuf_t b;
	t3_syscode_t sc;
	if (v->t3.other == 0) {
		// Only a trunking system of another kind heard (its own FID, no ETSI Aloha): no setting helps
		gfx_text_fit(sm, TR(TRUNK_OTHER_KIND, "Incompatible system"), w, fit, sizeof fit);
		if (ui_layout.activity_h >= 2 * sm->h) {
			gfx_text(sm, 0, y, fit, ui_col.warn);
			gfx_text_fit(sm, TR(TRUNK_OTHER_KIND_NOTE, "Cannot join"), w, fit, sizeof fit);
			gfx_text(sm, 0, y + sm->h, fit, ui_col.fg);
		} else {
			gfx_text(sm, 0, y + (ui_layout.activity_h - sm->h) / 2, fit, ui_col.warn);
		}
		return;
	}
	t3_syscode_split(v->t3.other, &sc);
	static const tr_id_t model[4] = {	// the editor's network model names
		TRN(TRUNK_MODEL_TINY, "Tiny"), TRN(TRUNK_MODEL_SMALL, "Small"), TRN(TRUNK_MODEL_LARGE, "Large"),
		TRN(TRUNK_MODEL_HUGE, "Huge"),
	};
	sb_init(&b, a, sizeof a);
	sb_str(&b, TR(TRUNK_OTHER_HEARD, "Heard: "));
	sb_str(&b, tr(model[sc.model & 3u]));
	sb_str(&b, TR(TRUNK_OTHER_NET, ", net "));
	sb_u32(&b, sc.net, 1);
	sb_init(&b, c, sizeof c);
	sb_str(&b, TR(TRUNK_OTHER_SITE, "site "));
	sb_u32(&b, sc.site, 1);
	if (sc.par == T3_PAR_A)
		sb_str(&b, TR(TRUNK_OTHER_CAT_A, ", category A only"));
	else if (sc.par == T3_PAR_B)
		sb_str(&b, TR(TRUNK_OTHER_CAT_B, ", category B only"));
	if (ui_layout.activity_h >= 2 * sm->h) {
		gfx_text_fit(sm, a, w, fit, sizeof fit);
		gfx_text(sm, 0, y, fit, ui_col.warn);
		gfx_text_fit(sm, c, w, fit, sizeof fit);
		gfx_text(sm, 0, y + sm->h, fit, ui_col.fg);
		return;
	}
	// One line: the model and network, what a wrong setting usually is
	gfx_text_fit(sm, a, w, fit, sizeof fit);
	gfx_text(sm, 0, y + (ui_layout.activity_h - sm->h) / 2, fit, ui_col.warn);
}
#endif

// Activity on Home, DMR idle: the last call heard. Two lines where they fit (the caller, then where and when), else
// one line where the caller's name gives way first: "Joe Smith" / "TG 91, 11s ago", or "Joe Smith  TG 91 11s ago"
static void draw_last_heard(const radio_view_t *v, const ui_state_t *s, int y)
{
	const font_t *sm = ui_layout.small;
	const radio_dmr_view_t *d = &v->dmr;
	int w = gfx_width();
	char who[24], where[48], fit[48];
	sbuf_t b;
	if (d->last_ms == 0) {
		gfx_text(sm, 0, y + (ui_layout.activity_h - sm->h) / 2, TR(EMPTY_NO_CALLS_HEARD, "No calls heard yet"),
			ui_col.dim);	// centered
		return;
	}
	sb_init(&b, who, sizeof who);
	sb_caller(&b, &d->last);
	sb_init(&b, where, sizeof where);
	if (d->last.group) {
		sb_str(&b, "TG ");
		sb_u32(&b, d->last.dst, 1);
	} else {
		sb_str(&b, d->last.to_us ? TR(CALL_VALUE_TO_YOU, "To you") : TR(VALUE_PRIVATE, "Private"));
	}
	sb_chr(&b, ' ');
	sb_age(&b, s->now_ms - d->last_ms);
	if (ui_layout.activity_h >= 2 * sm->h) {
		gfx_text_fit(sm, who, w, fit, sizeof fit);
		gfx_text(sm, 0, y, fit, ui_col.fg);
		gfx_text_fit(sm, where, w, fit, sizeof fit);
		gfx_text(sm, 0, y + sm->h, fit, ui_col.dim);
		return;
	}
	// One line: where and when on the right as the longest form that leaves the name 6 characters, the name before it
	y += (ui_layout.activity_h - sm->h) / 2;
	gfx_text_fit(sm, where, w - 7 * sm->w, fit, sizeof fit);
	int ww = gfx_text_width(sm, fit);
	gfx_text(sm, w - ww, y, fit, ui_col.dim);
	gfx_text_fit(sm, who, w - ww - sm->w, fit, sizeof fit);
	gfx_text(sm, 0, y, fit, ui_col.fg);
}

// One text line of the call screens, if it still fits above the hint row; returns the next y
static int call_line(const font_t *f, int y, const char *text, color_t c)
{
	char fit[48];
	if (y + f->h > ui_layout.hint_y)
		return y;
	gfx_text_fit(f, text, gfx_width(), fit, sizeof fit);
	gfx_text(f, 0, y, fit, c);
	return y + f->h;
}

// Receiving and Transmitting: the call takes the screen below the status strip
static void draw_dmr_call(const radio_view_t *v, const ui_state_t *s)
{
	const ui_layout_t *L = &ui_layout;
	const radio_dmr_view_t *d = &v->dmr;
	int w = gfx_width();
	char buf[48], title[40];
	sbuf_t b;

	sb_init(&b, title, sizeof title);
	if (v->state == RS_TX && d->target_private && d->target_id != 0xFFFFFFu) {
		// A private call says so where a group call has its "TG": "Private 9991234", else "Private"
		sb_str(&b, TEXT_ALT_OPEN);
		sb_str(&b, TR(CALL_BODY_PRIVATE_ID, "Private "));
		sb_u32(&b, d->target_id, 1);
		sb_str(&b, TEXT_ALT_NEXT);
		sb_str(&b, TR(VALUE_PRIVATE, "Private"));
		sb_str(&b, TEXT_ALT_CLOSE);
	} else if (v->state == RS_TX) {
		sb_target(&b, d->target_id, d->target_private, "");
	} else {
		sb_call_dst(&b, &d->call);
	}
	gfx_clear(ui_col.bg);
	int y = ui_draw_top(v, s, NULL, title) + 1;

	if (v->state == RS_TX) {
		const char *word = TR(STATE_TALKING, "TALKING");
		switch (d->state) {
		case RDMR_TX_WAIT: word = d->rmo && !d->repeater_up ? TR(CALL_STATE_WAKING_RPT, "WAKING RPT") :
			TR(CALL_STATE_WAITING, "WAITING"); break;
		// the channel is ours: the talk permit sounds now
		case RDMR_TX_PERMIT: word = TR(CALL_STATE_ADMITTED, "ADMITTED"); break;
		case RDMR_TX_ENDING: word = TR(CALL_STATE_ENDING, "ENDING"); break;
		default: break;
		}
		const font_t *big = gfx_text_width(L->large, word) <= w ? L->large : L->medium;
		gfx_text_fit(big, word, w, buf, sizeof buf);	// a translation too wide even for the medium font
		gfx_text(big, 0, y, buf, ui_col.tx);
		y += big->h + 2;
		// The target with its name, else its short form: a talkgroup's number, a private contact's name
		sb_init(&b, buf, sizeof buf);
		sb_target_alts(&b, d->target_id, d->target_private, d->target_name, "");
		y = call_line(L->medium, y, buf, ui_col.fg);
		if (v->vfo) {	// a channel has its name in the codeplug; the VFO has only its TX frequency
			sb_init(&b, buf, sizeof buf);
			sb_str(&b, "TX ");
			sb_freq(&b, v->ch.tx_hz);
			sb_str(&b, " MHz");
			y = call_line(L->small, y, buf, ui_col.tx);
		}
		sb_init(&b, buf, sizeof buf);
		sb_str(&b, "TS");
		sb_u32(&b, d->ts, 1);
		sb_str(&b, " CC");
		sb_u32(&b, d->cc, 1);
		sb_str(&b, d->target_private ? TR(CALL_BODY_TO_PRIVATE, " Private") : TR(CALL_BODY_TO_GROUP, " Group"));
		if (d->rmo)
			sb_str(&b, " RPT");
		if (d->privacy)
			sb_str(&b, " ENC");
		y = call_line(L->small, y, buf, ui_col.dim);
		sb_init(&b, buf, sizeof buf);
		sb_mmss(&b, v->tx_elapsed_ms / 1000u);
		if (v->tot_s) {
			sb_str(&b, TR(PART_OF, " of "));	// the time-out
			sb_mmss(&b, v->tot_s);
		}
		call_line(L->small, y, buf, ui_col.fg);
		draw_hint(d->state == RDMR_TX_ENDING ? TR(CALL_HINT_FINISHING, "Finishing the call") :
			TR(CALL_HINT_RELEASE_PTT, "Release PTT to stop"), 0, home_soft_key(s));
		return;
	}

	// Receiving: the caller large, then ID, alias, talkgroup, signal
	sb_init(&b, buf, sizeof buf);
	sb_caller(&b, &d->call);
	const font_t *big = gfx_text_width(L->large, buf) <= w ? L->large : L->medium;
	char fit[40];
	gfx_text_fit(big, buf, w, fit, sizeof fit);
	gfx_text(big, 0, y, fit, d->call.emergency ? ui_col.warn : (d->state == RDMR_RX ? ui_col.fg : ui_col.dim));
	y += big->h;
	sb_init(&b, buf, sizeof buf);
	if (d->call.src_name[0] != '\0') {
		sb_u32(&b, d->call.src, 1);
		sb_chr(&b, ' ');
	}
	sb_str(&b, "TS");
	sb_u32(&b, d->rx_ts ? d->rx_ts : d->ts, 1);
	if (d->hear_all && d->rx_cc != d->cc) {	// found by digital monitor: the code to set to talk to them
		sb_str(&b, " CC");
		sb_u32(&b, d->rx_cc, 1);
	}
	if (d->late_entry)
		sb_str(&b, TR(CALL_CHIP_LATE, " late"));
	if (d->call.encrypted)
		sb_str(&b, " ENC");	// privacy marker (played only with the key, never after a PI header)
	if (d->call.emergency)
		sb_str(&b, TR(CALL_CHIP_EMERGENCY, " EMERGENCY"));
	y = call_line(L->small, y, buf, d->call.emergency ? ui_col.warn : ui_col.dim);
	if (d->alias[0] != '\0' && s->alias_show) {
		// The talker alias in quotes, on two lines when it needs them and they fit above the signal line
		char lines[2][64];
		sb_init(&b, buf, sizeof buf);
		sb_chr(&b, '"');
		sb_str(&b, d->alias);
		sb_chr(&b, '"');
		int room = (L->hint_y - y) / L->small->h - 2;	// the talkgroup and signal lines stay
		int n = wrap_lines(L->small, buf, w, room >= 2 ? 2 : 1, lines, false);
		for (int i = 0; i < n; i++)
			y = call_line(L->small, y, lines[i], ui_col.fg);
	}
	if (d->call.gps_valid) {	// the caller's ETSI GPS Info: how far and which way, else where
		sb_init(&b, buf, sizeof buf);
		if (d->call.gps_range) {
			sb_range(&b, d->call.gps_dist_m, d->call.gps_bearing);
		} else {
			sb_degrees(&b, d->call.gps_lat);
			sb_chr(&b, ' ');
			sb_degrees(&b, d->call.gps_lon);
		}
		y = call_line(L->small, y, buf, ui_col.fg);
	}
	// Then why it isn't heard, or where the call goes ("TG 91 World")
	const char *why = d->state == RDMR_RX_OTHER ? (d->call.encrypted ?
		TR(CALL_WHY_ENCRYPTED, "Encrypted: not played") :
		TR(CALL_WHY_OTHER_TG, "Other TG: not played")) : !d->codec ? TR(WHY_NO_CODEC, "No codec: voice off") :
			NULL;
	if (why != NULL) {
		y = call_line(L->small, y, why, ui_col.warn);
	} else {
		sb_init(&b, buf, sizeof buf);
		sb_call_dst(&b, &d->call);
		y = call_line(L->small, y, buf, ui_col.fg);
	}
	if (y + L->small->h <= L->hint_y) {
		int16_t dbm = d->rssi_dbm != 0 ? d->rssi_dbm : v->rssi_dbm;
		sb_init(&b, buf, sizeof buf);
		sb_i32(&b, dbm);
		sb_str(&b, " dBm");
		int tw = gfx_text_width(L->small, buf);
		gfx_text(L->small, w - tw, y, buf, ui_col.fg);
		draw_meter(0, y + L->small->h / 4, w - tw - L->small->w, L->small->h / 2, dbm);
	}
	draw_hint(d->state == RDMR_RX_OTHER ? TR(CALL_HINT_MON_DMON, "MON: digital monitor") :
		TR(HINT_PTT_REPLIES, "PTT replies"), 0, home_soft_key(s));
}
#endif

#if CONFIG_AIR
// A P25 or NXDN call coming in (air_call): whether one is heard now, and its fields
typedef struct {
	bool played, enc, unit;
	uint32_t src, dst;
} air_call_t;

static bool air_call(const radio_view_t *v, air_call_t *c)
{
#if CONFIG_P25
	if (v->ch.mode == RADIO_MODE_P25 && (v->p25.flags & RADIO_P25_RX) != 0) {
		const radio_p25_view_t *p = &v->p25;
		c->played = (p->flags & RADIO_P25_PLAYED) != 0;
		c->enc = (p->flags & RADIO_P25_ENC) != 0;
		c->unit = (p->flags & RADIO_P25_UNIT) != 0;
		c->src = p->src;
		c->dst = p->dst;
		return true;
	}
#endif
#if CONFIG_NXDN
	if (v->ch.mode == RADIO_MODE_NXDN && (v->nxdn.flags & RADIO_NXDN_RX) != 0) {
		const radio_nxdn_view_t *n = &v->nxdn;
		c->played = (n->flags & RADIO_NXDN_PLAYED) != 0;
		c->enc = (n->flags & RADIO_NXDN_ENC) != 0;
		c->unit = (n->flags & RADIO_NXDN_UNIT) != 0;
		c->src = n->src;
		c->dst = n->dst;
		return true;
	}
#endif
	(void)v;
	(void)c;
	return false;
}

// Why the call isn't played, else who calls and where ("ID 1234 > TG 1", "ID 1234 > Private to 5678"; the ID is
// missing until the call's link control is read)
static void air_call_line(const air_call_t *c, const font_t *f, int y)
{
	char buf[64], fit[48];
	sbuf_t b;

	if (!c->played) {
		gfx_text_fit(f, c->enc ? TR(CALL_WHY_ENCRYPTED, "Encrypted: not played") :
			TR(CALL_WHY_OTHER_TG, "Other TG: not played"), gfx_width(), fit, sizeof fit);
		gfx_text(f, 0, y, fit, ui_col.warn);
		return;
	}
	// Too wide: keep the marker and destination, then cut the destination if needed
	for (int with_id = c->src != 0; with_id >= 0; with_id--) {
		sb_init(&b, buf, sizeof buf);
		if (c->enc)
			sb_str(&b, "ENC ");
		if (with_id) {
			sb_str(&b, "ID ");
			sb_u32(&b, c->src, 1);
			sb_str(&b, " " GLYPH_ARROW " ");
		}
		if (c->unit) {
			sb_str(&b, TR(CALL_VALUE_PRIVATE_TO, "Private to "));
		} else {
			sb_str(&b, "TG ");
		}
		sb_u32(&b, c->dst, 1);
		if (gfx_text_width(f, buf) <= gfx_width())
			break;
	}
	gfx_text_fit(f, buf, gfx_width(), fit, sizeof fit);
	gfx_text(f, 0, y, fit, ui_col.rx);
}
#endif

static void draw_activity(const radio_view_t *v, const ui_state_t *s)
{
	const ui_layout_t *L = &ui_layout;
	const font_t *big = L->medium, *sm = L->small;
	int w = gfx_width();
	int y = L->activity_y;
	bool two = L->activity_h >= big->h + sm->h;
	if (!two)
		big = sm;
	int y2 = y + big->h;
	char buf[96], fit[48];
	sbuf_t b;
	sb_init(&b, buf, sizeof buf);

	// The monitor open on a quiet channel shows the meter too: what the receiver hears with nothing to open the squelch
	bool meter = v->state == RS_RX || (v->state == RS_IDLE && v->monitor && v->ch.mode != RADIO_MODE_DMR);
	switch (meter ? RS_RX : v->state) {
	case RS_RX: {
#if CONFIG_AIR
		air_call_t ac;
		bool air_rx = air_call(v, &ac);
		if (!two && air_rx) {
			air_call_line(&ac, sm, y);	// one line: who calls says more than the meter
			break;
		}
#endif
		sb_i32(&b, v->rssi_dbm);
		sb_str(&b, " dBm");
		int tw = gfx_text_width(big, buf);
		gfx_text(big, w - tw, y, buf, ui_col.fg);
		draw_meter(0, y + big->h / 4, w - tw - big->w, big->h / 2, v->rssi_dbm);
		if (!two)
			break;
#if CONFIG_AIR
		if (air_rx) {
			air_call_line(&ac, sm, y2);
			break;
		}
#endif
		gfx_text(sm, 0, y2, v->monitor ? TR(ACTIVITY_STATE_SQUELCH_OPEN, "Squelch open") :
			TR(STATE_RECEIVING, "Receiving"), ui_col.rx);
		break;
	}
	case RS_TX: {
		if (v->aprs_tx) {
			gfx_text(big, 0, y, TR(STATE_SENDING, "SENDING"), ui_col.tx);
			break;
		}
		sb_mmss(&b, v->tx_elapsed_ms / 1000u);
		int tw = gfx_text_width(big, buf);
		gfx_text(big, w - tw, y, buf, ui_col.fg);
		gfx_text(big, 0, y, sending_data(v) ? TR(STATE_SENDING, "SENDING") : TR(STATE_TALKING, "TALKING"),
			ui_col.tx);
		if (two) {
			sb_init(&b, buf, sizeof buf);
			if (v->dtmf_digits[0] != 0) {
				// DTMF sent in this transmission (radio-features.md 3.7): what the other end got, newest last
				sb_str(&b, "DTMF ");
				sb_str(&b, v->dtmf_digits);
				gfx_text(sm, 0, y2, buf, ui_col.fg);
				break;
			}
			if (v->tot_s) {
				uint32_t el = v->tx_elapsed_ms / 1000u;
				sb_mmss(&b, el < v->tot_s ? v->tot_s - el : 0);
				sb_str(&b, TR(ACTIVITY_BODY_LEFT_OF, " left of "));	// the time-out
				sb_mmss(&b, v->tot_s);
			} else {
				sb_str(&b, TR(ACTIVITY_BODY_NO_TIME_LIMIT, "No time limit"));
			}
			gfx_text(sm, 0, y2, buf, ui_col.dim);
		}
		break;
	}
	case RS_TX_DENIED:
		// One line only: the pill already says NO TX, so the line gives the reason
		gfx_text_fit(sm, ui_deny_text(v, v->tx_deny, buf, sizeof buf), w, fit, sizeof fit);
		if (two) {
			gfx_text(big, 0, y, TR(ACTIVITY_STATE_CANT_TRANSMIT, "Can't transmit"), ui_col.warn);
			gfx_text(sm, 0, y2, fit, ui_col.fg);
		} else {
			gfx_text(sm, 0, y, fit, ui_col.warn);
		}
		break;
	default:
		if (board_hwopt_pending() != NULL) {
			// Something only a person can tell the radio, and transmit waits for it: said where the eye
			// goes, before the last call heard
			hwopt_needed(&b);
			gfx_text_fit(sm, buf, w, fit, sizeof fit);
			bool two_sm = L->activity_h >= 2 * sm->h;
			y += (L->activity_h - (two_sm ? 2 : 1) * sm->h) / 2;
			gfx_text(sm, 0, y, fit, ui_col.warn);
			if (two_sm) {
				gfx_text_fit(sm, TR(ACTIVITY_BODY_TX_WAITS, "Transmit waits for it"), w, fit,
					sizeof fit);
				gfx_text(sm, 0, y + sm->h, fit, ui_col.fg);
			}
			break;
		}
#if CONFIG_DMR_TRUNK
		if (v->t3.on && v->t3.state == T3_HUNT && (v->t3.other != 0 || v->t3.other_fid != 0)) {
			draw_t3_other(v, y);
			break;
		}
#endif
#if CONFIG_DMR
		if (v->ch.mode == RADIO_MODE_DMR && v->dmr.active) {
			draw_last_heard(v, s, y);	// hear all shows as "All" in the chips and in the hint
			break;
		}
#endif
#if CONFIG_APRS_RX
		{
			aprs_view_t aprs_v;
			if (ui_svc_aprs_view(&aprs_v) && aprs_v.last_msg_ms != 0 &&
			    (v->last_rx_ms == 0 || aprs_v.last_msg_ms + 2000u >= v->last_rx_ms)) {
				sb_str(&b, TR(APRS_ACTIVITY_LAST_MSG, "APRS: "));
				sb_str(&b, aprs_v.last_msg_from);
				sb_str(&b, " ");
				sb_age(&b, s->now_ms - aprs_v.last_msg_ms);
			} else if (v->last_rx_ms) {
				sb_str(&b, TR(ACTIVITY_BODY_LAST, "Last activity "));
				sb_age(&b, s->now_ms - v->last_rx_ms);
			} else {
				sb_str(&b, TR(ACTIVITY_EMPTY, "No activity yet"));
			}
		}
#else
		if (v->last_rx_ms) {
			sb_str(&b, TR(ACTIVITY_BODY_LAST, "Last activity "));
			sb_age(&b, s->now_ms - v->last_rx_ms);
		} else {
			sb_str(&b, TR(ACTIVITY_EMPTY, "No activity yet"));
		}
#endif
		bool mon = v->monitor && L->activity_h >= 2 * sm->h;
		y += (L->activity_h - (mon ? 2 : 1) * sm->h) / 2;	// centered in the activity area
		gfx_text_fit(sm, buf, gfx_width(), fit, sizeof fit);
		gfx_text(sm, 0, y, fit, ui_col.dim);
		if (mon)
			gfx_text(sm, 0, y + sm->h, TR(ACTIVITY_STATE_SQUELCH_OPEN, "Squelch open"), ui_col.warn);
		break;
	}
}

static const char *home_hint(const radio_view_t *v, const ui_state_t *s)
{
	if (v->state == RS_TX_DENIED)
		return deny_action(v);
	if (s->entry)
		return TR(FREQ_HINT_ENTRY, "OK tune  Back delete");
	if (s->locked)
		return TR(LOCK_HINT_HOLD_STAR, TEXT_ALT_OPEN "Keys locked: hold * to unlock"
			TEXT_ALT_NEXT "Locked: hold * to unlock" TEXT_ALT_CLOSE);
	if (s->scanning)
		return TR(HOME_HINT_SCANNING, "# skip  Back stop scan");
	if (board_hwopt_pending() != NULL)
		return hwopt_action();
#if CONFIG_DMR
	if (v->ch.mode == RADIO_MODE_DMR) {
		if (v->dmr.reply_active)
			return TR(HOME_HINT_REPLY, "# caller  Back end reply");
		if (!v->dmr.codec)
			return TR(WHY_NO_CODEC, "No codec: voice off");
		if (v->dmr.hear_all)
			return TR(HOME_HINT_DMON, "Digital monitor  Back off");
	}
#endif
	if (v->temporary)
		return TR(HOME_HINT_UNSAVED, "Back undo  Hold OK save");
#if CONFIG_DMR
	if (v->ch.mode == RADIO_MODE_DMR && v->dmr.target_manual)
		return v->vfo ?
			TR(HOME_HINT_TALK_TARGET_VFO, TEXT_ALT_OPEN "Back: VFO's talk target"
				TEXT_ALT_NEXT "Back default target" TEXT_ALT_CLOSE) :
			TR(HOME_HINT_TALK_TARGET_CHANNEL, TEXT_ALT_OPEN "Back: channel's talkgroup"
				TEXT_ALT_NEXT "Back default target" TEXT_ALT_CLOSE);
#endif
	if (v->vfo)
		return TR(HOME_HINT_VFO, "0-9 freq  * Quick  Up/Dn");
	return s->home_hint != NULL ? s->home_hint : TR(HOME_HINT_MENU_QUICK, "OK Menu  * Quick");
}

// The largest font no taller than room in which text fits the width (at least the medium font), trying each of the
// text's alternates (font.h) from the longest; out gets the form chosen
static const font_t *large_font(const char *text, int room, char *out, size_t cap)
{
	const ui_layout_t *L = &ui_layout;
	const font_t *fonts[] = { font_largest, L->large, L->medium };
	bool more = true;
	for (int level = 0; more; level++) {
		gfx_text_alt(text, level, out, cap, &more);
		for (unsigned i = 0; i < sizeof fonts / sizeof fonts[0]; i++)
			if (fonts[i]->h <= room && gfx_text_width(fonts[i], out) <= gfx_width())
				return fonts[i];
	}
	return L->medium;	// out: the shortest form, shortened when drawn
}

// Large-text Home (accessibility): the status strip, the channel and the talk target, each as large as fits
static void draw_home_large(const radio_view_t *v, const ui_state_t *s)
{
	const ui_layout_t *L = &ui_layout;
	int w = gfx_width();
	char buf[48], fit[48];
	sbuf_t b;
	int y = L->strip_h + 2;
	int room = (L->hint_y - y) / 2;
	char form[48];
	sb_init(&b, buf, sizeof buf);
	bool tx = v->state == RS_TX && v->vfo;
	if (v->vfo)
		sb_freq(&b, tx ? v->ch.tx_hz : v->ch.rx_hz);
	else
		sb_str(&b, v->ch.name);
	const font_t *f = large_font(buf, room, form, sizeof form);
	gfx_text_fit(f, form, w, fit, sizeof fit);
	gfx_text(f, 0, y, fit, tx ? ui_col.tx : ui_col.fg);
	y += room;
	sb_init(&b, buf, sizeof buf);
#if CONFIG_DMR
	if (v->ch.mode == RADIO_MODE_DMR && v->dmr.active) {
		// The target with its name, or its short form when the whole doesn't fit even the medium font
		bool named = v->dmr.target_name[0] != '\0' && v->dmr.target_id != 0xFFFFFFu;
		if (named)
			sb_str(&b, TEXT_ALT_OPEN);
		sb_str(&b, GLYPH_ARROW);
		sb_target(&b, v->dmr.target_id, v->dmr.target_private, v->dmr.target_name);
		if (named) {
			sb_str(&b, TEXT_ALT_NEXT GLYPH_ARROW);
			sb_target_short(&b, v->dmr.target_id, v->dmr.target_private, v->dmr.target_name);
			sb_str(&b, TEXT_ALT_CLOSE);
		}
	} else
#endif
	{
		sb_str(&b, mode_word(v));
		sb_chr(&b, ' ');
		sb_str(&b, ui_power_name(v->power, v->ch.tx_hz));
	}
	f = large_font(buf, room, form, sizeof form);
	gfx_text_fit(f, form, w, fit, sizeof fit);
	gfx_text(f, 0, y, fit, v->state == RS_TX ? ui_col.tx : v->state == RS_RX ? ui_col.rx : ui_col.fg);
}

#if CONFIG_UI_PROFILES
static void fit_end(const font_t *f, const char *s, int max_w, char *out, size_t len);

static void profile_center(const font_t *f, int y, const char *text, color_t c)
{
	char fit[64];
	int w = gfx_width();
	fit_end(f, text, w - 30, fit, sizeof fit);
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
		if (v->state == RS_TX && d->call.dst != 0 && !v->aprs_tx && !sending_data(v)) {
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
		home_entry_text(s, out, cap);
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
		if (!v->aprs_tx && !sending_data(v) && v->tot_s) {
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
		hwopt_needed(&b);
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
	if (!s->gui_profiles)
		return ui_draw_top(v, s, NULL, NULL);
	int strip_h = s->gui_profile ? ui_layout.strip_h : PROFILE_COMMERCIAL_STATUS_H;
	int operating_top = strip_h + PROFILE_HOME_GAP;
	ui_strip_draws++;
	profile_home_devices(v, s, strip_h);
	profile_pane(operating_top, PROFILE_HOME_OPERATING_H, profile_row_bg);
	profile_home_operating_marks(v, s, operating_top);
	return operating_top + PROFILE_HOME_OPERATING_H + PROFILE_HOME_GAP;
}

static void banner_box(const ui_state_t *s, int bottom, int x0, int right_inset, color_t bg);

static void profile_home_footer(const ui_state_t *s, const char *hint)
{
	// Above the footer pane; what it reads of the screen under it stays clear of the panes' rounded corners
	banner_box(s, profile_footer_top() - PROFILE_HOME_GAP, PROFILE_HOME_INSET + 5, PROFILE_HOME_INSET + 5,
		ui_col.strip_bg);
	draw_bottom(s, hint, home_soft_key(s));
}

static void draw_profile_home(const radio_view_t *v, const ui_state_t *s)
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
	profile_home_footer(s, home_hint(v, s));
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
#endif

static void ref_home(const radio_view_t *v, const ui_state_t *s)
{
#if CONFIG_UI_PROFILES
	if (s->gui_profiles && !profile_operational_home(v)) {
		draw_profile_home(v, s);
		return;
	}
#endif
#if CONFIG_DMR
	ui_st_dmr_off = s->dmr_off;
	if (dmr_call_screen(v)) {
		draw_dmr_call(v, s);
		ui_draw_banner(s);
		if (s->toast != NULL)
			ui_draw_hint(s->toast, true);
		return;
	}
#endif
	gfx_clear(ui_col.bg);
	ui_draw_top(v, s, NULL, NULL);
	if (s->large_text) {
		draw_home_large(v, s);
	} else {
		draw_place(v, s);
		draw_talk(v, s->label_first);
		draw_chips(v, s);
		if (ui_layout.sep_y >= 0)
			gfx_hline(0, ui_layout.sep_y, gfx_width(), ui_col.line);
		draw_activity(v, s);
	}
	ui_draw_banner(s);
	draw_bottom(s, home_hint(v, s), home_soft_key(s));
}

void ui_draw_home(const radio_view_t *v, const ui_state_t *s)
{
#if CONFIG_DUAL_WATCH
	if (s->dual.on) {
		ui_draw_home_dual(v, s);
		return;
	}
#endif
	UI_THEME_OP(home)(v, s);
}

#if CONFIG_DUAL_WATCH
// --- Dual watch Home (docs/ux/dual-watch.md 3) ---
// Two panes, A over B, the same size: neither channel is second class. Each pane has a letter tag, the channel's
// number and name (the VFO: its frequency), then what PTT talks to there (DMR) or its frequency (FM), and its
// frequency and chips where there is room. The selected pane's tag is filled and says PTT; the pane a call is coming
// in on is green and says RX, with the caller; the pane the last call came in on says how long ago. Every color has
// its word (P5)

typedef struct {
	const font_t *name, *target, *detail;	// detail NULL: no third row
	bool zone;				// a fourth row for the zone (else it shares the third when it fits)
	int act;				// activity lines under the panes
} dual_fit_t;

static const font_t *dual_nf;	// the channel names' font: one for both panes, the largest both fit

static int dual_pane_h(const dual_fit_t *ft)
{
	int h = ft->name->h + ft->target->h;
	if (ft->detail != NULL)
		h += ft->detail->h * (ft->zone ? 2 : 1);
	return h;
}

// Text right-aligned at x_end on a row of font rf that starts at y with height row_h, whole (or one of its
// alternates) or not at all: returns its left edge, or x_end when it doesn't fit right of min_x
static int dual_right(const font_t *rf, int x_end, int y, int row_h, int min_x, const char *text, color_t c)
{
	char fit[48];
	int room = x_end - min_x;
	bool more = true;
	int tw = 0;
	for (int level = 0; more; level++) {	// the longest alternate that fits
		gfx_text_alt(text, level, fit, sizeof fit, &more);
		tw = gfx_text_width(rf, fit);	// pixels: a CJK character is not one cell wide
		if (tw <= room)
			break;
	}
	if (tw > room || fit[0] == 0)
		return x_end;
	gfx_text(rf, x_end - tw, y + (row_h - rf->h + 1) / 2, fit, c);
	return x_end - tw;
}

// Where a pane's text starts: right of its tag, which is sized from the layout's name font
static int dual_tag_w(const font_t *nf) { return nf->w + (nf->w >= 12 ? 10 : nf->w >= 8 ? 6 : 4); }
static int dual_x0(const font_t *nf) { return dual_tag_w(nf) + (ui_layout.small->w + 1) / 2; }
static int dual_pill_w(const char *word)
{
	const font_t *sm = ui_layout.small;
	return gfx_text_width(sm, word) + 2 * (sm->w >= 8 ? 4 : 3);
}
// The channel number's font next to a name in f: medium beside the large font, else the name's
static const font_t *dual_num_font(const font_t *f) { return f == ui_layout.large ? ui_layout.medium : f; }

// The pane's first-row text: the channel name, the VFO's frequency, or the selected VFO's typed digits
static void dual_name(const ui_state_t *s, int i, char *buf, size_t len)
{
	const ui_dual_pane_t *p = &s->dual.pane[i];
	if (p->vfo && s->entry && s->dual.sel == i) {
		home_entry_text(s, buf, len);
		return;
	}
	sbuf_t b;
	sb_init(&b, buf, len);
	if (p->vfo)
		sb_freq(&b, p->rx_hz);
	else
		sb_str(&b, p->name);
}

// The largest font up to the layout's name font in which pane i's name fits whole next to its number and the
// widest pill (PTT)
static const font_t *dual_name_font(const ui_state_t *s, int i, const font_t *nf)
{
	const ui_layout_t *L = &ui_layout;
	const ui_dual_pane_t *p = &s->dual.pane[i];
	const font_t *order[] = { L->large, L->medium, L->small };
	char buf[40], num[8];
	sbuf_t nb;
	sb_init(&nb, num, sizeof num);
	sb_u32(&nb, p->number, 1);
	dual_name(s, i, buf, sizeof buf);
	int room = gfx_width() - dual_x0(nf) - dual_pill_w("PTT") - L->small->w / 2 - 1;
	for (unsigned k = 0; k < 3; k++) {
		const font_t *f = order[k];
		if (f->h > nf->h)
			continue;
		const font_t *mf = dual_num_font(f);
		int need = gfx_text_width(f, buf) + (p->vfo ? 0 : gfx_text_width(mf, num) + mf->w / 2);
		if (need <= room || f == L->small)
			return f;
	}
	return L->small;
}

// Row 3's text: a DMR channel's frequency then its chips, else the chips. At their widest (440.6125, CC12) they
// overflow the narrowest row they share (240 px less a bezel inset, the big font), so the shorter forms give up the
// gap, then the unit, never a digit: the end was cut there before ("CC1", #837)
static char dual_r3[80];	// its three forms: static, the ui task's stack is tight

static void dual_row3(const ui_dual_pane_t *p, char *buf, size_t len)
{
	sbuf_t b;
	sb_init(&b, buf, len);
	if (p->mode != RADIO_MODE_DMR || p->vfo) {
		sb_str(&b, p->detail);
		return;
	}
	static const char *const sep[] = { " MHz  ", " MHz ", " " };
	sb_str(&b, TEXT_ALT_OPEN);
	for (unsigned k = 0; k < sizeof sep / sizeof sep[0]; k++) {
		if (k)
			sb_str(&b, TEXT_ALT_NEXT);
		sb_freq(&b, p->rx_hz);
		sb_str(&b, sep[k]);
		sb_str(&b, p->detail);
	}
	sb_str(&b, TEXT_ALT_CLOSE);
}

// A right-pointing triangle ah high (odd) and aw wide at x, y: the selected channel's mark
static void dual_arrow(int x, int y, int ah, int aw, color_t c)
{
	int mid = ah / 2;
	for (int r = 0; r < ah; r++) {
		int d = r < mid ? mid - r : r - mid;
		int len = mid ? (aw * (mid - d) + mid - 1) / mid : aw;
		gfx_hline(x, y + r, len > 0 ? len : 1, c);
	}
}

static int dual_home_inset(const ui_state_t *s)
{
#if CONFIG_UI_PROFILES
	if (s->gui_profiles)
		return 12;
#else
	(void)s;
#endif
	return 0;
}

static void dual_pane(const radio_view_t *v, const ui_state_t *s, int i, int y, int h, int pad, const dual_fit_t *ft)
{
	const ui_layout_t *L = &ui_layout;
	const ui_dual_t *d = &s->dual;
	const ui_dual_pane_t *p = &d->pane[i];
	const font_t *nf = ft->name, *sm = L->small;
	int inset = dual_home_inset(s), w = gfx_width() - inset;
	bool tx = v->state == RS_TX && d->sel == i, rx = !tx && p->rx, sel = d->sel == i;
	bool entry = s->entry && sel && p->vfo;
	bool commercial = false;
#if CONFIG_UI_PROFILES
	commercial = s->gui_profiles && s->gui_profile == 0;
#endif
	bool quiet = !rx && d->pane[!i].rx;	// the other pane has a call: this one steps back
	color_t fg = quiet ? ui_col.dim : ui_col.fg;
	char buf[48], fit[48];
	sbuf_t b;

	// The tag: the pane's letter in a box as tall as the pane's text. Filled for the selected pane (red while it
	// transmits), green while a call comes in on it, an outline otherwise (green while it is busy: p->busy)
	int ty = y + pad, th = h - 2 * pad;
	int tw = dual_tag_w(nf);
	const char *letter = i ? "B" : "A";
	color_t tbg = ui_col.bg, tfg = ui_col.dim;
	if (tx) {
		tbg = ui_col.tx;
		tfg = ui_col.pill_fg;
	} else if (rx) {
		tbg = ui_col.rx;
		tfg = ui_col.pill_dark_fg;
	} else if (sel) {
		tbg = ui_col.fg;
		tfg = ui_col.bg;
	}
	bool busy = p->busy && !rx && !tx;
	if (tbg != ui_col.bg)
		gfx_round_fill(inset, ty, tw, th, tbg);
	else
		gfx_rect(inset, ty, tw, th, busy ? ui_col.rx : ui_col.line);
	// The letter, and under it on the selected pane an arrow pointing at its channel (none on the other). Both
	// letters sit at the same height; a font smaller where the tag is too short for the letter and the arrow
	int ah = nf->h / 3 < 5 ? 5 : nf->h / 3 | 1;		// the arrow's height, odd
	const font_t *lf = nf;
	if (lf->h + 2 + ah > th)
		lf = nf == L->large ? L->medium : L->small;
	if (lf->h + 2 + ah > th)
		lf = L->small;
	if (lf->h + 1 + ah > th)
		ah = 0;					// no room even so: the filled tag says it alone
	else if (ah < lf->h * 2 / 3 && lf->h + 2 + lf->h * 2 / 3 <= th)
		ah = lf->h * 2 / 3 | 1;			// tall tags: an arrow in proportion to the letter
	int ly = ty + (th - lf->h - (ah ? ah + 2 : 0) + 1) / 2;
	gfx_text(lf, inset + (tw - gfx_text_width(lf, letter) + 1) / 2, ly, letter, tfg);
	if (sel && ah) {
		dual_arrow(inset + (tw - (ah + 1) / 2) / 2, ly + lf->h + 2, ah, (ah + 1) / 2, tfg);
	}

	int x0 = inset + dual_x0(nf);
	int rx_end = w;

	// Row 1, right: PTT on the selected pane, always (TX while transmitting), so where PTT goes stays plain with
	// calls on both; RX is on row 2. A name that doesn't fit whole beside it keeps the row: the pill goes to the
	// right of row 3, where there is one
	int y1 = ty, pill_h = sm->h;
	const font_t *f = dual_nf != NULL ? dual_nf : nf;
	const char *word = tx ? "TX" : "PTT";
	bool pill3 = false;
	if ((tx || sel) && ft->detail != NULL) {
		dual_name(s, i, buf, sizeof buf);
		int need = gfx_text_width(f, buf);
		if (!p->vfo) {
			sb_init(&b, fit, sizeof fit);
			sb_u32(&b, p->number, 1);
			need += gfx_text_width(dual_num_font(f), fit) + dual_num_font(f)->w / 2;
		}
		// Row 3 has room for it after the frequency and chips
		dual_row3(p, dual_r3, sizeof dual_r3);
		int room3 = w - x0 - gfx_text_width(ft->detail, dual_r3) - 2 * ft->detail->w;
		pill3 = x0 + need > w - dual_pill_w(word) - sm->w / 2 - 1 && room3 >= dual_pill_w(word);
	}
	if ((tx || sel) && !pill3) {
		rx_end -= dual_pill_w(word);
		gfx_pill(sm, rx_end, y1 + (nf->h - pill_h + 1) / 2, pill_h, word, tx ? ui_col.pill_fg : ui_col.bg,
			 tx ? ui_col.tx : ui_col.fg);
		rx_end -= sm->w / 2 + 1;
	}

	// Row 1, left: the channel number (medium, the channel number's color) and name in the names' font; the VFO: its
	// frequency
	int base = y1 + nf->ascent;
	int x = x0;
	if (!p->vfo && !commercial) {
		char num[8];
		sbuf_t nb;
		sb_init(&nb, num, sizeof num);
		sb_u32(&nb, p->number, 1);
		const font_t *mf = dual_num_font(f);
		x = gfx_text(mf, x, base - mf->ascent, num, quiet || gfx_bpp() == 1 ? ui_col.dim : ui_col.chnum) +
			mf->w / 2;
	}
	dual_name(s, i, buf, sizeof buf);
	if (tx && p->vfo && !entry) {	// the VFO transmitting: its TX frequency (the view's channel is the selected pane's)
		sb_init(&b, buf, sizeof buf);
		sb_freq(&b, v->ch.tx_hz);
	}
	gfx_text_fit(f, buf, rx_end - x, fit, sizeof fit);
	x = gfx_text(f, x, base - f->ascent, fit, entry ? ui_col.warn : tx && p->vfo ? ui_col.tx : rx ? ui_col.rx : fg);
	if (p->vfo && x + sm->w / 2 + gfx_text_width(sm, "MHz") <= rx_end) {
		gfx_text(sm, x + sm->w / 2, base - sm->ascent, "MHz", ui_col.dim);
		x += sm->w / 2 + gfx_text_width(sm, "MHz");
	}
	// Then how long ago a call last came in on this pane, where it fits whole: both panes, the newest call's in amber
	bool aged = false;
	uint32_t ago = s->now_ms - d->ended_ms[i];
	color_t agec = d->last == i ? ui_col.warn : ui_col.dim;
	if (d->ended[i] && !rx && !tx && !busy) {
		sb_init(&b, buf, sizeof buf);
		sb_str(&b, TEXT_ALT_OPEN);
		sb_str(&b, TR(DUAL_BODY_HEARD, "Heard "));
		sb_age_long(&b, ago);
		sb_str(&b, TEXT_ALT_NEXT);
		sb_age_long(&b, ago);
		sb_str(&b, TEXT_ALT_CLOSE);
		aged = dual_right(sm, rx_end, y1, nf->h, x + sm->w, buf, agec) < rx_end;
	}

	// Row 2: the caller while a call comes in here; else what PTT talks to (DMR) or the frequency (FM). Right: the
	// frequency (DMR) or the chips (FM) when there is no third row, the signal while receiving, Busy while the watch
	// waits for a reply on the other pane and a look found a signal here
	const font_t *tf = ft->target;
	int y2 = y1 + nf->h;
	char right[32];
	sbuf_t rb;
	sb_init(&rb, right, sizeof right);
	sb_init(&b, buf, sizeof buf);
	color_t c2 = fg;
	if (rx) {
		const char *caller = p->caller;
		if (commercial) {
			caller = "";
#if CONFIG_DMR
			if (d->rx == i && v->ch.mode == RADIO_MODE_DMR && v->dmr.state == RDMR_RX && s->alias_show)
				caller = v->dmr.alias;
#endif
		}
		sb_str(&b, caller[0] ? caller : TR(STATE_RECEIVING, "Receiving"));
		c2 = ui_col.rx;
#if CONFIG_UI_PROFILES
	} else if (commercial) {
		sb_str(&b, p->target_label[0] ? p->target_label : p->zone);
#endif
	} else if (p->mode == RADIO_MODE_DMR) {
		sb_str(&b, p->target[0] ? p->target : "DMR");
		if (ft->detail == NULL && !p->vfo)
			sb_freq(&rb, p->rx_hz);
		else if (ft->detail == NULL)
			sb_str(&rb, p->detail);
	} else if (p->vfo) {
		sb_str(&b, "VFO ");
		sb_str(&b, dual_mode_word(p));
		if (ft->detail == NULL)
			sb_str(&rb, p->detail);
	} else {
		sb_freq(&b, p->rx_hz);
		sb_str(&b, " MHz");
		if (ft->detail == NULL)
			sb_str(&rb, p->detail);
	}
	// No list of calls under the panes (the smallest displays): the last call's pane says how long ago instead
	color_t rc = rx ? ui_col.fg : ui_col.dim;
	if (ft->act == 0 && d->last == i && !rx && !tx && !aged) {
		sb_init(&rb, right, sizeof right);
		sb_str(&rb, TEXT_ALT_OPEN);
		sb_str(&rb, TR(DUAL_BODY_HEARD, "Heard "));
		sb_age_long(&rb, ago);
		sb_str(&rb, TEXT_ALT_NEXT);
		sb_age_long(&rb, ago);
		sb_str(&rb, TEXT_ALT_CLOSE);
		rc = ui_col.warn;
	}
	if (busy) {
		sb_init(&rb, right, sizeof right);
		sb_str(&rb, TR(DUAL_STATE_BUSY, "Busy"));
		rc = ui_col.rx;
	}
	int r2 = w;
	if (rx) {	// the RX pill at the right of the caller
		r2 -= dual_pill_w("RX");
		gfx_pill(sm, r2, y2 + (tf->h - pill_h + 1) / 2, pill_h, "RX", ui_col.pill_dark_fg, ui_col.rx);
		r2 -= sm->w / 2 + 1;
	} else if (right[0]) {
		int need = gfx_text_width(tf, buf) + sm->w;
		int nx = dual_right(sm, w, y2, tf->h, x0 + need, right, rc);
		if (nx < w)
			r2 = nx - sm->w;
	}
	gfx_text_fit(tf, buf, r2 - x0, fit, sizeof fit);
	gfx_text(tf, x0, y2, fit, c2);

	// Row 3: the frequency and the chips, the zone at the right (B may be in another zone than A)
	if (ft->detail != NULL) {
		const font_t *df = ft->detail;
		int y3 = y2 + tf->h;
		int r3 = w;
		if (pill3) {
			r3 -= dual_pill_w(word);
			gfx_pill(sm, r3, y3 + (df->h - pill_h + 1) / 2, pill_h, word, tx ? ui_col.pill_fg : ui_col.bg,
				 tx ? ui_col.tx : ui_col.fg);
			r3 -= sm->w / 2 + 1;
		}
		dual_row3(p, dual_r3, sizeof dual_r3);
		gfx_text_fit(df, dual_r3, r3 - x0, buf, sizeof buf);
		int lx = gfx_text(df, x0, y3, buf, ui_col.dim);
		if (ft->zone)
			gfx_text(df, x0, y3 + df->h, p->zone, ui_col.dim);
		else
			dual_right(df, r3, y3, df->h, lx + 2 * df->w, p->zone, ui_col.dim);
	}
}

// Large text (Settings > Display): the two channels only, each as large as fits. The selected one has the arrow;
// the one a call comes in on is green and says RX (a word, for mono displays)
static void dual_large(const radio_view_t *v, const ui_state_t *s, int top, int bottom)
{
	const ui_layout_t *L = &ui_layout;
	const ui_dual_t *d = &s->dual;
	int inset = dual_home_inset(s), w = gfx_width() - 2 * inset;
	int y = top + 2;
	int room = (bottom - y) / 2;
	for (int i = 0; i < 2; i++) {
		const ui_dual_pane_t *p = &d->pane[i];
		char buf[48], form[48], fit[48];
		sbuf_t b;
		sb_init(&b, buf, sizeof buf);
		sb_str(&b, i ? "B " : "A ");
		if (p->vfo) {
			char name[24];
			dual_name(s, i, name, sizeof name);
			sb_str(&b, name);
		} else
			sb_str(&b, p->name);
		const font_t *f = large_font(buf, room, form, sizeof form);
		if (gfx_text_width(f, form) + (d->sel == i ? f->w / 3 + 2 : 0) > w) {
			const font_t *smaller[] = { L->large, L->medium, L->small };
			for (unsigned k = 0; k < sizeof smaller / sizeof smaller[0]; k++) {
				f = smaller[k];
				if (f->h <= room && gfx_text_width(f, form) + (d->sel == i ? f->w / 3 + 2 : 0) <= w)
					break;
			}
		}
		bool tx = v->state == RS_TX && d->sel == i, rx = !tx && p->rx;
		color_t c = s->entry && d->sel == i && p->vfo ? ui_col.warn :
			    tx ? ui_col.tx : rx ? ui_col.rx : d->sel == i ? ui_col.fg : ui_col.dim;
		// The arrow in the room a selection bar would take: f->w / 3 wide, half the letter high, a 2 px gap; the
		// other line starts at 0
		int aw = d->sel == i ? f->w / 3 + 2 : 0, ah = (f->h / 2) | 1;
		if (aw)
			dual_arrow(inset, y + (f->h - ah) / 2, ah, aw - 2, c);
		gfx_text_fit(f, form, w - aw, fit, sizeof fit);
		if (rx || tx) {	// receiving or transmitting: the letter in a filled box, as the tag on Home (the strip has the word)
			char letter[2] = { fit[0], 0 };
			gfx_fill(inset + aw, y, f->w, f->h, c);
			gfx_text(f, inset + aw, y, letter, gfx_bpp() == 1 ? ui_col.bg : ui_col.pill_dark_fg);
			gfx_text(f, inset + aw + f->w, y, fit + 1, c);
		} else {
			gfx_text(f, inset + aw, y, fit, c);
		}
		y += room;
	}
}

// One call of the list: its pane's letter in an outline tag, the caller, how long ago at the right
static void dual_call(int y, uint8_t pane, const char *caller, uint32_t age_ms, color_t c)
{
	const font_t *sm = ui_layout.small;
	int w = gfx_width();
	char age[16], fit[48];
	sbuf_t ab;
	int tw = sm->w + 4;
	gfx_rect(0, y, tw, sm->h, ui_col.line);
	gfx_text(sm, 2, y, pane ? "B" : "A", c);
	sb_init(&ab, age, sizeof age);
	sb_age(&ab, age_ms);
	int ax = dual_right(sm, w, y, sm->h, tw + 8 * sm->w, age, ui_col.dim);
	gfx_text_fit(sm, caller, ax - sm->w - tw - sm->w / 2, fit, sizeof fit);
	gfx_text(sm, tw + sm->w / 2, y, fit, c);
}

// The lines under the panes: what is happening, then the last calls, each with its pane
static void dual_activity(const radio_view_t *v, const ui_state_t *s, int y, int lines)
{
	const ui_layout_t *L = &ui_layout;
	const ui_dual_t *d = &s->dual;
	const font_t *sm = L->small;
	int inset = dual_home_inset(s), w = gfx_width() - inset;
	char buf[64];
	sbuf_t b;
	if (lines <= 0)
		return;
#if CONFIG_UI_PROFILES
	if (s->gui_profiles && v->state == RS_TX_DENIED) {
		profile_center(sm, y, ui_deny_text(v, v->tx_deny, buf, sizeof buf), ui_col.warn);
		return;
	}
#endif
	// A transmission or a call coming in takes the first line
	sb_init(&b, buf, sizeof buf);
	if (v->state == RS_TX) {
		if (v->aprs_tx)
			sb_str(&b, TR(STATE_SENDING, "SENDING"));
		else {
			sb_str(&b, sending_data(v) ? TR(DUAL_STATE_SENDING_ON, "SENDING on ") :
				TR(DUAL_STATE_TALKING_ON, "TALKING on "));
			sb_str(&b, d->sel ? "B" : "A");
		}
		gfx_text(sm, inset, y, buf, ui_col.tx);
		if (!v->aprs_tx) {
			sb_init(&b, buf, sizeof buf);
			sb_mmss(&b, v->tx_elapsed_ms / 1000u);
			gfx_text(sm, w - gfx_text_width(sm, buf), y, buf, ui_col.fg);
		}
		y += sm->h;
		lines--;
	} else if (d->pane[0].rx && d->pane[1].rx && lines == 1) {
		// two receivers, a call on each
		gfx_text(sm, inset, y, TR(DUAL_STATE_RECEIVING_ON_BOTH, "Receiving on A and B"), ui_col.rx);
		y += sm->h;
		lines--;
	} else if (d->rx != UI_DUAL_NONE) {
		// A line per pane receiving, the one heard first; the other (two receivers) says it is muted
		for (int k = 0; k < 2 && lines > 0; k++) {
			uint8_t i = (uint8_t)(k ? !d->rx : d->rx);
			if (!d->pane[i].rx)
				continue;
			sb_init(&b, buf, sizeof buf);
			sb_str(&b, TR(DUAL_STATE_RECEIVING_ON, "Receiving on "));
			sb_str(&b, i ? "B" : "A");
			if (k)
				sb_str(&b, TR(DUAL_STATE_MUTED, ", muted"));
			int x = gfx_text(sm, inset, y, buf, ui_col.rx) + sm->w;
			if (w - x >= 4 * sm->w)
				draw_meter(x, y + sm->h / 4, w - x, sm->h / 2, d->pane[i].rssi_dbm);
			y += sm->h;
			lines--;
		}
	}
	// Then the calls heard, newest first, as many as fit
#if CONFIG_UI_PROFILES
	if (s->gui_profiles)
		return;	// the current alias is only a live receiving indication on the profile Homes
#endif
	if (lines > 0 && d->calls == 0)
		gfx_text(sm, 0, y, TR(EMPTY_NO_CALLS_HEARD, "No calls heard yet"), ui_col.dim);
	for (int k = 0; k < d->calls && k < lines; k++, y += sm->h)
		dual_call(y, d->hist[k].pane, d->hist[k].caller, s->now_ms - d->hist[k].ms, k ? ui_col.dim : ui_col.fg);
}

static const char *dual_hint(const radio_view_t *v, const ui_state_t *s)
{
	const ui_dual_t *d = &s->dual;
	if (v->state == RS_TX_DENIED)
		return deny_action(v);
	if (s->entry)
		return TR(FREQ_HINT_ENTRY, "OK tune  Back delete");
	if (s->locked)
		return TR(LOCK_HINT_HOLD_STAR, TEXT_ALT_OPEN "Keys locked: hold * to unlock"
			TEXT_ALT_NEXT "Locked: hold * to unlock" TEXT_ALT_CLOSE);
	// Talk back on the channel a call came in on: the way back now
	if (d->back_to != UI_DUAL_NONE)
		return d->back_to ?
			TR(DUAL_HINT_BACK_TO_B, TEXT_ALT_OPEN "# 1 back to B  Back stop"
				TEXT_ALT_NEXT "# 1 back to B" TEXT_ALT_CLOSE) :
			TR(DUAL_HINT_BACK_TO_A, TEXT_ALT_OPEN "# 1 back to A  Back stop"
				TEXT_ALT_NEXT "# 1 back to A" TEXT_ALT_CLOSE);
	// A call just came in on the other channel: the way to answer it there
	if (d->last != UI_DUAL_NONE && d->last != d->sel && s->now_ms - d->last_ms < 10000u)
		return d->last ?
			TR(DUAL_HINT_TALK_ON_B, TEXT_ALT_OPEN "# 1 talk on B  Back stop"
				TEXT_ALT_NEXT "# 1 talk on B" TEXT_ALT_CLOSE) :
			TR(DUAL_HINT_TALK_ON_A, TEXT_ALT_OPEN "# 1 talk on A  Back stop"
				TEXT_ALT_NEXT "# 1 talk on A" TEXT_ALT_CLOSE);
	return TR(DUAL_HINT_OPTIONS, "# options  Back stop");
}

// The richest layout that fits avail rows: names in the large font first, then fewer rows, never a smaller font for a
// row than the single-channel Home uses for it (P5). noinline: inlined, the table sits in the frame every pane is
// drawn from, on the UI task's deepest path
static void __attribute__((noinline)) dual_choose(int avail, int gap, bool commercial, dual_fit_t *out, int *pad)
{
	const ui_layout_t *L = &ui_layout;
	const font_t *lg = L->large, *md = L->medium, *sm = L->small;
	const dual_fit_t fits[] = {
		{ lg, md, sm, true, 2 }, { lg, md, sm, false, 2 }, { lg, md, sm, false, 1 },
		{ lg, md, NULL, false, 1 }, { lg, sm, NULL, false, 1 }, { md, sm, NULL, false, 1 },
		{ md, sm, NULL, false, 0 }, { sm, sm, NULL, false, 0 },
	};
	bool tall = gfx_height() >= UI_TALL_PX;
	const dual_fit_t *ft = &fits[sizeof fits / sizeof fits[0] - 1];
	int used = 0;
	for (unsigned k = 0; k < sizeof fits / sizeof fits[0]; k++) {
		if (commercial && fits[k].detail != NULL)
			continue;
		used = 2 * dual_pane_h(&fits[k]) + gap + fits[k].act * sm->h + (fits[k].act ? 2 : 0);
		if (used <= avail) {
			ft = &fits[k];
			break;
		}
	}
	int extra = avail - used;
	*pad = extra / 4 < (tall ? 5 : 1) ? extra / 4 : (tall ? 5 : 1);
	if (*pad < 0)
		*pad = 0;
	*out = *ft;
}

static void ref_home_dual(const radio_view_t *v, const ui_state_t *s)
{
	const ui_layout_t *L = &ui_layout;
	int bottom = L->hint_y;
#if CONFIG_UI_PROFILES
	gfx_clear(s->gui_profiles ? profile_home_gutter : ui_col.bg);
	int top = draw_home_status(v, s);
	if (s->gui_profiles) {
		bottom = profile_footer_top() - PROFILE_HOME_GAP;
		profile_pane(top, bottom - top, ui_col.bg);
		top += 4;
		bottom -= 4;
	}
#else
	gfx_clear(ui_col.bg);
	int top = ui_draw_top(v, s, NULL, NULL);
#endif
	if (s->large_text) {
		dual_large(v, s, top, bottom);
	} else {
		int gap = gfx_height() >= UI_TALL_PX ? 6 : 2, pad;
		dual_fit_t fit;
		bool commercial = false;
#if CONFIG_UI_PROFILES
		commercial = s->gui_profiles && s->gui_profile == 0;
#endif
		dual_choose(bottom - top, gap, commercial, &fit, &pad);
		const dual_fit_t *ft = &fit;
		const font_t *sm = L->small;
		int ph = dual_pane_h(ft) + 2 * pad;
		const font_t *f0 = dual_name_font(s, 0, ft->name), *f1 = dual_name_font(s, 1, ft->name);
		dual_nf = f0->h < f1->h ? f0 : f1;
		if (dual_nf->h < ft->target->h)
			dual_nf = ft->target;	// never smaller than the row under it: a long name is shortened instead
		dual_pane(v, s, 0, top, ph, pad, ft);
		int inset = dual_home_inset(s);
		gfx_hline(inset, top + ph + gap / 2, gfx_width() - 2 * inset, ui_col.line);
		dual_pane(v, s, 1, top + ph + gap, ph, pad, ft);
		int ay = top + 2 * ph + gap + 2;
		dual_activity(v, s, ay, ft->act ? (bottom - ay) / sm->h : 0);
	}
#if CONFIG_UI_PROFILES
	if (s->gui_profiles) {
		profile_home_footer(s, dual_hint(v, s));
		return;
	}
#endif
	ui_draw_banner(s);
	draw_bottom(s, dual_hint(v, s), home_soft_key(s));
}

void ui_draw_home_dual(const radio_view_t *v, const ui_state_t *s)
{
	UI_THEME_OP(home_dual)(v, s);
}
#endif

void ui_draw_bottom(const ui_state_t *s, const char *hint)
{
	draw_bottom(s, hint, NULL);
}

static void fit_end(const font_t *f, const char *s, int max_w, char *out, size_t len);

// Up to max lines of t wrapped to max_w: lines[] filled, the count returned. Text left after the last line shortens
// it (at the end for a preview, else from the middle), so nothing is silently dropped
static int wrap_lines(const font_t *f, const char *t, int max_w, int max, char lines[][64], bool preview)
{
	int n = 0;
	while (*t && n < max) {
		const char *next = wrap_line(f, t, max_w, lines[n], sizeof lines[0]);
		if (n == max - 1 && *next) {
			if (preview)
				fit_end(f, t, max_w, lines[n], sizeof lines[0]);
			else
				gfx_text_fit(f, t, max_w, lines[n], sizeof lines[0]);
		}
		t = next;
		n++;
	}
	return n;
}

// Banner: a framed box with the title, the text and how to act on it. Title and text take a second line each when
// they need it, so they are shown whole. The caller places it: bottom is its lower edge (the top of the bottom row), bg
// its background on a color display, and x0 and right_inset bound the columns of the screen under it that are read to
// find a gap between lines for its top edge (overlay_top), for a caller whose screen has something other than text at
// its sides
static void banner_box(const ui_state_t *s, int bottom, int x0, int right_inset, color_t bg)
{
	const ui_banner_t *bn = &s->banner;
	if (bn->kind == UI_BANNER_NONE)
		return;
	const ui_layout_t *L = &ui_layout;
	const font_t *f = L->small;
	int w = gfx_width();
	char title[2][64], text[2][64];
	int nt;
	if (gfx_text_width(f, bn->title) > w - 4 && bn->name_at > 0 && bn->name_at < sizeof title[0]) {
		// "Call alert from" / "Jodi Ruiz": the station's name together on the second line
		size_t n = bn->name_at;
		while (n > 0 && bn->title[n - 1] == ' ')
			n--;
		memcpy(title[0], bn->title, n);
		title[0][n] = 0;
		nt = 1 + wrap_lines(f, bn->title + bn->name_at, w - 4, 1, &title[1], false);
	} else {
		nt = wrap_lines(f, bn->title, w - 4, 2, title, false);
	}
	int nx = bn->text[0] ? wrap_lines(f, bn->text, w - 4, 2, text, bn->kind == UI_BANNER_MESSAGE) : 0;
	int pad = gfx_height() >= UI_TALL_PX ? 2 : 1;
	int h = (nt + nx + 1) * f->h + 2 * pad;
	int y = bottom - h;
	int top = overlay_top(y, x0, right_inset);	// the box grows up to a gap between lines; its text stays at the bottom
	color_t edge = bn->warn ? ui_col.warn : ui_col.focus;
	if (gfx_bpp() == 1) {
		// inverse video box (wireframe 128x64)
		gfx_fill(0, top, w, bottom - top, ui_col.fg);
	} else {
		gfx_fill(0, top, w, bottom - top, bg);
		gfx_rect(0, top, w, bottom - top, edge);
	}
	color_t fg = gfx_bpp() == 1 ? ui_col.bg : ui_col.fg;
	int ty = y + pad;
	for (int i = 0; i < nt; i++, ty += f->h)
		gfx_text(f, 2, ty, title[i], gfx_bpp() == 1 ? fg : bn->warn ? ui_col.warn : ui_col.fg);
	for (int i = 0; i < nx; i++, ty += f->h)
		gfx_text(f, 2, ty, text[i], fg);
	const char *act = (bn->kind == UI_BANNER_MESSAGE || bn->kind == UI_BANNER_ZONE_OFFER || bn->kind == UI_BANNER_APRS_MESSAGE) ?
		TR(BANNER_HINT_OPEN, "# Open") :
		(bn->kind == UI_BANNER_CALL_ALERT || bn->kind == UI_BANNER_PRIVATE) ?
			TR(BANNER_HINT_ACTIONS, "# Actions") : TR(BANNER_HINT_HIDE, "Back hide");
	gfx_text(f, 2, ty, act, gfx_bpp() == 1 ? fg : ui_col.dim);
	if (bn->hold)
		return;	// no countdown: it waits for the user
	char sec[8];
	sbuf_t b;
	sb_init(&b, sec, sizeof sec);
	uint32_t left = (int32_t)(bn->until_ms - s->now_ms) > 0 ? bn->until_ms - s->now_ms : 0;
	sb_u32(&b, (left + 999u) / 1000u, 1);
	sb_chr(&b, 's');
	gfx_text(f, w - 2 - gfx_text_width(f, sec), ty, sec, gfx_bpp() == 1 ? fg : ui_col.dim);
}

// The reference theme's banner: above the hint row, on its color
static void ref_banner(const ui_state_t *s)
{
	int x0 = 0;
	color_t bg = ui_col.hint_bg;
#if CONFIG_UI_PROFILES
	if (s->gui_profiles) {
		bg = ui_col.strip_bg;
#if CONFIG_DUAL_WATCH
		if (s->dual.on)
			x0 = dual_tag_w(ui_layout.large);	// the receiver tabs span multiple lines of text
#endif
	}
#endif
	banner_box(s, ui_layout.hint_y, x0, 0, bg);
}

void ui_draw_banner(const ui_state_t *s)
{
	UI_THEME_OP(banner)(s);
}

// --- List screens (Hub, Contacts, Find, Settings, ...): title, optional header and body, rows, hint ---

#if CONFIG_UI_PROFILES
#define PROFILE_GEAR_OUTLINE RGB565(0, 0, 0)
#define PROFILE_GEAR_FILL RGB565(148, 148, 148)

// The palette entries the profile look's rows and icons need, taken before anything else is drawn
static void profile_list_colors(const ui_list_t *l)
{
	const color_t row_colors[] = { profile_row_bg, profile_row_focus, ui_col.fg };
	const color_t icon_colors[] = { ui_col.priv, ui_col.rx, ui_col.warn, ui_col.tx,
		PROFILE_GEAR_FILL, PROFILE_GEAR_OUTLINE };
	gfx_reserve_colors(row_colors, sizeof row_colors / sizeof row_colors[0]);
	if (l->menu_icons)
		gfx_reserve_colors(icon_colors, sizeof icon_colors / sizeof icon_colors[0]);
}

// The profile look's list heading. noinline, as list_header below: its buffers stay out of the frame of
// ref_list_head, which also leads to the reference strip
static int __attribute__((noinline)) profile_list_head(const radio_view_t *v, const ui_state_t *s, const ui_list_t *l,
	bool draw)
{
	char title[64];
	sbuf_t b;
	sb_init(&b, title, sizeof title);
	if (l->prefix)
		sb_str(&b, l->prefix);
	if (l->prefix && l->name)
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
#endif

static int ref_list_head(const radio_view_t *v, const ui_state_t *s, const ui_list_t *l, bool draw)
{
#if CONFIG_UI_PROFILES
	if (draw && s->gui_profiles)
		profile_list_colors(l);
	if (l->roomy && (l->prefix || l->name))
		return profile_list_head(v, s, l, draw);
#endif
	return draw ? ui_draw_top(v, s, l->prefix, l->name) : ui_layout.strip_h +
		((l->prefix || l->name) && gfx_height() >= UI_TALL_PX ? ui_layout.small->h + 2 : 0);
}

// noinline: its buffer stays out of list_top's frame. The stack check follows list_top from a screen that only measures
// (ui_list_body_fits) on through the theme's head and the strip, one of the UI task's deepest paths, and the head is a
// frame of its own there
static void __attribute__((noinline)) list_header(const char *header, int y)
{
	char fit[48];
	gfx_text_fit(ui_layout.small, header, gfx_width(), fit, sizeof fit);
	gfx_text(ui_layout.small, 0, y, fit, ui_col.warn);
}

// Where a list's rows (or its body) start: below the theme's strip and title, and the header when there is one. draw
// false only measures (v and s may be NULL)
static int list_top(const radio_view_t *v, const ui_state_t *s, const ui_list_t *l, bool draw)
{
	int y = UI_THEME_OP(list_head)(v, s, l, draw);
	if (l->header != NULL) {
		if (draw)
			list_header(l->header, y);
		y += ui_layout.small->h;
	}
	return y;
}

static int list_bottom(const ui_state_t *s)
{
	return s == NULL || s->hints || s->toast ? ui_layout.hint_y : gfx_height();
}

static int list_pitch(const ui_list_t *l)
{
#if CONFIG_UI_PROFILES
	if (l->roomy)
		return ui_layout.row_h + 10;
#endif
	return ui_layout.row_h;
}

// One line of word-wrapped text: copies into line what fits in max_w pixels (breaking at the last space, or inside a
// word longer than the line, never shortening), returns where the next line starts
static const char *wrap_line(const font_t *f, const char *t, int max_w, char *line, size_t cap)
{
#if CONFIG_LANG_ZH
	// A character at a time: a CJK character may end a line as well as a space, and the width is measured on whole
	// characters, so a line never holds half of one
	int n = 0, cut = 0;
	while (t[n] && t[n] != '\n') {
		int seq;
		uint32_t cp = gfx_utf8_next(t + n, &seq);
		if (n + seq > (int)cap - 1)
			break;
		memcpy(line + n, t + n, (size_t)seq);
		n += seq;
		line[n] = 0;
		if (gfx_text_width(f, line) > max_w) {
			n -= seq;
			line[n] = 0;
			break;
		}
		if (t[n - seq] == ' ' || cp > 0xFF)
			cut = n;
	}
	bool fits_all = t[n] == 0 || t[n] == '\n';
	if (!fits_all && cut > 0)
		n = cut;
	if (n == 0 && t[0] && t[0] != '\n') {
		int seq;
		gfx_utf8_next(t, &seq);		// narrower than one character: still move on
		n = seq < (int)cap ? seq : (int)cap - 1;
	}
	line[n] = 0;
	t += n;
	if (*t == '\n')
		t++;		// an empty line ("\n\n") stays one line
	else
		while (*t == ' ')
			t++;
	return t;
#else
	int n = 0, cut = 0;
	while (t[n] && t[n] != '\n' && n < (int)cap - 1) {
		line[n] = t[n];
		line[n + 1] = 0;
		if (gfx_text_width(f, line) > max_w)
			break;
		if (t[n] == ' ')
			cut = n;
		n++;
	}
	bool fits_all = t[n] == 0 || t[n] == '\n';
	if (!fits_all && cut > 0)
		n = cut;
	if (n == 0 && t[0] && t[0] != '\n')
		n = 1;		// narrower than one glyph: still move on
	line[n] = 0;
	t += n;
	if (*t == '\n')
		t++;		// an empty line ("\n\n") stays one line
	else
		while (*t == ' ')
			t++;
	return t;
#endif
}

// Word wrap into lines of the screen's width; draws them (when draw) and returns the y after the last line
static int wrap_text(const font_t *f, const char *t, int y, int y_end, bool draw, color_t c)
{
	char line[64];
	while (*t && y + f->h <= y_end) {
		const char *next = wrap_line(f, t, gfx_width(), line, sizeof line);
		if (*next && y + 2 * f->h > y_end)
			fit_end(f, t, gfx_width(), line, sizeof line);	// the last line that fits: never drop text silently
		t = next;
		if (draw)
			gfx_text(f, 0, y, line, c);
		y += f->h;
	}
	return y;
}

// The length of a text line's "Label:" (colon included) when the line starts with one: a colon within its first 20
// characters followed by a space or the line's end. 0 when it has none
static size_t label_len(const char *t)
{
	for (size_t n = 0; n < 20 && t[n] != 0 && t[n] != '\n'; n++) {
		if (t[n] == ':')
			return t[n + 1] == ' ' || t[n + 1] == '\n' || t[n + 1] == 0 ? n + 1 : 0;
	}
	return 0;
}

int ui_draw_text_page(const radio_view_t *v, const ui_state_t *s, const char *title, const char *text,
	uint16_t *first, bool labels)
{
	const ui_layout_t *L = &ui_layout;
	const font_t *f = L->tiny;
	int w = gfx_width();
	char line[64];
	gfx_clear(ui_col.bg);
	// No status strip while idle: the page gets the whole height. A call or a transmission brings it back (P2: the
	// TX / RX pill is never hidden)
	int y = v->state != RS_IDLE ? ui_draw_top(v, s, NULL, title) : 0;
	int y_end = list_bottom(s);
	int rows = (y_end - y) / f->h;
	if (rows < 1)
		rows = 1;
	int total = 0;
	for (const char *t = text; *t; total++)
		t = wrap_line(f, t, w, line, sizeof line);
	int max_first = total > rows ? total - rows : 0;
	if (*first > max_first)
		*first = (uint16_t)max_first;
	const char *t = text;
	for (int i = 0; *t; i++) {
		// a label only where a line of the text starts, never on a wrapped value's next line
		bool starts = t == text || t[-1] == '\n';
		size_t lab = starts && labels ? label_len(t) : 0;
		t = wrap_line(f, t, w, line, sizeof line);
		int r = i - *first;
		if (r < 0)
			continue;
		if (r >= rows)
			break;
		// "Label: value" shows the label dim, and so does a line that is only "Label:" (a list follows). The label is
		// found in the text, not the wrapped line: a wrap right after the colon drops the space
		size_t len = strlen(line);
		color_t dim = gfx_bpp() == 1 ? ui_col.fg : ui_col.dim;
		if (lab > 0 && lab < len) {
			char label[24];
			memcpy(label, line, lab);
			label[lab] = 0;
			int x = gfx_text(f, 0, y + r * f->h, label, dim);
			gfx_text(f, x, y + r * f->h, line + lab, ui_col.fg);
		} else if (lab > 0 && lab == len) {
			gfx_text(f, 0, y + r * f->h, line, dim);
		} else {
			gfx_text(f, 0, y + r * f->h, line, ui_col.fg);
		}
	}
	if (*first > 0)
		gfx_fill(w - 3, y, 3, 2, ui_col.dim);
	if (*first < max_first)
		gfx_fill(w - 3, y + rows * f->h - 2, 3, 2, ui_col.dim);
	ui_draw_banner(s);
	ui_draw_bottom(s, max_first > 0 ? TR(PAGE_HINT_MORE, "Up/Dn more  Back") : TR(HINT_BACK, "Back"));
	return max_first;
}

// s shortened at the end, at a word if one ends late enough, with an ellipsis: for a preview of longer text (a
// message), where the start is what matters
static void fit_end(const font_t *f, const char *s, int max_w, char *out, size_t len)
{
#if CONFIG_LANG_ZH
	// Chinese has no spaces to break at, and a character is not one byte: a line with a wide character is cut by whole
	// characters at the end; Latin text keeps the word cut below
	gfx_text_fit_end(f, s, max_w, out, len);
#else
	size_t n = strlen(s), fit = max_w <= 0 ? 0 : (size_t)(max_w / f->w);
	if (fit >= len)
		fit = len - 1;
	if (n <= fit) {
		memcpy(out, s, n + 1);
		return;
	}
	if (fit == 0) {
		out[0] = 0;
		return;
	}
	size_t keep = fit - 1;
	size_t cut = keep;
	while (cut > 0 && s[cut] != ' ')
		cut--;
	if (cut >= keep / 2)
		keep = cut;		// end at a word unless that throws away more than half
	while (keep > 0 && s[keep - 1] == ' ')
		keep--;
	memcpy(out, s, keep);
	out[keep] = GLYPH_ELLIPSIS[0];
	out[keep + 1] = 0;
#ifdef GFX_FIT_TRACE
	gfx_fit_trace(s, out);
#endif
#endif	// CONFIG_LANG_ZH
}

// A row's label (left) and value (right, may hold text alternates). The label says what the row is, so it is kept
// whole whenever it can be: the value gets the room that is left, as the longest of its alternates that fits (an
// alternate may be empty: the value is left out). Only a label too long even next to the shortest value is shortened:
// at the end for a preview, else from the middle (gfx_text_fit). Margins shrink to nothing before anything is cut
static void row_text(const font_t *f, int x, int y, int w, const char *label, const char *value, color_t fg,
	color_t vfg, bool preview)
{
	char lab[48], val[48];
	bool more = true;
	int gap = f->w / 2 + 1;
	int lw = gfx_text_width(f, label);
	int vw = 0;
	val[0] = 0;
	// The longest value alternate that fits next to the whole label (or the shortest one there is)
	for (int level = 0; value[0] && more; level++) {
		gfx_text_alt(value, level, val, sizeof val, &more);
		vw = gfx_text_width(f, val);
		if (x + lw + (vw ? gap + vw : 0) <= w)
			break;
	}
	int room = w - x - (vw ? vw + gap : 0);
	if (lw <= room) {
		gfx_text(f, x, y, label, fg);
	} else {
		if (preview && !strchr(label, TEXT_ALT_OPEN[0]))
			fit_end(f, label, room, lab, sizeof lab);
		else
			gfx_text_fit(f, label, room, lab, sizeof lab);
		gfx_text(f, x, y, lab, fg);
	}
	if (vw) {
		int right = x + (lw < room ? lw : room) + gap + vw < w ? 1 : 0;	// a pixel of margin if it is free
		gfx_text(f, w - vw - right, y, val, vfg);
	}
}

// Where a list's body ends: half the room below y (a message keeps its replies in view), all of it without rows
static int list_body_end(const ui_list_t *l, int y, int y_end)
{
	return l->count ? y + (y_end - y) / 2 + ui_layout.small->h : y_end;
}

bool ui_list_body_fits(const ui_state_t *s, const ui_list_t *l)
{
	const font_t *f = ui_layout.small;
	int y = list_top(NULL, NULL, l, false);
	int y_end = list_body_end(l, y, list_bottom(s));
	char line[64];
	// as wrap_text: a line that doesn't fit is where ui_draw_list shortens the text
	for (const char *t = l->body; t != NULL && *t; y += f->h) {
		if (y + f->h > y_end)
			return false;
		t = wrap_line(f, t, gfx_width(), line, sizeof line);
	}
	return true;
}

int ui_list_rows(const ui_list_t *l)
{
	int y = list_top(NULL, NULL, l, false);
	if (l->body != NULL)
		y = wrap_text(ui_layout.small, l->body, y, y + (ui_layout.hint_y - y) / 2 + ui_layout.small->h, false, 0) + 2;
	int n = (ui_layout.hint_y - y) / list_pitch(l);
	return n < 1 ? 1 : n;
}

#if CONFIG_UI_PROFILES
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
#endif

// The reference theme's rows: one column of them from edge to edge, as many as fit
static void ref_list_geom(const ui_list_t *l, int y, int y_end, ui_list_geom_t *g)
{
	g->font = ui_layout.small;
	g->x = 0;
	g->y = (int16_t)y;
	g->pitch = (int16_t)list_pitch(l);
	int rows = (y_end - y) / g->pitch;
	if (rows < 1)
		rows = 1;
	g->cols = 1;
	g->col_w[1] = 0;
	g->col_gap = 0;
	g->paged = false;
#if CONFIG_UI_PROFILES
	if (l->roomy)
		g->x = 7;
	if (l->two_columns) {
		rows = 3;
		g->pitch = (int16_t)((y_end - y) / rows);
		if (l->main_menu)
			g->font = &font_aa_12x24;
		g->cols = 2;
		g->col_gap = 8;
		g->paged = true;
	}
#endif
	g->rows = (uint8_t)rows;
	int inner_w = gfx_width() - 2 * g->x;
	g->col_w[0] = (int16_t)inner_w;
#if CONFIG_UI_PROFILES
	if (l->two_columns) {
		int visible = 2 * rows;
		bool paginated = l->count > visible;
		int column_w = (inner_w - g->col_gap) / 2;
		if (!l->main_menu) {
			// Give a column extra width for longer translations before shortening a label. Measured over every page,
			// so the columns keep one split as the pages turn
			int minimum[2] = { 0, 0 };
			for (uint16_t i = 0; i < l->count; i++) {
				ui_item_t it = { 0 };
				l->item(i, &it);
				int need = (paginated ? 8 : 10) + gfx_text_width(g->font, it.label);
				if (l->menu_icons && it.icon != UI_MENU_ICON_NONE)
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
#endif
}

// The reference theme's row: the focus as a filled bar, the key's digit, the label and its value
static void ref_cell(const ui_list_t *l, const ui_list_geom_t *g, const ui_cell_t *c, const ui_item_t *it)
{
	const ui_layout_t *L = &ui_layout;
	const font_t *f = g->font;
	int pitch = c->h;
	bool paginated = false;
	bool main_menu = false;
	int inset = 0, gap = 0;
	color_t disabled = ui_col.line;
#if CONFIG_UI_PROFILES
	bool two_columns = g->cols > 1;
	paginated = two_columns && l->count > g->rows * g->cols;
	main_menu = l->main_menu;
	inset = g->x;
	if (inset) {
		gap = l->roomy ? 4 : 2;
		disabled = ui_col.dim;
	}
#else
	(void)l;
#endif
	bool focus = c->focus;
	color_t fg = (it->flags & UI_ITEM_DIM) ? disabled : (it->flags & UI_ITEM_WARN) ? ui_col.warn : ui_col.fg;
	if (gfx_bpp() == 1)
		fg = ui_col.fg;
	if ((it->flags & UI_ITEM_INFO) && gfx_bpp() != 1)
		fg = ui_col.dim;
	color_t vfg = (it->flags & UI_ITEM_DIM) && gfx_bpp() != 1 ? disabled : ui_col.dim;
	if (it->flags & UI_ITEM_TEMP)
		vfg = ui_col.warn;
	if (focus) {
		color_t bg = (it->flags & UI_ITEM_EDIT) ? ui_col.warn : ui_col.focus;
#if CONFIG_UI_PROFILES
		if (inset) {
			profile_row(c->x, c->y + gap, c->w, pitch - 2 * gap,
				    it->flags & UI_ITEM_EDIT ? ui_col.warn : profile_row_focus);
			gfx_fill(c->x + 2, c->y + gap + 5, 2, pitch - 2 * gap - 10, ui_col.fg);
		} else
#endif
			gfx_fill(c->x, c->y + gap, c->w, pitch - 2 * gap, bg);
		fg = gfx_bpp() == 1 ? ui_col.bg : (it->flags & UI_ITEM_EDIT) ? ui_col.pill_dark_fg : ui_col.focus_fg;
		vfg = fg;
#if CONFIG_UI_PROFILES
	} else if (inset && !(it->flags & UI_ITEM_INFO)) {
		profile_row(c->x, c->y + gap, c->w, pitch - 2 * gap, profile_row_bg);
#endif
	}
	int x = inset ? c->x + 5 : 1;
#if CONFIG_UI_PROFILES
	if (l->menu_icons && it->icon != UI_MENU_ICON_NONE) {
		draw_menu_icon(it->icon, x, c->y + (pitch - 13) / 2);
		x += two_columns ? 16 : 19;
		if (!it->num)
			x += two_columns ? f->w + 3 : 2 * f->w;
	}
#endif
	int right = c->x + c->w - (paginated ? 3 : inset ? 5 : 0);
	const font_t *row_font = f;
	int digit_gap = main_menu ? 5 : 3;
	if (main_menu && gfx_text_width(f, it->label) + (it->num ? f->w + digit_gap : 0) > right - x) {
		row_font = L->small;
		if (gfx_text_width(row_font, it->label) + (it->num ? row_font->w + digit_gap : 0) > right - x &&
		    gfx_text_width(L->tiny, it->label) + (it->num ? L->tiny->w + digit_gap : 0) <= right - x)
			row_font = L->tiny;
	}
	int pad = (pitch - row_font->h) / 2;
	if (gfx_bpp() == 1 && !focus)
		x = gfx_text(row_font, 0, c->y + pad, " ", fg);
	int label_room = right - x - (it->num ? row_font->w + digit_gap : 0);
	char val[48];
	const char *rest = NULL;
	if (paginated && !it->value[0] && pitch >= 2 * row_font->h &&
	    gfx_text_width(row_font, it->label) > label_room)
		rest = wrap_line(row_font, it->label, label_room, val, sizeof val);
	int ty = c->y + (rest && *rest ? (pitch - 2 * row_font->h) / 2 : pad);
	if (it->num) {
		char n[3] = { it->num, ' ', 0 };
		color_t num_ink = focus || gfx_bpp() == 1 ? fg : ui_col.dim;
#if CONFIG_UI_PROFILES
		if (two_columns) {
			n[1] = '\0';
			int digit_w = row_font->w + 1;
			gfx_text_title(row_font, x + digit_w / 2, c->y + pad + row_font->ascent, digit_w, n, num_ink, 4);
			x += row_font->w + digit_gap;
		} else
#endif
			x = gfx_text(row_font, x, ty, n, num_ink);
	}
	if (rest && *rest) {
		row_text(row_font, x, ty, right, val, "", fg, vfg, false);
		row_text(row_font, x, ty + row_font->h, right, rest, "", fg, vfg, false);
		return;
	}
	if (it->flags & UI_ITEM_EDIT) {
		sbuf_t b;
		sb_init(&b, val, sizeof val);
		sb_chr(&b, '<');
		sb_str(&b, it->value);
		sb_chr(&b, '>');
	} else {
		str_copy(val, sizeof val, it->value);
	}
	row_text(row_font, x, ty, right, it->label, val, fg, vfg,
		 (it->flags & UI_ITEM_PREVIEW) != 0);
}

// The reference theme's scroll marks, over the rows: more above / below
static void ref_scroll(const ui_list_geom_t *g, uint16_t top, uint16_t count, bool over)
{
	int w = gfx_width();
	int visible = g->rows * g->cols;
#if CONFIG_UI_PROFILES
	if (g->cols > 1) {
		// The page in the title, before the cells: its shades take their palette entries ahead of the cells' (gfx.c)
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
#endif
	if (!over)
		return;
	if (top > 0)
		gfx_fill(w - 3, g->y, 3, 2, ui_col.dim);
	if (top + visible < count)
		gfx_fill(w - 3, g->y + g->rows * g->pitch - 2, 3, 2, ui_col.dim);
}

// A list screen's frame, the same in every theme: the title, the body text, which rows are on screen, the banner and
// the bottom row. The theme places the cells and draws each one (ui_theme.h); only this function moves *l->top
void ui_draw_list(const radio_view_t *v, const ui_state_t *s, const ui_list_t *l)
{
	gfx_clear(ui_col.bg);
	int y = list_top(v, s, l, true);
	int y_end = list_bottom(s);
	if (l->body != NULL) {
		y = wrap_text(ui_layout.small, l->body, y, list_body_end(l, y, y_end), true, ui_col.fg);
		if (l->count) {
			gfx_hline(0, y, gfx_width(), ui_col.line);
			y += 2;
		}
	}
	ui_list_geom_t g;
	UI_THEME_OP(list_geom)(l, y, y_end, &g);
	int visible = g.rows * g.cols;
	uint16_t top = l->top != NULL ? *l->top : 0;
	if (g.paged) {
		top = (uint16_t)(l->focus / visible * visible);
	} else {
		if (l->focus < top)
			top = l->focus;
		if (l->focus >= top + visible)
			top = (uint16_t)(l->focus - visible + 1);
		if (top + visible > l->count)
			top = (uint16_t)(l->count > visible ? l->count - visible : 0);
	}
	if (l->top != NULL)
		*l->top = top;
	if (l->count == 0 && l->empty != NULL)
		wrap_text(g.font, l->empty, y + 1, y_end, true, ui_col.dim);
	UI_THEME_OP(scroll)(&g, top, l->count, false);
	for (int r = 0; r < visible && top + r < l->count; r++) {
		uint16_t i = (uint16_t)(top + r);
		ui_item_t it;
		it.label[0] = it.value[0] = 0;
		it.num = 0;
		it.flags = 0;
#if CONFIG_UI_PROFILES
		it.icon = UI_MENU_ICON_NONE;
#endif
		l->item(i, &it);
		// The first column is filled first, so Up and Down keep the order of a numbered menu
		int col = r / g.rows;
		ui_cell_t c = { .x = g.x, .y = (int16_t)(g.y + (r % g.rows) * g.pitch), .w = g.col_w[col], .h = g.pitch,
			.focus = i == l->focus && !(it.flags & UI_ITEM_INFO) };
		for (int k = 0; k < col; k++)
			c.x += g.col_w[k] + g.col_gap;
		UI_THEME_OP(cell)(l, &g, &c, &it);
	}
	UI_THEME_OP(scroll)(&g, top, l->count, true);
	ui_draw_banner(s);
	if (s->toast)
		ui_draw_hint(s->toast, true);
	else if (s->hints && l->hint) {
		uint8_t bar = 0;
#if CONFIG_UI_PROFILES
		bar = (l->roomy ? UI_BAR_LIST : 0) | (l->numbered ? UI_BAR_NUMBERED : 0);
#endif
		draw_hint(l->hint, bar, NULL);
	}
}

// Received emergency: the whole screen, a word label and the caller (P5: never color alone)
void ui_draw_emergency(const radio_view_t *v, const ui_state_t *s, uint32_t id, const char *name)
{
	const ui_layout_t *L = &ui_layout;
	int w = gfx_width();
	char buf[32];
	sbuf_t b;
	gfx_clear(gfx_bpp() == 1 ? ui_col.bg : ui_col.tx);
	color_t fg = gfx_bpp() == 1 ? ui_col.fg : ui_col.pill_fg;	// the words on the TX color, as on the TX pill
	int y = ui_draw_top(v, s, TR(TITLE_EMERGENCY, "Emergency"), NULL) + 2;
	const char *word = TR(EMERG_STATE_EMERGENCY, "EMERGENCY");
	const font_t *big = gfx_text_width(L->large, word) <= w ? L->large : L->medium;
	if (gfx_bpp() == 1) {
		gfx_fill(0, y, w, big->h, ui_col.fg);
		gfx_text(big, (w - gfx_text_width(big, word)) / 2, y, word, ui_col.bg);
	} else {
		gfx_text(big, (w - gfx_text_width(big, word)) / 2, y, word, fg);
	}
	y += big->h + 2;
	sb_init(&b, buf, sizeof buf);
	if (name != NULL && name[0])
		sb_str(&b, name);
	else
		sb_u32(&b, id, 1);
	char fit[40];
	gfx_text_fit(L->medium, buf, w, fit, sizeof fit);
	gfx_text(L->medium, (w - gfx_text_width(L->medium, fit)) / 2, y, fit, fg);
	y += L->medium->h;
	if (name != NULL && name[0] && y + L->small->h <= L->hint_y) {
		sb_init(&b, buf, sizeof buf);
		sb_str(&b, "ID ");
		sb_u32(&b, id, 1);
		gfx_text(L->small, (w - gfx_text_width(L->small, buf)) / 2, y, buf, fg);
	}
	ui_draw_hint(s->toast != NULL ? s->toast : TR(EMERG_HINT, "OK acknowledge  Back hide"), s->toast != NULL);
}

// The volume step as RADIO_VOLUME_MAX segments in w px, filled up to step: the volume screen and the knob's bar
static void volume_segments(int x, int y, int w, int h, int step)
{
	int pitch = w / RADIO_VOLUME_MAX, gap = pitch >= 6 ? 2 : 1;
	x += (w - pitch * RADIO_VOLUME_MAX) / 2;
	for (int i = 0; i < RADIO_VOLUME_MAX; i++) {
		if (i < step)
			gfx_fill(x + i * pitch, y, pitch - gap, h, ui_col.fg);
		else
			gfx_rect(x + i * pitch, y, pitch - gap, h, ui_col.line);
	}
}

#if CONFIG_VOLUME_BAR
// noinline: inlined into the frame code, its locals land in ui_task's deepest path, which has 8 bytes to spare on
// the boards with the tightest UI stack (make stack-check)
__attribute__((noinline)) void ui_draw_volume_bar(const ui_state_t *s, bool top)
{
	const ui_layout_t *L = &ui_layout;
	const font_t *f = L->small;
	int w = gfx_width();
	// Rows no screen lays its content in, so nothing is cut on any display class: the hint row (its keys and toasts
	// come back with it), or on a screen that fills the bottom (Quick's tiles, a list with hints off) the status strip
	int y = top ? 0 : L->hint_y, y0 = top ? 0 : y - 1, bh = top ? L->strip_h : gfx_height() - y0;
	char num[4];
	sbuf_t b;
	sb_init(&b, num, sizeof num);
	sb_u32(&b, (uint32_t)s->volbar, 1);
	const char *label = TR(VOL_LABEL_SHORT, "Vol");
	int lw = gfx_text_width(f, label), nw = gfx_text_width(f, "16");
	gfx_fill(0, y0, w, bh, ui_col.bg);
	gfx_hline(0, top ? y0 + bh - 1 : y0, w, ui_col.line);
	gfx_text(f, 2, y, label, ui_col.fg);
	gfx_text(f, w - 2 - gfx_text_width(f, num), y, num, ui_col.fg);
	volume_segments(lw + 6, y + 1, w - lw - nw - 12, f->h - 2, s->volbar);
}
#endif

// The volume screen: the step as a number (a word at 0) over a bar of RADIO_VOLUME_MAX segments.
// Large type, no scale to read
void ui_draw_volume(const radio_view_t *v, const ui_state_t *s)
{
	const ui_layout_t *L = &ui_layout;
	int w = gfx_width();
	char buf[16];
	sbuf_t b;
	gfx_clear(ui_col.bg);
	int y = ui_draw_top(v, s, TR(TITLE_VOLUME, "Volume"), NULL) + 4;
	sb_init(&b, buf, sizeof buf);
	if (s->volume == 0)
		sb_str(&b, TR(VOL_VALUE_MUTED, "Muted"));
	else
		sb_u32(&b, s->volume, 1);
	const font_t *big = L->large;
	if (gfx_text_width(big, buf) > w)
		big = L->medium;
	gfx_text(big, (w - gfx_text_width(big, buf)) / 2, y, buf, ui_col.fg);
	y += big->h + 6;
	volume_segments(4, y, w - 8, L->row_h > 10 ? L->row_h : 10, s->volume);
	ui_draw_bottom(s, TR(VOL_HINT, "Up Down change  OK done"));
}

// --- Hotspot ---
#if CONFIG_HOTSPOT
void ui_draw_hotspot(const radio_view_t *v, const hotspot_view_t *h, const ui_state_t *s)
{
	const ui_layout_t *L = &ui_layout;
	int w = gfx_width();
	char buf[48];
	sbuf_t b;

	gfx_clear(ui_col.bg);
	// the strip's pill shows what the hotspot does
	int y = draw_top_as(v, h->transmitting ? RS_TX : h->receiving ? RS_RX : RS_IDLE, s,
		TR(TITLE_HOTSPOT, "Hotspot"), NULL) + 1;
	const char *word = !h->running ? TR(HOTSPOT_STATE_STARTING, "STARTING") : h->transmitting ?
		TR(HOTSPOT_STATE_TX_NET, "TX NET") : h->receiving ? TR(HOTSPOT_STATE_RX_RF, "RX RF") :
		TR(HOTSPOT_STATE_READY, "READY");
	const font_t *big = gfx_text_width(L->large, word) <= w ? L->large : L->medium;
	gfx_text_fit(big, word, w, buf, sizeof buf);	// a translation too wide even for the medium font
	gfx_text(big, 0, y, buf, h->transmitting ? ui_col.tx : h->receiving ? ui_col.rx : ui_col.fg);
	y += big->h + 1;

	sb_init(&b, buf, sizeof buf);
	if (h->rx_hz == 0) {
		sb_str(&b, TR(HOTSPOT_BODY_NO_FREQUENCY, "No frequency yet"));
	} else if (h->rx_hz == h->tx_hz) {
		sb_freq(&b, h->rx_hz);
		sb_str(&b, " MHz");
	} else {
		sb_str(&b, "RX ");
		sb_freq(&b, h->rx_hz);
		sb_str(&b, " TX ");
		sb_freq(&b, h->tx_hz);
	}
	y = call_line(L->medium, y, buf, ui_col.fg);
	sb_init(&b, buf, sizeof buf);
	sb_str(&b, "CC");
	sb_u32(&b, h->cc, 1);
	sb_str(&b, " ");
	// What the hotspot keys; the radio's own power until the radio core has entered hotspot mode
	sb_str(&b, ui_power_name(v->hotspot ? v->hs_power : v->power, h->tx_hz != 0 ? h->tx_hz : v->ch.tx_hz));
	y = call_line(L->small, y, buf, ui_col.dim);
	sb_init(&b, buf, sizeof buf);
	if (h->src != 0) {
		sb_str(&b, h->from_net ? TR(HOTSPOT_BODY_FROM_NET, "Net ") : "RF ");
		sb_u32(&b, h->src, 1);
		sb_str(&b, " " GLYPH_ARROW " ");
		if (h->group)
			sb_str(&b, "TG ");
		sb_u32(&b, h->dst, 1);
	} else {
		sb_str(&b, TR(HOTSPOT_EMPTY_NO_CALLS, "No calls yet"));
	}
	call_line(L->small, y, buf, ui_col.fg);
	ui_draw_hint(s->toast != NULL ? s->toast : TR(HOTSPOT_HINT_OK_STOPS, "OK stop hotspot"), s->toast != NULL);
}
#endif

// --- Quick settings (3x3 grid, positions match the keypad; a numbered list on 128x64) ---

uint8_t ui_quick_kind(int pos, const radio_view_t *v)
{
	static const uint8_t fm[UI_QUICK_TILES] = {
		QT_POWER, QT_SQUELCH, QT_TONE, QT_MODE, QT_MONITOR, QT_VOX, QT_SCAN, QT_BANDWIDTH, QT_BRIGHTNESS,
	};
	static const uint8_t dmr[UI_QUICK_TILES] = {
		QT_POWER, QT_TIMESLOT, QT_CC, QT_MODE, QT_HEAR_ALL, QT_PRIVACY, QT_SCAN, QT_TALK_TO, QT_BRIGHTNESS,
	};
	if (pos < 0 || pos >= UI_QUICK_TILES)
		return QT_POWER;
	if (!v->vfo && pos == 3)
		return QT_TO_VFO;	// a channel's tile 4 copies it to the VFO; the VFO's stays Mode
	uint8_t kind = v->ch.mode == RADIO_MODE_DMR ? dmr[pos] : fm[pos];
	if (v->vfo && (kind == QT_MONITOR || kind == QT_PRIVACY))
		return QT_SPLIT;	// the VFO's TX frequency; Monitor stays on its key, privacy on channels
	return kind;
}

static bool feat(uint32_t f)
{
	return (ui_svc_features() & f) != 0;
}

bool ui_quick_tile(int tile, const radio_view_t *v, const ui_state_t *s, char *label, size_t label_len,
	char *value, size_t value_len)
{
	// The whole word, and an abbreviation of 6 characters at most for a tile (font.h alternates), so a tile's name
	// never runs into the next tile's digit in the label font. Written out (no macro): tools/i18n.py reads the marks
	static const tr_id_t names[] = {
		[QT_POWER] = TRN(QUICK_LABEL_POWER, TEXT_ALT_OPEN "Power" TEXT_ALT_NEXT "Pwr" TEXT_ALT_CLOSE),
		[QT_SQUELCH] = TRN(QUICK_LABEL_SQUELCH, TEXT_ALT_OPEN "Squelch" TEXT_ALT_NEXT "SQL" TEXT_ALT_CLOSE),
		[QT_TONE] = TRN(QUICK_LABEL_TONE, "Tone"), [QT_MODE] = TRN(LABEL_MODE, "Mode"),
		[QT_MONITOR] = TRN(QUICK_LABEL_MONITOR, TEXT_ALT_OPEN "Monitor" TEXT_ALT_NEXT "MON" TEXT_ALT_CLOSE),
		[QT_VOX] = TRX(WORD_VOX, "VOX"),
#if CONFIG_DUAL_WATCH
		// tile 7 is Dual watch; a zone scan is in Tools > Scan
		[QT_SCAN] = TRN(QUICK_LABEL_DUAL_WATCH, TEXT_ALT_OPEN "Dual watch" TEXT_ALT_NEXT "Dual" TEXT_ALT_CLOSE),
#else
		[QT_SCAN] = TRN(WORD_SCAN, "Scan"),
#endif
		[QT_BANDWIDTH] = TRN(QUICK_LABEL_WIDTH, "Width"), [QT_BRIGHTNESS] = TRN(QUICK_LABEL_BRIGHT, "Bright"),
		[QT_TIMESLOT] = TRN(QUICK_LABEL_TIMESLOT, TEXT_ALT_OPEN "Timeslot" TEXT_ALT_NEXT "Slot" TEXT_ALT_CLOSE),
		[QT_CC] = TRN(QUICK_LABEL_COLOR, "Color"),
		[QT_HEAR_ALL] = TRN(QUICK_LABEL_DMON, TEXT_ALT_OPEN "Digital monitor"
			TEXT_ALT_NEXT "DMon" TEXT_ALT_CLOSE),
		[QT_PRIVACY] = TRN(QUICK_LABEL_PRIVACY, TEXT_ALT_OPEN "Privacy" TEXT_ALT_NEXT "Priv" TEXT_ALT_CLOSE),
		[QT_TALK_TO] = TRN(QUICK_LABEL_TALK_TO, TEXT_ALT_OPEN "Talk to" TEXT_ALT_NEXT "Talk" TEXT_ALT_CLOSE),
		[QT_TO_VFO] = TRN(QUICK_LABEL_COPY_TO_VFO, TEXT_ALT_OPEN "Copy to VFO"
			TEXT_ALT_NEXT "To VFO" TEXT_ALT_CLOSE),
		[QT_SPLIT] = TRN(QUICK_LABEL_TX_FREQ, TEXT_ALT_OPEN "TX freq" TEXT_ALT_NEXT "TXFreq" TEXT_ALT_CLOSE),
	};
	uint8_t kind = ui_quick_kind(tile, v);
	bool editing = s->quick_edit && s->quick_focus == tile;
	sbuf_t b;
	sb_init(&b, label, label_len);
	sb_chr(&b, (char)('1' + tile));
	sb_str(&b, tr(names[kind]));
	sb_init(&b, value, value_len);
	switch (kind) {
	case QT_POWER:
		if (v->power != channel_power(v))
			sb_str(&b, GLYPH_DOT);
		sb_str(&b, ui_power_name(v->power, v->ch.tx_hz));
		return true;
	case QT_SQUELCH:
		if (v->temp & RADIO_TEMP_SQUELCH)
			sb_str(&b, GLYPH_DOT);
		sb_u32(&b, v->squelch, 1);
		return true;
	case QT_TONE: {
		if (!CONFIG_SUBTONE)
			return false;
		if (editing && s->tone_target != RADIO_TONE_BOTH) {
			sbuf_t lb;
			sb_init(&lb, label, label_len);
			sb_chr(&lb, (char)('1' + tile));
			sb_str(&lb, s->tone_target == RADIO_TONE_TX ? TR(QUICK_LABEL_TONE_TX, "Tone TX") :
				TR(QUICK_LABEL_TONE_RX, "Tone RX"));
		}
		if (!editing && (v->temp & (RADIO_TEMP_TX_TONE | RADIO_TEMP_RX_TONE)))
			sb_str(&b, GLYPH_DOT);
		const subtone_t *t = &v->ch.tx_tone;
		bool same = v->ch.tx_tone.type == v->ch.rx_tone.type &&
			(v->ch.tx_tone.type == TONE_NONE || v->ch.tx_tone.value == v->ch.rx_tone.value);
		if (editing) {
			t = &s->tone_edit;
		} else if (!same && v->ch.tx_tone.type != TONE_NONE) {
			sb_chr(&b, 'T');	// TX only, or TX and RX differ: the TX tone, marked
		} else if (!same) {
			sb_chr(&b, 'R');
			t = &v->ch.rx_tone;
		}
		if (t->type == TONE_NONE)
			sb_str(&b, TR(VALUE_NONE, "None"));
		else
			sb_tone(&b, t);
		return v->ch.mode == RADIO_MODE_FM;
	}
	case QT_MODE:
		sb_str(&b, mode_word(v));	// what it is, as every other tile
		// Only in the VFO (a channel's tile 4 is QT_TO_VFO), in a build with both VFO kinds; it keeps the mode like
		// its other settings
		return feat(UI_FEAT_MODE) && v->vfo && CONFIG_VFO_FM && CONFIG_VFO_DMR;
	case QT_TO_VFO:
		sb_str(&b, TR(QUICK_VALUE_COPY, "Copy"));
		return CONFIG_VFO_FM || CONFIG_VFO_DMR;	// a channel the VFO can't hold is refused on the press, with the reason
	case QT_SPLIT:
		if (editing && s->entry_len > 0) {
			sb_str(&b, "TX");	// the typed frequency is too wide for a tile: the hint row has it whole
		} else {
			sb_split(&b, v->ch.rx_hz, v->ch.tx_hz);
		}
		return v->vfo && (CONFIG_VFO_FM || CONFIG_VFO_DMR);
	case QT_MONITOR:
		sb_str(&b, v->monitor ? TR(VALUE_ON, "On") : TR(VALUE_OFF, "Off"));
		return feat(UI_FEAT_MONITOR_LATCH);
	case QT_VOX:
		sb_str(&b, v->vox_on ? TR(VALUE_ON, "On") : TR(VALUE_OFF, "Off"));
		return feat(UI_FEAT_VOX) && v->ch.mode == RADIO_MODE_FM;
	case QT_SCAN:
#if CONFIG_DUAL_WATCH
		sb_str(&b, s->dual.on ? TR(VALUE_ON, "On") : TR(VALUE_OFF, "Off"));
#else
		sb_str(&b, s->scanning ? TR(VALUE_ON, "On") : TR(VALUE_OFF, "Off"));
#endif
		return feat(UI_FEAT_SCAN);
	case QT_BANDWIDTH:
		if (v->temp & RADIO_TEMP_BW)
			sb_str(&b, GLYPH_DOT);
		sb_str(&b, v->ch.bw == RADIO_BW_25K ? "25k" : "12.5k");
		return v->ch.mode == RADIO_MODE_FM;
	case QT_BRIGHTNESS:
		sb_u32(&b, s->brightness, 1);
		sb_chr(&b, '%');
		return true;
#if CONFIG_DMR
	case QT_TIMESLOT:
		sb_str(&b, "TS");
		sb_u32(&b, v->dmr.ts ? v->dmr.ts : v->ch.timeslot, 1);
		return feat(UI_FEAT_TIMESLOT);
	case QT_CC:
		sb_str(&b, "CC");
		sb_u32(&b, editing ? s->cc_edit : v->dmr.active ? v->dmr.cc : v->ch.color_code, 1);
		return feat(UI_FEAT_COLOR_CODE);
	case QT_HEAR_ALL:
		sb_str(&b, s->hear_all ? TR(VALUE_ON, "On") : TR(VALUE_OFF, "Off"));
		return feat(UI_FEAT_HEAR_ALL);
	case QT_PRIVACY:
		sb_str(&b, s->privacy ? TR(VALUE_ON, "On") : TR(VALUE_OFF, "Off"));
		return feat(UI_FEAT_PRIVACY) && CONFIG_DMR_PRIVACY;
	case QT_TALK_TO:
		if (v->dmr.target_private)
			sb_u32(&b, v->dmr.target_id, 1);
		else if (v->dmr.target_id == 0xFFFFFFu)
			sb_str(&b, TR(QUICK_VALUE_ALL, "All"));
		else
			sb_u32(&b, v->dmr.target_id, 1);
		return true;
#endif
	default:
		return false;
	}
}

static uint16_t quick_list_focus;
static const radio_view_t *quick_v;
static const ui_state_t *quick_s;

static void quick_list_item(uint16_t i, ui_item_t *it)
{
	char label[24];
	bool ok = ui_quick_tile(i, quick_v, quick_s, label, sizeof label, it->value, sizeof it->value);
	it->num = label[0];
	str_copy(it->label, sizeof it->label, label + 1);	// the whole word: a list row has room for it
	if (!ok)
		it->flags |= UI_ITEM_DIM;
	if (quick_s->quick_edit && quick_s->quick_focus == i)
		it->flags |= UI_ITEM_EDIT;
}

// The hint while the TX freq tile edits: what Up / Down and the digits do, or the TX frequency typed so far, whole
// (blanks for the digits to come); NULL when that tile isn't editing
static const char *quick_split_hint(const radio_view_t *v, const ui_state_t *s, char *buf, size_t len)
{
	if (!s->quick_edit || ui_quick_kind(s->quick_focus, v) != QT_SPLIT)
		return NULL;
	if (s->entry_len == 0)
		return TR(QUICK_HINT_SPLIT, TEXT_ALT_OPEN "Up/Dn: +/- offset  Type: TX freq"
			TEXT_ALT_NEXT "Up/Dn +/- offset  0-9 TX freq"
			TEXT_ALT_NEXT "Up/Dn offset  0-9 TX freq" TEXT_ALT_NEXT "Up/Dn +/-  0-9 TX freq" TEXT_ALT_CLOSE);
	sbuf_t b;
	sb_init(&b, buf, len);
	sb_str(&b, "TX ");
	for (uint8_t i = 0; i < UI_ENTRY_MAX; i++) {
		if (i == UI_ENTRY_MHZ)
			sb_chr(&b, '.');
		sb_chr(&b, i < s->entry_len ? s->entry_digits[i] : '_');
	}
	sb_str(&b, TR(QUICK_HINT_OK_SETS, "  OK sets"));
	return buf;
}

// 128x64: a numbered list with the same numbers (wireframe 128x64). The list and the tiles each have their own
// frame, so the list's deep draw doesn't sit on the tiles' (the ui task's stack is tight)
static void __attribute__((noinline)) draw_quick_list(const radio_view_t *v, const ui_state_t *s)
{
	static uint16_t top;
	char hint[24];
	const char *split_hint = quick_split_hint(v, s, hint, sizeof hint);
	quick_v = v;
	quick_s = s;
	quick_list_focus = s->quick_focus;
	ui_list_t l = { .prefix = TR(QUICK_TITLE, "Quick"), .name = v->vfo ? "VFO" : v->ch.name,
		.count = UI_QUICK_TILES, .focus = quick_list_focus, .top = &top, .item = quick_list_item,
		.hint = split_hint != NULL ? split_hint : TR(QUICK_HINT, "1-9 change  Hold OK save") };
	ui_draw_list(v, s, &l);
}

static void __attribute__((noinline)) draw_quick_tiles(const radio_view_t *v, const ui_state_t *s)
{
	const ui_layout_t *L = &ui_layout;
	int w = gfx_width(), h = gfx_height();
	const char *name = v->vfo ? "VFO" : v->ch.name;
	char label[24];	// a tile's label, and the hint's text after the tiles (one buffer: the ui task's stack is tight)
	const char *split_hint = quick_split_hint(v, s, label, sizeof label);

	gfx_clear(ui_col.bg);
	// Title row keeps the state pill (P2)
	int top = ui_draw_top(v, s, TR(QUICK_TITLE, "Quick"), name);
	int bottom = h;
	if (s->toast != NULL || split_hint != NULL || h >= UI_TALL_PX)
		bottom = L->hint_y;
	int gh = bottom - top;
	const font_t *lf = L->small;
	const font_t *vf = L->medium;
	if (gh / 3 < lf->h + vf->h + 2)
		vf = lf;
	if (gh / 3 < lf->h + vf->h)
		lf = L->tiny;
	for (int i = 0; i < UI_QUICK_TILES; i++) {
		int cx = i % 3, cy = i / 3;
		int x0 = w * cx / 3, x1 = w * (cx + 1) / 3 - (cx == 2 ? 1 : 0);
		int y0 = top + gh * cy / 3, y1 = top + gh * (cy + 1) / 3 - (cy == 2 ? 1 : 0);
		char value[12], fit[12];	// label: "5" and the name's alternates
		bool ok = ui_quick_tile(i, v, s, label, sizeof label, value, sizeof value);
		bool focus = i == s->quick_focus;
		color_t fg = ok ? ui_col.fg : ui_col.line, bg = ui_col.bg;
		if (gfx_bpp() == 1 && !ok)
			fg = ui_col.fg;
		if (focus) {
			bg = s->quick_edit ? ui_col.warn : ui_col.focus;
			fg = gfx_bpp() == 1 ? ui_col.bg : s->quick_edit ? ui_col.pill_dark_fg : ui_col.focus_fg;
			gfx_fill(x0 + 1, y0 + 1, x1 - x0 - 1, y1 - y0 - 1, bg);
		}
		gfx_rect(x0, y0, x1 - x0 + 1, y1 - y0 + 1, ui_col.line);
		gfx_clip(x0 + 1, y0 + 1, x1 - x0 - 1, y1 - y0 - 1);
		// The key's digit, half a character, then the name (it ends before the next tile's digit)
		char key[2] = { label[0], 0 };
		int nx = gfx_text(lf, x0 + 2, y0 + 1, key, fg) + lf->w / 2;
		gfx_text_fit(lf, label + 1, x1 - nx - (cx < 2 ? lf->w / 2 : 1), fit, sizeof fit);	// a gap before the next tile
		gfx_text(lf, nx, y0 + 1, fit, focus ? fg : ok ? ui_col.dim : fg);
		if (focus && s->quick_edit) {
			// Editing: arrows around the value say Up/Dn changes it
			sbuf_t vb;
			char tmp[16];
			sb_init(&vb, tmp, sizeof tmp);
			sb_chr(&vb, '<');
			sb_str(&vb, value);
			sb_chr(&vb, '>');
			memcpy(value, tmp, sizeof value - 1);
			value[sizeof value - 1] = 0;
		}
		const font_t *tf = vf;	// a value too wide for the tile ("<D023N>") drops to the label font
		if (gfx_text_width(tf, value) > x1 - x0 - 3)
			tf = lf;
		gfx_text_fit(tf, value, x1 - x0 - 3, fit, sizeof fit);
		int vx = x0 + (x1 - x0 + 1 - gfx_text_width(tf, fit)) / 2;
		int vy = y0 + 1 + lf->h + (y1 - y0 - 1 - lf->h - tf->h) / 2;
		gfx_text(tf, vx, vy, fit, fg);
		gfx_clip_off();
	}
	ui_draw_banner(s);
	if (s->toast != NULL)
		ui_draw_hint(s->toast, true);
	else if (split_hint != NULL)
		ui_draw_hint(quick_split_hint(v, s, label, sizeof label), false);	// the tiles used label
	else if (h >= UI_TALL_PX)
		ui_draw_hint(TR(QUICK_HINT_ALL_SETTINGS, "0 All settings  Hold OK save"), false);
}

static void ref_quick(const radio_view_t *v, const ui_state_t *s)
{
	if (gfx_width() < 150)
		draw_quick_list(v, s);
	else
		draw_quick_tiles(v, s);
}

void ui_draw_quick(const radio_view_t *v, const ui_state_t *s)
{
	UI_THEME_OP(quick)(v, s);
}

#if CONFIG_DIAG
static uint8_t diag_lines_per_page;

uint8_t ui_diag_next_page(uint8_t first_line)
{
	char buf[40];
	uint8_t next = (uint8_t)(first_line + (diag_lines_per_page ? diag_lines_per_page : 1));
	if (board.diag_line == NULL || !board.diag_line(next, buf, sizeof buf))
		return 0;
	return next;
}

void ui_draw_diag(const radio_view_t *v, uint32_t keys, int32_t enc_total, uint8_t first_line)
{
	const ui_layout_t *L = &ui_layout;
	const font_t *f = L->tiny != NULL ? L->tiny : L->small;	// small print: as many lines as fit
	char buf[40];
	sbuf_t b;
	int y = 0;

	gfx_clear(ui_col.bg);
	gfx_text(f, 0, y, TR(TITLE_DIAGNOSTICS, "Diagnostics"), ui_col.warn);
	y += f->h;
	// On every page, at most two lines: a translation may be longer than one
	if (board_variant_mismatch())
		y = wrap_text(f, TR(DIAG_VARIANT_MISMATCH, "This firmware may be for another version of this radio"), y,
			y + 2 * f->h, true, ui_col.warn);

	sb_init(&b, buf, sizeof buf);
	sb_str(&b, "keys ");
	for (int i = 5; i >= 0; i--)
		sb_chr(&b, "0123456789ABCDEF"[(keys >> (4 * i)) & 0xF]);
	sb_str(&b, " enc ");
	if (enc_total < 0) {
		sb_chr(&b, '-');
		enc_total = -enc_total;
	}
	sb_u32(&b, (uint32_t)enc_total, 1);
	gfx_text(f, 0, y, buf, ui_col.fg);
	y += f->h;

	sb_init(&b, buf, sizeof buf);
	sb_str(&b, "rx ");
	if (v->rssi_dbm < 0) {
		sb_chr(&b, '-');
		sb_u32(&b, (uint32_t)-v->rssi_dbm, 1);
	} else {
		sb_u32(&b, (uint32_t)v->rssi_dbm, 1);
	}
	sb_str(&b, "dBm nz ");
	sb_u32(&b, v->noise, 1);
	sb_str(&b, v->state == RS_RX ? " open" : v->state == RS_TX ? " TX" : " closed");
	if (v->ch.rx_tone.type != TONE_NONE)
		sb_str(&b, v->tone_ok ? " tone" : " -tone");
	gfx_text(f, 0, y, buf, ui_col.fg);
	y += f->h;

	diag_lines_per_page = 0;
	for (uint8_t i = first_line; board.diag_line != NULL && y + f->h <= gfx_height() - f->h; i++) {
		diag_lines_per_page++;
		if (!board.diag_line(i, buf, sizeof buf))
			break;
		gfx_text(f, 0, y, buf, ui_col.dim);
		y += f->h;
	}
	char named[32];
	gfx_text(f, 0, gfx_height() - f->h, key_names(TR(DIAG_HINT, "* more  Back"), named, sizeof named, true),
		ui_col.idle);
}
#endif

// --- The reference theme: the look every screen above draws unless another theme gives its own op (ui_theme.h) ---

static const ui_theme_ops_t reference_ops = {
	.layout = ref_layout,
	.colors = accent_apply,
	.top = ref_top,
	.bottom = ref_bottom,
	.banner = ref_banner,
	.home = ref_home,
#if CONFIG_DUAL_WATCH
	.home_dual = ref_home_dual,
#endif
	.quick = ref_quick,
	.list_head = ref_list_head,
	.list_geom = ref_list_geom,
	.cell = ref_cell,
	.scroll = ref_scroll,
};

const ui_theme_t ui_theme_reference = {
	.palettes = palettes,
	.palette_count = sizeof palettes / sizeof palettes[0],
	.ops = &reference_ops,
};
