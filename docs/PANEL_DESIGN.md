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
| `skills-light.png`, `skills-dark.png` | A contextual introductory surface, grouped content, generous spacing, consistent theme pairing | Introductory view headings; grouped sound previews; editorial headline list; matching geometry in both themes |

The screenshots describe two complementary visual styles: editorial
typography on public pages and practical sidebar navigation in the app.
The redesign combines those patterns using locally available fonts and
inline SVG. It does not depend on proprietary fonts or remote assets.

The Claude mark uses the actual irregular vector path from the inline
header SVG on [Claude's website](https://claude.com/), captured on
2026-10-05 in `assets/claude-mark.svg` and embedded in the shared brand
symbol. It replaces the previous symmetrical spoke approximation.

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
| Usage | Both accounts, usage bars, responsive seven-day chart, manual refresh |
| Accounts | Account names and masked token replacement, including save-time API probe results |
| Device | Display/time zone, rotation, Wi-Fi and network scan, complete diagnostics, destructive maintenance |
| Alerts & sound | Poll interval, warning thresholds, quiet hours, volume, six speaker previews |
| News | Feed headlines, publication dates, source link, fetch/stale feedback |

Desktop uses a 224px sidebar and a constrained content column. The sidebar
narrows at 1100px and becomes a sticky header with a scrollable navigation
row at 760px. Below 480px, usage cards stack. Settings rows stack before
controls become cramped. Device diagnostics live in Device details.
Sidebar navigation uses icons and labels without index numbers.
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
survives polling and is applied with Save settings.
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
storage falls back safely. Reduced-motion preference removes transitions
and view-entry motion. Native buttons provide keyboard operation for the
chart legend and Wi-Fi scan results. Form labels, focus outlines, live
messages, and an adaptive skip link support keyboard navigation.

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
81 kB; the compiled firmware uses about 46% of the application partition
and 39% of static RAM. Browser screenshots were inspected in light/dark,
desktop/mobile, and the settings views.

On 2026-10-05, the firmware was uploaded to the ESP32-S3-PICO-1 on COM5;
the uploader verified the flash hash. Serial output confirmed a successful
boot, Wi-Fi connection, HTTP 200 polls for both accounts, and LAN panel
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

The pending refinements restore the standard percentage text color,
share the 6px progress-bar height with the slider track, and remove the
sidebar navigation index numbers and their styling.
Chromium inspection measured the rendered native track at 6px and checked
0%, 50%, and 100% fill/value rendering in both themes. The browser suite
and firmware build passed; the binary contains the exact 80,387-byte page.
These refinements have not yet been flashed: COM5 was absent at upload time
and no USB ports returned during the three-minute board-specific retry.
