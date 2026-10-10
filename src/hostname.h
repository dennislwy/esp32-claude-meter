#pragma once

#include <stddef.h>

// mDNS label rules for the device hostname, kept separate from settings so the
// host test in test/hostname can compile them without the Arduino core.
constexpr size_t HOSTNAME_LIMIT = 15;

// Lowercases `in` into `out` (capacity HOSTNAME_LIMIT + 1) when it is a valid
// mDNS label: 1 to HOSTNAME_LIMIT characters of a-z, 0-9 and '-', with no
// leading or trailing '-'. Returns false and leaves `out` untouched otherwise;
// a null or empty `in` is not valid.
bool hostnameNormalize(const char *in, char *out);
