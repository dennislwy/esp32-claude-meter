#pragma once

#include <stddef.h>
#include <time.h>
#include "settings.h"
#include "usage_poll.h"

// Edge-triggered sound alerts (R4), per account and window:
// - warning when usage crosses the warning threshold (settings::warningPercent5h/7d)
// - depleted at 100% (replaces the warning when usage jumps straight there)
// - reset when the window's reset time passes
// Each fires once. State lives in NVS so a reboot or power cycle doesn't repeat an alert.
// Only accounts whose latest poll succeeded are checked.

// What one evaluation pass found. Sounds are queued rather than played so the caller
// can draw the screen first and have the sound land on the view it refers to.
struct AlertOutcome
{
  bool fired[settings::CLAUDE_TOKEN_COUNT]; // accounts that raised an event this pass
  uint8_t firedCount;
  bool silenced; // quiet hours suppressed the sounds
  // Two windows per account, each contributing at most one level sound (warning and
  // depleted are mutually exclusive in one pass) plus one reset sound.
  const char *sounds[settings::CLAUDE_TOKEN_COUNT * 4];
  size_t soundCount;
};

// Updates the stored alert state and reports what fired. Plays nothing.
AlertOutcome evaluateAlerts(const AccountUsage accounts[settings::CLAUDE_TOKEN_COUNT], time_t now);

// Plays whatever evaluateAlerts queued, unless quiet hours silenced it.
void playAlertSounds(const AlertOutcome &outcome);

void printAlertState();

// Forgets every alert, so the next poll alerts again for anything above a threshold
void clearAlertState();
