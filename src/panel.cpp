#include "panel.h"

#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_heap_caps.h>
#include <esp_random.h>
#include "audio_player.h"
#include "battery.h"
#include "board_pins.h"
#include "build_info.h"
#include "clock.h"
#include "daily_window.h"
#include "history.h"
#include "news.h"
#include "panel_html.h"
#include "panel_usage_poll.h"
#include "settings.h"

namespace
{
  extern const uint8_t echartsStart[] asm("_binary_assets_echarts_echarts_min_js_gz_start");
  extern const uint8_t echartsEnd[] asm("_binary_assets_echarts_echarts_min_js_gz_end");
  WebServer *server = nullptr;
  bool active = false;
  char pinCode[7] = {0};
  // The name handed to MDNS.begin() for this session. A rename only takes effect
  // on restart, so the readouts must report this rather than the saved setting.
  String activeHostname;
  const AccountUsage *usageSource = nullptr;

  // A few concurrent sessions (phone + laptop, say). A login past the limit evicts the slot
  // that has been idle longest.
  struct Session
  {
    uint8_t id[16];
    uint32_t lastSeenMs;
    bool used;
  };
  constexpr int MAX_SESSIONS = 4;
  Session sessions[MAX_SESSIONS] = {};
  uint32_t lastActivityMs = 0;
  bool scanStarted = false;
  uint32_t scanStartAtMs = 0; // non-zero: a requested scan that panelService() hasn't started yet
  uint8_t pendingActions = 0;

  // Simple failed-login throttle. RAM only — exit from panel mode resets it.
  uint8_t loginFails = 0;
  uint32_t loginLockUntilMs = 0;

  Battery battery(PIN_BATTERY_ADC, BATTERY_DIVIDER_RATIO);

  bool hexToBytes(const char *hex, uint8_t *out, size_t n)
  {
    for (size_t i = 0; i < n * 2; i++)
    {
      char c = hex[i];
      uint8_t v;
      if (c >= '0' && c <= '9')
        v = c - '0';
      else if (c >= 'a' && c <= 'f')
        v = c - 'a' + 10;
      else
        return false;
      if (i & 1)
        out[i / 2] |= v;
      else
        out[i / 2] = v << 4;
    }
    return true;
  }

  void sendJson(int code, const JsonDocument &d)
  {
    String out;
    serializeJson(d, out);
    server->send(code, "application/json", out);
  }

  void sendErr(int code, const char *err)
  {
    JsonDocument d;
    d["error"] = err;
    sendJson(code, d);
  }

