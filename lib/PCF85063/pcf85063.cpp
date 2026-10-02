#include "pcf85063.h"

namespace
{
constexpr uint8_t REG_CONTROL_1 = 0x00;
constexpr uint8_t REG_SECONDS = 0x04; // seconds..years follow in 0x05-0x0A

constexpr uint8_t CONTROL_1_STOP = 0x20;
constexpr uint8_t CONTROL_1_12_24 = 0x02;
constexpr uint8_t SECONDS_OS = 0x80; // oscillator stopped

uint8_t toBcd(uint8_t value)
{
  return ((value / 10) << 4) | (value % 10);
}

uint8_t fromBcd(uint8_t bcd)
{
  return (bcd >> 4) * 10 + (bcd & 0x0F);
}

// Sakamoto's algorithm, 0 = Sunday
uint8_t weekdayOf(uint16_t year, uint8_t month, uint8_t day)
{
  static const uint8_t offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3)
  {
    year--;
  }
  return (year + year / 4 - year / 100 + year / 400 + offsets[month - 1] + day) % 7;
}
}

Pcf85063::Pcf85063(TwoWire &wire, uint8_t address) : wire_(wire), address_(address) {}

bool Pcf85063::begin()
{
  uint8_t control;
  if (!readRegisters(REG_CONTROL_1, &control, 1))
  {
    return false;
  }
  control &= ~(CONTROL_1_STOP | CONTROL_1_12_24);
  return writeRegisters(REG_CONTROL_1, &control, 1);
}

bool Pcf85063::read(RtcDateTime &dateTime)
{
  uint8_t buf[7];
  if (!readRegisters(REG_SECONDS, buf, sizeof(buf)))
  {
    return false;
  }
  timeValid_ = (buf[0] & SECONDS_OS) == 0;
  dateTime.second = fromBcd(buf[0] & 0x7F);
  dateTime.minute = fromBcd(buf[1] & 0x7F);
  dateTime.hour = fromBcd(buf[2] & 0x3F);
  dateTime.day = fromBcd(buf[3] & 0x3F);
  dateTime.weekday = buf[4] & 0x07;
  dateTime.month = fromBcd(buf[5] & 0x1F);
  dateTime.year = 2000 + fromBcd(buf[6]);
  return true;
}

bool Pcf85063::write(const RtcDateTime &dateTime)
{
  const uint8_t buf[7] = {
      toBcd(dateTime.second), // writing OS = 0 marks the time valid
      toBcd(dateTime.minute),
      toBcd(dateTime.hour),
      toBcd(dateTime.day),
      weekdayOf(dateTime.year, dateTime.month, dateTime.day),
      toBcd(dateTime.month),
      toBcd(dateTime.year % 100),
  };
  if (!writeRegisters(REG_SECONDS, buf, sizeof(buf)))
  {
    return false;
  }
  timeValid_ = true;
  return true;
}

bool Pcf85063::readRegisters(uint8_t reg, uint8_t *buf, uint8_t len)
{
  wire_.beginTransmission(address_);
  wire_.write(reg);
  if (wire_.endTransmission(false) != 0)
  {
    return false;
  }
  if (wire_.requestFrom(address_, len) != len)
  {
    return false;
  }
  for (uint8_t i = 0; i < len; i++)
  {
    buf[i] = wire_.read();
  }
  return true;
}

bool Pcf85063::writeRegisters(uint8_t reg, const uint8_t *buf, uint8_t len)
{
  wire_.beginTransmission(address_);
  wire_.write(reg);
  wire_.write(buf, len);
  return wire_.endTransmission() == 0;
}
