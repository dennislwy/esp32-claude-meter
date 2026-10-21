#pragma once

#include <stddef.h>
#include <time.h>

// Local time of a usage-window reset for the e-paper reset line: "1:10", or
// "Fri 13:00" with showWeekday. 24-hour, no leading zero on the hour. The
// longest result is 9 characters ("Wed 23:59").
void formatResetWhen(time_t reset, bool showWeekday, char *out, size_t len);
