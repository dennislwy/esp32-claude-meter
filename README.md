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
pio run -t uploadfs      # upload the sound theme (data_dir) to LittleFS
pio device monitor       # serial console at 115200 bps
```

The first upload needs both `uploadfs` (sounds) and `upload` (firmware).
Close the serial monitor before flashing — it holds COM5.

There is no `pio test` environment. The host-side checks under `test/` build
with a plain C++ compiler; each folder's `README.md` has the command.

## First-time setup

A fresh board has no Device PIN, so it has no key to encrypt anything with.
Setup therefore starts on the device's own Wi-Fi, not on the serial console.

1. Power the board. It comes up as the open access point
   `claude-meter-XXXXXX` (the last six hex digits of its Wi-Fi MAC address)
   and shows that name plus `http://192.168.4.1` on the ePaper.
2. Join that network. The captive-portal popup opens the setup page by
   itself; if your phone doesn't offer it, browse to `http://192.168.4.1`.
3. Pick your network and type its password. The optional **Tokens** step lets
   you add Claude OAuth tokens and display names for up to two accounts.
   Choose **Skip for now** to add them later.
4. Choose a **4-digit Device PIN** and select **Save & connect**. The password
   and any supplied tokens are encrypted together with a key derived from
   that PIN, then the board reboots onto your Wi-Fi.
5. If you skipped the tokens, the board opens the [Web
   panel](docs/PANEL_FEATURES.md) by itself and shows its URLs, a QR code and
   a six-digit sign-in code on the ePaper. Sign in, open **Accounts**, and paste your
   tokens there. Accounts can also be added or updated in the panel later.

The serial console at 115200 does the same job once the board is unlocked:

```
token1 <sk-ant-oat01-... from `claude setup-token`>
token2 <optional second account token>
account1 <display name>
account2 <display name>
```

