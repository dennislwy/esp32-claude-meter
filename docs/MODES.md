# Operating modes

The firmware runs in one of four **modes** at a time, plus an orthogonal
**debug** flag that only matters in Normal mode.

| #   | Mode                | Wi-Fi radio                              | MCU                              | LED                        | When                                      |
| --- | ------------------- | ---------------------------------------- | -------------------------------- | -------------------------- | ----------------------------------------- |
| 1   | **Normal**          | Off between polls, STA for ~5 s per poll | Deep-sleeps between polls        | Off (or solid if debug on) | Default on battery and USB                |
| 2   | **Web Panel**       | STA, always on                           | Always awake                     | 1 Hz blink                 | User-triggered: long BOOT, serial `panel` |
| 3   | **AP provisioning** | SoftAP + DNS, always on                  | Always awake                     | 4 Hz blink                 | Boot with no Wi-Fi SSID stored            |
| 4   | **Powered off**     | Off                                      | Chip is off (VBAT latch dropped) | Off                        | Long PWR on battery                       |

## 1 — Normal

The dashboard. Wakes on a timer every `poll_min` minutes (default 2),
polls both Claude accounts, draws the ePaper, triggers any alerts, and
deep-sleeps again. Averages ~1 mA on battery (~17 days on a 400 mAh
pack).

**Pause Hours** (Polling & alerts > Polling & pauses) optionally pauses automatic
usage checks for a daily local-time window. It defaults to 00:00-06:00,
disabled, and preserves existing saved schedules after the Break Hours rename. Normal
mode defers its next timer wake until the window ends, retaining cached
usage and leaving Wi-Fi off; it does not wake every poll interval just to
skip a request. Overnight windows work; matching From/Until times are rejected.
Button wakeups still work. An explicit panel **Refresh now** or
serial `usage` overrides the pause. Quiet hours only mutes alerts and remains
independent. Panel/debug modes keep their existing awake behavior.

Serial configuration: `pause on`, `pause off`, and `pause 22:30-7:15` (or
`pause 22-8`). `status` shows the saved window and enabled state.

Button behaviour (awake only):
- Short **BOOT** → cycle to the next view
- Long **BOOT** (≥ 1 s alone) → enter Web Panel mode
- Long **PWR** (≥ 1 s alone, battery) → power off
- Long **BOOT + PWR** (≥ 1 s together) → toggle debug flag

Button behaviour (from deep sleep): `setup()` polls both pins for up to
`LONG_PRESS_MS` right after an `ext1` wake and classifies the press
accordingly — short BOOT = next view, long BOOT = enter panel mode
directly, PWR = power off (any duration), BOOT+PWR held = toggle debug.

### Debug flag

Orthogonal to mode: changes behaviour **within Normal mode** only.

| Debug on                       | Debug off                 |
| ------------------------------ | ------------------------- |
| Serial console at 115200       | `Serial.end()`            |
| Green LED solid while awake    | LED off                   |
| Stays awake, polls on schedule | Deep-sleeps between polls |

Auto-detected at cold boot: USB attached → on, battery boot → off.
Toggle anytime with long **BOOT + PWR**, or serial `debug off`.

In practice: "I'm working on it over USB" vs "it's on my desk doing its
job." Panel and Provisioning modes keep the MCU fully awake regardless
of this flag, so the flag is only meaningful in Normal mode.

## 2 — Web Panel

A STA-mode HTTP server on `claude-meter.local` for editing settings,
reviewing history, playing alert sounds, and triggering a refresh. See
[WEB_SERVER.md](WEB_SERVER.md).

Entry: long **BOOT** (alone) while awake, serial `panel`, or long BOOT
while in debug mode after a wake. Requires Wi-Fi to be configured.

Exit: long **BOOT** (same gesture that entered it), serial `panel`,
5 min idle, `/api/logout`, or any reboot.

LED: 1 Hz blink. Wi-Fi radio ~70-100 mA — a 400 mAh pack lasts 4-6 h in
this mode, which is why there's an idle auto-exit.

## 3 — AP provisioning

First-time Wi-Fi setup. The board comes up as an open SoftAP named
`claude-meter-XXXXXX` at `192.168.4.1`; a DNS hijack triggers the OS
captive-portal popup on connect. See the "Provisioning (Phase 2)"
section of [WEB_SERVER.md](WEB_SERVER.md).

Entry: boot with no `wifi.ssid` NVS key — typically on first boot or
after a factory reset (or after wiping the SSID with serial `ssid `).

Exit: user saves creds via the portal → `ESP.restart()` into Normal
mode. No idle timeout; the AP stays up until creds are saved or power
is removed.

LED: 4 Hz blink. Radio ~70-100 mA — same drain as panel mode.

## 4 — Powered off

VBAT latch dropped. The chip is actually off; nothing is running.

Entry: long **PWR** alone on battery. On USB the latch drop doesn't
remove 5 V, so the chip stays running and the firmware flips the debug
flag on.

Exit: single press of either button (re-latches VBAT, cold-boot).

## State transitions

```
                            cold boot
                               │
                               ▼
                    ┌─────────────────────┐
                    │ wifi.ssid empty?    │
                    └─────────┬───────────┘
                              │
                   yes ◄──────┴──────► no
                   │                   │
                   ▼                   ▼
            ┌─────────────┐    ┌─────────────┐
            │ Provision   │    │   Normal    │◄───────┐
            │ (4 Hz LED)  │    │             │        │
            └──────┬──────┘    └──┬───┬──────┘        │
                   │              │   │               │
          save creds│   long BOOT │   │ long PWR      │
          → restart │              │   │ (battery)    │
                   │              ▼   │               │
                   │        ┌──────────────┐          │
                   │        │  Web Panel   │          │
                   │        │  (1 Hz LED)  │          │
                   │        └──────┬───────┘          │
                   │               │ long BOOT /      │
                   │               │ 5 min idle       │
                   │               └──────────────────┘
                   │                   │
                   │                   ▼
                   │             ┌──────────────┐
                   │             │ Powered off  │
                   │             └──────┬───────┘
                   │                    │ any button
                   └────────────────────┤
                                        ▼
                                    (cold boot)
```

Debug toggle (long **BOOT + PWR**) can be invoked anywhere Normal mode is
active, awake or asleep, without changing the mode itself.
