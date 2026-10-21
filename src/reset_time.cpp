#include "reset_time.h"

#include <stdio.h>

void formatResetWhen(time_t reset, bool showWeekday, char *out, size_t len)
{
  static const char *const DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  struct tm local = {};
#ifdef _WIN32
  localtime_s(&local, &reset);
#else
  localtime_r(&reset, &local);
#endif
  if (showWeekday)
  {
    snprintf(out, len, "%s %d:%02d", DAYS[local.tm_wday], local.tm_hour, local.tm_min);
  }
  else
  {
    snprintf(out, len, "%d:%02d", local.tm_hour, local.tm_min);
  }
}
