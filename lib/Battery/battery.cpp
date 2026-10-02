#include "battery.h"

namespace
{
constexpr int SAMPLES = 16;

struct CurvePoint
{
  uint16_t millivolts;
  uint8_t percent;
};

// Typical resting voltage of a single Li-ion cell vs remaining charge, highest first
const CurvePoint CURVE[] = {
    {4200, 100}, {4150, 95}, {4110, 90}, {4020, 80}, {3950, 70}, {3870, 60}, {3840, 50},
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
