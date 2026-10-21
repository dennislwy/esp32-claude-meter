# Display layouts

200 x 200 px, 1-bit (black / white). All coordinates and sizes below match
`src/meter_ui.cpp`. The ASCII mock-ups are not to scale — they show
element order and relative placement.

## Views at a glance

Every view is a `MeterView` (`src/meter_ui.h`), drawn by `meterUiShow()`.

| View              | Kind      | Drawn by        | Shown when                                                   |
| ----------------- | --------- | --------------- | ------------------------------------------------------------ |
| `Dual`            | Dashboard | `dualCard()` x2 | Both accounts have a token                                   |
| `Account1`        | Dashboard | `singleView()`  | Account 1 has a token                                        |
| `Account1History` | Dashboard | `historyView()` | Account 1 has a token and recorded history                   |
| `Account2`        | Dashboard | `singleView()`  | Account 2 has a token                                        |
| `Account2History` | Dashboard | `historyView()` | Account 2 has a token and recorded history                   |
| `Panel`           | Mode      | `panelView()`   | Web Panel mode is on (long BOOT, or no token after setup)    |
| `Setup`           | Mode      | `setupView()`   | No Wi-Fi SSID is stored: the AP captive portal is running    |
| `Unlock`          | Mode      | `unlockView()`  | Cold boot of a provisioned board: the Device PIN is needed   |

A short BOOT press (or serial `view`) cycles the dashboard views in table
order, skipping any whose condition is not met. The mode views are set by
code, never reached by cycling, and each replaces the dashboard until its
mode ends.

## Status bar (shared by every view)

Height 24 px, black background, white content.

```
0                                                       199
+-----------------------------------------------------------+ y=0
| [M] [fan]          12:34               [bat] 85%          |  <- 24 px black
+-----------------------------------------------------------+ y=24
```

| Element    | Position             | Font / size            | Notes                                           |
| ---------- | -------------------- | ---------------------- | ----------------------------------------------- |
| Mascot `M` | x = 4                | 16 x 16 bitmap         | `claude_icon`                                   |
| Wi-Fi fan  | x = 4 + mascot + 6   | 22 px wide, 15 px tall | Dot + up to 3 upward arcs (RSSI dependent)      |
| Clock      | centred              | Montserrat 16, white   | `HH:MM`, or `--:--` when the clock isn't set    |
| Battery    | right, 4 px pad      | Montserrat 12, white   | Icon + percent (full / 3 / 2 / 1 / empty)       |

Wi-Fi arc thresholds (lit from innermost out):

| RSSI               | Lit arcs |
| ------------------ | -------- |
| >= -60 dBm         | 3        |
| -70 to -61 dBm     | 2        |
| -80 to -71 dBm     | 1        |
| <  -80 dBm         | 0        |
| Connect failed     | 0 + `x`  |

## Dual view (both accounts, R2 / Layout #2)

Two cards stacked below the status bar, separated by a 2 px black divider.
Each card is 87 px tall.

```
0                                                       199
+-----------------------------------------------------------+ y=0
| [M] [fan]          12:34               [bat] 85%          |   status bar (24)
+-----------------------------------------------------------+ y=24
| Claude 1                                        now       |   name (14) / age (10)
|                                                           |
|   +---------------------------------------------------+   |
|   |########### 5-HOUR                      42%        |   |   split bar, 192 x 18
|   +---------------------------------------------------+   |
|   +---------------------------------------------------+   |
|   |###################### 7-DAY               78%    |   |
|   +---------------------------------------------------+   |
| 5H resets 1:10 in 1h 14m                                  |   10 px
| 7D resets Fri 13:00 in 6d 13h                             |
+===========================================================+ y=111  (2-px divider)
| Claude 2                                      2m ago      |
|                                                           |
|   +---------------------------------------------------+   |
|   |### 5-HOUR                   15%                   |   |
|   +---------------------------------------------------+   |
|   +---------------------------------------------------+   |
|   |############################### 7-DAY       92%   |   |
|   +---------------------------------------------------+   |
| 5H resets 5:40 in 5h 44m                                  |
| 7D resets Fri 13:00 in 6d 13h                             |
+-----------------------------------------------------------+ y=200
```

