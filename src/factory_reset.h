#pragma once

// The one reset the device has: the panel's Factory reset, serial "reset
// confirm", and BOOT + PWR held at the PIN prompt all end here. Erases every
// credential, the Device PIN, all settings, alert state and history, and zeroes
// the session key in RTC RAM. Sound files on LittleFS are kept. The caller
// restarts the board, which then comes up in Wi-Fi setup.
void factoryReset();
