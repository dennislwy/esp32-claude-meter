#pragma once

#include <Arduino.h>
#include <Wire.h>

// Minimal driver for the Everest ES8311 audio codec, playback (DAC) only.
// The codec runs as I2S slave with MCLK = 256 x sample rate supplied by the host.
// Register sequences follow Espressif's esp_codec_dev es8311 driver.
class Es8311
{
public:
  explicit Es8311(TwoWire &wire = Wire, uint8_t address = 0x18);

  // Checks the chip ID and applies the base configuration. Returns false if the codec doesn't respond.
  // MCLK must already be running.
  bool begin();

  // 16-bit I2S at the given sample rate, then powers up the DAC path.
  bool start(uint32_t sampleRate);
  // Powers the codec back down.
  void stop();

  // DAC volume in dB, from -95.5 (mute) to +32 in 0.5 dB steps
  void setVolume(float db);

private:
  bool writeRegister(uint8_t reg, uint8_t value);
  uint8_t readRegister(uint8_t reg);

  TwoWire &wire_;
  uint8_t address_;
};
