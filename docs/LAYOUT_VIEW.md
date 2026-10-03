# Display layouts

200 x 200 px, 1-bit (black / white). All coordinates and sizes below match
`src/meter_ui.cpp`. The ASCII mock-ups are not to scale — they show
element order and relative placement.

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
| 5H resets SAT 3 Oct 01:10 in 1h 14m                       |   10 px UPPERCASE
| 7D resets FRI 9 Oct 13:00 in 6d 13h                       |
+===========================================================+ y=111  (2-px divider)
| Claude 2                                      2m ago      |
|                                                           |
|   +---------------------------------------------------+   |
|   |### 5-HOUR                   15%                   |   |
|   +---------------------------------------------------+   |
|   +---------------------------------------------------+   |
|   |############################### 7-DAY       92%   |   |
|   +---------------------------------------------------+   |
| 5H resets SAT 3 Oct 05:40 in 5h 44m                       |
| 7D resets FRI 9 Oct 13:00 in 6d 13h                       |
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
| 5H resets  | +60      | Montserrat 10 UPPERCASE  |
| 7D resets  | +72      | Montserrat 10 UPPERCASE  |

## Single-account view (R2 / Layout #1 style)

One account filling the area below the status bar. Switched to by a
short BOOT press, cycling dual -> account 1 -> account 2.

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
|   [R] Sat 3 Oct 01:10 in 1h 14m                           |   12 px Title case
|                                                           |
|                                                           |
|   7D                                             78%      |
|   +---------------------------------------------------+   |
|   |##########################################         |   |
|   +---------------------------------------------------+   |
|   [R] Fri 9 Oct 13:00 in 6d 13h                           |
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
| Reset line    | +49      | Montserrat 12 Title case |

Window pitch = 76 px (so `7D` starts 76 px below `5H`).

Account-header offsets (`singleView()`, relative to y = STATUS_BAR_H + 4 = 28):

| Element      | y offset | Font                 |
| ------------ | -------- | -------------------- |
| Account name | 0        | Montserrat 16, black |
| Age          | +2       | Montserrat 10, right |
| First window | +22      | starts at y = 50     |

## Pop-ups and notices

- **Wi-Fi failure popup** — `WifiState::Failed`. A 170 px bordered white
  box, centred over the view, title Montserrat 14, body Montserrat 12
  wrapped. Shows the SSID, failure reason, and (when the clock is valid)
  the retry time.
- **Setup notice** — shown instead of any view when the Wi-Fi SSID is
  blank or no account has a token: `Setup needed\nConnect USB and type
  help in the serial monitor`.

## Reset-line format

`formatResetLine()` builds strings like:

- Dual view: `5H resets SAT 3 Oct 01:10 in 1h 14m` (`DayCase::Upper`)
- Single view: `[R] Sat 3 Oct 01:10 in 1h 14m` (`DayCase::Title`)

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
