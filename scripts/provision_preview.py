"""Serve the firmware's actual AP setup page with simulated data, without an ESP32.

Run: python scripts/provision_preview.py
Open the URL printed at startup. No device is contacted and nothing is saved.

Use --scan to reach the three branches of the page's scan() handler, which are
otherwise awkward to produce on hardware:

    --scan ok      a varied network list (default)
    --scan empty   "No networks found"
    --scan fail    "Scan failed"

handleScan() always answers 200 with a JSON array and has no error path, so
the page only reaches "Scan failed" when the request itself dies -- the board
rebooting or the radio dropping during the blocking scan. --scan fail closes
the connection without replying, which is what the page sees in that case.
"""

import argparse
import errno
import json
import socket
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]

# Deliberately varied: an open network for the "leave blank" placeholder, a hidden
# SSID that the firmware rejects as empty_ssid, an over-long name for layout, and
# enough rows to scroll past the list's 220 px max-height.
NETWORKS = [
    {"ssid": "Studio Wi-Fi", "rssi": -42, "channel": 6, "secure": True},
    {"ssid": "reflexpointKR", "rssi": -47, "channel": 11, "secure": True},
    {"ssid": "Cafe Guest", "rssi": -58, "channel": 1, "secure": False},
    {"ssid": "A-Very-Long-Network-Name-That-Should-Wrap-Or-Clip", "rssi": -61,
     "channel": 3, "secure": True},
    {"ssid": "", "rssi": -64, "channel": 9, "secure": True},
    {"ssid": "Neighbour 2.4G", "rssi": -71, "channel": 2, "secure": True},
    {"ssid": "Neighbour 5G", "rssi": -76, "channel": 36, "secure": True},
    {"ssid": "IoT-Devices", "rssi": -79, "channel": 13, "secure": True},
    {"ssid": "Printer_Direct", "rssi": -84, "channel": 7, "secure": False},
    {"ssid": "Barely There", "rssi": -91, "channel": 4, "secure": True},
]

# Every path the firmware routes to handleCaptive(), which serves the setup page
# with 200 OK rather than a redirect. See the comment in src/provisioning.cpp.
CAPTIVE_PATHS = ("/generate_204", "/gen_204", "/hotspot-detect.html",
                 "/library/test/success.html", "/connecttest.txt", "/ncsi.txt",
                 "/redirect")

SCAN_MODE = "ok"


def setup_html():
    source = (ROOT / "src/provisioning.cpp").read_text(encoding="utf-8")
    return source.split('R"HTML(', 1)[1].rsplit(')HTML";', 1)[0].encode("utf-8")


class PreviewHandler(BaseHTTPRequestHandler):
    def reply(self, status, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def send_page(self):
        body = setup_html()
        self.send_response(200)
        self.send_header("Content-Type", "text/html")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = urlsplit(self.path).path
        if path == "/scan":
            if SCAN_MODE == "fail":
                self.close_connection = True
                return
            return self.reply(200, {"networks": [] if SCAN_MODE == "empty" else NETWORKS})
        # The firmware's onNotFound also serves the page, so unknown paths match it.
        self.send_page()

    def do_POST(self):
        if urlsplit(self.path).path != "/save":
            return self.reply(404, {"error": "not_found"})
        length = int(self.headers.get("Content-Length") or 0)
        if not length:
            return self.reply(400, {"error": "bad_body"})
        try:
            body = json.loads(self.rfile.read(length))
        except ValueError:
            return self.reply(400, {"error": "bad_json"})
        ssid = str(body.get("ssid", "")).strip()
        if not ssid:
            return self.reply(400, {"error": "empty_ssid"})
        password = str(body.get("pass", ""))
        print('Would save SSID "%s" with a %d-character password, then reboot.'
              % (ssid, len(password)), flush=True)
        self.reply(200, {"ok": True})

    def log_message(self, *_):
        pass


class PreviewServer(ThreadingHTTPServer):
    # ThreadingHTTPServer enables SO_REUSEADDR, which can silently share an
    # occupied port on Windows instead of raising an address-in-use error.
    allow_reuse_address = False

    def server_bind(self):
        if hasattr(socket, "SO_EXCLUSIVEADDRUSE"):
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
        super().server_bind()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--port", type=int, default=8081)
    parser.add_argument("--scan", choices=("ok", "empty", "fail"), default="ok")
    args = parser.parse_args()
    SCAN_MODE = args.scan
    try:
        server = PreviewServer(("127.0.0.1", args.port), PreviewHandler)
    except OSError as exc:
        if exc.errno not in (errno.EADDRINUSE, errno.EACCES, 10048, 10013):
            raise
        server = PreviewServer(("127.0.0.1", 0), PreviewHandler)
        print("Port %s is unavailable; using a free port." % args.port, flush=True)
    print("Setup preview: http://127.0.0.1:%s · scan=%s · Nothing is saved"
          % (server.server_port, args.scan), flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
