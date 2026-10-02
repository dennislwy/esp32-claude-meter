#pragma once

#include <lvgl.h>
#include "epaper.h"

// Registers the e-paper panel as LVGL's display. LVGL renders full RGB565 frames
// into PSRAM; each flush is thresholded to black/white and partial-refreshed.
// Call lv_timer_handler() regularly from loop() afterwards.
lv_display_t *lvglPortBegin(Epaper &epaper);
