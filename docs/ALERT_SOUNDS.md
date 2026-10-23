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

### Sound themes

Each theme is a folder of the same six WAVs under `sounds/`. The
original audio a theme was made from (MP3s, credits) lives in a
matching folder under `sound-sources/`, so it is never flashed. The
audio itself is gitignored (`*.wav`, `*.mp3`), and `sound-sources/` is
ignored outright, so a fresh clone has to supply its own sounds.

```
sounds/
  jarvis/          the six WAVs, nothing else
  mario/
sound-sources/
  jarvis/          5h-warning.mp3 ... and README.md
```

`data_dir` in the `[platformio]` section of `platformio.ini` picks the
theme `uploadfs` flashes:

```ini
[platformio]
data_dir = sounds/jarvis
```

`uploadfs` packs *everything* in that folder, subfolders included, into
the 1.5 MB partition — keep a theme folder to its six WAVs. To flash a
different theme once without editing the file:

```sh
PLATFORMIO_DATA_DIR=sounds/mario pio run -t uploadfs
```

That wipes the usage history, which shares the partition — see
[Changing themes without losing the
history](#changing-themes-without-losing-the-history).

A new theme is a new `sounds/<name>/` folder with the six filenames
below; the firmware plays fixed paths and needs no change.

| File | When it plays |
| --- | --- |
| `/5h-warning.wav` | 5-hour usage crosses the warning threshold |
| `/5h-depleted.wav` | 5-hour usage reaches 100 % |
| `/5h-reset.wav` | 5-hour window resets: its reset time passes, or its usage drops |
| `/7d-warning.wav` | 7-day usage crosses the warning threshold |
| `/7d-depleted.wav` | 7-day usage reaches 100 % |
| `/7d-reset.wav` | 7-day window resets: its reset time passes, or its usage drops |

The warning threshold is per-window (`warn5h`, `warn7d`; defaults 80 %
and 90 %). The depleted threshold is a fixed `99.95 %` — see
`DEPLETED_PERCENT` in `src/alerts.cpp`.

## Trigger logic (`src/alerts.cpp`)

Each `evaluateAlerts()` call iterates every (account, window) pair and
compares the current level (`NORMAL` / `WARNING` / `DEPLETED`) against
the last-seen level stored in NVS (namespace `alerts`, keys
`lvl<n>_<window>` and `rst<n>_<window>`).

- Rising edges only, for level alerts. A drop (after a reset, or a
  raised threshold) just updates the stored level; no warning or
  depleted re-alert. The drop can still raise a reset alert — see
  below.
- `DEPLETED` wins over `WARNING` when usage jumps straight to 100 %.
- Window-reset alerts have two triggers. The first is
  `now >= rst<n>_<window>`: the firmware records the previously
  reported reset timestamp and waits for it to pass. The second is a
  fall in utilisation of more than half a percentage point since the
  previous evaluated poll — utilisation is cumulative within a window,
  so it can only fall when the window was replaced early. That is what
  a gifted usage-limit reset does. `isEarlyWindowReset()` in
  `src/window_reset.cpp` decides the second case, and it is suppressed
  when the first already fired for the same window in the same pass, so
  one reset never chimes twice.
- For detecting an early reset, only the fall is used, never the reset
  timestamp moving. Measured 2026-10-22: a gift moved the 5-hour reset
  time but left the 7-day reset time unchanged, so a timestamp test
  would miss every 7-day gift.
- The fall is measured against `prevPercent` / `prevSeen` in
  `src/alerts.cpp`, which live in RTC memory rather than NVS because
  utilisation changes on nearly every poll. They survive deep sleep; a
  power cycle clears them, so the first poll afterwards skips the fall
  test rather than guessing. A failed poll leaves the last good reading
  as the baseline, so a gift is still detected after an outage.
- A gift zeroes both windows, so both reset WAVs play in sequence.
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
| `alerts` | Print the stored alert level, pending reset time, and last-seen utilisation for each (account, window) |
| `alerts clear` | Wipe the `alerts` NVS namespace and the RTC drop baseline. The next poll re-alerts for anything above a threshold, but skips the reset check for that poll while it repopulates the baseline |
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
   16000 Hz mono, double that in stereo. The Mario theme's six sounds run
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

To keep themes at the same perceived volume, normalise loudness rather
than adding fixed gain. Matching LUFS alone is not enough on this
speaker: steady tones (the Mario theme) sound louder than speech at
the same reading, because speech is spiky — its peaks hit the ceiling
while its average stays low. Measure the crest factor (peak minus RMS)
as well as loudness:

| Theme | Integrated | Crest factor | Peak |
| --- | --- | --- | --- |
| Tones (`mario`) | -16 LUFS | 10-12 dB | -1.3 to -6 dBFS |
| Speech (`jarvis`) | -12 LUFS | 11-12 dB | -1 dBFS |

Tonal sources only need loudness normalisation:

```sh
ffmpeg -i source.mp3 -af "loudnorm=I=-16:TP=-1.5:LRA=11" -ac 1 -ar 16000 -sample_fmt s16 5h-warning.wav
```

Speech needs compressing and limiting first. The high-pass drops bass
the small speaker cannot reproduce; the compressor and limiter bring
the crest factor down to the tones' range, which frees the headroom to
raise the level. Tune `volume` per file until the output reads about
-12 LUFS (+20 dB or so for a -27 LUFS source):

```sh
ffmpeg -i source.mp3 -af "aresample=16000,highpass=f=180,acompressor=threshold=-26dB:ratio=4:attack=3:release=100,volume=21dB,alimiter=limit=0.89:attack=1:release=50:level=false" -ac 1 -sample_fmt s16 5h-warning.wav
```

Check a file's loudness with
`ffmpeg -i 5h-warning.wav -af ebur128 -f null -` (the `I:` line in the
summary), and its crest factor from the `Peak level dB` and
`RMS level dB` lines of `-af astats`.

For a one-shot batch of a tonal theme (all six files at once):

```sh
theme=mario
mkdir -p "sounds/$theme"
for f in "sound-sources/$theme"/*.mp3; do
  ffmpeg -y -i "$f" -af "loudnorm=I=-16:TP=-1.5:LRA=11" -ac 1 -ar 16000 -sample_fmt s16 \
    "sounds/$theme/$(basename "${f%.mp3}").wav"
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
no sound. Upload the `data_dir` theme once with:

```sh
pio run -t uploadfs
```

LittleFS survives firmware flashes, so subsequent `pio run -t upload`
calls don't need `uploadfs`.

`scripts/check_sound_theme.py` fails the `buildfs` and `uploadfs`
targets when `data_dir` holds none of the six sounds, and warns when it
holds only some. Left to itself, `mklittlefs` reports success on an
empty folder, and the resulting blank image overwrites both the sounds
and the history ring. Plain `pio run` and `pio run -t upload` skip the
check, since the firmware runs fine with no sounds on LittleFS.

Run `upload` and `uploadfs` as separate commands. `pio run -t upload -t
uploadfs` turns PlatformIO's library dependency finder off for the
whole run, and the firmware then fails to compile with missing headers
(`epaper.h`, `LittleFS.h`, ...).

### Changing themes without losing the history

`uploadfs` replaces the whole partition, and `/history.bin` lives there
beside the WAVs, so switching themes with a plain `uploadfs` costs the
7-day usage history. Keep it with:

```sh
pip install littlefs-python          # once
python scripts/uploadfs_keep_history.py --theme sounds/mario
```

Both arguments are optional: `--theme` defaults to `data_dir`, and
`--port` to whatever esptool detects. The script reads the partition
off the board, copies every non-WAV file it finds into a staging
folder, adds the theme's WAVs, and runs `uploadfs` against that — so
anything the firmware keeps on LittleFS survives, not just the
history.

Reading 1.5 MB over USB-Serial/JTAG takes about half a minute and
fails often enough that the dump is done in retried 64 KB chunks; a
single whole-partition read reliably dies with "Serial data stream
stopped" partway through. If a chunk can't be read after six tries the
script exits without writing anything, leaving the board as it was.
The chip is also held in its bootloader during the dump (`--after
no-reset`) — a running firmware would append a history sample after
the read, and that sample would then be lost to the flash.

A board whose filesystem has never been written has no history to
keep. The script says so and flashes the theme alone.

Nothing preserves the history across a `buildfs` of the wrong folder,
so the guard above still matters.

## Known limitations

- Volume is panel-only. No serial command sets it, so a board with no
  network access is stuck with whatever is stored in NVS.
- No crossfade or mix. If two different alerts qualify in the same
  poll, they play back-to-back.
- Playback blocks. `playWav()` streams to I²S synchronously, so the
  main loop stalls for the length of the sound.
