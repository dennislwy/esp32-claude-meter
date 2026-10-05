# Alert sounds

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

## Preparing audio files

### Format requirements

The player (`src/audio_player.cpp`) accepts WAVs that satisfy **all**
of the following, otherwise it logs `"<path> is not a 16-bit PCM WAV"`
and skips the sound:

- **Container:** standard `RIFF`/`WAVE`
- **Encoding:** PCM (format code `1`); no µ-law, A-law, ADPCM, or
  float
- **Bit depth:** exactly 16 bits per sample
- **Channels:** 1 (mono) or 2 (stereo). The player accepts both.
- **Sample rate:** any rate the ES8311 can be clocked at. 22050 Hz
  and 44100 Hz are both fine. The sample rate is read from the WAV
  header and the codec is reconfigured on each play.

### Why mono is chosen

Stereo *works*, but we convert source material to mono because:

1. **One speaker.** The board has a single voice coil driven by one
   amplifier (`PIN_SPEAKER_AMP = GPIO46`). Both I²S channels drive
   the same physical driver, so stereo imaging is physically
   impossible. `streamSamples()` in `audio_player.cpp` already
   duplicates mono samples to both I²S channels for exactly this
   reason — stereo content would just get summed in the air.
2. **Half the file size.** LittleFS lives in the same 8 MB flash as
   the firmware. A 2-second 22 kHz chime is ~88 kB mono or ~176 kB
   stereo. Six alerts × ~90 kB each stays comfortably under the
   data partition; stereo doubles that for zero audible benefit.
3. **Faster `uploadfs`** and less flash wear every time the WAVs
   change.
4. **Short alert chimes** aren't musical content — there is nothing
   to pan.

### Converting MP3 (or anything else) to the right WAV

Install [ffmpeg](https://ffmpeg.org/) (`winget install ffmpeg`,
`brew install ffmpeg`, `apt install ffmpeg`), then:

```sh
ffmpeg -i source.mp3 -ac 1 -ar 22050 -sample_fmt s16 5h-warning.wav
```

Flag reference:

| Flag | Meaning | Why |
| --- | --- | --- |
| `-ac 1` | Audio channels = 1 (mono) | See above |
| `-ar 22050` | Sample rate 22050 Hz | Plenty for short beeps; halves the data vs 44100 |
| `-sample_fmt s16` | 16-bit signed PCM | What the player requires |

For a louder result without re-recording, add gain (ffmpeg clips at
0 dBFS, so measure first):

```sh
ffmpeg -i source.mp3 -ac 1 -ar 22050 -sample_fmt s16 -af "volume=6dB" 5h-warning.wav
```

For a one-shot batch (all six files at once from a `src_sfx/` dir):

```sh
for f in src_sfx/*.mp3; do
  ffmpeg -y -i "$f" -ac 1 -ar 22050 -sample_fmt s16 "data/$(basename "${f%.mp3}").wav"
done
```

Then verify what you got — the player prints the actual format on
each playback:

```
> play 5h-warning.wav
Played /5h-warning.wav: 22050 Hz, 1 ch, 1240 ms
```

If you see `2 ch`, the file is stereo (still plays fine, just twice
the size). If `play` reports `not a 16-bit PCM WAV`, re-encode with
the flags above.

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
