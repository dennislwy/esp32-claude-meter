# Memory footprint

Where the ESP32-S3-PICO-1-N8R8's 8 MB flash and 320 KB of internal RAM go,
and how much room is left.

Figures below are a snapshot taken on `develop` at `e35b4ae` (2026-10-10).
Regenerate them at any time with `pio run` — the size report is the last
thing it prints before building the binary.

## Current usage

| Resource | Used | Capacity | Free |
| --- | --- | --- | --- |
| Firmware (app0) | 1,774,341 B (1.69 MB) | 3,342,336 B (3.19 MB) | ~1.49 MB (53.1% used) |
| Internal RAM | 127,788 B | 327,680 B | ~195 KB (39.0% used) |
| Sound filesystem | 892,296 B (871 KB) | 1,572,864 B (1.5 MB) | ~665 KB (56.7% used) |

The 8 MB octal PSRAM is a separate address space and is not counted in the
RAM figure. LVGL draw buffers and the panel's larger allocations live there.

The sound figure is the raw size of `data/`; the LittleFS image carries
block and metadata overhead on top, so usable free space is somewhat less
than 665 KB.

## Partition layout

The build uses the Arduino core's stock `default_8MB.csv` — no custom
partition table is set in `platformio.ini`.

| Name | Type | Offset | Size | Contents |
| --- | --- | --- | --- | --- |
| `nvs` | data | 0x9000 | 20 KB | settings, Wi-Fi credentials, Claude tokens, alert latches |
| `otadata` | data | 0xE000 | 8 KB | which app slot to boot |
| `app0` | app | 0x10000 | 3.19 MB | the running firmware |
| `app1` | app | 0x340000 | 3.19 MB | second OTA slot — reserved, currently unused |
| `spiffs` | data | 0x670000 | 1.5 MB | LittleFS: WAV alert sounds and the history ring |
| `coredump` | data | 0x7F0000 | 64 KB | panic dumps |

`app1` is the single largest block of idle flash on the board. The dual-slot
layout exists so an over-the-air update can be written to the inactive slot
and rolled back if it fails to boot. OTA is on the roadmap wish list; until
it ships, that 3.19 MB is reserved rather than spent. Switching to a
single-app partition table would recover it at the cost of ever doing OTA
safely.

## Reading the numbers

`pio run` reports:

```
RAM:   [====      ]  39.0% (used 127788 bytes from 327680 bytes)
Flash: [=====     ]  53.1% (used 1774341 bytes from 3342336 bytes)
```

- **Flash** is measured against `app0`'s 3.19 MB, not against the 8 MB chip.
  Hitting 100% there means the firmware no longer fits in one OTA slot,
  which is a hard limit even though the chip still has megabytes spare.
- **RAM** is internal DRAM available at link time. It excludes PSRAM and
  does not predict runtime heap exhaustion; use the panel's Device details
  card or the `status` serial command for live free-heap figures.

`pio run -t upload` reports a slightly larger number (e.g. 1,779,840 bytes)
than the size check. That is the padded application image written at
0x10000, including the image header and segment alignment, so a small
difference from the ELF-derived figure is expected.

## Things that move these numbers

- **WAV files** dominate the filesystem. Six sounds in `data/` account for
  essentially all of its 871 KB; the history ring is small by comparison.
- **Embedded assets** count against firmware, not the filesystem: the two
  root CA bundles in `assets/certs/` and the gzipped ECharts bundle in
  `assets/echarts/` are linked into `app0` via `board_build.embed_txtfiles`
  and `board_build.embed_files`.
- **The web panel** is a single embedded string in `src/panel_html.h`,
  so panel markup, CSS and JavaScript all consume firmware space.
