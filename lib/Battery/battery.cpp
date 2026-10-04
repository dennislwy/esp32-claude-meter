#include "battery.h"

namespace
{
constexpr int SAMPLES = 16;

struct CurvePoint
{
  uint16_t millivolts;
  uint8_t percent;
};

// Resting-voltage curve for a single Li-ion cell, highest first. The top is shifted down from
// the textbook 4.20 V so a fully-charged pack reads 100 % after unplug: once the charger stops
// holding CV, surface charge drops within seconds to ~4.17 V and then sags toward 4.10 V under
// even a tiny load. 4.17 V is treated as 100 %; the lower half (where empty-warnings live) is
// unchanged. See docs/battery.md for the full rationale.
const CurvePoint CURVE[] = {
    {4170, 100}, {4120, 95}, {4080, 90}, {4000, 80}, {3950, 70}, {3870, 60}, {3840, 50},
    {3800, 40},  {3770, 30}, {3730, 20}, {3690, 10}, {3610, 5},  {3300, 0},
};
constexpr size_t CURVE_LEN = sizeof(CURVE) / sizeof(CURVE[0]);
}

Battery::Battery(int adcPin, float dividerRatio) : adcPin_(adcPin), dividerRatio_(dividerRatio) {}

uint32_t Battery::readMillivolts() const
{
  uint32_t total = 0;
  for (int i = 0; i < SAMPLES; i++)
  {
    total += analogReadMilliVolts(adcPin_);
  }
  return total / SAMPLES * dividerRatio_;
}

uint8_t Battery::percentFromMillivolts(uint32_t millivolts)
{
  if (millivolts >= CURVE[0].millivolts)
  {
    return 100;
  }
  for (size_t i = 1; i < CURVE_LEN; i++)
  {
    if (millivolts >= CURVE[i].millivolts)
    {
      // Linear interpolation between the two surrounding points
      const CurvePoint &hi = CURVE[i - 1];
      const CurvePoint &lo = CURVE[i];
      return lo.percent + (millivolts - lo.millivolts) * (hi.percent - lo.percent) / (hi.millivolts - lo.millivolts);
    }
  }
  return 0;
}
