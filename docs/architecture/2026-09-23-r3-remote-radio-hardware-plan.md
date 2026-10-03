# R3 radio hardware in a remote window implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.
> Runs in the integration worktree (lane A) after the remote window Setup plan
> (its `SetupScope` and Setup-while-disconnected work touch the same dialog).

**Goal:** a remote window controls the Core's radio hardware the way a local
window controls its own: it knows exactly which radio the Core has, and the
attenuator, preamp, auto-attenuate, overload indication, receive antennas, OC
receive pins, the Hermes Lite 2 filter board, calibration and the receive sample
rate all work, applied by the Core live and saved on the Core.

**Architecture:** the Core sends its radio's model, protocol and address in the
capabilities, so the window uses the real capability row (never a guessed board)
and every page and applet that follows the radio refreshes once per identity
change. The Core already owns a step attenuator; a mirrored Core-side object
exposes it the way PureSignal settings are exposed, and a second one exposes the
Alex receive settings. Hardware Config pages write through a Core-side hardware
apply step (beside the existing DSP > Options apply) that updates the Core's own
controllers live; raw hardware keys for any radio but the Core's own are refused.
Transmit-side hardware (PA, TX antennas and relays, attenuation on TX, User Dig
Out) follows the transmit permission until remote transmit.

**Tech stack:** C++20, Qt 6, Qt Test (off-screen), `StationCapabilities`,
`StationServer`, `StationClient`, `MirrorSchema`, `MirrorPolicy`, `RadioModel`,
`StepAttenuatorController`, `AlexController`, `OcMatrix`, `CalibrationController`,
`HardwarePage` and its tabs, `RxApplet`, `GeneralOptionsPage`, `DaemonApp`,
`SettingsProxyServer`.

**Spec:** operator directive of 2026-09-23 (a remote window does everything a
local window does; nothing stays disabled without a plan). R3 plan requirements
R-R3-11, R-R3-13, R-R3-21 and new R-R3-46 (added to the R3 plan's requirement
table with this plan). Scout notes with file:line and the controller's rulings on
its open questions:
`/Users/j.j.boyd/.config/nereus/work/r3-remote-radio-hardware-scout-2026-09-23.md`.

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`),
  never `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash
  characters. Stage explicit paths only. Every commit names its R-R3 IDs.
- Protocol: a per-feature capability version `radioHardwareVersion` (1: identity
  and attenuator; 2: hardware apply), never a change to `kSessionProtocolMinor`.
  Older apps and older Cores keep today's behaviour; goldens stay byte-for-byte.
- The Core applies hardware changes through its own controllers, never by
  accepting raw keys that its teardown save would overwrite. Radio-authoritative
  values (attenuation, preamp, antenna) follow the project's policy: applied live,
  per-band intent saved by the controllers as today.
- Hermes Lite 2 behaviour comes from `../mi0bot-Thetis/` where it differs; cite
  sources with version stamps as the project's porting rules require.
- Operator wording: plain user words; every new string passes
  `OperatorWording::isPlain`; Core reasons go through `OperatorReasonText`.
- Local (direct radio) operation unchanged.
- Tests: prefix every ctest and test binary with `QT_QPA_PLATFORM=offscreen`.
  Build exact targets, then `ctest -R '^(...)$' --no-tests=error
  --output-on-failure`. No unfiltered suite. Never build the `NereusSDR` target.
  No hardware; hardware evidence stays pending for the operator checkpoint.

## What already exists

- Capabilities already carry the Core's radio model name, firmware text, MAC,
  board and connected state; the window sets its profile from
  `defaultModelForBoard(board)` (scout facts 1-2).
- The Core runs its own `StepAttenuatorController`, wired to MOX, the radio, band
  and mode (scout fact 15); the Core's AGC source and S-meter offset follow it.
- PureSignal settings are a mirrored Core-side object with an edit gate: the
  pattern for new Core-owned objects (scout fact 18).
- The Core applies DSP > Options writes live after a 50 ms coalesce: the pattern
  for a hardware apply step (scout fact 14).
- `requestSliceSampleRate` changes a receiver's rate on the Core (scout fact 6).

## Task 1: The window knows the Core's radio

**Requirements:** R-R3-46, R-R3-21, R-R3-10.

**Files:**
- Modify: `src/core/session/StationCapabilities.{h,cpp}` and
  `src/core/session/StationServer.cpp` (entries `hpsdrModel`, `radioProtocol`,
  `radioAddress`), `src/core/HardwareProfile.cpp` (an unknown board resolves to
  Unknown, never Hermes; the Core's reported model wins when it matches the board,
  so the 8000DLE and G2-1K keep their own rows), `src/models/RadioModel.{h,cpp}`
  (fill the stored radio info on a remote model and emit `currentRadioChanged`
  once per identity change, after the profile is set), `src/gui/MainWindow.cpp`
  (Radio > Protocol Info shows the Core's radio: name, protocol, firmware, MAC,
  address; audit every `currentRadioChanged` listener to be remote-safe),
  `src/gui/SetupDialog.cpp` (the PA pages follow the transmit permission with its
  reason until remote transmit)
