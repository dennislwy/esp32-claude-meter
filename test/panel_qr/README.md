# Panel QR payload regression

These host tests compile the Panel view's QR payload builder
(`src/panel_qr.cpp`). They check the `HTTP://<ip>/<pin>` format, the
fallback when no usable PIN is available, the longest result's length, that
every payload stays inside QR alphanumeric mode, and the six-digit path
shape used by the login throttle. No board, credentials, network requests,
or additional libraries are needed.

The alphanumeric check is the important one. QR alphanumeric mode holds 38
characters at version 2 where byte mode holds 26, so a single out-of-set
character — a lowercase `http://`, a `?` query separator — silently pushes
the code to a denser version and costs the Panel view its 3-px-per-module
QR. Nothing at runtime reports that.

From the repository root on Linux/macOS:

```sh
mkdir -p foobar/panel-qr
c++ -std=c++11 -Wall -Wextra -Isrc test/panel_qr/check.cpp src/panel_qr.cpp -o foobar/panel-qr/check
foobar/panel-qr/check
```

From an MSVC developer command prompt on Windows:

```bat
if not exist foobar\panel-qr mkdir foobar\panel-qr
cl /nologo /EHsc /std:c++17 /W4 /WX /Isrc test\panel_qr\check.cpp src\panel_qr.cpp /Fofoobar\panel-qr\ /Fefoobar\panel-qr\check.exe
foobar\panel-qr\check.exe
```

`pio run` verifies the firmware builds for the ESP32-S3. The QR code itself
is encoded and drawn in `src/meter_ui.cpp` and checked by scanning the
device's display.
