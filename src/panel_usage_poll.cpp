#include "panel_usage_poll.h"

#include <atomic>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "daily_window.h"

namespace
{
enum class State { Idle, Running, Ready };
std::atomic<State> state{State::Idle};
std::atomic<bool> cancelRequested{false};

struct Job
{
  String tokens[settings::CLAUDE_TOKEN_COUNT];
  String names[settings::CLAUDE_TOKEN_COUNT];
  AccountUsage accounts[settings::CLAUDE_TOKEN_COUNT];
  bool attempted[settings::CLAUDE_TOKEN_COUNT];
  PollReport report;
  uint32_t startedAtMs;
  bool force;
  bool pauseEnabled;
  uint16_t pauseStart;
  uint16_t pauseEnd;
};
Job job;

void clearTokens()
{
  for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; ++i)
  {
    job.tokens[i] = "";
    job.names[i] = "";
  }
}

void run(void *)
{
  for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; ++i)
  {
    if (cancelRequested.load() || !job.report.configured)
      break;
    if (job.tokens[i].isEmpty())
      continue;
    // A request may cross the pause boundary: finish it, but don't start
    // another account inside the window. Settings changes cancel the snapshot.
    if (!job.force && dailyWindowResumeAt(time(nullptr), job.pauseEnabled, job.pauseStart, job.pauseEnd))
    {
      job.report.paused = job.report.accountsOk == 0 && job.report.accountsFailed == 0;
      break;
    }
    job.report.wifiConnected = WiFi.status() == WL_CONNECTED;
    job.report.wifiStatus = WiFi.status();
    if (!job.report.wifiConnected)
      break;
    job.report.wifiRssi = WiFi.RSSI();
    const uint32_t start = millis();
    const ClaudeUsage response = probeToken(job.tokens[i]);
    job.attempted[i] = true;
    AccountUsage &account = job.accounts[i];
    account.lastStatus = response.httpStatus;
    account.lastPollFailed = !response.valid;
    if (response.valid)
    {
      account.hasData = true;
      account.fiveHourPercent = constrain(response.fiveHourPercent, 0.0f, 100.0f);
      account.sevenDayPercent = constrain(response.sevenDayPercent, 0.0f, 100.0f);
      account.fiveHourReset = response.fiveHourReset;
      account.sevenDayReset = response.sevenDayReset;
      account.fetchedAt = static_cast<uint32_t>(time(nullptr));
      ++job.report.accountsOk;
    }
    else
      ++job.report.accountsFailed;
    Serial.printf("Panel poll: %s HTTP %d, %lu ms%s\n", job.names[i].c_str(), response.httpStatus,
                  (unsigned long)(millis() - start), response.valid ? "" : " - keeping cached usage");
  }
  // Publish only after all writes. The loop consumes the snapshots; the task
  // touches none of them after this release and only deletes its own stack.
  state.store(State::Ready, std::memory_order_release);
  vTaskDelete(nullptr);
}
}

bool startPanelUsagePoll(const AccountUsage accounts[settings::CLAUDE_TOKEN_COUNT], bool force)
{
  if (panelUsagePollPending())
    return false;
  job.report = {};
  job.force = force;
  job.pauseEnabled = settings::pauseHoursEnabled();
  job.pauseStart = settings::pauseHoursStart() * 60 + settings::pauseMinuteStart();
  job.pauseEnd = settings::pauseHoursEnd() * 60 + settings::pauseMinuteEnd();
  for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; ++i)
  {
    job.tokens[i] = settings::claudeToken(i + 1);
    job.names[i] = settings::accountName(i + 1);
    job.accounts[i] = accounts[i];
    job.attempted[i] = false;
    job.report.configured |= !job.tokens[i].isEmpty();
  }
  job.report.configured &= !settings::wifiSsid().isEmpty();
  job.startedAtMs = millis();
  cancelRequested.store(false);
  state.store(State::Running, std::memory_order_release);
  // ESP-IDF's task stack size is in bytes. TLS runs on the worker's stack;
  // polling is infrequent, so release the stack between jobs.
  if (xTaskCreate(run, "panel-usage", 12288, nullptr, 1, nullptr) != pdPASS)
  {
    clearTokens();
    state.store(State::Idle);
    return false;
  }
  return true;
}

bool panelUsagePollPending()
{
  return state.load(std::memory_order_acquire) != State::Idle;
}

void cancelPanelUsagePoll()
{
  if (panelUsagePollPending())
    cancelRequested.store(true);
}

bool finishPanelUsagePoll(AccountUsage accounts[settings::CLAUDE_TOKEN_COUNT],
                          PollReport &report, uint32_t &startedAtMs, bool &cancelled)
{
  if (state.load(std::memory_order_acquire) != State::Ready)
    return false;
  report = job.report;
  startedAtMs = job.startedAtMs;
  cancelled = cancelRequested.load();
  if (!cancelled)
  {
    for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; ++i)
      if (job.tokens[i] != settings::claudeToken(i + 1))
        cancelled = true;
    // Don't record old-account samples or alerts if credentials changed
    // while the HTTP request was running. A new poll is scheduled by the loop.
    if (!cancelled)
      for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; ++i)
        if (job.attempted[i])
          accounts[i] = job.accounts[i];
  }
  clearTokens();
  state.store(State::Idle, std::memory_order_release);
  return true;
}
