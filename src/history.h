#pragma once

#include <stdint.h>
#include "settings.h"
#include "usage_poll.h"

// Per-account 7-day usage history: one slot per 15 minutes, 672 slots per
// account, ~2.6 KB total. Slots are addressed by absolute index (epoch/900)
// so gaps while the device was off stay visible as gaps. Persisted to LittleFS
// only when the slot advances (96 writes/day — wear-irrelevant).
constexpr uint16_t HIST_SLOTS = 672;
constexpr uint32_t HIST_SLOT_SEC = 900;
constexpr uint8_t HIST_EMPTY = 0xFF;

struct HistSlot
{
  uint8_t h5; // 0..100, HIST_EMPTY = no sample
  uint8_t d7;
};

// Mount LittleFS (expects uploadfs to have run) and load the ring
void historyInit();

// Call after each successful poll; records every account that returned fresh data
void historyRecord(const AccountUsage accounts[settings::CLAUDE_TOKEN_COUNT]);

// True once per new slot (chart redraw hint)
bool historySlotAdvancedTake();

// Fills out[HIST_SLOTS] oldest-to-newest for one account; newestEpoch = end of
// the newest slot. account is 0..CLAUDE_TOKEN_COUNT-1.
void historySnapshot(int account, HistSlot *out, uint32_t &newestEpoch);

// True when the account has at least one recorded sample (used to hide the
// chart view until there's something to draw)
bool historyHasData(int account);

// Serial dump: coverage per account and newest-slot timestamp
void printHistoryState();

// Serial dump: the recorded samples per account with the off periods between
// them, which a coverage percentage alone hides.
void printHistorySeries();

// Wipe ring and file (factory reset)
void historyErase();
