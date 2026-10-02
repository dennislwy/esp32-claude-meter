#pragma once

#include <Arduino.h>
#include <time.h>
#include "settings.h"
#include "usage_poll.h"

enum class MeterView : uint8_t
{
  Dual,     // both accounts, Layout #2 (two cards with split bars)
  Account1, // one account, Layout #1 style (large % per window)
  Account2,
};

struct MeterScreen
{
  const AccountUsage *accounts; // settings::CLAUDE_TOKEN_COUNT entries
  String names[settings::CLAUDE_TOKEN_COUNT];
  MeterView view;
  time_t now;
  bool clockValid;
  uint8_t batteryPercent;
  const char *notice; // shown instead of the account(s) when set, e.g. setup instructions
};

// Rebuilds the LVGL screen. The caller refreshes the panel (lv_refr_now).
void meterUiShow(const MeterScreen &screen);
