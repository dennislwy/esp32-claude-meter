# LED and buttons

Two buttons and one LED. All logic lives in `src/main.cpp`
(`handleBootButton`, `handlePowerButton`, `applyDebugMode`, and the
cold-boot/wake branches in `setup()`). Pin assignments are in
`src/board_pins.h`.

## Hardware

| Signal          | Pin    | Polarity               | Notes                                                                                                             |
| --------------- | ------ | ---------------------- | ----------------------------------------------------------------------------------------------------------------- |
| **BOOT** button | GPIO0  | Active low             | Also the ESP32-S3 strapping pin — the chip enters its ROM bootloader if it's held during reset                    |
| **PWR** button  | GPIO18 | Active low             | Wired alongside the latch so a long press cuts VBAT and switches the board off                                    |
| **Green LED**   | GPIO3  | Active low (LOW = lit) | Anode to 3V3 via 24 kΩ; the dual-LED's other half is driven by the charger IC's `STAT` pin and shows charge state |
| **VBAT latch**  | GPIO17 | HIGH = powered         | Firmware pulls HIGH at cold boot so the board stays on after the user releases PWR                                |

Both buttons share one `ext1` wake source: either one going LOW wakes the
board. `esp_sleep_get_ext1_wakeup_status()` tells which pin caused the
wake.

## LED

The green LED is a **debug-mode indicator**, not a status light.

| Device state                     | LED                                                                   |
| -------------------------------- | --------------------------------------------------------------------- |
| Debug mode on, awake             | **Lit** (solid)                                                       |
| Debug mode off, awake            | Off                                                                   |
| Web Panel mode                   | **Blinks at 1 Hz** — overrides debug-mode state while active          |
| AP captive portal (provisioning) | **Blinks at 4 Hz** — overrides debug-mode state while active          |
| Deep sleep (any mode)            | Off — the pin is held LED_OFF via `gpio_hold_en` to prevent glitching |

The two blink rates let you tell the two "awake with a web server" modes
apart at a glance: slow blink = panel on home Wi-Fi, fast blink = AP
setup portal. When either mode exits, `applyDebugMode()` restores the
LED to its solid/off state based on the debug flag.

Debug mode is set at cold boot from `usbHostConnected()`: USB attached →
debug on, battery boot → debug off. It can be toggled awake-or-asleep by
long-pressing **BOOT + PWR together** (see below). Serial over USB is
also gated by debug mode — no debug, no serial.

The charger's STAT LED (the other colour in the dual-LED package) is
separate: lit while charging over USB, off when the pack is full or no
USB is connected.

## Buttons

### BOOT (short press, awake)

| State          | Action                                                                                                                               |
| -------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| Normal views   | Cycle to the next view (Dual → Account 1 → Account 1 history → Account 2 → Account 2 history → Dual). Unavailable views are skipped. |
| Web Panel mode | No-op (use a long press to exit; this prevents accidental taps from killing the session).                                            |

A short press is anything between `BUTTON_MIN_PRESS_MS` (30 ms, de-bounce
floor) and `LONG_PRESS_MS` (1000 ms).

### BOOT (long press, alone, awake)

**Toggles Web Panel mode** — same as the serial `panel` command.

- Not in panel → connect Wi-Fi, start HTTP server, show PIN on ePaper
- In panel → shut down the server, drop Wi-Fi, return to the previous view

See [WEB_SERVER.md](WEB_SERVER.md). Entering requires Wi-Fi to be
configured (empty SSID → the serial reports `Panel: Wi-Fi not
configured`).

### PWR (long press, alone, awake)

Powers the board off by dropping the VBAT latch. On USB power the chip
stays running — the firmware falls through to a `debugMode = true`
branch so the board becomes a serial-attached dev kit.

### BOOT + PWR (long press, both held together)

**Toggles debug mode**, awake or asleep.

- From awake: flips the flag, applies it (serial up/down, LED on/off),
  prints a line to serial.
- From deep sleep: the wake branch in `setup()` detects both pins LOW at
  wake, waits up to `LONG_PRESS_MS` to confirm the combo, then flips
  debug mode and continues the normal wake path (poll, show, sleep
  again if debug just turned off).

This gesture is the only way to turn debug mode on from a battery boot,
and the only way to turn it off without a serial `debug off` command.

### BOOT or PWR (any press, from deep sleep)

Wakes the board via `ext1`. What happens next depends on which pin
tripped the wake and how long it's held — `setup()` polls both pins for
up to `LONG_PRESS_MS` right after wake, same as the combo detection.

| Wake pin                  | Behaviour                                                                                                                        |
| ------------------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| BOOT, released before 1 s | Cycle to the next view (poll first if a poll is due), then deep-sleep until the next scheduled poll.                             |
| BOOT, held ≥ 1 s          | Enter Web Panel mode directly (requires Wi-Fi configured). Exits via another long BOOT; deep-sleep resumes if debug mode is off. |
| PWR, any duration         | Run `powerOff()`. On battery, VBAT drops and the board stops. On USB, the board stays running and debug mode is forced on.       |
| BOOT + PWR held ≥ 1 s     | Toggle debug mode (combo). Overrides either single-pin behaviour.                                                                |

## State diagram

```
                          (any button or timer)
          ┌───────────────────────────────────────────┐
          │                                           │
     ┌────▼─────┐  long BOOT alone        ┌───────────┴──┐
     │  Awake   ├────────────────────────►│  Web Panel   │
     │  normal  │◄────────────────────────┤  mode        │
     └────┬─────┘  long BOOT (or idle)    └──────────────┘
          │
          │ long PWR alone (on battery)
          ▼
     ┌──────────┐
     │ Powered  │
     │   off    │
     └──────────┘

   long BOOT+PWR anywhere   →  toggle debug mode (LED on/off, serial on/off)
```

Web Panel mode has its own timeout; see [WEB_SERVER.md](WEB_SERVER.md).
Provisioning mode (fresh boot with no Wi-Fi SSID) stays awake
indefinitely and the buttons behave as "awake normal" above, minus the
long-BOOT panel entry (there's nothing to log into yet).

## Timing constants

Defined at the top of `src/main.cpp`:

| Constant              | Value   | Meaning                                                        |
| --------------------- | ------- | -------------------------------------------------------------- |
| `LONG_PRESS_MS`       | 1000 ms | Threshold for all long-press gestures                          |
| `BUTTON_MIN_PRESS_MS` | 30 ms   | Below this, releases are treated as contact bounce and ignored |
| `USB_DETECT_MS`       | 5 ms    | How long `usbHostConnected()` samples the USB D+ line          |
