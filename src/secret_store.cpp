#include "secret_store.h"

#include <Preferences.h>
#include <esp_random.h>
#include <string.h>

#include <mbedtls/gcm.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>

// Retained across deep sleep and a software reset, re-initialised by a
// power-on reset. The software-reset half is load-bearing: provisioning ends
// in ESP.restart(), so without it the user would set a PIN and be challenged
// for it seconds later.
RTC_DATA_ATTR static uint8_t rtcSessionKey[secret_store::KEY_LEN];
RTC_DATA_ATTR static bool rtcKeyValid = false;

namespace
{
using secret_store::KEY_LEN;
using secret_store::NONCE_LEN;
using secret_store::PLAIN_MAX;
using secret_store::RECORD_MAX;
using secret_store::RECORD_VERSION;
using secret_store::SALT_LEN;
using secret_store::Slot;
using secret_store::SLOT_COUNT;
using secret_store::TAG_LEN;

const char *const LIVE_NS = "meter";
// Used only by selfTest(), so a failing check can never cost a real token.
const char *const SCRATCH_NS = "sstest";
const char *const KEY_GENERATION = "pin_gen";
const char *const KEY_WIFI_SSID = "wifi_ssid";
const char *const KEY_ACCOUNT_NAMES[] = {"account1", "account2"};
// Pre-PIN keys. Never written again; wipeCredentials() removes them.
const char *const LEGACY_KEYS[] = {"wifi_pass", "claude_token", "claude_token2"};
// One format per Slot, in Slot order. %d is the generation, so the longest
// name is "wifi_pass0" at 10 characters — inside NVS's 15.
const char *const SLOT_KEY_FORMATS[] = {"wifi_pass%d", "ctok1_%d", "ctok2_%d"};
static_assert(sizeof(SLOT_KEY_FORMATS) / sizeof(SLOT_KEY_FORMATS[0]) == SLOT_COUNT,
              "one key format per slot");
// Longest key is "wifi_pass0"; 16 covers it with the NUL.
constexpr size_t KEY_NAME_MAX = 16;

void slotKey(Slot slot, uint8_t generation, char *out, size_t len)
{
  snprintf(out, len, SLOT_KEY_FORMATS[(int)slot], (int)generation);
}

void saltKey(uint8_t generation, char *out, size_t len)
{
  snprintf(out, len, "pin_salt%d", (int)generation);
}

bool hasGeneration(const char *ns)
{
  Preferences prefs;
  prefs.begin(ns, false);
  const bool present = prefs.isKey(KEY_GENERATION);
  prefs.end();
  return present;
}

// Masked to one bit: a corrupted marker selects a generation rather than
// indexing out of range.
uint8_t liveGeneration(const char *ns)
{
  Preferences prefs;
  prefs.begin(ns, false);
  const uint8_t generation = prefs.isKey(KEY_GENERATION) ? prefs.getUChar(KEY_GENERATION, 0) : 0;
  prefs.end();
  return generation & 1;
}

// The single one-byte write that makes a generation live.
bool commitGeneration(const char *ns, uint8_t generation)
{
  Preferences prefs;
  prefs.begin(ns, false);
  const bool ok = prefs.putUChar(KEY_GENERATION, generation) == 1;
  prefs.end();
  return ok;
}

bool deriveKey(const char *pin, const uint8_t *salt, uint8_t *keyOut)
{
  mbedtls_md_context_t md;
  mbedtls_md_init(&md);
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  // The 1 selects HMAC mode, which is what PBKDF2 needs. mbedtls 2.28 has no
  // _ext entry point, so the context has to be set up by hand.
  bool ok = info != nullptr && mbedtls_md_setup(&md, info, 1) == 0;
  if (ok)
  {
    ok = mbedtls_pkcs5_pbkdf2_hmac(&md, (const unsigned char *)pin, strlen(pin),
                                   salt, SALT_LEN, secret_store::PBKDF2_ROUNDS,
                                   KEY_LEN, keyOut) == 0;
  }
  mbedtls_md_free(&md);
  return ok;
}

bool sealRecord(const uint8_t *key, const String &plain, uint8_t *out, size_t &outLen)
{
  const size_t n = plain.length();
  if (n > PLAIN_MAX)
  {
    return false;
  }
  out[0] = RECORD_VERSION;
  esp_fill_random(out + 1, NONCE_LEN);
  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  // The version byte and nonce are authenticated as additional data, so
  // neither can be swapped without failing the tag.
  const bool ok = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, KEY_LEN * 8) == 0 &&
                  mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, n,
                                            out + 1, NONCE_LEN,
                                            out, 1 + NONCE_LEN,
                                            (const unsigned char *)plain.c_str(),
                                            out + 1 + NONCE_LEN,
                                            TAG_LEN,
                                            out + 1 + NONCE_LEN + n) == 0;
  mbedtls_gcm_free(&gcm);
  outLen = 1 + NONCE_LEN + n + TAG_LEN;
  return ok;
}

