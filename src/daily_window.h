#pragma once

#include <stdint.h>
#include <time.h>

// Local daily window [start, end), in minutes since midnight. Equal times disable it.
bool dailyWindowContains(uint16_t start, uint16_t end, uint16_t minute);

// Zero outside the window; otherwise the first real minute outside it. Uses the
// active time zone, including skipped/repeated minutes at daylight-saving changes.
time_t dailyWindowResumeAt(time_t now, bool enabled, uint16_t start, uint16_t end);
