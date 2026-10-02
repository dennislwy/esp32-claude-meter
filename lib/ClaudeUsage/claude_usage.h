#pragma once

#include <Arduino.h>

struct ClaudeUsage
{
  int httpStatus;           // negative on connection failure
  float fiveHourPercent;    // 5-hour session window, 0-100
  uint32_t fiveHourReset;   // Unix time the 5-hour window resets
  float sevenDayPercent;    // 7-day window, 0-100
  uint32_t sevenDayReset;   // Unix time the 7-day window resets
  bool valid;               // all four rate-limit headers were present
  String errorBody;         // start of the response body when the status isn't 200
};

// Reads Claude plan utilisation from the anthropic-ratelimit-unified-* headers of a
// 1-token Messages API request. The headers come back whether or not the request succeeds.
// token: from `claude setup-token`. rootCaPem: null-terminated PEM with the root certificate(s)
// trusted for api.anthropic.com.
ClaudeUsage fetchClaudeUsage(const char *token, const char *rootCaPem);
