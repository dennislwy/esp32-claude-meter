// Host regression: compile this with src/daily_window.cpp (no board required).
#include "daily_window.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

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

time_t at(int month, int day, int hour, int minute, int second = 0, int dst = -1)
{
  struct tm t = {};
  t.tm_year = 2026 - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = second;
  t.tm_isdst = dst;
  return mktime(&t);
}

int main()
{
  // Shared validation used by API and settings setters for both schedules.
  for (int minute = 0; minute < 1440; minute++)
  {
    const int h = minute / 60, m = minute % 60;
    assert(!dailyWindowValid(h, m, h, m));
    const int next = (minute + 1) % 1440;
    assert(dailyWindowValid(h, m, next / 60, next % 60));
  }
  assert(dailyWindowValid(0, 0, 6, 0));
  assert(dailyWindowValid(22, 30, 7, 15));
  assert(!dailyWindowValid(-1, 0, 6, 0));
  assert(!dailyWindowValid(0, 60, 6, 0));
  assert(!dailyWindowValid(0, 0, 24, 0));
  assert(!dailyWindowValid(0, 0, 6, -1));
  // Every minute of the day, daytime/overnight/empty windows and their boundaries.
  for (uint16_t minute = 0; minute < 1440; minute++)
  {
    assert(dailyWindowContains(12 * 60, 13 * 60, minute) == (minute >= 720 && minute < 780));
    assert(dailyWindowContains(22 * 60 + 30, 7 * 60 + 15, minute) == (minute >= 1350 || minute < 435));
    assert(!dailyWindowContains(720, 720, minute));
  }
  assert(!dailyWindowContains(1440, 0, 100));

  zone("MYT-8");
  const time_t before = at(10, 6, 12, 29, 59);
  const time_t start = at(10, 6, 12, 30);
  const time_t end = at(10, 6, 13, 15);
  assert(!dailyWindowResumeAt(before, true, 750, 795));
  assert(dailyWindowResumeAt(start, true, 750, 795) == end);
  assert(dailyWindowResumeAt(end - 1, true, 750, 795) == end);
  assert(!dailyWindowResumeAt(end, true, 750, 795));
  assert(!dailyWindowResumeAt(start, false, 750, 795));
  assert(!dailyWindowResumeAt(start, true, 750, 750));
  // Deep sleep spans the whole overnight pause, not another short poll interval.
  assert(dailyWindowResumeAt(at(10, 6, 23, 42, 17), true, 1350, 435) == at(10, 7, 7, 15));
  assert(dailyWindowResumeAt(at(10, 7, 6, 1), true, 1350, 435) == at(10, 7, 7, 15));
  assert(dailyWindowResumeAt(at(12, 31, 23, 59), true, 1350, 435) > at(12, 31, 23, 59));
  assert(dailyWindowResumeAt(at(10, 6, 0, 0, 1), true, 0, 1439) == at(10, 6, 23, 59));

  // Both host CRTs and the firmware handle US Eastern's 2026 DST transitions.
#ifdef _WIN32
  zone("EST5EDT");
#else
  zone("EST5EDT,M3.2.0/2,M11.1.0/2");
#endif
  // 02:30 doesn't exist at spring-forward: resume at the first outside minute, 03:00.
  assert(dailyWindowResumeAt(at(3, 8, 1, 45), true, 60, 150) == at(3, 8, 3, 0));
  // During fall-back, both instances of 01:xx are still inside a 00:00-02:30 pause.
  const time_t fallStart = at(11, 1, 0, 30);
  const time_t fallEnd = at(11, 1, 2, 30);
  assert(fallEnd - fallStart == 3 * 3600);
  assert(dailyWindowResumeAt(fallStart, true, 0, 150) == fallEnd);
  assert(dailyWindowResumeAt(at(11, 1, 1, 30, 0, 1), true, 0, 150) == fallEnd);
  assert(dailyWindowResumeAt(at(11, 1, 1, 30, 0, 0), true, 0, 150) == fallEnd);
  puts("PASS: valid distinct schedule times, all daily minutes, exact boundaries, legacy empty windows, overnight sleep, year rollover, and DST gaps/repeats");
}
