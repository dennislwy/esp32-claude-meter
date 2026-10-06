#include "daily_window.h"

namespace
{
bool localTime(time_t epoch, struct tm &local)
{
#ifdef _WIN32
  return localtime_s(&local, &epoch) == 0;
#else
  return localtime_r(&epoch, &local) != nullptr;
#endif
}
}

bool dailyWindowContains(uint16_t start, uint16_t end, uint16_t minute)
{
  if (start == end || start >= 1440 || end >= 1440 || minute >= 1440)
    return false;
  return start < end ? (minute >= start && minute < end) : (minute >= start || minute < end);
}

time_t dailyWindowResumeAt(time_t now, bool enabled, uint16_t start, uint16_t end)
{
  struct tm local;
  if (!enabled || !localTime(now, local) || !dailyWindowContains(start, end, static_cast<uint16_t>(local.tm_hour * 60 + local.tm_min)))
    return 0;

  // Walk real minute boundaries instead of adding wall-clock hours: DST may
  // skip the configured end time or repeat it. No network or delay is involved.
  time_t candidate = now - local.tm_sec + 60;
  for (int minute = 0; minute < 48 * 60; minute++, candidate += 60)
  {
    if (localTime(candidate, local) && !dailyWindowContains(start, end, static_cast<uint16_t>(local.tm_hour * 60 + local.tm_min)))
      return candidate;
  }
  // An unusual zone/clock failure: retry the schedule shortly, without polling.
  return now + 60;
}