- Test: `tests/tst_station_session.cpp`, `tests/tst_remote_window_harness.cpp`,
  `tests/tst_remote_gui_gating.cpp` (the PA case with capabilities applied)

**Interfaces:**
- Produces: the three capability entries; `radioHardwareVersion` 1 advertised by
  the Core together with Task 2's object.

**Acceptance:**
- A harness push of a Saturn ANAN-G2 1K Core updates the RX applet's preamp items
  and attenuator range, the VFO flag's antenna labels and the Hardware Config tabs
  to that radio; a Core whose radio is offline gives Unknown, not Hermes.
- Protocol Info is enabled in a remote window and shows P2 and the radio's
  address; a Core without the new entries shows what it has and stays usable.
- The PA pages are visible in a remote window and disabled with the transmit
  reason.
- Local mode unchanged (existing identity and capability tests pass).

**Verification:**
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_station_session tst_remote_window_harness tst_remote_gui_gating tst_pa_setup_per_sku_visibility -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_station_session|tst_remote_window_harness|tst_remote_gui_gating|tst_pa_setup_per_sku_visibility)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Capability entries, model resolution and the identity signal
  with tests.
- [ ] **Step 2:** Listener audit, Protocol Info, PA gating; commit.

## Task 2: The Core's attenuator and preamp as a Core-owned object

**Requirements:** R-R3-46, R-R3-11, R-R3-13.

**Files:**
- Create: `src/core/StepAttenuatorFacade.{h,cpp}` (mirrored object `stepAtt`)
- Modify: `src/core/StepAttenuatorController.{h,cpp}` (change signals for every
  setting, including auto-attenuate mode, undo and hold; a debounced save),
  `src/core/daemon/DaemonApp.cpp` (the Core's range comes from the capability row's
  `stepAttMaxDb`, so boards with Alex reach 61 dB), `src/core/session/MirrorSchema.cpp`,
  `src/core/session/MirrorPolicy.cpp`, `src/core/session/StationServer.cpp`
  (register the object; advertise `radioHardwareVersion` 1),
  `src/core/session/StationClient.cpp` (edit gate),
  `src/core/settings/SettingsScope.cpp` (the `options/stepAtt`, `options/autoAtt`
  and `options/preamp` keys are Core-owned: raw writes refused like the notch keys)
- Test: a new `tests/tst_remote_step_attenuator.cpp`, `tests/tst_daemon_app.cpp`,
  `tests/tst_station_session.cpp`, `tests/tst_mirror_inbound.cpp`,
  `tests/tst_settings_proxy.cpp`, `tests/tst_step_attenuator_controller.cpp`

**Interfaces:**
- Produces: mirrored `stepAtt` properties, settable: `enabled`, `attenuationDb`,
  `preampMode`, `rx1Preamp`, `autoAttEnabled`, `autoAttMode`, `autoAttUndo`,
  `autoAttUndoDelayMs`, `autoAttHoldMs`; Core-reported: `minDb`, `maxDb`,
  `autoAttApplied`, `overloadAdc0`, `overloadAdc1`, `adcLinked`.

**Acceptance:**
- 20 dB set from a window reaches the radio (fake connection) and is saved on the
  Core for that radio and band.
- On an ANAN-100D with Alex, 45 dB is accepted and 70 dB settles at the maximum
  with a plain reason; on a Hermes Lite 2 the range is -28..31 dB and Adaptive
  settles to Classic.
- A band change on the Core restores that band's attenuation and every window
  follows; overload on either ADC reaches the window.
- A raw write to the Core-owned keys is refused with the plain "update this app"
  reason.

**Verification:**
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_step_attenuator tst_daemon_app tst_station_session tst_mirror_inbound tst_settings_proxy tst_step_attenuator_controller tst_hl2_step_att_range -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_step_attenuator|tst_daemon_app|tst_station_session|tst_mirror_inbound|tst_settings_proxy|tst_step_attenuator_controller|tst_hl2_step_att_range)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus. Requires Task 1.

- [ ] **Step 1:** Controller signals, facade, range fix and Core registration with
  tests; commit.

## Task 3: The window's attenuator, preamp and overload controls use the Core

**Requirements:** R-R3-46, R-R3-21.

**Files:**
- Modify: `src/gui/applets/RxApplet.cpp` (ATT/S-ATT, preamp combo, RX1 preamp),
  `src/gui/setup/GeneralOptionsPage.cpp` (Step Attenuator and Auto Attenuate
  groups), `src/gui/MainWindow.cpp` (overload badge; wiring to the mirrored object
  when the Core advertises `radioHardwareVersion` 1),
  `docs/architecture/2026-09-20-remote-daemon-r3-verification/remote-controls.md`
