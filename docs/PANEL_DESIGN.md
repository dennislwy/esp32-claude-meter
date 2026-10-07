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
inline SVG icons and a locally bundled ECharts history chart. It does not
depend on proprietary fonts or remote assets.
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

Open the printed URL and enter **123456**. The preview defaults to port 8080
and chooses a free port if it is busy. The preview extracts the
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
edits, connection failure, session expiry, and hostname rename (pending
notice, client-side rejection, server-side 400, and survival of an
unsaved edit across a poll). No external assets may load.

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
pending. See the host test instructions for
[pause hours](../test/pause_hours/README.md) and
[the hostname rule](../test/hostname/README.md).

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

On 2026-10-06, the history renderer was replaced with Apache ECharts 6.1.0.
The card retains the two account colors, solid 5-hour/dashed 7-day lines,
and weekday labels aligned with midnight ticks. Users can choose one
combined graph or separate 5-hour and 7-day graphs with a shared zoom range.
Four keyboard-accessible legend buttons independently hide each series.
Hover/tap tooltips show device-local times and percentages. Mouse wheel,
click, pinch, slider, plus/minus buttons, and keyboard controls provide
zoom and pan; Reset zoom restores the full week. Selection and zoom remain
intact across polling, themes, and page switches.

A custom module bundle is compressed into firmware flash and served at
`/assets/echarts-6.1.0.js`, with no CDN or filesystem update. The committed
asset uses 190,685 bytes. Its source lockfile, reproducible build script,
licenses, and notices are documented in [assets/echarts](../assets/echarts/README.md).
Loading is deferred until authenticated history is displayed; failed asset
loads show a retry action without preventing other panel functions.

Local Chromium checks exercise the actual bundled engine, independent
legend selection, safe tooltip text, combined/separate graphs, preserved
zoom, click/keyboard zoom, touch pinch events, asset retry, and midnight
alignment in whole-, half-, and quarter-hour device time zones. The existing
responsive and first-paint theme checks also pass. These touch checks use
browser emulation; this change has not been flashed or tested on a phone.
The ESP32 build passed at 52.2% flash usage, and the firmware binary contains
the compressed chart asset byte-for-byte.

The ECharts firmware, including the current icon edits, was flashed on
2026-10-06 to the ESP32-S3-PICO-1 on COM5. The uploader verified the flash
hash. Live Chromium checks confirmed the exact embedded HTML and chart
bundle, sign-in at 360 x 780/DPR 3 in both themes, four 336-sample series,
solid/dashed styles, independent legend toggles, combined/separate graphs,
tooltips, click/button/pinch zoom, and preserved zoom across page changes.
The first chart-initialization check timed out; a fresh browser session
passed the interaction checks and four authenticated state requests over
30 seconds without script errors. The serial wake connection and an
authenticated browser were left open for testing. Phone touch gestures
were emulated in Chromium; physical phone testing remains to be done.

The subsequent chart refinement removes the View/layout selector and uses
one combined graph. Double-click or double-tap inside the plot toggles
between a half-week view centered on the selected point and the full week.
Single clicks/taps retain tooltip behavior without zooming. Drag, pinch,
and cancelled touch gestures are excluded from double-tap detection;
wheel/pinch, slider, buttons, and keyboard controls remain available.
Local browser regressions passed desktop double-click toggling, emulated
touch double-tap toggling, unchanged zoom on single clicks/taps and drags,
pinch zoom, retained selections/range, and the existing responsive checks.
This refinement has not been flashed.

The additional 5-hour/7-day key above the plot has been removed; the series
buttons retain their solid/dashed swatches. ECharts' Save image toolbox
control downloads the current chart as a 2x-resolution PNG with an opaque
background matching the active theme. The toolbox is excluded from the
export. The local bundle now includes ToolboxComponent (202,957 compressed
bytes), and `/assets/echarts-6.1.0-v2.js` prevents an older cached engine from
missing the new feature. Local browser checks passed actual PNG downloads
on desktop/light and emulated mobile/dark, with valid image dimensions and
opaque theme backgrounds, along with the existing chart regressions.
The ESP32 firmware build passed at 52.6% flash usage, with the updated
bundle and v2 asset route verified in the binary.
These changes have not been flashed.

On 2026-10-06, the chart zoom-status label was removed and the current
firmware, including double-click/double-tap zoom toggling and Save image,
was flashed to the ESP32 on COM5. The uploader verified the flash hash.
Live Chromium checks confirmed exact HTML and chart bundle, absence of
the layout selector/key/zoom-status label, zoom/legend/tooltip interactions,
PNG downloads on desktop/light and emulated mobile/dark, and four
authenticated state responses over 30 seconds without browser errors.
Serial and browser connections were left open to keep the panel available
for testing. These checks supersede the earlier unflashed status above.

