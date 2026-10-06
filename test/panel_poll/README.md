# Panel usage polling regression

These host checks compile the firmware's actual `panel_usage_poll.cpp` with
small Arduino, Wi-Fi, and FreeRTOS adapters. FreeRTOS tasks use real host
threads; fake HTTPS requests can be held open while the caller reads cached
state. No board, tokens, or network requests are needed.

They cover nonblocking starts, one job at a time (including uncollected
results), cache publication on the main loop, retained data on HTTP errors,
clamped percentages, cancellation between accounts, token replacement,
Pause Hours and manual override, disconnected Wi-Fi, and task allocation
failure/recovery.

From the repository root on Linux/macOS:

```sh
mkdir -p foobar/panel-poll
c++ -std=c++17 -Wall -Wextra -pthread -Itest/panel_poll/stubs -Isrc -Ilib/ClaudeUsage test/panel_poll/check.cpp src/panel_usage_poll.cpp src/daily_window.cpp -o foobar/panel-poll/check
foobar/panel-poll/check
```

From an MSVC developer command prompt on Windows:

```bat
if not exist foobar\panel-poll mkdir foobar\panel-poll
cl /nologo /EHsc /std:c++17 /W4 /WX /Itest\panel_poll\stubs /Isrc /Ilib\ClaudeUsage test\panel_poll\check.cpp src\panel_usage_poll.cpp src\daily_window.cpp /Fofoobar\panel-poll\ /Fefoobar\panel-poll\check.exe
foobar\panel-poll\check.exe
```

The existing [Pause Hours tests](../pause_hours/README.md) cover exact daily
boundaries and daylight saving. `pio run` verifies the ESP32 integration.
Host checks do not measure actual TLS heap usage, HTTP latency, or Wi-Fi
reconnection behavior on the board.
