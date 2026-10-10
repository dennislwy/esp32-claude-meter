# Hostname rule regression

These host tests compile the mDNS label rule used by firmware. They check the
length bounds, the allowed character set, rejected characters including a
high-bit byte, hyphen placement, case folding, and that a rejected name leaves
the caller's buffer untouched. No board, credentials, network requests, or
additional libraries are needed.

From the repository root on Linux/macOS:

```sh
mkdir -p foobar/hostname
c++ -std=c++11 -Wall -Wextra -Isrc test/hostname/check.cpp src/hostname.cpp -o foobar/hostname/check
foobar/hostname/check
```

From an MSVC developer command prompt on Windows:

```bat
if not exist foobar\hostname mkdir foobar\hostname
cl /nologo /EHsc /std:c++17 /W4 /WX /Isrc test\hostname\check.cpp src\hostname.cpp /Fofoobar\hostname\ /Fefoobar\hostname\check.exe
foobar\hostname\check.exe
```

The rule is shared by the serial command, the panel settings API, and
`settings::setHostname()`, so this is the single place it is verified.
