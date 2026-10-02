#include "es8311.h"

namespace
{
constexpr uint8_t REG_RESET = 0x00;
constexpr uint8_t REG_CLK_MANAGER_01 = 0x01;
constexpr uint8_t REG_CLK_MANAGER_02 = 0x02;
constexpr uint8_t REG_CLK_MANAGER_03 = 0x03;
constexpr uint8_t REG_CLK_MANAGER_04 = 0x04;
constexpr uint8_t REG_CLK_MANAGER_05 = 0x05;
constexpr uint8_t REG_CLK_MANAGER_06 = 0x06;
constexpr uint8_t REG_CLK_MANAGER_07 = 0x07;
constexpr uint8_t REG_CLK_MANAGER_08 = 0x08;
constexpr uint8_t REG_SDP_IN = 0x09;  // DAC serial port
constexpr uint8_t REG_SDP_OUT = 0x0A; // ADC serial port
constexpr uint8_t REG_SYSTEM_0B = 0x0B;
constexpr uint8_t REG_SYSTEM_0C = 0x0C;
constexpr uint8_t REG_SYSTEM_0D = 0x0D;
constexpr uint8_t REG_SYSTEM_0E = 0x0E;
constexpr uint8_t REG_SYSTEM_10 = 0x10;
constexpr uint8_t REG_SYSTEM_11 = 0x11;
constexpr uint8_t REG_SYSTEM_12 = 0x12;
constexpr uint8_t REG_SYSTEM_13 = 0x13;
constexpr uint8_t REG_SYSTEM_14 = 0x14;
constexpr uint8_t REG_ADC_15 = 0x15;
constexpr uint8_t REG_ADC_16 = 0x16;
constexpr uint8_t REG_ADC_17 = 0x17;
constexpr uint8_t REG_ADC_1B = 0x1B;
constexpr uint8_t REG_ADC_1C = 0x1C;
constexpr uint8_t REG_DAC_MUTE = 0x31;
constexpr uint8_t REG_DAC_VOLUME = 0x32;
constexpr uint8_t REG_DAC_RAMP = 0x37;
constexpr uint8_t REG_GPIO_44 = 0x44;
constexpr uint8_t REG_GP_45 = 0x45;
constexpr uint8_t REG_CHIP_ID1 = 0xFD;
constexpr uint8_t REG_CHIP_ID2 = 0xFE;
}

Es8311::Es8311(TwoWire &wire, uint8_t address) : wire_(wire), address_(address) {}

bool Es8311::begin()
{
  // Improves I2C noise immunity; the first write after power-up sometimes fails, so write twice
  writeRegister(REG_GPIO_44, 0x08);
  if (!writeRegister(REG_GPIO_44, 0x08))
  {
    return false;
  }
  if (readRegister(REG_CHIP_ID1) != 0x83 || readRegister(REG_CHIP_ID2) != 0x11)
  {
    return false;
  }

  writeRegister(REG_CLK_MANAGER_01, 0x30);
  writeRegister(REG_CLK_MANAGER_02, 0x00);
  writeRegister(REG_CLK_MANAGER_03, 0x10);
  writeRegister(REG_ADC_16, 0x24);
  writeRegister(REG_CLK_MANAGER_04, 0x10);
  writeRegister(REG_CLK_MANAGER_05, 0x00);
  writeRegister(REG_SYSTEM_0B, 0x00);
  writeRegister(REG_SYSTEM_0C, 0x00);
  writeRegister(REG_SYSTEM_10, 0x1F);
  writeRegister(REG_SYSTEM_11, 0x7F);
  writeRegister(REG_RESET, 0x80); // power on, slave mode
  writeRegister(REG_CLK_MANAGER_01, 0x3F); // clocks on, from the MCLK pin, not inverted
  writeRegister(REG_CLK_MANAGER_06, readRegister(REG_CLK_MANAGER_06) & ~0x20); // BCLK not inverted
  writeRegister(REG_SYSTEM_13, 0x10);
  writeRegister(REG_ADC_1B, 0x0A);
  writeRegister(REG_ADC_1C, 0x6A);
  writeRegister(REG_GPIO_44, 0x58);
  return true;
}

