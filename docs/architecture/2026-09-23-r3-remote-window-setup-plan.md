# R3 remote window Setup and Connections implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.
> Runs in the integration worktree (lane A). Tasks 1 and 2 run after the lane B
> carry, so the lane B wording helpers (`OperatorReasonText`,
> `tests/OperatorWording.h`) are present; Task 3 adds no user-visible text and may
> run before the carry.

**Goal:** a remote window's Setup treats this computer's settings as this
computer's: the audio devices work whether the window is connected or not,
settings stored on the Core are clearly unavailable while disconnected, and the
Connections window opens only when the operator disconnects.

**Architecture:** every Setup page declares its scope (this computer, Core, or
mixed) at registration, so the automatic local-processing gate keeps catching
real signal-processing pages but can never silently catch a this-computer page
again. This computer's audio devices are reached through an accessor the gate
does not count, allowed only in listed files. The dialog always exists in a
remote window and tells Core and mixed pages whether Core settings are
available. The Connections window opens from the operator's own Disconnect, not
from every disconnected state.

**Tech stack:** C++20, Qt 6 widgets, Qt Test (off-screen), `SetupDialog`,
`SetupPage`, `RadioModel`, `AudioEngine`, `SettingsProxy`, `StationClient`,
`RemoteConnectionController`, `GuiConnectionController`, `MainWindow`,
`RemoteWindowHarness`.

**Spec:** R3 plan requirements R-R3-10, R-R3-16, R-R3-17, R-R3-21, R-R3-23,
R-R3-36, R-R3-38. Operator answers of 2026-09-23: a disconnected remote window
keeps this computer's settings usable while Core settings stay disabled with
"Connect to the Core to change these."; Connections opens only after the
operator's own Disconnect (on link loss the window stays, retries and says it is
reconnecting; when the Core reports its radio offline the window stays and says
so); Setup's audio Devices page picks this computer's devices like it always has,
connected or not, the title-bar picker staying a shortcut to the same setting.
Scout notes with file:line (at 3f3d48fc; re-locate by text):
`/Users/j.j.boyd/.config/nereus/work/r3-remote-vax-audio-pages-scout-2026-09-23.md`
sections A, C, E, F and "Other bug".

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`),
  never `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash
  characters. Stage explicit paths only. Every commit names its R-R3 IDs.
- Local (direct radio) operation unchanged: every page behaves as today when the
  window owns its own radio.
- Operator wording: plain user words; every new or changed string passes
  `OperatorWording::isPlain`; Core reasons shown to the user go through
  `OperatorReasonText`. Wire strings never change.
- Nothing is written to the Core while the window is disconnected.
- Tests run off-screen. Build exact targets, then
  `ctest -R '^(...)$' --no-tests=error --output-on-failure`. No unfiltered suite.
  Never build the `NereusSDR` target. No hardware.

## What already exists

- The local-processing gate in `SetupDialog::realizePage` (R2 Task 20) counts
  `RadioModel::audioEngine()`, `wdspEngine()` and `receiverManager()` hand-outs
  during a page factory and disables the page on a remote model. The audio
  backend strip helper calls `audioEngine()` itself, so all five wrapped Audio
  pages trip it (scout fact 1).
- A remote window's `AudioEngine` drives this computer's speakers; the remote
  player re-reads `audio/Speakers` on `AudioEngine::speakersConfigChanged`; the
  title-bar picker writes the same setting (scout fact 2).
- `markRemoteUnavailable`, `refreshTransmitPresentation`,
  `SetupPage::setTransmitPermitted` (the model for a per-page availability push),
  `applyRemoteRoleGating` (pushes to an open dialog), the settings proxy's
  not-ready state on disconnect (scout facts 19-20).
- `RemoteConnectionController::disconnectFromStation` records an operator
  disconnect; wording "Retrying Core (attempt %1)" and "Radio offline" exists
  (scout facts 21-24).
- `scripts/verify-no-gui-dsp-access.py` check 2: the allow-list precedent.

## Task 1: Pages declare their scope; this computer's audio devices work in a remote window

