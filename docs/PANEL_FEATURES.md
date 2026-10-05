# Panel features

Everything a signed-in user can do in the LAN control panel, by card.
See [WEB_SERVER.md](WEB_SERVER.md) for how to reach it and the
underlying REST surface.

## Sign in

Enter the 6-digit PIN shown on the ePaper → mint a `sid` cookie
(`HttpOnly`, `SameSite=Strict`), held in RAM. Up to 4 sessions at once
(e.g. phone + laptop); a 5th login evicts the one idle longest.

Five wrong PINs in a row → throttle: 60 s lockout, doubles each extra
miss, capped at 5 min. Exiting panel mode resets the counter.

## Status

View-only, plus one action.

- IP, hostname, uptime (`Xd Yh Zm`), battery %, last-poll age
- Wi-Fi: SSID, RSSI and a quality word (`GSFwifi  ·  -55 dBm (excellent)`;
  ≥ -55 excellent, ≥ -67 good, ≥ -75 fair, below that weak)
- Heap: internal-SRAM free and low-water mark (`142 KB free (min 96 KB)`)
- Firmware: git revision + build time (`82dc70e  ·  2026-10-05 16:42 +0800`),
  generated per build by `scripts/build_info.py`
- Per-account 5 H / 7 D bars with a `resets Sun 4 Oct 17:01 in 1h 4m`
  line each
- **Refresh now** — forces an on-demand `/api/refresh` poll (device
  re-polls Claude, re-renders ePaper, re-connects Wi-Fi after the poll
  drops it). Expect a 3–5 s stall.

## 7-day history

168-column inline SVG line chart.

- Orange = account 1, green = account 2
- Thin line = 5 H series, thick line = 7 D series
- Click a legend chip to toggle that account's series on/off
- Day letters aligned to local midnight; y-axis ticks every 25 %
- Empty slots render as broken segments (JSON `null`)

Downsampled server-side from the 336-slot, 30-min ring to 168 one-hour
columns (max of the two 30-min samples per hour).

## Accounts

- Rename `Name 1` / `Name 2` (shown on the ePaper and in the status
  card)
- Replace `Token 1` / `Token 2` — leave blank to keep the current token
- **Save accounts** persists to NVS, then probes each newly entered
  token against the API and reports a verdict per token: `Token 1 OK:
  5h 12%, 7d 40%`, `rejected (HTTP 401)`, or `could not reach the API`.
  Blocks ~2-3 s per token. The token is saved even if the probe fails.

Tokens are never read back by the panel — the fields are POST-only and
blank means unchanged. Tokens are stored unencrypted in NVS (R9 is
pending).

## Wi-Fi

- Edit SSID + password (blank password = keep current)
- **Scan** → sorted list of nearby 2.4 GHz networks (SSID, RSSI,
  lock icon, "saved" marker). Tap a row to populate the SSID field.
  The scan runs asynchronously on the device (~8 s from click to list,
  bound by the radio, which is off-channel while scanning) while the page
  polls for results, so the panel stays responsive.
- **Save Wi-Fi** persists to NVS; takes effect at the next poll (the
  current session stays on the old network).

## Alert sounds

- Volume slider 0–100 % — saves on release, maps to −40…0 dB in the
  ES8311 codec. 0 % is near-mute, 100 % is codec max.
- Six test-play buttons, one per alert WAV: `5h-warning`,
  `5h-depleted`, `5h-reset`, `7d-warning`, `7d-depleted`, `7d-reset`.
  Each click plays the WAV via LittleFS (blocks the panel for 1–3 s
  per play).

`/api/sounds/play` is allow-listed to those six names only — the panel
will never play an arbitrary file.

## Display &amp; time

- **Time zone** — type-ahead picker: start typing a city, country,
  alias, or offset and the list narrows as you type (`berlin`, `japan`,
  `seattle` → Los Angeles, `sabah` → Kuching, `gmt+8`, `+5:30`). Arrow
  keys + Enter or a click to pick, Esc to cancel. Accents are ignored
  (`sao paulo` finds São Paulo). The zone this browser is in is offered
  at the top. Under the field: the IANA name and the current local time
  in the picked zone.
- 145 zones covering every UTC offset, each stored as a POSIX TZ string
  taken from tzdata 2025c, so daylight saving switches automatically.
  Labels show the standard offset, e.g. `(GMT+1:00) Amsterdam,
  Netherlands`.
- **Screen rotation** — 0° (default), 90° clockwise, 180°, 270°.
- **Save display &amp; time** persists both. A time-zone change re-applies
  the zone at once and rewrites the RTC (it keeps local time); history
  labels, quiet hours, and reset times follow. A rotation change redraws
  with a full (flashing) e-paper refresh to avoid ghosting.

## Polling &amp; alerts

- Poll interval, 1–5 min
- 5 H warn %, 7 D warn %, each 50–99
- Quiet-hours start / end as `<input type="time">` with HH:MM
  precision. Wraps past midnight (e.g. `22:30`–`07:15` is overnight).
  Setting start = end disables the window.
- Quiet-hours enabled on/off
- **Save settings** persists to NVS

Settings changes do not force a re-poll; the UI picks them up on its
next `/api/state` tick.

## Sign out

Clears the `sid` cookie and ends the session. Panel mode on the device
stays up until the normal exit (long BOOT, serial `panel`, 5 min idle,
or reboot).

## Danger zone

Each button uses a two-tap arm pattern: first tap shows "Tap again to
confirm", second tap within 5 s executes.

- **Clear 7-day history** — wipes `/history.bin` on LittleFS. Dashboard
  reverts to "no history yet". No reboot.
- **Reboot** — `ESP.restart()` after flushing the HTTP response (~1 s
  delay). Session is lost; device re-enters Normal mode after boot.
- **Factory reset** — wipes NVS (`meter` + `alerts` namespaces) and
  history, then reboots. Device comes up in [AP provisioning mode](WEB_SERVER.md#provisioning-phase-2)
  because Wi-Fi SSID is empty. You will need to re-enter the SSID,
  password, and Claude tokens.

The `/api/factory-reset` route requires `{"confirm":"wipe"}` in the
JSON body — a stray POST returns 400. The two-tap UI arm is a second
line of defence.

## Not exposed in the panel

Serial commands without a panel equivalent (noted in [WEB_SERVER.md](WEB_SERVER.md#serial-parity)):

- `alerts` / `alerts clear` — per-window alert-latch inspection and reset
- `rtc` / `rtc set` — read/set the PCF85063 RTC chip
- `tlscheck` — verify the Anthropic root-CA chain
- `files` — LittleFS directory listing
- `debug off` — turn off debug mode (serial + LED) without rebooting
- `play <file>` — arbitrary WAV playback (deliberately restricted)
