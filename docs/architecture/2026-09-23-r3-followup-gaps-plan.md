# R3 follow-up gaps implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.

**Goal:** close the gaps the September 22-23 reviews and the control inventory
uncovered: refused pans that stay blank with no reason, a noisy pacer warning,
transmit sections and a RADE applet that are not gated in a remote window, local
flags and a TUNE button that show stale state after a link loss or a refusal,
DSP settings the Core only applies at the next mode change, and a set of small
cleanups. Notch filter (TNF) live apply and remote NR3 model selection wait for
the operator's design choice and are not in this plan.

**Architecture:** six independent tasks in existing files. Each follows the
surrounding code's pattern: pan status lines for display refusals, the pushed
transmit permission for gating, the -400 no-reading sentinel for meters, the
Core's existing mode-change apply path for DSP options.

**Tech stack:** C++20, Qt 6, Qt Test (off-screen), `RemoteMediaController`,
`DaemonMediaController`, `SetupDialog`, `RadeApplet`, `SliceMeterPump`,
`RadioModel`, `TxApplet`, `StationServer`.

**Spec:** R3 plan requirements R-R3-01, R-R3-08, R-R3-09, R-R3-13, R-R3-21,
R-R3-37; the control matrix
(`2026-09-20-remote-daemon-r3-verification/remote-controls.md`, findings F1-F6);
the crew ledgers of the display limits, polish, media establishment and window
harness plans (September 22-23).

**Controller decisions (settled, within scope):** the receive-only Core refuses
`DspOptions*Tx` settings writes, consistent with its refusal of direct
TransmitModel writes; a local flag with the radio link down shows "-- dBm";
a display refusal outside budget mode shows the budget-mode pan status line and
no toast; RADE's Reset vocoder uses the standard transmit reason; a refused
Tune adds no toast (the MOX refusal already shows one).

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`),
  never `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash
  characters. Stage explicit paths only. Every commit names its R-R3 IDs.
- NereusSDR-original code except where a task cites Thetis; Thetis-derived
  changes follow CLAUDE.md's source-first protocol with `[v2.10.3.15]` cites.
- Operator strings are plain English. The standard transmit reason is
  "Remote transmit controls are not available from this Core yet." Non-breaking
  spaces use the six-character escape (backslash-u00A0), checked with
  `grep -c $'\xc2\xa0'` after editing (the Edit tool decodes typed escapes).
- Local operation is unchanged unless a task says otherwise; every gating test
  has a local half.
- Tests run off-screen by default. Build exact targets, then
  `ctest --test-dir build-integration -R '^(<targets>)$' --no-tests=error --output-on-failure`.
  No unfiltered suite. Never build the `NereusSDR` target. Hardware is off
  limits to implementers.

## What already exists

Read-only investigation 2026-09-23 at 3075a5b9 (private scout notes); key facts:
- The Core's per-pan `rejected` has five keys (`DaemonMediaController.cpp:2297-2317`);
  the GUI's per-pan branch requires six (`RemoteMediaController.cpp:2161-2174`),
  so outside budget mode a refused pan stays blank (`:1257`) or frozen (`:1282`)
  with no reason, and the non-budget refresh clears pan statuses (`:1133-1145`).
- The QWARN "refused invalid display pacer state update" comes from
  `refreshDisplayBudgetPacer` (`DaemonMediaController.cpp:615-631`) at
  teardown: the destructor ends the pacer session (`:390`) and then clears the
  session (`:391`) with the pacer-initialised flag still set (`:762-763`,
  `:776-777`).
- After the display fix wave, only a lone survivor on a shared engine is
  re-granted; two or more pans held at `SharedEngine` stay reduced after the
  pan that sized the engine leaves.
- TX Leveler and TX ALC groups (`DspSetupPages.cpp:267, 335`) never leave the
  window; the nine DSP > Options TX combos (`DspOptionsPage.cpp:321-327`) reach
  the Core and apply. `refreshTransmitPresentation` (`SetupDialog.cpp:457-514`)
  gates whole pages; RxApplet's save-and-restore pattern (`RxApplet.cpp:1334-1363`)
  gates individual controls.
- RADE's profile combo needs a radio MAC a remote model never has
  (`MicProfileManager.cpp:1014-1018`); Reset vocoder needs the window's own RADE
  channel (`RadeApplet.cpp:265-274`); the applet is not in
  `applyRemoteRoleGating` (`MainWindow.cpp:10155-10205`).
