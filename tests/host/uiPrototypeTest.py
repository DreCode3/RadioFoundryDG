# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
"""Exercise the display profiles through the native simulator's public entry points."""

import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import zlib


SIM = Path(__file__).resolve().with_name("uiSim")
# The simulator with Chinese built in (make uiSimZh; run-ui builds it before this test)
SIM_ZH = Path(__file__).resolve().with_name("uiSimZh")
GEOMETRY = "320x240iR10"
# Settings > Display's panels rows: Display profile and Day/Night, each a pick list, after Theme
AMATEUR = "ok 8 1 row:Display_profile ok down ok long:back "
NIGHT = "ok 8 1 row:Day/Night ok down ok long:back "
GPS_CLOCK = "ok 8 1 row:GPS_clock ok "
# The panels theme's layout on the 310 x 240 drawable area: the status strip, a list's title row, one row height (five
# rows above the hint row), a grid's tile (two lines of the 8x16 font; three rows of them above the hint row), the hint
# row's top and the soft key bar's
STRIP_H = 29
ROWS_Y = 59
ROW_H = 26
TILE_H = 41
HINT_Y = 186
BAR_Y = 204
# The panels theme's Day ink, its palette's fg RGB(23, 40, 53), as the display shows it (RGB565)
DAY_INK = bytes((16, 40, 49))


def ink_map(frame):
    """Each pixel as dark, light or between: the solid strokes, which a 16-color palette keeps whatever else the
    screen shows (only anti-aliased edges take the nearest of the colors in use)."""
    return [b"".join(b"d" if max(row[x:x + 3]) < 60 else b"l" if min(row[x:x + 3]) > 200 else b"."
                     for x in range(0, len(row), 3)) for row in frame]


def row_starts(frame, x):
    """The rows where a row panel starts in column x, between the title and the bottom row."""
    paper = frame[ROWS_Y][:3]
    column = [row[x * 3:x * 3 + 3] for row in frame]
    return [y for y in range(ROWS_Y + 1, HINT_Y) if column[y - 1] == paper and column[y] != paper]


def rgb565(c):
    """A color as the display shows it (the simulator writes RGB565 expanded to 8 bits a channel)."""
    return (c[0] >> 3) * 255 // 31, (c[1] >> 2) * 255 // 63, (c[2] >> 3) * 255 // 31


# The panels theme's ink and focus accent (core/ui/themes/panels/panels.c), Day and Night
PANELS_INK = {"": (rgb565((23, 40, 53)), rgb565((36, 79, 136)), rgb565((53, 78, 101)), rgb565((189, 41, 41))),
              "night": (rgb565((237, 242, 247)), rgb565((152, 191, 231)), rgb565((180, 194, 209)),
                        rgb565((255, 119, 119)))}


def near_segment(p, a, b, tol=50):
    """p within tol of the colors between a and b: one of them, or an anti-aliased blend of the two (on a 16-color
    display an edge takes the nearest color in use, so the blends are loose)."""
    d = [b[i] - a[i] for i in range(3)]
    n = sum(v * v for v in d) or 1
    t = max(0.0, min(1.0, sum((p[i] - a[i]) * d[i] for i in range(3)) / n))
    return sum((p[i] - a[i] - t * d[i]) ** 2 for i in range(3)) <= tol * tol


def pixel(frame, x, y):
    return tuple(frame[y][x * 3:x * 3 + 3])


def panel_runs(frame, x):
    """The rows of each tile panel crossing column x, between the title and the hint row: (top, bottom) inclusive."""
    paper, runs, top = pixel(frame, 0, ROWS_Y), [], None
    for y in range(ROWS_Y, HINT_Y):
        inside = pixel(frame, x, y) != paper
        if inside and top is None:
            top = y
        if not inside and top is not None:
            runs.append((top, y - 1))
            top = None
    return runs