**Requirements:** R-R3-21, R-R3-23, R-R3-36, R-R3-10.

**Files:**
- Modify: `src/gui/SetupDialog.{h,cpp}` (`enum class SetupScope { ThisComputer,
  Core, Mixed }` as a required `registerPage` argument with no default; the gate
  applies to Core and Mixed pages as today; a ThisComputer page that reaches an
  audited accessor is disabled, logs at critical and names the accessor; the
  strip helper uses the unaudited accessor), `src/models/RadioModel.{h,cpp}`
  (`AudioEngine* localAudioDevices()`: the same engine, not counted by the audit),
  `src/gui/setup/AudioDevicesPage.cpp`, `src/gui/setup/AudioTxInputPage.cpp`
  (this computer's PC microphone device, backend, buffer and Test Mic usable in a
  remote window; the radio's own microphone hardware controls stay under the
  transmit gate with its existing reason), `src/gui/setup/AudioAdvancedPage.cpp`
  (in a remote window Reset removes only this computer's `audio/*` keys and
  recreates no VAX outputs), `src/gui/widgets/MasterOutputWidget.cpp` (save the
  speaker choice before announcing it), `src/gui/MainWindow.cpp` (no VAX first-run
  check in a remote window, as the Linux audio first-run already skips),
  `scripts/verify-no-gui-dsp-access.py` (a check that `localAudioDevices()` is
  called only from the strip helper, `AudioDevicesPage`, `AudioTxInputPage`, the
  title-bar wiring and `RemoteMediaController`),
  `docs/architecture/2026-09-20-remote-daemon-r3-verification/remote-controls.md` (the Setup rows the change affects)
- Test: `tests/tst_remote_gui_gating.cpp` (the sweep becomes scope-aware; the
  Devices row moves to enabled), `tests/tst_settings_scope.cpp`,
  `tests/tst_master_output_widget.cpp`, an Advanced Reset case

**Interfaces:**
- Produces: `SetupScope`; `SetupDialog::registerPage(parent, label, SetupScope,
  factory, ...)`; `RadioModel::localAudioDevices()`.

**Acceptance:**
- Every page registration names a scope; a registration without one does not
  compile. Audio > Devices is ThisComputer; TX Input and Advanced are Mixed; VAX,
  TCI and TX Profile keep today's behaviour in this plan (the receiver audio plan
  changes VAX and TCI).
- Remote window, connected: Devices is enabled; choosing a Speakers device
  restarts remote audio on it and the remote audio status names the new device
  (not the previous one); the microphone choice is saved to `audio/TxInput/*`;
  Test Mic opens this computer's microphone and meters it; the radio microphone
  hardware controls are disabled with the transmit reason.
- Advanced Reset in a remote window leaves every Core-held key untouched and
  creates no VAX output; local Reset unchanged.
- A ThisComputer page that reaches an audited accessor fails the sweep test
  (prove it with a deliberate test page).
- The script fails on a `localAudioDevices()` call outside the allow-list.
- Local mode: every page enabled exactly as today (existing gating tests stay
  green).

