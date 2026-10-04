#include "meter_ui.h"

#include <lvgl.h>
#include "history.h"

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
constexpr int RESET_LINE_X = 2;

// Single-account view (Layout #1 style)
constexpr int THIN_BAR_H = 14;
// Vertical offsets inside singleWindow()
constexpr int SINGLE_BAR_Y_OFFSET = 32;
constexpr int SINGLE_RESET_Y_OFFSET = SINGLE_BAR_Y_OFFSET + THIN_BAR_H + 3;
// Pitch between 5H and 7D windows (the second starts this many pixels below the first)
constexpr int SINGLE_WINDOW_PITCH = SINGLE_RESET_Y_OFFSET + 12 + 15;

// Wi-Fi icon in the status bar: a dot with three concentric arcs fanning upwards
constexpr int WIFI_ICON_W = 22;           // width of the outer arc's 90-degree wedge
constexpr int WIFI_DOT_R = 2;
constexpr int WIFI_ARC_RADII[] = {7, 11, 15}; // outer radius of each arc, innermost first
constexpr int WIFI_ARC_LIT_W = 2;         // lit arcs are thick, unlit ones a thin outline
constexpr int WIFI_ARC_UNLIT_W = 1;
constexpr int8_t WIFI_3_ARCS_DBM = -60;
constexpr int8_t WIFI_2_ARCS_DBM = -70;
constexpr int8_t WIFI_1_ARC_DBM = -80;    // weaker: the dot alone

