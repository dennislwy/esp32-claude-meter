#pragma once

#include <stdint.h>
#include "settings.h"
#include "usage_poll.h"

// Per-account 7-day usage history: one slot per 30 minutes, 336 slots per
// account, ~1.3 KB total. Slots are addressed by absolute index (epoch/1800)
// so gaps while the device was off stay visible as gaps. Persisted to LittleFS
// only when the slot advances (48 writes/day — wear-irrelevant).
constexpr uint16_t HIST_SLOTS = 336;
constexpr uint32_t HIST_SLOT_SEC = 1800;
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

// Serial dump: coverage per account and newest-slot timestamp
void printHistoryState();

// Wipe ring and file (factory reset)
void historyErase();
