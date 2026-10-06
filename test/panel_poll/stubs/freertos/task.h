#pragma once
#include "FreeRTOS.h"
#include <stdint.h>
BaseType_t xTaskCreate(void (*entry)(void *), const char *, uint32_t, void *, unsigned, void *);
void vTaskDelete(void *);
