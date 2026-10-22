# Gifted-reset detection regression

These host tests compile the early-window-reset predicate
(`src/window_reset.cpp`). They check that a fall in utilisation is treated as
a window replacement, that a missing baseline stays silent, that the
half-point margin is exclusive, and that a natural rollover suppresses the
drop so one reset never chimes twice. No board, credentials, network
requests, or additional libraries are needed.

This suite exists because the behaviour it guards is nearly untestable on
hardware: triggering it needs a real gifted usage-limit reset, which is
scarce and cannot be forced. Every edge case therefore has to be settled
here.

The margin cases are the important ones. Utilisation arrives as a 0-1
fraction with two decimals, so real changes land in 1-point steps; a margin
above 1 point would miss real resets and a margin of 0 would fire on float
noise.

From the repository root on Linux/macOS:

```sh
mkdir -p foobar/window-reset
c++ -std=c++11 -Wall -Wextra -Isrc test/window_reset/check.cpp src/window_reset.cpp -o foobar/window-reset/check
foobar/window-reset/check
```

From an MSVC developer command prompt on Windows:

```bat
if not exist foobar\window-reset mkdir foobar\window-reset
cl /nologo /EHsc /std:c++17 /W4 /WX /Isrc test\window_reset\check.cpp src\window_reset.cpp /Fofoobar\window-reset\ /Fefoobar\window-reset\check.exe
foobar\window-reset\check.exe
```

`pio run` verifies the firmware builds for the ESP32-S3. The baseline the
predicate compares against lives in RTC memory in `src/alerts.cpp`; the
`alerts` serial command prints it.
