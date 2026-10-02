#pragma once

#include <time.h>
#include "pcf85063.h"

// System time (time()) is what countdowns are computed from. The PCF85063 keeps it across
// deep sleep and power loss (the ESP32's own sleep timer drifts); NTP corrects the PCF85063.
// The PCF85063 holds local time.

// Sets the time zone and loads system time from the RTC. Call on every boot and wake.
void clockBegin(Pcf85063 &rtc);

// True once system time has come from a valid RTC or NTP
bool clockValid();

// Syncs system time over NTP (Wi-Fi must be up) and writes it to the RTC. Returns false on timeout.
bool clockSyncNtp(Pcf85063 &rtc);

// Writes the current system time to the RTC, e.g. after setting it by hand.
void clockSaveToRtc(Pcf85063 &rtc);
