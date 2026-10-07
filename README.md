# Claude Usage Meter — ESP32-S3 ePaper 1.54

Firmware for the [Waveshare ESP32-S3-ePaper-1.54](https://www.waveshare.com/wiki/ESP32-S3-ePaper-1.54)
board. The device polls `api.anthropic.com` over verified HTTPS for the 
5-hour and 7-day usage of up to two Claude accounts, and shows the result 
on the 200×200 black & white ePaper panel. Audible alerts play from an 
onboard speaker when a window crosses a configurable warning threshold or 
depletes, with optional quiet hours. Pause Hours can pause automatic polling
and keep the board asleep through a daily window; it defaults to disabled,
with a saved window of 00:00-06:00.

Firmware version: **0.0.9**, set in `src/build_info.h`.

The web panel's usage history uses a locally served Apache ECharts bundle
with series toggles, tooltips, mouse/touch zoom, and double-click/double-tap
zoom toggling, plus PNG image export.
It works without a CDN. See [panel features](docs/PANEL_FEATURES.md#7-day-history).

## Hardware

- Waveshare ESP32-S3-ePaper-1.54 (ESP32-S3-PICO-1-N8R8: 8 MB quad flash,
  8 MB octal PSRAM)
- 200×200 1.54" ePaper display (partial + full refresh)
- PCF85063 RTC (I²C 0x51) — kept in sync via NTP every 6 h
- ES8311 audio codec + onboard speaker for WAV alert playback
- SHTC3 temperature/humidity sensor (I²C 0x70) — put to sleep (~0.3 µA);
  not used
- 3.7V 400mAh Li-ion battery with ADC-based gauge

## Build & flash

Requires [PlatformIO](https://platformio.org/).

```sh
pio run                  # compile
pio run -t upload        # flash firmware over USB (COM5 on Windows)
pio run -t uploadfs      # upload data/ (WAV sounds) to LittleFS
pio device monitor       # serial console at 115200 bps
```

The first upload needs both `uploadfs` (sounds) and `upload` (firmware).
Close the serial monitor before flashing — it holds COM5.

## First-time setup

Connect USB, open the serial monitor at 115200, then:

```
ssid <your Wi-Fi network>
pass <your Wi-Fi password>
token1 <sk-ant-oat01-... from `claude setup-token`>
token2 <optional second account token>
account1 <display name>
account2 <display name>
```

Credentials are stored in NVS. The device begins polling immediately and
starts the deep-sleep cycle once a USB host is no longer detected.

### Serial commands

Type `help` for the full list. Highlights:

| Command                               | Purpose                                                                 |
| ------------------------------------- | ----------------------------------------------------------------------- |
| `status`                              | Time, battery, current view, next poll, warning thresholds, quiet/pause hours |
| `usage`                               | Poll now, print the result, redraw, trigger any alerts                  |
| `view`                                | Switch to the next view (dual / account 1 / account 2)                  |
| `interval <1-5>`                      | Minutes between polls                                                   |
| `warn5h <50-99>` / `warn7d <50-99>`   | Warning threshold per window                                            |
| `quiet on \| off`                     | Enable or disable quiet hours                                           |
| `quiet <start>-<end>`                 | Set quiet hours, 24-h local time (e.g. `quiet 22-8`)                    |
| `pause on \| off` | Enable or disable Pause Hours |
| `pause <start>-<end>` | Set the local polling-pause window (e.g. `pause 00:00-06:00`) |
| `hostname` / `hostname <name>`        | Show or set the mDNS name, max 15 chars; `clear` restores the default   |
| `alerts` / `alerts clear`             | Inspect / reset the per-window alert state                              |
| `history` / `history clear`           | Inspect / wipe the 7-day usage ring                                     |
| `rtc` / `rtc set YYYY-MM-DD HH:MM:SS` | Read / set the hardware clock                                           |
| `scan`, `tlscheck`                    | Diagnostics                                                             |
| `files`, `play <file.wav>`            | LittleFS tools                                                          |
| `debug off`                           | Turn off serial & LED, start the sleep cycle                            |

## Operation

- **Debug mode** (default when USB host is attached at cold boot): serial
  enabled, LED lit while awake. Long-press `BOOT + PWR` together ≥ 1 s to
  toggle at any time (awake, asleep, or on battery). It has no idle
  timeout: the board stays awake and skips deep sleep until you turn it off
  with `BOOT + PWR`, serial `debug off`, or a cold boot with no USB host.
  Clear it before unplugging, or battery life drops well short of the
  normal-mode figure.
- **Normal mode** (battery): serial and LED off to save power. The board
  wakes on its poll timer, polls, draws, and deep-sleeps again. During enabled
  Pause Hours it sleeps until the window ends without automatic usage
  requests; manual Refresh now and serial `usage` override the pause. A short
  `BOOT` press wakes it to show the next view; a long `PWR` press powers
  it off (full refresh to white first, then VBAT_PWR is cut). A warning,
  depletion, or reset for one account switches the screen to that account's
  view until the next poll.
- **Web Panel**: a long `BOOT` press opens a browser control panel on
  your Wi-Fi (PIN on the ePaper). Time zone (default Asia/Kuala_Lumpur)
  and screen rotation (default 0°) are set there, along with tokens,
  Wi-Fi, alerts, sounds, and the mDNS device name (applied at the next
  restart). Usage continues polling automatically at the
  configured interval, except during Pause Hours. HTTPS requests run on a
  worker while the panel serves cached state; completed results update the
  Usage page on its next status refresh. The panel exits by itself after
  5 minutes with no authenticated request — the Wi-Fi radio is the board's
  heaviest load — and hands control back to the sleep cycle unless debug mode
  is on. Automatic polls do not count as activity, but an open signed-in tab
  polls every 5 seconds and keeps the panel alive indefinitely — close the tab
  when you're done, or the radio will flatten the battery.
  See [docs/WEB_SERVER.md](docs/WEB_SERVER.md)
  and [docs/PANEL_FEATURES.md](docs/PANEL_FEATURES.md).

The usage ring survives power cycles (persisted to LittleFS; written only
when the 30-min slot advances — ~48 writes/day).

## Panel preview

The panel has Usage, Accounts, Device, Polling & alerts, and News views.
It supports automatic six-digit sign-in, System/Light/Dark appearance
applied before first paint, reopening the same page after a browser
refresh, and a seven-day chart with weekday labels
directly below the midnight ticks in the device's time zone. Device details
includes the Wi-Fi MAC address. See [panel features](docs/PANEL_FEATURES.md)
and [panel design](docs/PANEL_DESIGN.md) for the full behavior and verification.

Preview the actual embedded page locally without a board:

```sh
python scripts/panel_preview.py
```

Open the URL printed by the script and use PIN **123456**. It defaults to port
8080 and chooses a free port if that port is busy. Data and device actions are
simulated; the preview binds only to loopback.

## Project layout

```
src/           firmware (main, poll loop, UI, alerts, history, settings)
include/       lv_conf.h and other build-only headers
lib/           local libraries (Battery, Epaper154, ES8311, PCF85063, ClaudeUsage)
assets/certs/  pinned root CAs for api.anthropic.com and the news feed (embedded at build time)
scripts/       build_info.py: generates git revision; FW_VERSION is set in src/build_info.h
data/          LittleFS payload: WAV alert sounds (gitignored)
docs/          design notes: modes, buttons, battery, alert sounds, web panel
```

## Security notes

- NVS is **not** encrypted on this board. Anyone with the device and a
  USB cable can read back the Wi-Fi password and Claude tokens.
- `.env` and `*.wav` are gitignored; keep tokens out of source control.

## Credits

- Thanks to [oauramos/claude-usage-stick](https://github.com/oauramos/claude-usage-stick),
  which inspired this project. Its control panel set the bar for this
  one's feature list.
- Thanks to [Olshansk/rss-feeds](https://github.com/Olshansk/rss-feeds)
  for maintaining the RSS feed of Anthropic news that the panel's news card
  reads.
- Thanks to [Lucide](https://lucide.dev) for the icons the panel draws inline:
  [Gauge](https://lucide.dev/icons/gauge) for the usage view,
  [Camera](https://lucide.dev/icons/camera) for the chart's Take snapshot
  control, and [File Down](https://lucide.dev/icons/file-down) for Export to CSV.
  ISC licensed.
- The Claude Code mark on the e-paper status bar (`src/claude_icon.c`) is
  rasterized from the MIT-licensed mono SVG on
  [theSVG](https://thesvg.org/icon/claude-code?variant=mono). The panel asks
  for Anthropic Serif by name and falls back to Georgia; no font file is
  bundled or downloaded. Both are Anthropic brand assets used to match the
  look of Claude, not a claim of endorsement — see the
  [disclaimer](#disclaimer).

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
The Lucide icons retain their ISC license; see
[third-party notices](docs/THIRD_PARTY_NOTICES.md).

## Disclaimer

"Claude" is a trademark of Anthropic. This project is not affiliated with Anthropic.