- `SliceMeterPump::poll` (`SliceMeterPump.cpp:124-238`) has no link-state gate.
- TUNE: the click handler writes "TUNING..." after a refused `setTune`
  (`TxApplet.cpp:1005-1014`); `MoxController::setTune` marks Tune on before
  keying (`MoxController.cpp:420-424`), and a refused key leaves the flag, the
  tune tone, a CW-to-SSB switch and saved power in place
  (`RadioModel.cpp:14930-15204`); the TUN-off path restores them
  (`:15312-15333`).
- The Core applies stored DSP > Options keys only at the next mode change
  (`RadioModel.cpp:11418-11437` -> `RxChannel::onModeChanged`, `RxChannel.cpp:2379-2396`).

## Task 1: Show why a pan is refused; quiet pacer teardown; re-grant every survivor

**Requirements:** R-R3-01, R-R3-08, R-R3-09, R-R3-37.

**Files:**
- Modify: `src/gui/RemoteMediaController.cpp`, `src/core/session/media/DaemonMediaController.cpp`
- Test: `tests/tst_remote_media_controller.cpp`, `tests/tst_daemon_media_controller.cpp`

**Acceptance:**
- Outside budget mode, a per-pan `rejected` with five keys is honoured: the pan
  shows "Display allocation refused: <reason>" (the budget-mode line), the
  reason is re-shown on each refresh until the pan's request changes or a new
  view is accepted, and no toast is raised. Check first whether "slice removed"
  or "slice stream binding changed" can reach the GUI while its binding still
  exists; if so, do not show a refusal line for a slice the operator removed,
  and say how you told the cases apart.
- The pacer-initialised flag is cleared with the pacer session in the
  destructor (or the session is cleared first, as `onSessionEnded` does), and
  the budget tests fail on the warning (`QTest::failOnWarning`, as
  `tst_daemon_config.cpp:297`). A destroy-while-live case passes without the
  warning.
- After the pan that sized a shared engine leaves, every remaining pan on that
  key held at `SharedEngine` is re-granted (not only a lone survivor); the
  status lines follow; no neighbour's context is renewed without a geometry
  change.

**Verification:**
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_media_controller tst_daemon_media_controller tst_display_budget_contract -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_media_controller|tst_daemon_media_controller|tst_display_budget_contract)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Failing tests for the refusal line, the pacer warning and
  the multi-survivor re-grant.
- [ ] **Step 2:** Implement; run the commands; commit.

## Task 2: Gate transmit sections, the RADE applet and TX DSP writes

**Requirements:** R-R3-21.

**Files:**
- Modify: `src/gui/SetupDialog.{h,cpp}`, `src/gui/setup/DspSetupPages.cpp`,
  `src/gui/setup/DspOptionsPage.cpp`, `src/gui/applets/RadeApplet.{h,cpp}`,
  `src/gui/MainWindow.cpp` (`applyRemoteRoleGating`),
  `src/core/session/StationServer.cpp` (refuse `DspOptions*Tx` writes),
  `docs/architecture/2026-09-20-remote-daemon-r3-verification/remote-controls.md`
- Test: `tests/tst_remote_gui_gating.cpp`, `tests/tst_rade_applet.cpp`,
  `tests/tst_station_session.cpp`

**Acceptance:**
- The TX Leveler and TX ALC groups and the nine DSP > Options TX combos are
  disabled in a remote window without transmit permission, with the standard
  transmit reason as tooltip and accessible description, restored when
  permission returns; receive sections of the same pages stay enabled; local
  mode unchanged.
- The receive-only Core refuses settings writes to the `DspOptions*Tx` keys
  with the same refusal path it uses for TransmitModel writes; RX DSP option
  writes are still accepted.
- RadeApplet gains `setTransmitPermitted`: the profile combo starts denied in a
  remote window and follows `applyRemoteRoleGating`; Reset vocoder shows the
  standard transmit reason when unavailable remotely and no longer looks up the
  window's own WDSP engine in a remote window.
- The control matrix rows for F3 and F5 are updated.

**Verification:**
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_gui_gating tst_rade_applet tst_station_session tst_ui_capability_gating -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_gui_gating|tst_rade_applet|tst_station_session|tst_ui_capability_gating)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Failing gating and refusal tests.
- [ ] **Step 2:** Implement; update the matrix; run the commands; commit.

## Task 3: Local flags show no reading while the radio link is down

**Requirements:** R-R3-13.

**Files:**
- Modify: `src/core/meters/SliceMeterPump.{h,cpp}`
- Test: `tests/tst_slice_meter_pump.cpp`

**Acceptance:**
- When the model's connection state is not Connected (the same rule the window
  uses for the container meters, `MainWindow.cpp:5421-5430`), `poll` writes the
  no-reading value (-400, a core-side named constant) to all three readings of
  every slice and returns before the TX check; the no-channel branch does the
  same. The flag bar then shows "-- dBm".
