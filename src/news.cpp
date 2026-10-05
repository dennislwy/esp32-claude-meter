#include "news.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <time.h>

// Trusted roots for the feed host (assets/certs/news_roots.pem), embedded null-terminated
// via board_build.embed_txtfiles
extern const char newsRootsPem[] asm("_binary_assets_certs_news_roots_pem_start");

const char *const NEWS_FEED_URL =
    "https://raw.githubusercontent.com/Olshansk/rss-feeds/main/feeds/feed_anthropic_news.xml";

namespace
{
constexpr uint32_t TIMEOUT_MS = 10000;

NewsState state = {};

// Appends a code point as UTF-8, if it fits whole
size_t putCodePoint(char *dst, size_t len, size_t cap, uint32_t cp)
{
  char buf[4];
  size_t n;
  if (cp < 0x80)
  {
    buf[0] = (char)cp;
    n = 1;
  }
  else if (cp < 0x800)
  {
    buf[0] = (char)(0xC0 | (cp >> 6));
    buf[1] = (char)(0x80 | (cp & 0x3F));
    n = 2;
  }
  else if (cp < 0x10000)
  {
    buf[0] = (char)(0xE0 | (cp >> 12));
    buf[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[2] = (char)(0x80 | (cp & 0x3F));
    n = 3;
  }
  else if (cp < 0x110000)
  {
    buf[0] = (char)(0xF0 | (cp >> 18));
    buf[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    buf[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    buf[3] = (char)(0x80 | (cp & 0x3F));
    n = 4;
  }
  else
  {
    return len;
  }
  if (len + n >= cap)
  {
    return len;
  }
  memcpy(dst + len, buf, n);
  return len + n;
}

// Decodes &entities; and folds whitespace runs into single spaces. Raw UTF-8 passes through;
// truncation backs off to a character boundary so the JSON never carries a split sequence.
void decodeText(const char *raw, char *dst, size_t cap)
{
  size_t len = 0;
  bool space = true; // drops leading whitespace too
  for (size_t i = 0; raw[i] && len + 1 < cap;)
  {
    const unsigned char c = raw[i];
    uint32_t cp = c;
    if (c == '&')
    {
      char ent[12];
      size_t e = 0;
      size_t j = i + 1;
      while (raw[j] && raw[j] != ';' && e < sizeof(ent) - 1)
        ent[e++] = raw[j++];
      ent[e] = '\0';
      if (raw[j] == ';')
      {
        i = j + 1;
        if (!strcmp(ent, "amp"))
          cp = '&';
        else if (!strcmp(ent, "lt"))
          cp = '<';
        else if (!strcmp(ent, "gt"))
          cp = '>';
        else if (!strcmp(ent, "quot"))
          cp = '"';
        else if (!strcmp(ent, "apos"))
          cp = '\'';
        else if (!strcmp(ent, "nbsp"))
          cp = ' ';
        else if (ent[0] == '#')
          cp = (ent[1] == 'x' || ent[1] == 'X') ? strtoul(ent + 2, nullptr, 16) : strtoul(ent + 1, nullptr, 10);
        else
          cp = '?';
      }
      else
      {
        i++; // no terminator: a literal ampersand
      }
    }
    else if (c >= 0x80)
    {
      // Copy one whole UTF-8 sequence, or stop if it doesn't fit
      size_t n = (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
      if (len + n >= cap)
        break;
      for (size_t k = 0; k < n && raw[i]; k++)
        dst[len++] = raw[i++];
      space = false;
      continue;
    }
    else
    {
      i++;
    }
    const bool isSpace = cp == ' ' || cp == '\n' || cp == '\r' || cp == '\t';
    if (isSpace)
    {
      if (!space)
        len = putCodePoint(dst, len, cap, ' ');
      space = true;
      continue;
    }
    if (cp < 0x20)
      continue;
    len = putCodePoint(dst, len, cap, cp);
    space = false;
  }
  while (len > 0 && dst[len - 1] == ' ')
    len--;
  dst[len] = '\0';
}

// "Wed, 27 Aug 2026 00:00:00 +0000" -> "27 Aug 2026"
void decodeDate(const char *raw, char *dst, size_t cap)
{
  char day[4] = "", mon[4] = "", year[6] = "";
  if (sscanf(raw, " %*s %3s %3s %5s", day, mon, year) == 3 && isDigit(day[0]))
    snprintf(dst, cap, "%s %s %s", day, mon, year);
  else
    dst[0] = '\0';
}

// Per-byte RSS 2.0 state machine, so tags and entities split across TCP reads need no
// reassembly. Captures title, pubDate and link of the first NEWS_MAX_ITEMS items, then stops.
struct RssParser
{
  enum Mode : uint8_t
  {
    TEXT,
    TAG,
    CDATA
  } mode = TEXT;
  enum Field : uint8_t
  {
    NONE,
    TITLE,
    DATE,
    LINK
  } field = NONE;
  char tag[24];
  uint8_t tagLen = 0;
  bool tagClipped = false;
  char raw[256];
  uint16_t rawLen = 0;
  uint8_t cdataTail = 0; // matched characters of "]]>"
  bool inItem = false;
  NewsItem item;
  NewsItem items[NEWS_MAX_ITEMS];
  uint8_t count = 0;
  bool done = false;

  void put(char c)
  {
    if (field != NONE && rawLen + 1 < sizeof(raw))
      raw[rawLen++] = c;
  }

  void start(Field f)
  {
    field = f;
    rawLen = 0;
  }

  void tagComplete()
  {
    tag[tagLen] = '\0';
    char *space = strchr(tag, ' ');
    if (space)
      *space = '\0';
    if (!strcmp(tag, "item"))
    {
      inItem = true;
      memset(&item, 0, sizeof(item));
    }
    else if (!strcmp(tag, "/item"))
    {
      if (inItem && item.title[0] && count < NEWS_MAX_ITEMS)
      {
        items[count++] = item;
        done = count >= NEWS_MAX_ITEMS;
      }
      inItem = false;
    }
    else if (!inItem)
    {
      return;
    }
    else if (!strcmp(tag, "title"))
      start(TITLE);
    else if (!strcmp(tag, "pubDate"))
      start(DATE);
    else if (!strcmp(tag, "link"))
      start(LINK);
    else if (field != NONE && tag[0] == '/')
    {
      raw[rawLen] = '\0';
      if (field == TITLE && !strcmp(tag, "/title"))
        decodeText(raw, item.title, sizeof(item.title));
      else if (field == DATE && !strcmp(tag, "/pubDate"))
        decodeDate(raw, item.date, sizeof(item.date));
      else if (field == LINK && !strcmp(tag, "/link"))
      {
        decodeText(raw, item.link, sizeof(item.link));
        // The panel turns this into an <a href>: only plain https links, never javascript: etc.
        if (strncmp(item.link, "https://", 8) != 0 || strchr(item.link, ' '))
          item.link[0] = '\0';
      }
      field = NONE;
    }
  }

  void feed(char c)
  {
    switch (mode)
    {
    case TEXT:
      if (c == '<')
      {
        mode = TAG;
        tagLen = 0;
        tagClipped = false;
      }
      else
      {
        put(c);
      }
      break;
    case TAG:
      if (c == '>')
      {
        if (!tagClipped)
          tagComplete();
        mode = TEXT;
      }
      else if (tagLen < sizeof(tag) - 1)
      {
        tag[tagLen++] = c;
        // "<![CDATA[" opens raw content that may contain '<' and '>'
        if (tagLen == 8 && !memcmp(tag, "![CDATA[", 8))
        {
          mode = CDATA;
          cdataTail = 0;
        }
      }
      else
      {
        tagClipped = true; // long, uninteresting tag: skip to '>'
      }
      break;
    case CDATA:
      if (c == ']' && cdataTail < 2)
      {
        cdataTail++;
      }
      else if (c == '>' && cdataTail == 2)
      {
        mode = TEXT;
        cdataTail = 0;
      }
      else
      {
        for (; cdataTail; cdataTail--)
          put(']');
        put(c);
      }
      break;
    }
  }
};

// Streams the feed and stops reading after the last wanted item: the whole feed is ~200 kB,
// far more than is worth buffering. Returns the HTTP status (negative on connection failure).
int fetchFeed(RssParser &parser)
{
  WiFiClientSecure client;
  client.setCACert(newsRootsPem);
  HTTPClient https;
  https.setTimeout(TIMEOUT_MS);
  if (!https.begin(client, NEWS_FEED_URL))
    return -1;
  // HTTP/1.0: getStreamPtr() bypasses chunked decoding, so chunk-size lines would corrupt the XML
  https.useHTTP10(true);
  const int status = https.GET();
  if (status != HTTP_CODE_OK)
  {
    https.end();
    return status;
  }
  WiFiClient *stream = https.getStreamPtr();
  const uint32_t deadline = millis() + TIMEOUT_MS + 5000;
  uint8_t buf[256];
  while (!parser.done && (int32_t)(deadline - millis()) > 0)
  {
    if (!stream->available())
    {
      if (!stream->connected())
        break;
      delay(2);
      continue;
    }
    const int n = stream->read(buf, sizeof(buf));
    for (int i = 0; i < n && !parser.done; i++)
      parser.feed((char)buf[i]);
  }
  https.end(); // closing mid-body is fine
  return status;
}
} // namespace

void newsRequestFetch()
{
  state.pending = true;
}

void newsService()
{
  if (!state.pending)
    return;
  const uint32_t start = millis();
  // ~2.5 kB of parser state: on the heap, not the loop task's stack
  RssParser *parser = new RssParser();
  const int status = fetchFeed(*parser);
  state.ok = status == HTTP_CODE_OK && parser->count > 0;
  if (state.ok)
  {
    memcpy(state.items, parser->items, sizeof(state.items));
    state.count = parser->count;
    state.fetchedAt = time(nullptr);
  }
  delete parser;
  state.pending = false;
  Serial.printf("News: HTTP %d, %u items, %lu ms%s\n", status, state.ok ? state.count : 0, millis() - start,
                state.ok ? "" : " - keeping earlier items");
}

const NewsState &newsState()
{
  return state;
}
