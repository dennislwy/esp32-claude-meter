#pragma once

// Defined in src/build_info.cpp, which scripts/build_info.py regenerates on every build
extern const char FW_GIT_REV[];    // `git describe --always --dirty --tags`
extern const char FW_BUILD_TIME[]; // local time, "YYYY-MM-DD HH:MM +hhmm"
