#include "usage_poll.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "claude_usage.h"
#include "clock.h"

// Trusted roots for api.anthropic.com (assets/certs/anthropic_roots.pem), embedded null-terminated
// via board_build.embed_txtfiles
extern const char rootCaPem[] asm("_binary_assets_certs_anthropic_roots_pem_start");

namespace
{
constexpr uint32_t WIFI_TIMEOUT_MS = 15000;

bool connectWifi()
{
  const String ssid = settings::wifiSsid();
  if (ssid.isEmpty())
  {
    Serial.println("Wi-Fi not set - send: ssid <name> and pass <password>");
    return false;
  }
  Serial.printf("Connecting to Wi-Fi \"%s\"...\n", ssid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), settings::wifiPassword().c_str());
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    if (millis() - start > WIFI_TIMEOUT_MS)
    {
      Serial.printf("Wi-Fi connect failed (status %d)\n", WiFi.status());
      return false;
    }
    delay(100);
  }
  Serial.printf("Wi-Fi connected: %s, RSSI %d dBm, %lu ms\n", WiFi.localIP().toString().c_str(), WiFi.RSSI(), millis() - start);
  return true;
}

void wifiOff()
{
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void printWindow(const char *name, float percent, uint32_t reset)
{
  const time_t resetTime = reset;
  struct tm local;
  localtime_r(&resetTime, &local);
  char when[24];
  strftime(when, sizeof(when), "%Y-%m-%d %H:%M", &local);
  Serial.printf("  %s  %5.1f %%   resets %s", name, percent, when);
  const long seconds = (long)reset - (long)time(nullptr);
  if (clockValid() && seconds > 0)
  {
    Serial.printf(" (in %ldd %ldh %ldm)", seconds / 86400, seconds % 86400 / 3600, seconds % 3600 / 60);
  }
  Serial.println();
}
}

PollReport pollUsage(AccountUsage accounts[settings::CLAUDE_TOKEN_COUNT], Pcf85063 &rtc, bool syncClock)
{
  PollReport report = {};
  String tokens[settings::CLAUDE_TOKEN_COUNT];
  bool anyToken = false;
  for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; i++)
  {
    tokens[i] = settings::claudeToken(i + 1);
    anyToken |= !tokens[i].isEmpty();
  }
  report.configured = anyToken && !settings::wifiSsid().isEmpty();
  if (!anyToken)
  {
    Serial.println("No Claude token set - send: token1 <value from `claude setup-token`>");
  }
  if (!report.configured)
  {
    return report;
  }

  report.wifiConnected = connectWifi();
  report.wifiStatus = WiFi.status();
  if (!report.wifiConnected)
  {
    wifiOff();
    return report;
  }
  report.wifiRssi = WiFi.RSSI();
  // TLS certificate checks need a correct clock
  if (syncClock || !clockValid())
  {
    report.clockSynced = clockSyncNtp(rtc);
    Serial.println(report.clockSynced ? "Clock synced over NTP" : "NTP sync failed");
  }

  for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; i++)
  {
    if (tokens[i].isEmpty())
    {
      continue;
    }
    const uint32_t start = millis();
    const ClaudeUsage usage = fetchClaudeUsage(tokens[i].c_str(), rootCaPem);
    AccountUsage &account = accounts[i];
    account.lastStatus = usage.httpStatus;
    account.lastPollFailed = !usage.valid;
    if (usage.valid)
    {
      account.hasData = true;
      // Utilisation goes past 1.0 once an account is over its limit; show it as 100%
      account.fiveHourPercent = constrain(usage.fiveHourPercent, 0.0f, 100.0f);
      account.sevenDayPercent = constrain(usage.sevenDayPercent, 0.0f, 100.0f);
      account.fiveHourReset = usage.fiveHourReset;
      account.sevenDayReset = usage.sevenDayReset;
      account.fetchedAt = time(nullptr);
      report.accountsOk++;
    }
    else
    {
      report.accountsFailed++;
    }
    Serial.printf("%s: HTTP %d, %lu ms%s\n", settings::accountName(i + 1).c_str(), usage.httpStatus, millis() - start,
                  usage.valid ? "" : " - no usage headers");
    if (usage.errorBody.length())
    {
      Serial.printf("  Response: %s\n", usage.errorBody.c_str());
    }
  }

  wifiOff();
  return report;
}

void printUsage(const AccountUsage accounts[settings::CLAUDE_TOKEN_COUNT])
{
  for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; i++)
  {
    const AccountUsage &account = accounts[i];
    Serial.printf("%s (token %d):\n", settings::accountName(i + 1).c_str(), i + 1);
    if (settings::claudeToken(i + 1).isEmpty())
    {
      Serial.printf("  not set - send: token%d <value>\n", i + 1);
      continue;
    }
    if (!account.hasData)
    {
      Serial.printf("  no data yet%s\n", account.lastPollFailed ? " (last poll failed)" : "");
      continue;
    }
    printWindow("5-hour", account.fiveHourPercent, account.fiveHourReset);
    printWindow("7-day ", account.sevenDayPercent, account.sevenDayReset);
    if (account.lastPollFailed)
    {
      Serial.printf("  last poll failed (HTTP %d), showing earlier data\n", account.lastStatus);
    }
  }
}

void runWifiScan()
{
  const String saved = settings::wifiSsid();
  WiFi.mode(WIFI_STA);
  const int count = WiFi.scanNetworks();
  if (count < 0)
  {
    Serial.println("Wi-Fi scan failed");
  }
  bool found = false;
  for (int i = 0; i < count; i++)
  {
    const bool isSaved = WiFi.SSID(i) == saved;
    found |= isSaved;
    Serial.printf("  %-32s %4d dBm  ch %2d  %s%s\n", WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i),
                  WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "open" : "secured", isSaved ? "  <- saved" : "");
  }
  Serial.printf("%d networks; saved SSID \"%s\" %s\n", count < 0 ? 0 : count, saved.c_str(), found ? "in range" : "NOT found");
  WiFi.scanDelete();
  wifiOff();
}

void runTlsCheck()
{
  if (!connectWifi())
  {
    wifiOff();
    return;
  }

  WiFiClientSecure client;
  // Safe only because nothing is sent: we just want to see who signed the chain
  client.setInsecure();
  if (!client.connect("api.anthropic.com", 443))
  {
    Serial.println("TLS connect to api.anthropic.com:443 failed");
  }
  else
  {
    Serial.println("Certificate chain presented by api.anthropic.com:");
    int depth = 0;
    for (const mbedtls_x509_crt *cert = client.getPeerCertificate(); cert && cert->raw.len; cert = cert->next, depth++)
    {
      char subject[160];
      char issuer[160];
      mbedtls_x509_dn_gets(subject, sizeof(subject), &cert->subject);
      mbedtls_x509_dn_gets(issuer, sizeof(issuer), &cert->issuer);
      Serial.printf("  [%d] subject: %s\n      issuer:  %s\n      valid:   %04d-%02d-%02d to %04d-%02d-%02d\n", depth, subject, issuer,
                    cert->valid_from.year, cert->valid_from.mon, cert->valid_from.day,
                    cert->valid_to.year, cert->valid_to.mon, cert->valid_to.day);
    }
    client.stop();
  }
  wifiOff();
}
