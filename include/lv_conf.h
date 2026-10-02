// LVGL 9 configuration. Anything not set here uses LVGL's default from lv_conf_internal.h.
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

// LVGL runs only from Arduino's loop(), so it needs no OS locking
#define LV_USE_OS LV_OS_NONE

#define LV_MEM_SIZE (64 * 1024U)

// Meter UI fonts (14 is LVGL's default and is on already)
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_28 1

#endif
