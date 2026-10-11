#include "pin_entry.h"

void PinEntry::reset()
{
  for (int i = 0; i < PIN_DIGITS; i++)
  {
    values[i] = 0;
  }
  active = 0;
}

void PinEntry::increment()
{
  if (complete())
  {
    return;
  }
  values[active] = (uint8_t)((values[active] + 1) % 10);
}

bool PinEntry::confirm()
{
  if (complete())
  {
    return false;
  }
  active++;
  return complete();
}

bool PinEntry::complete() const
{
  return active >= PIN_DIGITS;
}

int PinEntry::cursor() const
{
  return active;
}

uint8_t PinEntry::digit(int slot) const
{
  return slot >= 0 && slot < PIN_DIGITS ? values[slot] : 0;
}

void PinEntry::digits(char *out) const
{
  for (int i = 0; i < PIN_DIGITS; i++)
  {
    out[i] = (char)('0' + values[i]);
  }
  out[PIN_DIGITS] = '\0';
}
