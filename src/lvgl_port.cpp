#include "lvgl_port.h"

#include <Arduino.h>

namespace
{
uint32_t tickMillis()
{
  return millis();
}

void flushCb(lv_display_t *display, const lv_area_t *area, uint8_t *pxMap)
{
  Epaper *epaper = static_cast<Epaper *>(lv_display_get_user_data(display));
  const uint16_t *pixel = reinterpret_cast<const uint16_t *>(pxMap);
  for (int y = area->y1; y <= area->y2; y++)
  {
    for (int x = area->x1; x <= area->x2; x++)
    {
      // Same black/white threshold as Waveshare's port
      epaper->setPixel(x, y, *pixel++ < 0x7FFF);
    }
  }
  epaper->refresh();
  lv_display_flush_ready(display);
}
}

lv_display_t *lvglPortBegin(Epaper &epaper)
{
  lv_init();
  lv_tick_set_cb(tickMillis);

  lv_display_t *display = lv_display_create(Epaper::WIDTH, Epaper::HEIGHT);
  lv_display_set_user_data(display, &epaper);
  lv_display_set_flush_cb(display, flushCb);

  // The panel only supports full-frame refreshes
  const size_t bufferSize = Epaper::WIDTH * Epaper::HEIGHT * LV_COLOR_FORMAT_GET_SIZE(LV_COLOR_FORMAT_RGB565);
  void *buffer = ps_malloc(bufferSize);
  assert(buffer);
  lv_display_set_buffers(display, buffer, nullptr, bufferSize, LV_DISPLAY_RENDER_MODE_FULL);
  return display;
}
