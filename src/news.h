#pragma once

#include <Arduino.h>

// Anthropic news headlines for the Web Panel. Fetched once per panel session: the panel asks
// for a fetch when it opens, and the main loop runs it (Wi-Fi is up in panel mode). The feed
// is an unofficial RSS mirror of anthropic.com/news; only the first few items are read.

constexpr uint8_t NEWS_MAX_ITEMS = 10;

struct NewsItem
{
  char title[112]; // UTF-8, entities decoded, truncated on a character boundary
  char date[12];   // "27 Aug 2026"
  char link[160];  // https:// only, "" otherwise
};

struct NewsState
{
  NewsItem items[NEWS_MAX_ITEMS];
  uint8_t count;
  uint32_t fetchedAt; // Unix time of the last successful fetch, 0 = never
  bool ok;            // the last attempt succeeded
  bool pending;       // a fetch has been asked for and hasn't run yet
};

extern const char *const NEWS_FEED_URL;

// Marks a fetch as due; newsService() runs it
void newsRequestFetch();

// Runs a due fetch, blocking for up to ~15 s. Call from the main loop while Wi-Fi is up.
// A failed fetch keeps the earlier items and clears `ok`.
void newsService();

const NewsState &newsState();
