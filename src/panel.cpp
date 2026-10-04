#include "panel.h"

#include <WebServer.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <esp_random.h>
#include "audio_player.h"
#include "battery.h"
#include "board_pins.h"
#include "history.h"
#include "panel_html.h"
#include "settings.h"

namespace
{
WebServer *server = nullptr;
bool active = false;
char pinCode[7] = {0};
const AccountUsage *usageSource = nullptr;

// Single session slot. The panel is one user at a time by design.
struct Session
{
  uint8_t id[16];
  uint32_t lastSeenMs;
  bool used;
};
Session session = {};
uint32_t lastActivityMs = 0;
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

bool sessionValid()
{
  String cookie = server->header("Cookie");
  int at = cookie.indexOf("sid=");
  if (at < 0 || (int)cookie.length() < at + 4 + 32)
  {
    return false;
  }
  uint8_t want[16];
  if (!hexToBytes(cookie.c_str() + at + 4, want, sizeof(want)))
  {
    return false;
  }
  if (!session.used || memcmp(session.id, want, sizeof(want)) != 0)
  {
    return false;
  }
  session.lastSeenMs = millis();
  lastActivityMs = millis();
  return true;
}

bool requireAuth()
{
  if (sessionValid())
  {
    return true;
  }
  sendErr(401, "auth");
  return false;
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
  if (!requireAuth())
    return;
  session.used = false;
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
  d["hostname"] = "claude-meter";
  d["uptime_s"] = (uint32_t)(millis() / 1000);
  d["now_epoch"] = (uint32_t)time(nullptr);
  const uint32_t mv = battery.readMillivolts();
  d["battery_mv"] = mv;
  d["battery_pct"] = Battery::percentFromMillivolts(mv);
  d["wifi_rssi"] = WiFi.RSSI();
  d["wifi_ssid"] = settings::wifiSsid();
  d["poll_min"] = settings::pollIntervalMinutes();
  d["warn5"] = settings::warningPercent5h();
  d["warn7"] = settings::warningPercent7d();
  d["quiet_start_h"] = settings::quietHoursStart();
  d["quiet_start_m"] = settings::quietMinuteStart();
  d["quiet_end_h"] = settings::quietHoursEnd();
  d["quiet_end_m"] = settings::quietMinuteEnd();
  d["quiet_on"] = settings::quietHoursEnabled();
  d["audio_vol"] = settings::audioVolume();

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
    a["age"] = age < 0 ? String("no data") : (age < 60 ? String("just now") : String(age / 60) + " m ago");
  }
  const AccountUsage *first = usageSource;
  d["poll_age_s"] = first && first->hasData ? (long)(now - (long)first->fetchedAt) : -1;
  sendJson(200, d);
}

