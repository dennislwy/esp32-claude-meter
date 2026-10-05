#pragma once

#include <Arduino.h>
#include <time.h>
#include "settings.h"
#include "usage_poll.h"

enum class MeterView : uint8_t
{
  Dual,            // both accounts, Layout #2 (two cards with split bars)
  Account1,        // one account, Layout #1 style (large % per window)
  Account1History, // 7-day line chart for account 1
  Account2,
  Account2History, // 7-day line chart for account 2
  Panel,           // R8 LAN panel: hostname/IP + login PIN
  Setup,           // R8 Phase 2: AP captive portal for first-time Wi-Fi setup
};

// Result of the last poll's Wi-Fi connection (Wi-Fi is off between polls)
enum class WifiState : uint8_t
{
  Unknown, // not tried yet, or not configured: no icon
  Connected,
  Failed,
};

struct MeterScreen
{
  const AccountUsage *accounts; // settings::CLAUDE_TOKEN_COUNT entries
  String names[settings::CLAUDE_TOKEN_COUNT];
  MeterView view;
  time_t now;
  bool clockValid;
  uint8_t batteryPercent;
  WifiState wifiState;
  int8_t wifiRssi; // dBm, when connected
  const char *notice; // shown instead of the account(s) when set, e.g. setup instructions
  const char *popupTitle; // when set, a box centred over the view, e.g. a Wi-Fi failure
  String popupBody;
  // Only used when view == Panel
  String panelHostname;
  String panelIp;
  String panelPin;
  // Only used when view == Setup
  String setupApSsid; // "claude-meter-XXXXXX"
  String setupApIp;   // "192.168.4.1"
};

// Rebuilds the LVGL screen. The caller refreshes the panel (lv_refr_now).
void meterUiShow(const MeterScreen &screen);