bool Es8311::start(uint32_t sampleRate)
{
  // 16-bit standard I2S
  writeRegister(REG_SDP_IN, (readRegister(REG_SDP_IN) & 0xFC) | 0x0C);
  writeRegister(REG_SDP_OUT, (readRegister(REG_SDP_OUT) & 0xFC) | 0x0C);

  // Clock tree for MCLK = 256 x fs: no pre-divider or multiplier, LRCK = MCLK / 256, BCLK = MCLK / 4
  writeRegister(REG_CLK_MANAGER_02, readRegister(REG_CLK_MANAGER_02) & 0x07);
  writeRegister(REG_CLK_MANAGER_05, 0x00);
  writeRegister(REG_CLK_MANAGER_03, (readRegister(REG_CLK_MANAGER_03) & 0x80) | 0x10);
  const uint8_t dacOsr = sampleRate <= 16000 ? 0x20 : 0x10;
  writeRegister(REG_CLK_MANAGER_04, (readRegister(REG_CLK_MANAGER_04) & 0x80) | dacOsr);
  writeRegister(REG_CLK_MANAGER_07, readRegister(REG_CLK_MANAGER_07) & 0xC0);
  writeRegister(REG_CLK_MANAGER_08, 0xFF);
  writeRegister(REG_CLK_MANAGER_06, (readRegister(REG_CLK_MANAGER_06) & 0xE0) | 0x03);

  writeRegister(REG_RESET, 0x80);
  writeRegister(REG_CLK_MANAGER_01, 0x3F);
  // Enable the DAC serial input, leave the ADC output disabled
  writeRegister(REG_SDP_IN, readRegister(REG_SDP_IN) & ~0x40);
  writeRegister(REG_SDP_OUT, readRegister(REG_SDP_OUT) | 0x40);

  writeRegister(REG_ADC_17, 0xBF);
  writeRegister(REG_SYSTEM_0E, 0x02);
  writeRegister(REG_SYSTEM_12, 0x00);
  writeRegister(REG_SYSTEM_14, 0x1A); // analog mic, no digital mic
  writeRegister(REG_SYSTEM_0D, 0x01);
  writeRegister(REG_ADC_15, 0x40);
  writeRegister(REG_DAC_RAMP, 0x08);
  writeRegister(REG_GP_45, 0x00);
  writeRegister(REG_DAC_MUTE, readRegister(REG_DAC_MUTE) & 0x9F);
  return true;
}

void Es8311::stop()
{
  writeRegister(REG_DAC_VOLUME, 0x00);
  writeRegister(REG_ADC_17, 0x00);
  writeRegister(REG_SYSTEM_0E, 0xFF);
  writeRegister(REG_SYSTEM_12, 0x02);
  writeRegister(REG_SYSTEM_14, 0x00);
  writeRegister(REG_SYSTEM_0D, 0xFA);
  writeRegister(REG_ADC_15, 0x00);
  writeRegister(REG_CLK_MANAGER_02, 0x10);
  writeRegister(REG_RESET, 0x00);
  writeRegister(REG_RESET, 0x1F);
  writeRegister(REG_CLK_MANAGER_01, 0x30);
  writeRegister(REG_CLK_MANAGER_01, 0x00);
  writeRegister(REG_GP_45, 0x00);
  writeRegister(REG_SYSTEM_0D, 0xFC);
  writeRegister(REG_CLK_MANAGER_02, 0x00);
}

void Es8311::setVolume(float db)
{
  // 0x00 = -95.5 dB, 0.5 dB per step, 0xBF = 0 dB
  const float steps = (db + 95.5f) * 2.0f;
  writeRegister(REG_DAC_VOLUME, steps <= 0 ? 0 : steps >= 255 ? 255 : (uint8_t)(steps + 0.5f));
}

bool Es8311::writeRegister(uint8_t reg, uint8_t value)
{
  wire_.beginTransmission(address_);
  wire_.write(reg);
  wire_.write(value);
  return wire_.endTransmission() == 0;
}

uint8_t Es8311::readRegister(uint8_t reg)
{
  wire_.beginTransmission(address_);
  wire_.write(reg);
  if (wire_.endTransmission(false) != 0 || wire_.requestFrom(address_, (uint8_t)1) != 1)
  {
    return 0;
  }
  return wire_.read();
}
