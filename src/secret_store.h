#pragma once

#include <Arduino.h>
#include "pin_entry.h"

// The Wi-Fi password and both Claude OAuth tokens, sealed in NVS with
// AES-256-GCM under a key derived from the 4-digit Device PIN. The key lives
// only in RTC RAM: it survives deep sleep and a software reset, and is cleared
// by a power-on reset, so a cold boot or a battery pull needs the PIN again.
//
// This is the only module that touches the sealed keys. Nothing else derives,
// seals, or opens anything.
namespace secret_store
{
// PBKDF2-HMAC-SHA256 iterations, measured on hardware to land near 400 ms.
// SHA-256 runs unaccelerated here, so a round costs about 97 us. "selftest"
// prints the measured time; the value is frozen once measured, because
// changing it invalidates every stored blob.
constexpr uint32_t PBKDF2_ROUNDS = 4100;
constexpr size_t SALT_LEN = 16;
constexpr size_t KEY_LEN = 32;
constexpr size_t NONCE_LEN = 12;
constexpr size_t TAG_LEN = 16;
// Bumping this is the one forward-compatible escape hatch. It costs one byte
// and avoids an unrecoverable format migration.
constexpr uint8_t RECORD_VERSION = 1;
// An OAuth token runs to roughly 200 characters; 256 leaves headroom and keeps
// every buffer on the stack.
constexpr size_t PLAIN_MAX = 256;
// [version:1][nonce:12][ciphertext:N][tag:16]
constexpr size_t RECORD_MAX = 1 + NONCE_LEN + PLAIN_MAX + TAG_LEN;

enum class Slot : uint8_t
{
  WifiPassword = 0,
  ClaudeToken1 = 1,
  ClaudeToken2 = 2,
};
constexpr int SLOT_COUNT = 3;

// Exactly PIN_DIGITS decimal digits. The single definition of a valid PIN,
// shared by provisioning, the panel, and unlock.
bool validPin(const char *pin);

// No session key held. This is the complete definition of "locked": every
// sealed read returns "" and every sealed write is refused until unlock().
bool locked();

// Provisioned before this firmware: an SSID is stored but no generation
// marker, so the blobs are pre-PIN plaintext. The caller wipes and
// re-provisions.
bool legacy();

// Derive from pin and check it against the sealed Wi-Fi password's GCM tag.
// Keeps the key in RTC RAM on success. Takes roughly PBKDF2_ROUNDS' worth of
// time whether the PIN is right or wrong.
bool unlock(const char *pin);

// First seal: fresh salt, derive, seal the password and two empty tokens into
// the inactive generation, then commit. Leaves the device unlocked.
// The radio must be up, because esp_random() is only truly random then.
bool provision(const char *pin, const String &wifiPassword);

// Re-seal every secret under a new PIN and commit the flip. Requires
// !locked() and the radio up. An interruption leaves the old PIN working.
bool changePin(const char *newPin);

// "" when locked, or when the blob is missing or fails its tag.
String read(Slot slot);
// Silently ignored when locked, which is why callers gate on locked() first.
void write(Slot slot, const String &value);

// Erase every credential and the generation marker, including the pre-PIN
// plaintext keys, the SSID and the account names. Settings and history
// survive. Serves both the legacy migration and the forgotten-PIN reset.
void wipeCredentials();

// Serial "selftest": derivation timing, the seal/open round-trip, wrong-key
// and tamper rejection, key naming, and the A/B generation commit. Prints
// each result and returns true when all passed. Touches no live credential.
bool selfTest();
}
