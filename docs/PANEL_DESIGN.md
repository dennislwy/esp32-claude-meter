# Claude Meter panel design

Implemented on `feature/claude-panel-redesign`, 2026-10-05.

The concept is **a little more perspective**: a calm workspace for seeing
usage and configuring a physical desktop companion. The panel's primary
task is reading two accounts quickly; device maintenance lives in its own
view. The original single-column layout made every action part of one long
scroll and used the same visual weight for usage, configuration, and resets.

## Snapshot analysis

All ten supplied images in `claude-ui-samples/` were inspected. These are
local references, not runtime assets.

| Reference | Observed pattern | Applied to the panel |
| --- | --- | --- |
| `claude-website1.png` | Warm paper background, large serif headline, compact dark primary buttons, generous negative space | PIN welcome screen and view headings use Georgia with warm neutral surfaces; actions have a clear hierarchy |
| `claude-console-website1.png` | A focused sign-in card, quiet background texture, strongly separated primary action | One PIN field, clear instructions, one primary action, and a subtle terracotta rule above the card |
| `claude-website-footer.png` | Near-black surface, muted metadata, fine serif wordmark, restrained terracotta accent | Dark theme, compact connection metadata, serif brand, independent-project attribution |
| `settings-light.png`, `settings-dark.png` | Stable sidebar, selected navigation surface, divided setting rows, three-way theme selector | Five focused views, consistent selected state, setting descriptions beside controls, persistent System / Light / Dark controls |
| `account-light.png` | Account settings presented as labelled rows with understated borders | Two clearly labelled account groups; masked tokens; save feedback next to the form |
| `usage-light.png`, `usage-dark.png` | Session and weekly usage separated; blue bars; reset copy below labels; restrained numeric emphasis | Two account cards with distinct session / weekly rows, blue and green account colors, readable percentages and device-time-zone resets |
| `skills-light.png`, `skills-dark.png` | A contextual introductory surface, grouped content, generous spacing, consistent theme pairing | Current-state summary above usage; grouped sound previews; editorial headline list; matching geometry in both themes |

The screenshots describe two complementary visual styles: editorial
typography on public pages and practical sidebar navigation in the app.
The redesign combines those patterns using locally available fonts and
inline SVG. It does not depend on proprietary fonts or remote assets.

## Award benchmarks

These are quality goals, not an award certification or an assertion that
the page has been judged.

- **Awwwards:** its published evaluation weighs design, usability,
  creativity, and content. The panel addresses these through consistent
  typography, responsive hierarchy, a companion-specific visual concept,
  and concise descriptions. [Evaluation system](https://www.awwwards.com/about-evaluation/).
- **Webby:** its website criteria include content, structure/navigation,
  visual design, functionality, interactivity, innovation, and overall
  experience. Focused views, preserved drafts, useful failure feedback,
  keyboard controls, and local asset delivery support those goals.
  [Judging criteria](https://www.webbyawards.com/judging-criteria/).
- **FWA:** its stated focus includes digital innovation, creativity,
  originality, and technical excellence. For this hardware panel, the
  interpretation is a distinct visual identity and deliberate interaction
  with the device, including actual speaker previews. This does not claim
  an official FWA numerical rubric. [FWA anniversary statement](https://thefwa.com/FWA25/25.html).

## Interaction and layout

| View | Purpose |
| --- | --- |
| Usage | Current threshold summary, both accounts, battery/poll/signal/uptime, responsive seven-day chart |
| Accounts | Account names and masked token replacement, including save-time API probe results |
| Device | Display/time zone, rotation, Wi-Fi and network scan, complete diagnostics, destructive maintenance |
| Alerts & sound | Poll interval, warning thresholds, quiet hours, volume, six speaker previews |
| News | Feed headlines, publication dates, source link, fetch/stale feedback |

Desktop uses a 224px sidebar and a constrained content column. The sidebar
narrows at 1100px and becomes a sticky header with a scrollable navigation
row at 760px. Below 480px, usage cards stack and device summaries become a
two-column grid. Settings rows stack before controls become cramped.

Theme colors are CSS variables shared by chart paths and interface
surfaces. Theme choice persists in localStorage when available; blocked
storage falls back safely. Reduced-motion preference removes transitions
and view-entry motion. Native buttons provide keyboard operation for the
chart legend and Wi-Fi scan results. Form labels, focus outlines, live
messages, and an adaptive skip link support keyboard navigation.

Account names and feed/SSID text are rendered with textContent. Headlines
only become links for plain HTTPS URLs. Missing usage is a dash, chart gaps
remain gaps, and the summary reflects configured warning thresholds rather
than making a usage forecast. Any authenticated API returning 401 brings
back the PIN screen. Failed requests provide feedback and release controls.

The firmware API and its destructive two-click confirmation remain in use.
The factory-reset copy describes the existing setup hotspot correctly.
There are no production demo data, new firmware endpoints, external fonts,
image downloads, or JavaScript frameworks.

## Preview without hardware

From the repository root:

```powershell
python scripts/panel_preview.py
```

Open `http://127.0.0.1:8080` and enter **123456**. The preview extracts the
actual page from `src/panel_html.h` on every reload. All data and API writes
are simulated in memory, and the server binds only to loopback. Tokens and
Wi-Fi passwords are not persisted. Reboot, factory reset, and speaker
buttons return simulated responses. Stop the server with Ctrl+C.

## Verification

The browser regression script uses Playwright and the exact embedded HTML,
with local API mocks. It checks sign-in/429, theme persistence and system
theme changes, device-time-zone formatting with a different browser zone,
keyboard chart toggles, all five views at 320/390/640/768/1024/1440px,
non-overlapping mobile header controls, account/Wi-Fi/display/settings
writes, all six sound requests, unsafe text handling, five-row news sizing,
destructive confirmation, empty history/usage, preservation of unsaved
edits, connection failure, and session expiry. No external assets may load.

Use an installed Playwright package and browser:

```powershell
node scripts/check_panel_ui.cjs <path-to-playwright-module> <path-to-chromium-executable>
```

If Playwright is already resolvable in this project and its Chromium is
installed, `node scripts/check_panel_ui.cjs` also works. This task reused
the workstation's cached installation and added no npm dependencies.

The PlatformIO firmware build passed. The embedded page is approximately
78 kB; the compiled firmware uses about 46% of the application partition
and 39% of static RAM. Browser screenshots were inspected in light/dark,
desktop/mobile, and the settings views.

On 2026-10-05, the firmware was uploaded to the ESP32-S3-PICO-1 on COM5;
the uploader verified the flash hash. Serial output confirmed a successful
boot, Wi-Fi connection, HTTP 200 polls for both accounts, and LAN panel
startup. The page retrieved from the board matched the embedded HTML
byte-for-byte (78,516 bytes). Physical sound playback, screen-reader, and
cross-browser audits remain separate from these checks.
