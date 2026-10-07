#include "hostname.h"

namespace
{
bool allowed(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
}
}

bool hostnameNormalize(const char *in, char *out)
{
  if (!in)
  {
    return false;
  }
  // Build into a local buffer so a rejected name leaves `out` as the caller had it
  char buffer[HOSTNAME_LIMIT + 1];
  size_t length = 0;
  for (; in[length] != '\0'; length++)
  {
    if (length >= HOSTNAME_LIMIT)
    {
      return false;
    }
    char c = in[length];
    if (c >= 'A' && c <= 'Z')
    {
      c = (char)(c - 'A' + 'a');
    }
    if (!allowed(c))
    {
      return false;
    }
    buffer[length] = c;
  }
  if (length == 0 || buffer[0] == '-' || buffer[length - 1] == '-')
  {
    return false;
  }
  buffer[length] = '\0';
  for (size_t i = 0; i <= length; i++)
  {
    out[i] = buffer[i];
  }
  return true;
}
