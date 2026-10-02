#include <Arduino.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_sleep.h>
#include <lvgl.h>
#include <soc/usb_serial_jtag_struct.h>
#include "alerts.h"
#include "audio_player.h"
#include "battery.h"
#include "board_pins.h"
#include "clock.h"
#include "epaper.h"
#include "lvgl_port.h"
#include "meter_ui.h"
#include "pcf85063.h"
#include "settings.h"
#include "usage_poll.h"

// How the meter runs:
// - Debug mode off (default on battery): no serial, LED off. Wake, poll every account, draw,
//   deep sleep until the next poll. A BOOT press wakes it to show the next view and sleeps again;
//   PWR wakes it and switches it off.
// - Debug mode on (USB host attached at cold boot): serial commands, LED lit while awake. Stays
//   awake and polls on the same schedule; "sleep" starts the sleep cycle with debug mode kept on.
// - A long BOOT press (>= 1 s) toggles debug mode, awake or asleep.

constexpr uint32_t LONG_PRESS_MS = 1000;
// Shorter presses are treated as contact bounce
constexpr uint32_t BUTTON_MIN_PRESS_MS = 30;
constexpr uint32_t USB_DETECT_MS = 5;
constexpr uint32_t MIN_SLEEP_S = 1;
// Re-sync the RTC over NTP this often
constexpr time_t NTP_RESYNC_S = 6 * 3600;
// A full (flashing) refresh after this many partial ones clears e-paper ghosting
constexpr uint16_t FULL_REFRESH_EVERY = 60;
constexpr uint8_t SHTC3_ADDRESS = 0x70;
constexpr uint16_t SHTC3_CMD_SLEEP = 0xB098;

// RTC memory survives deep sleep
RTC_DATA_ATTR uint8_t savedFrame[Epaper::FRAME_BYTES]; // image left on the panel, for partial refresh on wake
RTC_DATA_ATTR AccountUsage usage[settings::CLAUDE_TOKEN_COUNT];
RTC_DATA_ATTR MeterView view = MeterView::Dual;
RTC_DATA_ATTR time_t nextPollAt = 0;
RTC_DATA_ATTR time_t lastNtpSync = 0;
RTC_DATA_ATTR uint16_t partialRefreshes = 0;
RTC_DATA_ATTR uint32_t wakeCount = 0;
RTC_DATA_ATTR bool debugMode = false;
// Result of the last poll's Wi-Fi connection, for the status bar icon and the failure popup
RTC_DATA_ATTR WifiState wifiState = WifiState::Unknown;
RTC_DATA_ATTR int8_t wifiRssi = 0;
RTC_DATA_ATTR uint8_t wifiFailStatus = 0;

Pcf85063 rtc;
Battery battery(PIN_BATTERY_ADC, BATTERY_DIVIDER_RATIO);
Epaper epaper({PIN_EPD_SCK, PIN_EPD_MOSI, PIN_EPD_CS, PIN_EPD_DC, PIN_EPD_RST, PIN_EPD_BUSY});

// The PWR button press that switched the board on must be released before a long press can switch it off
bool powerOffArmed = false;
bool pwrButtonWasDown = false;
uint32_t pwrButtonDownAt = 0;

bool accountConfigured(int index)
{
  return !settings::claudeToken(index + 1).isEmpty();
}

// Views cycle dual -> account 1 -> account 2 when both accounts have tokens; otherwise only the
// configured account's view is shown
MeterView firstView()
{
  if (accountConfigured(0) && accountConfigured(1))
  {
    return MeterView::Dual;
  }
  return accountConfigured(1) ? MeterView::Account2 : MeterView::Account1;
}

bool viewAvailable(MeterView candidate)
{
  switch (candidate)
  {
  case MeterView::Dual:
    return accountConfigured(0) && accountConfigured(1);
  case MeterView::Account1:
    return accountConfigured(0);
  case MeterView::Account2:
    return accountConfigured(1);
  }
  return false;
}

MeterView nextView(MeterView current)
{
  MeterView candidate = current;
  for (int i = 0; i < 3; i++)
  {
    candidate = candidate == MeterView::Dual ? MeterView::Account1 : candidate == MeterView::Account1 ? MeterView::Account2 : MeterView::Dual;
    if (viewAvailable(candidate))
    {
      return candidate;
    }
  }
  return firstView();
}

