#pragma once
#include <atomic>
#include <stdint.h>
constexpr uint8_t WL_CONNECTED = 3;
struct TestWifi
{
  std::atomic<uint8_t> connection{WL_CONNECTED};
  uint8_t status() const { return connection.load(); }
  int8_t RSSI() const { return -50; }
};
extern TestWifi WiFi;
