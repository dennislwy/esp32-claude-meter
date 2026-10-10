# Battery gauge

## Hardware

The Waveshare ESP32-S3-ePaper-1.54 board carries only a basic Li-ion
charge-management circuit, not a dedicated fuel-gauge IC. The three
devices on the shared I²C bus (SHTC3 at `0x70`, PCF85063 RTC at `0x51`,
ES8311 codec at `0x18`) have no battery-related function.

| Component | Part | Role |
| --- | --- | --- |
| Charger | ETA6098 (linear, single-cell) | CC/CV charge, status LED drive, no I²C |
| Voltage sense | GPIO4 behind R21/R38 (200 kΩ / 200 kΩ) | 1:2 divider to ADC1 channel |
| Fuel gauge | — | **none** |

Installed pack on this device: **3.7 V nominal, 400 mAh, 1.48 Wh, 502535
pouch cell** (5 × 25 × 35 mm).

## How the percentage is computed

`lib/Battery/battery.cpp`:

1. `analogReadMilliVolts(PIN_BATTERY_ADC)` is averaged over 16 samples
   (ESP32 built-in eFuse calibration gives a reading in mV).
2. The average is multiplied by `BATTERY_DIVIDER_RATIO = 2.0` to recover
   pack voltage.
3. `percentFromMillivolts()` walks a 13-point lookup table, finds the
   two bracketing voltages, and linearly interpolates between their
   percent values.

The status bar in `src/meter_ui.cpp` picks one of five battery icons by
thresholding the number (≥90, ≥65, ≥40, ≥15, else empty).

## When the reading is taken

A voltage-only gauge is only as good as the moment it samples, so
`sampleBattery()` in `src/main.cpp` caches a percentage at the two
quietest points in the cycle and `render()` reads the cache:

- **Top of `setup()`**, before the display and audio rails come up.
  Straight out of deep sleep the pack has been resting at microamps for
  a whole poll interval, which is as close to open-circuit voltage as
  this board can get.
- **Top of `pollAndShow()`**, before `poll()` powers up Wi-Fi. In debug
  mode the board stays awake across many polls, so the `setup()` sample
  would otherwise go stale.

Sampling inside `render()` would be worse than it looks. `pollUsage()`
already calls `wifiOff()` before returning (`src/usage_poll.cpp`), so
the radio is down by then — but a Li-ion cell relaxes toward its resting
voltage over seconds to tens of seconds after a load drops. A reading
taken immediately after a multi-second TLS session still sits low on the
curve and under-reports.

Two call sites deliberately still sample live, because they report an
instantaneous voltage next to the percentage and are diagnostics rather
than the user-facing gauge: the serial `status` command and the panel's
Device card (`src/panel.cpp`). In panel mode Wi-Fi is up continuously,
so those readings are expected to sit a few percent below the ePaper's.

## The shifted curve

Current table (highest voltage first):

| mV | % |
| --- | --- |
| 4170 | 100 |
| 4120 | 95 |
| 4080 | 90 |
| 4000 | 80 |
| 3950 | 70 |
| 3870 | 60 |
| 3840 | 50 |
| 3800 | 40 |
| 3770 | 30 |
| 3730 | 20 |
| 3690 | 10 |
| 3610 | 5 |
| 3300 | 0 |

### Why the top isn't 4.20 V

A textbook Li-ion curve puts 100 % at 4.20 V — the CC/CV target the
charger holds while the pack is on USB. The moment the charger
terminates, however:

- surface charge dissipates in seconds; voltage drops to ~4.17 V,
- then sags under even a 1 mA load to ~4.10 V within a few minutes.

With a voltage-only gauge there is no way to distinguish "charging at
4.20 V" from "resting under load at 4.10 V". Keeping the top anchored
at 4.20 V meant a fully-charged pack read **88 %** immediately after
unplug, which looks broken.

The curve now treats **4.17 V = 100 %**. A rested, fully-charged pack
should land in the 97–100 % band. The lower half of the curve
(where the empty-warnings live) is unchanged, so the trade-off is
contained to the top ~50 mV.

### Trade-off

