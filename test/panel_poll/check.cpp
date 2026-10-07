// Compile the real panel worker with thread/network adapters. No board or
// Anthropic calls: requests are held open to exercise publication and races.
#include "panel_usage_poll.h"
#include <WiFi.h>
#include <freertos/task.h>
#include <assert.h>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdio.h>
#include <thread>
#include <vector>

TestSerial Serial;
TestWifi WiFi;
namespace
{
String tokens[] = {"one", "two"};
bool pauseEnabled = false;
uint16_t pauseStart = 0, pauseEnd = 360;
bool rejectTask = false;
std::vector<std::thread> threads;
std::mutex mutex;
std::condition_variable condition;
bool hold = false, entered = false, released = false;
std::vector<String> requests;
ClaudeUsage responses[2];
AccountUsage cache[2];
const auto boot = std::chrono::steady_clock::now();

void join()
{
  for (auto &thread : threads) thread.join();
  threads.clear();
}

void reset()
{
  assert(!panelUsagePollPending());
  join();
  tokens[0] = "one"; tokens[1] = "two";
  pauseEnabled = false;
  WiFi.connection = WL_CONNECTED;
  hold = entered = released = false;
  requests.clear();
  for (int i = 0; i < 2; ++i)
  {
    cache[i] = {};
    cache[i].hasData = true;
    cache[i].fiveHourPercent = 10.0f + i;
    cache[i].sevenDayPercent = 20.0f + i;
    cache[i].fetchedAt = 1700000000;
    responses[i] = {};
    responses[i].valid = true;
    responses[i].httpStatus = 200;
    responses[i].fiveHourPercent = 30.0f + i;
    responses[i].sevenDayPercent = 40.0f + i;
    responses[i].fiveHourReset = 1900000000;
    responses[i].sevenDayReset = 1900100000;
  }
}

void waitForRequest()
{
  std::unique_lock<std::mutex> lock(mutex);
  assert(condition.wait_for(lock, std::chrono::seconds(2), [] { return entered; }));
}

void releaseRequest()
{
  std::lock_guard<std::mutex> lock(mutex);
  released = true;
  condition.notify_all();
}

PollReport collect(bool expectedCancelled = false)
{
  join();
  uint32_t start = 0;
  bool cancelled = false;
  PollReport report;
  assert(panelUsagePollPending());
  // Completed-but-uncollected work must also prevent overlapping jobs.
  assert(!startPanelUsagePoll(cache, false));
  assert(finishPanelUsagePoll(cache, report, start, cancelled));
  assert(cancelled == expectedCancelled);
  assert(start <= millis());
  assert(!panelUsagePollPending());
  assert(!finishPanelUsagePoll(cache, report, start, cancelled));
  return report;
}
}

uint32_t millis()
{
  return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - boot).count();
}
BaseType_t xTaskCreate(void (*entry)(void *), const char *, uint32_t, void *argument, unsigned, void *)
{
  if (rejectTask) { rejectTask = false; return 0; }
  threads.emplace_back([=] { entry(argument); });
  return pdPASS;
}
void vTaskDelete(void *) {}

namespace settings
{
String claudeToken(int number) { return tokens[number - 1]; }
String accountName(int number) { return number == 1 ? "first" : "second"; }
String wifiSsid() { return "test"; }
bool pauseHoursEnabled() { return pauseEnabled; }
uint8_t pauseHoursStart() { return (uint8_t)(pauseStart / 60); }
uint8_t pauseMinuteStart() { return (uint8_t)(pauseStart % 60); }
uint8_t pauseHoursEnd() { return (uint8_t)(pauseEnd / 60); }
uint8_t pauseMinuteEnd() { return (uint8_t)(pauseEnd % 60); }
}

ClaudeUsage probeToken(const String &token)
{
  std::unique_lock<std::mutex> lock(mutex);
  const size_t index = requests.size();
  requests.push_back(token);
  entered = true;
  condition.notify_all();
  if (hold && index == 0)
    condition.wait(lock, [] { return released; });
  assert(index < 2);
  return responses[index];
}

int main()
{
  reset();
  hold = true;
  assert(startPanelUsagePoll(cache, false));
  waitForRequest();
  assert(!startPanelUsagePoll(cache, true));
  // The loop can serve cached state while TLS is stalled on the worker.
  for (int service = 0; service < 50; ++service)
  {
    PollReport report; uint32_t started; bool cancelled;
    assert(!finishPanelUsagePoll(cache, report, started, cancelled));
    assert(cache[0].fiveHourPercent == 10 && cache[1].fiveHourPercent == 11);
  }
  releaseRequest();
  join();
  assert(cache[0].fiveHourPercent == 10); // No worker writes to the live cache.
  auto report = collect();
  assert(report.accountsOk == 2 && report.wifiConnected);
  assert(cache[0].fiveHourPercent == 30 && cache[1].fiveHourPercent == 31);
  assert(cache[0].fetchedAt > 1700000000);

  reset();
  responses[0].valid = false; responses[0].httpStatus = 429;
  responses[1].fiveHourPercent = 130;
  assert(startPanelUsagePoll(cache, false));
  report = collect();
  assert(report.accountsOk == 1 && report.accountsFailed == 1);
  assert(cache[0].lastPollFailed && cache[0].lastStatus == 429);
  assert(cache[0].fiveHourPercent == 10 && cache[0].fetchedAt == 1700000000);
  assert(cache[1].fiveHourPercent == 100);

  reset(); hold = true;
  assert(startPanelUsagePoll(cache, false)); waitForRequest();
  cancelPanelUsagePoll(); releaseRequest(); collect(true);
  assert(requests.size() == 1 && cache[0].fiveHourPercent == 10);

  reset(); hold = true;
  assert(startPanelUsagePoll(cache, false)); waitForRequest();
  tokens[0] = "replacement";
  releaseRequest(); collect(true);
  assert(cache[0].fiveHourPercent == 10 && cache[1].fiveHourPercent == 11);

  reset();
  const time_t now = time(nullptr); struct tm local;
#ifdef _WIN32
  localtime_s(&local, &now);
#else
  localtime_r(&now, &local);
#endif
  const int minute = local.tm_hour * 60 + local.tm_min;
  pauseStart = (uint16_t)((minute + 1439) % 1440);
  pauseEnd = (uint16_t)((minute + 2) % 1440);
  pauseEnabled = true;
  assert(startPanelUsagePoll(cache, false));
  report = collect();
  assert(report.paused && requests.empty() && cache[0].fiveHourPercent == 10);
  assert(startPanelUsagePoll(cache, true));
  report = collect();
  assert(!report.paused && requests.size() == 2);

  reset(); WiFi.connection = 0;
  assert(startPanelUsagePoll(cache, false));
  report = collect();
  assert(!report.wifiConnected && requests.empty() && cache[0].fiveHourPercent == 10);

  reset(); tokens[0] = tokens[1] = "";
  assert(startPanelUsagePoll(cache, false));
  report = collect();
  assert(!report.configured && requests.empty());

  reset(); rejectTask = true;
  assert(!startPanelUsagePoll(cache, false) && !panelUsagePollPending());
  assert(startPanelUsagePoll(cache, false)); collect();
  puts("PASS: nonblocking panel poll, result publication, failure cache, cancellation, token changes, pause/manual override, Wi-Fi loss and task-allocation recovery");
}
