// Host regression: compile this with src/panel_qr.cpp (no board required).
#include "panel_qr.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef NDEBUG
#error "this suite asserts; NDEBUG would compile every check away"
#endif

namespace
{
  const char *payload(const char *ip, const char *pin)
  {
    static char buf[PANEL_QR_PAYLOAD_MAX];
    if (!formatPanelQrPayload(ip, pin, buf, sizeof(buf)))
      return "";
    return buf;
  }

  // True when every character is encodable in QR alphanumeric mode. This table is the QR
  // alphanumeric set verbatim; anything outside it drops the encoder to byte mode, which
  // holds 26 bytes at version 2 against alphanumeric's 38 characters. It lives here rather
  // than in src/ because only the assertions below ever need to ask.
  bool isQrAlphanumeric(const char *text)
  {
    if (text == nullptr)
      return false;
    static const char *const ALLOWED = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
    for (const char *c = text; *c != '\0'; c++)
      if (strchr(ALLOWED, *c) == nullptr)
        return false;
    return true;
  }
}

int main()
{
  // The PIN rides in a path segment, so a scan can be traded for a session
  assert(strcmp(payload("192.168.1.194", "465657"), "HTTP://192.168.1.194/465657") == 0);

  // Uppercase scheme: a lowercase one is outside QR alphanumeric mode
  assert(strncmp(payload("10.0.0.5", "000001"), "HTTP://", 7) == 0);

  // No usable PIN degrades to a navigate-only URL rather than a broken one
  assert(strcmp(payload("10.0.0.5", nullptr), "HTTP://10.0.0.5") == 0);
  assert(strcmp(payload("10.0.0.5", ""), "HTTP://10.0.0.5") == 0);
  assert(strcmp(payload("10.0.0.5", "12345"), "HTTP://10.0.0.5") == 0);
  assert(strcmp(payload("10.0.0.5", "1234567"), "HTTP://10.0.0.5") == 0);
  assert(strcmp(payload("10.0.0.5", "12345a"), "HTTP://10.0.0.5") == 0);

  // No IP means nothing to encode; the caller draws no QR at all
  char buf[PANEL_QR_PAYLOAD_MAX];
  assert(!formatPanelQrPayload("", "465657", buf, sizeof(buf)));
  assert(buf[0] == '\0');
  assert(!formatPanelQrPayload(nullptr, "465657", buf, sizeof(buf)));
  assert(buf[0] == '\0');

  // A short buffer fails rather than truncating into a wrong URL
  char tight[16];
  assert(!formatPanelQrPayload("192.168.1.194", "465657", tight, sizeof(tight)));
  assert(tight[0] == '\0');

  // Staying inside alphanumeric mode is what keeps the code at version 2 / 25 modules,
  // and so the Panel view's 75 px QR at 3 px per module
  assert(isQrAlphanumeric(payload("192.168.1.194", "465657")));
  assert(isQrAlphanumeric(payload("255.255.255.255", "999999")));
  assert(isQrAlphanumeric(payload("10.0.0.5", nullptr)));
  assert(isQrAlphanumeric("HTTP://10.0.0.5/$%*+-./:"));
  assert(!isQrAlphanumeric("http://192.168.1.194/465657")); // lowercase
  assert(!isQrAlphanumeric("HTTP://10.0.0.5/?p=465657"));   // '?' and '='
  assert(!isQrAlphanumeric(nullptr));

  // Version 2 at ECC MEDIUM holds 38 alphanumeric characters; the longest payload is 29
  assert(strlen(payload("255.255.255.255", "999999")) == 29);

  // The 404 throttle guard must catch the shape of a login path, PIN match or not
  assert(isPanelQrLoginPath("/465657"));
  assert(isPanelQrLoginPath("/000000"));
  assert(!isPanelQrLoginPath("/46565"));   // five digits
  assert(!isPanelQrLoginPath("/4656578")); // seven digits
  assert(!isPanelQrLoginPath("/46565a"));
  assert(!isPanelQrLoginPath("/api/state"));
  assert(!isPanelQrLoginPath("465657")); // no leading slash
  assert(!isPanelQrLoginPath("/"));
  assert(!isPanelQrLoginPath(""));
  assert(!isPanelQrLoginPath(nullptr));

  printf("panel_qr: all checks passed\n");
  return 0;
}
