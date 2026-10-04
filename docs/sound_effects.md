# Sound effects

The board has an onboard ES8311 audio codec driving a small speaker.
The firmware uses it for a small set of edge-triggered usage alerts.

## Audio chain

| Stage | Component / pin |
| --- | --- |
| Codec | ES8311 on I²C `0x18` (shared bus) |
| MCLK / BCLK / WS | GPIO14 / GPIO15 / GPIO38 |
| I²S data to DAC | GPIO45 (`PIN_I2S_DOUT`) |
| I²S data from mic ADC | GPIO16 (`PIN_I2S_DIN`, unused by this firmware) |
| Speaker amplifier enable | GPIO46 (`PIN_SPEAKER_AMP`, HIGH = on) |
| Codec power rail enable | GPIO42 (`PIN_AUDIO_PWR`, active low) |

`src/audio_player.cpp` plays 16-bit PCM WAV files from LittleFS through
the codec. Any other WAV format fails with
`"<path> is not a 16-bit PCM WAV"` on the serial log.

## Sound files

Six WAVs live on LittleFS (uploaded via `pio run -t uploadfs`). They
are intentionally gitignored (`*.wav` in `.gitignore`) — the repo keeps
the firmware honest about licensing and lets each user drop in their
own audio.

| File | When it plays |
| --- | --- |
| `/5h-warning.wav` | 5-hour usage crosses the warning threshold |
| `/5h-depleted.wav` | 5-hour usage reaches 100 % |
| `/5h-reset.wav` | 5-hour window reset time passes |
| `/7d-warning.wav` | 7-day usage crosses the warning threshold |
| `/7d-depleted.wav` | 7-day usage reaches 100 % |
| `/7d-reset.wav` | 7-day window reset time passes |

The warning threshold is per-window (`warn5h`, `warn7d`; defaults 80 %
and 90 %). The depleted threshold is a fixed `99.95 %` — see
`DEPLETED_PERCENT` in `src/alerts.cpp`.

## Trigger logic (`src/alerts.cpp`)

Each `checkAlerts()` call iterates every (account, window) pair and
compares the current level (`NORMAL` / `WARNING` / `DEPLETED`) against
the last-seen level stored in NVS (namespace `alerts`, keys
`lvl<n>_<window>` and `rst<n>_<window>`).

- Rising edges only. A drop (after a reset, or a raised threshold)
  just updates the stored level; no re-alert.
- `DEPLETED` wins over `WARNING` when usage jumps straight to 100 %.
- Window-reset alerts fire when `now >= rst<n>_<window>` — the firmware
  records the previously-reported reset timestamp and waits for it to
  pass.
- Accounts whose latest poll failed are skipped.
- Multiple accounts can request the same WAV in a single poll; the
  sound queue de-duplicates and plays each sound once.

Successful plays go through `playWav(path)` in `audio_player.cpp`,
which powers up the amplifier, streams the file to I²S, and powers it
back down.

## Quiet hours

`settings::isQuietHour(hour)` gates the final play step. Window
`quiet 22-8` (default) suppresses playback from 22:00 to 08:00 local.
The alert *state* is still updated during quiet hours — this is
deliberate: when quiet hours end the firmware does **not** replay the
backlog. If 5-hour usage silently crossed the warning threshold at
02:00, there is no belated beep at 08:00. One miss, forever silent.

Quiet hours can be disabled entirely with `quiet off`.

## Serial commands

| Command | Effect |
| --- | --- |
| `alerts` | Print the stored alert level and pending reset time for each (account, window) |
| `alerts clear` | Wipe the `alerts` NVS namespace. The next poll re-alerts for anything above a threshold |
| `warn5h <50-99>` / `warn7d <50-99>` | Change the warning threshold per window |
| `quiet on` / `quiet off` | Enable or disable quiet hours |
| `quiet <start>-<end>` | Set the quiet-hours window (24-h local, e.g. `quiet 22-8`) |
| `files` | List files on LittleFS |
| `play <file.wav>` | Force-play a WAV directly (bypasses alerts) |

## First-time setup

A fresh board with no WAVs on LittleFS still runs — alerts just make
no sound. Upload the WAVs once with:

```sh
pio run -t uploadfs
```

LittleFS survives firmware flashes, so subsequent `pio run -t upload`
calls don't need `uploadfs`.

## Known limitations

- No volume control. Loudness is set entirely by the WAV mastering.
- No crossfade or mix. If two different alerts qualify in the same
  poll, they play back-to-back.
- No "test tone" command beyond `play`.