const char *viewName(MeterView value)
{
  return value == MeterView::Dual ? "dual" : value == MeterView::Account1 ? "account 1" : "account 2";
}

// A USB host sends a start-of-frame every millisecond, whether or not a terminal has the port open
bool usbHostConnected()
{
  const uint32_t frame = USB_SERIAL_JTAG.fram_num.sof_frame_index;
  delay(USB_DETECT_MS);
  return USB_SERIAL_JTAG.fram_num.sof_frame_index != frame;
}

// Pushes the LVGL screen to the panel, with a full refresh every FULL_REFRESH_EVERY updates
void refreshPanel()
{
  if (++partialRefreshes >= FULL_REFRESH_EVERY)
  {
    partialRefreshes = 0;
    epaper.begin();
    lv_obj_invalidate(lv_screen_active());
  }
  lv_refr_now(nullptr);
}

// The SHTC3 temperature sensor isn't used, but it powers up idle (~45 uA). Its sleep command
// drops it to ~0.3 uA.
void sleepTemperatureSensor()
{
  Wire.beginTransmission(SHTC3_ADDRESS);
  Wire.write(SHTC3_CMD_SLEEP >> 8);
  Wire.write(SHTC3_CMD_SLEEP & 0xFF);
  Wire.endTransmission();
}

// Most likely cause, from the Wi-Fi status at the connect timeout
const char *wifiFailReason(uint8_t status)
{
  switch (status)
  {
  case WL_NO_SSID_AVAIL:
    return "Network not found";
  case WL_CONNECT_FAILED:
    return "Connection refused - check the password";
  case WL_CONNECTION_LOST:
    return "Connection lost";
  default:
    return "No response - check the password and signal";
  }
}

void render()
{
  MeterScreen screen = {};
  screen.accounts = usage;
  for (int i = 0; i < settings::CLAUDE_TOKEN_COUNT; i++)
  {
    screen.names[i] = settings::accountName(i + 1);
  }
  if (!viewAvailable(view))
  {
    view = firstView();
  }
  screen.view = view;
  screen.now = time(nullptr);
  screen.clockValid = clockValid();
  screen.batteryPercent = Battery::percentFromMillivolts(battery.readMillivolts());
  screen.wifiState = wifiState;
  screen.wifiRssi = wifiRssi;
  if (settings::wifiSsid().isEmpty() || (!accountConfigured(0) && !accountConfigured(1)))
  {
    screen.notice = "Setup needed\nConnect USB and type help in the serial monitor";
  }
  else if (wifiState == WifiState::Failed)
  {
    screen.popupTitle = "Wi-Fi connect failed";
    screen.popupBody = "\"" + settings::wifiSsid() + "\"\n" + wifiFailReason(wifiFailStatus);
    if (screen.clockValid)
    {
      const time_t retry = nextPollAt;
      struct tm local;
      localtime_r(&retry, &local);
      char at[8];
      strftime(at, sizeof(at), "%H:%M", &local);
      screen.popupBody += String("\nRetry at ") + at;
    }
  }
  meterUiShow(screen);
  refreshPanel();
}

void poll()
{
  const uint32_t start = millis();
  const bool syncClock = !clockValid() || time(nullptr) - lastNtpSync > NTP_RESYNC_S;
  const PollReport report = pollUsage(usage, rtc, syncClock);
  if (report.clockSynced)
  {
    lastNtpSync = time(nullptr);
  }
  if (!report.configured)
  {
    wifiState = WifiState::Unknown;
  }
  else if (report.wifiConnected)
  {
    wifiState = WifiState::Connected;
    wifiRssi = report.wifiRssi;
  }
  else
  {
    wifiState = WifiState::Failed;
    wifiFailStatus = report.wifiStatus;
  }
  // Schedule from when the poll started, so the interval doesn't drift by the poll's own duration
  nextPollAt = time(nullptr) - (millis() - start) / 1000 + settings::pollIntervalMinutes() * 60;
}

// Poll, draw the new figures, then play any alert they trigger
void pollAndShow()
{
  poll();
  render();
  checkAlerts(usage, time(nullptr));
}

