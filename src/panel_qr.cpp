#include "panel_qr.h"

#include <stdio.h>
#include <string.h>

namespace
{
  bool isSixDigits(const char *text)
  {
    if (text == nullptr || strlen(text) != 6)
      return false;
    for (int i = 0; i < 6; i++)
      if (text[i] < '0' || text[i] > '9')
        return false;
    return true;
  }
}

bool isPanelQrLoginPath(const char *uri)
{
  if (uri == nullptr || strlen(uri) != 7 || uri[0] != '/')
    return false;
  return isSixDigits(uri + 1);
}

bool formatPanelQrPayload(const char *ip, const char *pin, char *out, size_t len)
{
  if (out == nullptr || len == 0)
    return false;
  out[0] = '\0';
  if (ip == nullptr || ip[0] == '\0')
    return false;
  const int written = isSixDigits(pin) ? snprintf(out, len, "HTTP://%s/%s", ip, pin)
                                       : snprintf(out, len, "HTTP://%s", ip);
  if (written < 0 || (size_t)written >= len)
  {
    out[0] = '\0';
    return false;
  }
  return true;
}
