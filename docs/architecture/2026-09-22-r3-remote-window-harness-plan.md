# R3 remote window harness and control inventory implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.

**Goal:** close R3 section 4c's open software items: a complete, owner-by-owner
inventory of what the remote GUI shows with the gating it implies; one test
harness that drives the real main window's connection actions against a
loopback Core instead of checking that slots exist; and the last slice-flag
lifetime leftover, where presentation handlers pile up on every rehome.

**Architecture:** Task 1 audits every visible menu, applet and Setup page,
records its owner and acceptance case in the existing control matrix, and
fixes the gating the audit finds. Task 2 builds a reusable fixture of one real
`MainWindow` connected to an in-process Core over the existing loopback
transport and uses it for the connection, reconnect and snapshot cases,
including the extra-slice startup reproduction. Task 3 moves the remaining
MainWindow-context flag lambdas onto the flag widget so a rehome retires them.

**Tech stack:** C++20, Qt 6 widgets, Qt Test (off-screen), `MainWindow`,
`StationServer`/`StationClient`, `tests/fakes/LoopbackTransport`,
`tests/fakes/LoopbackStationLink`, `tests/fakes/RemoteAudioSessionHarness.h`,
`SliceFlagPresentationBinding`.

**Spec:** [R3 plan](2026-09-20-remote-daemon-r3-plan.md) section 4c (lines
520-620): inventory by owner (547-551), connection actions and TX gating
(561-571), extra-slice startup (572-575), injected harness (576-577), R-R3-30
(578+); requirements R-R3-16, R-R3-17, R-R3-21, R-R3-24, R-R3-30; the
[control matrix](2026-09-20-remote-daemon-r3-verification/remote-controls.md)
and [VFO lifetime evidence](2026-09-20-remote-daemon-r3-verification/vfo-lifetime.md).

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`),
  never `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash
  characters. Stage explicit paths only. Every commit names its R-R3 IDs.
- NereusSDR-original code. Keep existing headers; new files carry the house
  header plus `// no-port-check: NereusSDR-original. <reason>`.
- Automatic panel callbacks stay separate from explicit operator connect; the
  automatic path never dials. Local operation, receive controls and the
  separate local-DSP resource gate are preserved. TX gating consumes the
  existing `txPermitted` capability and never implements or removes R4 station
  TX commands. Do not delete an existing station slice to hide an unintended
  create.
- Operator strings are plain English (no roadmap, protocol or capability
  names in overlays, tooltips or refusal text).
- Tests run off-screen by default. Build exact targets, then
  `ctest --test-dir build-integration -R '^(<targets>)$' --no-tests=error --output-on-failure`.
  No unfiltered suite. Never build the `NereusSDR` target (the controller
  builds the app at gates). Hardware is off limits.

## What already exists

Read-only investigation, 2026-09-22 (line numbers at 6bf8d086):
- The control matrix `remote-controls.md` says it is not a full audit
  (lines 3-6); only Setup's local-DSP sweep is tested
  (`tests/tst_remote_gui_gating.cpp:298-372`). The named TX pass is done
  (`remote-controls.md:110-129`; `src/gui/SetupDialog.cpp:420-446`).
  Candidates: the CFC page (`SetupDialog.cpp:693-698`) and Test > Two-Tone IMD
  (`:894-896`) are not marked as transmit pages.
- Real-window tests cover Tools > Network Diagnostics
  (`tests/tst_remote_connection_controls.cpp:167-218`) and Radio > Connect /
  switching (`tests/tst_gui_connection_controller.cpp:72-147`); the title is
  tested on the bare widget only (`tst_remote_connection_controls.cpp:220-232`).
- Every operator connect entry point reaches `connectionRequestedByOperator`
  (`src/gui/MainWindow.cpp:1177-1193`; entry points 658-668, 2500, 4152, 8086,
  9115); the automatic panel path never dials (10023-10040). After a manual
  Disconnect the automatic reopen path runs (11307-11313) and in picker mode
  opens Connections (10025-10028).
- The extra-slice startup test drives a copied static hook
  (`tst_remote_connection_controls.cpp:525-562`) with a boolean "delayed
  snapshot" (578-587); the real hook (`MainWindow.cpp:3390-3395, 10797-10811`)
  is untested.
- R-R3-30: 14 bindings use the flag as receiver
  (`src/gui/SliceFlagPresentationBinding.cpp:23-66`, `MainWindow.cpp:1737`),
  but the QPointer-guarded lambdas at `MainWindow.cpp:1613-1723` use MainWindow
  as context and are registered again on every rehome (3869-3892), so
  frequency, mode and filter handlers accumulate one copy per rehome.

## Task 1: Visible control inventory by owner and the gating it finds

**Requirements:** R-R3-21.

**Files:**
- Modify: `docs/architecture/2026-09-20-remote-daemon-r3-verification/remote-controls.md`
  (full inventory table), `src/gui/SetupDialog.cpp` and any page or applet the
  audit finds mis-gated
- Test: `tests/tst_remote_gui_gating.cpp`

**Interfaces:**
- Consumes: the existing remote gating helpers and `txPermitted`.
- Produces: no new API.