constexpr int POPUP_W = 170;

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
  // Two most significant units only
  if (days > 0)
  {
    snprintf(buf, sizeof(buf), "%ldd %ldh", days, hours);
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

enum class DayCase
{
  Upper, // "SAT"
  Title, // "Sat", narrower
};

// prefix + " SAT 3 Oct 01:10 in 1h 14m", e.g. "5H resets SAT 3 Oct 01:10 in 1h 14m"
String formatResetLine(const MeterScreen &screen, const AccountUsage &account, const char *prefix, uint32_t reset,
                       DayCase dayCase = DayCase::Upper)
{
  static const char *const UPPER_DAYS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  static const char *const TITLE_DAYS[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  static const char *const MONTHS[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  const char *const *DAYS = dayCase == DayCase::Upper ? UPPER_DAYS : TITLE_DAYS;
  String line = String(prefix) + " ";
  if (!account.hasData)
  {
    return line + "--";
  }
  const time_t resetTime = reset;
  struct tm local;
  localtime_r(&resetTime, &local);
  char when[24];
  snprintf(when, sizeof(when), "%s %d %s %02d:%02d", DAYS[local.tm_wday], local.tm_mday, MONTHS[local.tm_mon],
           local.tm_hour, local.tm_min);
  line += when;
  if (screen.clockValid)
  {
    const long seconds = (long)reset - (long)screen.now;
    line += seconds > 0 ? " in " + formatDuration(seconds) : String(" (now)");
  }
  return line;
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

int wifiArcs(int8_t rssi)
{
  return rssi >= WIFI_3_ARCS_DBM ? 3 : rssi >= WIFI_2_ARCS_DBM ? 2 : rssi >= WIFI_1_ARC_DBM ? 1 : 0;
}

// One arc of the fan: a 90-degree wedge pointing up, centred on (cx, cy)
void wifiArc(lv_obj_t *parent, int cx, int cy, int radius, int width)
{
  lv_obj_t *arc = lv_arc_create(parent);
  lv_obj_remove_style_all(arc);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(arc, 2 * radius, 2 * radius);
  lv_obj_set_pos(arc, cx - radius, cy - radius);
  // LVGL measures angles clockwise from 3 o'clock, so 270 is straight up
  lv_arc_set_bg_angles(arc, 225, 315);
  lv_obj_set_style_arc_width(arc, width, LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, WHITE, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
  lv_obj_set_style_arc_opa(arc, LV_OPA_TRANSP, LV_PART_INDICATOR);
}

// Wi-Fi signal, white on the black status bar: a dot with three arcs fanning upwards. Lit arcs
// (thick) show strength from the innermost out; unlit ones stay as a thin outline. A failed
// connection shows no lit arcs, followed by an "x".
void wifiIcon(lv_obj_t *bar, int x, const MeterScreen &screen)
{
  if (screen.wifiState == WifiState::Unknown)
  {
    return;
  }
  const int lit = screen.wifiState == WifiState::Connected ? wifiArcs(screen.wifiRssi) : 0;
  const int cx = x + WIFI_ICON_W / 2;
  const int cy = STATUS_BAR_H - 4;
  for (int i = 0; i < 3; i++)
  {
    wifiArc(bar, cx, cy, WIFI_ARC_RADII[i], i < lit ? WIFI_ARC_LIT_W : WIFI_ARC_UNLIT_W);
  }
  lv_obj_t *dot = box(bar, cx - WIFI_DOT_R, cy - WIFI_DOT_R, 2 * WIFI_DOT_R, 2 * WIFI_DOT_R, WHITE);
  lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
  if (screen.wifiState == WifiState::Failed)
  {
    lv_obj_t *cross = text(bar, "x", &lv_font_montserrat_12, WHITE);
    lv_obj_set_pos(cross, x + WIFI_ICON_W + 1, cy - lv_font_get_line_height(&lv_font_montserrat_12) + 3);
  }
}

// Bordered box centred over the view: bold-ish title, then wrapped detail lines
void popup(lv_obj_t *parent, const char *title, const String &body)
{
  lv_obj_t *frame = box(parent, 0, 0, POPUP_W, LV_SIZE_CONTENT, WHITE);
  lv_obj_set_style_border_width(frame, 2, 0);
  lv_obj_set_style_border_color(frame, BLACK, 0);
  lv_obj_set_style_radius(frame, 6, 0);
  lv_obj_set_style_pad_all(frame, 8, 0);
  lv_obj_set_style_pad_row(frame, 4, 0);
  lv_obj_set_flex_flow(frame, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(frame, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  text(frame, title, &lv_font_montserrat_14, BLACK);
  lv_obj_t *detail = text(frame, body.c_str(), &lv_font_montserrat_12, BLACK);
  lv_obj_set_width(detail, POPUP_W - 20);
  lv_label_set_long_mode(detail, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(detail, LV_TEXT_ALIGN_CENTER, 0);

  // Centre in the area below the status bar
  lv_obj_align(frame, LV_ALIGN_CENTER, 0, STATUS_BAR_H / 2);
}

void statusBar(lv_obj_t *parent, const MeterScreen &screen)
{
  lv_obj_t *bar = box(parent, 0, 0, WIDTH, STATUS_BAR_H, BLACK);

  lv_obj_t *icon = lv_image_create(bar);
  lv_image_set_src(icon, &claude_icon);
  lv_obj_set_pos(icon, PAD, (STATUS_BAR_H - claude_icon.header.h) / 2);
  wifiIcon(bar, PAD + claude_icon.header.w + 6, screen);

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

  // Up to ~195 px at the longest ("WED 30 Sep ... in 4h 59m"), so these start nearer the edge
  const String reset5h = formatResetLine(screen, account, "5H resets", account.fiveHourReset);
  const String reset7d = formatResetLine(screen, account, "7D resets", account.sevenDayReset);
  lv_obj_set_pos(text(parent, reset5h.c_str(), &lv_font_montserrat_10, BLACK), RESET_LINE_X, y + 60);
  lv_obj_set_pos(text(parent, reset7d.c_str(), &lv_font_montserrat_10, BLACK), RESET_LINE_X, y + 72);
}

// One window in the single-account view: window name ("5H") left and large % right in the same
// style, thin bar, "<refresh icon> Sat 3 Oct 01:10 in ..."
void singleWindow(lv_obj_t *parent, int y, const MeterScreen &screen, const AccountUsage &account,
                  const char *windowName, float percent, uint32_t reset)
{
  lv_obj_set_pos(text(parent, windowName, &lv_font_montserrat_28, BLACK), PAD, y);
  textRight(parent, formatPercent(account, percent).c_str(), &lv_font_montserrat_28, BLACK, WIDTH - PAD, y);

  lv_obj_t *track = box(parent, PAD, y + SINGLE_BAR_Y_OFFSET, CONTENT_W, THIN_BAR_H, WHITE);
  lv_obj_set_style_border_width(track, 1, 0);
  lv_obj_set_style_border_color(track, BLACK, 0);
  const int fillW = lroundf((CONTENT_W - 2) * clampPercent(percent) / 100.0f);
  if (account.hasData && fillW > 0)
  {
    box(track, 0, 0, fillW, THIN_BAR_H - 2, BLACK);
  }

  // A refresh icon instead of "Resets", and "Wed" rather than "WED", keep the longest form
  // ("Wed 20 May 00:00 in 4h 48m") to ~195 px at 12 px, so it starts nearer the edge
  const String resets = formatResetLine(screen, account, LV_SYMBOL_REFRESH, reset, DayCase::Title);
  lv_obj_set_pos(text(parent, resets.c_str(), &lv_font_montserrat_12, BLACK), RESET_LINE_X, y + SINGLE_RESET_Y_OFFSET);
}

// Chart geometry (used by historyView and chartSeries). Shifted left so the right-side
// y-axis labels ("100", "50", "0") fit within the 200-px panel.
constexpr int CHART_COLS = HIST_SLOTS / 2;    // 168 columns, 1 col = 1 hour
constexpr int CHART_LEFT = 6;
constexpr int CHART_RIGHT = CHART_LEFT + CHART_COLS; // 174
constexpr int CHART_TOP = 50;
constexpr int CHART_BOTTOM = 170;
constexpr int CHART_H = CHART_BOTTOM - CHART_TOP; // 120 px

// Static point buffers survive each UI rebuild; LVGL stores the pointer in lv_line
lv_point_precise_t points5h[CHART_COLS];
lv_point_precise_t points7d[CHART_COLS];

int chartY(uint8_t value)
{
  const int clamped = value > 100 ? 100 : value;
  return CHART_BOTTOM - (clamped * CHART_H) / 100;
}

// Draws one series as a sequence of lv_line widgets, breaking at HIST_EMPTY gaps.
// buf is a static buffer the caller owns; segments share slices of it.
void chartSeries(lv_obj_t *parent, lv_point_precise_t *buf, const uint8_t *values, int lineWidth)
{
  int bufIdx = 0;
  int segStart = -1;
  for (int i = 0; i < CHART_COLS; i++)
  {
    if (values[i] != HIST_EMPTY)
    {
      buf[bufIdx].x = CHART_LEFT + i;
      buf[bufIdx].y = chartY(values[i]);
      if (segStart < 0)
      {
        segStart = bufIdx;
      }
      bufIdx++;
      continue;
    }
    if (segStart >= 0 && bufIdx - segStart >= 2)
    {
      lv_obj_t *line = lv_line_create(parent);
      lv_line_set_points(line, buf + segStart, bufIdx - segStart);
      lv_obj_set_style_line_color(line, BLACK, 0);
      lv_obj_set_style_line_width(line, lineWidth, 0);
    }
    segStart = -1;
  }
  if (segStart >= 0 && bufIdx - segStart >= 2)
  {
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points(line, buf + segStart, bufIdx - segStart);
    lv_obj_set_style_line_color(line, BLACK, 0);
    lv_obj_set_style_line_width(line, lineWidth, 0);
  }
}

void historyView(lv_obj_t *parent, const MeterScreen &screen, int index)
{
  const int y = STATUS_BAR_H + 4;
  const String title = screen.names[index] + " - 7-day";
  lv_obj_set_pos(text(parent, title.c_str(), &lv_font_montserrat_14, BLACK), PAD, y);

  // Legend top-right: "5H" with a thin line sample, "7D" with a thick one
  lv_obj_set_pos(text(parent, "5H", &lv_font_montserrat_10, BLACK), 118, y + 4);
  box(parent, 134, y + 10, 14, 1, BLACK);
  lv_obj_set_pos(text(parent, "7D", &lv_font_montserrat_10, BLACK), 156, y + 4);
  box(parent, 172, y + 9, 14, 2, BLACK);

  // L-shaped axis frame: bottom baseline + left vertical
  box(parent, CHART_LEFT, CHART_BOTTOM, CHART_RIGHT - CHART_LEFT + 1, 1, BLACK);
  box(parent, CHART_LEFT, CHART_TOP, 1, CHART_BOTTOM - CHART_TOP, BLACK);
  // Y-axis: ticks and labels every 25 %
  for (int pct = 0; pct <= 100; pct += 25)
  {
    const int yTick = chartY(pct);
    if (pct > 0)
    {
      box(parent, CHART_LEFT - 2, yTick, 3, 1, BLACK);
    }
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", pct);
    lv_obj_set_pos(text(parent, buf, &lv_font_montserrat_10, BLACK), CHART_RIGHT + 2, yTick - 5);
  }

  // Snapshot and downsample to one column per hour, max of the two 30-min samples
  HistSlot buf[HIST_SLOTS];
  uint32_t newest = 0;
  historySnapshot(index, buf, newest);
  uint8_t cols5h[CHART_COLS];
  uint8_t cols7d[CHART_COLS];
  for (int c = 0; c < CHART_COLS; c++)
  {
    const HistSlot &a = buf[c * 2];
    const HistSlot &b = buf[c * 2 + 1];
    uint8_t h5 = HIST_EMPTY;
    if (a.h5 != HIST_EMPTY)
    {
      h5 = a.h5;
    }
    if (b.h5 != HIST_EMPTY && (h5 == HIST_EMPTY || b.h5 > h5))
    {
      h5 = b.h5;
    }
    uint8_t d7 = HIST_EMPTY;
    if (a.d7 != HIST_EMPTY)
    {
      d7 = a.d7;
    }
    if (b.d7 != HIST_EMPTY && (d7 == HIST_EMPTY || b.d7 > d7))
    {
      d7 = b.d7;
    }
    cols5h[c] = h5;
    cols7d[c] = d7;
  }

  chartSeries(parent, points5h, cols5h, 1);
  chartSeries(parent, points7d, cols7d, 2);

  // X-axis: ticks at local midnight boundaries, one-letter day labels centred on each day's slice
  if (newest != 0)
  {
    static const char *const DAY_INITIALS[] = {"S", "M", "T", "W", "T", "F", "S"};
    const time_t newestT = (time_t)newest;
    struct tm newestLocal;
    localtime_r(&newestT, &newestLocal);
    // Column at the local midnight that started "today" (col 167 = newest hour)
    const int midnightCol = CHART_COLS - 1 - newestLocal.tm_hour;
    for (int d = 0; d <= 7; d++)
    {
      const int col = midnightCol - d * 24;
      if (col >= 0 && col < CHART_COLS)
      {
        box(parent, CHART_LEFT + col, CHART_BOTTOM + 1, 1, 2, BLACK);
      }
    }
    for (int d = 0; d < 8; d++)
    {
      int sliceStart = midnightCol - d * 24;
      int sliceEnd = d == 0 ? CHART_COLS - 1 : sliceStart + 23;
      if (sliceEnd < 0)
      {
        break;
      }
      if (sliceStart < 0)
      {
        sliceStart = 0;
      }
      if (sliceEnd >= CHART_COLS)
      {
        sliceEnd = CHART_COLS - 1;
      }
      const int colCentre = (sliceStart + sliceEnd) / 2;
      const time_t t = newestT - (time_t)(d * 86400);
      struct tm local;
      localtime_r(&t, &local);
      // Single letter is ~5 px wide; shift by -2 to centre under the tick
      lv_obj_set_pos(text(parent, DAY_INITIALS[local.tm_wday], &lv_font_montserrat_10, BLACK),
                     CHART_LEFT + colCentre - 2, CHART_BOTTOM + 4);
    }
  }
}

void panelView(lv_obj_t *parent, const MeterScreen &screen)
{
  const int y = STATUS_BAR_H + 6;
  lv_obj_t *title = text(parent, "LAN Panel", &lv_font_montserrat_16, BLACK);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, y);

  const String url = String("http://") + screen.panelHostname + ".local";
  lv_obj_t *urlLabel = text(parent, url.c_str(), &lv_font_montserrat_12, BLACK);
  lv_obj_set_width(urlLabel, CONTENT_W);
  lv_obj_set_style_text_align(urlLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(urlLabel, LV_ALIGN_TOP_MID, 0, y + 22);

  lv_obj_t *ipLabel = text(parent, screen.panelIp.c_str(), &lv_font_montserrat_12, BLACK);
  lv_obj_set_width(ipLabel, CONTENT_W);
  lv_obj_set_style_text_align(ipLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(ipLabel, LV_ALIGN_TOP_MID, 0, y + 38);

  lv_obj_t *pinHint = text(parent, "PIN", &lv_font_montserrat_10, BLACK);
  lv_obj_set_width(pinHint, CONTENT_W);
  lv_obj_set_style_text_align(pinHint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(pinHint, LV_ALIGN_TOP_MID, 0, y + 60);

  lv_obj_t *pin = text(parent, screen.panelPin.c_str(), &lv_font_montserrat_28, BLACK);
  lv_obj_set_width(pin, CONTENT_W);
  lv_obj_set_style_text_align(pin, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(pin, LV_ALIGN_TOP_MID, 0, y + 72);

  lv_obj_t *footer = text(parent, "Short BOOT = exit", &lv_font_montserrat_10, BLACK);
  lv_obj_set_width(footer, CONTENT_W);
  lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, -14);

  lv_obj_t *warn = text(parent, "Wi-Fi on, drains fast", &lv_font_montserrat_10, BLACK);
  lv_obj_set_width(warn, CONTENT_W);
  lv_obj_set_style_text_align(warn, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(warn, LV_ALIGN_BOTTOM_MID, 0, -2);
}

void singleView(lv_obj_t *parent, const MeterScreen &screen, int index)
{
  const AccountUsage &account = screen.accounts[index];
  const int y = STATUS_BAR_H + 4;
  lv_obj_set_pos(text(parent, screen.names[index].c_str(), &lv_font_montserrat_16, BLACK), PAD, y);
  textRight(parent, formatAge(screen, account).c_str(), &lv_font_montserrat_10, BLACK, WIDTH - PAD, y + 2);

  const int y5h = y + 22;
  singleWindow(parent, y5h, screen, account, "5H", account.fiveHourPercent, account.fiveHourReset);
  singleWindow(parent, y5h + SINGLE_WINDOW_PITCH, screen, account, "7D", account.sevenDayPercent, account.sevenDayReset);
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
  case MeterView::Account1History:
    historyView(root, screen, 0);
    break;
  case MeterView::Account2:
    singleView(root, screen, 1);
    break;
  case MeterView::Account2History:
    historyView(root, screen, 1);
    break;
  case MeterView::Panel:
    panelView(root, screen);
    break;
  }

  if (screen.popupTitle)
  {
    popup(root, screen.popupTitle, screen.popupBody);
  }
}
