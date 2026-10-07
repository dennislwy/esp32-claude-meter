// Host regression: compile this with src/hostname.cpp (no board required).
#include "hostname.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main()
{
  char out[HOSTNAME_LIMIT + 1];

  // The default name round-trips unchanged.
  assert(hostnameNormalize("claude-meter", out));
  assert(strcmp(out, "claude-meter") == 0);

  // Length bounds: one character and exactly the limit are fine, one more is not.
  assert(hostnameNormalize("a", out));
  assert(strcmp(out, "a") == 0);
  char longest[HOSTNAME_LIMIT + 1];
  memset(longest, 'a', HOSTNAME_LIMIT);
  longest[HOSTNAME_LIMIT] = '\0';
  assert(hostnameNormalize(longest, out));
  assert(strlen(out) == HOSTNAME_LIMIT);
  char tooLong[HOSTNAME_LIMIT + 2];
  memset(tooLong, 'a', HOSTNAME_LIMIT + 1);
  tooLong[HOSTNAME_LIMIT + 1] = '\0';
  assert(!hostnameNormalize(tooLong, out));

  // Nothing to name.
  assert(!hostnameNormalize("", out));
  assert(!hostnameNormalize(nullptr, out));

  // Every allowed character, one at a time.
  for (char c = 'a'; c <= 'z'; c++)
  {
    const char one[2] = {c, '\0'};
    assert(hostnameNormalize(one, out));
  }
  for (char c = '0'; c <= '9'; c++)
  {
    const char one[2] = {c, '\0'};
    assert(hostnameNormalize(one, out));
  }

  // Rejected characters, including a high-bit byte from UTF-8 input.
  const char *const bad[] = {"my meter", "my_meter", "my.meter", "my/meter",
                             "a:b", "meter!", "caf\xc3\xa9"};
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++)
  {
    assert(!hostnameNormalize(bad[i], out));
  }

  // Hyphens may separate but not bracket.
  assert(!hostnameNormalize("-abc", out));
  assert(!hostnameNormalize("abc-", out));
  assert(!hostnameNormalize("-", out));
  assert(hostnameNormalize("a-b", out));
  assert(hostnameNormalize("a--b", out));

  // mDNS labels are case-insensitive; store one form.
  assert(hostnameNormalize("Claude-Meter", out));
  assert(strcmp(out, "claude-meter") == 0);
  assert(hostnameNormalize("STUDIO1", out));
  assert(strcmp(out, "studio1") == 0);

  // A rejected value must not corrupt the caller's buffer.
  memcpy(out, "keepme", 7);
  assert(!hostnameNormalize("bad name", out));
  assert(strcmp(out, "keepme") == 0);
  assert(!hostnameNormalize(tooLong, out));
  assert(strcmp(out, "keepme") == 0);

  printf("hostname checks passed\n");
  return 0;
}
