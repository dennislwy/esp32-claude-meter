# Pause Hours schedule regression

These host tests compile the daily-window code used by firmware. They check
all daily minutes, distinct valid From/Until times, rejected equal/invalid times,
exact start/end seconds, disabled and legacy empty windows,
overnight wake deadlines, year rollover, and skipped/repeated DST minutes.
No board, credentials, network requests, or additional libraries are needed.

From the repository root on Linux/macOS:

```sh
mkdir -p foobar/pause-hours
c++ -std=c++11 -Wall -Wextra -Isrc test/pause_hours/check.cpp src/daily_window.cpp -o foobar/pause-hours/check
foobar/pause-hours/check
```

From an MSVC developer command prompt on Windows:

```bat
if not exist foobar\pause-hours mkdir foobar\pause-hours
cl /nologo /EHsc /std:c++17 /W4 /WX /Isrc test\pause_hours\check.cpp src\daily_window.cpp /Fofoobar\pause-hours\ /Fefoobar\pause-hours\check.exe
foobar\pause-hours\check.exe
```

Panel interaction tests run separately via `scripts/check_panel_ui.cjs`;
`pio run` verifies the firmware builds for the ESP32-S3. Host/browser checks
do not measure current draw or replace a physical sleep/wake test.