**Acceptance:**
- The matrix lists every menu item, applet control group and Setup page or
  leaf visible in a remote receive session, each with its owner (GUI-local,
  station-backed, or unavailable in remote receive), the handler or property
  that implements it, and its acceptance case (a named test, or "hardware
  pending" with the session it belongs to). Generic mirrored DSP setters are
  traced before being called broken.
- Every item the audit marks unavailable or transmit-only is gated with a
  plain reason and covered in `tst_remote_gui_gating` (the CFC page and Test >
  Two-Tone IMD at least, unless the audit shows with evidence that they are
  already gated some other way).
- Nothing local-only changes behaviour in a local session (existing local
  tests pass).

**Verification:** inventory plus functional gating checks.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_gui_gating tst_ui_capability_gating -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_gui_gating|tst_ui_capability_gating)$' --no-tests=error --output-on-failure
```
(Add every other existing gating test the change touches, for example
`tst_hardware_page_capability_gating` or `tst_pan_layout_dialog_gating`.)

**Execution note (advisory):** opus (judgment across the whole GUI).

- [ ] **Step 1:** Audit and write the table.
- [ ] **Step 2:** Gate what it finds with tests; run the commands; commit.

## Task 2: Real main-window harness for connection and snapshot paths

**Requirements:** R-R3-16, R-R3-17, R-R3-21, R-R3-24.

**Files:**
- Create: `tests/fakes/RemoteWindowHarness.{h,cpp}` (one real `MainWindow`, an
  in-process `StationServer` and `DaemonMediaController` or the lightest
  station fixture the existing tests already use, connected over
  `LoopbackTransport`), `tests/tst_remote_window_harness.cpp`
- Modify: `tests/tst_remote_connection_controls.cpp` (replace the copied
  static hook test with the real path), `tests/CMakeLists.txt`
- Modify (only where a case exposes a defect): `src/gui/MainWindow.cpp`,
  `src/gui/GuiConnectionController.cpp`, `src/gui/RemoteConnectionController.cpp`

**Interfaces:**
- Consumes: existing fakes (`LoopbackTransport`, `LoopbackStationLink`,
  `RemoteAudioSessionHarness.h`), isolated settings profile helpers.
- Produces: a harness other tests can reuse: start a Core, show the window,
  trigger an action by its real `QAction` or widget, hold or release the
  Core's first snapshot, drop and restore the link, read what the window shows.

**Acceptance (each through the real window's actions, never by calling a
slot directly):**
- Title bar, station block, a disconnected pan and the Setup entry point each
  start an explicit connect; the automatic panel path never dials.
- Cancel during reconnect backoff stops the retry and leaves the window
  disconnected with its persistent status.
- Manual Disconnect followed by the automatic reopen path: pin what the
  window does in picker mode (opens Connections) and in direct mode, and say
  in the report which of those is intended by the design text.
- Extra-slice startup: with the Core's first snapshot held back, the window's
  real `populateEmptyPans` hook creates no slice before the snapshot arrives;
  after it arrives the slice count equals the station's; a reconnect with a
  delayed snapshot repeats that. Hydration and layout restore are kept apart
  from an explicit operator create.
- A capability change from the Core (for example `txPermitted`) updates the
  window's gating without a reconnect.
- The copied static-hook test is removed because the real path now covers it.

**Verification:** GUI routing with functional evidence, off-screen.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_window_harness tst_remote_connection_controls tst_gui_connection_controller -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_window_harness|tst_remote_connection_controls|tst_gui_connection_controller)$' --repeat until-fail:3 --no-tests=error --output-on-failure
```
Hardware (pending, operator session S1): the same entry points on the real
GUI against the Rock.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Harness and the cases; record which fail.
- [ ] **Step 2:** Fix what they expose; run the commands; commit.

## Task 3: Retire every flag presentation handler on rehome

**Requirements:** R-R3-30.

**Files:**
- Modify: `src/gui/MainWindow.cpp` (the lambdas at 1613-1723 and their
  registration at 3869-3892), `src/gui/SliceFlagPresentationBinding.{h,cpp}`
  (if the lambdas move there)
- Test: `tests/tst_slice_flag_presentation_lifetime.cpp`

**Interfaces:**
- Consumes: `SliceFlagPresentationBinding`.
- Produces: no new API.

**Acceptance:**
- Each frequency, mode and filter presentation handler uses the flag widget
  (or an object it owns) as its connection context, so destroying or
  rehoming the flag retires it.
- After rehoming one slice five times, one change to its frequency, mode or
  filter runs each presentation handler exactly once, and the flag shows the
  new value. A deleted flag never receives a callback.
- Valid station updates still reach the surviving flag (no suppression).

**Verification:** lifetime regression that fails first on the current tree
(handler count grows with rehomes).
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_slice_flag_presentation_lifetime -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^tst_slice_flag_presentation_lifetime$' --no-tests=error --output-on-failure
```
Hardware (pending, session S1): a live two-pan to one-pan rehome.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Handler-count regression (fails today).
- [ ] **Step 2:** Move the context; run the commands; commit.
