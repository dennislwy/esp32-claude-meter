#pragma once

#include <Arduino.h>
#include <Wire.h>

struct RtcDateTime
{
  uint16_t year; // 2000-2099
  uint8_t month; // 1-12
  uint8_t day;   // 1-31
  uint8_t hour;  // 0-23
  uint8_t minute;
  uint8_t second;
  uint8_t weekday; // 0 = Sunday
};

// Minimal driver for the NXP PCF85063 real-time clock, in 24-hour mode.
class Pcf85063
{
public:
  explicit Pcf85063(TwoWire &wire = Wire, uint8_t address = 0x51);

  // Starts the clock in 24-hour mode. Returns false if the RTC doesn't respond.
  bool begin();

  // Returns false on I2C error. Check timeValid() to see if the time has been set.
  bool read(RtcDateTime &dateTime);
  // Sets the time and clears the oscillator-stopped flag. The weekday is computed from the date.
  bool write(const RtcDateTime &dateTime);

  // False if the oscillator stopped (e.g. power loss) since the time was last set
  bool timeValid() const { return timeValid_; }

private:
  bool readRegisters(uint8_t reg, uint8_t *buf, uint8_t len);
  bool writeRegisters(uint8_t reg, const uint8_t *buf, uint8_t len);

  TwoWire &wire_;
  uint8_t address_;
  bool timeValid_ = false;
};
