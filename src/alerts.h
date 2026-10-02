#pragma once

#include <time.h>
#include "settings.h"
#include "usage_poll.h"

// Edge-triggered sound alerts (R4), per account and window:
// - warning when usage crosses the warning threshold (settings::warningPercent5h/7d)
// - depleted at 100% (replaces the warning when usage jumps straight there)
// - reset when the window's reset time passes
// Each fires once. State lives in NVS so a reboot or power cycle doesn't repeat an alert.
// Only accounts whose latest poll succeeded are checked. Plays the sounds before returning.
void checkAlerts(const AccountUsage accounts[settings::CLAUDE_TOKEN_COUNT], time_t now);

void printAlertState();

// Forgets every alert, so the next poll alerts again for anything above a threshold
void clearAlertState();