- Test: `tests/tst_remote_gui_gating.cpp` (the attenuator rows become enabled
  with a supporting Core), `tests/tst_remote_window_harness.cpp`,
  `tests/tst_rxapplet_att_range.cpp`, `tests/tst_preamp_combo.cpp`

**Interfaces:**
- Consumes: Task 2's `stepAtt` object; Task 1's identity.

**Acceptance:**
- With a supporting Core every attenuator, preamp and auto-attenuate control is
  enabled and shows the Core's settled values; a change round-trips through the
  Core; the overload badge lights on the Core's report.
- With an older Core the rows stay disabled with a plain reason through
  `OperatorReasonText`.
- Local mode unchanged.

**Verification:**
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_gui_gating tst_remote_window_harness tst_rxapplet_att_range tst_preamp_combo tst_general_options_page_step_att_init -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_gui_gating|tst_remote_window_harness|tst_rxapplet_att_range|tst_preamp_combo|tst_general_options_page_step_att_init)$' --no-tests=error --output-on-failure
```
Hardware (pending, operator checkpoint): attenuator and preamp on the ANAN-G2 and
the HL2 from a remote window.

**Execution note (advisory):** opus. Requires Task 2.

- [ ] **Step 1:** Bind the controls, older-Core fallback, controls document;
  commit.

## Task 4: Hardware Config's receive settings through the Core

**Requirements:** R-R3-46, R-R3-21, R-R3-11.

**Files:**
- Modify: `src/gui/setup/HardwarePage.cpp` and its tabs under
  `src/gui/setup/hardware/` (the Core's MAC from Task 1; receive settings write
  through the Core; transmit fields follow the transmit permission; tabs whose
  settings nothing reads even locally behave as they do locally: saved on the Core
  for that radio), a Core-side hardware apply step in
  `src/core/session/StationServer.cpp` and `src/models/RadioModel.cpp` beside the
  DSP > Options apply (the OC receive pin matrix and the HL2 N2ADR filter board
  reload live; the P2 frequency calibration factor and 10 MHz reference apply
  live), a mirrored Core-side object for the Alex receive settings (per-band RX
  antenna, RX-only antenna, use-TX-antenna-for-RX) in `src/core/accessories/`,
  `src/core/settings/SettingsProxyServer.cpp` (refuse `hardware/<mac>/` writes for
  any MAC but the Core's connected radio), the Radio Info tab's RX1 sample rate
  (the Core's first receiver through `requestSliceSampleRate`; the Core keeps the
  per-radio default), the HL2 I/O board Probe (a Core verb), `radioHardwareVersion` 2
- Test: `tests/tst_hardware_page_capability_gating.cpp`,
  `tests/tst_hardware_page_persistence.cpp`, `tests/tst_station_session.cpp`,
  `tests/tst_remote_gui_gating.cpp`, `tests/tst_oc_matrix.cpp`,
  `tests/tst_hl2_io_board_tab_n2adr.cpp`, `tests/tst_calibration_controller.cpp`,
  `tests/tst_alex_controller.cpp`

**Interfaces:**
- Consumes: Task 1's identity; the hardware apply step reuses the DSP > Options
  apply's coalesce pattern.

**Acceptance:**
- In a remote window every Hardware Config tab shows the Core's radio's values; an
  RX antenna change, an OC receive pin change, the N2ADR switch and the P2
  frequency calibration apply on the Core live and are saved there; the Core's
  shutdown save keeps them.
- A write for any other radio's MAC is refused.
- Changing the RX1 sample rate changes the Core's first receiver without a
  reconnect.
- Transmit fields (TX antennas, relays, external PA, User Dig Out, PA
  calibration) are disabled with the transmit reason.
- Local mode unchanged.

**Verification:**
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_hardware_page_capability_gating tst_hardware_page_persistence tst_station_session tst_remote_gui_gating tst_oc_matrix tst_hl2_io_board_tab_n2adr tst_calibration_controller tst_alex_controller -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_hardware_page_capability_gating|tst_hardware_page_persistence|tst_station_session|tst_remote_gui_gating|tst_oc_matrix|tst_hl2_io_board_tab_n2adr|tst_calibration_controller|tst_alex_controller)$' --no-tests=error --output-on-failure
```
Hardware (pending, operator checkpoint): antenna and filter-board changes on the
ANAN-G2 and the HL2 from a remote window.

**Execution note (advisory):** opus. Requires Task 1. This task switches relays
on the radio live; flagged for the batch review's attention.

- [ ] **Step 1:** Core hardware apply step, MAC check, Alex receive object with
  tests.
- [ ] **Step 2:** HardwarePage through the Core, transmit gating, sample rate,
  Probe; commit.

