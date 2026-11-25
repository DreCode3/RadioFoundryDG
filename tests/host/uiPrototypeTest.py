# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors
"""Exercise the display profiles through the native simulator's public entry points."""

from pathlib import Path
import subprocess
import tempfile
import unittest
import zlib


SIM = Path(__file__).resolve().with_name("uiSim")
GEOMETRY = "320x240iR10"


class PrototypeTest(unittest.TestCase):
    def test_right_soft_key_opens_quick_only_on_home(self):
        self.run_script("back expect:screen=home ok 7 8 up up "
                        "expect:focus=Right_soft_key ok ok expect:shows=Quick "
                        "back expect:screen=settings back expect:screen=hub "
                        "back expect:screen=home back expect:screen=quick back expect:screen=home "
                        "reboot back expect:screen=quick back expect:screen=home "
                        "quick expect:screen=quick")

    def test_home_footer_follows_the_right_soft_key_assignment(self):
        settings = "ok 7 8 up up "
        home = "back back back "
        self.run_script("expect:shows=Back expect:hides=*_Quick " +
                        settings + "ok ok " + home + "expect:shows=Quick expect:hides=*_Quick "
                        "expect:shows=Menu expect:hides=OK_Menu expect:layout=0 "
                        "ok expect:shows=OK expect:shows=Back")
        self.run_script(settings + "ok ok ok ok " + home +
                        "expect:shows=Power_level expect:layout=0 long:dualkey wait:2000 "
                        "expect:dual=1 expect:shows=Power_level expect:layout=0 "
                        "gui:original expect:shows=Power_level")
        for profile in ("", "ok 7 2 ok down ok long:back "):
            frame = self.render_frame(profile + settings + "ok " + home +
                                      "expect:shows=Menu expect:hides=OK_Menu "
                                      "expect:hides=Nothing expect:hides=Quick expect:layout=0")
            self.assertTrue(all(row[570:870] == row[570:573] * 100 for row in frame[208:230]))

    def render_frame(self, script):
        with tempfile.TemporaryDirectory() as temp:
            image = Path(temp) / "frame.png"
            subprocess.run([SIM, "--geom", GEOMETRY, "--scale", "1", "--out", image,
                            "--script", script], check=True, capture_output=True, timeout=15)
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
        for select in ("", "ok 7 2 ok down ok long:back "):
            with self.subTest(profile=select or "commercial"):
                def meter(script):
                    frame = self.render_frame(select + script)
                    return [row[15 * 3:38 * 3] != row[50 * 3:51 * 3] * 23 for row in frame[:20]]

                self.assertFalse(any(meter("")), "idle RSSI must not draw signal bars")
                self.assertTrue(any(meter("fmrx")), "an active FM signal must draw bars")
                self.assertFalse(any(meter("fmrx fmrxend")), "the last RSSI sample must not leave bars behind")
                self.assertTrue(any(meter("rx:9991234,3100,g,Morgan_Reed")), "a DMR call must draw bars")
                self.assertFalse(any(meter("rx:9991234,3100,g,Morgan_Reed rxend")))

    def test_gps_pin_has_an_antialiased_tip_in_both_profiles(self):
        for select, strip_end in (("", 35), ("ok 7 2 ok down ok long:back ", 27)):
            before = self.render_frame(select)
            fixed = self.render_frame(select + "gps:fix")
            rows = [[fixed[y][x * 3:x * 3 + 3] for x in range(215, 250)
                     if fixed[y][x * 3:x * 3 + 3] != before[y][x * 3:x * 3 + 3]]
                    for y in range(strip_end)]
            tip = next(row for row in reversed(rows) if row)
            self.assertLessEqual(len(tip), 3)
            self.assertTrue(all(0 < max(pixel) < 230 for pixel in tip))

    def test_profile_day_uses_vertical_battery_and_black_receive_only_mark(self):
        frame = self.render_frame("rxonly:on")
        idle = self.render_frame("")

        def black(x, y):
            return frame[y][x * 3:x * 3 + 3] == bytes(3)

        battery = [(x, y) for y in range(35) for x in range(280, 303) if black(x, y)]
        xs, ys = zip(*battery)
        self.assertLess(max(xs) - min(xs), max(ys) - min(ys))
        receive_only = {(x, y) for y in range(36, 64) for x in range(15, 300)
                        if black(x, y) and idle[y][x * 3:x * 3 + 3] != bytes(3)}
        left = min(x for x, _ in receive_only)
        # The R keeps its solid stem and the X is separated by a blank column.
        self.assertGreaterEqual(sum(x == left for x, _ in receive_only), 11)
        self.assertFalse(any(x == left + 11 for x, _ in receive_only))

    def test_monitor_uses_separate_speaker_and_sound_wave(self):
        idle = self.render_frame("")
        monitor = self.render_frame("mon wait:3000")
        pixels = {(x, y) for y in range(36, 64) for x in range(15, 300)
                  if monitor[y][x * 3:x * 3 + 3] == bytes(3) and idle[y][x * 3:x * 3 + 3] != bytes(3)}
        left = min(x for x, _ in pixels)
        # A solid cone with a stem on the left and a separate wave on the right.
        self.assertGreaterEqual(sum(x == left + 10 for x, _ in pixels), 10)
        self.assertFalse(any(x == left + 12 for x, _ in pixels))
        self.assertTrue(any(x >= left + 14 for x, _ in pixels))

        # Look for an intermediate edge color around the speaker's native pixels.
        edges = [monitor[y][x * 3:x * 3 + 3] for y in range(36, 64)
                 for x in range(left, left + 19)
                 if monitor[y][x * 3:x * 3 + 3] != idle[y][x * 3:x * 3 + 3]]
        self.assertTrue(any(0 < max(edge) < 220 for edge in edges))

    def test_talkgroup_is_larger_and_home_text_holds_position_during_rx(self):
        call = "rx:9991234,3100,g,Morgan_Reed expect:centered=Morgan_Reed expect:layout=0"
        commercial = self.render_frame("expect:centered=USA")
        title_rows = [y for y in range(115, 155)
                      if sum(commercial[y][x * 3] < 120 for x in range(110, 200)) >= 6]
        self.assertGreaterEqual(max(title_rows) - min(title_rows) + 1, 24)
        amateur = "ok 7 2 ok down ok long:back "
        for idle, receiving in ((commercial[75:158], self.render_frame(call)[75:158]),
                                (self.render_frame(amateur)[64:166],
                                 self.render_frame(amateur + call)[64:166])):
            # The indexed palette can shift antialiased edge colors by one 4-bit step.
            self.assertLessEqual(max(abs(a - b) for idle_row, rx_row in zip(idle, receiving)
                                     for a, b in zip(idle_row, rx_row)), 17)

    def test_commercial_home_has_larger_status_and_title(self):
        commercial = self.render_frame("expect:layout=0")
        amateur = self.render_frame("ok 7 2 ok down ok long:back expect:layout=0")
        # Commercial's device strip is still taller than Amateur's.
        self.assertNotEqual(commercial[35][300:303], commercial[36][300:303])
        self.assertEqual(amateur[35][300:303], amateur[36][300:303])
        ink_rows = [y for y in range(115, 166)
                    if sum(commercial[y][x * 3] < 120 for x in range(110, 200)) >= 6]
        self.assertGreaterEqual(max(ink_rows) - min(ink_rows) + 1, 29)
        frame = self.render_frame("gps:fix prototype:bt_connected lockkey wait:3000 expect:layout=0")
        heights = []
        for first_x in range(164, 304, 28):
            rows = [y for y, row in enumerate(frame[:35])
                    if any(row[x * 3:x * 3 + 3] != row[150:153]
                           for x in range(first_x, min(first_x + 28, 300)))]
            heights.append(max(rows) - min(rows) + 1)
        self.assertGreaterEqual(min(heights), 19)
        self.assertLessEqual(max(heights) - min(heights), 10)

    def test_dstar_preview_uses_full_mode_label(self):
        self.run_script("prototype:dstar_on expect:shows=D-STAR expect:layout=0 "
                        "aprstx expect:hides=D-STAR aprsend expect:shows=D-STAR "
                        "prototype:dstar_off expect:hides=D-STAR")
        for profile, top in (("", 36), ("ok 7 2 ok down ok long:back ", 29)):
            with self.subTest(profile=profile or "Commercial"):
                frame = self.render_frame(profile + "prototype:dstar_on expect:layout=0")
                # The first 60 pixels of the operating row hold only the mode.
                mode_rows = [y for y in range(top, top + 28)
                             if any(max(frame[y][x * 3:x * 3 + 3]) < 100 for x in range(15, 60))]
                self.assertGreaterEqual(max(mode_rows) - min(mode_rows) + 1, 19)
                self.assertGreaterEqual(min(mode_rows) - top, 1)
                self.assertLessEqual(max(mode_rows) - top, 26)

    def test_crossed_key_is_only_a_receive_warning(self):
        plain = "rx:9991234,3100,g"
        encrypted = "rx:9991234,3100,e"
        incompatible = "rx:9991234,3100,i"
        for profile in ("", "ok 7 2 ok down ok long:back "):
            def key_colors(script):
                frame = self.render_frame(profile + script + " expect:layout=0")
                pixels = [frame[y][x * 3:x * 3 + 3]
                          for y in range(28, 65) for x in range(65, 110)]
                return (any(r > 140 and g < 100 and b < 100 for r, g, b in pixels),
                        any(g > r + 20 and g > b + 10 for r, g, b in pixels))

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
        for x in range(65, 110):
            count = 0
            for y in range(36, 48):
                red, green, blue = frame[y][x * 3:x * 3 + 3]
                count += green > red + 20 and green > blue + 10
            if count:
                key_columns.append((x, count))
        middle = (key_columns[0][0] + key_columns[-1][0]) / 2
        self.assertGreater(sum(n for x, n in key_columns if x < middle),
                           sum(n for x, n in key_columns if x > middle))

    def test_profile_display_has_a_left_heading_and_roomier_rows(self):
        frame = self.render_frame("ok 7 2 expect:layout=0")
        heading = [(x, y) for y in range(30, 69) for x in range(8, 124)
                   if frame[y][x * 3] < 110]
        self.assertLess(min(x for x, _ in heading), 26)
        self.assertGreaterEqual(max(y for _, y in heading) - min(y for _, y in heading), 18)
        # Along the left edge of the row backgrounds, count their starts.
        section = [row[16 * 3:16 * 3 + 3] for row in frame[69:211]]
        paper = frame[180][:3]
        starts = [y for y in range(1, len(section)) if section[y - 1] == paper and section[y] != paper]
        self.assertGreaterEqual(len(starts), 4)
        self.assertGreaterEqual(starts[1] - starts[0], 32)
        self.run_script("ok 7 2 expect:count=8 up expect:focus=Language "
                        "up expect:focus=Hint_row up expect:focus=Large_text "
                        "up expect:focus=Backlight_Dim/Off expect:layout=0")

    def test_colored_icons_are_on_settings_categories_only(self):
        first = self.render_frame("ok 7 expect:layout=0")
        second = self.render_frame("ok 7 up expect:layout=0")
        display = self.render_frame("ok 7 2 expect:layout=0")

        def saturated(frame, top, bottom, left=12):
            return any(max(rgb) - min(rgb) > 70 for y in range(top, bottom)
                       for x in range(left, left + 14) for rgb in [frame[y][x * 3:x * 3 + 3]])

        def right_icon_start(frame):
            row = frame[93]
            # The right tile starts at the end of the eight-pixel gutter.
            gutter = next(x for x in range(100, 200) if row[x * 3:x * 3 + 3] == row[:3])
            return gutter + 8 + 5

        for page in (first, second):
            for left in (12, right_icon_start(page)):
                for y in range(85, 172, 43):
                    self.assertTrue(saturated(page, y, y + 13, left))
        self.assertEqual(first[93][9 * 3:10 * 3], first[93][10 * 3:11 * 3])
        self.assertLess(max(first[93][9 * 3:10 * 3]), 100)
        # Shortcut 3 belongs in the tile's vertical center.
        digit_rows = [y for y in range(159, 198) for x in range(28, 38)
                      if max(first[y][x * 3:x * 3 + 3]) < 150]
        self.assertLess(abs(sum(digit_rows) / len(digit_rows) - 178), 3)
        key_left = right_icon_start(first)
        key_sides = [set(), set()]
        for y in range(85, 103):
            for x in range(key_left, key_left + 13):
                red, green, blue = first[y][x * 3:x * 3 + 3]
                if red > blue + 40 and green > blue + 30:
                    key_sides[x >= key_left + 6].add(y)
        self.assertGreater(len(key_sides[0]), len(key_sides[1]))
        self.assertFalse(saturated(display, 77, 202))
        self.run_script("ok 7 expect:shows=1/2 expect:shows=OK expect:hides=OK_Open expect:hides=0-9_open "
                        "expect:shows=Sound expect:shows=Connections "
                        "expect:hides=Advanced expect:layout=0 "
                        "down down down down down expect:focus=Connections "
                        "down expect:focus=Power expect:shows=Power expect:hides=Power_saving "
                        "expect:shows=2/2 expect:hides=Sound "
                        "expect:shows=Advanced expect:layout=0 up expect:shows=1/2 "
                        "7 expect:screen=group expect:shows=Power expect:hides=Power_Saving "
                        "back expect:shows=2/2 expect:focus=Power")

    def test_profile_settings_submenus_use_roomy_headings(self):
        groups = [("1", 12), ("2", 8), ("3", 13), ("4", 2), ("5", 8), ("6", 10),
                  ("7", 3), ("8", 11), ("9", 2), ("0", 2), ("up up ok", 9), ("up ok", 5)]
        for shortcut, count in groups:
            with self.subTest(shortcut=shortcut):
                self.run_script(f"ok 7 {shortcut} expect:screen=group expect:count={count} expect:layout=0 "
                                "up expect:layout=0")
        self.run_script("ok 7 2 ok expect:screen=display-choice expect:layout=0")
        sound = self.render_frame("ok 7 1")
        heading_ink = sum(sound[y][x * 3] < 110 for y in range(39, 49) for x in range(16, 100))
        self.assertGreater(heading_ink, 75)

    def test_profile_menu_has_rounded_tinted_rows_and_icons(self):
        menu = self.render_frame("ok expect:layout=0")
        self.assertTrue(any(max(rgb) - min(rgb) > 70 for y in range(76, 103)
                            for x in range(20, 34) for rgb in [menu[y][x * 3:x * 3 + 3]]))
        # The inset row begins with a curved corner and the unselected fill is distinctly tinted.
        self.assertEqual(menu[75][7 * 3:8 * 3], menu[75][:3])
        self.assertNotEqual(menu[81][7 * 3:8 * 3], menu[81][:3])
        tint = menu[130][299 * 3:300 * 3]
        self.assertLess(tint[0], 230)
        self.assertNotEqual(tint, menu[130][:3])
        self.run_script("ok expect:count=7 up expect:focus=Settings 7 expect:screen=settings")
        self.run_script("ok expect:shows=Contacts expect:shows=Messages expect:shows=Recent "
                        "expect:shows=Zones expect:shows=Channels expect:shows=Tools "
                        "expect:hides=Settings expect:layout=0")
        self.run_script("scankey wait:5000 msg:9991234,Hello wait:9000 ok "
                        "expect:shows=Messages expect:shows=Tools "
                        "expect:hides=Scanning expect:hides=1_new expect:layout=0")
        self.run_script("ok 7 2 up ok down ok long:back ok "
                        "expect:shows=Herramientas expect:layout=0")
        self.run_script("ok down down down down down down expect:focus=Settings expect:layout=0")
        self.run_script("ok expect:shows=Zones expect:hides=Local_repeaters 4 "
                        "expect:screen=zones expect:shows=Local_repeaters")
        settings = self.render_frame("ok up expect:focus=Settings expect:layout=0")
        gear = {row[x * 3:x * 3 + 3] for row in settings[86:99] for x in range(12, 25)}
        self.assertIn(bytes(3), gear)
        self.assertTrue(any(100 < min(rgb) < 180 and max(rgb) - min(rgb) <= 4 for rgb in gear))

    def test_inbox_hints_follow_the_focused_action(self):
        self.run_script("ok 2 expect:shows=OK expect:hides=OK_Compose expect:hides=#_reply ok expect:screen=compose")
        self.run_script("msg:9991234,Hello wait:9000 ok 2 expect:shows=#_reply down "
                        "expect:shows=OK expect:hides=OK_Compose expect:hides=#_reply expect:layout=0")

    def test_profile_menu_sections_use_roomy_headings_and_rows(self):
        sections = (
            ("ok 1", "Contacts", "contacts", 31),
            ("msg:9991234,Meet_at_the_north_lot wait:9000 ok 2", "Messages", "messages", 2),
            ("call:9991234,3100,g wait:11000 ok 3", "Recent", "recent", 1),
            ("ok 4", "Zones", "zones", 5),
            ("ok 5", "Channels", "channels", 41),
            ("ok 6", "Tools", "tools", 9),
        )
        for script, heading, screen, count in sections:
            with self.subTest(screen=screen):
                frame = self.render_frame(
                    f"{script} expect:screen={screen} expect:count={count} "
                    f"expect:shows={heading} expect:layout=0"
                )
                heading_ink = sum(frame[y][x * 3] < 110 for y in range(39, 49) for x in range(16, 100))
                self.assertGreater(heading_ink, 75)
                self.assertEqual(frame[75][7 * 3:8 * 3], frame[75][:3])
                self.assertNotEqual(frame[75][10 * 3:11 * 3], frame[75][:3])
                self.run_script(f"{script} down up expect:count={count} expect:layout=0")

    def test_nested_profile_lists_share_the_roomy_heading(self):
        for script, screen in (("ok 1 down ctx", "actions"), ("ok 5 ok", "channel"),
                               ("ok 4 down ctx", "zone"), ("ok 6 6", "roaming")):
            with self.subTest(screen=screen):
                frame = self.render_frame(f"{script} expect:screen={screen} expect:layout=0")
                ink = sum(frame[y][x * 3] < 110 for y in range(39, 49) for x in range(16, 100))
                self.assertGreater(ink, 75)

    def test_home_mode_sits_in_the_upper_left_of_the_middle_frame(self):
        frame = self.render_frame("expect:layout=0")
        bar_ink = sum(frame[y][x * 3] < 80 for y in range(8, 30) for x in range(37, 70))
        middle_ink = sum(frame[y][x * 3] < 80 for y in range(38, 62) for x in range(15, 41))
        self.assertEqual(bar_ink, 0)
        self.assertGreater(middle_ink, 25)
        self.run_script("expect:shows=DMR prototype:dstar_on expect:shows=D-STAR expect:layout=0")

    def test_home_footer_separates_menu_and_quick_hints(self):
        frame = self.render_frame("expect:layout=0")

        def footer_ink(left, right):
            return sum(min(row[x * 3:x * 3 + 3]) > 220 for row in frame[211:] for x in range(left, right))

        self.assertGreater(footer_ink(15, 90), 40)
        self.assertGreater(footer_ink(225, 296), 40)
        self.assertEqual(footer_ink(137, 174), 0)
        label_rows = [y for y in range(202, 236)
                      if any(min(frame[y][x * 3:x * 3 + 3]) > 220 for x in range(15, 99))]
        self.assertGreaterEqual(max(label_rows) - min(label_rows) + 1, 18)

    def test_menu_pages_and_footer_match_home(self):
        self.run_script("ok expect:shows=1/2 expect:shows=OK expect:hides=1-7_open "
                        "expect:hides=Settings down down down down down down "
                        "expect:shows=2/2 expect:focus=Settings ok expect:screen=settings "
                        "expect:shows=OK expect:hides=OK_Open 2 expect:screen=group "
                        "expect:shows=OK expect:hides=OK_change ok expect:screen=display-choice "
                        "expect:shows=OK expect:hides=OK_pick")
        home = self.render_frame("")
        for screen in ("ok", "ok 7", "ok 7 2", "ok 7 2 ok"):
            frame = self.render_frame(screen + " expect:layout=0")
            for y in range(198, 240):
                self.assertEqual(frame[y][450:453] == home[216][450:453], 202 <= y < 236)

    def test_menu_status_uses_larger_mode_and_device_marks(self):
        for screen in ("ok", "ok 7", "ok 7 2 ok"):
            frame = self.render_frame("gps:fix " + screen + " expect:layout=0")
            for x0, x1, height in ((5, 60, 14), (246, 270, 15), (277, 305, 22)):
                ink = [y for y in range(1, 28) if any(
                    max(frame[y][x * 3:x * 3 + 3]) < 180 for x in range(x0, x1))]
                self.assertGreaterEqual(max(ink) - min(ink) + 1, height)
        self.run_script("gps:fix prototype:bt_connected mon vox:on roam:ready "
                        "msg:9991234,Hello wait:9000 prototype:crypt_aes ok 7 "
                        "rx:9991234,3100,g,Morgan_Reed expect:shows=DMR expect:hides=RX "
                        "expect:shows=AES expect:layout=0")

    def test_submenu_rows_align_with_main_menu_edges(self):
        for screen in ("ok", "ok 1", "ok 1 down ctx", "ok 2",
                       "call:9991234,3100,g wait:11000 ok 3", "ok 4", "ok 4 down ctx",
                       "ok 5", "ok 5 ok", "ok 6", "ok 6 6", "ok 7", "ok 7 2", "ok 7 2 ok"):
            with self.subTest(screen=screen):
                frame = self.render_frame(screen + " expect:layout=0")
                for outside, inside in ((6, 7), (303, 302)):
                    self.assertTrue(any(
                        row[outside * 3:outside * 3 + 3] != row[inside * 3:inside * 3 + 3]
                        for row in frame[75:198]))

    def test_home_has_four_inset_panes_in_both_themes(self):
        amateur = "ok 7 2 ok down ok long:back "
        night = "ok 7 2 down ok down ok long:back "
        for profile in ("", amateur):
            for theme in ("", night):
                with self.subTest(profile=profile or "Commercial", theme=theme or "Day"):
                    frame = self.render_frame(profile + theme + "expect:layout=0")
                    gutter = frame[0][:3]
                    runs = []
                    for y, row in enumerate(frame):
                        self.assertEqual(row[:3], gutter)
                        center = row[50 * 3:51 * 3]  # Clear of the footer labels and centered channel text.
                        if center != gutter and (y == 0 or frame[y - 1][50 * 3:51 * 3] == gutter):
                            runs.append(y)
                    self.assertEqual(len(runs), 4)
                    for top in runs:
                        self.assertEqual(frame[top][4 * 3:5 * 3], gutter)
                        self.assertNotEqual(frame[top + 6][4 * 3:5 * 3], gutter)
                    self.assertLessEqual(runs[-1], 202)
                    self.assertEqual(frame[-1][50 * 3:51 * 3], gutter)

    def run_script(self, script, geometry=GEOMETRY):
        run = subprocess.run([SIM, "--geom", geometry, "--script", script],
                             capture_output=True, text=True, errors="replace", timeout=15)
        self.assertEqual(run.returncode, 0, run.stdout + run.stderr)

    def test_display_choices_are_native_and_survive_reboot(self):
        self.run_script(
            "ok 7 2 expect:shows=Display_Profile expect:hides=GUI_profile ok expect:screen=display-choice "
            "expect:shows=Commercial expect:shows=Amateur down ok "
            "down ok expect:shows=Day expect:shows=Night down ok "
            "down ok expect:shows=Off expect:shows=On down ok reboot "
            "expect:screen=home ok 7 2 expect:shows=Amateur expect:shows=Night down down ok expect:focus=On"
        )
        self.run_script("ok 7 2 expect:hides=Display_Profile", "320x240")

    def test_talkgroup_order_key_only_applies_to_legacy_home(self):
        self.run_script("labelkey expect:toast=none")
        self.run_script("labelkey expect:toast=Talkgroup_name_first", "320x240")
        after_call_back = "ok 7 8 " + "down " * 8 + "ok " * 14
        self.run_script(after_call_back + "expect:set47=12 ok expect:set47=16")
        self.run_script(after_call_back + "expect:set47=12 ok expect:set47=19", "320x240")

    def test_gps_clock_obeys_fix_and_native_receiver_setting(self):
        self.run_script(
            "ok 7 2 down down ok down ok back back back expect:hides=14:08 "
            "gps:fix expect:shows=14:08 ok 7 2 expect:hides=14:08 back back back "
            "gps:searching expect:hides=14:08 gps:fix "
            "ok 7 5 expect:focus=GPS ok back back back expect:hides=14:08"
        )
        setup = "ok 7 2 down down ok down ok long:back gps:fix "
        for profile, top in (("", 36), ("ok 7 2 ok down ok long:back ", 29)):
            with self.subTest(profile=profile or "Commercial"):
                frame = self.render_frame(profile + setup +
                                          "expect:shows=21_Nov expect:shows=14:08 expect:layout=0")
                self.assertTrue(all(row[42 * 3:160 * 3] == row[150:153] * 118 for row in frame[:top - 1]))
                # Date and time each end at the right edge of the mode row.
                for first in (top + 3, top + 17):
                    self.assertTrue(any(row[287 * 3:298 * 3] != row[150:153] * 11
                                        for row in frame[first:first + 14]))

    def test_profiles_keep_call_context_on_home(self):
        self.run_script(
            "expect:centered=USA expect:hides=TS1 expect:hides=TG_3100 "
            "rx:9991234,3100,g,Morgan_Reed expect:centered=USA expect:centered=Morgan_Reed "
            "expect:hides=Receiving rxend expect:centered=USA expect:hides=Morgan_Reed "
            "ok 7 2 ok down ok long:back expect:centered=USA expect:shows=TS1 expect:shows=TG_3100"
        )

    def test_layout_with_crowded_status_bar_and_notifications(self):
        for theme in ("", "ok 7 2 down ok down ok long:back "):
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
            "ok 7 2 down down ok down ok long:back gps:fix mon vox:on roam:searching "
            "prototype:bt_connected prototype:record_on prototype:dstar_on prototype:crypt_arc4 "
            "prototype:talkaround_on msg:9991234,Hello rxonly:on scankey wait:9000 "
            "expect:shows=D-STAR expect:shows=ARC4 expect:shows=14:08 expect:layout=0"
        )

    def test_commercial_crowded_status_keeps_the_clock_and_device_icons(self):
        setup = (
            "ok 7 2 down down ok down ok long:back gps:fix quick 7 ctx 3 5 ok wait:3000 "
            "expect:dual=1 mon roam:ready prototype:bt_connected prototype:dstar_on "
            "prototype:crypt_arc4 prototype:talkaround_on msg:9991234,Hello wait:9000 lockkey wait:3000"
        )
        check = " expect:shows=21_Nov expect:shows=14:08 expect:shows=ARC4 expect:layout=0"

        def operating_ink(frame):
            # Compare the solid strokes. The indexed palette can map an edge to different neighboring shades.
            return [tuple(max(rgb) < 40 for rgb in zip(row[::3], row[1::3], row[2::3]))
                    for row in frame[36:64]]

        crowded = self.render_frame(setup + " vox:on prototype:record_on" + check)
        for state in ("vox:off prototype:record_on", "vox:on prototype:record_off"):
            with self.subTest(state=state):
                toggled = self.render_frame(setup + " " + state + check)
                self.assertTrue(crowded[:36] != toggled[:36], "device mark should change the top bar")
                self.assertTrue(operating_ink(crowded) == operating_ink(toggled),
                                "operating marks should stay in place")
        # Dual Watch owns a device slot immediately to the left of GPS.
        self.assertTrue(any(row[153 * 3:169 * 3] != row[:3] * 16 for row in crowded[7:29]))

    def test_operational_activity_stays_visible(self):
        self.run_script("dlmode:p25 p25t:1 expect:shows=Finding_system")
        self.run_script("t3 dlpos:3 t3state:fallback expect:shows=Fallback")
        self.run_script("aprstx expect:shows=TX_APRS aprsend expect:hides=TX_APRS")

    def test_gui_switch_keeps_channel_and_display_preferences(self):
        self.run_script(
            "down ok 7 2 ok down ok gui:original expect:screen=home "
            "expect:shows=Valley_rpt expect:freq=147060000 "
            "ok 7 2 expect:hides=Display_Profile gui:prototype expect:screen=home "
            "expect:centered=Valley_rpt ok 7 2 expect:shows=Amateur"
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
