# Reset-time format regression

These host tests compile the reset-time formatter used by the e-paper
reset lines (`src/reset_time.cpp`). They check the 24-hour format with no
leading zero on the hour, every weekday name in title case, the longest
result's length, and that the output follows the local time zone. No
board, credentials, network requests, or additional libraries are needed.

From the repository root on Linux/macOS:

```sh
mkdir -p foobar/reset-time
c++ -std=c++11 -Wall -Wextra -Isrc test/reset_time/check.cpp src/reset_time.cpp -o foobar/reset-time/check
foobar/reset-time/check
```

From an MSVC developer command prompt on Windows:

```bat
if not exist foobar\reset-time mkdir foobar\reset-time
cl /nologo /EHsc /std:c++17 /W4 /WX /Isrc test\reset_time\check.cpp src\reset_time.cpp /Fofoobar\reset-time\ /Fefoobar\reset-time\check.exe
foobar\reset-time\check.exe
```

`pio run` verifies the firmware builds for the ESP32-S3. The full reset
line (prefix, countdown, `--` and `(now)` cases) is assembled in
`src/meter_ui.cpp` and checked on the device.
