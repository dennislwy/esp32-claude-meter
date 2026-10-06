#pragma once

#include "usage_poll.h"

// Main-loop-only API. The worker uses its own snapshots and never owns Wi-Fi,
// the RTC, history, alert playback, or the account cache exposed by the panel.
bool startPanelUsagePoll(const AccountUsage accounts[settings::CLAUDE_TOKEN_COUNT], bool force);
// True until a running or completed job has been collected.
bool panelUsagePollPending();
// Cooperative: finishes the current HTTPS request, skips subsequent accounts,
// and discards the result. Collect it before leaving panel mode.
void cancelPanelUsagePoll();
// Copies eligible results into the cache on the main loop. Changed tokens and
// cancelled jobs cannot replace cached data. Returns false while still running.
bool finishPanelUsagePoll(AccountUsage accounts[settings::CLAUDE_TOKEN_COUNT],
                          PollReport &report, uint32_t &startedAtMs, bool &cancelled);
