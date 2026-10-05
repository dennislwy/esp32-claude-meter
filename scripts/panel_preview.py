"""Serve the firmware's actual panel with simulated data, without an ESP32.

Run: python scripts/panel_preview.py
Open http://127.0.0.1:8080 and use PIN 123456. No device is contacted.
"""

import argparse
import json
import math
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlsplit

ROOT = Path(__file__).resolve().parents[1]
STATE = {
    "ip": "192.168.1.42", "hostname": "claude-meter", "uptime_s": 93642,
    "fw_rev": "preview", "fw_built": "Simulated device", "heap_free": 148480,
    "heap_min": 102400, "battery_mv": 4120, "battery_pct": 94,
    "wifi_rssi": -52, "wifi_ssid": "Studio Wi-Fi", "poll_min": 2,
    "warn5": 80, "warn7": 90, "quiet_start_h": 22, "quiet_start_m": 30,
    "quiet_end_h": 7, "quiet_end_m": 15, "quiet_on": True, "audio_vol": 65,
    "tz": "<+08>-8", "tz_name": "Asia/Kuala_Lumpur", "rotation": 0,
    "poll_age_s": 42,
    "accounts": [
        {"name": "Personal", "configured": True, "has_data": True,
         "h5": 29, "d7": 60, "age": "1 m ago"},
        {"name": "Studio", "configured": True, "has_data": True,
         "h5": 12, "d7": 38, "age": "1 m ago"},
    ],
}
HISTORY_CLEARED = False


def panel_html():
    source = (ROOT / "src/panel_html.h").read_text(encoding="utf-8")
    return source.split('R"HTML(', 1)[1].rsplit(')HTML";', 1)[0].encode("utf-8")


def history():
    accounts = []
    for index, account in enumerate(STATE["accounts"]):
        series = {}
        for name in ("h5", "d7"):
            series[name] = [
                None if HISTORY_CLEARED or 55 <= hour < 62 else
                round((hour % 19) * (2.4 if index == 0 else 1.6)) if name == "h5" else
                round(12 + hour * (0.28 if index == 0 else 0.14) + 2 * math.sin(hour / 8))
                for hour in range(168)
            ]
        accounts.append({"name": account["name"], **series})
    return {"cols": 168, "col_seconds": 3600, "newest_epoch": int(time.time()),
            "accounts": accounts}


class PreviewHandler(BaseHTTPRequestHandler):
    def reply(self, status, payload, cookie=None):
        self.send_response(status)
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Type", "text/html; charset=utf-8" if isinstance(payload, bytes)
                         else "application/json")
        if cookie:
            self.send_header("Set-Cookie", cookie)
        self.end_headers()
        self.wfile.write(payload if isinstance(payload, bytes) else json.dumps(payload).encode())

    def authenticated(self):
        return "sid=preview" in self.headers.get("Cookie", "")

    def do_GET(self):
        path = urlsplit(self.path).path
        if path == "/":
            return self.reply(200, panel_html())
        if not self.authenticated():
            return self.reply(401, {"error": "auth"})
        if path == "/api/state":
            now = int(time.time())
            data = {**STATE, "now_epoch": now, "accounts": [
                {**a, "h5_reset": now + 8040, "d7_reset": now + 172800}
                for a in STATE["accounts"]]}
            return self.reply(200, data)
        if path == "/api/history":
            return self.reply(200, history())
        if path == "/api/news":
            return self.reply(200, {"ok": True, "fetching": False, "fetched_epoch": int(time.time()),
                                   "items": [{"title": "Preview headline %s — A little room for the next idea" % i,
                                              "date": "Oct %s, 2026" % (6 - i),
                                              "link": "https://www.anthropic.com/news"}
                                             for i in range(1, 7)]})
        if path == "/api/wifi/scan":
            return self.reply(200, {"networks": [
                {"ssid": "Studio Wi-Fi", "rssi": -52, "channel": 6, "secure": True, "saved": True},
                {"ssid": "Guest", "rssi": -68, "channel": 11, "secure": False, "saved": False}]})
        self.reply(404, {"error": "not_found"})

    def do_POST(self):
        global HISTORY_CLEARED
        path = urlsplit(self.path).path
        try:
            body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
        except (ValueError, TypeError):
            return self.reply(400, {"error": "bad_json"})
        if path == "/api/login":
            if body.get("pin") != "123456":
                return self.reply(401, {"error": "auth"})
            return self.reply(200, {"ok": True}, "sid=preview; HttpOnly; SameSite=Strict; Path=/")
        if not self.authenticated():
            return self.reply(401, {"error": "auth"})
        if path == "/api/logout":
            return self.reply(200, {"ok": True}, "sid=; Max-Age=0; Path=/")
        if path == "/api/settings":
            STATE.update({key: value for key, value in body.items() if key in STATE and key != "accounts"})
        elif path == "/api/tokens":
            for i, account in enumerate(STATE["accounts"], 1):
                account["name"] = body.get("name%s" % i, account["name"])
            return self.reply(200, {"ok": True, "probes": [
                {"account": i, "ok": True, "http": 200, "h5": 29, "d7": 60}
                for i in (1, 2) if body.get("token%s" % i)]})
        elif path == "/api/wifi":
            STATE["wifi_ssid"] = body.get("ssid", STATE["wifi_ssid"])
        elif path == "/api/history/clear":
            HISTORY_CLEARED = True
        elif path not in ("/api/refresh", "/api/sounds/play", "/api/reboot", "/api/factory-reset"):
            return self.reply(404, {"error": "not_found"})
        self.reply(200, {"ok": True})

    def log_message(self, *_):
        pass


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()
    server = ThreadingHTTPServer(("127.0.0.1", args.port), PreviewHandler)
    print("Preview: http://127.0.0.1:%s · PIN 123456 · Simulated data only" % args.port, flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