[[noreturn]] void deepSleep(uint32_t sleepSeconds)
{
  // A held button would wake the board straight away
  while (digitalRead(PIN_BOOT_BUTTON) == LOW || digitalRead(PIN_PWR_BUTTON) == LOW)
  {
    delay(10);
  }
  memcpy(savedFrame, epaper.frame(), sizeof(savedFrame));
  epaper.sleep();

  esp_sleep_enable_timer_wakeup(sleepSeconds * 1000000ULL);
  // ext1, not ext0: two pins (BOOT and PWR), and the wake status tells which one was pressed
  esp_sleep_enable_ext1_wakeup((1ULL << PIN_BOOT_BUTTON) | (1ULL << PIN_PWR_BUTTON), ESP_EXT1_WAKEUP_ANY_LOW);
  // Keep the battery latch high and the LED off while the chip sleeps
  gpio_hold_en((gpio_num_t)PIN_VBAT_PWR);
  digitalWrite(PIN_LED, LED_OFF);
  gpio_hold_en((gpio_num_t)PIN_LED);
  Serial.flush();
  esp_deep_sleep_start();
}

[[noreturn]] void sleepUntilNextPoll()
{
  const long remaining = (long)nextPollAt - (long)time(nullptr);
  const uint32_t seconds = remaining > (long)MIN_SLEEP_S ? remaining : MIN_SLEEP_S;
  Serial.printf("Sleeping %lu s until the next poll\n", seconds);
  deepSleep(seconds);
}

void setDebugMode(bool on);

void printHelp()
{
  Serial.println("Commands:");
  Serial.println("  status                     time, battery, mode, next poll");
  Serial.println("  usage                      poll now, print and show the result");
  Serial.println("  view                       show the next view (same as BOOT)");
  Serial.println("  interval <1-5>             minutes between polls (default 1)");
  Serial.println("  warn5h <50-99>             5-hour warning sound threshold % (default 80)");
  Serial.println("  warn7d <50-99>             7-day warning sound threshold % (default 90)");
  Serial.println("  alerts                     show which alerts have fired; \"alerts clear\" re-arms them");
  Serial.println("  quiet on | off             enable or disable quiet hours (default on)");
  Serial.println("  quiet <start>-<end>        set quiet hours in 24h local time, e.g. quiet 22-8");
  Serial.println("  sleep                      deep sleep between polls, debug mode kept on (long-press BOOT or PWR on USB returns)");
  Serial.println("  debug off                  turn debug mode off: no serial, no LED (long-press BOOT turns it back on)");
  Serial.println("  rtc                        read the RTC chip, compare with the system clock, last NTP sync");
  Serial.println("  rtc set YYYY-MM-DD HH:MM:SS  set the RTC, local time (NTP also sets it when online)");
  Serial.println("  ssid <name>                save Wi-Fi network name");
  Serial.println("  pass <password>            save Wi-Fi password");
  Serial.println("  token1 <value>             save Claude token 1 (from `claude setup-token`)");
  Serial.println("  token2 <value>             save Claude token 2; \"clear\" as the value removes a token");
  Serial.println("  account1 <name>            name account 1 (default \"Claude 1\"), max 20 chars");
  Serial.println("  account2 <name>            name account 2 (default \"Claude 2\"); \"clear\" restores the default");
  Serial.println("  creds                      show saved credentials, masked");
  Serial.println("  scan                       list Wi-Fi networks in range, flagging the saved SSID");
  Serial.println("  tlscheck                   show api.anthropic.com's certificate chain (sends no token)");
  Serial.println("  files                      list the file system");
  Serial.println("  play <file>                play a WAV, e.g. play 5h-warning.wav");
}

void printStatus()
{
  const time_t now = time(nullptr);
  if (clockValid())
  {
    struct tm local;
    localtime_r(&now, &local);
    char text[32];
    strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S %a", &local);
    Serial.printf("Time:      %s\n", text);
  }
  else
  {
    Serial.println("Time:      not set");
  }
  const uint32_t millivolts = battery.readMillivolts();
  Serial.printf("Battery:   %.2f V  %u %%\n", millivolts / 1000.0f, Battery::percentFromMillivolts(millivolts));
  Serial.printf("View:      %s\n", viewName(view));
  Serial.printf("Interval:  %u min, next poll in %ld s\n", settings::pollIntervalMinutes(), (long)nextPollAt - (long)now);
  Serial.printf("Warnings:  5h at %u%%, 7d at %u%%\n", settings::warningPercent5h(), settings::warningPercent7d());
  Serial.printf("Quiet:     %02u:00-%02u:00 (%s)\n", settings::quietHoursStart(), settings::quietHoursEnd(), settings::quietHoursEnabled() ? "on" : "off");
}

