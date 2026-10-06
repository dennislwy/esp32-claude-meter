# Panel features

Everything a signed-in user can do in the Web control panel, by feature.
See [WEB_SERVER.md](WEB_SERVER.md) for how to reach it and the
underlying REST surface.

## Navigation and appearance

The panel has five views: **Usage**, **Accounts**, **Device**,
**Polling & alerts**, and **News**. Desktop uses a fixed sidebar; phones use
a horizontally scrollable navigation row. Only the selected view is shown.
The topbar is hidden at mobile widths (760px and below) to give the page
content more room.
On mobile, Sign out sits to the right of the theme switch; desktop keeps
it on the left. Keyboard order follows the displayed order.
Switching views keeps unsaved field edits. Automatic state updates also
preserve edits until they are saved.

Choose **System**, **Light**, or **Dark** beside Sign out. The selection is
saved in this browser; System follows the operating system's appearance.
The preference is applied before the first render, avoiding a light flash
when loading in dark mode. Blocked storage falls back to the system theme.
The complete UI loads from the meter, with no external fonts or scripts.
Serif text prefers locally installed Anthropic Serif, then Georgia,
Times New Roman, and the browser's default serif. No font file is shipped.
Both the sign-in footer and page footer link to the project's GitHub repository.
Keyboard focus, labelled controls, live feedback, and reduced-motion
preferences are supported.

Design analysis and local preview instructions: [PANEL_DESIGN.md](PANEL_DESIGN.md).

## Sign in

The complete sign-in screen fits a 360 × 780 CSS viewport at DPR 3 in
light and dark themes, with normal text size and the keyboard closed.
Enlarged text and shorter viewports can scroll naturally.

Entering or pasting six digits signs in automatically. Incomplete or
non-numeric PINs are not submitted; a pending sign-in cannot submit twice.
The button and Enter key also support manual retries after an error.

Enter the 6-digit PIN shown on the ePaper → mint a `sid` cookie
(`HttpOnly`, `SameSite=Strict`), held in RAM. Up to 4 sessions at once
(e.g. phone + laptop); a 5th login evicts the one idle longest.

Five wrong PINs in a row → throttle: 60 s lockout, doubles each extra
miss, capped at 5 min. Exiting panel mode resets the counter.

## Status

Usage shows per-account bars and the seven-day chart. Device → Device
details contains battery, last poll, signal strength, uptime, and the
complete diagnostics.

- IP, hostname, uptime (`Xd Yh Zm`), battery %, last-poll age
- Wi-Fi: SSID, RSSI and a quality word (`GSFwifi  ·  -55 dBm (excellent)`;
  ≥ -55 excellent, ≥ -67 good, ≥ -75 fair, below that weak)
- MAC address: the device's Wi-Fi station address, shown after Wi-Fi
- Heap: internal-SRAM free and low-water mark (`142 KB free (min 96 KB)`)
- Firmware: release version + git revision (`0.0.9-aae7f66-dirty`).
  `FW_VERSION` is set in `src/build_info.h`; `scripts/build_info.py`
  generates the git revision. No build timestamp is shown.
- Per-account 5 H / 7 D bars with a `resets Sun 4 Oct 17:01 in 1h 4m`
  line each, formatted in the device's selected time zone. Missing usage
  displays a dash instead of a fabricated percentage. Warning and exhausted
  windows use distinct bar colors.
- **Refresh now** — forces an on-demand `/api/refresh` poll (device
  re-polls Claude, re-renders ePaper, re-connects Wi-Fi after the poll
  drops it). Expect a 3–5 s stall.

Last poll and the news fetch status use compact relative ages: `25s ago`
below one minute, `12m ago` below one hour, and `3h ago` thereafter.
Minutes and hours are rounded; there is no day unit (`48h ago` for two
days). Missing poll data displays a dash. Account cards use `Updated just now`
below one minute, then whole minutes rounded down, such as `Updated 120m ago`.

## 7-day history

Apache ECharts 6.1.0 chart in Usage, with 168 hourly columns. Its compressed
library is served locally from firmware flash and loaded after sign-in.

- Blue = account 1, green = account 2; colors adapt to the theme
- Solid line = 5 H series (1.6 px), dashed line = 7 D series (1 px)
- The in-chart legend independently toggles each account's 5-hour/7-day
  series and is included in saved images. Long account names are shortened
  to fit the chart. Equivalent keyboard controls appear when focused; use
  Enter/Space to show/hide each series
- Compact series labels use `account 1 · 5h`, `account 1 · 7d`,
  `account 2 · 5h`, and `account 2 · 7d`, with the saved account names
- All account/window series share one combined graph
- The chart's **Save image** control sits beside the chart title and
  downloads the current visible graph as
  `claude-meter-usage-history.png`, at 2x resolution with the current theme's
  background. The title is included in the PNG; the toolbox control
  itself is omitted
- The **Export CSV** control (Lucide file-down), right of Save image,
  downloads the raw 7-day history as
  `claude-meter-<first-timestamp>-<last-timestamp>.csv`. Columns:
  `timestamp,acct1-5h,acct1-7d,acct2-5h,acct2-7d`, where `timestamp` is the
  start of the 30-minute sample slot in epoch seconds and the values are
  usage %. It is the device's full 30-minute resolution, not the chart's
  hourly maximum. Slots with no sample for either account (device off) are
  left out; a value missing for one account is an empty cell. With no
  history yet, the chart status says "No history to export yet."
- Hover or tap for device-local time, account, usage window, and percentage.
  Account names are rendered as text, including inside tooltips. Mobile
  taps preserve line colors, widths, and opacity without focusing one
  series or dimming the others