Settings are stored in NVS as plain values and usage history in LittleFS; the
Wi-Fi password and both tokens are encrypted — see [Security
notes](#security-notes). The device begins polling immediately. Powered from
USB at cold boot it stays awake in debug mode; the deep-sleep cycle starts
after `debug off`, a long `BOOT + PWR`, or a cold boot with no USB host.

**The PIN is asked for again after every power loss.** The key lives in RTC
RAM, which survives deep sleep and a `reboot` but not a loss of power — a
battery pull, a flat pack, or a PWR power-off while on battery. On USB the PWR
long-press cannot cut the latch, so the board stays powered and stays
unlocked. Enter it on the device: `BOOT` changes the digit under the cursor,
`PWR` confirms it. Forgotten it? Hold `BOOT + PWR` for 5 seconds on the PIN
screen and confirm with `PWR` to factory reset the device and return to step 1
(serial `reset confirm` does the same, but serial is only active when a USB
host was detected at cold boot — on battery the button hold is the only way
out). A factory reset erases the credentials, the PIN, all settings and
history; only the sound files are kept.

### Serial commands

Type `help` for the full list. Highlights:

| Command                               | Purpose                                                                                 |
| ------------------------------------- | --------------------------------------------------------------------------------------- |
| `status`                              | Time, battery, current view, hostname, next poll, warning thresholds, quiet/pause hours |
| `usage`                               | Poll now, print the result, redraw, trigger any alerts                                  |
| `view`                                | Switch to the next view (see [Device views](#device-views))                             |
| `interval <1-5>`                      | Minutes between polls                                                                   |
| `warn5h <50-99>` / `warn7d <50-99>`   | Warning threshold per window                                                            |
| `quiet on \| off`                     | Enable or disable quiet hours                                                           |
| `quiet <start>-<end>`                 | Set quiet hours, 24-h local time (e.g. `quiet 22-8`)                                    |
| `pause on \| off`                     | Enable or disable Pause Hours                                                           |
| `pause <start>-<end>`                 | Set the local polling-pause window (e.g. `pause 00:00-06:00`)                           |
| `hostname` / `hostname <name>`        | Show or set the mDNS name, max 15 chars; `clear` restores the default                   |
| `alerts` / `alerts clear`             | Inspect / reset the per-window alert state                                              |
| `history` / `history clear`           | Inspect / wipe the 7-day usage ring                                                     |
| `rtc` / `rtc set YYYY-MM-DD HH:MM:SS` | Read / set the hardware clock                                                           |
| `scan`, `tlscheck`                    | Diagnostics                                                                             |
| `files`, `play <file.wav>`            | LittleFS tools                                                                          |
| `debug off`                           | Turn off serial & LED, start the sleep cycle                                            |
| `reset` / `reset confirm`             | Factory reset: erase the credentials, the Device PIN, all settings and history          |
| `selftest`                            | Verify the credential encryption; touches no stored credential                          |

While the board is locked the console answers only `help`, `status`, `creds`,
`rtc`, `files`, `selftest`, `debug off`, `reboot`, `reset` and `reset confirm`
— everything else would need a credential it cannot read. `creds` prints
`(locked)` in place of the token and password masks.

## Operation

- **Debug mode** (default when USB host is attached at cold boot): serial
  enabled, LED lit while awake. Long-press `BOOT + PWR` together ≥ 1 s on a
  wake to toggle (awake, asleep, or on battery) — except on the PIN screen,
  where holding the chord for 5 s instead opens the factory-reset prompt
  described above. To get
  serial at a battery PIN prompt, leave it untouched for two minutes so it
  deep-sleeps, then wake it holding both buttons for one second. Debug mode
  has no idle timeout: the board stays awake and skips deep sleep until you
  turn it off with `BOOT + PWR`, serial `debug off`, or a cold boot with no
  USB host. Clear it before unplugging, or battery life drops well short of
  the normal-mode figure.
- **Normal mode** (battery): serial and LED off to save power. The board
  wakes on its poll timer, polls, draws, and deep-sleeps again. During enabled
  Pause Hours it sleeps until the window ends without automatic usage
  requests; manual Refresh now and serial `usage` override the pause. A short
  `BOOT` press wakes it to show the next view. A short `PWR` press does
  nothing and the board goes back to sleep; only a long `PWR` press (≥ 1 s)
  powers it off (full refresh to white first, then VBAT_PWR is cut). A warning,
  depletion, or reset for one account switches the screen to that account's
  view until the next poll.
- **Web Panel**: a long `BOOT` press opens a browser control panel on
  your Wi-Fi; another long `BOOT` press closes it. The ePaper shows the
  panel's `.local` and IP URLs, a QR code that signs you in when scanned,
  and the six-digit code for typing in by hand. Time zone (default Asia/Kuala_Lumpur)
  and screen rotation (default 0°) are set there, along with tokens,
  Wi-Fi, the Device PIN, alerts, sounds, and the mDNS device name (applied at the next
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
- **Locked** (after any cold boot — battery pull, a PWR power-off on battery,
  or first power-up of a provisioned board): the radio stays off and the ePaper
  shows `Locked`, `Enter your 4-digit PIN`, and four digit slots. `BOOT`
  cycles the digit under the cursor 0-9, `PWR` confirms it and moves to the
  next slot; the fourth `PWR` tries the PIN. A long `PWR` press still powers
  the board off from here. A correct PIN restarts the board straight into
  normal operation. Two minutes with no button press deep-sleeps the board
  until a button wakes it, so a locked meter in a drawer is not a flat
  battery. See [docs/MODES.md](docs/MODES.md).

The usage ring survives power cycles (persisted to LittleFS; written only
when the 15-min slot advances — ~96 writes/day).

### Device views

A short `BOOT` press (or serial `view`) steps through the dashboard views:

| View               | Shows                                                         |
| ------------------ | ------------------------------------------------------------- |
| Dual               | Both accounts as two cards with 5-hour and 7-day bars         |
| Account 1          | One account, with a large percentage per window               |
| Account 1 history  | 7-day line chart for account 1                                |
| Account 2          | One account, with a large percentage per window               |
| Account 2 history  | 7-day line chart for account 2                                |

Views that can't be shown are skipped: Dual needs both accounts, an account
view needs that account's token, and a history view needs recorded history.
The Web Panel, Wi-Fi setup and PIN screens are modes, not part of the cycle.

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
scripts/       build_info.py (git revision; FW_VERSION is set in src/build_info.h),
               panel_preview.py and provision_preview.py (run the embedded pages
               without a board), chart_bundle/ (ECharts build), check_panel_ui.cjs
test/          host-side checks: hostname, panel_poll, pause_hours, pin_entry, reset_time
sounds/        LittleFS payload: one folder of six WAVs per theme (gitignored)
sound-sources/ original audio (MP3s, credits) the themes were made from (gitignored)
docs/          design notes: modes, battery, alert sounds, web panel, memory footprint
```

## Security notes

- **The Wi-Fi password and both Claude tokens are encrypted in NVS.**
  AES-256-GCM under a key derived from the 4-digit Device PIN with
  PBKDF2-HMAC-SHA256 (~400 ms on this chip). The key exists only in RTC RAM,
  so a cold boot or a battery pull leaves nothing readable until the PIN is
  entered on the device.
- **No plaintext copy is kept elsewhere.** The Wi-Fi driver is told not to
  save the SSID and password in its own NVS namespace, and every cold boot
  erases any copy an older firmware or the board's factory demo left behind
  (`nvs.net80211`, `wifi`, `mqtt`, `websocket`).
- **A 4-digit PIN is a speed bump, not a vault.** There are 10 000
  candidates. On the device each costs ~400 ms, but someone who dumps the
  flash guesses on a PC instead, where the whole space takes seconds to
  minutes. The derivation key is not bound to an eFuse, so the work factor is
  all there is. This defends a lost, stolen, or resold device against casual
  inspection — not against a determined attacker.
- **Setup happens over an open Wi-Fi network and plain HTTP.** The
  provisioning AP has no password, so the Device PIN, your Wi-Fi password and
  both OAuth tokens cross the air unencrypted while you type them in. Anyone
  in range can capture them passively. Provision somewhere you would not mind
  being overheard.

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
