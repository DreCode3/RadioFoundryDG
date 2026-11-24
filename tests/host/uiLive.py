#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 The RadioFoundry Authors

"""Launch the native radio UI simulator in a local web browser."""

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import select
import signal
import subprocess
import sys
import tempfile
import threading
import time
import webbrowser


HOST_DIR = Path(__file__).resolve().parent
LOOPBACK = "127.0.0.1"
FRAME_SIGNATURE = b"\x89PNG\r\n\x1a\n"
TICK_MS = 50
MAX_IDLE_MS = 60_000
ACTIONS = frozenset("""
    ok back up down quick ctx 0 1 2 3 4 5 6 7 8 9
    long:ok long:back long:quick long:ctx
    abkey long:dualkey vfokey long:copykey scankey
    pttdown pttup mondown monup
    rx:9991234,3100,g rx:9991234,3100,g,Morgan_Reed rx:9995678,0,p rxend fmrx fmrxend
    msg:9991234,Meet_at_the_north_lot_in_15_minutes. alert:9995678 zoneoffer:9991234
    aprstx aprsend
    gps:fix gps:searching gps:off
    dlmode:fm dlmode:dmr dlmode:p25 dlmode:nxdn
    vox:on vox:off rxonly:on rxonly:off
    roam:off roam:ready roam:searching roam:weak roam:locked
    gui:original gui:prototype
    prototype:bt_off prototype:bt_on prototype:bt_connected
    prototype:record_on prototype:record_off prototype:dstar_on prototype:dstar_off
    prototype:talkaround_on prototype:talkaround_off
    prototype:crypt_actual prototype:crypt_arc4 prototype:crypt_aes
    reboot
""".split()) | {""}


class Simulator:
    def __init__(self, process, frame_path):
        self.process = process
        self.frame_path = frame_path
        self.lock = threading.Lock()
        if not self.read_ack():
            raise RuntimeError("The simulator could not render its first frame")
        self.frame = self.read_frame()
        self.clock = time.monotonic_ns()

    def read_ack(self):
        if not select.select([self.process.stdout], [], [], 5)[0]:
            raise RuntimeError("Timed out waiting for the native simulator")
        response = self.process.stdout.readline().strip()
        if response == b"OK":
            return True
        if response.startswith(b"ERROR"):
            return False
        raise RuntimeError("The native simulator stopped responding")

    def read_frame(self):
        frame = self.frame_path.read_bytes()
        if not frame.startswith(FRAME_SIGNATURE):
            raise RuntimeError("The native simulator did not write a PNG frame")
        return frame

    def act(self, action):
        with self.lock:
            now = time.monotonic_ns()
            elapsed_ms = (now - self.clock) // 1_000_000
            idle_ms = min(MAX_IDLE_MS, elapsed_ms // TICK_MS * TICK_MS)
            if idle_ms < MAX_IDLE_MS:
                self.clock += idle_ms * 1_000_000
            else:
                self.clock = now
            script = f"wait:{idle_ms} {action}" if idle_ms else action
            if script:
                self.process.stdin.write(f"{script}\n".encode("ascii"))
                self.process.stdin.flush()
                ok = self.read_ack()
                self.frame = self.read_frame()
                if not ok:
                    return None
            return self.frame


class LiveHandler(BaseHTTPRequestHandler):
    timeout = 8

    def log_message(self, *args):
        # The browser refreshes the frame repeatedly, so leave the launch URL visible.
        pass

    def respond(self, status, body, content_type="text/plain; charset=utf-8"):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Security-Policy", (
            "default-src 'none'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; "
            "connect-src 'self'; img-src 'self' blob:; base-uri 'none'; frame-ancestors 'none'"
        ))
        self.end_headers()
        try:
            self.wfile.write(body)
        except (BrokenPipeError, ConnectionResetError):
            pass

    def check_host(self):
        if self.headers.get("Host") != self.server.host_header:
            self.respond(403, b"Open the simulator at the printed loopback URL.")
            return False
        return True

    def do_GET(self):
        if not self.check_host():
            return
        if self.path == "/":
            self.respond(200, self.server.page, "text/html; charset=utf-8")
        elif self.path == "/frame.png":
            self.respond(200, self.server.simulator.frame, "image/png")
        else:
            self.respond(404, b"Page not found.")

    def do_POST(self):
        if not self.check_host():
            return
        if self.path != "/action":
            self.respond(404, b"Page not found.")
            return
        if self.headers.get("Origin") != self.server.origin:
            self.respond(403, b"Send commands from the simulator page.")
            return
        size = self.headers.get("Content-Length", "")
        if not (size.isascii() and size.isdecimal() and len(size) <= 3 and int(size) <= 128):
            self.respond(400, b"Expected one simulator action.")
            return
        if self.headers.get_content_type() != "text/plain" or self.headers.get("Transfer-Encoding"):
            self.respond(400, b"Expected a plain-text action.")
            return
        action = self.rfile.read(int(size)).decode("ascii", errors="replace")
        if action not in ACTIONS:
            self.respond(400, b"Unknown simulator action.")
            return
        try:
            frame = self.server.simulator.act(action)
        except (OSError, RuntimeError) as exc:
            print(f"Simulator error: {exc}", file=sys.stderr)
            self.respond(503, b"The native simulator stopped. Restart the launcher to continue.")
            return
        if frame is None:
            self.respond(422, b"The native simulator declined that action.")
        else:
            self.respond(200, frame, "image/png")


def serve(port, open_browser):
    page = (HOST_DIR / "uiLive.html").read_bytes()
    subprocess.run(["make", "-C", HOST_DIR, "uiSim"], stdout=sys.stderr, check=True)
    with tempfile.TemporaryDirectory(prefix="rfoundry-ui-") as directory:
        frame_path = Path(directory) / "frame.png"
        command = [HOST_DIR / "uiSim", "--geom", "320x240iR10", "--scale", "2",
                   "--out", frame_path, "--interactive"]
        with subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.PIPE) as process:
            try:
                with ThreadingHTTPServer((LOOPBACK, port), LiveHandler) as server:
                    host, port = server.server_address
                    server.host_header = f"{host}:{port}"
                    server.origin = f"http://{server.host_header}"
                    server.page = page
                    server.simulator = Simulator(process, frame_path)
                    url = f"{server.origin}/"
                    print(f"RadioFoundry UI simulator: {url}", flush=True)
                    if open_browser and not webbrowser.open(url):
                        print("Open the loopback URL in your browser.", file=sys.stderr)
                    server.serve_forever()
            finally:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=2)
                    except subprocess.TimeoutExpired:
                        process.kill()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=0, help="loopback port (default: choose a free port)")
    parser.add_argument("--no-open", action="store_true", help="print the URL without opening a browser")
    args = parser.parse_args()
    if not 0 <= args.port <= 65535:
        parser.error("--port must be between 0 and 65535")

    def stop(_signum, _frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, stop)
    try:
        serve(args.port, not args.no_open)
    except KeyboardInterrupt:
        return 0
    except (OSError, RuntimeError, subprocess.CalledProcessError) as exc:
        print(f"Simulator launch failed: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