**Verification:** UI gating and settings ownership, off-screen.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_gui_gating tst_settings_scope tst_master_output_widget -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_gui_gating|tst_settings_scope|tst_master_output_widget)$' --no-tests=error --output-on-failure
python3 scripts/verify-no-gui-dsp-access.py
```
(Setup gating cases live in `tst_remote_gui_gating`; the title-bar order case goes
in `tst_master_output_widget`; the Advanced Reset case goes in whichever existing
test covers `AudioAdvancedPage`, or a new `tst_audio_advanced_page` added to
`tests/CMakeLists.txt` and to this command.)

**Execution note (advisory):** opus.

- [ ] **Step 1:** Scope enum, accessor, gate change and script check with tests.
- [ ] **Step 2:** Devices, TX Input, Advanced Reset, title-bar order, first-run
  skip, remote-controls rows; run the commands; commit.

## Task 2: Setup while the remote window is disconnected

**Requirements:** R-R3-21, R-R3-10, R-R3-17.

**Files:**
- Modify: `src/gui/MainWindow.cpp` (`createSetupDialog` always creates the dialog
  in a remote window; `applyRemoteRoleGating` pushes availability),
  `src/gui/SetupDialog.{h,cpp}` (`setStationSettingsAvailable(bool, QString
  reason)`; Core pages disabled with the reason while unavailable and not built
  before the first settings snapshot; keep a copy of each Core and Mixed page's
  factory and rebuild a realized page on each new snapshot, guarding re-entry),
  `src/gui/setup/SetupPage.h` (`setStationSettingsAvailable` with a do-nothing
  default, like `setTransmitPermitted`), the Mixed pages from Task 1 (their Core
  controls follow availability), `docs/architecture/2026-09-20-remote-daemon-r3-verification/remote-controls.md`
- Test: `tests/tst_remote_gui_gating.cpp` (a per-page table on a disconnected
  window, replacing the refusal expectation), `tests/tst_settings_scope.cpp` (every
  key literal on a ThisComputer page classifies as this computer's), a harness case
  in `tests/tst_remote_window_harness.cpp`

**Interfaces:**
- Consumes: Task 1's `SetupScope`.
- Produces: `SetupDialog::setStationSettingsAvailable(bool, QString)`,
  `SetupPage::setStationSettingsAvailable(bool, QString)`.

**Acceptance:**
- A disconnected remote window opens Setup (no "not ready" toast); ThisComputer
  pages are usable; Core pages show "Connect to the Core to change these." and
  are disabled; Mixed pages disable only their Core controls with that reason.
- A Devices change made while disconnected sticks and is used on reconnect.
- Nothing is sent to the Core while disconnected (assert the proxy writes).
- After reconnect Core pages are enabled and show the Core's values (a page
  realized before the first snapshot is rebuilt, not left with stale values).
- Local mode unchanged.

**Verification:** UI state and settings ownership, off-screen, with the remote
window harness.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_gui_gating tst_settings_scope tst_remote_window_harness -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_gui_gating|tst_settings_scope|tst_remote_window_harness)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus. Requires Task 1.

- [ ] **Step 1:** Availability push, page rebuild and the dialog in every state,
  with tests; run the commands; commit.

## Task 3: Connections opens only after the operator's Disconnect

**Requirements:** R-R3-16, R-R3-17, R-R3-38.

**Files:**
- Modify: `src/gui/RemoteConnectionController.{h,cpp}` (emit
  `operatorDisconnected()` from the operator's Disconnect),
  `src/gui/MainWindow.cpp` (open Connections on that signal: with the picker,
  `connectionsRequested`; without it, `showRemoteConnectionPanel`, which never
  dials; the automatic open on a disconnected state stays for local models only),
  `src/gui/GuiConnectionController.cpp` (its Disconnect paths reach the same
  signal), `tests/fakes/RemoteWindowHarness.{h,cpp}` (a hook that makes the Core report
  its radio offline), `docs/architecture/2026-09-20-remote-daemon-r3-verification/remote-controls.md`
- Test: `tests/tst_remote_window_harness.cpp`,
  `tests/tst_gui_connection_controller.cpp`, `tests/tst_gui_session_coordinator.cpp`

**Interfaces:**
- Produces: `RemoteConnectionController::operatorDisconnected()`.

**Acceptance:**
- Each operator Disconnect path (Radio > Disconnect, the Connections window, the
  Core panel) opens Connections exactly once in picker mode and shows the
  connection panel in direct mode, with no new dial (still one accepted
  connection).
- Link loss: nothing opens; the title bar says "Retrying Core (attempt 1)"; the
  redial happens.
- Core reports its radio offline: nothing opens; the station block says
  "Radio offline".
- Local models keep today's behaviour.

**Verification:** connection state machine, off-screen, harness plus controller
tests.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_window_harness tst_gui_connection_controller tst_gui_session_coordinator -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_window_harness|tst_gui_connection_controller|tst_gui_session_coordinator)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus. Independent of Tasks 1-2 (different files
apart from `MainWindow.cpp` and the controls document).

- [ ] **Step 1:** Signal, open rule, harness hook and tests; run the commands;
  commit.