Split-bar rendering (`splitBar()`): a white rectangle with a black fill
proportional to usage %. The label (`5-HOUR` / `7-DAY`, Montserrat 12)
sits on the left and the value (Montserrat 14) on the right at a fixed
position. Both texts are drawn once in black on the white part and once
in white as children of the black fill, so the colour inverts cleanly at
the fill boundary.

Per-card y offsets (relative to the card's top-left):

| Element    | y offset | Font                     |
| ---------- | -------- | ------------------------ |
| Name       | +2       | Montserrat 14, black     |
| Age        | +4       | Montserrat 10, right     |
| 5-HOUR bar | +20      | 18 px tall               |
| 7-DAY bar  | +40      | 18 px tall               |
| 5H resets  | +60      | Montserrat 10            |
| 7D resets  | +72      | Montserrat 10            |

## Single-account view (R2 / Layout #1 style)

One account filling the area below the status bar. Reached by a short
BOOT press, in the cycle `Dual -> Acct1 -> Acct1 History -> Acct2 ->
Acct2 History -> Dual`.

```
0                                                       199
+-----------------------------------------------------------+ y=0
| [M] [fan]          12:34               [bat] 85%          |   status bar (24)
+-----------------------------------------------------------+ y=24
| Claude 1                                       now        |   name (16) / age (10)
|                                                           |
|   5H                                             42%      |   Montserrat 28
|   +---------------------------------------------------+   |
|   |#####################                              |   |   14 px bar
|   +---------------------------------------------------+   |
|   [R] 1:10 in 1h 14m                                      |   16 px
|                                                           |
|                                                           |
|   7D                                             78%      |
|   +---------------------------------------------------+   |
|   |##########################################         |   |
|   +---------------------------------------------------+   |
|   [R] Fri 13:00 in 6d 13h                                 |
|                                                           |
+-----------------------------------------------------------+ y=200
```

`[R]` is `LV_SYMBOL_REFRESH` (a small clockwise arrow glyph from LVGL's
built-in Font Awesome set).

Per-window y offsets (`singleWindow()`, relative to the window's top-left):

| Element       | y offset | Font                     |
| ------------- | -------- | ------------------------ |
| `5H` / `7D`   | 0        | Montserrat 28, black     |
| Value `NN%`   | 0        | Montserrat 28, right     |
| Bar track     | +32      | 14 px tall               |
| Reset line    | +49      | Montserrat 16            |

Window pitch = 76 px (so `7D` starts 76 px below `5H`).

Account-header offsets (`singleView()`, relative to y = STATUS_BAR_H + 4 = 28):

| Element      | y offset | Font                 |
| ------------ | -------- | -------------------- |
| Account name | 0        | Montserrat 16, black |
| Age          | +2       | Montserrat 10, right |
| First window | +22      | starts at y = 50     |

## History view (R7, per account)

A dedicated 7-day line chart, one per account. Reached from the view
cycle: `Dual -> Acct1 -> Acct1 History -> Acct2 -> Acct2 History -> Dual`.
A history view is skipped when its account has no recorded samples yet.

```
0                                                       199
+-----------------------------------------------------------+ y=0
| [M] [fan]          12:34               [bat] 85%          |   status bar (24)
+-----------------------------------------------------------+ y=24
| Claude 1                    5H --   7D ==                 |   title (14) + legend (10)
|                                                           |
| 100 -|                                                    |
|      |                                                    |
|  75 -|            . .                                     |
|      |         .    . . . .      . .                      |
|  50 -|    .  .            . .  .     .      . .           |
|      | .                       .      .  . .   . .        |
|  25 -|.                                           . .  .  |
|      |                                                    |
|   0 -+--|--|--|--|--|--|--|-------------------------------+
|         S  M  T  W  T  F  S                               |   single-letter days
+-----------------------------------------------------------+ y=200
```

Chart geometry:

| Element       | Value                 | Purpose                           |
| ------------- | --------------------- | --------------------------------- |
| `CHART_COLS`  | 168                   | One column per hour (`HIST_SLOTS / 2`) |
| `CHART_LEFT`  | 6                     | Left edge of plot                 |
| `CHART_RIGHT` | 174                   | Right edge (= LEFT + COLS)        |
| `CHART_TOP`   | 50                    | Top of plot                       |
| `CHART_BOTTOM`| 170                   | Baseline (0 %)                    |
| `CHART_H`     | 120                   | Plot height, 120 px               |

Data pipeline:

1. `historySnapshot(index, buf, newest)` fills a 336-slot stack buffer
   oldest-to-newest.
2. Downsample 2 adjacent slots per column (`max`, so sawtooth peaks
   survive): `cols[c] = max(buf[2c], buf[2c+1])`.
3. For each series, group contiguous non-`HIST_EMPTY` columns and emit
   one `lv_line` widget per segment. `HIST_EMPTY` columns become line
   breaks, so off-device gaps stay visible.

Series styling:

| Series | Line width | Static buffer         |
| ------ | ---------- | --------------------- |
| 5-hour | 1 px       | `points5h[168]`       |
| 7-day  | 2 px       | `points7d[168]`       |

Y-axis ticks and labels at 25 % intervals (`0 / 25 / 50 / 75 / 100`),
right of `CHART_RIGHT`. The baseline (`CHART_BOTTOM`, 0 %) carries the
`0` label but no extra tick (the axis itself is the tick).

X-axis: one tick (1 px x 2 px) at each local midnight, 24 columns apart.
The 168-column window always holds exactly 7 midnights, so there are 7
ticks. Each tick carries the single-letter initial of the day that starts
there (`S / M / T / W / T / F / S`; duplicates for Sun/Sat and Tue/Thu are
intentional), centred under the tick by its measured width. The rightmost
tick is today's midnight, at column `167 - <current hour>`. The partial
day left of the first tick has no label. Weekdays are counted back from
today's rather than derived from `newest - d * 86400`, so a DST change
cannot shift a label by a day.

Visibility of a line segment requires >= 2 adjacent valid columns, i.e.
roughly 2 hours of continuous polling after a cold boot.

## Panel view (Web Panel mode)

Shown while the Web Panel is serving on your Wi-Fi. The QR code encodes
`http://<ip>` — the IP, not the `.local` name, because Android browsers
don't resolve mDNS — so a phone camera opens the panel directly.

```
0                                                       199
+-----------------------------------------------------------+ y=0
| [M] [fan]          12:34               [bat] 85%          |   status bar (24)
+-----------------------------------------------------------+ y=24
|  +-----------+                                            | y=30
|  |▓▓ ▓ ▓▓▓ ▓▓|         Web Panel                          |   Montserrat 16
|  |▓ ▓▓▓  ▓ ▓ |                                            |
|  |▓▓▓ QR ▓▓▓ |            PIN                             |   Montserrat 10
|  |▓  ▓ ▓▓ ▓▓ |          123456                            |   Montserrat 28
|  |▓▓ ▓▓▓ ▓ ▓▓|                                            |
|  +-----------+                                            | y=105
|                                                           |
|              http://claude-meter.local                    | y=115  14 / 12 / 10
|               http://192.168.1.200                        | y=133  Montserrat 14
|                                                           |
|                 Long BOOT = exit                          | y=168  Montserrat 12
|               Wi-Fi on, drains fast                       | y=183  Montserrat 12
+-----------------------------------------------------------+ y=200
```

| Element            | Position                          | Font / size              | Notes                                              |
| ------------------ | --------------------------------- | ------------------------ | -------------------------------------------------- |
| QR code            | x = 8, y = 30                     | 75 x 75 px               | `lv_qrcode`; drawn only when the IP is known       |
| `Web Panel` title  | column centre, y = 32             | Montserrat 16            | Column runs x = 89..196, centre x = 142            |
| `PIN` label        | column centre, y = 58             | Montserrat 10            |                                                    |
| Sign-in code       | column centre, y = 70             | Montserrat 28            | No fixed width, so a wide code spills, never wraps |
| `.local` URL       | centred, y = 115                  | 14, else 12, else 10     | Largest size that fits 192 px; dots if none fits   |
| IP URL             | centred, y = 115 + URL line + 2   | Montserrat 14            | y = 133 with the 14 px URL font                    |
| Exit hint          | bottom-centred, one line up       | Montserrat 12            | `Long BOOT = exit`                                 |
| Battery warning    | bottom-centred, 2 px above edge   | Montserrat 12            | `Wi-Fi on, drains fast`                            |

QR sizing: any IP URL (`http://255.255.255.255` at most, 22 bytes) is a
version 2 code at LVGL's ECC level M — 25 modules — so 75 px is exactly
3 px per module. The white screen around the code is its quiet zone. The
widest six-digit code at 28 px is ~3 px wider than the 107 px column;
centring lets it spill evenly into the margins.

## Setup view (AP provisioning mode)

Shown while the board runs its own open access point for first-time
setup, or after a factory reset.

```
0                                                       199
+-----------------------------------------------------------+ y=0
| [M] [fan]          12:34               [bat] 85%          |   status bar (24)
+-----------------------------------------------------------+ y=24
|                     Wi-Fi setup                           | y=28   Montserrat 16
|                                                           |
|                  Join this network:                       | y=52   Montserrat 12
|                 claude-meter-D81A24                       | y=70   Montserrat 14
|                 (open, no password)                       | y=90   Montserrat 10
|                                                           |
|                      Then open:                           | y=112  Montserrat 12
|                  http://192.168.4.1                       | y=130  Montserrat 14
|                                                           |
|                                                           |
|           Most phones show it automatically               | y=187  Montserrat 10
+-----------------------------------------------------------+ y=200
```

Every line is centred across `CONTENT_W` (192 px), offset from
y = STATUS_BAR_H + 4 = 28:

| Element               | y offset | y   | Font          |
| --------------------- | -------- | --- | ------------- |
| `Wi-Fi setup`         | 0        | 28  | Montserrat 16 |
| `Join this network:`  | +24      | 52  | Montserrat 12 |
| AP SSID               | +42      | 70  | Montserrat 14 |
| `(open, no password)` | +62      | 90  | Montserrat 10 |
| `Then open:`          | +84      | 112 | Montserrat 12 |
| `http://192.168.4.1`  | +102     | 130 | Montserrat 14 |
| Footer                | bottom   | 187 | Montserrat 10 |

The SSID is `claude-meter-` plus the last three bytes of the eFuse Wi-Fi
MAC in hex, the same MAC the Web Panel shows under Device details.

## Unlock view (PIN entry)

Shown on a cold boot of a provisioned board, until the Device PIN is
entered on the buttons. It has two screens.

**PIN entry** — BOOT changes the digit under the cursor, PWR confirms it:

```
0                                                       199
+-----------------------------------------------------------+ y=0
| [M] [fan]          --:--               [bat] 85%          |   status bar (24)
+-----------------------------------------------------------+ y=24
|                       Locked                              | y=30   Montserrat 16
|                                                           |
|               Enter your 4-digit PIN                      | y=54   Montserrat 12
|                                                           |
|                                                           |
|                    *  *  3  _                             | y=86   Montserrat 28
|                                                           |
|                                                           |
|                 BOOT = change digit                       | y=165  Montserrat 10
|                 PWR = confirm digit                       | y=176  Montserrat 10
|                Hold both 5 s = reset                      | y=187  Montserrat 10
+-----------------------------------------------------------+ y=200
```

| Element       | y   | Font          | Notes                                                             |
| ------------- | --- | ------------- | ----------------------------------------------------------------- |
| `Locked`      | 30  | Montserrat 16 |                                                                   |
| Message       | 54  | Montserrat 12 | `Enter your 4-digit PIN`, `Wrong PIN - try again`, or `Checking...` |
| Digit slots   | 86  | Montserrat 28 | Confirmed `*`, active slot shows its digit, rest `_`, space-separated |
| Hints (x3)    | 165 / 176 / 187 | Montserrat 10 | Stacked up from 2 px above the bottom edge, 11 px apart |

`Checking...` shows once the fourth digit is confirmed, while the PIN
derivation (~400 ms) runs.

**Factory-reset confirm** — BOOT + PWR held together for 5 s switches the
view to this screen. PWR erases, BOOT cancels back to PIN entry:

```
+-----------------------------------------------------------+ y=24
|                   Factory reset?                          | y=30   Montserrat 16
|                                                           |
|              Erases Wi-Fi, both tokens,                   | y=60   Montserrat 12
|                the PIN, all settings                      |        3 lines, wrapped
|                     and history.                          |
|                                                           |
|            PWR = reset    BOOT = cancel                    | y=187  Montserrat 10
+-----------------------------------------------------------+ y=200
```

## Pop-ups and notices

Neither is a view: both are drawn over (or instead of) a dashboard view,
and never over the Panel, Setup or Unlock views.

- **Wi-Fi failure popup** — `WifiState::Failed`. A 170 px bordered white
  box, centred over the view, title Montserrat 14, body Montserrat 12
  wrapped. Shows the SSID, failure reason, and (when the clock is valid)
  the retry time.
- **Setup notice** — shown instead of a dashboard view when the Wi-Fi SSID
  is blank or no account has a token: `Setup needed\nConnect USB and type
  help in the serial monitor`, Montserrat 14, centred below the status
  bar.

## Reset-line format

`formatResetLine()` builds strings like:

- Dual view: `5H resets 1:10 in 1h 14m`, `7D resets Fri 13:00 in 6d 13h`
- Single view: `[R] 1:10 in 1h 14m`, `[R] Fri 13:00 in 6d 13h`

The weekday appears only on the 7D line, and only while the reset is a
day or more away (`7D resets 6:00 in 22h 1m` under that). Without a day
part the countdown settles which day `1:10` falls on, so the 5H line, which
always resets within five hours, never shows one. Times are
24-hour with no leading zero on the hour. The time of day comes from
`formatResetWhen()` in `src/reset_time.cpp`, covered by the host check in
`test/reset_time/`.

Countdown shows the two most significant units via `formatDuration()`:

| Elapsed  | Formatted  |
| -------- | ---------- |
| 2 days   | `2d 15h`   |
| 4 hours  | `4h 34m`   |
| 12 min   | `12m`      |

When the reset time has passed, the "in ..." tail becomes `(now)`.

## Constants

| Constant                  | Value | Purpose                                   |
| ------------------------- | ----- | ----------------------------------------- |
| `WIDTH / HEIGHT`          | 200   | Panel size                                |
| `STATUS_BAR_H`            | 24    | Status bar height                         |
| `PAD`                     | 4     | Outer horizontal padding                  |
| `DIVIDER_H`               | 2     | Divider between dual-view cards           |
| `CARD_H`                  | 87    | `(200 - 24 - 2) / 2`                      |
| `SPLIT_BAR_H`             | 18    | Dual-view bar height                      |
| `THIN_BAR_H`              | 14    | Single-view bar height                    |
| `SINGLE_WINDOW_PITCH`     | 76    | Gap between 5H and 7D blocks              |
| `POPUP_W`                 | 170   | Pop-up frame width                        |
| `CHART_COLS`              | 168   | History columns (one per hour)            |
| `CHART_LEFT / RIGHT`      | 6/174 | History plot x bounds                     |
| `CHART_TOP / BOTTOM`      | 50/170| History plot y bounds                     |
| `QR_SIZE`                 | 75    | Panel-view QR code, 3 px per module       |
| `QR_X / QR_GAP`           | 8/6   | QR left edge; gap to the title/PIN column |