void listFiles()
{
  // Don't format on failure: that would hide a missing uploadfs
  if (!LittleFS.begin(false))
  {
    Serial.println("LittleFS mount failed - run: pio run -t uploadfs");
    return;
  }
  File root = LittleFS.open("/");
  for (File file = root.openNextFile(); file; file = root.openNextFile())
  {
    Serial.printf("%-20s %7u bytes\n", file.name(), file.size());
  }
  Serial.printf("Used %u of %u bytes\n", LittleFS.usedBytes(), LittleFS.totalBytes());
}

// Matches "<prefix><digit> <value>". A value of "clear" becomes empty. The number isn't range-checked.
bool parseNumbered(const String &line, const char *prefix, int &number, String &value)
{
  const size_t prefixLen = strlen(prefix);
  if (!line.startsWith(prefix) || line.length() < prefixLen + 2 || !isDigit(line[prefixLen]) || line[prefixLen + 1] != ' ')
  {
    return false;
  }
  number = line[prefixLen] - '0';
  value = line.substring(prefixLen + 2);
  value.trim();
  if (value == "clear")
  {
    value = "";
  }
  return true;
}

// "rtc": the PCF85063's own time, compared with the system clock the meter uses
void printRtc()
{
  const char *const weekdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  RtcDateTime chip;
  if (!rtc.read(chip))
  {
    Serial.println("RTC read failed");
    return;
  }
  Serial.printf("RTC:       %04u-%02u-%02u %02u:%02u:%02u %s (%s)\n", chip.year, chip.month, chip.day, chip.hour,
                chip.minute, chip.second, weekdays[chip.weekday % 7],
                rtc.timeValid() ? "valid" : "not set - oscillator stopped since last set");

  const time_t now = time(nullptr);
  if (!clockValid())
  {
    Serial.println("System:    not set");
  }
  else
  {
    struct tm local;
    localtime_r(&now, &local);
    char text[24];
    strftime(text, sizeof(text), "%Y-%m-%d %H:%M:%S", &local);
    struct tm chipTm = {};
    chipTm.tm_year = chip.year - 1900;
    chipTm.tm_mon = chip.month - 1;
    chipTm.tm_mday = chip.day;
    chipTm.tm_hour = chip.hour;
    chipTm.tm_min = chip.minute;
    chipTm.tm_sec = chip.second;
    chipTm.tm_isdst = -1;
    Serial.printf("System:    %s (%ld s from RTC)\n", text, (long)(now - mktime(&chipTm)));
  }

  if (lastNtpSync == 0)
  {
    Serial.println("NTP sync:  not since power-up");
  }
  else
  {
    Serial.printf("NTP sync:  %ld min ago, next within %ld min\n", (long)(now - lastNtpSync) / 60,
                  (long)(lastNtpSync + NTP_RESYNC_S - now) / 60);
  }
}

