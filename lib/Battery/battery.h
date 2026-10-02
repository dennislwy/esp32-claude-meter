#pragma once

#include <Arduino.h>

// Single-cell Li-ion battery gauge from an ADC pin behind a resistor divider.
class Battery
{
public:
  // dividerRatio: battery voltage / ADC pin voltage
  Battery(int adcPin, float dividerRatio);

  // Averaged battery voltage in millivolts
  uint32_t readMillivolts() const;

  // Estimated charge (0-100) from a typical Li-ion discharge curve. Only meaningful
  // while discharging: on USB the charger holds the voltage near 4.2 V.
  static uint8_t percentFromMillivolts(uint32_t millivolts);

private:
  int adcPin_;
  float dividerRatio_;
};
