# Temporary alert view switch

When a usage event fires, the ePaper switches to the view for the account that
raised it, then returns to the default view on the next poll.

## Motivation

Alerts are audible only. A warning sound tells you something happened but not
which account or which window, and the display keeps showing whatever view was
last selected. Switching to the relevant account's view makes the alert
self-explanatory at a glance.

## Behavior

An event is any of the three edge triggers `alerts.cpp` already detects:
warning threshold crossed, window depleted, or window reset.

| Condition | Display |
| --------- | ------- |
| Exactly one account fired | That account's single-account view |
| Both accounts fired | Unchanged (`Dual` is already the relevant view) |
| Quiet hours active | Unchanged — suppressed with the sounds |
| Panel mode | Unchanged — the screen is showing the URL and PIN |

The switch targets `MeterView::Account1` or `MeterView::Account2`, the large
per-window percentage views, not the history charts. The chart views are gated
on `historyHasData()` and are harder to read at a glance.

The view reverts to `firstView()` on the next poll, so an alert view lasts one
poll interval — 1 to 5 minutes depending on `poll_min`.

The behavior is always on. There is no setting, no NVS key, and no panel or
serial control.

### Manual selection is preserved

`view` is `RTC_DATA_ATTR` and survives deep sleep, so a view chosen with `BOOT`
persists across timer wakes indefinitely. That behavior must not change. Only
alert-driven switches revert, which requires distinguishing the two.

## Design

### `alerts.h`

`checkAlerts()` splits into evaluation and playback so the caller can choose a
view before the single ePaper refresh, and so the sound plays against the
correct screen rather than the previous one.

```cpp
// One evaluation pass: NVS bookkeeping done, sounds queued but not played.
struct AlertOutcome
{
  bool fired[settings::CLAUDE_TOKEN_COUNT]; // accounts that raised an event this pass
  uint8_t firedCount;
  bool silenced;                            // quiet hours suppressed the sounds
  const char *sounds[settings::CLAUDE_TOKEN_COUNT * 4];
  size_t soundCount;
};

AlertOutcome evaluateAlerts(const AccountUsage accounts[settings::CLAUDE_TOKEN_COUNT], time_t now);
void playAlertSounds(const AlertOutcome &outcome);
```

The queued sounds travel in the struct rather than in module state, so
`playAlertSounds()` carries no hidden requirement to be called immediately
after `evaluateAlerts()`.

The sound array is sized at four per account: two windows, each contributing at
most one level sound (warning and depleted are mutually exclusive in a single
pass) and one reset sound. This matches the existing private `MAX_SOUNDS`.

`evaluateAlerts()` keeps the current ordering: NVS state updates land before
the quiet-hours check, so a silenced alert stays silenced rather than firing
when the window ends.

### `main.cpp`

View policy stays next to the existing view helpers. A new `alertView(int
account)` maps account 0 to `MeterView::Account1` and account 1 to
`MeterView::Account2`, guarded by `viewAvailable()`.

The switch condition is one expression:

```
!outcome.silenced && outcome.firedCount == 1
```

`firedCount == 1` produces the both-accounts-stay-on-Dual rule without a
severity table. `silenced` produces quiet-hours suppression.

A new `RTC_DATA_ATTR bool viewFromAlert` records that the current view was set
by an alert. It is set only when the view actually changed, so a single-account
device — where `firstView()` already returns that account's view — does not
schedule a pointless revert.

`pollAndShow()` becomes:

```
revert if viewFromAlert
  -> poll
  -> historyRecord
  -> evaluateAlerts
  -> switch view if the condition holds
  -> render (once)
  -> playAlertSounds
```

The revert runs before the poll so it happens even when the poll fails.

### Clearing the flag on manual selection

`viewFromAlert` must clear wherever the user or the firmware sets the view
deliberately:

- `main.cpp:804`, `main.cpp:1024`, `main.cpp:1298` — the three `view =
  nextView(view)` sites
- `main.cpp:1173` — `exitPanelMode()`'s `view = firstView()`

The three `nextView` sites fold into a `cycleView()` helper that advances the
view and clears the flag together. This replaces triplicated logic in code the
change already touches.

### Call sites

| Site | Context | View switch |
| ---- | ------- | ----------- |
| `main.cpp:327` (`pollAndShow`) | Normal and debug | Yes |
| `main.cpp:388` (`servicePanelUsagePolling`) | Web Panel | No |
| `main.cpp:708` (serial `usage`) | Debug, explicit | Yes |

Panel mode is excluded because the ePaper is displaying the hostname, IP, and
sign-in PIN. Replacing it with a usage view would leave no way to read the PIN.

Debug mode needs no special handling. It never deep-sleeps, but `loop()` still
calls `pollAndShow()` on schedule, so the revert happens on the next poll
exactly as in normal mode. Framing the revert around the poll rather than the
wake is what makes the two modes share one path.

## Edge cases

- **Single account configured.** `firstView()` already returns that account's
  view, so the switch is a no-op and `viewFromAlert` stays clear.
- **Poll fails.** The revert already ran, so the flag is clear. `pollAndShow()`
  returns early without drawing, leaving the previous image until the next
  successful render. This matches current failure behavior.
- **Target view unavailable.** `viewAvailable()` guards the assignment. In
  practice `fired[i]` implies a configured token and a successful poll, which is
  the same condition, so the guard is defensive only.
- **Cold boot.** `viewFromAlert = false` initializes alongside the adjacent
  `view = MeterView::Dual`.
- **Panel Refresh now.** Routed through the panel path, so no switch. Consistent
  with the rest of panel mode.

## Verification

The view policy stays in `main.cpp` with `firstView()`, `nextView()`, and
`viewAvailable()`, none of which have unit tests today. A `!silenced &&
firedCount == 1` condition does not justify a new translation unit and a stub
harness, so verification is on-device through existing serial commands.

1. `warn5h 50` (at or below current usage), then `alerts clear`
2. `usage` — expect a switch to the triggering account's view and the warning
   sound
3. Wait one poll interval — expect a revert to the dual view
4. Press `BOOT` to select a view, wait one poll interval — expect the manual
   selection to persist. This is the regression guard for the behavior described
   under "Manual selection is preserved".
5. `quiet 00-23`, `alerts clear`, `usage` — expect no switch and no sound
6. Long-press `BOOT` to enter panel mode, trigger an alert — expect the PIN
   screen to stay up

## Out of scope

- A setting to disable the behavior
- Switching to the history chart views
- Severity ranking between accounts
- `main.cpp:705-708`, the serial `usage` path, does not call `historyRecord()`
  unlike `pollAndShow()`. Confirm during implementation whether this is
  deliberate; do not change it as part of this work.

## Documentation to update

- `docs/MODES.md` — Normal mode section, alongside the existing view-cycling notes
- `docs/ALERT_SOUNDS.md` — the alert events now also drive the display
- `README.md` — the Operation section's description of views