## Task 5: Each band remembers its attenuator and preamp, local and remote

**Requirements:** R-R3-46, R-R3-11.

**Spec:** operator decision of 2026-09-23 ("each band should remember what was used there
before"): a band change restores that band's last attenuator and preamp, with a local
radio and through the Core, as Thetis does (console.cs:17325 [v2.10.3.15]). Today the
Core restores and sends them (DaemonApp::syncStepAttenuatorBandAndMode, Task 2's
`setBandRestoreToRadio(true)`), while nothing local calls
`StepAttenuatorController::setBand`, so a local band change never restores them.

**Files:**
- Modify: the local wiring that owns the `StepAttenuatorController` in a local window
  (`src/gui/MainWindow.cpp` or `src/models/RadioModel.cpp`, wherever the controller is
  created for a local radio): feed it the transmit-bound slice's band and mode on
  connect and on every band or mode change of that slice, exactly as
  `DaemonApp::syncStepAttenuatorBandAndMode` does on the Core, and turn on
  `setBandRestoreToRadio(true)` for local radios
- Test: `tests/tst_step_attenuator_controller.cpp` and a local wiring case (extend the
  test that builds a local RadioModel with a fake connection, or add one)

**Acceptance:**
- Local radio: 20 dB set on 40 m, then a change to 20 m where 0 dB was last used: the
  attenuator reads 0 dB and 0 dB reaches the radio (the fake connection); back to 40 m:
  20 dB is restored and sent. The preamp setting follows the same way.
- A band never visited keeps the current setting, as the controller does today.
- The per-band memory survives a restart for that radio (the existing per-radio saving).
- A remote window is unchanged (the Core already restores and sends).
- The test for the local restore fails before the change.

**Final review ruling (fix wave, item 2):** the band the attenuator follows is the
receive band of slice A (slice 0, Thetis `rx1_band`), locally and on the Core, not the
transmit-bound slice's; the ATT-on-TX value and the CW check on MOX follow the transmit
slice's band and mode (Thetis `_tx_band` and the TX DSP mode). The feed is
`RadioModel::followReceiveSliceWithStepAttenuator`, which the Core also calls.

**Verification:** `tst_step_attenuator_controller` and the local wiring test, built and
run by exact name. Hardware (pending, operator checkpoint): band changes on the G2 and the
HL2, locally and through the Rock.

**Execution note (advisory):** opus (small). After Task 4.

- [ ] **Step 1:** Local band and mode feed, restore to radio, tests; commit.

## Task 6: The filter policy from a remote window

Added 2026-09-24 after the unfinished-controls plan's review: the filter policy dialog
says "Remote policy editing is not available yet." in a remote window, while a local
window edits it. The operator's rule is remote parity. Neither existing path can carry
it: the policy's saved keys (`hardware/<mac>/alex/antenna/Alex{0,1}_BpfMode`) are
model-owned, so the Core's settings proxy refuses them, and the mirrored `rxFilterN*`
properties are outbound only; the `alexAntennas` object has no filter-mode property.

**Requirements:** R-R3-46, R-R3-21.

**Files:**
- Modify: `src/core/accessories/AlexAntennaFacade.{h,cpp}` (a writable filter-policy
  property, or a typed verb if a property does not fit the facade's shape: say which
  and why), the Core's write handler applying it exactly as the local dialog does (the
  same controller calls and saved keys), `src/core/session/MirrorPolicy.cpp` and
  `MirrorSchema` registration, `src/core/session/StationServer.cpp` and
  `StationCapabilities.{h,cpp}` (`radioHardwareVersion` 4), `src/core/session/StationClient.cpp`,
  `src/gui/widgets/FilterPolicyDialog.{h,cpp}` (in a remote window it reads the Core's
  policy and sends changes; with an older Core it keeps a plain reason)
- Test: the facade's test, `tests/tst_remote_hardware*` (or the file the remote
  hardware tasks used), a dialog test in a remote window, `tst_mirror_schema`

**Acceptance:**
- In a remote window the dialog shows the Core's current policy and a change reaches the
  Core, takes effect there as it does locally (the same filter selection the local
  dialog produces, checked on the Core's controller), is saved on the Core for that
  radio, and every window shows it.
- A remote window connected to a Core without `radioHardwareVersion` 4 sends nothing
  and says in plain words that the Core needs updating for this.
- Older windows see exactly today's wire.
- A local window is unchanged.

**Verification:** the tests above, built and run by exact name, offscreen. Hardware
(pending, operator checkpoint): change the policy from the Rock's remote window and
hear the filter change on the G2.

**Execution note (advisory):** opus. After Tasks 1-5.

- [ ] **Step 1:** Core property or verb, write handler, version gate, tests; commit.
- [ ] **Step 2:** The dialog in a remote window, tests; commit.