bool openRecord(const uint8_t *key, const uint8_t *record, size_t len, String &out)
{
  constexpr size_t OVERHEAD = 1 + NONCE_LEN + TAG_LEN;
  if (len < OVERHEAD || len > OVERHEAD + PLAIN_MAX || record[0] != RECORD_VERSION)
  {
    return false;
  }
  const size_t n = len - OVERHEAD;
  char plain[PLAIN_MAX + 1];
  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  // mbedtls 2.28 puts tag before input/output here, unlike crypt_and_tag.
  const bool ok = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, KEY_LEN * 8) == 0 &&
                  mbedtls_gcm_auth_decrypt(&gcm, n,
                                           record + 1, NONCE_LEN,
                                           record, 1 + NONCE_LEN,
                                           record + 1 + NONCE_LEN + n, TAG_LEN,
                                           record + 1 + NONCE_LEN,
                                           (unsigned char *)plain) == 0;
  mbedtls_gcm_free(&gcm);
  if (ok)
  {
    plain[n] = '\0';
    out = String(plain);
  }
  memset(plain, 0, sizeof(plain));
  return ok;
}

bool loadBlob(const char *ns, const char *key, uint8_t *out, size_t &len)
{
  Preferences prefs;
  prefs.begin(ns, false);
  const size_t stored = prefs.isKey(key) ? prefs.getBytesLength(key) : 0;
  bool ok = stored > 0 && stored <= RECORD_MAX;
  if (ok)
  {
    len = prefs.getBytes(key, out, stored);
    ok = len == stored;
  }
  prefs.end();
  return ok;
}

bool storeBlob(const char *ns, const char *key, const uint8_t *data, size_t len)
{
  Preferences prefs;
  prefs.begin(ns, false);
  const size_t written = prefs.putBytes(key, data, len);
  prefs.end();
  return written == len;
}

bool sealInto(const char *ns, const uint8_t *key, Slot slot, uint8_t generation, const String &value)
{
  uint8_t record[RECORD_MAX];
  size_t len = 0;
  char name[KEY_NAME_MAX];
  slotKey(slot, generation, name, sizeof(name));
  const bool ok = sealRecord(key, value, record, len) && storeBlob(ns, name, record, len);
  memset(record, 0, sizeof(record));
  return ok;
}

bool openFrom(const char *ns, const uint8_t *key, Slot slot, uint8_t generation, String &out)
{
  uint8_t record[RECORD_MAX];
  size_t len = 0;
  char name[KEY_NAME_MAX];
  slotKey(slot, generation, name, sizeof(name));
  const bool ok = loadBlob(ns, name, record, len) && openRecord(key, record, len, out);
  memset(record, 0, sizeof(record));
  return ok;
}

bool loadSalt(const char *ns, uint8_t generation, uint8_t *out)
{
  char name[KEY_NAME_MAX];
  saltKey(generation, name, sizeof(name));
  Preferences prefs;
  prefs.begin(ns, false);
  const size_t read = prefs.isKey(name) ? prefs.getBytes(name, out, SALT_LEN) : 0;
  prefs.end();
  return read == SALT_LEN;
}

bool storeSalt(const char *ns, uint8_t generation, const uint8_t *salt)
{
  char name[KEY_NAME_MAX];
  saltKey(generation, name, sizeof(name));
  return storeBlob(ns, name, salt, SALT_LEN);
}

void removeGeneration(const char *ns, uint8_t generation)
{
  char name[KEY_NAME_MAX];
  Preferences prefs;
  prefs.begin(ns, false);
  for (int i = 0; i < SLOT_COUNT; i++)
  {
    slotKey((Slot)i, generation, name, sizeof(name));
    prefs.remove(name);
  }
  saltKey(generation, name, sizeof(name));
  prefs.remove(name);
  prefs.end();
}
} // namespace

