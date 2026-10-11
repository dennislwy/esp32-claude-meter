// Host regression: compile this with src/pin_entry.cpp (no board required).
#include "pin_entry.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main()
{
  PinEntry entry;

  // A fresh entry sits on slot 0 with every digit at zero and nothing confirmed.
  assert(entry.cursor() == 0);
  assert(!entry.complete());
  for (int i = 0; i < PIN_DIGITS; i++)
    assert(entry.digit(i) == 0);

  // BOOT presses only move the active slot, and never advance the cursor.
  entry.increment();
  entry.increment();
  entry.increment();
  assert(entry.digit(0) == 3);
  assert(entry.cursor() == 0);
  assert(entry.digit(1) == 0);

  // Ten presses wrap 9 -> 0 and land back where they started.
  for (int i = 0; i < 10; i++)
    entry.increment();
  assert(entry.digit(0) == 3);

  // PWR confirms the active digit, freezes it, and advances. Not complete yet.
  assert(!entry.confirm());
  assert(entry.cursor() == 1);
  assert(entry.digit(0) == 3);
  entry.increment();
  assert(entry.digit(1) == 1);
  assert(entry.digit(0) == 3); // the confirmed slot is untouched

  // The fourth confirm completes the entry and only then returns true.
  assert(!entry.confirm()); // slot 1 -> 2
  entry.increment();
  entry.increment();
  assert(!entry.confirm()); // slot 2 -> 3
  entry.increment();
  assert(entry.confirm()); // slot 3 -> complete
  assert(entry.complete());
  assert(entry.cursor() == PIN_DIGITS);

  // The digits read back as a NUL-terminated string.
  char out[PIN_DIGITS + 1];
  entry.digits(out);
  assert(strcmp(out, "3121") == 0);

  // Further presses are no-ops: a completed entry must not mutate while the
  // caller is deriving a key from it.
  entry.increment();
  entry.confirm();
  entry.digits(out);
  assert(strcmp(out, "3121") == 0);
  assert(entry.cursor() == PIN_DIGITS);

  // reset() returns it to the fresh state, which is what a wrong PIN does.
  entry.reset();
  assert(entry.cursor() == 0);
  assert(!entry.complete());
  for (int i = 0; i < PIN_DIGITS; i++)
    assert(entry.digit(i) == 0);

  // An all-zero PIN is legitimate and must round-trip.
  for (int i = 0; i < PIN_DIGITS; i++)
    assert(entry.confirm() == (i == PIN_DIGITS - 1));
  entry.digits(out);
  assert(strcmp(out, "0000") == 0);

  // Out-of-range slots read zero rather than reading past the array.
  assert(entry.digit(-1) == 0);
  assert(entry.digit(PIN_DIGITS) == 0);

  printf("pin_entry: all checks passed\n");
  return 0;
}