  // Index of the session slot matching the request's sid cookie, or -1
  int findSession()
  {
    String cookie = server->header("Cookie");
    int at = cookie.indexOf("sid=");
    if (at < 0 || (int)cookie.length() < at + 4 + 32)
    {
      return -1;
    }
    uint8_t want[16];
    if (!hexToBytes(cookie.c_str() + at + 4, want, sizeof(want)))
    {
      return -1;
    }
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
      if (sessions[i].used && memcmp(sessions[i].id, want, sizeof(want)) == 0)
      {
        return i;
      }
    }
    return -1;
  }

  bool requireAuth()
  {
    const int slot = findSession();
    if (slot < 0)
    {
      sendErr(401, "auth");
      return false;
    }
    sessions[slot].lastSeenMs = millis();
    lastActivityMs = millis();
    return true;
  }

  bool readJsonBody(JsonDocument &doc)
  {
    if (server->header("Content-Type").indexOf("application/json") < 0)
    {
      sendErr(400, "json_required");
      return false;
    }
    if (deserializeJson(doc, server->arg("plain")))
    {
      sendErr(400, "bad_json");
      return false;
    }
    return true;
  }

  void throttleFail()
  {
    if (++loginFails >= 5)
    {
      uint32_t backoffS = 60UL << (loginFails - 5);
      if (backoffS > 300)
      {
        backoffS = 300;
      }
      loginLockUntilMs = millis() + backoffS * 1000UL;
    }
  }

  bool throttled(uint32_t &retryS)
  {
    if (loginFails < 5 || (int32_t)(loginLockUntilMs - millis()) <= 0)
    {
      return false;
    }
    retryS = (loginLockUntilMs - millis()) / 1000 + 1;
    return true;
  }

  void handleRoot()
  {
    server->send_P(200, "text/html", PANEL_HTML);
  }

  void handleLogin()
  {
    JsonDocument body;
    if (!readJsonBody(body))
      return;
    uint32_t retryS;
    if (throttled(retryS))
    {
      server->sendHeader("Retry-After", String(retryS));
      JsonDocument d;
      d["error"] = "throttled";
      d["retry_s"] = retryS;
      sendJson(429, d);
      return;
    }
    const char *pin = body["pin"] | "";
    if (strlen(pin) != 6 || strcmp(pin, pinCode) != 0)
    {
      throttleFail();
      sendErr(401, "auth");
      return;
    }
    loginFails = 0;
    int slot = 0;
    for (int i = 0; i < MAX_SESSIONS; i++)
    {
      if (!sessions[i].used)
      {
        slot = i;
        break;
      }
      if (millis() - sessions[i].lastSeenMs > millis() - sessions[slot].lastSeenMs)
      {
        slot = i;
      }
    }
    Session &session = sessions[slot];
    esp_fill_random(session.id, sizeof(session.id));
    session.used = true;
    session.lastSeenMs = millis();
    lastActivityMs = millis();
    char sid[33];
    for (int i = 0; i < 16; i++)
      sprintf(sid + i * 2, "%02x", session.id[i]);
    server->sendHeader("Set-Cookie", String("sid=") + sid + "; Path=/; HttpOnly; SameSite=Strict");
    JsonDocument d;
    d["ok"] = true;
    sendJson(200, d);
  }

  void handleLogout()
  {
    const int slot = findSession();
    if (slot < 0)
    {
      sendErr(401, "auth");
      return;
    }
    sessions[slot].used = false;
    server->sendHeader("Set-Cookie", "sid=; Path=/; HttpOnly; Max-Age=0");
    JsonDocument d;
    d["ok"] = true;
    sendJson(200, d);
  }

  void handleState()
  {
    if (!requireAuth())
      return;
    JsonDocument d;
    d["ip"] = WiFi.localIP().toString();
    d["hostname"] = activeHostname;
    d["hostname_saved"] = settings::hostname();
    d["uptime_s"] = (uint32_t)(millis() / 1000);
    d["fw_version"] = FW_VERSION;
    d["fw_rev"] = FW_GIT_REV;
    // Internal SRAM only: PSRAM is plentiful, internal heap is what TLS and the web server exhaust
    d["heap_free"] = (uint32_t)heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    d["heap_min"] = (uint32_t)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    d["now_epoch"] = (uint32_t)time(nullptr);
    const uint32_t mv = battery.readMillivolts();
    d["battery_mv"] = mv;
    d["battery_pct"] = Battery::percentFromMillivolts(mv);
    d["wifi_rssi"] = WiFi.RSSI();
    d["wifi_ssid"] = settings::wifiSsid();
    d["wifi_mac"] = WiFi.macAddress();
    d["poll_min"] = settings::pollIntervalMinutes();
    d["warn5"] = settings::warningPercent5h();
    d["warn7"] = settings::warningPercent7d();
    d["quiet_start_h"] = settings::quietHoursStart();
    d["quiet_start_m"] = settings::quietMinuteStart();
    d["quiet_end_h"] = settings::quietHoursEnd();
    d["quiet_end_m"] = settings::quietMinuteEnd();
    d["quiet_on"] = settings::quietHoursEnabled();
    d["pause_start_h"] = settings::pauseHoursStart();
    d["pause_start_m"] = settings::pauseMinuteStart();
    d["pause_end_h"] = settings::pauseHoursEnd();
    d["pause_end_m"] = settings::pauseMinuteEnd();
    d["pause_on"] = settings::pauseHoursEnabled();
    const time_t pauseResume = clockValid() ? settings::pauseHoursResumeAt(time(nullptr)) : 0;
    d["pause_active"] = pauseResume != 0;
    d["pause_resume_epoch"] = (uint32_t)pauseResume;
    d["audio_vol"] = settings::audioVolume();
    d["tz"] = settings::timeZone();
    d["tz_name"] = settings::timeZoneName();
    d["rotation"] = settings::displayRotation() * 90;

    const time_t now = time(nullptr);
    JsonArray accounts = d["accounts"].to<JsonArray>();
    for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; i++)
    {
      JsonObject a = accounts.add<JsonObject>();
      a["name"] = settings::accountName(i + 1);
      a["configured"] = !settings::claudeToken(i + 1).isEmpty();
      const AccountUsage *u = usageSource ? &usageSource[i] : nullptr;
      a["has_data"] = u && u->hasData;
      a["h5"] = u && u->hasData ? (int)u->fiveHourPercent : -1;
      a["d7"] = u && u->hasData ? (int)u->sevenDayPercent : -1;
      a["h5_reset"] = u && u->hasData ? (uint32_t)u->fiveHourReset : 0;
      a["d7_reset"] = u && u->hasData ? (uint32_t)u->sevenDayReset : 0;
      const long age = u && u->hasData ? (long)(now - (long)u->fetchedAt) : -1;
      a["age"] = age < 0 ? String("no data") : (age < 60 ? String("just now") : String(age / 60) + "m ago");
    }
    const AccountUsage *first = usageSource;
    d["poll_age_s"] = first && first->hasData ? (long)(now - (long)first->fetchedAt) : -1;
    sendJson(200, d);
  }

  bool readWindowSettings(JsonDocument &body, const char *const keys[4], const char *onKey,
                          int (&times)[4], bool &hasTimes)
  {
    hasTimes = false;
    for (int i = 0; i < 4; i++)
    {
      const JsonVariant value = body[keys[i]];
      if (value.isUnbound())
        continue;
      if (!value.is<int>())
        return false;
      times[i] = value.as<int>();
      hasTimes = true;
    }
    const JsonVariant enabled = body[onKey];
    if (!enabled.isUnbound() && !enabled.is<bool>())
      return false;
    // Validate the complete resulting window, including omitted saved values.
    return !(hasTimes || (enabled.is<bool>() && enabled.as<bool>())) ||
           dailyWindowValid(times[0], times[1], times[2], times[3]);
  }

  void handleSettings()
  {
    if (!requireAuth())
      return;
    JsonDocument body;
    if (!readJsonBody(body))
      return;
    // Validate everything that can be rejected before applying any of it
    const char *const quietKeys[] = {"quiet_start_h", "quiet_start_m", "quiet_end_h", "quiet_end_m"};
    int quietTimes[] = {settings::quietHoursStart(), settings::quietMinuteStart(),
                        settings::quietHoursEnd(), settings::quietMinuteEnd()};
    bool hasQuietTimes;
    if (!readWindowSettings(body, quietKeys, "quiet_on", quietTimes, hasQuietTimes))
    {
      sendErr(400, "bad_quiet_hours");
      return;
    }
    const char *const pauseKeys[] = {"pause_start_h", "pause_start_m", "pause_end_h", "pause_end_m"};
    int pauseTimes[] = {settings::pauseHoursStart(), settings::pauseMinuteStart(),
                        settings::pauseHoursEnd(), settings::pauseMinuteEnd()};
    bool hasPauseTimes;
    if (!readWindowSettings(body, pauseKeys, "pause_on", pauseTimes, hasPauseTimes))
    {
      sendErr(400, "bad_pause_hours");
      return;
    }
    const bool hasRotation = !body["rotation"].isNull();
    const int rotation = body["rotation"] | -1;
    if (hasRotation && (rotation < 0 || rotation > 270 || rotation % 90 != 0))
    {
      sendErr(400, "bad_rotation");
      return;
    }
    if (!body["hostname"].isNull())
    {
      // `| ""`, not as<String>(): a missing key would become the string "null"
      const String name = body["hostname"] | "";
      if (!settings::setHostname(name))
      {
        sendErr(400, "bad_hostname");
        return;
      }
    }
    if (!body["tz"].isNull())
    {
      // `| ""`, not as<String>(): a missing key would become the string "null"
      const String tz = body["tz"] | "";
      const String tzName = body["tz_name"] | "";
      if (tz != settings::timeZone() || tzName != settings::timeZoneName())
      {
        if (!settings::setTimeZone(tz, tzName))
        {
          sendErr(400, "bad_time_zone");
          return;
        }
        pendingActions |= PANEL_ACT_TIME_ZONE;
      }
    }
    const bool pollingChanged = body["poll_min"].is<int>() || hasPauseTimes ||
                                body["pause_on"].is<bool>() || (pendingActions & PANEL_ACT_TIME_ZONE);
    // Stop an old schedule snapshot before saving a new pause window.
    // The loop collects the running request without blocking this handler.
    if (pollingChanged)
      cancelPanelUsagePoll();
    if (hasRotation && rotation / 90 != settings::displayRotation())
    {
      settings::setDisplayRotation(rotation / 90);
      pendingActions |= PANEL_ACT_ROTATION;
    }
    if (body["poll_min"].is<int>())
      settings::setPollIntervalMinutes(body["poll_min"].as<int>());
    if (body["warn5"].is<int>())
      settings::setWarningPercent5h(body["warn5"].as<int>());
    if (body["warn7"].is<int>())
      settings::setWarningPercent7d(body["warn7"].as<int>());
    if (hasQuietTimes)
      settings::setQuietHours(quietTimes[0], quietTimes[1], quietTimes[2], quietTimes[3]);
    if (!body["quiet_on"].isNull())
      settings::setQuietHoursEnabled(body["quiet_on"].as<bool>());
    if (hasPauseTimes)
      settings::setPauseHours(pauseTimes[0], pauseTimes[1], pauseTimes[2], pauseTimes[3]);
    if (!body["pause_on"].isNull())
      settings::setPauseHoursEnabled(body["pause_on"].as<bool>());
    if (body["audio_vol"].is<int>())
      settings::setAudioVolume(body["audio_vol"].as<int>());
    if (pollingChanged)
      pendingActions |= PANEL_ACT_SETTINGS_SAVED;
    JsonDocument d;
    d["ok"] = true;
    sendJson(200, d);
  }

  void handleTokens()
  {
    if (!requireAuth())
      return;
    JsonDocument body;
    if (!readJsonBody(body))
      return;
    const String t1 = body["token1"].as<String>();
    const String t2 = body["token2"].as<String>();
    const String n1 = body["name1"].as<String>();
    const String n2 = body["name2"].as<String>();
    const bool tokensChanged = !t1.isEmpty() || !t2.isEmpty();
    if (tokensChanged)
      cancelPanelUsagePoll();
    if (!t1.isEmpty())
      settings::setClaudeToken(1, t1);
    if (!t2.isEmpty())
      settings::setClaudeToken(2, t2);
    settings::setAccountName(1, n1);
    settings::setAccountName(2, n2);
    if (tokensChanged)
      pendingActions |= PANEL_ACT_SETTINGS_SAVED;
    JsonDocument d;
    d["ok"] = true;
    // Probe each newly saved token so the user gets a verdict now, not at the next poll.
    // Blocks ~2-3 s per token; Wi-Fi is already up in panel mode.
    JsonArray probes = d["probes"].to<JsonArray>();
    const String *saved[] = {&t1, &t2};
    for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; i++)
    {
      if (saved[i]->isEmpty())
        continue;
      const ClaudeUsage u = probeToken(*saved[i]);
      JsonObject p = probes.add<JsonObject>();
      p["account"] = i + 1;
      p["ok"] = u.valid;
      p["http"] = u.httpStatus;
      if (u.valid)
      {
        p["h5"] = (int)constrain(u.fiveHourPercent, 0.0f, 100.0f);
        p["d7"] = (int)constrain(u.sevenDayPercent, 0.0f, 100.0f);
      }
    }
    sendJson(200, d);
  }

  void handleWifi()
  {
    if (!requireAuth())
      return;
    JsonDocument body;
    if (!readJsonBody(body))
      return;
    const String ssid = body["ssid"].as<String>();
    const String pass = body["pass"].as<String>();
    if (!ssid.isEmpty())
      settings::setWifiSsid(ssid);
    if (!pass.isEmpty())
      settings::setWifiPassword(pass);
    JsonDocument d;
    d["ok"] = true;
    sendJson(200, d);
  }

  void handleRefresh()
  {
    if (!requireAuth())
      return;
    pendingActions |= PANEL_ACT_REFRESH;
    JsonDocument d;
    d["ok"] = true;
    sendJson(200, d);
  }

  // Allowed test-play WAVs — kept in a whitelist so a crafted JSON body can't
  // probe arbitrary paths on LittleFS.
  const char *const ALLOWED_SOUNDS[] = {
      "5h-warning.wav",
      "5h-depleted.wav",
      "5h-reset.wav",
      "7d-warning.wav",
      "7d-depleted.wav",
      "7d-reset.wav",
  };
  constexpr size_t ALLOWED_SOUND_COUNT = sizeof(ALLOWED_SOUNDS) / sizeof(ALLOWED_SOUNDS[0]);

  void handleSoundsPlay()
  {
    if (!requireAuth())
      return;
    JsonDocument body;
    if (!readJsonBody(body))
      return;
    const char *file = body["file"] | "";
    bool allowed = false;
    for (size_t i = 0; i < ALLOWED_SOUND_COUNT; i++)
    {
      if (strcmp(file, ALLOWED_SOUNDS[i]) == 0)
      {
        allowed = true;
        break;
      }
    }
    if (!allowed)
    {
      sendErr(400, "unknown_sound");
      return;
    }
    const String path = String("/") + file;
    const bool ok = playWav(path.c_str());
    JsonDocument d;
    d["ok"] = ok;
    if (!ok)
      d["error"] = "play_failed";
    sendJson(ok ? 200 : 500, d);
  }

  void handleHistory()
  {
    if (!requireAuth())
      return;
    constexpr int COLS = HIST_SLOTS; // 336 half-hour columns, the device's full resolution
    HistSlot buf[HIST_SLOTS];
    JsonDocument d;
    d["cols"] = COLS;
    d["col_seconds"] = HIST_SLOT_SEC;
    JsonArray accounts = d["accounts"].to<JsonArray>();
    uint32_t newestAny = 0;
    for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; i++)
    {
      uint32_t newest = 0;
      historySnapshot(i, buf, newest);
      if (newest > newestAny)
      {
        newestAny = newest;
      }
      JsonObject a = accounts.add<JsonObject>();
      a["name"] = settings::accountName(i + 1);
      JsonArray h5 = a["h5"].to<JsonArray>();
      JsonArray d7 = a["d7"].to<JsonArray>();
      for (int c = 0; c < COLS; c++)
      {
        if (buf[c].h5 == HIST_EMPTY)
          h5.add(nullptr);
        else
          h5.add(buf[c].h5);
        if (buf[c].d7 == HIST_EMPTY)
          d7.add(nullptr);
        else
          d7.add(buf[c].d7);
      }
    }
    d["newest_epoch"] = newestAny;
    sendJson(200, d);
  }

  // Raw 30-min samples as CSV: timestamp (slot start, epoch seconds), then 5h and 7d % per
  // account. Rows with no sample for any account (device off) are left out; a missing value for
  // one account is an empty cell.
  void handleHistoryCsv()
  {
    if (!requireAuth())
      return;
    static_assert(settings::CLAUDE_TOKEN_COUNT == 2, "CSV columns are acct1 and acct2");
    HistSlot buf[settings::CLAUDE_TOKEN_COUNT][HIST_SLOTS];
    uint32_t newest = 0;
    for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; i++)
    {
      historySnapshot(i, buf[i], newest);
    }
    String csv;
    csv.reserve(40 + HIST_SLOTS * 32);
    csv += "timestamp,acct1-5h,acct1-7d,acct2-5h,acct2-7d\r\n";
    uint32_t from = 0, till = 0;
    for (int slot = 0; slot < HIST_SLOTS; slot++)
    {
      bool any = false;
      for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; i++)
      {
        any |= buf[i][slot].h5 != HIST_EMPTY || buf[i][slot].d7 != HIST_EMPTY;
      }
      if (!any)
        continue;
      const uint32_t start = newest - (HIST_SLOTS - slot) * HIST_SLOT_SEC;
      if (from == 0)
        from = start;
      till = start;
      csv += String(start);
      for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; i++)
      {
        for (const uint8_t v : {buf[i][slot].h5, buf[i][slot].d7})
        {
          csv += ',';
          if (v != HIST_EMPTY)
            csv += String(v);
        }
      }
      csv += "\r\n";
    }
    if (from == 0)
    {
      sendErr(404, "no_history");
      return;
    }
    server->sendHeader("Content-Disposition",
                       String("attachment; filename=\"claude-meter-") + from + "-" + till + ".csv\"");
    server->sendHeader("Cache-Control", "no-store");
    server->send(200, "text/csv; charset=utf-8", csv);
  }

  void handleHistoryClear()
  {
    if (!requireAuth())
      return;
    historyErase();
    JsonDocument d;
    d["ok"] = true;
    sendJson(200, d);
  }

  // Async scan: ?start=1 queues one and returns 202 at once; the client then polls without it,
  // getting 202 until the results are in. The scan itself starts from panelService() a moment
  // later, because while it runs the radio is off-channel and a reply sent then is lost until TCP
  // retransmits it (~7 s).
  void handleWifiScan()
  {
    if (!requireAuth())
      return;
    if (server->hasArg("start"))
    {
      WiFi.scanDelete();
      scanStarted = true;
      scanStartAtMs = millis() + 150;
    }
    const int count = WiFi.scanComplete();
    if (scanStartAtMs != 0 || count == WIFI_SCAN_RUNNING)
    {
      JsonDocument d;
      d["scanning"] = true;
      sendJson(202, d);
      return;
    }
    if (count < 0)
    {
      // scanComplete() reports "never started" and "failed" the same way
      sendErr(scanStarted ? 500 : 409, scanStarted ? "scan_failed" : "no_scan");
      scanStarted = false;
      return;
    }
    scanStarted = false;
    const String savedSsid = settings::wifiSsid();
    JsonDocument d;
    JsonArray arr = d["networks"].to<JsonArray>();
    for (int i = 0; i < count; i++)
    {
      JsonObject n = arr.add<JsonObject>();
      n["ssid"] = WiFi.SSID(i);
      n["rssi"] = WiFi.RSSI(i);
      n["channel"] = WiFi.channel(i);
      n["secure"] = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
      n["saved"] = WiFi.SSID(i) == savedSsid;
    }
    WiFi.scanDelete();
    sendJson(200, d);
  }

  void handleNews()
  {
    if (!requireAuth())
      return;
    const NewsState &news = newsState();
    JsonDocument d;
    d["ok"] = news.ok;
    d["fetching"] = news.pending;
    d["fetched_epoch"] = news.fetchedAt;
    d["source"] = NEWS_FEED_URL;
    JsonArray items = d["items"].to<JsonArray>();
    for (int i = 0; i < news.count; i++)
    {
      JsonObject o = items.add<JsonObject>();
      o["title"] = news.items[i].title;
      o["date"] = news.items[i].date;
      o["link"] = news.items[i].link;
    }
    sendJson(200, d);
  }

  void handleFactoryReset()
  {
    if (!requireAuth())
      return;
    // Confirmation guard: body must contain {"confirm":"wipe"} so a stray POST
    // can't erase everything.
    JsonDocument body;
    if (!readJsonBody(body))
      return;
    if (strcmp(body["confirm"] | "", "wipe") != 0)
    {
      sendErr(400, "confirm_required");
      return;
    }
    Preferences prefs;
    prefs.begin("meter", false);
    prefs.clear();
    prefs.end();
    prefs.begin("alerts", false);
    prefs.clear();
    prefs.end();
    historyErase();
    pendingActions |= PANEL_ACT_REBOOT;
    JsonDocument d;
    d["ok"] = true;
    d["rebooting_in_ms"] = 1000;
    sendJson(200, d);
  }

  void handleReboot()
  {
    if (!requireAuth())
      return;
    pendingActions |= PANEL_ACT_REBOOT;
    JsonDocument d;
    d["ok"] = true;
    d["rebooting_in_ms"] = 1000;
    sendJson(200, d);
  }

  void handleNotFound()
  {
    server->send(404, "text/plain", "not found");
  }

  void handleChartAsset()
  {
    // Static library only; usage/history endpoints retain their session checks.
    server->sendHeader("Content-Encoding", "gzip");
    server->sendHeader("Cache-Control", "public, max-age=31536000, immutable");
    server->sendHeader("X-Content-Type-Options", "nosniff");
    server->send_P(200, "application/javascript", reinterpret_cast<const char *>(echartsStart), echartsEnd - echartsStart);
  }
} // namespace

