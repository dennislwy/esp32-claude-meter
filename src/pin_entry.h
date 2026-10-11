#pragma once

#include <stdint.h>

constexpr int PIN_DIGITS = 4;

// Four-slot Device PIN entry driven by the two physical buttons: BOOT
// increments the active digit, PWR confirms it and advances. Holds no key and
// calls no crypto — the caller reads the finished digits and hands them to
// secret_store. Keeping it free of hardware is what makes it host-testable.
class PinEntry
{
public:
  // Back to slot 0 with every digit zero. Used on entry and after a wrong PIN.
  void reset();
  // BOOT short press: 0-9 on the active slot, wrapping 9 -> 0.
  void increment();
  // PWR short press: freeze the active digit and advance. Returns true only on
  // the confirm that completes the fourth slot.
  bool confirm();
  bool complete() const;
  // 0 to PIN_DIGITS-1 while entering, PIN_DIGITS once complete.
  int cursor() const;
  // 0-9, or 0 for a slot outside the range.
  uint8_t digit(int slot) const;
  // Writes PIN_DIGITS characters plus a NUL. Only meaningful once complete().
  void digits(char *out) const;

private:
  uint8_t values[PIN_DIGITS] = {0, 0, 0, 0};
  int active = 0;
};
