#pragma once

#include <Arduino.h>

// AP-mode captive portal for first-time Wi-Fi provisioning.
// Entered automatically on boot when no SSID is stored in NVS. Opens an
// open SoftAP named "claude-meter-XXXXXX" at 192.168.4.1, runs a DNS
// hijack so any phone/laptop triggers its captive-portal popup, and serves
// a minimal SSID+password page. On save the creds are written to NVS and
// the device reboots into normal STA mode.

struct ProvisionInfo
{
  String apSsid; // "claude-meter-XXXXXX"
  String apIp;   // "192.168.4.1"
};

// Brings up SoftAP + DNS + HTTP server. Fills `out` for the ePaper to show.
void provisionBegin(ProvisionInfo &out);

// Pumps DNS + HTTP handlers. Call every loop() iteration while active.
void provisionService();

// True once the user has saved valid-looking creds. Caller should reboot.
bool provisionShouldReboot();

// Tears everything down. Safe to call when inactive.
void provisionEnd();
