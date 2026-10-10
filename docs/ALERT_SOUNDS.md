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

Each `evaluateAlerts()` call iterates every (account, window) pair and
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

## Display switch

`evaluateAlerts()` reports which accounts fired, and the caller switches the
ePaper to that account's single-account view before rendering. It applies only
when exactly one account fired: two accounts firing leaves the dual view, which
already shows both. Quiet hours suppresses the switch along with the sound.

The view reverts to the default on the next poll, so an alert view lasts one
poll interval (1-5 minutes). A view selected with `BOOT` is tracked separately
and keeps persisting across wakes indefinitely.

Panel mode evaluates and plays alerts but never switches the view — the ePaper
is showing the panel URL and sign-in PIN.

## Volume

`volumeDb()` in `audio_player.cpp` reads `settings::audioVolume()` (NVS
key `audio_vol`, default 80) and maps 0-100 % onto -40…0 dB on the
ES8311. 0 % is treated as mute (-60 dB). The level is applied per play,
after `codec.start()`, so a change takes effect on the next sound
without a restart.

The only way to change it is the **Alert sounds** slider under
Polling & alerts in the web panel; there is no serial command.

## Quiet hours

`settings::isQuietTime(hour, minute)` gates the final play step. Window
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
- **Sample rate:** any rate the ES8311 can be clocked at. The rate is
  read from the WAV header and the codec is reconfigured on each play,
  so 16000 Hz, 22050 Hz and 44100 Hz all work. The six shipped sounds
  are 16000 Hz mono.

### Why mono is chosen

Stereo *works*, but we convert source material to mono because:

1. **One speaker.** The board has a single voice coil driven by one
   amplifier (`PIN_SPEAKER_AMP = GPIO46`). Both I²S channels drive
   the same physical driver, so stereo imaging is physically
   impossible. `streamSamples()` in `audio_player.cpp` already
   duplicates mono samples to both I²S channels for exactly this
   reason — stereo content would just get summed in the air.
2. **Half the file size.** LittleFS lives in the same 8 MB flash as
   the firmware. Uncompressed 16-bit PCM costs 32 kB per second at
   16000 Hz mono, double that in stereo. The six shipped sounds run
   2.7-6.7 s and total 871 kB of the 1.5 MB partition — stereo would
   not fit, for zero audible benefit.
3. **Faster `uploadfs`** and less flash wear every time the WAVs
   change.
4. **Short alert chimes** aren't musical content — there is nothing
   to pan.

### Converting MP3 (or anything else) to the right WAV

Install [ffmpeg](https://ffmpeg.org/) (`winget install ffmpeg`,
`brew install ffmpeg`, `apt install ffmpeg`), then:

```sh
ffmpeg -i source.mp3 -ac 1 -ar 16000 -sample_fmt s16 5h-warning.wav
```

Flag reference:

| Flag | Meaning | Why |
| --- | --- | --- |
| `-ac 1` | Audio channels = 1 (mono) | See above |
| `-ar 16000` | Sample rate 16000 Hz | Plenty for short beeps; ~1/3 the data of 44100, and what the shipped sounds use |
| `-sample_fmt s16` | 16-bit signed PCM | What the player requires |

For a louder result without re-recording, add gain (ffmpeg clips at
0 dBFS, so measure first):

```sh
ffmpeg -i source.mp3 -ac 1 -ar 16000 -sample_fmt s16 -af "volume=6dB" 5h-warning.wav
```

For a one-shot batch (all six files at once from a `src_sfx/` dir):

```sh
for f in src_sfx/*.mp3; do
  ffmpeg -y -i "$f" -ac 1 -ar 16000 -sample_fmt s16 "data/$(basename "${f%.mp3}").wav"
done
```

Then verify what you got — the player prints the actual format on
each playback:

```
> play 5h-warning.wav
Played /5h-warning.wav: 16000 Hz, 1 ch, 6658 ms
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

- Volume is panel-only. No serial command sets it, so a board with no
  network access is stuck with whatever is stored in NVS.
- No crossfade or mix. If two different alerts qualify in the same
  poll, they play back-to-back.
- Playback blocks. `playWav()` streams to I²S synchronously, so the
  main loop stalls for the length of the sound.