A pack that was only charged to ~95 % and then rested will also read
100 %. For a usage meter this is irrelevant — the moment the device
runs on battery and voltage falls below ~4.12 V, the percentage
reflects reality again.

## Charge time (Waveshare's figures)

Per the vendor FAQ for this board, from empty:

- **30 min** to reach 4.1 V (CC phase done)
- **44 min** to full charge (CV taper complete)

At 400 mAh that implies a ~360 mA linear charge rate, consistent with
how the ETA6098 is typically programmed.

## Runtime expectations (Waveshare's figures)

- Deep sleep + wake every 60 s: **~4 days** per charge
- Continuous low-power mode: **~10 days** per charge
- Measured board current in low-power mode: **1 mA**

The meter's actual duty cycle (Wi-Fi poll every 2–5 min, ePaper
partial refresh, occasional WAV playback) sits between those two
figures. At 400 mAh / 1.48 Wh the theoretical ceiling with a 1 mA
average draw is ~400 h ≈ 16 days.

## Known limitations

- No charge-complete signal to the MCU. The ETA6098's `STAT` pin
  drives one half of the dual LED on the board, not an ESP32 GPIO.
  The firmware can't tell "charging" from "charged" from the voltage
  reading alone.
- Load regulation isn't compensated. The sleep-cycle gauge dodges this
  by sampling only while the radio is off, but the live diagnostics in
  `status` and the panel still read a sagged pack.
- No coulomb counting. SoC resets each boot from whatever voltage the
  pack happens to read.

## Open question: a full pack reads 90 %, not 100 %

Unresolved as of 2026-10-10. After 11 hours on USB the gauge reported
4.08 V / 90 %, so a voltage trend was logged every 30 s for 6.5 minutes
with the board awake in debug mode:

```
22:10:36  4.08 V      22:14:06  4.08 V
22:11:36  4.08 V      22:14:36  4.08 V
22:12:06  4.08 V      22:15:07  4.08 V
22:12:36  4.08 V      22:15:37  4.08 V
22:13:06  4.08 V      22:16:07  4.08 V
22:13:36  4.08 V      22:16:37  4.08 V
```

The reading never moved. (The percentage flickers between 89 and 90
because 4080 mV is exactly the `{4080, 90}` table entry, so a few mV
either side flips the digit.)

Flat rules out the two obvious explanations. A terminated charger with
the board drawing current would show a slow decline toward the recharge
threshold; a recharge cycle would show a rise. A node that holds steady
for 6.5 minutes under load is being actively regulated, which means the
ETA6098 is sitting in CV.

That points at a **systematic underread of roughly 100-120 mV**. If the
charger holds its 4.2 V setpoint and the firmware measures 4.08 V, the
error is ~2.9 % — enough to cancel out the deliberate 4.17 V = 100 %
shift above, so a fully-charged pack reads 90 %. Candidate causes, none
yet confirmed:

- The 200 kΩ / 200 kΩ divider presents a 100 kΩ source impedance to the
  SAR ADC. The ESP32 prefers a much stiffer source; an incompletely
  charged sampling capacitor reads low.
- eFuse calibration error, typically worth a few tens of mV at the
  ~2.04 V the ADC actually sees, doubled by the divider ratio.
- Real IR drop between the cell and the sense point, if any series
  element sits between them.

**To resolve:** measure the pack with a multimeter at the battery
terminals while USB is connected. ~4.2 V means the gauge has a fixed
offset worth correcting (either in `BATTERY_DIVIDER_RATIO` or as a
calibration constant). ~4.08 V means the charger's CV setpoint is lower
than assumed and the top of the curve should move instead. Do not adjust
the curve until a reference measurement exists — the two fixes pull in
opposite directions.

## Future upgrade path

If accurate SoC ever matters, the cheapest fix is a **MAX17048** or
LC709203F soldered across the pack and tied to the existing I²C bus
(SDA = GPIO47, SCL = GPIO48). Pick an address that doesn't collide
with 0x18 / 0x51 / 0x70. Firmware change is minimal — replace the
curve lookup in `lib/Battery/battery.cpp` with the fuel gauge's I²C
read. A real gauge also reports under load, so `sampleBattery()`'s
timing would stop mattering. All call sites are already percent-based
and don't need to change.
