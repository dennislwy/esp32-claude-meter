#pragma once

#include <lvgl.h>
#include "epaper.h"

// Registers the e-paper panel as LVGL's display. LVGL renders full RGB565 frames
// into PSRAM; each flush is thresholded to black/white and partial-refreshed.
// Call lv_timer_handler() regularly from loop() afterwards.
lv_display_t *lvglPortBegin(Epaper &epaper);

// Rotates what LVGL draws by quarter turns clockwise (0-3) as it's copied to the panel. The panel
// is square, so layouts need no change. Takes effect on the next flush.
void lvglPortSetRotation(uint8_t quarterTurns);
