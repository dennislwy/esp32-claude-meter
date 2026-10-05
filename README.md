# Claude Usage Meter — ESP32-S3 ePaper 1.54

Firmware for the [Waveshare ESP32-S3-ePaper-1.54](https://www.waveshare.com/wiki/ESP32-S3-ePaper-1.54)
board. The device polls `api.anthropic.com` over verified HTTPS, reads the
`anthropic-ratelimit-unified-*` response headers for the 5-hour and 7-day
usage of up to two Claude accounts, and shows the result on the 200×200
black & white ePaper panel. Audible alerts play from an onboard speaker
when a window crosses a configurable warning threshold or depletes, with
optional quiet hours.

## Hardware

- Waveshare ESP32-S3-ePaper-1.54 (ESP32-S3-PICO-1-N8R8: 8 MB quad flash,
  8 MB octal PSRAM)
- 200×200 1.54" ePaper display (partial + full refresh)
- PCF85063 RTC (I²C 0x51) — kept in sync via NTP every 6 h
- ES8311 audio codec + onboard speaker for WAV alert playback
- SHTC3 temperature/humidity sensor (I²C 0x70) — put to sleep (~0.3 µA);
  not used
- Li-ion battery with ADC-based gauge

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
| `status`                              | Time, battery, current view, next poll, warning thresholds, quiet hours |
| `usage`                               | Poll now, print the result, redraw, trigger any alerts                  |
| `view`                                | Switch to the next view (dual / account 1 / account 2)                  |
| `interval <1-5>`                      | Minutes between polls                                                   |
| `warn5h <50-99>` / `warn7d <50-99>`   | Warning threshold per window                                            |
| `quiet on \| off`                     | Enable or disable quiet hours                                           |
| `quiet <start>-<end>`                 | Set quiet hours, 24-h local time (e.g. `quiet 22-8`)                    |
| `alerts` / `alerts clear`             | Inspect / reset the per-window alert state                              |
| `history` / `history clear`           | Inspect / wipe the 7-day usage ring                                     |
| `rtc` / `rtc set YYYY-MM-DD HH:MM:SS` | Read / set the hardware clock                                           |
| `scan`, `tlscheck`                    | Diagnostics                                                             |
| `files`, `play <file.wav>`            | LittleFS tools                                                          |
| `debug off`                           | Turn off serial & LED, start the sleep cycle                            |

## Operation

- **Debug mode** (default when USB host is attached at cold boot): serial
  enabled, LED lit while awake. Long-press `BOOT + PWR` together ≥ 1 s to
  toggle at any time (awake, asleep, or on battery).
- **Normal mode** (battery): serial and LED off to save power. The board
  wakes on its poll timer, polls, draws, and deep-sleeps again. A short
  `BOOT` press wakes it to show the next view; a long `PWR` press powers
  it off (full refresh to white first, then VBAT_PWR is cut).

The usage ring survives power cycles (persisted to LittleFS; written only
when the 30-min slot advances — ~48 writes/day).

## Project layout

```
src/           firmware (main, poll loop, UI, alerts, history, settings)
include/       lv_conf.h and other build-only headers
lib/           local libraries (Battery, Epaper154, ES8311, PCF85063, ClaudeUsage)
assets/certs/  pinned root CAs for api.anthropic.com (embedded at build time)
data/          LittleFS payload: WAV alert sounds (gitignored)
```

## Security notes

- NVS is **not** encrypted on this board. Anyone with the device and a
  USB cable can read back the Wi-Fi password and Claude tokens.
- `.env` and `*.wav` are gitignored; keep tokens out of source control.
