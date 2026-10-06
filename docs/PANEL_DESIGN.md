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

| Reference                                 | Observed pattern                                                                                    | Applied to the panel                                                                                                                                                            |
| ----------------------------------------- | --------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `claude-website1.png`                     | Warm paper background, large serif headline, compact dark primary buttons, generous negative space  | PIN welcome screen and view headings prefer locally installed Anthropic Serif, with Georgia and other serif fallbacks, on warm neutral surfaces; actions have a clear hierarchy |
| `claude-console-website1.png`             | A focused sign-in card, quiet background texture, strongly separated primary action                 | One PIN field, clear instructions, one primary action, and a subtle terracotta rule above the card                                                                              |
| `claude-website-footer.png`               | Near-black surface, muted metadata, fine serif wordmark, restrained terracotta accent               | Dark theme, compact connection metadata, serif brand, independent-project attribution                                                                                           |
| `settings-light.png`, `settings-dark.png` | Stable sidebar, selected navigation surface, divided setting rows, three-way theme selector         | Five focused views, consistent selected state, setting descriptions beside controls, persistent System / Light / Dark controls                                                  |
| `account-light.png`                       | Account settings presented as labelled rows with understated borders                                | Two clearly labelled account groups; masked tokens; save feedback next to the form                                                                                              |
| `usage-light.png`, `usage-dark.png`       | Session and weekly usage separated; blue bars; reset copy below labels; restrained numeric emphasis | Two account cards with distinct session / weekly rows, blue and green account colors, readable percentages and device-time-zone resets                                          |
| `skills-light.png`, `skills-dark.png`     | A contextual introductory surface, grouped content, generous spacing, consistent theme pairing      | Introductory view headings; grouped sound previews; editorial headline list; matching geometry in both themes                                                                   |

The screenshots describe two complementary visual styles: editorial
typography on public pages and practical sidebar navigation in the app.
The redesign combines those patterns using locally available fonts and
inline SVG. It does not depend on proprietary fonts or remote assets.
The shared serif stack is `"Anthropic Serif", Georgia, "Times New Roman", serif`.
Anthropic Serif is an optional local preference: no font file is bundled or
downloaded. A webfont loaded on Claude's website is not available to the panel;
devices without the named installed font use the remaining fallbacks.

