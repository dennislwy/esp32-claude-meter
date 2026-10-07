#include "settings.h"

#include <Preferences.h>
#include "daily_window.h"
#include "hostname.h"

namespace
{
const char *const NAMESPACE = "meter";
const char *const KEY_WIFI_SSID = "wifi_ssid";
const char *const KEY_QUIET_ENABLED = "quiet_on";
const char *const KEY_QUIET_START = "quiet_start";
const char *const KEY_QUIET_END = "quiet_end";
const char *const KEY_QUIET_START_MIN = "quiet_startM";
const char *const KEY_QUIET_END_MIN = "quiet_endM";
const char *const KEY_PAUSE_ENABLED = "pause_on";
const char *const KEY_PAUSE_START = "pause_start";
const char *const KEY_PAUSE_END = "pause_end";
const char *const KEY_PAUSE_START_MIN = "pause_startM";
const char *const KEY_PAUSE_END_MIN = "pause_endM";
const char *const KEY_AUDIO_VOLUME = "audio_vol";
const char *const KEY_TIME_ZONE = "tz";
const char *const KEY_TIME_ZONE_NAME = "tz_name";
const char *const KEY_ROTATION = "rotation";
const char *const KEY_HOSTNAME = "hostname";
const char *const KEY_WIFI_PASSWORD = "wifi_pass";
const char *const KEY_POLL_INTERVAL = "poll_minutes";
const char *const KEY_WARNING_5H = "warn_5h";
const char *const KEY_WARNING_7D = "warn_7d";
// Token 1 keeps the original single-token key so an existing save carries over
const char *const KEY_CLAUDE_TOKENS[] = {"claude_token", "claude_token2"};
constexpr int TOKEN_COUNT = sizeof(KEY_CLAUDE_TOKENS) / sizeof(KEY_CLAUDE_TOKENS[0]);
static_assert(TOKEN_COUNT == settings::CLAUDE_TOKEN_COUNT, "one key per token");
const char *const KEY_ACCOUNT_NAMES[] = {"account1", "account2"};
static_assert(sizeof(KEY_ACCOUNT_NAMES) / sizeof(KEY_ACCOUNT_NAMES[0]) == TOKEN_COUNT, "one name per token");

bool validNumber(int number)
{
  return number >= 1 && number <= TOKEN_COUNT;
}

String load(const char *key)
{
  Preferences prefs;
  // Read-write so a first read creates the namespace; isKey avoids NOT_FOUND error logs for unset keys
  prefs.begin(NAMESPACE, false);
  const String value = prefs.isKey(key) ? prefs.getString(key, "") : "";
  prefs.end();
  return value;
}

void store(const char *key, const String &value)
{
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  prefs.putString(key, value);
  prefs.end();
}

// A byte setting, clamped to min..max, or fallback when unset
uint8_t loadByte(const char *key, uint8_t fallback, uint8_t min, uint8_t max, const char *legacyKey = nullptr)
{
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  // Lazily migrate the old Break Hours keys; new Pause Hours saves take precedence.
  if (!prefs.isKey(key) && legacyKey && prefs.isKey(legacyKey))
    prefs.putUChar(key, prefs.getUChar(legacyKey));
  const uint8_t value = prefs.isKey(key) ? prefs.getUChar(key) : fallback;
  prefs.end();
  return constrain(value, min, max);
}

void storeByte(const char *key, uint8_t value, uint8_t min, uint8_t max)
{
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  prefs.putUChar(key, constrain(value, min, max));
  prefs.end();
}

// Every character in allowed, and length within min..max
bool onlyChars(const String &value, const char *allowed, size_t min, size_t max)
{
  if (value.length() < min || value.length() > max)
  {
    return false;
  }
  for (size_t i = 0; i < value.length(); i++)
  {
    if (!strchr(allowed, value[i]))
    {
      return false;
    }
  }
  return true;
}

const char *const ALNUM = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
}

