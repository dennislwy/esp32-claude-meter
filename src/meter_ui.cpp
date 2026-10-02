#include "meter_ui.h"

#include <lvgl.h>

extern "C" const lv_image_dsc_t claude_icon;

namespace
{
constexpr int WIDTH = 200;
constexpr int HEIGHT = 200;
constexpr int STATUS_BAR_H = 24;
constexpr int PAD = 4;
constexpr int DIVIDER_H = 2;
constexpr int CONTENT_W = WIDTH - 2 * PAD;

// Dual view (Layout #2): two cards below the status bar, split by a divider
constexpr int CARD_H = (HEIGHT - STATUS_BAR_H - DIVIDER_H) / 2;
constexpr int SPLIT_BAR_H = 18;

// Single-account view (Layout #1 style)
constexpr int THIN_BAR_H = 8;

const lv_color_t BLACK = lv_color_black();
const lv_color_t WHITE = lv_color_white();

// A plain rectangle without theme styling
lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, lv_color_t color)
{
  lv_obj_t *obj = lv_obj_create(parent);
  lv_obj_remove_style_all(obj);
  lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(obj, x, y);
  lv_obj_set_size(obj, w, h);
  lv_obj_set_style_bg_color(obj, color, 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  return obj;
}

lv_obj_t *text(lv_obj_t *parent, const char *value, const lv_font_t *font, lv_color_t color)
{
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_label_set_text(label, value);
  return label;
}

// Text right-aligned so it ends at x
lv_obj_t *textRight(lv_obj_t *parent, const char *value, const lv_font_t *font, lv_color_t color, int x, int y)
{
  lv_obj_t *label = text(parent, value, font, color);
  lv_obj_set_width(label, x);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(label, 0, y);
  return label;
}

float clampPercent(float percent)
{
  return percent < 0 ? 0 : percent > 100 ? 100 : percent;
}

String formatPercent(const AccountUsage &account, float percent)
{
  return account.hasData ? String((int)lroundf(percent)) + "%" : String("--%");
}

// "2d 15h 1m", "4h 34m", "12m"
String formatDuration(long seconds)
{
  const long days = seconds / 86400;
  const long hours = seconds % 86400 / 3600;
  const long minutes = seconds % 3600 / 60;
  char buf[24];
  if (days > 0)
  {
    snprintf(buf, sizeof(buf), "%ldd %ldh %ldm", days, hours, minutes);
  }
  else if (hours > 0)
  {
    snprintf(buf, sizeof(buf), "%ldh %ldm", hours, minutes);
  }
  else
  {
    snprintf(buf, sizeof(buf), "%ldm", minutes);
  }
  return buf;
}

String formatCountdown(const MeterScreen &screen, const AccountUsage &account, uint32_t reset)
{
  if (!account.hasData || !screen.clockValid)
  {
    return "--";
  }
  const long seconds = (long)reset - (long)screen.now;
  return seconds > 0 ? formatDuration(seconds) : String("now");
}

// Staleness for the card header: "2m ago", or the error when the last poll failed
String formatAge(const MeterScreen &screen, const AccountUsage &account)
{
  String error;
  if (account.lastPollFailed)
  {
    error = account.lastStatus < 0 ? "offline" : "HTTP " + String(account.lastStatus);
  }
  if (!account.hasData)
  {
    return error.isEmpty() ? "no data" : error;
  }
  const long age = screen.clockValid ? (long)screen.now - (long)account.fetchedAt : 0;
  const String ago = age < 60 ? String("now") : formatDuration(age) + " ago";
  return error.isEmpty() ? ago : "! " + ago;
}

void statusBar(lv_obj_t *parent, const MeterScreen &screen)
{
  lv_obj_t *bar = box(parent, 0, 0, WIDTH, STATUS_BAR_H, BLACK);

  lv_obj_t *icon = lv_image_create(bar);
  lv_image_set_src(icon, &claude_icon);
  lv_obj_set_pos(icon, PAD, (STATUS_BAR_H - claude_icon.header.h) / 2);

  char clock[8] = "--:--";
  if (screen.clockValid)
  {
    struct tm local;
    localtime_r(&screen.now, &local);
    strftime(clock, sizeof(clock), "%H:%M", &local);
  }
  lv_obj_t *time = text(bar, clock, &lv_font_montserrat_16, WHITE);
  lv_obj_align(time, LV_ALIGN_CENTER, 0, 0);

  const char *symbol = screen.batteryPercent >= 90 ? LV_SYMBOL_BATTERY_FULL
                       : screen.batteryPercent >= 65 ? LV_SYMBOL_BATTERY_3
                       : screen.batteryPercent >= 40 ? LV_SYMBOL_BATTERY_2
                       : screen.batteryPercent >= 15 ? LV_SYMBOL_BATTERY_1
                                                      : LV_SYMBOL_BATTERY_EMPTY;
  const String battery = String(symbol) + " " + screen.batteryPercent + "%";
  lv_obj_t *batteryLabel = text(bar, battery.c_str(), &lv_font_montserrat_12, WHITE);
  lv_obj_align(batteryLabel, LV_ALIGN_RIGHT_MID, -PAD, 0);
}

// Label left and value right, vertically centred in a w x h area
void barText(lv_obj_t *parent, const char *label, const char *value, int w, int h, lv_color_t color)
{
  lv_obj_t *left = text(parent, label, &lv_font_montserrat_12, color);
  lv_obj_set_pos(left, 3, (h - lv_font_get_line_height(&lv_font_montserrat_12)) / 2);
  textRight(parent, value, &lv_font_montserrat_14, color, w - 3, (h - lv_font_get_line_height(&lv_font_montserrat_14)) / 2);
}

// Layout #2 bar: text is black over the empty part and white over the filled part. The white copy
// is a child of the fill, so LVGL clips it to the fill exactly at the boundary.
void splitBar(lv_obj_t *parent, int x, int y, const char *label, float percent, const char *value)
{
  lv_obj_t *bar = box(parent, x, y, CONTENT_W, SPLIT_BAR_H, WHITE);
  lv_obj_set_style_border_width(bar, 1, 0);
  lv_obj_set_style_border_color(bar, BLACK, 0);
  lv_obj_set_style_border_post(bar, true, 0);
  const int innerW = CONTENT_W - 2;
  const int innerH = SPLIT_BAR_H - 2;
  barText(bar, label, value, innerW, innerH, BLACK);
  const int fillW = lroundf(innerW * clampPercent(percent) / 100.0f);
  if (fillW > 0)
  {
    lv_obj_t *fill = box(bar, 0, 0, fillW, innerH, BLACK);
    barText(fill, label, value, innerW, innerH, WHITE);
  }
}

void dualCard(lv_obj_t *parent, int y, const MeterScreen &screen, int index)
{
  const AccountUsage &account = screen.accounts[index];
  lv_obj_t *name = text(parent, screen.names[index].c_str(), &lv_font_montserrat_14, BLACK);
  lv_obj_set_pos(name, PAD, y + 2);
  textRight(parent, formatAge(screen, account).c_str(), &lv_font_montserrat_10, BLACK, WIDTH - PAD, y + 4);

  splitBar(parent, PAD, y + 20, "5-HOUR", account.fiveHourPercent, formatPercent(account, account.fiveHourPercent).c_str());
  splitBar(parent, PAD, y + 40, "7-DAY", account.sevenDayPercent, formatPercent(account, account.sevenDayPercent).c_str());

  const String reset5h = "5H Reset " + formatCountdown(screen, account, account.fiveHourReset);
  const String reset7d = "7D Reset " + formatCountdown(screen, account, account.sevenDayReset);
  lv_obj_set_pos(text(parent, reset5h.c_str(), &lv_font_montserrat_10, BLACK), PAD, y + 60);
  lv_obj_set_pos(text(parent, reset7d.c_str(), &lv_font_montserrat_10, BLACK), PAD, y + 72);
}

// One window in the single-account view: large %, badge, thin bar, "Resets in ..."
void singleWindow(lv_obj_t *parent, int y, const MeterScreen &screen, const AccountUsage &account,
                  const char *badgeText, float percent, uint32_t reset)
{
  lv_obj_set_pos(text(parent, formatPercent(account, percent).c_str(), &lv_font_montserrat_28, BLACK), PAD, y);

  lv_obj_t *badge = box(parent, WIDTH - PAD - 28, y + 6, 28, 18, BLACK);
  lv_obj_set_style_radius(badge, 5, 0);
  lv_obj_center(text(badge, badgeText, &lv_font_montserrat_12, WHITE));

  lv_obj_t *track = box(parent, PAD, y + 32, CONTENT_W, THIN_BAR_H, WHITE);
  lv_obj_set_style_border_width(track, 1, 0);
  lv_obj_set_style_border_color(track, BLACK, 0);
  const int fillW = lroundf((CONTENT_W - 2) * clampPercent(percent) / 100.0f);
  if (account.hasData && fillW > 0)
  {
    box(track, 0, 0, fillW, THIN_BAR_H - 2, BLACK);
  }

  const String resets = "Resets in " + formatCountdown(screen, account, reset);
  lv_obj_set_pos(text(parent, resets.c_str(), &lv_font_montserrat_12, BLACK), PAD, y + 43);
}

void singleView(lv_obj_t *parent, const MeterScreen &screen, int index)
{
  const AccountUsage &account = screen.accounts[index];
  const int y = STATUS_BAR_H + 4;
  lv_obj_set_pos(text(parent, screen.names[index].c_str(), &lv_font_montserrat_14, BLACK), PAD, y);
  textRight(parent, formatAge(screen, account).c_str(), &lv_font_montserrat_10, BLACK, WIDTH - PAD, y + 2);

  singleWindow(parent, y + 22, screen, account, "5H", account.fiveHourPercent, account.fiveHourReset);
  singleWindow(parent, y + 92, screen, account, "7D", account.sevenDayPercent, account.sevenDayReset);
}

lv_obj_t *clearScreen()
{
  lv_obj_t *screen = lv_screen_active();
  lv_obj_clean(screen);
  lv_obj_set_style_bg_color(screen, WHITE, 0);
  lv_obj_set_style_pad_all(screen, 0, 0);
  lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  return screen;
}
}

void meterUiShow(const MeterScreen &screen)
{
  lv_obj_t *root = clearScreen();
  statusBar(root, screen);

  if (screen.notice)
  {
    lv_obj_t *notice = text(root, screen.notice, &lv_font_montserrat_14, BLACK);
    lv_obj_set_width(notice, CONTENT_W);
    lv_label_set_long_mode(notice, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(notice, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(notice, LV_ALIGN_CENTER, 0, STATUS_BAR_H / 2);
    return;
  }

  switch (screen.view)
  {
  case MeterView::Dual:
    dualCard(root, STATUS_BAR_H, screen, 0);
    box(root, 0, STATUS_BAR_H + CARD_H, WIDTH, DIVIDER_H, BLACK);
    dualCard(root, STATUS_BAR_H + CARD_H + DIVIDER_H, screen, 1);
    break;
  case MeterView::Account1:
    singleView(root, screen, 0);
    break;
  case MeterView::Account2:
    singleView(root, screen, 1);
    break;
  }
}
