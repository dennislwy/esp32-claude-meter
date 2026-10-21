# Device PIN entry regression

These host checks compile the firmware's actual `pin_entry.cpp`. They cover the
starting state, digit increment, the 9 -> 0 wrap, confirming and advancing,
completion on the fourth confirm, the NUL-terminated read-back, no-ops after
completion, `reset()` after a wrong PIN, an all-zero PIN, and out-of-range slot
reads. No board, credentials, network requests, or additional libraries are
needed.

From the repository root on Linux/macOS:

```sh
mkdir -p foobar/pin-entry
c++ -std=c++17 -Wall -Wextra -Isrc test/pin_entry/check.cpp src/pin_entry.cpp -o foobar/pin-entry/check
foobar/pin-entry/check
```

From an MSVC developer command prompt on Windows:

```bat
if not exist foobar\pin-entry mkdir foobar\pin-entry
cl /nologo /EHsc /std:c++17 /W4 /WX /Isrc test\pin_entry\check.cpp src\pin_entry.cpp /Fofoobar\pin-entry\ /Fefoobar\pin-entry\check.exe
foobar\pin-entry\check.exe
```

The state machine holds no key and calls no crypto, so this suite covers it in
full. The crypto it feeds is verified on-device by the `selftest` serial
command, since mbedtls ships inside the ESP32 framework.