The `history-tools` toolbar, zoom-button markup/styles, and button handlers
were subsequently removed. Double-click/double-tap, pinch/wheel, slider,
and keyboard zoom/pan/reset remain available, together with Save image.
The updated firmware was flashed to COM5 with hash verification. Local
browser regressions and live board checks passed toolbar absence, gesture
and keyboard zoom, PNG exports, and four authenticated state requests over
30 seconds without browser errors. The serial and browser connections
remain open for testing.

The chart now displays its native ECharts legend above the plot, with
independent series toggles and wrapping on narrow screens. Long account
names are shortened while retaining the usage-window label. The legend
is included in saved PNGs; the Save image icon is reduced from 32 to 18 px.
Equivalent keyboard-accessible series buttons appear on focus in an
overlay, avoiding duplicate visible controls or page-layout shifts.
Local browser regressions and live checks on the firmware flashed to COM5
on 2026-10-06 passed native legend selection, mobile layout, desktop/light
and emulated mobile/dark PNG exports, and existing zoom interactions.
The uploader verified the flash hash, and four authenticated state requests
over 30 seconds passed without browser errors. Serial and browser wake
connections were left open for testing; physical phone testing remains
to be done.

The bottom zoom slider is now removed. The plot uses the reclaimed space,
while wheel/pinch, drag, double-click/double-tap, and keyboard interactions
remain available. The 18 px Save image icon is vertically centered with
the first legend row using rendered bounds, including when the legend
wraps on mobile. Local Chromium regressions passed slider absence, icon
alignment on desktop and mobile, PNG downloads in both themes, and the
existing gesture/keyboard and responsive checks. These refinements are
included in the subsequent mobile-pointer flash.

On mobile (up to 760 px), the x-axis pointer explicitly enables snapping
with a 1 px line and a device-local weekday/time label. Desktop pointer
behavior is preserved, and resizing re-applies the appropriate options.
The firmware was flashed to COM5 on 2026-10-06 with hash verification.
Local Chromium regressions and live checks at 360 x 780/DPR 3 passed touch
pointer visibility, snapping to a sample tick, line width, and time label.
Live checks also verified slider removal, legend/export-icon alignment,
PNG exports, gesture/keyboard zoom, exact HTML/chart bundle, and four
authenticated state requests over 30 seconds without browser errors.
Serial and browser connections were left open to keep the panel awake
for testing. Touch checks used Chromium emulation; physical phone testing
remains to be done.

The dashed 7-day series are now 1.4 px wide (previously 2.2 px); the solid
5-hour series remain 1.6 px. The mobile x-axis pointer displays a 44 px
draggable handle, including on initial load. Mobile plot margins reserve
space below the axis and at its right edge to keep the handle fully visible.
Local Chromium checks verified handle bounds, touch dragging without
changing zoom or scrolling the page, and the existing chart regressions.
The firmware was flashed to COM5 on 2026-10-06 with hash verification.
Live browser checks confirmed exact HTML/chart bytes, thinner lines,
mobile handle dragging, removal of the handle on desktop, PNG downloads,
and four authenticated state responses over 30 seconds without errors.
Serial and browser connections were left open for testing. Mobile touch
checks used Chromium emulation.

The latest refinement disables series emphasis and axis-pointer emphasis
on mobile, so tapping a line shows its tooltip without thickening or
recoloring it or dimming other series. Desktop hover emphasis is retained.
The dashed 7-day lines are reduced to 1 px and the mobile pointer handle
to 12 px. Smaller mobile margins reclaim plot space while keeping the
handle below the time label and within the canvas. Local Chromium checks
passed actual line taps with unchanged rendered color/width/opacity,
12 px handle sizing and touch dragging, responsive emphasis settings,
PNG exports, and existing chart regressions. These refinements are
included in the following flash.

The mobile pointer time label is now hidden; its snapping line, 12 px
handle, and time-bearing tap tooltip remain available. The firmware was
flashed to COM5 on 2026-10-06 with hash verification, including 1 px dashed
7-day lines and mobile taps without series emphasis or dimming. Local
browser regressions passed. Live Chromium checks confirmed exact HTML
and chart bytes, hidden pointer label, handle dragging, unchanged line
styles after taps, PNG exports, and existing zoom gestures. One initial
connectivity check ended with ECONNRESET; a fresh browser session passed
all chart checks and four authenticated state requests over 30 seconds
without browser errors. Serial and browser connections were left open
for testing. Mobile gestures were emulated in Chromium.

The mobile pointer handle is now 14 px and centered on the x-axis with
zero handle margin. The plot reclaims the extra space previously reserved
below the weekday labels. Compact legend/tooltip labels use the saved
account name plus `5hr` for account 1, `5h` for account 2, and `7d` for
both weekly series, matching the requested labels. Long-name truncation
retains the compact suffix and uses an ellipsis. Stable series IDs retain
legend selections and zoom across refreshes.
Local Chromium regressions passed exact label text, rendered handle size
and axis alignment, touch dragging, tap styling, and existing chart checks.
The firmware was flashed to COM5 on 2026-10-06 with hash verification.
Live checks confirmed the four labels (`dev1 · 5hr`, `dev1 · 7d`,
`dev2 · 5h`, `dev2 · 7d`), handle alignment and dragging, PNG exports,
exact HTML/chart bytes, and four authenticated state requests over
30 seconds without browser errors. Serial and browser connections were
left open for testing. Mobile touch was emulated in Chromium.

