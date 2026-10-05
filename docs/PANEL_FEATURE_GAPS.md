# Panel feature gaps vs. claude-usage-stick

Comparison between this project's LAN panel ([PANEL_FEATURES.md](PANEL_FEATURES.md))
and the [claude-usage-stick](https://github.com/oauramos/claude-usage-stick)
panel (local clone: `C:\Users\dennis\Documents\git_public\claude-usage-stick`, see its `docs/PANEL_FEATURES.md`),
as of 2026-10-05. "Gap" = feature they have, we don't.

Each gap has a stable **code** (`SEC-1`, `OBS-2`, …) so follow-up issues,
commits, and PRs can reference them without quoting the whole row. Codes
are append-only — once assigned, don't reuse. Closed gaps keep their code
and get a strike-through.

## Security &amp; auth (SEC)

| # | Gap | What they do |
| --- | --- | --- |
| **SEC-1** | **PIN-encrypted token** | AES-256-GCM encrypts the OAuth token; PIN is never stored — the GCM tag is the only oracle. We store tokens plaintext in NVS (R9 pending). |
| **SEC-2** | **24-hour session cookie** | `Max-Age=86400`, survives panel re-entry. Ours is RAM-only; any reboot or panel exit kills it. |
| ~~**SEC-3**~~ | ~~**Multiple concurrent sessions**~~ | **Closed 2026-10-05:** 4 session slots; a 5th login evicts the slot idle longest. |
| **SEC-4** | **Token rotation re-prompts PIN** | Changing the token requires the PIN again so the current blob can be decrypted first. N/A for us until SEC-1 lands. |
| ~~**SEC-5**~~ | ~~**Token save probes the API**~~ | **Closed 2026-10-05:** `/api/tokens` probes each newly saved token and returns `probes[]`. |

## Status &amp; observability (OBS)

| # | Gap | What they do |
| --- | --- | --- |
| ~~**OBS-1**~~ | ~~**Firmware version + codename**~~ | **Closed 2026-10-05:** Status card shows git revision + build time (`fw_rev`, `fw_built`). No codename. |
| ~~**OBS-2**~~ | ~~**RSSI on a card**~~ | **Closed 2026-10-05:** Status card shows SSID, dBm, and a quality word (excellent / good / fair / weak). |
| ~~**OBS-3**~~ | ~~**Heap free + heap min**~~ | **Closed 2026-10-05:** Status card shows internal-SRAM free + low-water mark (`heap_free`, `heap_min`). |
| **OBS-4** | **Model health** | Haiku / Sonnet / Opus / Fable up/down indicator in `/api/state`. We don't poll model status. |
| **OBS-5** | **Anthropic news feed** | `/api/news` returns 5 RSS items (title + "Oct 02"); device fetches on a 6 h schedule. We have nothing. |
| **OBS-6** | **Lock state** | "locked (PIN screen)" / "unlocked" — N/A for us (no device-side PIN screen). |

## Device settings (SET)

| # | Gap | What they do |
| --- | --- | --- |
| **SET-1** | **Editable device name** | `dev_name` persisted; mDNS hostname regenerates on reboot (UI flags `hostname_pending_reboot`). Ours is fixed `claude-meter`. |
| **SET-2** | **Brightness** | 0–3 via `ledcWrite`. N/A on our ePaper. |
| ~~**SET-3**~~ | ~~**Timezone dropdown**~~ | **Closed 2026-10-05:** type-ahead picker over 145 IANA zones (search by city, country, alias, or offset like `gmt+8`). Stores a POSIX TZ string from tzdata 2025c, so DST is handled, which their fixed `tz_min` offset can't do. |
| ~~**SET-4**~~ | ~~**Flip screen 180°**~~ | **Closed 2026-10-05:** 0 / 90 / 180 / 270° rotation (the square panel needs no relayout), applied in the LVGL flush with a full e-paper refresh. |

## UI mode / screens (UI)

| # | Gap | What they do |
| --- | --- | --- |
| **UI-1** | **Carousel screens** | Static / carousel / clock-only, dwell 5/10/15/30 s, screen bitmask over Dashboard / Chart / News / Clock, mascot bitmask. All N/A for our single-view 1.54" ePaper setup. |

## Mechanics &amp; API quality (API)

| # | Gap | What they do |
| --- | --- | --- |
| ~~**API-1**~~ | ~~**Async Wi-Fi scan**~~ | **Closed 2026-10-05:** `GET /api/wifi/scan?start=1` queues an async scan and returns `202` at once; the panel polls until results land. Click-to-list time is unchanged (~8 s, bound by the radio); the gain is that the device stays responsive. |
| **API-2** | **Deferred `202 queued`** | Refresh returns `202` immediately; `loop()` runs the fetch. Ours returns `{ok:true}` after the handler returns. |
| **API-3** | **`401` blanks panel to login** | Explicit client-side flow. We likely bounce to login too but not documented. |
| ~~**API-4**~~ | ~~**`429` with `Retry-After` header**~~ | **Not a gap (corrected 2026-10-05):** `/api/login` already sends the `Retry-After` header alongside `{retry_s:N}` in the body. |
| **API-5** | **Debug-build seed-history endpoint** | `/api/debug/seed-history` for layout testing. We have no debug-only endpoints. |

## Where we have more

Not gaps, but things we have that they don't — kept for completeness so the
comparison runs both ways. Codes here (`PLUS-N`) make it easy to reference
during trimming decisions ("do we really need PLUS-5?").

| # | Advantage |
| --- | --- |
| **PLUS-1** | Alert sounds + volume + 6 test-play buttons (onboard ES8311 speaker) |
| **PLUS-2** | Quiet hours with HH:MM precision and midnight wrap |
| **PLUS-3** | Battery % on the status card |
| **PLUS-4** | Two Claude accounts (names + tokens + history series) vs. their single token |
| **PLUS-5** | Separate 5 H / 7 D warn thresholds on configurable percent values |
| **PLUS-6** | User-settable poll interval (1–5 min) |
| **PLUS-7** | Explicit Reboot button (theirs only reboots as a side effect of Wi-Fi change or factory reset) |
| **PLUS-8** | "Clear 7-day history" as a standalone action (theirs only wipes it inside factory reset) |

## High-value asks (if we close gaps)

Ranked by return on effort for this project. Each line is one of the gap
codes above:

1. ~~**OBS-1** — Firmware version + build timestamp on Status card.~~ Done.
2. ~~**API-1** — Async Wi-Fi scan.~~ Done.
3. ~~**OBS-3** — Heap free / min on Status card.~~ Done.
4. ~~**SEC-5** — Token-save probe.~~ Done.
5. **SEC-1** — PIN-encrypted token storage. Biggest security win; already on the roadmap as R9.
6. **PANEL-ALERTS** — Alerts inspection endpoint. The only serial command without panel parity today (`alerts` / `alerts clear`). Not a gap vs. claude-usage-stick (they don't have alerts either), so it gets its own code outside the SEC/OBS/… tables.
