# Break Hours schedule regression

These host tests compile the daily-window code used by firmware. They check
all daily minutes, exact start/end seconds, disabled and equal-time windows,
overnight wake deadlines, year rollover, and skipped/repeated DST minutes.
No board, credentials, network requests, or additional libraries are needed.

From the repository root on Linux/macOS:

```sh
mkdir -p foobar/break-hours
c++ -std=c++11 -Wall -Wextra -Isrc test/break_hours/check.cpp src/daily_window.cpp -o foobar/break-hours/check
foobar/break-hours/check
```

From an MSVC developer command prompt on Windows:

```bat
if not exist foobar\break-hours mkdir foobar\break-hours
cl /nologo /EHsc /std:c++17 /W4 /WX /Isrc test\break_hours\check.cpp src\daily_window.cpp /Fofoobar\break-hours\ /Fefoobar\break-hours\check.exe
foobar\break-hours\check.exe
```

Panel interaction tests run separately via `scripts/check_panel_ui.cjs`;
`pio run` verifies the firmware builds for the ESP32-S3. Host/browser checks
do not measure current draw or replace a physical sleep/wake test.
