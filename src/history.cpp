#include "history.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <time.h>

namespace
{
// NTP sanity floor: epochs below this mean the clock isn't set and slot math
// would poison the ring.
constexpr uint32_t TIME_SANE_EPOCH = 1700000000UL;
const char *const HIST_PATH = "/history.bin";

constexpr uint32_t HIST_MAGIC = 0x31484D43; // "CMH1" little-endian
constexpr uint16_t HIST_VERSION = 1;

struct HistFile
{
  uint32_t magic;
  uint16_t version;
  uint16_t accountCount; // sanity check against build-time CLAUDE_TOKEN_COUNT
  uint32_t lastAbsSlot;  // absolute slot (epoch/1800) of the newest sample
  HistSlot ring[settings::CLAUDE_TOKEN_COUNT][HIST_SLOTS];
};

HistFile hist;
bool fsOk = false;
bool slotAdvanced = false;

void clearRing()
{
  memset(hist.ring, HIST_EMPTY, sizeof(hist.ring));
  hist.lastAbsSlot = 0;
}

void persist()
{
  if (!fsOk)
  {
    return;
  }
  File f = LittleFS.open(HIST_PATH, "w");
  if (!f)
  {
    return;
  }
  f.write((const uint8_t *)&hist, sizeof(hist));
  f.close();
}
}

void historyInit()
{
  hist.magic = HIST_MAGIC;
  hist.version = HIST_VERSION;
  hist.accountCount = settings::CLAUDE_TOKEN_COUNT;
  clearRing();

  // Don't format on failure: the WAV files are on this filesystem and formatting would wipe them.
  // begin() is idempotent, so a previous mount from listFiles() is fine.
  fsOk = LittleFS.begin(false);
  if (!fsOk)
  {
    return;
  }

  File f = LittleFS.open(HIST_PATH, "r");
  if (!f)
  {
    return;
  }
  HistFile onDisk;
  const bool ok = f.read((uint8_t *)&onDisk, sizeof(onDisk)) == sizeof(onDisk) &&
                  onDisk.magic == HIST_MAGIC && onDisk.version == HIST_VERSION &&
                  onDisk.accountCount == settings::CLAUDE_TOKEN_COUNT;
  f.close();
  if (ok)
  {
    hist = onDisk;
  }
}

void historyRecord(const AccountUsage accounts[settings::CLAUDE_TOKEN_COUNT])
{
  const uint32_t now = (uint32_t)time(nullptr);
  if (now < TIME_SANE_EPOCH)
  {
    return;
  }

  const uint32_t absSlot = now / HIST_SLOT_SEC;
  if (hist.lastAbsSlot == 0)
  {
    clearRing();
  }
  else if (absSlot > hist.lastAbsSlot)
  {
    // Blank everything skipped while the device was off/failing so old wrap-around samples
    // can't masquerade as fresh ones.
    const uint32_t gap = absSlot - hist.lastAbsSlot;
    if (gap >= HIST_SLOTS)
    {
      clearRing();
    }
    else
    {
      for (uint32_t s = hist.lastAbsSlot + 1; s <= absSlot; s++)
      {
        for (int a = 0; a < settings::CLAUDE_TOKEN_COUNT; a++)
        {
          hist.ring[a][s % HIST_SLOTS] = {HIST_EMPTY, HIST_EMPTY};
        }
      }
    }
  }

  bool wroteAny = false;
  for (int a = 0; a < settings::CLAUDE_TOKEN_COUNT; a++)
  {
    const AccountUsage &acc = accounts[a];
    // Skip accounts with no data or a stale cached value from a failed poll
    if (!acc.hasData || acc.lastPollFailed)
    {
      continue;
    }
    HistSlot &slot = hist.ring[a][absSlot % HIST_SLOTS];
    slot.h5 = (uint8_t)constrain((int)(acc.fiveHourPercent + 0.5f), 0, 100);
    slot.d7 = (uint8_t)constrain((int)(acc.sevenDayPercent + 0.5f), 0, 100);
    wroteAny = true;
  }
  if (!wroteAny)
  {
    return;
  }

  if (absSlot != hist.lastAbsSlot)
  {
    hist.lastAbsSlot = absSlot;
    slotAdvanced = true;
    persist();
  }
}

bool historySlotAdvancedTake()
{
  const bool r = slotAdvanced;
  slotAdvanced = false;
  return r;
}

void historySnapshot(int account, HistSlot *out, uint32_t &newestEpoch)
{
  if (account < 0 || account >= settings::CLAUDE_TOKEN_COUNT)
  {
    return;
  }
  const uint32_t now = (uint32_t)time(nullptr);
  uint32_t nowAbs = (now >= TIME_SANE_EPOCH) ? now / HIST_SLOT_SEC : hist.lastAbsSlot;
  // Nothing recorded and no clock: show an all-empty window
  if (nowAbs == 0)
  {
    nowAbs = HIST_SLOTS;
  }
  newestEpoch = (nowAbs + 1) * HIST_SLOT_SEC;

  for (uint16_t i = 0; i < HIST_SLOTS; i++)
  {
    const uint32_t absSlot = nowAbs - (HIST_SLOTS - 1) + i;
    const bool valid = hist.lastAbsSlot != 0 &&
                       absSlot <= hist.lastAbsSlot &&
                       absSlot + HIST_SLOTS > hist.lastAbsSlot;
    out[i] = valid ? hist.ring[account][absSlot % HIST_SLOTS]
                   : HistSlot{HIST_EMPTY, HIST_EMPTY};
  }
}

void printHistoryState()
{
  if (!fsOk)
  {
    Serial.println("History: LittleFS not mounted - run: pio run -t uploadfs");
    return;
  }
  if (hist.lastAbsSlot == 0)
  {
    Serial.println("History: empty (no samples yet)");
    return;
  }
  char when[24] = "unknown";
  const time_t newest = (time_t)(hist.lastAbsSlot + 1) * HIST_SLOT_SEC;
  struct tm local;
  localtime_r(&newest, &local);
  strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &local);
  Serial.printf("History: 30-min slots, newest %s (slot %u)\n", when, hist.lastAbsSlot);
  for (int a = 0; a < settings::CLAUDE_TOKEN_COUNT; a++)
  {
    uint16_t filled = 0;
    for (uint16_t i = 0; i < HIST_SLOTS; i++)
    {
      if (hist.ring[a][i].h5 != HIST_EMPTY || hist.ring[a][i].d7 != HIST_EMPTY)
      {
        filled++;
      }
    }
    Serial.printf("  %-20s %3u / %u slots filled (%u%%)\n", settings::accountName(a + 1).c_str(),
                  filled, HIST_SLOTS, (unsigned)(filled * 100U / HIST_SLOTS));
  }
}

void historyErase()
{
  clearRing();
  slotAdvanced = false;
  if (fsOk)
  {
    LittleFS.remove(HIST_PATH);
  }
}
