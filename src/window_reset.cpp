#include "window_reset.h"

namespace
{
// Utilisation arrives as a 0-1 fraction with two decimals, so real changes land in
// 1-point steps. Half a point sits below every real change and above any float noise
// from the multiply by 100.
constexpr float DROP_MARGIN = 0.5f;
}

bool isEarlyWindowReset(bool prevSeen, float prevPercent, float percent, bool rolledOver)
{
  if (!prevSeen || rolledOver)
  {
    return false;
  }
  return percent < prevPercent - DROP_MARGIN;
}