The first account's 5-hour legend label now uses `5h`, matching the
second account. The mobile pointer handle grows to 16 px. The chart title
and native Save image icon occupy the same row inside the ECharts canvas,
so the title also appears in exported PNGs. The original HTML heading
remains visible while the chart loads or retries, then remains available
to assistive technology. The native hover caption is
hidden because it lingered after a mobile tap. The chart canvas grows by
46 px to accommodate the title without reducing the plot substantially.
Local Chromium regressions passed desktop and mobile alignment, handle
dragging, compact labels, PNG downloads in both themes, rendered title
pixels in PNGs, and the existing gesture and responsive checks. This
refinement was flashed to COM5 on 2026-10-06 with hash verification.
Live Chromium checks confirmed the exact served assets, four compact
legend labels, 16 px mobile handle alignment and dragging, title and
Save image alignment, PNG export, and four authenticated state requests
over 30 seconds. Serial and browser connections were left open for testing.

The chart's Save image toolbox control uses Lucide's Camera outline, drawn by
ECharts at the existing 18 px size. Camera is wider than tall, so ECharts'
centered layout scales it to 18 px wide and leaves it shorter than the
Export CSV glyph beside it; both icons stay centered in their own 18 px cell,
so their centers remain level and the title offset (measured from the Save
image center) is unchanged. Its click target, native title alignment, and PNG
export behavior remain the same.

The two toolbox controls are titled "Take snapshot" and "Export to CSV", shown
as hover tooltips below each icon rather than as ECharts' inline `showTitle`
text, which would widen the toolbox and push into the legend. ECharts 6 renders
the feature *name* (`saveAsImage`) by default, so `toolbox.tooltip.formatter`
explicitly returns the title. The tooltip is themed to match the chart tooltip
and uses `confine` so it cannot spill outside the canvas on narrow screens.

Panel mode now continues automatic usage polling at the configured interval.
`panel_usage_poll.cpp` snapshots account data, credentials, and Pause Hours
on the main loop, then runs HTTPS on a FreeRTOS worker. Release/acquire
publication prevents `/api/state` from seeing partially written results.
Only the main loop updates the cache, history, alerts, and RTC; the worker
uses the existing Wi-Fi connection and never powers the radio off.
Automatic jobs honor Pause Hours before each account request. Manual refresh
requests coalesce and override the pause. Polling settings and credential
changes cancel outdated results; unrelated settings keep the poll schedule.
An exit waits for the current request to finish and cancels further accounts.
NTP and reconnect waits are serviced on the loop without a blocking delay.
Host regression checks cover a stalled request with readable cached state,
result publication, errors, cancellation, changed tokens, pause/override,
Wi-Fi loss, and allocation recovery. The firmware build and host worker and
Pause Hours regressions passed. On-board checks after flashing and rebooting
confirmed both accounts returned HTTP 200 on consecutive automatic polls at
the saved two-minute interval. The panel continued serving cached state
during HTTPS requests and published fresh values when the worker finished.
The active Pause Hours window suppressed automatic polling; it was briefly
disabled for the live interval check and restored afterward.

The solid 5-hour series narrowed from 1.6 px to 1.2 px (the dashed 7-day
series stays at 1 px, superseding the 1.4 px figure recorded earlier). Solid
versus dashed already separates the two windows, so the extra weight was only
adding visual noise at full 30-minute resolution; 1.2 px keeps the 5-hour line
the dominant one without it reading as a thick band.

The panel remembers the open page across a browser refresh. `selectView`
writes the view name to `sessionStorage` under `meter-view`, and `showDash`
reads it back, falling back to Usage when the key is missing or names a view
that no longer exists. `sessionStorage` rather than `localStorage` keeps this
per-tab, so a second tab opens on Usage instead of inheriting wherever the
first tab happened to be; a URL hash was rejected because `#accounts` and
`#news` collide with existing element ids and would scroll the page. Both
footers now compute to the same size at every breakpoint — 11 px above
480 px CSS pixels and 10 px below it. The page footer previously dropped to
9 px on narrow screens, half a step smaller than the sign-in footer on the
same phone. A dead `.page-footer{font-size:9px}` rule, overridden by a later
top-level rule, was removed at the same time. Local Chromium checks cover the
reload restoring the open page, and footer parity at six viewport widths.

Device details leads with a Hostname row showing `<hostname>.local`, placed
directly above IP address. The sidebar already printed the mDNS URL, but that
line is easy to miss and the diagnostics list — the place people copy values
from — only had the IP. The value comes from the existing `hostname` field in
`/api/state`, so no firmware API change was needed.
