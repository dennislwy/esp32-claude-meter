// Host regression: compile this with src/window_reset.cpp (no board required).
#include "window_reset.h"
#include <assert.h>
#include <stdio.h>

#ifdef NDEBUG
#error "this suite asserts; NDEBUG would compile every check away"
#endif

int main()
{
  // No baseline yet: the first poll after a power cycle must stay silent rather than
  // read the zero-initialised prevPercent as a real previous value.
  assert(!isEarlyWindowReset(false, 100.0f, 0.0f, false));
  assert(!isEarlyWindowReset(false, 0.0f, 0.0f, false));

  // The two drops measured on hardware 2026-10-22 when a gift reset was applied to dev1
  assert(isEarlyWindowReset(true, 100.0f, 0.0f, false)); // 5h, 100% -> 0%
  assert(isEarlyWindowReset(true, 83.0f, 0.0f, false));  // 7d, 83% -> 0%

  // Holding or rising inside a window is the normal case
  assert(!isEarlyWindowReset(true, 50.0f, 50.0f, false));
  assert(!isEarlyWindowReset(true, 50.0f, 60.0f, false));
  assert(!isEarlyWindowReset(true, 0.0f, 100.0f, false));

  // One percentage point is the smallest real change: the rate-limit headers carry a
  // 0-1 fraction with two decimals.
  assert(isEarlyWindowReset(true, 50.0f, 49.0f, false));

  // Below the margin, and under the 1-point granularity of real readings
  assert(!isEarlyWindowReset(true, 50.0f, 49.7f, false));

  // Pins the margin from above: a larger margin would miss this fall
  assert(isEarlyWindowReset(true, 50.0f, 49.4f, false));

  // The margin is exclusive: exactly half a point is not a drop
  assert(!isEarlyWindowReset(true, 50.0f, 49.5f, false));

  // A gift taken while the window already read ~0% has nothing to announce
  assert(!isEarlyWindowReset(true, 0.4f, 0.0f, false));

  // A natural rollover already chimed this pass, so the drop must not chime again
  assert(!isEarlyWindowReset(true, 100.0f, 0.0f, true));
  assert(!isEarlyWindowReset(true, 83.0f, 0.0f, true));
  assert(!isEarlyWindowReset(true, 50.0f, 50.0f, true));

  printf("window_reset: all checks passed\n");
  return 0;
}
