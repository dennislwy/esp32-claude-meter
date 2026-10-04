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

A fresh reading is taken once per draw in `src/main.cpp`
(`screen.batteryPercent = Battery::percentFromMillivolts(...)`). The
status bar in `src/meter_ui.cpp` picks one of five battery icons by
thresholding the number (≥90, ≥65, ≥40, ≥15, else empty).

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
- Load regulation isn't compensated. During Wi-Fi transmit bursts the
  pack voltage sags and the gauge temporarily drops.
- No coulomb counting. SoC resets each boot from whatever voltage the
  pack happens to read.

## Future upgrade path

If accurate SoC ever matters, the cheapest fix is a **MAX17048** or
LC709203F soldered across the pack and tied to the existing I²C bus
(SDA = GPIO47, SCL = GPIO48). Pick an address that doesn't collide
with 0x18 / 0x51 / 0x70. Firmware change is minimal — replace the
curve lookup in `lib/Battery/battery.cpp` with the fuel gauge's I²C
read. All call sites (`main.cpp:204`, `meter_ui.cpp:249`) are already
percent-based and don't need to change.
