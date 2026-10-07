#include "clock.h"

#include <Arduino.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include "settings.h"

namespace
{
const char *const NTP_SERVER_1 = "pool.ntp.org";
const char *const NTP_SERVER_2 = "time.google.com";
constexpr uint32_t NTP_TIMEOUT_MS = 10000;
// Anything earlier means the clock was never set
constexpr time_t VALID_AFTER = 1700000000;
}

void clockBegin(Pcf85063 &rtc)
{
  setenv("TZ", settings::timeZone().c_str(), 1);
  tzset();

  RtcDateTime now;
  if (!rtc.read(now) || !rtc.timeValid())
  {
    return;
  }
  struct tm local = {};
  local.tm_year = now.year - 1900;
  local.tm_mon = now.month - 1;
  local.tm_mday = now.day;
  local.tm_hour = now.hour;
  local.tm_min = now.minute;
  local.tm_sec = now.second;
  local.tm_isdst = -1;
  const timeval tv = {mktime(&local), 0};
  settimeofday(&tv, nullptr);
}

bool clockValid()
{
  return time(nullptr) > VALID_AFTER;
}

void clockStartNtpSync()
{
  sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
  const String timeZone = settings::timeZone();
  configTzTime(timeZone.c_str(), NTP_SERVER_1, NTP_SERVER_2);
}

bool clockNtpSyncComplete()
{
  return sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED && clockValid();
}

bool clockSyncNtp(Pcf85063 &rtc)
{
  clockStartNtpSync();
  // configTzTime returns before the first sync completes
  const uint32_t start = millis();
  while (!clockNtpSyncComplete())
  {
    if (millis() - start > NTP_TIMEOUT_MS)
    {
      return false;
    }
    delay(50);
  }
  clockSaveToRtc(rtc);
  return true;
}

void clockApplyTimeZone(Pcf85063 &rtc)
{
  setenv("TZ", settings::timeZone().c_str(), 1);
  tzset();
  if (clockValid())
  {
    clockSaveToRtc(rtc);
  }
}

void clockSaveToRtc(Pcf85063 &rtc)
{
  const time_t now = time(nullptr);
  struct tm local;
  localtime_r(&now, &local);
  RtcDateTime dateTime = {};
  dateTime.year = local.tm_year + 1900;
  dateTime.month = local.tm_mon + 1;
  dateTime.day = local.tm_mday;
  dateTime.hour = local.tm_hour;
  dateTime.minute = local.tm_min;
  dateTime.second = local.tm_sec;
  rtc.write(dateTime);
}