void handleSettings()
{
  if (!requireAuth())
    return;
  JsonDocument body;
  if (!readJsonBody(body))
    return;
  if (body["poll_min"].is<int>())
    settings::setPollIntervalMinutes(body["poll_min"].as<int>());
  if (body["warn5"].is<int>())
    settings::setWarningPercent5h(body["warn5"].as<int>());
  if (body["warn7"].is<int>())
    settings::setWarningPercent7d(body["warn7"].as<int>());
  if (body["quiet_start_h"].is<int>() && body["quiet_end_h"].is<int>())
  {
    const int sh = body["quiet_start_h"].as<int>();
    const int sm = body["quiet_start_m"].is<int>() ? body["quiet_start_m"].as<int>() : 0;
    const int eh = body["quiet_end_h"].as<int>();
    const int em = body["quiet_end_m"].is<int>() ? body["quiet_end_m"].as<int>() : 0;
    settings::setQuietHours(sh, sm, eh, em);
  }
  if (!body["quiet_on"].isNull())
    settings::setQuietHoursEnabled(body["quiet_on"].as<bool>());
  if (body["audio_vol"].is<int>())
    settings::setAudioVolume(body["audio_vol"].as<int>());
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
  if (!t1.isEmpty())
    settings::setClaudeToken(1, t1);
  if (!t2.isEmpty())
    settings::setClaudeToken(2, t2);
  settings::setAccountName(1, n1);
  settings::setAccountName(2, n2);
  pendingActions |= PANEL_ACT_SETTINGS_SAVED;
  JsonDocument d;
  d["ok"] = true;
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
    "5h-warning.wav", "5h-depleted.wav", "5h-reset.wav",
    "7d-warning.wav", "7d-depleted.wav", "7d-reset.wav",
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
  constexpr int COLS = HIST_SLOTS / 2; // 168 one-hour columns
  HistSlot buf[HIST_SLOTS];
  JsonDocument d;
  d["cols"] = COLS;
  d["col_seconds"] = 3600;
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
      // Max of the two 30-min samples per hour-column, matching the ePaper chart
      const HistSlot &aSlot = buf[c * 2];
      const HistSlot &bSlot = buf[c * 2 + 1];
      const auto pickMax = [](uint8_t x, uint8_t y) -> int {
        if (x == HIST_EMPTY && y == HIST_EMPTY)
          return -1;
        if (x == HIST_EMPTY)
          return (int)y;
        if (y == HIST_EMPTY)
          return (int)x;
        return (int)(x > y ? x : y);
      };
      const int v5 = pickMax(aSlot.h5, bSlot.h5);
      const int v7 = pickMax(aSlot.d7, bSlot.d7);
      if (v5 < 0)
        h5.add(nullptr);
      else
        h5.add(v5);
      if (v7 < 0)
        d7.add(nullptr);
      else
        d7.add(v7);
    }
  }
  d["newest_epoch"] = newestAny;
  sendJson(200, d);
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

void handleWifiScan()
{
  if (!requireAuth())
    return;
  const String savedSsid = settings::wifiSsid();
  const int count = WiFi.scanNetworks(false, false, false, 300);
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
} // namespace

void panelBegin(PanelDisplay &out)
{
  if (active)
  {
    return;
  }
  const uint32_t r = esp_random();
  snprintf(pinCode, sizeof(pinCode), "%06u", (unsigned)(r % 1000000u));
  session.used = false;
  loginFails = 0;
  loginLockUntilMs = 0;
  pendingActions = 0;
  lastActivityMs = millis();

  server = new WebServer(80);
  const char *trackedHeaders[] = {"Cookie", "Content-Type"};
  server->collectHeaders(trackedHeaders, sizeof(trackedHeaders) / sizeof(trackedHeaders[0]));
  server->on("/", HTTP_GET, handleRoot);
  server->on("/api/login", HTTP_POST, handleLogin);
  server->on("/api/logout", HTTP_POST, handleLogout);
  server->on("/api/state", HTTP_GET, handleState);
  server->on("/api/settings", HTTP_POST, handleSettings);
  server->on("/api/tokens", HTTP_POST, handleTokens);
  server->on("/api/wifi", HTTP_POST, handleWifi);
  server->on("/api/refresh", HTTP_POST, handleRefresh);
  server->on("/api/history", HTTP_GET, handleHistory);
  server->on("/api/history/clear", HTTP_POST, handleHistoryClear);
  server->on("/api/wifi/scan", HTTP_GET, handleWifiScan);
  server->on("/api/sounds/play", HTTP_POST, handleSoundsPlay);
  server->on("/api/factory-reset", HTTP_POST, handleFactoryReset);
  server->on("/api/reboot", HTTP_POST, handleReboot);
  server->onNotFound(handleNotFound);
  server->begin();
  active = true;

  out.ip = WiFi.localIP().toString();
  out.hostname = "claude-meter";
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
  session.used = false;
  memset(pinCode, 0, sizeof(pinCode));
}

bool panelActive()
{
  return active;
}

void panelService()
{
  if (active)
  {
    server->handleClient();
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