// "rtc set YYYY-MM-DD HH:MM:SS", local time
void setClock(const String &line)
{
  int year, month, day, hour, minute, second;
  if (sscanf(line.c_str(), "rtc set %d-%d-%d %d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6 ||
      year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || day > 31 ||
      hour > 23 || minute > 59 || second > 59)
  {
    Serial.println("Usage: rtc set YYYY-MM-DD HH:MM:SS");
    return;
  }
  RtcDateTime dateTime = {};
  dateTime.year = year;
  dateTime.month = month;
  dateTime.day = day;
  dateTime.hour = hour;
  dateTime.minute = minute;
  dateTime.second = second;
  if (!rtc.write(dateTime))
  {
    Serial.println("RTC write failed");
    return;
  }
  clockBegin(rtc);
  Serial.println("Clock set (NTP will correct it on the next sync)");
  printRtc();
  render();
}

// Serial commands; "help" lists them
void runCommand(const String &line)
{
  int number;
  String value;
  const bool isToken = parseNumbered(line, "token", number, value);
  const bool isAccount = !isToken && parseNumbered(line, "account", number, value);
  if ((isToken || isAccount) && (number < 1 || number > settings::CLAUDE_TOKEN_COUNT))
  {
    Serial.printf("Account number must be 1 to %d\n", settings::CLAUDE_TOKEN_COUNT);
  }
  else if (isToken)
  {
    settings::setClaudeToken(number, value);
    Serial.printf("Claude token %d %s (%s)\n", number, value.isEmpty() ? "cleared" : "saved",
                  settings::mask(settings::claudeToken(number)).c_str());
  }
  else if (isAccount)
  {
    if (value.length() > settings::ACCOUNT_NAME_MAX)
    {
      Serial.printf("Account name too long: %u chars, max %u\n", value.length(), settings::ACCOUNT_NAME_MAX);
      return;
    }
    settings::setAccountName(number, value);
    Serial.printf("Account %d name: \"%s\"\n", number, settings::accountName(number).c_str());
    render();
  }
  else if (line == "help")
  {
    printHelp();
  }
  else if (line == "status")
  {
    printStatus();
  }
  else if (line == "usage")
  {
    poll();
    printUsage(usage);
    render();
    checkAlerts(usage, time(nullptr));
  }
  else if (line.startsWith("warn5h ") || line.startsWith("warn7d "))
  {
    const long percent = line.substring(7).toInt();
    if (percent < settings::WARNING_PERCENT_MIN || percent > settings::WARNING_PERCENT_MAX)
    {
      Serial.printf("Warning threshold must be %u to %u %%\n", settings::WARNING_PERCENT_MIN, settings::WARNING_PERCENT_MAX);
      return;
    }
    if (line.startsWith("warn5h"))
    {
      settings::setWarningPercent5h(percent);
    }
    else
    {
      settings::setWarningPercent7d(percent);
    }
    Serial.printf("Warning at: 5h %u%%, 7d %u%%\n", settings::warningPercent5h(), settings::warningPercent7d());
  }
  else if (line == "alerts")
  {
    printAlertState();
  }
  else if (line == "quiet on" || line == "quiet off")
  {
    settings::setQuietHoursEnabled(line.endsWith("on"));
    Serial.printf("Quiet hours %s (%02u:00-%02u:00)\n", settings::quietHoursEnabled() ? "on" : "off",
                  settings::quietHoursStart(), settings::quietHoursEnd());
  }
  else if (line.startsWith("quiet "))
  {
    int startHour, endHour;
    if (sscanf(line.c_str(), "quiet %d-%d", &startHour, &endHour) != 2 ||
        startHour < 0 || startHour > 23 || endHour < 0 || endHour > 23)
    {
      Serial.println("Usage: quiet <start>-<end> in 24h local time, e.g. quiet 22-8");
    }
    else
    {
      settings::setQuietHours(startHour, endHour);
      Serial.printf("Quiet hours %02u:00-%02u:00 (%s)\n", startHour, endHour, settings::quietHoursEnabled() ? "on" : "off");
    }
  }
  else if (line == "alerts clear")
  {
    clearAlertState();
    Serial.println("Alert state cleared - the next poll alerts again for anything above a threshold");
  }
  else if (line == "view")
  {
    view = nextView(view);
    Serial.printf("View: %s\n", viewName(view));
    render();
  }
  else if (line.startsWith("interval "))
  {
    const long minutes = line.substring(9).toInt();
    if (minutes < settings::POLL_INTERVAL_MIN || minutes > settings::POLL_INTERVAL_MAX)
    {
      Serial.printf("Interval must be %u to %u minutes\n", settings::POLL_INTERVAL_MIN, settings::POLL_INTERVAL_MAX);
      return;
    }
    settings::setPollIntervalMinutes(minutes);
    // A shorter interval takes effect now rather than after the old, longer wait
    nextPollAt = min<time_t>(nextPollAt, time(nullptr) + minutes * 60);
    Serial.printf("Poll interval: %ld min\n", minutes);
  }
  else if (line == "debug off")
  {
    setDebugMode(false);
    sleepUntilNextPoll();
  }
  else if (line == "sleep")
  {
    Serial.println("Sleeping between polls, debug mode stays on");
    sleepUntilNextPoll();
  }
  else if (line == "rtc")
  {
    printRtc();
  }
  else if (line.startsWith("rtc set "))
  {
    setClock(line);
  }
  else if (line.startsWith("ssid "))
  {
    settings::setWifiSsid(line.substring(5));
    Serial.printf("Wi-Fi SSID saved: \"%s\"\n", settings::wifiSsid().c_str());
  }
  else if (line.startsWith("pass "))
  {
    settings::setWifiPassword(line.substring(5));
    Serial.printf("Wi-Fi password saved (%s)\n", settings::mask(settings::wifiPassword(), 0).c_str());
  }
  else if (line == "creds")
  {
    Serial.printf("Wi-Fi SSID:     \"%s\"\n", settings::wifiSsid().c_str());
    Serial.printf("Wi-Fi password: %s\n", settings::mask(settings::wifiPassword(), 0).c_str());
    for (number = 1; number <= settings::CLAUDE_TOKEN_COUNT; number++)
    {
      Serial.printf("Account %d:      \"%s\", token %s\n", number, settings::accountName(number).c_str(),
                    settings::mask(settings::claudeToken(number)).c_str());
    }
  }
  else if (line == "scan")
  {
    runWifiScan();
  }
  else if (line == "tlscheck")
  {
    runTlsCheck();
  }
  else if (line == "files")
  {
    listFiles();
  }
  else if (line.startsWith("play "))
  {
    String path = line.substring(5);
    path.trim();
    if (!path.startsWith("/"))
    {
      path = "/" + path;
    }
    playWav(path.c_str());
  }
  else
  {
    Serial.printf("Unknown command \"%s\" - type help\n", line.c_str());
  }
}

void handleSerialCommands()
{
  static String line;
  while (Serial.available())
  {
    const char c = Serial.read();
    if (c != '\n' && c != '\r')
    {
      line += c;
      continue;
    }
    if (line.length() > 0)
    {
      runCommand(line);
    }
    line = "";
  }
}

void powerOff()
{
  Serial.println("Power off");
  // Blank the panel before cutting power, since e-paper keeps its image. begin() does a full
  // refresh to white, which also clears any ghosting.
  epaper.begin();
  partialRefreshes = 0;
  digitalWrite(PIN_VBAT_PWR, LOW);
  powerOffArmed = false;

  delay(500);
  Serial.println("Still running: board is powered from USB, not battery");
}

void handlePowerButton()
{
  const bool down = digitalRead(PIN_PWR_BUTTON) == LOW;
  const uint32_t now = millis();
  if (!down)
  {
    powerOffArmed = true;
  }
  else if (!pwrButtonWasDown)
  {
    pwrButtonDownAt = now;
  }
  else if (powerOffArmed && now - pwrButtonDownAt >= LONG_PRESS_MS)
  {
    powerOff();
    render();
  }
  pwrButtonWasDown = down;
}

// Debug mode: serial console on and the LED lit while awake. Off: no serial and no LED, to save power.
void applyDebugMode()
{
  static bool serialStarted = false;
  if (debugMode && !serialStarted)
  {
    Serial.begin(115200);
    serialStarted = true;
  }
  else if (!debugMode && serialStarted)
  {
    Serial.end();
    serialStarted = false;
  }
  digitalWrite(PIN_LED, debugMode ? LED_ON : LED_OFF);
}

void setDebugMode(bool on)
{
  if (!on)
  {
    Serial.println("Debug mode off: serial and LED off, deep sleep between polls");
    Serial.flush();
  }
  debugMode = on;
  applyDebugMode();
  if (on)
  {
    Serial.println("Debug mode on - type help for commands");
  }
}

// In awake mode: a short BOOT press shows the next view, a long press turns debug mode off and
// starts the sleep cycle
void handleBootButton()
{
  // The press that woke the board must be released before a new press counts
  static bool armed = false;
  static uint32_t downAt = 0;
  static bool longPressHandled = false;
  const bool down = digitalRead(PIN_BOOT_BUTTON) == LOW;
  if (!down)
  {
    // Short press: act on release, once it's clear the press isn't a long one
    if (armed && downAt != 0 && !longPressHandled && millis() - downAt >= BUTTON_MIN_PRESS_MS)
    {
      view = nextView(view);
      Serial.printf("View: %s\n", viewName(view));
      render();
    }
    armed = true;
    downAt = 0;
    longPressHandled = false;
    return;
  }
  if (!armed)
  {
    return;
  }
  if (downAt == 0)
  {
    downAt = millis();
  }
  else if (!longPressHandled && millis() - downAt >= LONG_PRESS_MS)
  {
    longPressHandled = true;
    setDebugMode(false);
    sleepUntilNextPoll();
  }
}

void setup()
{
  // Latch battery power first, so the board stays on once the PWR button is released.
  // After deep sleep the pin is still held; set the level before releasing it so it never glitches low.
  pinMode(PIN_VBAT_PWR, OUTPUT);
  digitalWrite(PIN_VBAT_PWR, HIGH);
  gpio_hold_dis((gpio_num_t)PIN_VBAT_PWR);
  // LED off until debug mode is known; it's held off during deep sleep
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LED_OFF);
  gpio_hold_dis((gpio_num_t)PIN_LED);
  pinMode(PIN_PWR_BUTTON, INPUT_PULLUP);
  pinMode(PIN_BOOT_BUTTON, INPUT_PULLUP);

  const esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();
  const bool timerWake = wakeCause == ESP_SLEEP_WAKEUP_TIMER;
  const bool buttonWake = wakeCause == ESP_SLEEP_WAKEUP_EXT1;
  const bool pwrWake = buttonWake && (esp_sleep_get_ext1_wakeup_status() & (1ULL << PIN_PWR_BUTTON));
  const bool bootWake = buttonWake && !pwrWake;
  // After deep sleep the panel still shows savedFrame
  const bool resumed = timerWake || buttonWake;

  // Cold boot: debug mode on when a USB host is attached. After a BOOT wake, holding BOOT
  // (counting from the wake, which millis() starts at) toggles it.
  bool bootLongPress = false;
  if (!resumed)
  {
    debugMode = usbHostConnected();
  }
  else if (bootWake)
  {
    while (digitalRead(PIN_BOOT_BUTTON) == LOW && millis() < LONG_PRESS_MS)
    {
      delay(10);
    }
    bootLongPress = digitalRead(PIN_BOOT_BUTTON) == LOW;
    if (bootLongPress)
    {
      debugMode = !debugMode;
    }
  }
  applyDebugMode();

  // The vendor example powers both rails before using the display and I2C devices
  pinMode(PIN_EPD_PWR, OUTPUT);
  digitalWrite(PIN_EPD_PWR, LOW);
  pinMode(PIN_AUDIO_PWR, OUTPUT);
  digitalWrite(PIN_AUDIO_PWR, LOW);
  delay(10);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  sleepTemperatureSensor();
  if (!rtc.begin())
  {
    Serial.println("PCF85063 not responding");
  }
  clockBegin(rtc);

  if (resumed)
  {
    epaper.resume(savedFrame);
  }
  else
  {
    epaper.begin();
    partialRefreshes = 0;
  }
  lvglPortBegin(epaper);

  if (timerWake)
  {
    wakeCount++;
    pollAndShow();
    sleepUntilNextPoll();
  }
  if (bootWake && !bootLongPress)
  {
    // BOOT: next view from cached data, keeping the poll schedule unless a poll is due anyway
    view = nextView(view);
    if (time(nullptr) >= nextPollAt)
    {
      pollAndShow();
    }
    else
    {
      render();
    }
    sleepUntilNextPoll();
  }
  if (bootWake && !debugMode)
  {
    // Long press turned debug mode off: keep sleeping between polls
    sleepUntilNextPoll();
  }
  if (pwrWake)
  {
    // Returns only when USB keeps the board running; stay awake in debug mode then
    powerOff();
    debugMode = true;
    applyDebugMode();
  }

  // Cold boot, or debug mode just turned on: draw cached or placeholder data while the first poll runs
  render();
  if (!debugMode)
  {
    pollAndShow();
    sleepUntilNextPoll();
  }
  Serial.println("Debug mode on (awake) - type help for commands, long-press BOOT to turn it off");
  pollAndShow();
}

void loop()
{
  static time_t lastMinute = time(nullptr) / 60;
  lv_timer_handler();
  handleBootButton();
  handlePowerButton();
  handleSerialCommands();

  const time_t now = time(nullptr);
  if (now >= nextPollAt)
  {
    pollAndShow();
    lastMinute = now / 60;
  }
  else if (now / 60 != lastMinute)
  {
    // Clock and countdowns change every minute
    lastMinute = now / 60;
    render();
  }
  delay(5);
}
