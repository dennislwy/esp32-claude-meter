#pragma once

#include <Arduino.h>
#include "pcf85063.h"
#include "settings.h"

// Last known usage for one account; kept in RTC memory across deep sleep
struct AccountUsage
{
  bool hasData;           // at least one successful read
  bool lastPollFailed;
  int lastStatus;         // HTTP status of the last attempt; negative = no connection
  float fiveHourPercent;
  float sevenDayPercent;
  uint32_t fiveHourReset; // Unix time
  uint32_t sevenDayReset;
  uint32_t fetchedAt;     // Unix time of the last successful read
};

struct PollReport
{
  bool configured;    // Wi-Fi set and at least one token
  bool wifiConnected;
  int8_t wifiRssi;    // dBm, when connected
  uint8_t wifiStatus; // wl_status_t when the connection failed, e.g. WL_NO_SSID_AVAIL
  bool clockSynced;   // NTP ran and succeeded
  int accountsOk;
  int accountsFailed;
};

// Connects to Wi-Fi, syncs the clock over NTP when syncClock is set, reads every account that has a
// token, then turns Wi-Fi off. Accounts that fail keep their previous data.
PollReport pollUsage(AccountUsage accounts[settings::CLAUDE_TOKEN_COUNT], Pcf85063 &rtc, bool syncClock);

// Serial printout of the cached usage, with countdowns from the current system time
void printUsage(const AccountUsage accounts[settings::CLAUDE_TOKEN_COUNT]);

// Diagnostic: lists the Wi-Fi networks in range, flagging the saved SSID
void runWifiScan();

// Diagnostic: opens a TLS connection to api.anthropic.com without verification, prints the
// certificate chain the server presents, then disconnects. Sends no request and no token.
void runTlsCheck();
