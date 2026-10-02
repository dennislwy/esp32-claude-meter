#include "claude_usage.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace
{
const char *const HEADER_5H_UTILIZATION = "anthropic-ratelimit-unified-5h-utilization";
const char *const HEADER_5H_RESET = "anthropic-ratelimit-unified-5h-reset";
const char *const HEADER_7D_UTILIZATION = "anthropic-ratelimit-unified-7d-utilization";
const char *const HEADER_7D_RESET = "anthropic-ratelimit-unified-7d-reset";

constexpr uint16_t TIMEOUT_MS = 15000;
constexpr size_t ERROR_BODY_MAX = 300;

const char *const PROBE_BODY =
    "{\"model\":\"claude-haiku-4-5-20251001\",\"max_tokens\":1,"
    "\"messages\":[{\"role\":\"user\",\"content\":\".\"}]}";
}

ClaudeUsage fetchClaudeUsage(const char *token, const char *rootCaPem)
{
  ClaudeUsage usage = {};

  WiFiClientSecure client;
  // A PEM list rather than an ESP-IDF cert bundle: IDF 4.4's bundle only checks the topmost
  // issuer, which fails for chains cross-signed by roots Mozilla has since removed
  client.setCACert(rootCaPem);

  HTTPClient https;
  https.setTimeout(TIMEOUT_MS);
  if (!https.begin(client, "https://api.anthropic.com/v1/messages"))
  {
    usage.httpStatus = HTTPC_ERROR_CONNECTION_REFUSED;
    return usage;
  }
  https.addHeader("Authorization", String("Bearer ") + token);
  https.addHeader("anthropic-version", "2023-06-01");
  https.addHeader("anthropic-beta", "oauth-2025-04-20");
  https.addHeader("content-type", "application/json");
  https.setUserAgent("claude-code/2.1.5");
  const char *headers[] = {HEADER_5H_UTILIZATION, HEADER_5H_RESET, HEADER_7D_UTILIZATION, HEADER_7D_RESET};
  https.collectHeaders(headers, sizeof(headers) / sizeof(headers[0]));

  usage.httpStatus = https.POST(PROBE_BODY);
  if (usage.httpStatus > 0)
  {
    const String fiveHour = https.header(HEADER_5H_UTILIZATION);
    const String fiveHourReset = https.header(HEADER_5H_RESET);
    const String sevenDay = https.header(HEADER_7D_UTILIZATION);
    const String sevenDayReset = https.header(HEADER_7D_RESET);
    usage.valid = fiveHour.length() && fiveHourReset.length() && sevenDay.length() && sevenDayReset.length();
    usage.fiveHourPercent = fiveHour.toFloat() * 100.0f;
    usage.fiveHourReset = strtoul(fiveHourReset.c_str(), nullptr, 10);
    usage.sevenDayPercent = sevenDay.toFloat() * 100.0f;
    usage.sevenDayReset = strtoul(sevenDayReset.c_str(), nullptr, 10);

    if (usage.httpStatus != HTTP_CODE_OK)
    {
      usage.errorBody = https.getString().substring(0, ERROR_BODY_MAX);
    }
  }
  https.end();
  return usage;
}