- `pollLeavesSliceAtDefaultWhenNoWdspChannelExists` is updated to the new rule
  with a comment saying why (a decided behaviour change, not a weakened test);
  a new case covers LinkLost with a live channel and recovery to Connected.
- Remote clients still read the flag level only when ready.

**Verification:**
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_slice_meter_pump tst_remote_meter_poller -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_slice_meter_pump|tst_remote_meter_poller)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Implement with its tests; run the commands; commit.

## Task 4: A refused Tune shows and restores the true state

**Requirements:** R-R3-21 (transmit presentation), local TX safety.

**Files:**
- Modify: `src/gui/applets/TxApplet.cpp`, `src/models/RadioModel.cpp`
- Test: `tests/tst_radio_model_set_tune.cpp`, a TxApplet test (existing file if
  one covers TUNE, otherwise a new `tests/tst_tx_applet_tune_state.cpp`)

**Acceptance:**
- The TUNE button's text and checked state come only from the Tune state and
  refusal signals, never written after the `setTune` call.
- When `RadioModel::setTune(true)` ends with MOX still off (keying refused by
  the band plan or an interlock), it runs the TUN-off path: the Tune flag is
  cleared (button reads "TUNE", unchecked) and the tune tone, the CW-to-SSB
  switch and the saved power are restored, so the next press does not save the
  switched mode as the original.
- No new toast (the MOX refusal already shows one). A successful Tune is
  unchanged; unkey always works.

**Verification:** transmit path, consequential: the refused-keying case fails
first on the current tree.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_radio_model_set_tune tst_capture_admission tst_band_plan_guard_mox_rejection tst_tx_interlock_policy -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_radio_model_set_tune|tst_capture_admission|tst_band_plan_guard_mox_rejection|tst_tx_interlock_policy)$' --no-tests=error --output-on-failure
```
(Add the TxApplet test target to both lines.) Hardware (pending, bench with a
dummy load and explicit authorization): a band-plan-refused Tune leaves the
radio and the button in their prior state.

**Execution note (advisory):** opus (TX path).

- [ ] **Step 1:** Failing refused-Tune tests (model and applet).
- [ ] **Step 2:** Implement; run the commands; commit.

## Task 5: The Core applies DSP options without waiting for a mode change

**Requirements:** R-R3-21.

**Files:**
- Modify: `src/core/session/StationServer.cpp` (or the settings proxy server's
  accepted-write hook), `src/models/RadioModel.{h,cpp}`
- Test: `tests/tst_dsp_options_per_mode_apply.cpp`, `tests/tst_station_session.cpp`

**Acceptance:**
- After an accepted RX `DspOptions*` settings write from a remote window, the
  Core re-runs the existing mode-change apply (`RxChannel::onModeChanged`) for
  each slice whose mode group the key belongs to, coalescing a burst of keys
  into one apply (name the coalescing interval and its source).
- Unrelated keys trigger nothing; TX keys are refused by Task 2 and trigger
  nothing here; local mode unchanged.

**Verification:**
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_dsp_options_per_mode_apply tst_station_session nereusd -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_dsp_options_per_mode_apply|tst_station_session)$' --no-tests=error --output-on-failure
```
Hardware (pending, session S1): a buffer change from the remote window is heard
on the Core without a mode change.

**Execution note (advisory):** opus. Runs after Task 2 (shared StationServer).

- [ ] **Step 1:** Failing apply test; implement; run the commands; commit.

## Task 6: Small cleanups from the reviews

**Requirements:** as each item's original task.

**Files:** as listed per item.

**Acceptance:**
- `tests/tst_meter_item_no_reading.cpp:106` pins the TX needle's number, not
  only that it is not "-- dBm"; the `formatValue` doc comment sits above
  `formatValue` (`MeterItem.cpp` around 190-208); `SignalTextItem::noReadingText()`
  no longer hides `MeterItem::noReadingText(MeterUnit)` (rename one).
- DeviceCard no longer accumulates stale "(not available)" device items or
  odd buffer entries across reloads (`DeviceCard.cpp` around 458-462, 626-628),
  with a test in `tests/tst_device_card.cpp`.
- `src/core/daemon/DaemonConfig.h:106` rewrapped; the stale "coarse timer"
  comment in `tests/tst_remote_media_controller.cpp` around 666 corrected.
- Stale Thetis cites in `src/core/P2RadioConnection.cpp` (network.c:655-666
  near lines 56, 575, 2349, and `[v2.10.3.13]` near 2411, 2603) re-verified
  against `/Users/j.j.boyd/Thetis` at v2.10.3.15 and restamped with the correct
  line ranges; every existing inline author tag kept.

