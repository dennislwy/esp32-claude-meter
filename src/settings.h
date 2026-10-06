#pragma once

#include <Arduino.h>

// Wi-Fi and Claude credentials, kept in NVS so they never live in source code.
// NVS is not encrypted: anyone with the board and a USB cable can read them back.
namespace settings
{
constexpr int CLAUDE_TOKEN_COUNT = 2;
constexpr size_t ACCOUNT_NAME_MAX = 20;

String wifiSsid();
String wifiPassword();
// number: 1 to CLAUDE_TOKEN_COUNT
String claudeToken(int number);
// Display name for each token's account; "Claude <number>" when not set
String accountName(int number);

// Minutes between usage polls, POLL_INTERVAL_MIN to POLL_INTERVAL_MAX
constexpr uint8_t POLL_INTERVAL_MIN = 1;
constexpr uint8_t POLL_INTERVAL_MAX = 5;
constexpr uint8_t POLL_INTERVAL_DEFAULT = 1;
uint8_t pollIntervalMinutes();
void setPollIntervalMinutes(uint8_t minutes);

// Usage % at which the warning sound plays, per window (applies to every account)
constexpr uint8_t WARNING_PERCENT_MIN = 50;
constexpr uint8_t WARNING_PERCENT_MAX = 99;
constexpr uint8_t WARNING_5H_DEFAULT = 80;
constexpr uint8_t WARNING_7D_DEFAULT = 90;
uint8_t warningPercent5h();
uint8_t warningPercent7d();
void setWarningPercent5h(uint8_t percent);
void setWarningPercent7d(uint8_t percent);

// Quiet hours: no sounds play when the local wall-clock time is in [start, end), wrapping past midnight
constexpr uint8_t QUIET_HOUR_START_DEFAULT = 22;
constexpr uint8_t QUIET_MINUTE_START_DEFAULT = 0;
constexpr uint8_t QUIET_HOUR_END_DEFAULT = 8;
constexpr uint8_t QUIET_MINUTE_END_DEFAULT = 0;
bool quietHoursEnabled();
uint8_t quietHoursStart();        // 0-23
uint8_t quietMinuteStart();       // 0-59
uint8_t quietHoursEnd();          // 0-23
uint8_t quietMinuteEnd();         // 0-59
void setQuietHoursEnabled(bool enabled);
// Writes outside valid ranges are clamped; start == end disables the window
void setQuietHours(uint8_t startHour, uint8_t startMinute, uint8_t endHour, uint8_t endMinute);
// True when the given local time is inside the quiet window
bool isQuietTime(uint8_t localHour, uint8_t localMinute);

// Break hours: pause automatic usage polls and sleep until the window ends.
// Defaults to 01:00-06:00, disabled; same local-time/overnight rules as quiet hours.
bool breakHoursEnabled();
uint8_t breakHoursStart();
uint8_t breakMinuteStart();
uint8_t breakHoursEnd();
uint8_t breakMinuteEnd();
void setBreakHoursEnabled(bool enabled);
void setBreakHours(uint8_t startHour, uint8_t startMinute, uint8_t endHour, uint8_t endMinute);
// Zero outside Break Hours, otherwise the epoch at which automatic polls resume.
// The caller must have a valid clock before consulting the schedule.
time_t breakHoursResumeAt(time_t now);

// Audio playback volume as 0-100 (0 = near-mute, 100 = full). Mapped linearly to
// [-40, 0] dB when the codec is configured. Writes outside the range are clamped.
constexpr uint8_t AUDIO_VOLUME_DEFAULT = 80;
uint8_t audioVolume();
void setAudioVolume(uint8_t percent);

// Local time zone as a POSIX TZ string (what the C library's tzset() reads), plus the IANA
// name it was picked by in the panel, e.g. "CET-1CEST,M3.5.0,M10.5.0/3" + "Europe/Amsterdam"
const char *const TIME_ZONE_DEFAULT = "MYT-8";
const char *const TIME_ZONE_NAME_DEFAULT = "Asia/Kuala_Lumpur";
String timeZone();
String timeZoneName();
// Stores both and returns true, or stores nothing and returns false when either looks malformed
bool setTimeZone(const String &posix, const String &name);

// Display rotation in quarter turns clockwise: 0 = 0 deg (default), 1 = 90, 2 = 180, 3 = 270
uint8_t displayRotation();
void setDisplayRotation(uint8_t quarterTurns);

void setWifiSsid(const String &value);
void setWifiPassword(const String &value);
void setClaudeToken(int number, const String &value);
// An empty name restores the default
void setAccountName(int number, const String &value);

// For logs: length and the last few characters only, never the secret itself
String mask(const String &secret, size_t visibleTail = 4);
}
