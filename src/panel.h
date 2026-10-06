#pragma once

#include <Arduino.h>
#include "usage_poll.h"

// LAN control panel (R8 Phase 1). STA-mode HTTP server on :80, started on
// demand by a long BOOT press. The server runs single-threaded: handlers
// execute inside panelService(), which the main loop() pumps. Entry/exit
// is the caller's responsibility — this module owns the server lifecycle
// but not the Wi-Fi lifecycle, since the main loop also uses Wi-Fi for
// polling.

struct PanelDisplay
{
  String ip;        // dotted IPv4, "" when not connected
  String hostname;  // "<name>.local" for mDNS
  String pin;       // 6-digit login PIN, regenerated each entry
};

// Action flags returned by panelTakeAction() once a handler has queued them
constexpr uint8_t PANEL_ACT_REFRESH = 0x01;         // poll Claude usage now
constexpr uint8_t PANEL_ACT_SETTINGS_SAVED = 0x02;  // re-evaluate polling after a settings POST
constexpr uint8_t PANEL_ACT_REBOOT = 0x04;          // ESP.restart() after a short delay
constexpr uint8_t PANEL_ACT_TIME_ZONE = 0x08;       // apply the new time zone, rewrite the RTC, redraw
constexpr uint8_t PANEL_ACT_ROTATION = 0x10;        // apply the new rotation with a full refresh

// Brings up the server. Caller must have Wi-Fi connected and mDNS available.
// Generates a fresh random PIN and fills `out` so the display can show it.
void panelBegin(PanelDisplay &out);

// Stops the server. Safe to call when inactive.
void panelEnd();

bool panelActive();

// Pumps HTTP handlers. Call every loop() iteration while panelActive().
void panelService();

// Millis of the most recent authenticated request. Used for inactivity exit.
uint32_t panelLastActivityMs();

// Pops any pending deferred actions set by handlers
uint8_t panelTakeAction();

// Access to the pending claude usage snapshot for /api/state. Called by main.cpp
// to let the panel read the current cache.
void panelSetUsageSource(const AccountUsage *accounts);