**Verification:**
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_meter_item_no_reading tst_device_card tst_remote_media_controller tst_p2_established_silence nereusd -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_meter_item_no_reading|tst_device_card|tst_remote_media_controller|tst_p2_established_silence)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus (Thetis cite verification).

- [ ] **Step 1:** Implement each item; run the commands; commit.

## Task 7: Core host telemetry cleanups

Added after the Core host telemetry review (verdict Ready to merge: Yes; minors).

**Requirements:** R-R3-32, R-R3-33, R-R3-01 (test only).

**Files:**
- Modify: `src/gui/RemoteDiagnosticsDialog.cpp` (tooltip), `src/gui/RemoteTelemetryController.cpp`
  (log field name), `docs/architecture/2026-09-21-core-telemetry-design.md` (version line)
- Test: `tests/tst_remote_diagnostics.cpp`, `tests/tst_remote_telemetry.cpp`,
  `tests/tst_host_telemetry_sampler.cpp`, `tests/tst_daemon_media_controller.cpp`

**Acceptance:**
- The temperature graph's tooltip never shows a previous Core's sensor name: with no name in the
  current view it shows the generic tooltip (`RemoteDiagnosticsDialog.cpp` around 409); test it.
- The soak log line names the process CPU field so it cannot be read as per-core (for example
  `coreProcessCpuPercentOfAllCpus`); update any test that parses the line.
- One fixture thermal zone is a symlink to a directory, as real sysfs zones are, and is discovered.
- `minorNineSpectrumContextsReportTheGrant` is renamed to what it runs, and a raw minor-9 hello to the
  minor-10 Core (like the minor-8 case near line 3100) proves a minor-9 peer still gets the grant.
- The core telemetry design doc records protocol minor 10 and `stationTelemetryVersion = 2` in one line.

**Verification:**
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_diagnostics tst_remote_telemetry tst_host_telemetry_sampler tst_daemon_media_controller -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_diagnostics|tst_remote_telemetry|tst_host_telemetry_sampler|tst_daemon_media_controller)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Implement each item with its test; run the commands; commit.

## Task 8: Cleanups from the follow-up review

Added after the follow-up batch review (verdict Ready to merge: Yes; minors).

**Requirements:** R-R3-01, R-R3-09, R-R3-21, R-R3-37.

**Files:**
- Modify: a core header shared by `src/core/session/media/DaemonMediaController.cpp` and
  `src/gui/RemoteMediaController.cpp` (retirement reason constants), `src/models/RadioModel.cpp`
  (teardown releases a manual MOX; mode-group mapping reuse), `src/core/RxChannel.{h,cpp}` (export the
  mode-group helper if that is the cleanest single source), `src/gui/setup/DspOptionsPage.cpp`,
  `docs/architecture/2026-09-20-remote-daemon-r3-verification/remote-controls.md`
- Test: `tests/tst_remote_media_controller.cpp`, `tests/tst_daemon_media_controller.cpp`,
  `tests/tst_radio_model_set_tune.cpp`, `tests/tst_tx_applet_tune_state.cpp`

**Acceptance:**
- The Core's "slice removed" and "slice stream binding changed" retirement reasons (and "source retune no
  longer covers requested crop" if the window treats it the same way) come from named constants in one core
  header used by both the Core's senders and the window's filter; the window test builds its payload from the
  same constants.
- `sharedEngineRegrantsEverySurvivorWhenItsSizerLeaves` gains a non-held neighbour on the grown engine with its
  one expected renewal (a real bin-count change), recording the decision.
- A disconnect during Tune leaves the TUNE button reading "TUNE": `teardownConnection` releases a manual MOX
  through the normal Tune-off path before it clears the tuning state; a test covers disconnect mid-Tune. If a
  refused key leaves `PttMode::Manual` set, clear it on the refusal path with its test (say what Thetis does if
  the change touches ported logic; source-first rules apply).
- The control matrix records that the Core applies accepted RX DSP Options writes and removes after the 50 ms
  coalesce (hardware S1 pending) and lists the remove-refusal and apply tests.
- One mode-group mapping serves RadioModel, RxChannel and DspOptionsPage (or a test pins the three copies
  equal if a shared helper would cross the core/GUI boundary); the unused raw `new QApplication` in
  `tests/tst_tx_applet_tune_state.cpp` is removed.

**Verification:**
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_media_controller tst_daemon_media_controller tst_radio_model_set_tune tst_tx_applet_tune_state tst_dsp_options_per_mode_apply tst_capture_admission -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_media_controller|tst_daemon_media_controller|tst_radio_model_set_tune|tst_tx_applet_tune_state|tst_dsp_options_per_mode_apply|tst_capture_admission)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus (touches the Tune path).

- [ ] **Step 1:** Implement each item with its test; run the commands; commit.