namespace settings
{
uint8_t pollIntervalMinutes()
{
  return loadByte(KEY_POLL_INTERVAL, POLL_INTERVAL_DEFAULT, POLL_INTERVAL_MIN, POLL_INTERVAL_MAX);
}

void setPollIntervalMinutes(uint8_t minutes)
{
  storeByte(KEY_POLL_INTERVAL, minutes, POLL_INTERVAL_MIN, POLL_INTERVAL_MAX);
}

uint8_t warningPercent5h()
{
  return loadByte(KEY_WARNING_5H, WARNING_5H_DEFAULT, WARNING_PERCENT_MIN, WARNING_PERCENT_MAX);
}

uint8_t warningPercent7d()
{
  return loadByte(KEY_WARNING_7D, WARNING_7D_DEFAULT, WARNING_PERCENT_MIN, WARNING_PERCENT_MAX);
}

void setWarningPercent5h(uint8_t percent)
{
  storeByte(KEY_WARNING_5H, percent, WARNING_PERCENT_MIN, WARNING_PERCENT_MAX);
}

void setWarningPercent7d(uint8_t percent)
{
  storeByte(KEY_WARNING_7D, percent, WARNING_PERCENT_MIN, WARNING_PERCENT_MAX);
}

bool quietHoursEnabled()
{
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  // Default on, per R4a
  const bool enabled = prefs.isKey(KEY_QUIET_ENABLED) ? prefs.getBool(KEY_QUIET_ENABLED) : true;
  prefs.end();
  return enabled;
}

uint8_t quietHoursStart() { return loadByte(KEY_QUIET_START, QUIET_HOUR_START_DEFAULT, 0, 23); }
uint8_t quietMinuteStart() { return loadByte(KEY_QUIET_START_MIN, QUIET_MINUTE_START_DEFAULT, 0, 59); }
uint8_t quietHoursEnd() { return loadByte(KEY_QUIET_END, QUIET_HOUR_END_DEFAULT, 0, 23); }
uint8_t quietMinuteEnd() { return loadByte(KEY_QUIET_END_MIN, QUIET_MINUTE_END_DEFAULT, 0, 59); }

void setQuietHoursEnabled(bool enabled)
{
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  prefs.putBool(KEY_QUIET_ENABLED, enabled);
  prefs.end();
}

bool setQuietHours(uint8_t startHour, uint8_t startMinute, uint8_t endHour, uint8_t endMinute)
{
  if (!dailyWindowValid(startHour, startMinute, endHour, endMinute))
    return false;
  storeByte(KEY_QUIET_START, startHour, 0, 23);
  storeByte(KEY_QUIET_START_MIN, startMinute, 0, 59);
  storeByte(KEY_QUIET_END, endHour, 0, 23);
  storeByte(KEY_QUIET_END_MIN, endMinute, 0, 59);
  return true;
}

bool isQuietTime(uint8_t localHour, uint8_t localMinute)
{
  if (!quietHoursEnabled())
  {
    return false;
  }
  const uint16_t start = quietHoursStart() * 60 + quietMinuteStart();
  const uint16_t end = quietHoursEnd() * 60 + quietMinuteEnd();
  const uint16_t now = (uint16_t)localHour * 60 + localMinute;
  return dailyWindowContains(start, end, now);
}

bool pauseHoursEnabled()
{
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  if (!prefs.isKey(KEY_PAUSE_ENABLED) && prefs.isKey("break_on"))
    prefs.putBool(KEY_PAUSE_ENABLED, prefs.getBool("break_on"));
  const bool enabled = prefs.isKey(KEY_PAUSE_ENABLED) ? prefs.getBool(KEY_PAUSE_ENABLED) : false;
  prefs.end();
  return enabled;
}

uint8_t pauseHoursStart() { return loadByte(KEY_PAUSE_START, 0, 0, 23, "break_start"); }
uint8_t pauseMinuteStart() { return loadByte(KEY_PAUSE_START_MIN, 0, 0, 59, "break_startM"); }
uint8_t pauseHoursEnd() { return loadByte(KEY_PAUSE_END, 6, 0, 23, "break_end"); }
uint8_t pauseMinuteEnd() { return loadByte(KEY_PAUSE_END_MIN, 0, 0, 59, "break_endM"); }

void setPauseHoursEnabled(bool enabled)
{
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  prefs.putBool(KEY_PAUSE_ENABLED, enabled);
  prefs.end();
}

bool setPauseHours(uint8_t startHour, uint8_t startMinute, uint8_t endHour, uint8_t endMinute)
{
  if (!dailyWindowValid(startHour, startMinute, endHour, endMinute))
    return false;
  storeByte(KEY_PAUSE_START, startHour, 0, 23);
  storeByte(KEY_PAUSE_START_MIN, startMinute, 0, 59);
  storeByte(KEY_PAUSE_END, endHour, 0, 23);
  storeByte(KEY_PAUSE_END_MIN, endMinute, 0, 59);
  return true;
}

time_t pauseHoursResumeAt(time_t now)
{
  if (!pauseHoursEnabled())
    return 0;
  return dailyWindowResumeAt(now, true, pauseHoursStart() * 60 + pauseMinuteStart(),
                            pauseHoursEnd() * 60 + pauseMinuteEnd());
}

uint8_t audioVolume() { return loadByte(KEY_AUDIO_VOLUME, AUDIO_VOLUME_DEFAULT, 0, 100); }
void setAudioVolume(uint8_t percent) { storeByte(KEY_AUDIO_VOLUME, percent, 0, 100); }

String timeZone()
{
  const String value = load(KEY_TIME_ZONE);
  return value.isEmpty() ? String(TIME_ZONE_DEFAULT) : value;
}

String timeZoneName()
{
  const String value = load(KEY_TIME_ZONE_NAME);
  return value.isEmpty() ? String(TIME_ZONE_NAME_DEFAULT) : value;
}

bool setTimeZone(const String &posix, const String &name)
{
  const String posixChars = String(ALNUM) + "<>+-,./:";
  const String nameChars = String(ALNUM) + "_/+-";
  if (!onlyChars(posix, posixChars.c_str(), 3, 63) || !onlyChars(name, nameChars.c_str(), 1, 40))
  {
    return false;
  }
  store(KEY_TIME_ZONE, posix);
  store(KEY_TIME_ZONE_NAME, name);
  return true;
}

uint8_t displayRotation() { return loadByte(KEY_ROTATION, 0, 0, 3); }
void setDisplayRotation(uint8_t quarterTurns) { storeByte(KEY_ROTATION, quarterTurns, 0, 3); }

String wifiSsid() { return load(KEY_WIFI_SSID); }
String wifiPassword() { return load(KEY_WIFI_PASSWORD); }
String claudeToken(int number)
{
  return validNumber(number) ? load(KEY_CLAUDE_TOKENS[number - 1]) : "";
}

String accountName(int number)
{
  const String name = validNumber(number) ? load(KEY_ACCOUNT_NAMES[number - 1]) : "";
  return name.isEmpty() ? "Claude " + String(number) : name;
}

String hostname()
{
  const String name = load(KEY_HOSTNAME);
  return name.isEmpty() ? HOSTNAME_DEFAULT : name;
}

bool setHostname(const String &value)
{
  // An empty value clears the key, so hostname() falls back to the default
  if (value.isEmpty())
  {
    store(KEY_HOSTNAME, "");
    return true;
  }
  char normalized[HOSTNAME_LIMIT + 1];
  if (!hostnameNormalize(value.c_str(), normalized))
  {
    return false;
  }
  store(KEY_HOSTNAME, normalized);
  return true;
}

void setWifiSsid(const String &value) { store(KEY_WIFI_SSID, value); }
void setWifiPassword(const String &value) { store(KEY_WIFI_PASSWORD, value); }
void setClaudeToken(int number, const String &value)
{
  if (validNumber(number))
  {
    store(KEY_CLAUDE_TOKENS[number - 1], value);
  }
}

void setAccountName(int number, const String &value)
{
  if (validNumber(number))
  {
    store(KEY_ACCOUNT_NAMES[number - 1], value.substring(0, ACCOUNT_NAME_MAX));
  }
}

String mask(const String &secret, size_t visibleTail)
{
  if (secret.isEmpty())
  {
    return "(not set)";
  }
  // Short secrets show no characters at all
  if (visibleTail == 0 || secret.length() <= visibleTail * 3)
  {
    return String(secret.length()) + " chars";
  }
  return String(secret.length()) + " chars, ending ..." + secret.substring(secret.length() - visibleTail);
}
}
