#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors

"""Exercise the browser launcher against the native simulator."""

from http.client import HTTPConnection
from pathlib import Path
import select
import struct
import subprocess
import sys
import unittest
from urllib.parse import urlsplit


class LiveSimulatorTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        launcher = Path(__file__).with_name("uiLive.py")
        if not launcher.is_file():
            raise AssertionError("The live browser launcher is missing")
        process = subprocess.Popen(
            [sys.executable, "-u", launcher, "--port", "0", "--no-open"],
            stdout=subprocess.PIPE,
            text=True,
        )
        cls.addClassCleanup(cls.stop_launcher, process)
        if not select.select([process.stdout], [], [], 120)[0]:
            raise AssertionError("The live browser launcher did not start")
        ready = process.stdout.readline()
        cls.url = urlsplit(ready[ready.find("http://"):].strip())
        if cls.url.hostname != "127.0.0.1" or cls.url.port is None:
            raise AssertionError(f"The launcher did not report a loopback URL: {ready!r}")

    @staticmethod
    def stop_launcher(process):
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        process.stdout.close()

    def request(self, method, path, body=None, headers=None):
        connection = HTTPConnection(self.url.hostname, self.url.port, timeout=8)
        try:
            connection.request(method, path, body, headers or {})
            response = connection.getresponse()
            return response.status, response.read()
        finally:
            connection.close()

    def action(self, token, **headers):
        return self.request("POST", "/action", token, {
            "Origin": f"http://{self.url.netloc}",
            "Content-Type": "text/plain; charset=utf-8",
            **headers,
        })

    def assert_png(self, response):
        status, frame = response
        self.assertEqual(status, 200, frame[:100])
        self.assertEqual(frame[:8], b"\x89PNG\r\n\x1a\n")
        self.assertEqual(struct.unpack(">II", frame[16:24]), (620, 480))
        return frame

    def test_actions_share_native_menu_state(self):
        self.assertEqual(self.request("GET", "/")[0], 200)
        home = self.assert_png(self.action("long:back"))
        menu = self.assert_png(self.action("ok"))
        settings = self.assert_png(self.action("7"))
        restored_menu = self.assert_png(self.action("back"))
        self.assertNotEqual(home, menu)
        self.assertNotEqual(menu, settings)
        self.assertNotEqual(home, settings)
        self.assertNotEqual(home, restored_menu)
        self.assertNotEqual(settings, restored_menu)
        self.assertEqual(self.assert_png(self.request("GET", "/frame.png")), restored_menu)
        self.assertEqual(self.assert_png(self.action("ok")), settings)

    def test_preset_dmr_call_changes_native_frame(self):
        home = self.assert_png(self.action("long:back"))
        rx = self.assert_png(self.action("rx:9991234,3100,g"))
        alias = self.assert_png(self.action("rx:9991234,3100,g,Morgan_Reed"))
        self.assertNotEqual(home, rx)
        self.assertNotEqual(rx, alias)
        self.assertNotEqual(alias, self.assert_png(self.action("rxend")))

    def test_mode_presets_set_distinct_native_home_displays(self):
        self.assert_png(self.action("long:back"))
        try:
            frames = {self.assert_png(self.action(f"dlmode:{mode}")) for mode in ("fm", "dmr", "p25", "nxdn")}
            self.assertEqual(len(frames), 4)
        finally:
            self.action("dlmode:dmr")

    def test_radio_gui_can_switch_in_the_same_session(self):
        status, page = self.request("GET", "/")
        self.assertEqual(status, 200)
        self.assertIn(b'data-press="gui:original"', page)
        self.assertIn(b'data-press="gui:prototype"', page)
        self.assert_png(self.action("long:back"))
        prototype = self.assert_png(self.action("gui:prototype"))
        original = self.assert_png(self.action("gui:original"))
        self.assertNotEqual(original, prototype)
        self.assertEqual(self.assert_png(self.action("gui:prototype")), prototype)

    def test_indicator_presets_are_accepted_by_the_native_simulator(self):
        self.assert_png(self.action("long:back"))
        presets = ("vox:on", "vox:off", "rxonly:on", "rxonly:off", "roam:ready",
                   "roam:searching", "roam:weak", "roam:locked", "roam:off",
                   "aprstx", "aprsend", "scankey", "scankey",
                   "mondown", "monup", "mondown", "monup")
        for preset in presets:
            with self.subTest(preset=preset):
                self.assert_png(self.action(preset))

    def test_only_the_hosted_page_can_send_allowlisted_actions(self):
        self.assertEqual(self.action("ok", Origin="http://example.com")[0], 403)
        self.assertEqual(self.request("GET", "/", headers={"Host": "example.com"})[0], 403)
        self.assertEqual(self.action("ok\nback")[0], 400)
        self.assertEqual(self.action("wait:99999999")[0], 400)
        self.assertEqual(self.action("shot:other.png")[0], 400)
        self.assertEqual(self.request("GET", "/uiSim")[0], 404)


if __name__ == "__main__":
    unittest.main()
