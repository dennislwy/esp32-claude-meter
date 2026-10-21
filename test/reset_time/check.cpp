// Host regression: compile this with src/reset_time.cpp (no board required).
#include "reset_time.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NDEBUG
#error "this suite asserts; NDEBUG would compile every check away"
#endif

void zone(const char *value)
{
#ifdef _WIN32
  _putenv_s("TZ", value);
  _tzset();
#else
  setenv("TZ", value, 1);
  tzset();
#endif
}

time_t at(int month, int day, int hour, int minute)
{
  struct tm t = {};
  t.tm_year = 2026 - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_isdst = -1;
  return mktime(&t);
}

const char *when(time_t reset, bool showWeekday)
{
  static char buf[16];
  formatResetWhen(reset, showWeekday, buf, sizeof(buf));
  return buf;
}

int main()
{
  zone("MYT-8");

  // 2026-10-23 is a Friday.
  assert(strcmp(when(at(10, 23, 13, 0), true), "Fri 13:00") == 0);
  assert(strcmp(when(at(10, 23, 13, 0), false), "13:00") == 0);

  // No leading zero on the hour; minutes always two digits.
  assert(strcmp(when(at(10, 23, 1, 10), false), "1:10") == 0);
  assert(strcmp(when(at(10, 23, 9, 5), true), "Fri 9:05") == 0);
  assert(strcmp(when(at(10, 24, 0, 0), false), "0:00") == 0);
  assert(strcmp(when(at(10, 24, 23, 59), true), "Sat 23:59") == 0);

  // Every weekday, in title case. 2026-10-18 is a Sunday.
  const char *const days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  for (int d = 0; d < 7; d++)
  {
    char expected[16];
    snprintf(expected, sizeof(expected), "%s 12:00", days[d]);
    assert(strcmp(when(at(10, 18 + d, 12, 0), true), expected) == 0);
  }

  // The longest result fits the 16-byte buffer meter_ui.cpp uses.
  assert(strlen(when(at(10, 21, 23, 59), true)) == 9);

  // Local time, not UTC: the same instant reads differently in another zone.
  const time_t instant = at(10, 23, 13, 0);
  zone("UTC0");
  assert(strcmp(when(instant, true), "Fri 5:00") == 0);

  printf("reset_time: all checks passed\n");
  return 0;
}