- On mobile (up to 760 px), the x-axis pointer snaps to hourly samples,
  uses a 1 px line, and hides its time label. Tap tooltips still show
  device-local time.
  A visible 16 px handle is centered on the x-axis and can be dragged
  to inspect samples without changing zoom
- Double-click or double-tap the plot to toggle between a closer view
  centered on the selected point and the full week. Single clicks/taps
  show tooltips without changing zoom; dragging and pinching are not taps
- Scroll or pinch to zoom; drag to pan. When the chart is focused,
  `+`/`-` zoom, arrow keys pan,
  and `0` resets
- Zoom and hidden series survive history polling, theme changes,
  and switching panel pages within the session
- Weekday labels are centered directly below their 00:00 midnight ticks
  in the device's time zone, using the exact same x-coordinate. A partial
  day without a visible midnight tick has no extra label. Y-axis ticks
  appear every 25 %. Zoomed views also show hourly labels
- Empty slots render as broken segments (JSON `null`)
- No recorded samples show an explicit empty state
- A library-loading failure shows a Retry chart button; other views remain usable

Downsampled server-side from the 336-slot, 30-min ring to 168 one-hour
columns (max of the two 30-min samples per hour).

## Anthropic news

In News, the 10 latest headlines from anthropic.com/news: date and title, each a
link that opens in a new tab. The first 5 show; scroll the list for the
next 5.

Dates use 13px text. The successful-fetch status links `anthropic.com/news`
to the source page in a new tab.

- Fetched **once per panel session**: the device asks for a fetch when
  panel mode opens and runs it right after the PIN is drawn (~2–5 s,
  during which the panel doesn't answer). The next panel session fetches
  again; nothing is fetched outside panel mode.
- Source: an unofficial RSS mirror of anthropic.com/news
  (`raw.githubusercontent.com/Olshansk/rss-feeds`). The feed is ~200 kB, so the
  device streams it and stops reading after the 10th item.
- A failed fetch keeps the headlines from the last successful one and
  says so under the list.
- Headline text and links are third-party: they're rendered as plain
  text, and only plain `https://` links become clickable.

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

Under Device → Wi-Fi connection.

- Edit SSID + password (blank password = keep current)
- **Scan** → sorted list of nearby 2.4 GHz networks (SSID, RSSI,
  lock icon, "saved" marker). Tap a row to populate the SSID field.
  The scan runs asynchronously on the device (~8 s from click to list,
  bound by the radio, which is off-channel while scanning) while the page
  polls for results, so the panel stays responsive.
- **Save Wi-Fi** persists to NVS; takes effect at the next poll (the
  current session stays on the old network).

## Alerts &amp; sound

Under Polling & alerts > Alerts & sound. Warning thresholds for the 5-hour
and 7-day windows each accept 50-99%.

Quiet hours uses a compact blue
switch beside its label and description, with the From/Until fields below.
The gray off state and white thumb follow the supplied Claude snapshot.
It supports keyboard operation and preserves unsaved changes during state
updates; **Save alerts** applies the warning thresholds, switch, and time
window together. Quiet hours supports overnight windows; matching From/Until
times are rejected before saving, including when the switch is off.

Alert sounds appears below Warning thresholds in this card, with Quiet hours
below the sound controls.

The Alert sounds slider, playback icons, and button hover, focus, and
playing states share the quiet-hours switch's blue accent. The volume value
uses the standard text color. The slider track is 6px thick, matching the
usage progress bars, with a 44px control height for touch operation.

- Volume slider 0–100 % — saves on release, maps to −40…0 dB in the
  ES8311 codec. 0 % is near-mute, 100 % is codec max.
- Six test-play buttons, one per alert WAV: `5h-warning`,
  `5h-depleted`, `5h-reset`, `7d-warning`, `7d-depleted`, `7d-reset`.
  Each click plays the WAV via LittleFS (blocks the panel for 1–3 s
  per play).

`/api/sounds/play` is allow-listed to those six names only — the panel
will never play an arbitrary file.

## Display &amp; time

Under Device → Display & time.

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

## Polling &amp; pauses

Under Polling & alerts > Polling & pauses.

- Poll interval, 1–5 min
- **Pause Hours**: a matching blue switch and From/Until fields pause automatic
  usage requests during a daily local-time window. Disabled by default, with
  `00:00`-`06:00` (12:00am-6:00am) as the initial times. Overnight windows are
  supported; matching From/Until times are rejected even while disabled.
  Independent of Quiet hours, which only silences sounds.
- **Save polling &amp; pauses** persists these settings to NVS. Each card
  submits only its own settings and preserves unsaved drafts in the other card.

Pause Hours replaces Break Hours in the panel, API (`pause_*`), and serial
commands (`pause`). Existing saved Break Hours values migrate to the new
NVS keys without resetting the window or switch; the midnight default only
applies when no schedule has been saved.

Settings changes do not force a re-poll; the UI picks them up on its
next `/api/state` tick. Pause Hours changes re-evaluate the next poll on exit
from panel mode, including when a pause is disabled or shortened.

During Pause Hours, Usage shows a pause notice and keeps the last cached
figures. Normal mode sleeps until the window ends without periodic Wi-Fi
connections or duplicate history samples. Button wakeups and an explicit
**Refresh now** (or serial `usage`) remain available. Panel and debug modes
remain awake as usual; deep-sleep power savings apply in Normal mode with
debug off. If the clock is unset, the firmware establishes time over NTP
first and checks the window before sending usage requests.

## Sign out

Clears the `sid` cookie and ends the session. Panel mode on the device
stays up until the normal exit (long BOOT, serial `panel`, 5 min idle,
or reboot).

A `401` from any authenticated endpoint returns the page to the PIN
screen. Connection failures show feedback, and state polling retries
while the dashboard is visible.

## Danger zone

Under Device → Device management. Each button uses a two-tap arm pattern: first tap shows "Tap again to
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
