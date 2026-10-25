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
constexpr uint16_t HIST_VERSION = 2;

struct HistFile
{
  uint32_t magic;
  uint16_t version;
  uint16_t accountCount; // sanity check against build-time CLAUDE_TOKEN_COUNT
  uint32_t lastAbsSlot;  // absolute slot (epoch/HIST_SLOT_SEC) of the newest sample
  HistSlot ring[settings::CLAUDE_TOKEN_COUNT][HIST_SLOTS];
};

// Version 1: the same header, but 336 slots of 1800 s. Read so an existing
// 7-day history survives the move to 15-minute sampling.
constexpr uint16_t V1_SLOTS = 336;
static_assert(HIST_SLOTS == V1_SLOTS * 2, "v1 migration assumes two new slots per old one");

struct HistFileV1
{
  uint32_t magic;
  uint16_t version;
  uint16_t accountCount;
  uint32_t lastAbsSlot;
  HistSlot ring[settings::CLAUDE_TOKEN_COUNT][V1_SLOTS];
};

HistFile hist;
bool fsOk = false;
bool slotAdvanced = false;

void clearRing()
{
  memset(hist.ring, HIST_EMPTY, sizeof(hist.ring));
  hist.lastAbsSlot = 0;
}

// A v1 slot spanned 1800 s, so it covers the two 900 s slots at 2*abs and
// 2*abs+1; both get its value. HIST_SLOTS is exactly twice V1_SLOTS, so the
// ring index doubles the same way the absolute slot does and the whole 7-day
// window lands in the same place it occupied before.
void upsampleV1(const HistFileV1 &old)
{
  clearRing();
  for (int a = 0; a < settings::CLAUDE_TOKEN_COUNT; a++)
  {
    for (uint16_t i = 0; i < V1_SLOTS; i++)
    {
      hist.ring[a][i * 2] = old.ring[a][i];
      hist.ring[a][i * 2 + 1] = old.ring[a][i];
    }
  }
  // The newer half of the old slot, so the next poll doesn't blank it as a gap
  hist.lastAbsSlot = old.lastAbsSlot == 0 ? 0 : old.lastAbsSlot * 2 + 1;
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
  // The size tells the versions apart before any of the body is trusted
  const size_t size = f.size();
  bool migrated = false;
  if (size == sizeof(HistFile))
  {
    HistFile onDisk;
    if (f.read((uint8_t *)&onDisk, sizeof(onDisk)) == sizeof(onDisk) &&
        onDisk.magic == HIST_MAGIC && onDisk.version == HIST_VERSION &&
        onDisk.accountCount == settings::CLAUDE_TOKEN_COUNT)
    {
      hist = onDisk;
    }
  }
  else if (size == sizeof(HistFileV1))
  {
    HistFileV1 old;
    if (f.read((uint8_t *)&old, sizeof(old)) == sizeof(old) && old.magic == HIST_MAGIC &&
        old.version == 1 && old.accountCount == settings::CLAUDE_TOKEN_COUNT)
    {
      upsampleV1(old);
      migrated = true;
    }
  }
  f.close();
  if (migrated)
  {
    // Rewrite now so the upsample runs once, not on every boot
    persist();
    Serial.println("History: upsampled 30-min samples to 15-min slots");
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

bool historyHasData(int account)
{
  if (account < 0 || account >= settings::CLAUDE_TOKEN_COUNT || hist.lastAbsSlot == 0)
  {
    return false;
  }
  for (uint16_t i = 0; i < HIST_SLOTS; i++)
  {
    if (hist.ring[account][i].h5 != HIST_EMPTY || hist.ring[account][i].d7 != HIST_EMPTY)
    {
      return true;
    }
  }
  return false;
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
  Serial.printf("History: 15-min slots, newest %s (slot %u)\n", when, hist.lastAbsSlot);
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

namespace
{
void formatSlotTime(char *out, size_t size, uint32_t epoch)
{
  const time_t when = (time_t)epoch;
  struct tm local;
  localtime_r(&when, &local);
  strftime(out, size, "%Y-%m-%d %H:%M", &local);
}

void formatSpan(char *out, size_t size, uint16_t slots)
{
  const uint32_t minutes = (uint32_t)slots * (HIST_SLOT_SEC / 60);
  const unsigned days = (unsigned)(minutes / 1440);
  const unsigned hours = (unsigned)((minutes % 1440) / 60);
  const unsigned mins = (unsigned)(minutes % 60);
  if (days && hours)
  {
    snprintf(out, size, "%ud %uh", days, hours);
  }
  else if (days)
  {
    snprintf(out, size, "%ud", days);
  }
  else if (hours && mins)
  {
    snprintf(out, size, "%uh %umin", hours, mins);
  }
  else if (hours)
  {
    snprintf(out, size, "%uh", hours);
  }
  else
  {
    snprintf(out, size, "%umin", mins);
  }
}
}

void printHistorySeries()
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

  char newest[20];
  formatSlotTime(newest, sizeof(newest), (uint32_t)(hist.lastAbsSlot + 1) * HIST_SLOT_SEC);
  Serial.printf("History: %u slots of %u min, newest %s\n", (unsigned)HIST_SLOTS,
                (unsigned)(HIST_SLOT_SEC / 60), newest);

  for (int a = 0; a < settings::CLAUDE_TOKEN_COUNT; a++)
  {
    HistSlot slots[HIST_SLOTS];
    uint32_t newestEpoch = 0;
    historySnapshot(a, slots, newestEpoch);
    Serial.printf("%s:\n", settings::accountName(a + 1).c_str());

    // Consecutive slots sharing both percentages collapse into one line, so an
    // idle stretch prints once instead of 672 times. Empty runs are the gaps;
    // the leading and trailing ones are just unfilled ring, not downtime.
    bool printedAny = false;
    uint16_t start = 0;
    for (uint16_t i = 1; i <= HIST_SLOTS; i++)
    {
      if (i < HIST_SLOTS && slots[i].h5 == slots[start].h5 && slots[i].d7 == slots[start].d7)
      {
        continue;
      }
      const uint16_t count = i - start;
      char span[16];
      formatSpan(span, sizeof(span), count);
      if (slots[start].h5 == HIST_EMPTY && slots[start].d7 == HIST_EMPTY)
      {
        if (start != 0 && i != HIST_SLOTS)
        {
          Serial.printf("  .... no samples for %s (%u slots)\n", span, (unsigned)count);
        }
      }
      else
      {
        char from[20], to[20];
        formatSlotTime(from, sizeof(from),
                       newestEpoch - (uint32_t)(HIST_SLOTS - start) * HIST_SLOT_SEC);
        formatSlotTime(to, sizeof(to), newestEpoch - (uint32_t)(HIST_SLOTS - i) * HIST_SLOT_SEC);
        Serial.printf("  %s .. %s  5h=%3u%%  7d=%3u%%  (%s)\n", from, to, slots[start].h5,
                      slots[start].d7, span);
        printedAny = true;
      }
      start = i;
    }
    if (!printedAny)
    {
      Serial.println("  no samples in the last 7 days");
    }
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