namespace secret_store
{
bool validPin(const char *pin)
{
  if (pin == nullptr || strlen(pin) != PIN_DIGITS)
  {
    return false;
  }
  for (int i = 0; i < PIN_DIGITS; i++)
  {
    if (pin[i] < '0' || pin[i] > '9')
    {
      return false;
    }
  }
  return true;
}

bool locked()
{
  return !rtcKeyValid;
}

bool legacy()
{
  Preferences prefs;
  prefs.begin(LIVE_NS, false);
  const bool provisioned = prefs.isKey(KEY_WIFI_SSID) && !prefs.getString(KEY_WIFI_SSID, "").isEmpty();
  const bool sealed = prefs.isKey(KEY_GENERATION);
  prefs.end();
  return provisioned && !sealed;
}

bool unlock(const char *pin)
{
  if (!validPin(pin) || !hasGeneration(LIVE_NS))
  {
    return false;
  }
  const uint8_t generation = liveGeneration(LIVE_NS);
  uint8_t salt[SALT_LEN];
  if (!loadSalt(LIVE_NS, generation, salt))
  {
    // Sealed blobs with no salt cannot be opened by any PIN, so holding onto
    // them would lock the user out permanently.
    Serial.println("Secrets: salt missing, wiping credentials");
    wipeCredentials();
    return false;
  }
  uint8_t candidate[KEY_LEN];
  String probe;
  // The Wi-Fi password's GCM tag is the verifier. Nothing else is stored: a
  // separate PIN hash would be a strictly easier offline target.
  const bool ok = deriveKey(pin, salt, candidate) &&
                  openFrom(LIVE_NS, candidate, Slot::WifiPassword, generation, probe);
  if (ok)
  {
    memcpy(rtcSessionKey, candidate, KEY_LEN);
    rtcKeyValid = true;
  }
  memset(candidate, 0, sizeof(candidate));
  memset(salt, 0, sizeof(salt));
  return ok;
}

bool provision(const char *pin, const String &wifiPassword)
{
  if (!validPin(pin))
  {
    return false;
  }
  // A re-provision over an existing device still writes the inactive side, so
  // an interruption leaves the old generation openable.
  const uint8_t generation = hasGeneration(LIVE_NS) ? (uint8_t)(liveGeneration(LIVE_NS) ^ 1) : 0;
  uint8_t salt[SALT_LEN];
  esp_fill_random(salt, SALT_LEN);
  uint8_t key[KEY_LEN];
  bool ok = deriveKey(pin, salt, key);
  if (ok)
  {
    // Both tokens are sealed empty so that every slot exists from the start
    // and a later write is a replacement rather than a create.
    ok = sealInto(LIVE_NS, key, Slot::WifiPassword, generation, wifiPassword) &&
         sealInto(LIVE_NS, key, Slot::ClaudeToken1, generation, "") &&
         sealInto(LIVE_NS, key, Slot::ClaudeToken2, generation, "") &&
         storeSalt(LIVE_NS, generation, salt) &&
         commitGeneration(LIVE_NS, generation);
  }
  if (ok)
  {
    memcpy(rtcSessionKey, key, KEY_LEN);
    rtcKeyValid = true;
  }
  memset(key, 0, sizeof(key));
  memset(salt, 0, sizeof(salt));
  return ok;
}

// Overwrites each String's heap buffer before releasing it so the plaintext
// does not linger in freed memory after changePin() returns.
static void zeroStrings(String *arr, int count)
{
  for (int i = 0; i < count; i++)
  {
    memset((char *)arr[i].c_str(), 0, arr[i].length());
  }
}

bool changePin(const char *newPin)
{
  if (locked() || !validPin(newPin))
  {
    return false;
  }
  const uint8_t live = liveGeneration(LIVE_NS);
  const uint8_t next = (uint8_t)(live ^ 1);
  String plain[SLOT_COUNT];
  for (int i = 0; i < SLOT_COUNT; i++)
  {
    // A slot that fails its tag is corrupt, not absent. Re-sealing "" here
    // would quietly destroy it, so refuse the whole change instead.
    if (!openFrom(LIVE_NS, rtcSessionKey, (Slot)i, live, plain[i]))
    {
      Serial.printf("Secrets: slot %d unreadable, PIN unchanged\n", i);
      zeroStrings(plain, i); // zero slots already decrypted before this failure
      return false;
    }
  }
  uint8_t salt[SALT_LEN];
  esp_fill_random(salt, SALT_LEN);
  uint8_t key[KEY_LEN];
  bool ok = deriveKey(newPin, salt, key);
  for (int i = 0; ok && i < SLOT_COUNT; i++)
  {
    ok = sealInto(LIVE_NS, key, (Slot)i, next, plain[i]);
  }
  ok = ok && storeSalt(LIVE_NS, next, salt);
  // Everything above is written to the inactive side, so losing power at any
  // point before this one-byte write leaves the old PIN working.
  ok = ok && commitGeneration(LIVE_NS, next);
  if (ok)
  {
    memcpy(rtcSessionKey, key, KEY_LEN);
    removeGeneration(LIVE_NS, live);
  }
  zeroStrings(plain, SLOT_COUNT);
  memset(key, 0, sizeof(key));
  memset(salt, 0, sizeof(salt));
  return ok;
}

String read(Slot slot)
{
  if (locked())
  {
    return "";
  }
  String value;
  if (!openFrom(LIVE_NS, rtcSessionKey, slot, liveGeneration(LIVE_NS), value))
  {
    // The Wi-Fi password already opened at unlock, so a failure here is
    // corruption confined to this blob. Report it and stay usable.
    Serial.printf("Secrets: slot %d failed to open\n", (int)slot);
    return "";
  }
  return value;
}

void write(Slot slot, const String &value)
{
  if (locked())
  {
    Serial.println("Secrets: locked, nothing written");
    return;
  }
  // A single blob, written in place. Losing power mid-write costs only the
  // value the user just entered, so the A/B dance is not needed here.
  // Nonce quality depends on esp_fill_random, which is only cryptographically
  // strong while the RF subsystem is up — callers should ensure radio is on.
  if (!sealInto(LIVE_NS, rtcSessionKey, slot, liveGeneration(LIVE_NS), value))
  {
    Serial.printf("Secrets: slot %d failed to seal\n", (int)slot);
  }
}

void wipeCredentials()
{
  removeGeneration(LIVE_NS, 0);
  removeGeneration(LIVE_NS, 1);
  Preferences prefs;
  prefs.begin(LIVE_NS, false);
  prefs.remove(KEY_GENERATION);
  for (size_t i = 0; i < sizeof(LEGACY_KEYS) / sizeof(LEGACY_KEYS[0]); i++)
  {
    prefs.remove(LEGACY_KEYS[i]);
  }
  // The SSID is the "am I provisioned?" flag, so clearing it is what sends the
  // board to the captive portal. The account names go too: they name accounts
  // whose tokens have just been destroyed.
  prefs.remove(KEY_WIFI_SSID);
  for (size_t i = 0; i < sizeof(KEY_ACCOUNT_NAMES) / sizeof(KEY_ACCOUNT_NAMES[0]); i++)
  {
    prefs.remove(KEY_ACCOUNT_NAMES[i]);
  }
  prefs.end();
  memset(rtcSessionKey, 0, KEY_LEN);
  rtcKeyValid = false;
}

bool selfTest()
{
  int failures = 0;
  const auto check = [&failures](const char *what, bool ok) {
    Serial.printf("  %-38s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
    {
      failures++;
    }
  };

  Serial.println("selftest: secret_store");
  // A fixed salt, so the timing figure is comparable between runs.
  const uint8_t salt[SALT_LEN] = {0x5a, 0x11, 0x7c, 0x03, 0x9e, 0x42, 0xd8, 0x61,
                                  0x2f, 0xb4, 0x07, 0xcc, 0x95, 0x38, 0xe0, 0x7b};
  uint8_t keyA[KEY_LEN];
  uint8_t keyB[KEY_LEN];

  const uint32_t startedAt = millis();
  check("derive a key from PIN 1234", deriveKey("1234", salt, keyA));
  const uint32_t elapsed = millis() - startedAt;
  Serial.printf("  PBKDF2 %u rounds took %lu ms (target ~400)\n",
                (unsigned)PBKDF2_ROUNDS, (unsigned long)elapsed);
  check("derive a key from PIN 1235", deriveKey("1235", salt, keyB));
  check("different PINs give different keys", memcmp(keyA, keyB, KEY_LEN) != 0);

  check("accept a 4-digit PIN", validPin("0000") && validPin("9999"));
  check("reject a short, long or non-digit PIN",
        !validPin("123") && !validPin("12345") && !validPin("12a4") && !validPin(nullptr));

  uint8_t record[RECORD_MAX];
  size_t len = 0;
  String out;
  check("seal a secret", sealRecord(keyA, "correct horse", record, len));
  check("record is version+nonce+text+tag", len == 1 + NONCE_LEN + 13 + TAG_LEN);
  check("open with the right key", openRecord(keyA, record, len, out) && out == "correct horse");
  check("reject the wrong key", !openRecord(keyB, record, len, out));

  record[1 + NONCE_LEN] ^= 0x01;
  check("reject tampered ciphertext", !openRecord(keyA, record, len, out));
  record[1 + NONCE_LEN] ^= 0x01;
  record[1] ^= 0x01;
  check("reject a tampered nonce", !openRecord(keyA, record, len, out));
  record[1] ^= 0x01;
  record[0] = RECORD_VERSION + 1;
  check("reject an unknown record version", !openRecord(keyA, record, len, out));
  record[0] = RECORD_VERSION;
  check("reject a truncated record", !openRecord(keyA, record, NONCE_LEN, out));

  // Empty plaintext still authenticates. That is what lets an open network's
  // blank Wi-Fi password serve as the PIN verifier.
  check("seal and open an empty secret",
        sealRecord(keyA, "", record, len) && openRecord(keyA, record, len, out) && out.isEmpty());

  String longest;
  for (size_t i = 0; i < PLAIN_MAX; i++)
  {
    longest += (char)('a' + i % 26);
  }
  check("seal and open a full-length secret",
        sealRecord(keyA, longest, record, len) && openRecord(keyA, record, len, out) && out == longest);
  check("refuse an over-long secret", !sealRecord(keyA, longest + "x", record, len));

  // A repeated nonce under one key breaks GCM outright, so two seals of the
  // same text must differ.
  uint8_t repeat[RECORD_MAX];
  size_t repeatLen = 0;
  check("seal the same text twice",
        sealRecord(keyA, "same", repeat, repeatLen) && sealRecord(keyA, "same", record, len));
  check("a fresh nonce per seal", repeatLen == len && memcmp(repeat, record, len) != 0);

  char name[32];
  slotKey(Slot::WifiPassword, 0, name, sizeof(name));
  check("slot key names generation 0", strcmp(name, "wifi_pass0") == 0);
  slotKey(Slot::ClaudeToken1, 1, name, sizeof(name));
  check("slot key names token 1", strcmp(name, "ctok1_1") == 0);
  slotKey(Slot::ClaudeToken2, 1, name, sizeof(name));
  check("slot key names token 2", strcmp(name, "ctok2_1") == 0);
  saltKey(1, name, sizeof(name));
  check("salt key names generation 1", strcmp(name, "pin_salt1") == 0);
  bool withinNvsLimit = true;
  for (uint8_t generation = 0; generation < 2; generation++)
  {
    for (int i = 0; i < SLOT_COUNT; i++)
    {
      slotKey((Slot)i, generation, name, sizeof(name));
      withinNvsLimit = withinNvsLimit && strlen(name) <= 15;
    }
    saltKey(generation, name, sizeof(name));
    withinNvsLimit = withinNvsLimit && strlen(name) <= 15;
  }
  check("every key fits NVS's 15 characters", withinNvsLimit);

  // The A/B commit, run against the scratch namespace so a failure here can
  // never cost a real token. This is the real seal/commit/open path.
  {
    Preferences prefs;
    prefs.begin(SCRATCH_NS, false);
    prefs.clear();
    prefs.end();
  }
  check("seal generation 0 under the old key",
        sealInto(SCRATCH_NS, keyA, Slot::WifiPassword, 0, "old-pass") &&
            storeSalt(SCRATCH_NS, 0, salt) && commitGeneration(SCRATCH_NS, 0));
  check("generation 0 is live", liveGeneration(SCRATCH_NS) == 0);
  // An interrupted PIN change: the inactive side is written but never committed.
  check("seal generation 1 under the new key",
        sealInto(SCRATCH_NS, keyB, Slot::WifiPassword, 1, "new-pass") &&
            storeSalt(SCRATCH_NS, 1, salt));
  check("an uncommitted change leaves the old key live",
        liveGeneration(SCRATCH_NS) == 0 &&
            openFrom(SCRATCH_NS, keyA, Slot::WifiPassword, liveGeneration(SCRATCH_NS), out) &&
            out == "old-pass");
  check("one byte commits the flip", commitGeneration(SCRATCH_NS, 1) && liveGeneration(SCRATCH_NS) == 1);
  check("the new key opens the committed generation",
        openFrom(SCRATCH_NS, keyB, Slot::WifiPassword, liveGeneration(SCRATCH_NS), out) &&
            out == "new-pass");
  check("the old key no longer opens it",
        !openFrom(SCRATCH_NS, keyA, Slot::WifiPassword, liveGeneration(SCRATCH_NS), out));
  removeGeneration(SCRATCH_NS, 0);
  check("the superseded generation is gone",
        !openFrom(SCRATCH_NS, keyA, Slot::WifiPassword, 0, out));
  {
    Preferences prefs;
    prefs.begin(SCRATCH_NS, false);
    prefs.clear();
    prefs.end();
  }

  memset(keyA, 0, sizeof(keyA));
  memset(keyB, 0, sizeof(keyB));
  memset(record, 0, sizeof(record));
  memset(repeat, 0, sizeof(repeat));
  Serial.printf("selftest: %d failure(s)\n", failures);
  return failures == 0;
}
}
