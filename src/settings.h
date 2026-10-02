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

// Quiet hours: no sounds play when the local hour is in [start, end) (wrapping past midnight)
constexpr uint8_t QUIET_HOUR_START_DEFAULT = 22;
constexpr uint8_t QUIET_HOUR_END_DEFAULT = 8;
bool quietHoursEnabled();
uint8_t quietHoursStart();
uint8_t quietHoursEnd();
void setQuietHoursEnabled(bool enabled);
// Hours 0-23; start == end disables the window; writes outside that range are clamped
void setQuietHours(uint8_t startHour, uint8_t endHour);
// True when the given local hour (0-23) is inside the quiet window
bool isQuietHour(uint8_t localHour);

void setWifiSsid(const String &value);
void setWifiPassword(const String &value);
void setClaudeToken(int number, const String &value);
// An empty name restores the default
void setAccountName(int number, const String &value);

// For logs: length and the last few characters only, never the secret itself
String mask(const String &secret, size_t visibleTail = 4);
}