class PrototypeTest(unittest.TestCase):
    def test_right_soft_key_opens_quick_only_on_home(self):
        self.run_script("back expect:screen=home ok 8 8 up up "
                        "expect:focus=Right_soft_key ok down down ok expect:shows=Quick "
                        "back expect:screen=settings back expect:screen=hub "
                        "back expect:screen=home back expect:screen=quick back expect:screen=home "
                        "reboot back expect:screen=quick back expect:screen=home "
                        "quick expect:screen=quick")

    def test_home_footer_follows_the_right_soft_key_assignment(self):
        settings = "ok 8 8 up up "
        home = "back back back "
        self.run_script("expect:shows=Back expect:hides=*_Quick " +
                        settings + "ok down down ok " + home + "expect:shows=Quick expect:hides=*_Quick "
                        "expect:shows=Menu expect:hides=OK_Menu expect:layout=0 "
                        "ok expect:shows=OK expect:shows=Back")
        self.run_script(settings + "ok down down down down ok " + home +
                        "expect:shows=Power_level expect:layout=0 long:dualkey wait:2000 "
                        "expect:dual=1 expect:shows=Power_level expect:layout=0 "
                        "gui:original expect:shows=Power_level")
        for profile in ("", AMATEUR):
            frame = self.render_frame(profile + settings + "ok down ok " + home +
                                      "expect:shows=Menu expect:hides=OK_Menu "
                                      "expect:hides=Nothing expect:hides=Quick expect:layout=0")
            self.assertTrue(all(row[570:870] == row[570:573] * 100 for row in frame[208:230]))

    def render_frame(self, script, sim=SIM, lang=None):
        with tempfile.TemporaryDirectory() as temp:
            image = Path(temp) / "frame.png"
            env = dict(os.environ, UI_LANG=lang) if lang else None
            subprocess.run([sim, "--geom", GEOMETRY, "--scale", "1", "--out", image,
                            "--script", script], check=True, capture_output=True, timeout=15, env=env)
            png = image.read_bytes()
        offset, compressed = 8, bytearray()
        while offset < len(png):
            size = int.from_bytes(png[offset:offset + 4], "big")
            if png[offset + 4:offset + 8] == b"IDAT":
                compressed.extend(png[offset + 8:offset + 8 + size])
            offset += size + 12
        # The native simulator exports unfiltered RGB frames.
        pixels = zlib.decompress(compressed)
        stride = 310 * 3 + 1
        return [pixels[start + 1:start + stride] for start in range(0, len(pixels), stride)]

    def test_home_signal_bars_follow_reception_in_both_profiles(self):
        # The bars sit at the strip's left end and push the mode word right: the strip's start differs from idle's.
        for select in ("", AMATEUR):
            with self.subTest(profile=select or "commercial"):
                idle = self.render_frame(select)

                def meter(script):
                    frame = self.render_frame(select + script)
                    return any(frame[y][5 * 3:34 * 3] != idle[y][5 * 3:34 * 3] for y in range(STRIP_H - 1))

                self.assertTrue(meter("fmrx"), "an active FM signal must draw bars")
                self.assertFalse(meter("fmrx fmrxend"), "the last RSSI sample must not leave bars behind")
                self.assertTrue(meter("rx:9991234,3100,g,Morgan_Reed"), "a DMR call must draw bars")
                self.assertFalse(meter("rx:9991234,3100,g,Morgan_Reed rxend"))
    def test_gps_pin_has_an_antialiased_tip_in_both_profiles(self):
        for select in ("", AMATEUR):
            before = self.render_frame(select)
            fixed = self.render_frame(select + "gps:fix")
            rows = [[fixed[y][x * 3:x * 3 + 3] for x in range(240, 272)
                     if fixed[y][x * 3:x * 3 + 3] != before[y][x * 3:x * 3 + 3]]
                    for y in range(STRIP_H - 1)]
            tip = next(row for row in reversed(rows) if row)
            self.assertLessEqual(len(tip), 3)
            self.assertTrue(all(0 < max(pixel) < 230 for pixel in tip))
    def test_profile_day_uses_vertical_battery_and_black_receive_only_mark(self):
        frame = self.render_frame("rxonly:on")
        idle = self.render_frame("")

        def black(x, y):
            return frame[y][x * 3:x * 3 + 3] == DAY_INK

        battery = [(x, y) for y in range(STRIP_H) for x in range(285, 308) if black(x, y)]
        xs, ys = zip(*battery)
        self.assertLess(max(xs) - min(xs), max(ys) - min(ys))
        # The receive-only mark is in the strip, with the operating marks: the word RX, in the ink
        receive_only = {(x, y) for y in range(STRIP_H) for x in range(120, 285)
                        if black(x, y) and idle[y][x * 3:x * 3 + 3] != DAY_INK}
        self.assertTrue(receive_only)
        self.run_script("rxonly:on expect:shows=RX expect:layout=0")
    def test_monitor_uses_separate_speaker_and_sound_wave(self):
        idle = self.render_frame("")
        monitor = self.render_frame("mon wait:3000")
        # In the strip, at its left end on Home (Home's strip has no mode word: the mode bar has it)
        pixels = {(x, y) for y in range(STRIP_H) for x in range(5, 200)
                  if monitor[y][x * 3:x * 3 + 3] == DAY_INK and idle[y][x * 3:x * 3 + 3] != DAY_INK}
        left = min(x for x, _ in pixels)
        # A solid cone with a stem on the left and a separate wave on the right.
        self.assertGreaterEqual(sum(x == left + 8 for x, _ in pixels), 10)
        self.assertFalse(any(x == left + 10 for x, _ in pixels))
        self.assertTrue(any(x >= left + 12 for x, _ in pixels))

        # Look for an intermediate edge color around the speaker's native pixels.
        edges = [monitor[y][x * 3:x * 3 + 3] for y in range(STRIP_H)
                 for x in range(left, left + 19)
                 if monitor[y][x * 3:x * 3 + 3] != idle[y][x * 3:x * 3 + 3]]
        self.assertTrue(any(0 < max(edge) < 220 for edge in edges))
    def test_talkgroup_is_larger_and_home_text_holds_position_during_rx(self):
        # The main line is the channel's name; a call adds its lines below a rule and moves nothing above them.
        call = "rx:9991234,3100,g,Morgan_Reed expect:centered=Morgan_Reed expect:layout=0"
        commercial = self.render_frame("expect:centered=COUNTY_DMR")
        amateur = self.render_frame(AMATEUR + "expect:centered=COUNTY_DMR")

        def main_rows(frame):
            # The main line's ink: the second run of inked rows in the channel pane, below the zone line's
            ink = [sum(frame[y][x * 3] < 120 for x in range(40, 270)) >= 6 for y in range(58, 150)]
            runs, y = [], 0
            while y < len(ink):
                if ink[y]:
                    start = y
                    while y < len(ink) and ink[y]:
                        y += 1
                    runs.append(y - start)
                y += 1
            return runs[1]

        self.assertGreaterEqual(main_rows(commercial), 30)
        self.assertGreater(main_rows(commercial), main_rows(amateur))
        for idle, receiving in ((commercial, self.render_frame(call)),
                                (amateur, self.render_frame(AMATEUR + call))):
            # The indexed palette can shift antialiased edge colors by one 4-bit step.
            self.assertLessEqual(max(abs(a - b) for idle_row, rx_row in zip(idle[STRIP_H:148], receiving[STRIP_H:148])
                                     for a, b in zip(idle_row, rx_row)), 17)
    def test_home_strip_is_the_strip_of_every_screen(self):
        # One status strip, a rounded pane: Home's is the menus' and the lists', pixel for pixel inside the pane, in
        # both layouts and palettes (around it is each screen's own color: Home's gutter, a list's background)
        # Home's strip has no mode word (its mode bar has the mode): compared right of the mode word
        def pane(frame):
            return [row[60 * 3:300 * 3] for row in frame[3:STRIP_H - 2]]
        for profile in ("", AMATEUR):
            for theme in ("", NIGHT):
                with self.subTest(profile=profile or "Commercial", theme=theme or "Day"):
                    home = self.render_frame(profile + theme + "gps:fix expect:layout=0")
                    for screen in ("ok", "ok 8", "ok 8 1", "ok 1", "quick"):
                        frame = self.render_frame(profile + theme + "gps:fix " + screen + " expect:layout=0")
                        self.assertEqual(ink_map(pane(frame)), ink_map(pane(home)), screen)
                        # The pane's rounded corner: the screen's color at the very corner, the pane's inside it
                        self.assertNotEqual(frame[2][4 * 3:5 * 3], frame[10][10 * 3:11 * 3], screen)
                    # The list's strip starts with the mode word, Home's doesn't
                    def left(frame):
                        return any(max(row[x * 3:x * 3 + 3]) < 100 for row in frame[3:STRIP_H - 2]
                                   for x in range(10, 50))
                    if not theme:	# dark ink on the Day strip
                        self.assertFalse(left(home))
                        self.assertTrue(left(self.render_frame(profile + "gps:fix ok 1")))
        self.run_script("expect:shows=DMR down expect:shows=FM ok 8 expect:shows=FM")
    def test_dstar_preview_uses_full_mode_label(self):
        self.run_script("prototype:dstar_on expect:shows=D-STAR expect:layout=0 "
                        "aprstx expect:hides=D-STAR aprsend expect:shows=D-STAR "
                        "prototype:dstar_off expect:hides=D-STAR")
        for profile in ("", AMATEUR):
            with self.subTest(profile=profile or "Commercial"):
                # A list's strip: its first 80 pixels hold only the mode, in its largest font
                frame = self.render_frame(profile + "prototype:dstar_on ok 1 expect:layout=0")
                mode_rows = [y for y in range(STRIP_H - 1)
                             if any(max(frame[y][x * 3:x * 3 + 3]) < 100 for x in range(5, 80))]
                self.assertGreaterEqual(max(mode_rows) - min(mode_rows) + 1, 12)
                self.assertGreaterEqual(min(mode_rows), 1)
                # Home: the strip has no mode word, the mode bar has it in the largest font
                home = self.render_frame(profile + "prototype:dstar_on expect:layout=0")
                self.assertFalse([y for y in range(STRIP_H - 1)
                                  if any(max(home[y][x * 3:x * 3 + 3]) < 100 for x in range(5, 80))])
                bar_rows = [y for y in range(STRIP_H, 56)
                            if any(max(home[y][x * 3:x * 3 + 3]) < 100 for x in range(10, 120))]
                self.assertGreaterEqual(max(bar_rows) - min(bar_rows) + 1, 18)
    def test_crossed_key_is_only_a_receive_warning(self):
        plain = "rx:9991234,3100,g"
        encrypted = "rx:9991234,3100,e"
        incompatible = "rx:9991234,3100,i"
        for profile in ("", AMATEUR):
            def key_colors(script):
                frame = self.render_frame(profile + script + " expect:layout=0")
                # The operating marks: the strip's right half, left of the GPS pin
                pixels = [frame[y][x * 3:x * 3 + 3]
                          for y in range(STRIP_H - 1) for x in range(150, 245)]
                # A red slash, and a key in the strip's ink (dimmed under the slash)
                return (any(r > 140 and g < 100 and b < 100 for r, g, b in pixels),
                        any(max(r, g, b) < 70 for r, g, b in pixels))

            with self.subTest(profile=profile or "commercial"):
                for script in ("", plain, encrypted + " rxend"):
                    self.assertEqual(key_colors(script), (False, False))
                self.assertEqual(key_colors(encrypted), (True, False))
                for script in ("", plain, encrypted, incompatible + " rxend"):
                    self.assertEqual(key_colors("prototype:crypt_basic " + script), (False, True))
                self.assertEqual(key_colors("prototype:crypt_basic " + incompatible), (True, False))
    def test_encryption_key_has_its_large_bow_on_the_left(self):
        frame = self.render_frame("prototype:crypt_arc4 expect:shows=ARC4 expect:layout=0")
        key_columns = []
        for x in range(150, 245):
            count = 0
            for y in range(0, 13):  # the key, in the strip's ink; its ARC4 label is below it
                count += max(frame[y][x * 3:x * 3 + 3]) < 70
            if count:
                key_columns.append((x, count))
        middle = (key_columns[0][0] + key_columns[-1][0]) / 2
        self.assertGreater(sum(n for x, n in key_columns if x < middle),
                           sum(n for x, n in key_columns if x > middle))
    def test_profile_display_has_a_left_heading_and_roomier_rows(self):
        frame = self.render_frame("ok 8 1 expect:layout=0")
        heading = [(x, y) for y in range(STRIP_H, ROWS_Y) for x in range(8, 124)
                   if frame[y][x * 3] < 110]
        self.assertLess(min(x for x, _ in heading), 26)
        self.assertGreaterEqual(max(y for _, y in heading) - min(y for _, y in heading), 14)
        # One row height, five rows on the display: the row panels start every ROW_H pixels.
        self.assertEqual(row_starts(frame, 16), [ROWS_Y + 2 + ROW_H * k for k in range(5)])
        self.run_script("ok 8 1 expect:count=13 expect:focus=Theme up expect:focus=Low_battery_warning "
                        "up expect:focus=Auto_power_off up expect:focus=Battery_saver "
                        "up expect:focus=Language up expect:focus=Talkgroup_name_first expect:layout=0")
    def test_settings_grid_icons_digits_and_pages(self):
        first = self.render_frame("ok 8 expect:layout=0")
        display = self.render_frame("ok 8 1 expect:layout=0")

        def inked(frame, top, bottom, left, width=24):
            return any(max(frame[y][x * 3:x * 3 + 3]) < 70 for y in range(top, bottom) for x in range(left, left + width))

        # Two equal columns of tiles, three a column, the icon at the tile's left (its 24 px canvas from 4 px in)
        left_icon, right_icon = 7 + 4, 7 + 144 + 8 + 4
        for k in range(1, 3):
            y = ROWS_Y + TILE_H * k
            self.assertTrue(inked(first, y, y + TILE_H, left_icon))
            self.assertTrue(inked(first, y, y + TILE_H, right_icon))
        # Shortcut 3 (Audio, one line) belongs in its tile's vertical center, its digit right of the icon
        row3 = ROWS_Y + 2 * TILE_H
        digit_rows = [y for y in range(row3, row3 + TILE_H) for x in range(37, 45)
                      if max(first[y][x * 3:x * 3 + 3]) < 150]
        self.assertLess(abs(sum(digit_rows) / len(digit_rows) - (row3 + TILE_H / 2)), 4)
        # The privacy key (6, the second column's third tile) has its bow on the left
        key_sides = [set(), set()]
        for y in range(row3, row3 + TILE_H):
            for x in range(right_icon, right_icon + 24):
                if max(first[y][x * 3:x * 3 + 3]) < 70:
                    key_sides[x >= right_icon + 10].add(y)
        self.assertGreater(len(key_sides[0]), len(key_sides[1]))
        # Display's rows have no icons
        self.assertFalse(inked(display, ROWS_Y, HINT_Y, 12, 6))
        # Two pages of six: groups 1-9 and Radio info (no digit), in shortcut order down the left column, then the
        # right; the position n/N in the title, the digits and Find in the hint row, OK and Back on the bar
        self.run_script("ok 8 expect:shows=1/10 expect:shows=OK expect:shows=Back expect:shows=1-9_open "
                        "expect:shows=Display expect:hides=Programming expect:layout=0 "
                        "down down down down down expect:focus=Privacy "
                        "down expect:focus=Security_&_keypad_lock expect:shows=Programming expect:shows=Radio_info "
                        "expect:layout=0 up up up up up up up expect:focus=Radio_info "
                        "expect:layout=0 down expect:focus=Display "
                        "7 expect:screen=group expect:shows=Security_&_keypad_lock "
                        "back expect:focus=Security_&_keypad_lock")
        # Security & keypad lock: whole in its tile (two lines of the 8x16 font) and as its group's title
        self.run_script("ok 8 7 expect:shows=Security_&_keypad_lock expect:layout=0")
        self.run_script("ok 8 up expect:shows=Security_& expect:shows=keypad_lock expect:layout=0")

    def test_profile_settings_submenus_use_roomy_headings(self):
        groups = [("8 1", 13), ("8 2", 6), ("8 3", 3), ("8 4", 8), ("8 5", 4), ("8 6", 2), ("8 7", 2), ("8 8", 11),
                  ("8 9", 7), ("8 up ok", 6), ("8 2 row:Alert_tones ok", 8), ("8 2 row:Signaling_tones ok", 9),
                  ("6 4", 8), ("7 3", 3), ("7 5", 5)]
        for shortcut, count in groups:
            with self.subTest(shortcut=shortcut):
                self.run_script(f"ok {shortcut} expect:screen=group expect:count={count} expect:layout=0 "
                                "up expect:layout=0")
        self.run_script("ok 8 1 ok expect:screen=pick expect:layout=0")
        self.run_script("ok 8 1 row:Brightness ok expect:screen=group expect:layout=0")
        sound = self.render_frame("ok 8 2")
        heading_ink = sum(sound[y][x * 3] < 110 for y in range(39, 49) for x in range(16, 100))
        self.assertGreater(heading_ink, 75)

    def test_profile_menu_has_rounded_tinted_rows_and_icons(self):
        # The main menu: three columns of tiles in the keypad's order (1 2 3 across), inset 5 px so a third of the
        # screen holds a label like Connections in the 8x16 font
        menu = self.render_frame("ok expect:layout=0")
        top = ROWS_Y + 2  # the first tile's panel
        self.assertTrue(any(max(rgb) - min(rgb) > 70 for y in range(top, top + TILE_H - 4)
                            for x in range(12, 26) for rgb in [menu[y][x * 3:x * 3 + 3]]))
        # A tile begins with a curved corner and the unselected fill is distinctly tinted.
        self.assertEqual(menu[top][5 * 3:6 * 3], menu[top][:3])
        self.assertNotEqual(menu[top + 6][5 * 3:6 * 3], menu[top + 6][:3])
        tint = menu[top + TILE_H + 10][150 * 3:151 * 3]
        self.assertLess(tint[0], 230)
        self.assertNotEqual(tint, menu[top + TILE_H + 10][:3])
        self.run_script("ok expect:count=8 up expect:focus=Settings 8 expect:screen=settings")
        self.run_script("ok expect:shows=Menu expect:shows=Contacts expect:shows=Messages expect:shows=Call_log "
                        "expect:shows=Zones expect:shows=Scan expect:shows=Location expect:shows=Connections "
                        "expect:shows=Settings expect:hides=1/8 expect:layout=0")
        # Down follows the numbers: 1 2 3 across the first row
        self.run_script("ok expect:focus=Contacts down expect:focus=Messages down expect:focus=Call_log "
                        "down expect:focus=Zones")
        # The tiles show their values, as in every theme, on the line with the icon and the digit: unread messages and
        # a scan in their forms that fit; Zones' tile has no zone name (Home shows the zone)
        self.run_script("msg:9991234,Hello scankey wait:9000 ok "
                        "expect:shows=Messages expect:shows=1_new expect:shows=On expect:layout=0")
        self.run_script("ok expect:hides=Local_repeaters expect:shows=Off expect:layout=0")
        # tile 4: the second row's first, its first line right of the digit
        zones = self.render_frame("ok expect:layout=0")
        self.assertFalse(any(max(zones[y][x * 3:x * 3 + 3]) < 120
                             for y in range(ROWS_Y + TILE_H + 4, ROWS_Y + TILE_H + 20)
                             for x in range(46, 100)))
        self.run_script("ok 8 1 row:Language ok down ok long:back ok "
                        "expect:shows=Conexiones expect:layout=0")
        self.run_script("ok down down down down down down down expect:focus=Settings expect:layout=0")
        self.run_script("ok expect:shows=Zones 4 "
                        "expect:screen=zones expect:shows=Local_repeaters")
        settings = self.render_frame("ok up expect:focus=Settings expect:layout=0")
        # Settings is the third row's second tile
        gy, gx = ROWS_Y + 2 * TILE_H, 5 + 98 + 3 + 7
        gear = {row[x * 3:x * 3 + 3] for row in settings[gy:gy + TILE_H] for x in range(gx, gx + 13)}
        # in the accent (the focused tile), no black outline: it is the theme's ink on any other tile, Day and Night
        self.assertIn(bytes(rgb565((36, 79, 136))), gear)
        self.assertNotIn(bytes(3), gear)
    def test_inbox_hints_follow_the_focused_action(self):
        # Two labels, one per soft key: OK and Back. The # reply hint (not a soft key) is in the hint row, which follows
        # the focused row; what OK does in a list (open, compose) is its usual job and isn't repeated there
        self.run_script("ok 2 expect:shows=OK expect:hides=OK_compose expect:hides=#_reply ok expect:screen=compose")
        self.run_script("msg:9991234,Hello wait:9000 ok 2 expect:shows=OK expect:shows=Back expect:shows=#_reply down "
                        "expect:shows=OK expect:hides=OK_compose expect:hides=#_reply expect:layout=0")
    def test_profile_menu_sections_use_roomy_headings_and_rows(self):
        sections = (
            ("ok 1", "Contacts", "contacts", 31),
            ("msg:9991234,Meet_at_the_north_lot wait:9000 ok 2", "Messages", "messages", 2),
            ("call:9991234,3100,g wait:11000 ok 3", "Call_log", "recent", 1),
            ("ok 4", "Zones", "zones", 5),
            ("ok 5", "Scan", "scan-menu", 2),
            ("ok 6", "Location", "location", 4),
            ("ok 7", "Connections", "connections", 5),
            ("ok 8 9 ok", "Channels", "channels", 41),
        )
        top = ROWS_Y + 2
        for script, heading, screen, count in sections:
            with self.subTest(screen=screen):
                frame = self.render_frame(
                    f"{script} expect:screen={screen} expect:count={count} "
                    f"expect:shows={heading} expect:layout=0"
                )
                heading_ink = sum(frame[y][x * 3] < 110 for y in range(STRIP_H, ROWS_Y) for x in range(16, 100))
                self.assertGreater(heading_ink, 75)
                self.assertEqual(frame[top][7 * 3:8 * 3], frame[top][:3])
                self.assertNotEqual(frame[top][10 * 3:11 * 3], frame[top][:3])
                # The scroll position n/N where the list has more than five rows
                self.run_script(f"{script} down up expect:count={count} expect:layout=0 " +
                                (f"expect:shows=/{count}" if count > 5 else f"expect:hides=/{count}"))
    def test_nested_profile_lists_share_the_roomy_heading(self):
        for script, screen in (("ok 1 down ctx", "actions"), ("ok 8 9 ok ok", "channel"),
                               ("ok 4 down ctx", "zone"), ("ok 5 2", "roaming")):
            with self.subTest(screen=screen):
                frame = self.render_frame(f"{script} expect:screen={screen} expect:layout=0")
                ink = sum(frame[y][x * 3] < 110 for y in range(39, 49) for x in range(16, 100))
                self.assertGreater(ink, 75)

    def test_home_footer_separates_menu_and_quick_hints(self):
        frame = self.render_frame("expect:layout=0")

        def footer_ink(left, right):
            return sum(min(row[x * 3:x * 3 + 3]) > 220 for row in frame[211:236] for x in range(left, right))

        self.assertGreater(footer_ink(15, 90), 40)
        self.assertGreater(footer_ink(225, 296), 40)
        self.assertEqual(footer_ink(137, 174), 0)
        label_rows = [y for y in range(202, 236)
                      if any(min(frame[y][x * 3:x * 3 + 3]) > 220 for x in range(15, 99))]
        self.assertGreaterEqual(max(label_rows) - min(label_rows) + 1, 18)
    def test_theme_switch_applies_the_colors_again(self):
        # Colors belongs to the reference theme. Light set in Original, then Panels and back to Original: the switch
        # applies the appearance settings again, so Home is light, not the dark it would keep without that
        light = ("ok 8 1 ok row:Original ok row:Colors ok row:Light ok up ok row:Panels ok ok row:Original ok "
                 "long:back expect:screen=home")
        for x, y in ((250, 130), (150, 180)):
            self.assertEqual(self.render_frame(light)[y][x * 3:x * 3 + 3], b"\xff\xff\xff")
        self.assertEqual(self.render_frame("ok 8 1 ok row:Original ok long:back")[130][750:753], b"\x00\x00\x00")

    def test_find_lists_settings_before_radio_info(self):
        # Find's three settings rows: "GPS" lists GPS clock, GPS and Answer GPS requests, Radio info's GPS value
        # after them and so left out
        self.run_script("4 7 7 row:Answer_GPS_requests ok expect:screen=group expect:focus=Answer_GPS_requests")

    def test_menu_pages_and_footer_match_home(self):
        # The bar names the soft keys (OK, Back); the hint row above it has the rest of the hint
        self.run_script("ok expect:shows=OK expect:shows=Back expect:shows=1-8_open expect:shows=Settings "
                        "down down down down down down down expect:focus=Settings ok expect:screen=settings "
                        "expect:shows=OK expect:shows=1-9_open 1 expect:screen=group "
                        "expect:shows=OK expect:hides=OK_choose ok expect:screen=pick "
                        "expect:shows=OK expect:hides=OK_pick")
        home = self.render_frame("")
        for screen in ("ok", "ok 8", "ok 8 1", "ok 8 1 ok"):
            frame = self.render_frame(screen + " expect:layout=0")
            for y in range(198, 240):
                self.assertEqual(frame[y][450:453] == home[216][450:453], BAR_Y <= y < 236)
    def test_strip_marks_stay_inside_the_pane(self):
        # The strip's pane covers y 2 to 28 (inside the 29 px strip, 2 px of the screen's color above it). Every mark,
        # the battery's cap included, is inside it with 2 px clear above and below: rows 0-1 are the screen's color
        # and rows 2-3 and 27-28 the pane's, from the marks' left edge to their right one. Home and a list, both
        # layouts, Day and Night, an empty strip and two crowded ones (8x16 mode word, the clock, privacy, the lock)
        crowds = ("", GPS_CLOCK + "long:back gps:fix mon prototype:crypt_arc4 msg:9991234,Hello wait:9000 scankey "
                  "wait:3000 lockkey wait:3000 rx:9991234,3100,g battery:5 wait:2000 ",
                  GPS_CLOCK + "long:back gps:fix mon vox:on roam:searching prototype:bt_connected "
                  "prototype:record_on prototype:dstar_on prototype:crypt_aes msg:9991234,Hello rxonly:on scankey "
                  "wait:9000 ")
        for profile in ("", AMATEUR, NIGHT, AMATEUR + NIGHT):
            for crowd in crowds:
                for screen in ("", "ok 1"):
                    frame = self.render_frame(profile + crowd + screen)
                    surround, pane = frame[0][:3], frame[15][6 * 3:7 * 3]
                    for y in (0, 1, 2, 3, STRIP_H - 2, STRIP_H - 1):
                        want = surround if y < 2 else pane
                        stray = [x for x in range(10, len(frame[y]) // 3 - 10) if frame[y][x * 3:x * 3 + 3] != want]
                        self.assertEqual(stray, [], f"{profile + crowd + screen!r}: row {y}")
    def test_strip_marks_stay_inside_the_pane_while_they_move(self):
        # The marks that move stay in the pane at every step: the scan zigzag's gap (13 steps of 100 ms) and the
        # roaming dot (8 of 125 ms). Before step 5 two roaming frames reached rows 3 and 27
        for profile in ("", NIGHT):
            for start, steps, ms in (("scankey wait:2000 ", 13, 100), ("roam:searching wait:2000 ", 8, 125)):
                for k in range(steps):
                    script = profile + start + f"wait:{k * ms}"
                    frame = self.render_frame(script)
                    surround, pane = frame[0][:3], frame[15][6 * 3:7 * 3]
                    for y in (0, 1, 2, 3, STRIP_H - 2, STRIP_H - 1):
                        want = surround if y < 2 else pane
                        stray = [x for x in range(10, len(frame[y]) // 3 - 10) if frame[y][x * 3:x * 3 + 3] != want]
                        self.assertEqual(stray, [], f"{script!r}: row {y}")

    def test_tile_icons_stay_inside_their_tiles_in_the_theme_ink(self):
        # The Menu (3x3, tiles 39 px high and 2 px apart) and both Settings pages, Day and Night: each tile's first and
        # last rows are clear across its icon, and its icon (with the digit and the label beside and under it) is in
        # the theme's ink, the focused tile's in the accent: no tints, no fixed colors (the gear's black outline)
        menu, settings = ((5, 102), (106, 203), (207, 304)), ((7, 150), (159, 302))
        for profile, key in (("", ""), (NIGHT, "night")):
            fg, accent = PANELS_INK[key][:2]
            for screen, spans, right in (("ok", menu, 28), ("ok 8", settings, 30),
                                         ("ok 8 down down down down down down", settings, 30)):
                frame = self.render_frame(profile + screen)
                if screen == "ok":
                    self.assertEqual(panel_runs(frame, 10), [(60, 98), (101, 139), (142, 180)])
                for x0, x1 in spans:
                    for top, bottom in panel_runs(frame, x0 + 5):
                        panel = pixel(frame, x1 - 4, top + 2)
                        focus = pixel(frame, x0 + 1, (top + bottom) // 2) == accent
                        where = f"{profile + screen!r}: tile at {x0},{top}"
                        for y in (top, bottom):
                            stray = [x for x in range(x0 + 5, x0 + right) if pixel(frame, x, y) != panel]
                            self.assertEqual(stray, [], f"{where}: row {y}")
                        for y in range(top + 1, bottom):
                            for x in range(x0 + 4, x0 + right):
                                p = pixel(frame, x, y)
                                self.assertTrue(near_segment(p, panel, fg) or (focus and near_segment(p, panel, accent)),
                                                f"{where}: {p} at {x},{y}")

    def test_strip_privacy_key_in_the_theme_ink(self):
        # The privacy key and its AES label in the strip's ink, Day and Night (it was green); without a usable key on an
        # encrypted call, the key in the dim ink with a red slash
        for profile, key in (("", ""), (NIGHT, "night")):
            fg, _, dim, tx = PANELS_INK[key]
            for script, warning in (("prototype:crypt_aes", False), ("prototype:crypt_aes ok 1", False),
                                    ("rx:9991234,91,e", True)):
                frame = self.render_frame(profile + script)
                pane, red = frame[15][6 * 3:7 * 3], 0
                for y in range(4, STRIP_H - 2):
                    for x in range(10, len(frame[y]) // 3 - 10):
                        p = pixel(frame, x, y)
                        ok = near_segment(p, pane, fg) or (warning and (near_segment(p, pane, dim) or
                                                                       near_segment(p, pane, tx, 30)))
                        self.assertTrue(ok, f"{profile + script!r}: {p} at {x},{y}")
                        red += warning and near_segment(p, tx, tx, 30)
                if warning:
                    self.assertGreaterEqual(red, 10, f"{profile + script!r}: no red slash")

    def test_menu_status_uses_larger_mode_and_device_marks(self):
        for screen in ("ok", "ok 8", "ok 8 1 ok"):
            frame = self.render_frame("gps:fix " + screen + " expect:layout=0")
            for x0, x1, height in ((5, 60, 14), (246, 270, 15), (277, 305, 22)):
                ink = [y for y in range(1, 28) if any(
                    max(frame[y][x * 3:x * 3 + 3]) < 180 for x in range(x0, x1))]
                self.assertGreaterEqual(max(ink) - min(ink) + 1, height)
        self.run_script("gps:fix prototype:bt_connected mon vox:on roam:ready "
                        "msg:9991234,Hello wait:9000 prototype:crypt_aes ok 8 "
                        "rx:9991234,3100,g,Morgan_Reed expect:shows=DMR expect:hides=RX "
                        "expect:shows=AES expect:layout=0")

    def test_submenu_rows_align_with_main_menu_edges(self):
        # The main menu's tiles span x 5 to 304 (three columns that hold Connections in the 8x16 font)
        frame = self.render_frame("ok expect:layout=0")
        for outside, inside in ((4, 5), (305, 304)):
            self.assertTrue(any(row[outside * 3:outside * 3 + 3] != row[inside * 3:inside * 3 + 3]
                                for row in frame[75:180]))
        # Every list's rows and the Settings grid span x 7 to 302
        for screen in ("ok 1", "ok 1 down ctx", "ok 2",
                       "call:9991234,3100,g wait:11000 ok 3", "ok 4", "ok 4 down ctx",
                       "ok 5", "ok 5 2", "ok 6", "ok 7", "ok 8", "ok 8 1", "ok 8 1 ok", "ok 8 9 ok", "ok 8 9 ok ok"):
            with self.subTest(screen=screen):
                frame = self.render_frame(screen + " expect:layout=0")
                for outside, inside in ((6, 7), (303, 302)):
                    self.assertTrue(any(
                        row[outside * 3:outside * 3 + 3] != row[inside * 3:inside * 3 + 3]
                        for row in frame[75:180]))

    def test_soft_key_bar_has_two_labels(self):
        # Exactly two labels, one per soft key, OK and Back, wherever the hint declares them by its structure; the
        # groups for other keys (#, 0-9, PTT, hold OK) are in the hint row above the bar
        for script, row in (("ok 1", "#_more"), ("ok 8 9 ok ok", "Hold_OK_save"), ("ok 4", "#_channels"),
                            ("ok 8 9 ok", "0-9_find"), ("msg:9991234,Hello wait:9000 ok 2", "#_reply")):
            with self.subTest(screen=script):
                self.run_script(f"{script} expect:shows=OK expect:shows=Back expect:shows={row} expect:layout=0")
                frame = self.render_frame(script)
                # The bar's labels: white ink at its two ends only, none between them
                ink = [x for x in range(8, 302) if any(min(frame[y][x * 3:x * 3 + 3]) > 220 for y in range(206, 232))]
                self.assertTrue(ink and min(ink) < 60 and max(ink) > 250)
                self.assertFalse([x for x in ink if 130 < x < 170])
        # A hint without an OK group: Back alone on the bar, the PTT and # groups in the hint row
        self.run_script("call:9991234,3100,g wait:11000 ok 3 expect:shows=Back expect:shows=PTT_talk "
                        "expect:shows=#_more expect:layout=0")
        # Quick: OK and Back act there; its hint (0 All settings, hold OK) in the hint row
        self.run_script("quick expect:shows=OK expect:shows=Back expect:shows=Hold_OK_save "
                        "expect:shows=All_settings expect:layout=0")
        # The value editor: OK and Back on the bar, save and cancel in the hint row (not "Back cancel" on the bar)
        self.run_script("ok 8 1 row:Brightness ok expect:shows=OK expect:shows=Back expect:shows=OK_save "
                        "expect:shows=Back_cancel expect:layout=0")

    def test_banner_over_dual_watch_cuts_no_line(self):
        # A banner over dual watch Home starts in a gap between lines: no line of the panes or the hint row is partly
        # under it (the first layout attempt cut pane A's second line), in both layouts and both palettes
        for profile in ("", AMATEUR):
            for theme in ("", NIGHT):
                with self.subTest(profile=profile or "Commercial", theme=theme or "Day"):
                    self.run_script(profile + theme + "quick 7 ctx 3 5 ok wait:3000 expect:dual=1 battery:5 wait:2000 "
                                    "expect:shows=charge_soon expect:layout=0")

    def test_quick_words_fit_a_third_of_the_screen(self):
        # Tile 8 is the decided name without the spaces (TG/Contact, 80 px); tile 5 on DMR "DMR monitor" before the
        # tile's abbreviation (DMon)
        self.run_script("quick expect:shows=TG/Contact expect:shows=DMR_monitor expect:hides=DMon expect:layout=0")

    def test_list_hint_row_keeps_what_ok_does_where_it_does_more(self):
        # A list's plain open and choose aren't repeated in the hint row; a key that does more there is kept, the list
        # saying so by a flag for OK and one for Back (SF_OK_ACTS, SF_BACK_ACTS) or the hint function for its row:
        # (screen, its script, groups shown, groups hidden)
        cases = (
            ("compose", "ok 2 ok", ["OK_send"], []),
            ("zones", "ok 4", ["OK_go"], []),
            ("contacts", "ok 1", ["OK_talk"], []),
            ("picker", "quick 8", ["OK_use"], []),
            ("chzones", "ok 8 9 ok ok up up up up ok", ["OK_in/out"], []),
            ("channel", "ok 8 9 ok ok", ["OK_change", "OK_save"], []),
            ("contactedit", "ok 1 ok", ["OK_change", "OK_save"], []),
            ("discard", "ok 8 9 ok ok down down down down down down down down ok back", ["Back_keep_editing"],
             ["OK_choose"]),
            ("dual_pick", "quick 7 ctx 3", ["OK_watch"], []),
            ("fixed-pos", "ok 6 4 row:Fixed_position ok", ["OK_change"], []),
            ("zone_share_to", "ok 4 ctx down down ctx 5", ["OK_send"], []),
            ("dmr-id", "ok 8 9 row:DMR_ID_and_name ok", ["OK_edit/save"], []),
            ("hwopt", "hwopt ok 8 9 row:Hardware ok", ["OK_next"], []),
            ("group", "ok 8 1 row:GPS_clock", ["OK_change"], []),
            # plain: open, choose, details (an open); the other keys' groups stay
            ("channels", "ok 8 9 ok", ["#_go"], ["OK_details"]),
            ("group", "ok 8 9", [], ["OK_open"]),
            ("group", "ok 8 1", [], ["OK_choose"]),
            ("messages", "msg:9991234,Hello wait:9000 ok 2", ["#_reply"], ["OK_open"]),
        )
        for screen, script, shown, hidden in cases:
            with self.subTest(screen=screen, script=script):
                # (a screen name with "_" can't be checked: the simulator reads "_" in an expected value as a space)
                check = "" if "_" in screen else f"expect:screen={screen} "
                self.run_script(f"{script} {check}expect:shows=OK expect:shows=Back " +
                                "".join(f"expect:shows={g} " for g in shown) +
                                "".join(f"expect:hides={g} " for g in hidden) + "expect:layout=0")

    def test_hint_row_off_keeps_the_soft_keys(self):
        # Hint row off hides only the hint line: the bar and its two labels stay on every screen, and lists keep five
        # rows
        off = "ok 8 1 row:Hint_row ok long:back "
        self.run_script(off + "expect:shows=Menu expect:shows=Back expect:layout=0 "
                        "ok 1 expect:shows=OK expect:shows=Back expect:hides=#_more expect:layout=0")
        self.run_script(off + "quick expect:shows=OK expect:shows=Back expect:hides=Hold_OK_save expect:layout=0")
        # ... and on the screens drawn through ui_draw_bottom (the GPS pages, text pages, volume, freq trim)
        self.run_script(off + "ok 6 1 expect:screen=gps expect:shows=Back expect:layout=0")
        frame = self.render_frame(off + "ok 8 1")
        self.assertEqual(row_starts(frame, 16), [ROWS_Y + 2 + ROW_H * k for k in range(5)])
        on = self.render_frame("ok 8 1")
        for y in range(BAR_Y + 2, 234):
            self.assertEqual(frame[y][150 * 3:151 * 3], on[y][150 * 3:151 * 3])

    def test_home_menu_label_wherever_ok_opens_the_menu(self):
        # OK opens the menu on Home in the reply window, while scanning, in dual watch and in digital monitor: the left
        # label says Menu, whatever the hint names; the state's other groups are in the hint row
        for script, row in (("call:9991234,91,g ", "#_caller"), ("scankey wait:2000 ", "#_skip"),
                            ("quick 7 ctx 3 5 ok wait:3000 ", "#_options"), ("mon wait:3000 ", None)):
            with self.subTest(script=script):
                self.run_script(script + "expect:shows=Menu expect:shows=Back " +
                                (f"expect:shows={row} " if row else "") + "expect:layout=0 ok expect:screen=hub")
        # Keys locked: the bar has the unlock sentence in large text (as in step 2), Hint row on or off; the sentence is
        # the bar's, so the channel pane reaches the bar as with no hint row
        for off in ("", "ok 8 1 row:Hint_row ok long:back "):
            self.run_script(off + "lockkey wait:3000 expect:shows=to_unlock expect:hides=Menu expect:layout=0")
        locked, unlocked = self.render_frame("lockkey wait:3000"), self.render_frame("")
        for y in range(HINT_Y - 2, BAR_Y - 4):
            self.assertEqual(locked[y][20 * 3:21 * 3], unlocked[y][20 * 3:21 * 3], y)

    def test_hint_structure_survives_translation(self):
        # The bar finds the OK and Back keys' groups by structure (ui_hint_split's two-space groups, a group starting
        # with the key's name), so every catalog's hint keeps the English form's groups and their keys: OK as it is,
        # Back as that catalog's HINT_BACK
        import re
        lang_dir = Path(__file__).resolve().parents[2] / "core/ui/lang"

        def parse(path):
            out, cur, key = {}, {}, None
            for line in path.read_text(encoding="utf-8").splitlines():
                m = re.match(r'(msgctxt|msgid|msgstr) "(.*)"$', line)
                if m:
                    key = m.group(1)
                    cur[key] = m.group(2)
                    if key == "msgstr":
                        out[cur.get("msgctxt")] = (cur.get("msgid"), cur["msgstr"])
                        cur = {}
                elif line.startswith('"') and key:
                    cur[key] = cur.get(key, "") + line[1:-1]
            return out

        def forms(text):
            m = re.fullmatch(r"\{(.*)\}", text)
            return m.group(1).split("|") if m else [text]

        def keys(form, back):
            return [("OK" if g == "OK" or g.startswith("OK ") else "Back" if g.startswith(back) else "-")
                    for g in form.split("  ") if g]

        for po in sorted(lang_dir.glob("*.po")):
            catalog = parse(po)
            back = catalog["HINT_BACK"][1]
            for key, (english, text) in catalog.items():
                if not key or "HINT" not in key or key == "SET_LABEL_HINT_ROW" or not text:
                    continue
                with self.subTest(catalog=po.name, key=key):
                    want = [keys(f, "Back") for f in forms(english)]
                    for form in forms(text):
                        self.assertIn(keys(form, back), want, f"{text!r} for {english!r}")

    def run_script(self, script, geometry=GEOMETRY):
        run = subprocess.run([SIM, "--geom", geometry, "--script", script],
                             capture_output=True, text=True, errors="replace", timeout=15)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_display_choices_are_native_and_survive_reboot(self):
        # Display profile and Day/Night open a pick list with the value in use checked; GPS clock flips on OK
        self.run_script(
            "ok 8 1 expect:shows=Display_profile expect:hides=GUI_profile down ok expect:screen=pick "
            "expect:shows=Commercial expect:shows=Amateur expect:focus=Commercial down ok expect:screen=group "
            "down ok expect:shows=Day expect:shows=Night down ok "
            "down expect:focus=GPS_clock ok expect:shows=On reboot "
            "expect:screen=home ok 8 1 expect:shows=Amateur expect:shows=Night down down down expect:focus=GPS_clock "
            "expect:shows=On"
        )
        self.run_script("ok 8 1 expect:hides=Display_profile expect:shows=Colors", "320x240")
        # Theme, the first row where more than one theme fits: the one in use checked, and choosing another puts it in
        # use at once, its own appearance rows after the Theme row, and is kept across a restart
        self.run_script("ok 8 1 expect:focus=Theme expect:shows=Panels ok expect:screen=pick expect:checked=Panels "
                        "row:Original ok expect:screen=group expect:focus=Theme expect:shows=Original "
                        "expect:shows=Colors expect:hides=Day/Night reboot expect:screen=home "
                        "ok 8 1 expect:shows=Original expect:shows=Colors ok row:Panels ok expect:shows=Day/Night "
                        "expect:hides=Colors")
        # A display panels doesn't fit (150 x 120) has no Theme row; one it fits in its compact layout (220 x 176) has it
        self.run_script("ok 8 1 expect:hides=Theme expect:focus=Colors", "160x120i5")
        self.run_script("ok 8 1 expect:focus=Theme expect:shows=Original", "220x176")

    def test_new_radio_default_writes_nothing(self):
        # A start that finds no settings of its own (a new radio, or a read of the newest record that failed) turns
        # Talkgroup name first on as a default: in force (Amateur's talk line leads with the name), but no setting is
        # written, so nothing is saved over a record that may still be there (radioCoreTest has the store's side)
        self.run_script("expect:setwrites=0 " + AMATEUR + "expect:shows=USA_ reboot expect:setwrites=0")

    def test_talkgroup_order_key_and_setting_in_both_themes(self):
        # Talkgroup name first (the key and Settings > Display) works in every theme. A new radio that starts in the
        # panels theme starts with it on: Amateur Home's talk line reads "USA · TG 3100", then "TG 3100 · USA". One
        # whose display picks the reference theme starts with it off, panels built in or not (160x120i5), as does the
        # simulator's reference class (320x240). The key's toast says the new order
        for geometry, toast in ((GEOMETRY, "Talkgroup_number_first"), ("320x240", "Talkgroup_name_first"),
                                ("160x120i5", "Talkgroup_name_first")):
            with self.subTest(geometry=geometry):
                self.run_script("labelkey expect:toast=" + toast, geometry)
        # Only a new radio: after a restart the stored choice stays, off here
        self.run_script("labelkey reboot labelkey expect:toast=Talkgroup_name_first")
        # A factory reset sets it as the theme the radio restarts in (the build's default where it fits) asks, whatever
        # theme is in use when it is reset
        for geometry, script, want in ((GEOMETRY, "", "1"), (GEOMETRY, "ok 8 1 ok row:Original ok long:back ", "1"),
                                       ("160x120i5", "", "0"), ("220x176", "", "1")):
            with self.subTest(geometry=geometry, script=script):
                self.run_script(script + "ok 8 9 up ok down ok expect:resetnamefirst=" + want, geometry)
        self.run_script(AMATEUR + "expect:shows=USA_ expect:hides=3100_ "
                        "labelkey expect:shows=3100_ expect:hides=USA_ "
                        "ok 8 1 row:Talkgroup_name_first expect:shows=Off ok long:back expect:shows=USA_")
        after_call_back = "ok 8 8 row:SK3_hold ok " + "down " * 14 + "ok "
        for geometry in (GEOMETRY, "320x240"):
            with self.subTest(geometry=geometry):
                self.run_script(after_call_back + "expect:set47=12 ok down ok expect:set47=19", geometry)

    def test_gps_clock_obeys_fix_and_native_receiver_setting(self):
        self.run_script(
            GPS_CLOCK + "back back back expect:hides=14:08 "
            "gps:fix expect:shows=14:08 ok 8 1 expect:shows=14:08 back back back "
            "gps:searching expect:hides=14:08 gps:fix "
            "ok 6 4 expect:focus=GPS ok back back back expect:hides=14:08"
        )
        setup = GPS_CLOCK + "long:back gps:fix "
        for profile in ("", AMATEUR):
            with self.subTest(profile=profile or "Commercial"):
                clock = self.render_frame(profile + setup +
                                          "expect:shows=21_Nov expect:shows=14:08 expect:layout=0")
                plain = self.render_frame(profile + "gps:fix expect:layout=0")
                # In the status strip, as on every screen; nothing under the strip changes, compared by ink: the
                # clock's anti-aliased shades can leave the 16-color palette fewer blends for the edges below
                self.assertNotEqual(clock[:STRIP_H], plain[:STRIP_H])
                self.assertEqual(ink_map(clock[STRIP_H:]), ink_map(plain[STRIP_H:]))
                # ... and on a list
                self.run_script(profile + setup + "ok 1 expect:shows=21_Nov expect:shows=14:08 expect:layout=0")
    def test_profiles_keep_call_context_on_home(self):
        # The channel's name is the main line. Commercial shows the talk target only where it isn't the channel's own;
        # a call shows the caller and the talkgroup; Amateur shows the target and the details always
        self.run_script(
            "expect:centered=COUNTY_DMR expect:centered=Local_repeaters expect:hides=USA expect:hides=TS1 "
            "rx:9991234,3100,g,Morgan_Reed expect:centered=COUNTY_DMR expect:centered=Morgan_Reed "
            "expect:shows=TG_3100 expect:hides=Receiving rxend expect:hides=Morgan_Reed "
            + AMATEUR + "expect:centered=COUNTY_DMR expect:centered=USA expect:shows=TG_3100 "
            "expect:shows=TS1"
        )
        # The reply window's target differs from the channel's: Commercial shows it
        self.run_script("call:9991234,91,g expect:centered=COUNTY_DMR expect:centered=World")
    def test_layout_with_crowded_status_bar_and_notifications(self):
        for theme in ("", NIGHT):
            with self.subTest(theme="Night" if theme else "Day"):
                self.run_script(
                    theme + "expect:layout=0 "
                    "quick 7 ctx 3 5 ok wait:3000 "
                    "expect:dual=1 expect:shows=USA expect:hides=TS1 expect:hides=TG_3100 expect:hides=MHz "
                    "mondown monup vox:on rxonly:on roam:searching prototype:bt_connected "
                    "prototype:record_on prototype:dstar_on prototype:crypt_aes "
                    "prototype:talkaround_on msg:9991234,Hello expect:layout=0 "
                    "wait:9000 expect:layout=0"
                )
        self.run_script(
            GPS_CLOCK + "long:back gps:fix mon vox:on roam:searching "
            "prototype:bt_connected prototype:record_on prototype:dstar_on prototype:crypt_arc4 "
            "prototype:talkaround_on msg:9991234,Hello rxonly:on scankey wait:9000 "
            "expect:shows=D-STAR expect:shows=ARC4 expect:shows=14:08 expect:layout=0"
        )
        # The clock comes right after the states every theme shows (QA, step 4 review, B1): with privacy, an unread
        # message and a scan it stays, the mode word taking the smaller font first
        self.run_script(GPS_CLOCK + "long:back gps:fix msg:9991234,Hello wait:9000 prototype:crypt_aes scankey "
                        "wait:3000 expect:shows=14:08 expect:layout=0 ok 1 expect:shows=14:08 expect:layout=0")
        # ... and a list strip whose mode word must take 8x16 for the clock to fit: it does, before the clock goes
        self.run_script(GPS_CLOCK + "long:back gps:fix mon msg:9991234,Hello wait:9000 prototype:crypt_aes "
                        "scankey wait:3000 ok 1 lockkey wait:3000 expect:screen=contacts expect:shows=14:08 "
                        "expect:layout=0")

    def test_home_clock_moves_to_the_mode_bar_in_a_crowd(self):
        # Where Home's strip has no room for the clock, it is at the mode bar's right end (as in step 2); with room,
        # in the strip. A list can drop it in the worst crowd. With step 5's marks Home's strip holds the clock beside
        # every state ranked before it (receiving with signal bars, the monitor, ARC4 privacy, an unread message, a scan
        # and the lock), so the mode bar keeps no clock there; with NO TX as well it does. A list's strip, which has the
        # mode word too, drops it
        crowd = ("gps:fix mon prototype:crypt_arc4 msg:9991234,Hello wait:9000 scankey wait:3000 lockkey wait:3000 "
                 "rx:9991234,3100,g ")
        setup = GPS_CLOCK + "long:back "
        for profile in ("", AMATEUR):
            with self.subTest(profile=profile or "Commercial"):
                def clock_in_mode_bar(script):
                    # The mode bar's right half, with the clock and without (the strip's scan mark moves with time)
                    frame = self.render_frame(profile + setup + script + "expect:shows=14:08 expect:layout=0")
                    plain = self.render_frame(profile + script)
                    return any(frame[y][160 * 3:300 * 3] != plain[y][160 * 3:300 * 3] for y in range(STRIP_H + 2, 54))
                self.assertFalse(clock_in_mode_bar("gps:fix "), "the clock in the strip")
                self.assertFalse(clock_in_mode_bar(crowd), "the clock in the crowded strip")
                # NO TX (PTT on a receive-only channel) joins that crowd: the strip has no room left, the mode bar has it
                self.assertTrue(clock_in_mode_bar(crowd + "rxonly:on pttdown "), "the clock in the mode bar")
        self.run_script(setup + crowd.replace("lockkey wait:3000 ", "") + "ok 1 expect:screen=contacts "
                        "expect:hides=14:08 expect:layout=0")

    def test_strip_words_in_12x24_stay_clear_of_home_mode_bar(self):
        # TX on Home is a 12x24 word in the strip: its cell ends above the mode bar's text (934d80c5), both layouts
        for profile in ("", AMATEUR):
            self.run_script(profile + "pttdown expect:shows=TX expect:layout=0")

    def test_strip_words_take_8x16_before_an_item_drops(self):
        # On a list (the strip has the mode word) VOX, RX, the monitor and the clock fit only with the words in 8x16:
        # the words shrink, and VOX, the last of them, stays
        self.run_script(GPS_CLOCK + "long:back gps:fix vox:on rxonly:on mon ok 1 expect:screen=contacts "
                        "expect:shows=VOX expect:shows=RX expect:shows=14:08 expect:layout=0")

    def test_scan_gap_moves_along_the_zigzag(self):
        # The scan mark's 13 steps of 100 ms (a gap moving along the stroke, then the arrowhead out) are 13 different
        # strips, Day and Night
        for profile in ("", NIGHT):
            strips = {tuple(self.render_frame(profile + f"scankey wait:2000 wait:{k * 100}")[2:STRIP_H - 1])
                      for k in range(13)}
            self.assertEqual(len(strips), 13, f"{profile!r}: the scan mark doesn't step")

    def test_strip_marks_keep_their_gap(self):
        # The strip's icons start at their ink, the canvas offset taken off (panels_strip_icon): from the scan mark,
        # the unread mail, the lock and the pin to the power letter and the battery, each pair of neighbors is the
        # strip's 4 px gap apart (5 where an anti-aliased edge fades)
        for script in ("gps:fix msg:9991234,Hello wait:9000 lockkey wait:3000",
                       "gps:fix mon msg:9991234,Hello wait:9000 scankey wait:3000"):
            frame = self.render_frame(script)
            pane = frame[15][6 * 3:7 * 3]
            inked = [any(frame[y][x * 3:x * 3 + 3] != pane for y in range(4, STRIP_H - 2)) for x in range(155, 302)]
            runs = [x + 155 for x in range(len(inked)) if inked[x] and (x == 0 or not inked[x - 1])]
            ends = [x + 155 for x in range(len(inked)) if inked[x] and (x == len(inked) - 1 or not inked[x + 1])]
            gaps = [start - end - 1 for end, start in zip(ends, runs[1:])]
            self.assertGreaterEqual(len(gaps), 3, f"{script!r}: {runs}")
            self.assertTrue(all(4 <= g <= 5 for g in gaps), f"{script!r}: gaps {gaps}")

    @unittest.skipUnless(SIM_ZH.exists(), "uiSimZh not built (make -C tests/host uiSimZh)")
    def test_chinese_bar_labels_at_their_height_and_unlock_sentence_centered(self):
        # A bar with Chinese text puts both labels in the 8x16 font, centered on the bar (OK too: its ink is about
        # 11 px high, in 12x24 or larger more); Home's unlock sentence is centered on the bar
        bar = range(BAR_Y, BAR_Y + 32)
        for script in ("", "ok", "ok 8"):
            frame = self.render_frame(script, SIM_ZH, "zh")
            ground = frame[BAR_Y + 2][8 * 3:9 * 3]
            rows = [y for y in bar if any(frame[y][x * 3:x * 3 + 3] != ground for x in range(14, 40))]
            self.assertTrue(rows and max(rows) - min(rows) + 1 <= 16, f"{script!r}: OK's ink rows {rows}")
            self.assertLessEqual(abs((min(rows) + max(rows)) / 2 - (BAR_Y + 15.5)), 4, f"{script!r}: rows {rows}")
        frame = self.render_frame("lockkey wait:5000", SIM_ZH, "zh")
        ground = frame[BAR_Y + 2][8 * 3:9 * 3]
        cols = [x for x in range(6, 304) if any(frame[y][x * 3:x * 3 + 3] != ground for y in range(BAR_Y + 4, BAR_Y + 28))]
        self.assertLessEqual(abs((min(cols) + max(cols)) / 2 - 155), 2, f"the unlock sentence spans {min(cols)}-{max(cols)}")

    def test_commercial_crowded_status_keeps_the_clock_and_device_icons(self):
        setup = (
            GPS_CLOCK + "long:back gps:fix quick 7 ctx 3 5 ok wait:3000 "
            "expect:dual=1 roam:ready prototype:bt_connected prototype:dstar_on "
            "prototype:crypt_arc4 prototype:talkaround_on lockkey wait:3000"
        )
        check = " expect:shows=21_Nov expect:shows=14:08 expect:shows=ARC4 expect:layout=0"
        # (an unread message: a mark that comes before the clock, so it has room in this crowd)
        crowded = ink_map(self.render_frame(setup + " vox:on prototype:record_on msg:9991234,Hello wait:9000" + check))
        toggled = ink_map(self.render_frame(setup + " vox:on prototype:record_on wait:9000" + check))
        self.assertTrue(crowded[:STRIP_H] != toggled[:STRIP_H], "a mark should change the strip")
        self.assertTrue(crowded[STRIP_H:BAR_Y] == toggled[STRIP_H:BAR_Y], "Home under the strip stays")
    def test_operational_activity_stays_visible(self):
        self.run_script("dlmode:p25 p25t:1 expect:shows=Finding_system")
        self.run_script("t3 dlpos:3 t3state:fallback expect:shows=Fallback")
        self.run_script("aprstx expect:shows=TX_APRS aprsend expect:hides=TX_APRS")

    def test_narrow_display_short_forms(self):
        # The 220 x 310 display (240x320i10x5) in the panels theme, chosen in Settings > Display > Theme: Connections'
        # Menu tile takes its short form, which the screen's title doesn't need; the other labels fit whole
        narrow, panels = "240x320i10x5", "ok 8 1 ok row:Panels ok long:back "
        self.run_script(panels + "ok expect:screen=hub expect:shows=Links expect:hides=Connections "
                        "expect:shows=Contacts expect:shows=Messages expect:shows=Call_log expect:shows=Location "
                        "expect:shows=Settings expect:layout=0", narrow)
        self.run_script(panels + "ok 7 expect:screen=connections expect:shows=Connections expect:layout=0", narrow)
        # Quick's tiles keep their short names there; a value too long for its place beside the digit (a CTCSS tone)
        # takes a third line, whole
        self.run_script(panels + "quick expect:shows=Slot expect:shows=TG expect:hides=Timeslot expect:layout=0",
                        narrow)
        self.run_script(panels + "down quick expect:shows=T100.0 expect:layout=0", narrow)
        # Display profile's row takes its short form beside its value; the full width keeps the long one
        self.run_script(panels + "ok 8 1 expect:shows=Profile expect:hides=Display_p expect:shows=Commercial "
                        "expect:layout=0", narrow)
        # Dual watch: a pane's line that 12x24 would cut takes 8x16, whole
        self.run_script(panels + "quick 7 wait:3000 expect:shows=County_fire expect:shows=Local_repeaters "
                        "expect:layout=0", narrow)

    def test_gui_switch_keeps_channel_and_display_preferences(self):
        self.run_script(
            "down ok 8 1 row:Display_profile ok down ok gui:original expect:screen=home "
            "expect:shows=Valley_rpt expect:freq=147060000 "
            "ok 8 1 expect:hides=Display_profile gui:prototype expect:screen=home "
            "expect:centered=Valley_rpt ok 8 1 expect:shows=Amateur"
        )

    def test_session_keeps_native_menu_state_and_reports_bad_input(self):
        with tempfile.TemporaryDirectory() as temp:
            image = Path(temp) / "ui.png"
            run = subprocess.run(
                [SIM, "--geom", GEOMETRY, "--interactive", "--out", image],
                input="ok expect:screen=hub\nnot-a-key\nexpect:screen=hub\nback expect:screen=home\n",
                capture_output=True, text=True, timeout=15,
            )
            self.assertEqual(run.returncode, 0, run.stderr)
            replies = run.stdout.splitlines()
            self.assertEqual(len(replies), 5, run.stdout)
            self.assertEqual(replies[:2], ["OK", "OK"])
            self.assertTrue(replies[2].startswith("ERROR"), run.stdout)
            self.assertEqual(replies[3:], ["OK", "OK"])
            data = image.read_bytes()
            self.assertEqual(data[:8], b"\x89PNG\r\n\x1a\n")
            self.assertEqual(int.from_bytes(data[16:20], "big"), 620)
            self.assertEqual(int.from_bytes(data[20:24], "big"), 480)


if __name__ == "__main__":
    unittest.main()