The Claude mark uses the actual irregular vector path from the inline
header SVG on [Claude's website](https://claude.com/), captured on
2026-10-05 in `assets/claude-mark.svg` and embedded in the shared brand
symbol. It replaces the previous symmetrical spoke approximation.

The Usage navigation icon uses [Lucide's Gauge](https://lucide.dev/icons/gauge)
paths with the panel's shared icon styling. Lucide publishes the icon under
the ISC license; its [notice](THIRD_PARTY_NOTICES.md) accompanies the source.
It needs no runtime download or icon library.

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

| View             | Purpose                                                                                                              |
| ---------------- | -------------------------------------------------------------------------------------------------------------------- |
| Usage            | Both accounts, usage bars, responsive seven-day chart, manual refresh                                                |
| Accounts         | Account names and masked token replacement, including save-time API probe results                                    |
| Device           | Display/time zone, rotation, Wi-Fi and network scan, complete diagnostics, destructive maintenance                   |
| Polling & alerts | Polling & breaks: interval and break schedule. Alerts & sound: warning thresholds, speaker controls, and quiet hours |
| News             | Feed headlines, publication dates, source link, fetch/stale feedback                                                 |

Desktop uses a 224px sidebar and a constrained content column. The sidebar
narrows at 1100px and becomes a sticky header with a scrollable navigation
row at 760px. Below 480px, usage cards stack. Settings rows stack before
controls become cramped. Device diagnostics live in Device details.
Sidebar navigation uses icons and labels without index numbers.
Sign out sits right of the theme switch on mobile and left of it on desktop,
with keyboard order matching the visible arrangement. Device details shows
the Wi-Fi station MAC address immediately after Wi-Fi. Both footers include
a GitHub link to this repository. News dates use 13px text and the fetch
status links to anthropic.com/news.
Firmware details show release version `0.0.9` and the generated git revision;
the build timestamp has been removed. The release constant lives in
`src/build_info.h`.
The topbar is hidden at 760px and below, recovering its 48px mobile height.

The sign-in screen has compact typography and spacing below 480px. At
360 × 780 CSS pixels with DPR 3, the welcome copy, PIN field, 44px sign-in
button, help, and attribution fit without scrolling in both themes, with
the keyboard closed and normal text size. Shorter viewports, the on-screen
keyboard, and enlarged text can scroll naturally; content is not clipped.
Entering or pasting a complete six-digit PIN automatically signs in;
manual retry remains available and pending requests cannot submit twice.
Quiet hours uses a native checkbox styled as an accessible switch: a
36 × 20px blue track, white 16px thumb, and gray off state matching the
supplied notification-settings snapshot. A 44px label target keeps touch
operation comfortable. The label and description sit left of the switch
in both desktop and mobile layouts; the time fields sit below. Its draft
survives polling and is applied with Save alerts.
Alert sounds uses the same blue for the volume slider and playback
icons, hover/focus outlines, and playing states. Its soft blue surfaces
adapt to light and dark themes through shared CSS variables.
The volume percentage uses the standard text color. The slider and usage
progress bars share a 6px track height; the slider retains a 44px control
height and a 14px thumb. Its filled track follows the current value during
device updates and dragging, with explicit Chromium/WebKit and Firefox styles.

Settings rows draw separators between adjacent rows. The save-action area
owns the final divider, avoiding the duplicate bottom/top borders that
previously appeared in Display & time and Polling & alerts.

Theme colors are CSS variables shared by chart paths and interface
surfaces. Theme choice persists in localStorage when available; blocked
storage falls back safely. A small inline script in the head resolves the
saved or system theme before styles and body paint, keeping the page canvas,
native controls, and browser theme color aligned without a light-mode flash
on dark-mode loads. First-paint checks stream the actual embedded page while
holding back app initialization, including invalid and unavailable storage.
Reduced-motion preference removes transitions
and view-entry motion. Native buttons provide keyboard operation for the
chart legend and Wi-Fi scan results. Form labels, focus outlines, live
messages, and an adaptive skip link support keyboard navigation.
Navigation moves focus to the page heading for assistive technology. These
noninteractive headings suppress the visible outline; buttons, links, and
form controls retain their keyboard focus indicators.

The seven-day chart gives each visible midnight tick and its weekday label
the same x-coordinate, with the text anchored in the middle. Labels sit
below 00:00 in the device's time zone rather than halfway through a day.
The partial day at the chart's left edge gets no label unless its midnight
tick is visible. Theme changes, resizing, and account toggles rebuild both
the ticks and labels together.

Account names and feed/SSID text are rendered with textContent. Headlines
only become links for plain HTTPS URLs. Missing usage is a dash and chart
gaps remain gaps. Usage bars reflect configured warning thresholds.
Any authenticated API returning 401 brings
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
with local API mocks. It checks the complete sign-in screen at 360 × 780
and DPR 3 in both themes, touch target sizes, six-digit automatic sign-in,
pending-request deduplication, sign-in/429, theme persistence and system
theme changes, device-time-zone formatting with a different browser zone,
keyboard chart toggles, all five views at 320/390/640/768/1024/1440px,
non-overlapping mobile header controls, single separators above both save
areas, quiet-hours row alignment, account/Wi-Fi/display/settings writes,
quiet-hours switching with keyboard/dirty-state/save behavior,
all six sound requests, unsafe text handling, five-row news sizing,
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
84 kB; the compiled firmware uses about 46% of the application partition
and 39% of static RAM. Browser screenshots were inspected in light/dark,
desktop/mobile, and the settings views.

On 2026-10-05, the firmware was uploaded to the ESP32-S3-PICO-1 on COM5;
the uploader verified the flash hash. Serial output confirmed a successful
boot, Wi-Fi connection, HTTP 200 polls for both accounts, and Web Panel
startup. The page retrieved from the board matched the embedded HTML
byte-for-byte (78,516 bytes) for the initial redesign.

The compact sign-in and corrected mark were subsequently rebuilt and
flashed on the same board. At 22:14 MYT on 2026-10-05, live Chromium
verification confirmed the updated page matched the embedded HTML
(81,373 bytes), fit 360 × 780 at DPR 3 in both themes, and successfully
signed in. Four authenticated state checks over 30 seconds passed, with
no browser script errors. This was browser emulation, not a physical S22
test. Physical sound playback, screen-reader, and cross-browser audits
remain separate from these checks.

On 2026-10-06, automatic six-digit sign-in, the simplified Usage view,
the Wi-Fi card icon, and the quiet-hours switch passed the browser suite
and were flashed to COM5. The firmware binary and live HTTP response
matched the current 79,470-byte embedded page. Live mobile Chromium
confirmed automatic sign-in, both-theme sign-in fit, removal of the two
Usage sections, the Wi-Fi icon, and keyboard switch operation with draft
preservation during refresh. Four authenticated state checks over 30
seconds passed without browser script errors. Quiet-hours save behavior
was tested with API mocks; live verification did not change saved settings.

The duplicate save-area dividers were reproduced as two painted borders
in both cards and reduced to one by the shared row-separator fix. Browser
regression checks cover both save areas and the quiet-hours arrangement at
all six widths. The snapshot-matched switch was inspected at 384 × 824 in
both states and themes. The mobile-topbar/sound-theme build and COM5 upload passed; the live
response matched the 79,667-byte page. Live checks confirmed one divider
per save area, switch dimensions/colors, keyboard operation, mobile/desktop
topbar visibility, and matching blue sound controls. Four authenticated
state responses over 30 seconds passed without script errors. Light/dark
desktop and mobile screenshots were also inspected after the theme change.

The subsequent refinements restored the standard percentage text color,
share the 6px progress-bar height with the slider track, and remove the
sidebar navigation index numbers and their styling.
Chromium inspection measured the rendered native track at 6px and checked
0%, 50%, and 100% fill/value rendering in both themes. The browser suite
and firmware build passed; the binary contains the exact 80,387-byte page.
These refinements were not flashed at that attempt: COM5 was absent and
no USB ports returned during the three-minute board-specific retry. They
were included in the later successful uploads described below.

Break Hours was added on 2026-10-06 alongside Quiet hours with the same blue
switch, keyboard/touch behavior, and From/Until arrangement. It is disabled
by default, initially 01:00-06:00, and pauses automatic usage requests during
a daily local-time window; equal times disable it and overnight windows are supported. Normal
mode sleeps until polling resumes, preserving cached usage and button
wakeups. Quiet hours remains independent. An explicit Refresh now or serial
usage command overrides the pause. Usage shows the scheduled pause and
resume time without concealing the last figures.

The browser regression suite passed draft preservation, save/disable,
external state synchronization, pause-notice behavior, and both schedule
layouts at all six widths. The production daily-window code also passed
host tests for every minute of the day, exact second boundaries, overnight
and year rollover, and daylight-saving gaps/repeats; MSVC compiled these
tests with warnings treated as errors. The final ESP32 build passed and
the binary contains the exact 81,959-byte embedded page. This version has
not been flashed at that stage; physical sleep/wake and current-draw verification remain
pending. See [the host test instructions](../test/pause_hours/README.md).

The navigation item, breadcrumb, and page title now use Polling & alerts.
Its two cards are Polling & breaks (Poll interval, then Break hours) and
Alerts & sound (Warning thresholds, Alert sounds, then the retained Quiet
hours controls). Each card saves only its own settings; volume continues
to save on release. Browser regressions verified both save payloads and
preservation of unsaved edits in the other card, along with both switches
and one divider per save area at all six widths. Desktop/mobile light/dark
previews were inspected. The ESP32 build passed and embeds the exact
82,711-byte page. This layout was subsequently flashed with the heading
outline fix, as recorded below.

On 2026-10-06, the heading-outline firmware was flashed to the ESP32-S3-PICO-1 on
COM5 and the uploader verified its flash hash. The live board served the
exact 82,760-byte embedded page at its then-current LAN IP, 10.62.231.167.
Chromium verified automatic sign-in, 360 x 780 at DPR 3 in both themes,
the reordered cards/navigation, both switches and draft preservation,
neutral volume text and matching 6px tracks, and heading focus without an
outline. Four authenticated state checks over 30 seconds passed without
browser script errors. The board reported Break Hours 01:00-06:00,
disabled, and cached usage for both accounts. These are live page/runtime
checks; scheduled sleep/wake and current draw have not been measured.

A later upload on 2026-10-06 included the optional local serif stack,
Lucide Gauge, larger news dates and source link, Wi-Fi MAC address, mobile
Sign out order, GitHub footer links, and the first-paint theme fix. The
uploader verified the flash hash, and the live board served the exact
84,033-byte build snapshot. Chromium confirmed saved light/dark and system
dark preferences on the first frame, mobile sign-in fit, the MAC field and
footer links, and four authenticated state responses over 30 seconds with
no browser script errors. The serial wake connection stayed open for testing.

The subsequent weekday-label alignment change is verified locally and has
not yet been flashed. A focused Chromium check passed 40 combinations of
mobile/desktop widths, light/dark appearance, Kuala Lumpur/New York device
time zones, and hourly offsets including midnight. Every weekday label had
the same x-coordinate as its tick and a centered text anchor. Desktop and
mobile chart screenshots were inspected. The current compact age strings (`25s ago`, `12m ago`,
`3h ago`) are documented in [PANEL_FEATURES.md](PANEL_FEATURES.md) and
[WEB_SERVER.md](WEB_SERVER.md).


On 2026-10-06, Break Hours was renamed to Pause Hours throughout the current
panel, firmware settings, API (`pause_*`), and serial commands (`pause`).
The card and save button now say Polling & pauses. The initial window is
00:00-06:00 (12:00am-6:00am), disabled. Existing saved Break Hours schedules
migrate to new NVS keys and retain their times and enabled state.

Pause Hours and Quiet hours reject matching From/Until times, including
while disabled. The panel reports an accessible error and retains the draft;
the API validates merged partial updates before applying any settings, and
firmware setters/serial commands reject invalid windows. Legacy empty
windows remain inactive until corrected. Overnight windows still work.

The 7 days usage history chart uses solid 5-hour lines and dashed 7-day
lines, with matching legend samples and an updated accessible description.
Earlier Break Hours names, defaults, and board reports above are historical.

Browser regressions passed equality rejection for both schedules with switches
on/off, correction of invalid drafts, midnight defaults, overnight saves,
draft preservation, and the existing responsive/theme checks. Native tests
passed schedule validation, wake boundaries, and DST behavior with warnings
treated as errors. A temporary host harness exercised the production JSON
validator and settings helpers using simulated NVS, including legacy migration
and preservation of newer saves. Desktop light/mobile dark previews confirmed
the chart line styles and renamed card. The ESP32 firmware build passed.

The Pause Hours firmware was subsequently flashed on 2026-10-06 to the
ESP32-S3-PICO-1 on COM5, with the uploader verifying the flash hash. Live
checks confirmed exact embedded HTML, sign-in at 360 x 780/DPR 3 in both
themes, firmware version 0.0.9, Quiet/Pause equal-time rejection with switches
on/off, merged partial-update validation without changing saved settings,
and solid 5-hour/dashed 7-day paths with matching legend samples. Four
authenticated state checks over 30 seconds passed without browser errors.
Physical sleep/wake and current draw remain unmeasured. The later removal
of the PIN input placeholder has not been flashed.
