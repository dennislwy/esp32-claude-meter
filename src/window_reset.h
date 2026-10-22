#pragma once

// A usage window's utilisation is cumulative, so it can only fall when the window was
// replaced. A gifted usage-limit reset does exactly that. Measured 2026-10-22: a gift
// moved the 5h reset time from 16:20 to 20:30 but left the 7d reset time unchanged, so
// a reset-timestamp test would miss every 7d gift and the fall is the only signal that
// covers both windows.
//
// prevSeen is false until a poll has been evaluated since power-on. 0.0f cannot serve as
// the "nothing recorded yet" sentinel, because 0% is a legitimate reading.
//
// rolledOver is true when the caller has already announced this window's natural reset in
// this pass. It suppresses the result so one reset never chimes twice.
bool isEarlyWindowReset(bool prevSeen, float prevPercent, float percent, bool rolledOver);