void panelBegin(PanelDisplay &out)
{
  if (active)
  {
    return;
  }
  const uint32_t r = esp_random();
  snprintf(pinCode, sizeof(pinCode), "%06u", (unsigned)(r % 1000000u));
  memset(sessions, 0, sizeof(sessions));
  loginFails = 0;
  loginLockUntilMs = 0;
  pendingActions = 0;
  lastActivityMs = millis();

  server = new WebServer(80);
  const char *trackedHeaders[] = {"Cookie", "Content-Type"};
  server->collectHeaders(trackedHeaders, sizeof(trackedHeaders) / sizeof(trackedHeaders[0]));
  server->on("/", HTTP_GET, handleRoot);
  server->on("/assets/echarts-6.1.0-v2.js", HTTP_GET, handleChartAsset);
  server->on("/api/login", HTTP_POST, handleLogin);
  server->on("/api/logout", HTTP_POST, handleLogout);
  server->on("/api/state", HTTP_GET, handleState);
  server->on("/api/settings", HTTP_POST, handleSettings);
  server->on("/api/tokens", HTTP_POST, handleTokens);
  server->on("/api/wifi", HTTP_POST, handleWifi);
  server->on("/api/refresh", HTTP_POST, handleRefresh);
  server->on("/api/history", HTTP_GET, handleHistory);
  server->on("/api/history/clear", HTTP_POST, handleHistoryClear);
  server->on("/api/history.csv", HTTP_GET, handleHistoryCsv);
  server->on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server->on("/api/sounds/play", HTTP_POST, handleSoundsPlay);
  server->on("/api/news", HTTP_GET, handleNews);
  server->on("/api/factory-reset", HTTP_POST, handleFactoryReset);
  server->on("/api/reboot", HTTP_POST, handleReboot);
  server->onNotFound(handleNotFound);
  server->begin();
  active = true;

  out.ip = WiFi.localIP().toString();
  activeHostname = settings::hostname();
  out.hostname = activeHostname;
  out.pin = String(pinCode);
}

void panelEnd()
{
  if (!active)
  {
    return;
  }
  server->close();
  delete server;
  server = nullptr;
  active = false;
  memset(sessions, 0, sizeof(sessions));
  memset(pinCode, 0, sizeof(pinCode));
}

bool panelActive()
{
  return active;
}

void panelService()
{
  if (!active)
  {
    return;
  }
  server->handleClient();
  if (scanStartAtMs != 0 && (int32_t)(millis() - scanStartAtMs) >= 0)
  {
    scanStartAtMs = 0;
    // A failure leaves scanComplete() negative, which the next poll reports as scan_failed
    WiFi.scanNetworks(true);
  }
}

uint32_t panelLastActivityMs()
{
  return lastActivityMs;
}

uint8_t panelTakeAction()
{
  const uint8_t a = pendingActions;
  pendingActions = 0;
  return a;
}

void panelSetUsageSource(const AccountUsage *accounts)
{
  usageSource = accounts;
}
