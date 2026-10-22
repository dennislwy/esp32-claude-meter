#pragma once

#include <stddef.h>

// "HTTP://255.255.255.255/999999" is 29 characters, the longest payload possible.
constexpr size_t PANEL_QR_PAYLOAD_MAX = 32;

// A version 2 code at ECC MEDIUM holds 38 alphanumeric characters. Byte mode would
// hold only 26, which is why the payload is built to stay inside alphanumeric mode.
static_assert(PANEL_QR_PAYLOAD_MAX - 1 <= 38, "payload must fit a version 2 alphanumeric QR code");

// Builds the Panel view's QR payload: "HTTP://<ip>/<pin>", or "HTTP://<ip>" when pin is
// null, empty, or not six digits. The scheme is uppercase so every character stays in QR
// alphanumeric mode; a lowercase "http://" would force byte mode, overflow version 2 and
// cost the Panel view its 3-px-per-module QR. Writes an empty string and returns false
// when ip is empty or the result would not fit.
bool formatPanelQrPayload(const char *ip, const char *pin, char *out, size_t len);

// True when uri is "/" followed by exactly six digits — the shape of a QR login path,
// whether or not it matches the current PIN.
bool isPanelQrLoginPath(const char *uri);
