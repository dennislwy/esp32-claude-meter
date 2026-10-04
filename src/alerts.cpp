#include "alerts.h"

#include <Preferences.h>
#include "audio_player.h"

namespace
{
const char *const NAMESPACE = "alerts";

enum Level : uint8_t
{
  NORMAL,
  WARNING,
  DEPLETED,
};
const char *const LEVEL_NAMES[] = {"normal", "warning", "depleted"};

// The headers report utilisation as a 0-1 fraction with two decimals
constexpr float DEPLETED_PERCENT = 99.95f;

struct Window
{
  const char *name;
  const char *warningSound;
  const char *depletedSound;
  const char *resetSound;
};
const Window WINDOWS[] = {
    {"5h", "/5h-warning.wav", "/5h-depleted.wav", "/5h-reset.wav"},
    {"7d", "/7d-warning.wav", "/7d-depleted.wav", "/7d-reset.wav"},
};
constexpr int WINDOW_COUNT = sizeof(WINDOWS) / sizeof(WINDOWS[0]);

constexpr size_t MAX_SOUNDS = settings::CLAUDE_TOKEN_COUNT * WINDOW_COUNT * 2;

// NVS keys, e.g. "lvl1_5h": alert level reached in the current window; "rst1_5h": the reset time
// we're waiting for (0 = none)
String key(const char *prefix, int account, int window)
{
  return String(prefix) + (account + 1) + "_" + WINDOWS[window].name;
}

float windowPercent(const AccountUsage &account, int window)
{
  return window == 0 ? account.fiveHourPercent : account.sevenDayPercent;
}

uint32_t windowReset(const AccountUsage &account, int window)
{
  return window == 0 ? account.fiveHourReset : account.sevenDayReset;
}

uint8_t warningPercent(int window)
{
  return window == 0 ? settings::warningPercent5h() : settings::warningPercent7d();
}

Level levelFor(float percent, int window)
{
  return percent >= DEPLETED_PERCENT ? DEPLETED : percent >= warningPercent(window) ? WARNING : NORMAL;
}

// Several accounts can trigger the same sound in one poll; play it once
void addSound(const char *sounds[], size_t &count, const char *sound)
{
  for (size_t i = 0; i < count; i++)
  {
    if (sounds[i] == sound)
    {
      return;
    }
  }
  sounds[count++] = sound;
}
}

void checkAlerts(const AccountUsage accounts[settings::CLAUDE_TOKEN_COUNT], time_t now)
{
  const char *sounds[MAX_SOUNDS];
  size_t soundCount = 0;

  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; i++)
  {
    const AccountUsage &account = accounts[i];
    if (!account.hasData || account.lastPollFailed || settings::claudeToken(i + 1).isEmpty())
    {
      continue;
    }
    const String name = settings::accountName(i + 1);
    for (int w = 0; w < WINDOW_COUNT; w++)
    {
      const Window &window = WINDOWS[w];
      const String levelKey = key("lvl", i, w);
      const String resetKey = key("rst", i, w);
      const Level stored = (Level)(prefs.isKey(levelKey.c_str()) ? prefs.getUChar(levelKey.c_str()) : NORMAL);
      const uint32_t pendingReset = prefs.isKey(resetKey.c_str()) ? prefs.getULong(resetKey.c_str()) : 0;

      if (pendingReset != 0 && now >= (time_t)pendingReset)
      {
        Serial.printf("Alert: %s %s reset\n", name.c_str(), window.name);
        addSound(sounds, soundCount, window.resetSound);
      }

      // Rising only: a drop (after a reset, or a raised threshold) just updates the stored level
      const Level level = levelFor(windowPercent(account, w), w);
      if (level > stored)
      {
        Serial.printf("Alert: %s %s %s (%.0f%%)\n", name.c_str(), window.name, LEVEL_NAMES[level], windowPercent(account, w));
        addSound(sounds, soundCount, level == DEPLETED ? window.depletedSound : window.warningSound);
      }
      if (level != stored)
      {
        prefs.putUChar(levelKey.c_str(), level);
      }

      const uint32_t reset = windowReset(account, w);
      const uint32_t nextReset = (time_t)reset > now ? reset : 0;
      if (nextReset != pendingReset)
      {
        prefs.putULong(resetKey.c_str(), nextReset);
      }
    }
  }
  prefs.end();

  // State is updated first so alerts don't catch up when quiet hours end; silence here is permanent
  struct tm local;
  localtime_r(&now, &local);
  if (settings::isQuietTime(local.tm_hour, local.tm_min))
  {
    if (soundCount > 0)
    {
      Serial.printf("Quiet hours (%02u:%02u-%02u:%02u): %u alert sound(s) silenced\n",
                    settings::quietHoursStart(), settings::quietMinuteStart(),
                    settings::quietHoursEnd(), settings::quietMinuteEnd(), (unsigned)soundCount);
    }
    return;
  }
  for (size_t i = 0; i < soundCount; i++)
  {
    playWav(sounds[i]);
  }
}

void printAlertState()
{
  Serial.printf("Warning at: 5h %u%%, 7d %u%%\n", settings::warningPercent5h(), settings::warningPercent7d());
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; i++)
  {
    for (int w = 0; w < WINDOW_COUNT; w++)
    {
      const String levelKey = key("lvl", i, w);
      const String resetKey = key("rst", i, w);
      const uint8_t level = prefs.isKey(levelKey.c_str()) ? prefs.getUChar(levelKey.c_str()) : NORMAL;
      const time_t reset = prefs.isKey(resetKey.c_str()) ? prefs.getULong(resetKey.c_str()) : 0;
      char when[24] = "none";
      if (reset)
      {
        struct tm local;
        localtime_r(&reset, &local);
        strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &local);
      }
      Serial.printf("  %-20s %s  %-8s  reset alert at %s\n", settings::accountName(i + 1).c_str(), WINDOWS[w].name,
                    LEVEL_NAMES[level < 3 ? level : 0], when);
    }
  }
  prefs.end();
}

void clearAlertState()
{
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  prefs.clear();
  prefs.end();
}
