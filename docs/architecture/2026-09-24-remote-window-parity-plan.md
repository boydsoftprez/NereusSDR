# Remote Window Parity Implementation Plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and acceptance
> cases are binding; test order and review effort follow the risk-based policy. No review
> between tasks; one whole-branch review at the end. Tasks that touch the same big file
> (`src/gui/MainWindow.cpp`, `src/core/session/StationServer.cpp`,
> `src/models/RadioModel.cpp`) run one after another; the table below says which.

**Goal:** A desktop window connected to a remote Core does everything a local window does for
every control that does not put a carrier on the air: the 68 open B rows and every bug found in
passing in the remote parity sweep of 2026-09-24. Since remote transmit exists, it also covers
what a window shows while the Core is on the air: the transmit display and keyed view in both
windows (A11, A12, display duplex, the transmit monitor, the CFC bars and PA Values' transmit
readings) and a remote window's TCI raw I/Q, Tasks 23 and 27 to 34.

**Architecture:** The Core owns the radio, its accessories and its transmit chain. A remote
window asks the Core through the station link: a command in the existing documents' style, or
a write of a mirrored property, which the Core applies through its own objects (the objects a
local window drives), saves, and reflects back to every window. Every change that keys nothing
works whenever the radio is not on the air, and is refused while it is (the operator's ruling of
2026-09-24 and his decision D60). TUNE, two-tone, MOX, VOX arming and anything else that puts
a carrier out stay with remote transmit (the iPhone plan's Part F). Commit `bfab2b9e` on
`codex/integrate-r2-main` (with its follow-ups `d553327f` and `50c7c319`) is the pattern: a
versioned capability, a command or property, the Core's own model applying it, the on-the-air
refusal, the window's control following the Core's reported state, never the click.

**Tech Stack:** C++20, Qt6, the station link (`StationServer`, `StationClient`,
`SessionCommandDispatcher`, `MirrorPolicy`, `StateMirror`), WDSP through the Core's own
channels, the link conformance runners.

**Source of the findings:** `/Users/j.j.boyd/.config/nereus/work/remote-parity-gaps-2026-09-24.md`
(the sweep), read at integration `a6291588`. Row ids (B2.1 and so on) and file:line cites below
are the sweep's, at `a6291588`. Integration has since moved to `b87c5a08` (the Tuner Genius
commits); `TunerApplet.cpp`, `RadioModel.cpp`, `StationServer.cpp`,
`SessionCommandDispatcher.cpp` and `StationClient.cpp` lines have shifted, so find each cited
spot by its symbol.

**Why now:** the operator found the Tuner Genius antenna switch greyed in a remote window and
said parity is not progressing. His standing rule: a remote window does everything a local
one does, a greyed or inert control is never "done", and gaps found become scope. His ruling
of 2026-09-24: settings that do not key the radio work from a remote window whenever nobody is
transmitting, and are refused while someone is on the air.

## Global Constraints

- **Where:** the worktree, branch and build directory the controller names at dispatch (the
  integration worktree `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`, unless told
  otherwise).
- **CLAUDE.md and CONTRIBUTING.md bind.** `AppSettings`, never `QSettings`; no raw
  `new`/`delete` (Qt parent ownership or `std::unique_ptr`); braces on all control flow;
  `constexpr`, never `#define`, for constants; `qCWarning(lcCategory)` for errors, no
  exceptions; `Q_OS_*` platform guards; `m_camelCase` members, `kPascalCase` constants;
  don't remove code you did not add; atomics, never a mutex, across to the audio thread.
- **Source first.** Anything with a Thetis equivalent (a meter reading, a WDSP call, a PA
  value, the TX inhibit input) is read from Thetis (`/Users/j.j.boyd/Thetis`, v2.10.3.15 at
  `3759d096`) before it is written, cited `// From Thetis <file>:<line> [v2.10.3.15]`, with
  every author tag kept verbatim within the lines it ports; HL2 behaviour from
  `/Users/j.j.boyd/mi0bot-Thetis`; AetherSDR behaviour from `../AetherSDR` with `[@sha]`. A new
  port into a file not in `docs/attribution/THETIS-PROVENANCE.md` carries the verbatim header
  and a PROVENANCE row in the same commit. If the source cannot be found, stop with
  NEEDS_CONTEXT: never guess a register, meter id, constant or wire line.
- **The link.**
  - Every new command, property and capability value goes in the link document
    (`docs/architecture/2026-09-23-station-link-v1.md`: section 6.3's capability notes, section
    7.1's object tables, section 9.1's command table and its sentences) in the same commit; an
    accessory change also goes in `docs/architecture/2026-09-23-remote-accessory-control-v1.md`
    (Negotiation, the object, Commands, Refusals, Core-owned settings, "What waits for remote
    transmit", Window behaviour); a display request field also in
    `docs/architecture/2026-09-20-remote-media-control-v1.md`.
  - `tests/data/link/v1/surface.json` changes only through its regen target
    (`tst_link_surface_manifest_regen`), and `tst_link_surface_manifest` passes after.
  - `python3 scripts/render-link-tables.py --check` passes.
  - `kSessionProtocolMinor` does not change. A new feature gets a `<feature>Version`
    capability, appended after the last entry of the minor-11 block (link section 6.3, "the
    last capabilities entry"), sent only to a peer at agreed minor 11, and 0 when the feature
    is off; a revision raises its version. The session fixtures that carry the capabilities
    are updated in the same commit, as `bfab2b9e` did (`NEREUS_LINK_TRACE_DIR` with
    `tst_link_conformance_session` writes a fixture from what the code does), and each new
    command has a fixture that invokes it right and wrong.
  - Command names follow the documents: the accessory document's `set<Device><Thing>` style
    (`setTgxlAntenna`); the dotted families where a plan already names them (`txProfile.select`,
    `station.selectRadio`, `support.collect`, `records.subscribe`); `request<Thing>` for a
    radio hardware request (`requestIoBoardProbe`). Property names match their setter
    (`setTunePower` gives `tunePower`), as every mirrored Q_PROPERTY does today.
  - Only the verb, capability and property names written in this plan are new. A task that
    finds it needs another stops with NEEDS_CONTEXT.
- **The on-the-air rule.** Every command or write this plan adds or unblocks, on a Core, is
  refused with "The radio is on the air. Try again when it stops." while the Core's radio is
  keyed (its `MoxController` from any source, a hardware PTT included, and its hand-back to
  receive), TUNE is on, or the two-tone test runs; nothing reaches a device or the radio then.
  In a window, the control is disabled with that reason while the Core reports the same
  (Task 1's `RadioModel::isCoreOnAir()`). A receive-only Core accepts every such change when
  the radio is not on the air. Nothing in this plan keys the radio, starts a tune or two-tone,
  or arms VOX. Exempt (ruling M4, the fix wave after Tasks 19 and 21): `records.*` and
  `spots.*` never touch the radio and are answered on and off the air; refusing
  `records.subscribe` on the air would blank a reconnecting window's spots.
- **Wording.** User-facing strings in plain operator words; each new or changed string passes
  `OperatorWording::isPlain` and the wording sweep (`tst_operator_wording_sweep`,
  `tst_station_reason_wording`); "Core" means the NereusSDR computer; no source cites inside
  strings; no em dash anywhere (code, strings, commits, documents).
- **Tests.** Every ctest and test binary runs with `QT_QPA_PLATFORM=offscreen`. Test targets
  are built by exact name (they are EXCLUDE_FROM_ALL) and run by exact name with
  `--no-tests=error`. Tests never open real audio devices. No RF, no device writes: nothing in a
  test keys a radio or reaches a real accessory. A test that needs "on the air" keys the Core's
  own `MoxController` against a test `TxChannel` with the receive-only MOX pre-check lifted
  (the pattern of `tst_remote_peripherals` and `tst_tgxl_station_identity` since `d553327f`
  and `50c7c319`). A wire-byte change also runs `tst_p1_regression_freeze` and
  `tst_p2_regression_freeze`.
- **Commits.** GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`); never
  `--no-verify` or `--no-gpg-sign`; no `Co-Authored-By` trailer; explicit pathspecs only; the
  subject in plain English with the requirement ids at its end, for example
  `Operate the Power Genius from a remote window (R-R3-49, R-R3-47)`.
- **Hardware is pending until the operator checks.** Each task's bench line is his to run
  after the controller deploys the Core and relaunches the window: his ANAN-G2 on the Rock
  Core and his Hermes Lite 2 on the Pi 4 Core (and his accessories where named). Nothing is
  "verified on hardware" until he says so.

## What already exists

- The pattern: `bfab2b9e` (`setTgxlAntenna`, `setTgxlOperate`, `setTgxlBypass`,
  `remoteTgxlControlVersion` 2): `SessionCommandDispatcher::verbSpecs()` and
  `handleTgxlControl`, `StationServer::onTransportText`'s minor and version check,
  `StationServer::tgxlControlVersion()`, `RadioModel::stationTgxlControlAllowed` and
  `set*ForStation`, `IStationLink`'s default refusals and `*UnavailableReason()`,
  `StationClient::request*` and `*Available()`, `TunerApplet::remoteTunerControl()` and
  `coreOnAir()`. `d553327f` adds the `radio` object's `transmitting` (Outbound, the Core's real
  MOX) and `RadioModel::isTransmitting()` / `transmittingChanged`; `50c7c319` holds the refusal
  through the TX to RX hand-over.
- The gates: `MainWindow::transmitControlsPermitted()` and `applyRemoteRoleGating()`
  (`MainWindow.cpp:11325-11470`); `StationServer::handlePropertyWrite`
  (`receiveOnlyTransmitWrite`, `isTunerTransmitPathProperty`, `StationServer.cpp:1735-1769`);
  `StationServer::handleSettingsWrite` and `handleSettingsRemove` with `isTransmitDspOptionsKey`,
  `isTransmitHardwareKey`, `isReceiveOnlyRefusedKey` (`StationServer.cpp:440-546, 1894-1960`);
  `RadioModel::setReceiveOnlyStationPolicy`, `receiveOnlyTxOperationsBlocked`; PureSignal's
  `setOperationalPermissionPredicate` and the dispatcher's `ps3` gate
  (`SessionCommandDispatcher.cpp:667-673`); `StationClient.cpp:1411-1413` (`canActuate`).
- The mirror: `MirrorPolicy.cpp` (`TransmitModel` 15 entries at `:291-306`, `AlexAntennaFacade`
  at `:388-397`, `IoBoardHl2Facade` at `:401-403`, `RadioModel` 20 entries);
  `TransmitModel.h` (16 Q_PROPERTYs; the rest are setters only); `MicProfileManager.h`
  (`profileNames`, `setActiveProfile`, `saveProfile`, `deleteProfile`).
- Live apply on the Core: `RadioModel::scheduleRemoteDspOptionsApply` (RX only),
  `scheduleRemoteHardwareApply` (`oc`, `cal`, `hl2`, `n2adr`),
  `applyRemoteAccessorySetting`; `RadioModel.cpp:18610-18636`.
- Telemetry: `StationTelemetry.h` (`StationRadioTelemetry`, `StationHostTelemetry`,
  `stationTelemetryVersion` 3).
- Display requests: `RemoteMediaController.cpp` builds `subscribe` with `minDbm` -180 and
  `maxDbm` 0 (`:313`); `DisplayCodec.cpp:142-148` quantises to 8 bits over that window.
- The unbuilt-feature list: `src/gui/UnbuiltFeatures.h:46-110` (R-R3-49).
- The transmit display (PR #317 in the trunk, not on `main`): `TxAnalyzer` (display 5), made by
  `MainWindow` in a window that runs its own DSP (`MainWindow.cpp:5015`) and by
  `DaemonApp::createTxAnalyzer` on a Core (started and stopped on MOX, its pixels read by
  nothing); the local MOX edge lambda (`MainWindow.cpp:5185-5620`: the pan hosting the
  transmit slice, `setMoxOverlay`, the analyzer's window, `updateSpectrumFromTxPixels`,
  `pushTxWaterfallRow`); `TxDisplayPage` and the nine Station-scoped `DisplayTx*` analyzer keys
  (`SettingsScope.cpp:469-477`); `SpectrumWidget::setDisplayDuplex` (starts true);
  `UnbuiltFeature::Fdx` (the status bar's FDX and the container DUP button); the matrix
  `docs/architecture/tx-display-verification/README.md` (rows 1 to 8 passed 2026-08-05 on an
  ANAN-7000DLE, 9 to 14 pending, 15 and 16 open, 17 and 18 to re-check). A remote window has no
  transmit display: while keyed its pan draws the receiver hearing the transmitter.
- Tests to extend: `tst_remote_peripherals`, `tst_station_accessory_state`,
  `tst_tgxl_station_identity`, `tst_remote_gui_gating`, `tst_remote_tx_widgets`,
  `tst_remote_tx_presentation`, `tst_remote_meter_poller`, `tst_remote_media_controller`,
  `tst_remote_slice_commands`, `tst_remote_diagnostics`, `tst_remote_telemetry`,
  `tst_remote_step_attenuator`, `tst_unbuilt_features`, `tst_station_session`,
  `tst_link_conformance_session`, `tst_link_conformance_control`, `tst_link_surface_manifest`,
  `tst_operator_wording_sweep`, `tst_station_reason_wording`.

## Tasks and the big files they touch

| Task | Rows | `MainWindow.cpp` | `StationServer.cpp` | `RadioModel.cpp` |
| --- | --- | :-: | :-: | :-: |
| 1 On the air, and the transmit settings gate | B5.5, B5.10, B5.1 (RF power, TX filter) | yes | yes | yes |
| 2 TX applet and Phone/CW applet settings | B5.1, B5.2, B5.6 | yes | yes | |
| 3 Microphone inputs, TX profiles, RADE and TCI profiles | B5.1 (profile), B5.3, B5.7, B5.8, B5.18 | yes | yes | yes |
| 4 TX equalizer, CFC, phase rotator, CESSB, leveler and ALC | B5.1 (editors), B5.4, B5.9, B5.11, B5.13 | yes | yes | |
| 5 Transmit Power, DEXP/VOX and two-tone settings | B5.12, B5.14, B5.16 | | yes | yes |
| 6 PA pages and PA telemetry | B5.15, B6.5 | yes | yes | yes |
| 7 PureSignal arming | B5.17 | yes | | yes |
| 8 Tuner Genius | B1.3, B1.4, B1.5, B1.10 (TGXL), B1.11 (TGXL), B1.12 (TGXL) | yes | yes | yes |
| 9 Power Genius | B1.6, B1.10 (PGXL), B1.11 (PGXL), B1.12 (PGXL) | yes | yes | yes |
| 10 RF-Kit RF2K-S | B1.7, B1.8, B1.9, B1.11 (RF-Kit), B1.12 (RF-Kit) | yes | yes | yes |
| 11 VFO flag, RX applet and container buttons | B2.1, B2.3, B2.6 | yes | | |
| 12 Transmit antennas and relays | B4.1, B2.2 | yes | yes | yes |
| 13 OC transmit pins, User Dig Out, transmit calibration | B4.2, B4.3 (and C5, C6 settled) | | yes | yes |
| 14 HL2 I/O board and link quality | B4.4, B4.5 | | yes | yes |
| 15 Meters from the Core | B2.5, B3.11 | yes | yes | yes |
| 16 DSP facts from the Core | B2.4, B3.4, B3.6, B3.8 | yes | yes | yes |
| 17 Panadapter display requests | B3.2, B3.3, B3.7, B3.9, B3.10 (and C10, C11 settled) | yes | yes | |
| 18 Pan and slice interaction | B3.1, B3.5 (and C8 settled; passing bugs) | yes | | yes |
| 19 Record streams and spot sources at the Core | B7.1, B7.2 | yes | yes | yes |
| 20 FreeDV Reporter at the Core | B7.3, B7.4 | yes | yes | yes |
| 21 The Core's radio from the window | B6.2, B6.3 | yes | yes | yes |
| 22 Support bundle and the Core's log | B6.4, B6.6 | yes | yes | |
| 23 TCI in a remote window | B8.1, B8.2, B8.3, B8.4 (the minimum) | yes | yes | |
| 24 Setup diagnostics and preferences | B6.7, B6.8, B6.9, B6.10 (passing bugs) | yes | | yes |
| 25 Dead controls in both windows | passing bugs | yes | | yes |
| 26 The R3 control matrix | the sweep's stale rows | | | |
| 27 The transmit display's skirt (row 15) | A11 | yes (if the cause is there) | | |
| 28 The Core sends the transmit display | A11 | | yes | yes |
| 29 One MOX display controller for both windows | A11, row 16 | yes | | |
| 30 Setup > Display > TX Display from a remote window | A12 | | yes | yes |
| 31 DUP (display duplex) in both windows | A11 | yes | yes | yes |
| 32 The transmit monitor to the transmit holder | Part F (MON) | yes | yes | |
| 33 The CFC bar chart and PA Values' transmit readings | Part F (CFC, PA Values) | | yes | yes |
| 34 Bench and land the transmit display and keyed view | matrix rows 9 to 28 | | | |

Order: Task 1 first (every B5 task, Task 7, Task 12 and Task 13 use its gate and helpers).
Tasks 2, 3, 4, 5, 6 and 7 in that order (each raises `transmitSettingsVersion` by one). Task 6
before Task 14 (each raises `stationTelemetryVersion`). Task 12 before Task 14 (each raises
`radioHardwareVersion`). Task 19 before Tasks 20, 21, 22 and 23 (record streams). Tasks 27 to
33 in that order: 27 fixes the analyzer both windows draw, 28 sends it, 29 draws it, and 28, 30
and 31 raise `txDisplayVersion` to 1, 2 and 3. Task 26 after Task 33; Task 34 last.
Everything else in the order written, one at a time wherever the table shares a file.

---

## Task 1: On the air, in one place, and the transmit settings gate

**Requirements:** R-R3-49 (every visible control does what its label says); R-R3-21 (remote
controls act on their station owner; unavailable ones say why); R-R3-25 (receive-only remote
changes never start tune-carrier or TX-coupled actions); the operator's ruling of 2026-09-24;
D60 and rulings 7.4 and 7.7 of the several-devices design (`e45ffef5`).

**Files:**
- Modify: `src/models/RadioModel.{h,cpp}` (the Core's refusal helper, the window's
  on-the-air state, TGXL's check moved onto the helper, the TX half of the DSP Options live
  apply)
- Modify: `src/core/session/StationServer.{h,cpp}` (the receive-only split for `transmit`
  writes and settings keys; `transmitSettingsVersion`), `src/core/session/StationCapabilities.{h,cpp}`,
  `src/core/session/StationClient.{h,cpp}`, `src/core/session/IStationLink.h`
- Modify: `src/gui/MainWindow.{h,cpp}` (`transmitSettingsPermitted()` and its push),
  `src/gui/applets/TunerApplet.cpp` (`coreOnAir()` reads the model),
  `src/gui/applets/TxApplet.{h,cpp}`, `src/gui/applets/RxApplet.{h,cpp}`,
  `src/gui/VfoWidget.{h,cpp}`, `src/gui/SetupDialog.{h,cpp}`,
  `src/gui/setup/DspOptionsPage.cpp`
- Modify: the link document (section 6.3, section 7.1's `transmit` object and its refusals),
  `surface.json` (regen), the session fixtures
- Test: `tests/tst_transmit_settings_gate.cpp` (new), `tst_remote_tx_widgets`,
  `tst_remote_gui_gating`, `tst_tgxl_station_identity`, `tst_remote_peripherals`

**Interfaces:**
- Produces (Core): `bool RadioModel::stationOnAirRefusal(QString* reason) const`, true with
  "The radio is on the air. Try again when it stops." while `isTransmitting()` (the
  `MoxController`, any source, through its hand-back), `m_transmitModel.isTune()`, `isTune()`
  or the two-tone controller is active; `stationTgxlControlAllowed` calls it. Every later task
  calls it before applying a window's change.
- Produces (window): `bool RadioModel::isCoreOnAir() const` and `void coreOnAirChanged(bool)`,
  from the mirrored `transmitting`, the mirrored `transmit.tune` and PureSignal's two-tone;
  `TunerApplet::coreOnAir()` returns it.
- Produces capability `transmitSettingsVersion` 1 (last in the minor-11 block, 0 below minor
  11): a receive-only Core accepts a `property.write` on `transmit` of any property except
  the keying set, and a `settings.write` or `settings.remove` of a `DspOptions<Setting><Mode>Tx`
  key, while the radio is not on the air, and applies it live. The keying set, still refused
  on a receive-only Core with today's `kReceiveOnlyTransmitReason`: `mox`, `tune`,
  `voxEnabled`, `twoToneActive`. `transmitSettingsVersion` 1 covers the properties mirrored
  today: `power`, `micGain`, `filterLow`, `filterHigh`, `lineInGain`, `userDigOut`, `pureSig`,
  `forceAttwhenPSAoff`, `forceAttwhenPowerChangesWhenPSAon`,
  `forceAttwhenPowerChangesWhenPSAonAndDecreased`, `antiVoxTauMs`, `antiVoxRun`,
  `paSettingsBypass`. Raw settings writes of `hardware/<mac>/tx/...`, `powerByBand` and
  `tunePowerByBand` stay refused (the `transmit` object owns them).
- Produces (StationServer): `bool isTransmitSettingKeyAcceptedOffAir(const QString& key)`, the
  one list of transmit settings keys a receive-only Core takes off the air; this task puts
  `DspOptions*Tx` on it, and Tasks 6 and 13 add their keys to it.
- Produces (client): `bool StationClient::transmitSettingsAvailable(int minVersion = 1) const`
  and `static QString IStationLink::transmitSettingsUnavailableReason()` = "This Core does not
  let this app change transmit settings. Updating the Core may help."
- Produces (window): `bool MainWindow::transmitSettingsPermitted(int minVersion = 1) const`:
  local, or handshake complete, `transmitSettingsAvailable(minVersion)` and not
  `isCoreOnAir()`; its reason is the on-air reason while on the air, else the Core reason
  above. Each applet and page that holds a transmit setting gains
  `setTransmitSettingsPermitted(bool permitted, const QString& reason)` beside
  `setTransmitPermitted`, which keeps the keying controls (MOX, TUNE, 2-Tone, VOX arming, PS
  two-tone) only. `applyRemoteRoleGating()` pushes both, and runs again on
  `coreOnAirChanged`.

**Acceptance:**
- **Gate:** with the Core's radio on the air (keyed through its `MoxController`, TUNE, and
  two-tone, each in turn), a `transmit` write of `power` from a window is refused with the
  on-air reason and nothing changes; off the air on a receive-only Core it is applied and the
  delta returns to the window. `mox`, `tune`, `voxEnabled` and `twoToneActive` writes stay
  refused with the receive-only reason, on and off the air.
- **TGXL unchanged:** `setTgxlAntenna`, `setTgxlOperate`, `setTgxlBypass` give the same
  answers as before (the existing TGXL tests pass unchanged).
- **Window state:** `isCoreOnAir()` follows `transmitting`, `transmit.tune` and the two-tone
  flag, and a window's transmit settings grey with the on-air reason within one delta.
- **Older Core:** on a Core without `transmitSettingsVersion`, every transmit setting stays
  greyed with "This Core does not let this app change transmit settings. Updating the Core
  may help."
- **B5.1 (RF Power, TX filter low and high):** in a remote window the TX applet's RF Power
  and TX filter controls are enabled off the air, change the Core's `transmit` values, and
  show what the Core reports back.
- **B5.5:** Shift-click on a filter preset on the VFO flag and in the RX applet also sets the
  Core's TX passband (`filterLow`, `filterHigh`) as a local window does; no toast, no silent
  skip; on the air, refused with the on-air reason.
- **B5.10:** Setup > DSP > Options' nine TX buffer, filter size and filter type combos are
  enabled in a remote window, the Core saves the `DspOptions*Tx` key and applies it to its TX
  channel at once (the local page's own apply path, read from `DspOptionsPage` and
  `RadioModel`), and the combo shows the Core's value; on the air the write is refused and the
  combo settles on the Core's value.
- A local window behaves exactly as today.

**Verification:** the transmit boundary: tests first (the gate cases and the keying set, red
before the split). Build `NereusSDR`, `nereusd` and the named tests; run them by exact name,
then `tst_link_conformance_session`, `tst_link_conformance_control`,
`tst_link_surface_manifest`, the wording sweep and `render-link-tables.py --check`. Bench
(pending): on the G2 at the Rock and the HL2 at the Pi 4, the TX applet's RF Power and a DSP
Options TX combo change from a remote window and read back after a window restart.

**Execution note (advisory):** opus. Touches all three big files; runs first, alone.

- [ ] **Step 1:** Gate tests (Core side, keying set, on-air refusals), then `stationOnAirRefusal`,
  the receive-only split, the capability and the documents.
- [ ] **Step 2:** `isCoreOnAir`, `transmitSettingsPermitted`, the applet and page setters,
  B5.1's part, B5.5, B5.10, fixtures.

## Task 2: TX applet and Phone/CW applet settings

**Requirements:** R-R3-49; R-R3-21; the operator's ruling of 2026-09-24; ruling 7.7 (with
transmit unheld any device may change the transmitter's settings); the iPhone plan's Task 40
names (its controls are built here; see "Plan text to change").

**Files:**
- Modify: `src/models/TransmitModel.{h,cpp}` (Q_PROPERTYs for the setters below, each
  NOTIFY on its existing signal), `src/core/session/MirrorPolicy.cpp` (each Bidirectional),
  `src/core/session/StationServer.cpp` (`transmitSettingsVersion` 2, range refusals)
- Modify: `src/gui/applets/TxApplet.{h,cpp}` (`:1306, 1397, 2166-2225`),
  `src/gui/applets/PhoneCwApplet.{h,cpp}` (`:1316-1352`),
  `src/gui/containers/ContainerButtonDispatcher.cpp` (`:132-135`), `src/gui/MainWindow.cpp`
- Modify: the link document, `surface.json` (regen), fixtures
- Test: `tests/tst_transmit_model_properties.cpp` (new, the name Task 40 gave it),
  `tst_remote_tx_widgets`

**Interfaces:**
- Produces, on `transmit` (class `TransmitModel`), Bidirectional, under
  `transmitSettingsVersion` 2: `tunePower` (i64, W), `voxThresholdDb` (f64),
  `voxHangTimeMs` (i64), `monEnabled` (bool), `monitorVolume` (i64), `txLevelerOn` (bool),
  `txEqEnabled` (bool), `cfcEnabled` (bool), `cpdrOn` (bool), `cpdrLevelDb` (f64),
  `amCarrierLevel` (f64), `dexpEnabled` (bool), `micGainDb` (f64). Each keeps the setter's
  own range; a write outside it is refused with the range in plain words (for example
  "Choose a tune power from 0 to 100 W."), read from the setter or the local control.
- The MON output choice (speakers or phones) is this window's own audio routing: a Window-scope
  setting read by this window's audio engine, as in a local window.

**Acceptance:**
- **B5.1 (Tune Power, VOX level and delay, MON, monitor level, MON SPEAKERS/PHONES, LEV, EQ,
  CFC):** each is enabled in a remote window off the air, changes the Core's value, and shows
  the Core's value (a second window sees the change). VOX (the arming button) stays greyed
  with the remote transmit reason (A4). The MON output pair routes this window's monitor
  audio as a local window does.
- **B5.2 (mic level, PROC and its level, AM carrier, DEXP):** the same, on the Phone/CW
  applet. The mic profile is Task 3.
- **B5.6:** the container MON button toggles the Core's `monEnabled` and shows its state.
- Each property round-trips through a remote write and reaches the Core's TX chain (checked
  on the TX channel's own state, not the model), and a write out of range is refused with its
  range.
- Connecting a window never writes its own defaults to the Core (the snapshot hydrates first).
- On the air, each control greys with the on-air reason, and a write that arrives anyway is
  refused.

**Verification:** transmit chain settings: tests first for the refusals and one round trip
per property. Named tests, the conformance runners, `tst_link_surface_manifest`, the wording
sweep. Bench (pending): on the G2 at the Rock and the HL2 at the Pi 4, change LEV, EQ, PROC
and MON from a remote window; a local window opened on the same Core later shows them.

**Execution note (advisory):** opus. After Task 1. Shares `MainWindow.cpp` and
`StationServer.cpp`.

- [ ] **Step 1:** The properties, mirror entries, range refusals and their tests.
- [ ] **Step 2:** The two applets and the container button in a remote window, fixtures,
  documents.

## Task 3: Microphone inputs, TX profiles, RADE and TCI profile commands

**Requirements:** R-R3-49; R-R3-21; R-R3-42 (TCI commands act on the Core); ruling 7.7
(`txProfile.select`); the iPhone plan's Task 40 (`txProfile.select`, `activeTxProfile`,
`txProfilesJson`).

**Files:**
- Modify: `src/models/TransmitModel.{h,cpp}`, `src/core/MicProfileManager.{h,cpp}` (the
  Core's list and active profile published on `transmit`; a remote window's manager fed from
  them and never saving),
  `src/core/session/MirrorPolicy.cpp`, `src/core/session/SessionCommandDispatcher.{h,cpp}`,
  `src/core/session/StationServer.cpp`, `src/core/session/StationClient.{h,cpp}`,
  `src/core/session/IStationLink.h`, `src/models/RadioModel.{h,cpp}` (`RadioModel.cpp:9123-9139`:
  the remote model's profile manager)
- Modify: `src/gui/setup/AudioTxInputPage.cpp` (`:371-398`), `src/gui/SetupDialog.cpp`
  (`:1203-1208`), `src/gui/applets/RadeApplet.{h,cpp}` (`:309-347, 414-430`),
  `src/gui/applets/TxApplet.cpp`, `src/gui/applets/PhoneCwApplet.cpp`,
  `src/core/TciProtocol.cpp` (`:876-877, 3127-3141`), `src/gui/MainWindow.cpp`
- Modify: the link document, `surface.json` (regen), fixtures
- Test: `tests/tst_tx_profile_select.cpp` (new, Task 40's name), `tests/tst_remote_tx_profiles.cpp`
  (new), `tst_tci_remote_window` (or the existing remote TCI test)

**Interfaces:**
- Produces on `transmit`, under `transmitSettingsVersion` 3: Bidirectional `micBoost`,
  `micXlr`, `micTipRing`, `micBias`, `micPttDisabled`, `lineIn`, `lineInBoost` (bool);
  Outbound `activeTxProfile` (utf8) and `txProfilesJson` (utf8, a JSON array of names in the
  Core's order).
- Produces verbs under `transmitSettingsVersion` 3: `txProfile.select {name}` (applies it
  exactly as the local profile combo does), `txProfile.save {name}` (saves the Core's current
  transmit settings under that name, overwriting only with the same name, as the local Save
  does), `txProfile.delete {name}`, and `rade.resetVocoder {}` (the Core's `RadeChannel::resetTx`
  on the RADE channel, as the local Reset vocoder does). Each is refused on the air; a missing
  name with "There is no transmit profile called <name>."; a factory profile's delete with the
  local page's own words.
- Produces (client): `StationClient::requestTxProfileSelect(QString)`,
  `requestTxProfileSave(QString)`, `requestTxProfileDelete(QString)`,
  `requestRadeResetVocoder()`, gated as `bfab2b9e`'s requests are.

**Acceptance:**
- **B5.1 (profile), B5.2 (mic profile):** the TX applet's and Phone/CW applet's profile
  combos list the Core's profiles, select through `txProfile.select`, and show
  `activeTxProfile`.
- **B5.3:** the RADE applet's profile combo selects the Core's profile; Reset vocoder resets
  the Core's RADE transmit vocoder and keys nothing (a test checks `resetTx` ran and MOX did
  not).
- **B5.7:** Setup > Audio > TX Input's Mic Gain (Task 2's `micGainDb`) and the radio
  microphone groups (Mic In or Line In, Line In gain, +20 dB, tip/ring, bias, PTT disabled,
  3.5 mm or XLR) show the Core's values and change them off the air. Mic Source stays as C2
  decides.
- **B5.8:** Setup > Audio > TX Profile shows the Core's active profile and list; Save...,
  Delete, TX filter and AM carrier change the Core's; File > Profiles > TX Profiles and Mic
  Profiles open it enabled.
- **B5.18:** a TCI `tx_profile_ex` from an app in a remote window selects the Core's profile
  (and is echoed only after the Core accepts it); `tx_profiles_ex` answers the Core's list;
  `mon_volume` changes the Core's `monitorVolume`.
- A refused select leaves every combo on the Core's value.

**Verification:** transmit settings: tests first for select, save, delete and their
refusals. Named tests, the conformance runners, surface and wording. Bench (pending): on the
G2 at the Rock and the HL2 at the Pi 4, select a profile from a remote window and from a TCI
app (for example JTDX) through it; the Core's local log shows the profile applied.

**Execution note (advisory):** opus. After Task 2. Shares all three big files.

- [ ] **Step 1:** Profile verbs, the published list and their tests.
- [ ] **Step 2:** Microphone properties, the pages and applets, RADE reset, the TCI commands,
  fixtures, documents.

## Task 4: TX equalizer, CFC, phase rotator, CESSB, leveler and ALC

**Requirements:** R-R3-49; R-R3-21; ruling 7.7.

**Files:**
- Modify: `src/models/TransmitModel.{h,cpp}`, `src/core/session/MirrorPolicy.cpp`,
  `src/core/session/StationServer.cpp`
- Modify: `src/gui/MainWindow.cpp` (`:8083-8093, 11377-11381`, Tools > TX Equalizer),
  `src/gui/applets/TxApplet.cpp` (`:2110-2112`), the TX EQ dialog, `TxCfcDialog`,
  `src/gui/setup/DspSetupPages.cpp` (`:455-471`), the Setup > DSP > CFC page and Setup >
  Transmit > Speech Processor (`SetupDialog.cpp:1235-1240, 1323-1330`)
- Modify: the link document, `surface.json` (regen), fixtures
- Test: `tst_transmit_model_properties` (extended), `tests/tst_remote_tx_eq_cfc.cpp` (new)

**Interfaces:**
- Produces on `transmit`, Bidirectional, under `transmitSettingsVersion` 4: `txEqPreamp`
  (f64), `txEqBandsJson` (utf8, the ten band gains in dB as a JSON array, the form Task 40
  gave `eqBandGainsDb`), `txEqFreqsJson` (utf8, the band centres in Hz), `txEqNc`,
  `txEqMp`, `txEqCtfmode`, `txEqWintype` (i64), `txEqParaEqData` (utf8), `cfcCompressionJson`,
  `cfcEqFreqJson`, `cfcPostEqBandGainJson` (utf8, ten values each), `cfcPostEqEnabled`
  (bool), `cfcPostEqGainDb`, `cfcPrecompDb` (f64), `cfcParaEqData` (utf8),
  `phaseRotatorEnabled` (bool), `phaseRotatorFreqHz` (f64), `phaseRotatorStages` (i64),
  `phaseReverseEnabled` (bool), `cessbOn` (bool), `txLevelerMaxGain`, `txLevelerDecay`,
  `txAlcMaxGain`, `txAlcDecay` (the setters' own types). A JSON array of the wrong length or
  with a value out of range is refused whole, with the range.
- The CFC dialog's profile combo with Save, Save As, Delete and Reset uses Task 3's
  `txProfile.*` verbs, as the local dialog uses `MicProfileManager`.

**Acceptance:**
- **B5.1 (the EQ and CFC right-click editors), B5.4:** Tools > TX Equalizer and the CFC band
  editor (TX applet right-click, Setup > DSP > CFC > Configure CFC bands) open in a remote
  window, show the Core's values, and change them (preamp, every band, the centres,
  Nc/Mp/Ctfmode/Wintype, the CFC profile and all 30 values).
- **B5.9:** Setup > DSP > AGC/ALC's TX Leveler and TX ALC groups change the Core's values.
- **B5.11:** Setup > DSP > CFC (phase rotator, CFC, CESSB, Configure CFC bands) is enabled
  and works.
- **B5.13:** Setup > Transmit > Speech Processor is enabled and each control changes the
  Core's value (its CPDR and leveler rows use Task 2's properties).
- Each round trip reaches the Core's TX channel; a wrong-length array is refused whole.

**Verification:** tests first for the array refusals and a round trip per group. Named
tests, conformance, surface, wording. Bench (pending): on the G2 at the Rock, change an EQ
band and a CFC band from a remote window, and see them on a local window of the same Core.

**Execution note (advisory):** opus. After Task 3 (the CFC profile uses its verbs). Shares
`MainWindow.cpp` and `StationServer.cpp`.

- [ ] **Step 1:** The properties with array checks and tests.
- [ ] **Step 2:** The dialogs and pages, fixtures, documents.

## Task 5: Transmit Power, DEXP/VOX and two-tone settings

**Requirements:** R-R3-49; R-R3-21; ruling 7.7 (two-tone settings are settings;
`ps3.twoTone` and `tx.twoTone` stay with remote transmit).

**Files:**
- Modify: `src/models/TransmitModel.{h,cpp}`, `src/core/session/MirrorPolicy.cpp`,
  `src/core/session/StationServer.cpp` (the Power page's Station-scoped keys added to
  `isTransmitSettingKeyAcceptedOffAir` where they are transmit keys), `src/models/RadioModel.cpp`
  (those keys applied live on the Core through the setters the local page calls)
- Modify: `src/gui/setup/TransmitSetupPages.cpp` (`:199-810, 1380-1956`),
  `src/gui/setup/TestTwoTonePage.cpp` (`:55-160`), `src/gui/SetupDialog.cpp` (`:1313, 1343,
  1468-1469`)
- Modify: the link document, `surface.json` (regen), fixtures
- Test: `tst_transmit_model_properties` (extended), `tests/tst_remote_transmit_setup_pages.cpp`
  (new)

**Interfaces:**
- Produces on `transmit`, Bidirectional, under `transmitSettingsVersion` 5: `tuneDrivePowerSource`
  (enum as its type), `swrProtectFactor` (f64), `powerByBandJson`, `tunePowerByBandJson` (utf8,
  per-band W keyed by the app's band key), `dexpAttackTimeMs`, `dexpDetectorTauMs`,
  `dexpExpansionRatioDb`, `dexpHighCutHz`, `dexpHysteresisRatioDb`, `dexpLookAheadEnabled`,
  `dexpLookAheadMs`, `dexpLowCutHz`, `dexpReleaseTimeMs`, `dexpSideChannelFilterEnabled`,
  `antiVoxGainDb`, `twoToneFreq1`, `twoToneFreq2`, `twoToneLevel`, `twoTonePower`,
  `twoTonePulsed`, `twoToneInvert`, `twoToneFreq2Delay`, `twoToneDrivePowerSource` (each the
  setter's type).
- The Power page's other settings (Max Power, ATT on TX, force ATT, SWR protection on,
  External TX inhibit, Disable HF PA): the implementer lists each control's key and reader in
  the report; a key the Core's `TransmitModel` or `RadioModel` reads is applied live on the
  Core when a window writes it (S), through the reader the local page's change calls; a
  `hardware/<mac>/tx/...` key is changed through its `transmit` property instead (added here
  under version 5 with the setter's name).

**Acceptance:**
- **B5.12:** Setup > Transmit > Power is enabled in a remote window; every control shows the
  Core's value and changes it; the SWR protection and TX inhibit settings take effect on the
  Core at once (a test reads the Core's live object, not the settings file).
- **B5.14:** Setup > Transmit > DEXP/VOX (DEXP enable and timing, VOX threshold and hang,
  look-ahead, side-channel filter, anti-VOX) is enabled and works; Enable VOX stays greyed
  with the remote transmit reason (A4).
- **B5.16:** Setup > Test > Two-Tone IMD's tone frequencies, level, power, pulsed, invert and
  delay change the Core's values; the two-tone start stays with remote transmit (A3).
- Nothing on these pages keys, tunes or arms VOX (a test asserts MOX, TUNE, two-tone and VOX
  stay off through every write).

**Verification:** transmit boundary: tests first (the no-key assertion and the live-apply
checks). Named tests, conformance, surface, wording. Bench (pending): on the G2 at the Rock and
the HL2 at the Pi 4, change the SWR protection and a DEXP time from a remote window, confirm on
a local window of the same Core.

**Execution note (advisory):** opus. After Task 4. Shares `StationServer.cpp` and
`RadioModel.cpp`.

- [ ] **Step 1:** Tests, properties and the Power page keys' live apply.
- [ ] **Step 2:** The three pages in a remote window, fixtures, documents.

## Task 6: PA pages and PA telemetry

**Requirements:** R-R3-49; R-R3-21; R-R3-32 (the banner's live telemetry names its source);
R-R3-46 (transmit-side hardware; its "follows the transmit permission until remote transmit"
is narrowed by the operator's ruling of 2026-09-24 to "not while on the air").

**Source first:** the PA readings (volts, current, temperature, supply volts) and their
scaling as the local `RadioStatus` and `connection()` compute them, which already cite Thetis
and mi0bot-Thetis; the Core reads the same, never a new formula.

**Files:**
- Modify: `src/core/session/StationTelemetry.{h,cpp}` (radio PA fields),
  `src/core/session/StationServer.cpp` (`stationTelemetryVersion` 4; the `pa/` keys and
  `paCalibration` keys on the off-air list), `src/models/RadioModel.{h,cpp}` (the Core fills
  them from its own `RadioStatus` and connection; the Core applies `hardware/<mac>/pa/...` live
  through `PaProfileManager`)
- Modify: `src/gui/MainWindow.cpp` (`:8938-9030, 11224-11232`: the System tile PA row reads the
  Core's values in a remote window), `src/gui/setup/RadioStatusPage.cpp` (`:178-198`),
  `src/gui/RemoteDiagnosticsDialog.cpp` (`:143-217, 332-341`), `src/gui/SetupDialog.cpp`
  (`:1107-1147`), the PA pages (`src/gui/setup/PaSetupPages.cpp`), the container HW Volts,
  Amps and Temperature meter bindings (`src/gui/meters/ItemGroup.cpp`, `MeterPoller.cpp`)
- Modify: the link document (section 10), `surface.json` (regen), fixtures
- Test: `tst_remote_telemetry` (extended), `tests/tst_remote_pa_pages.cpp` (new),
  `tst_remote_diagnostics`

**Interfaces:**
- Produces `StationRadioTelemetry` fields (each `std::optional`, absent when the board has
  none): `paVolts`, `supplyVolts`, `paCurrentAmps`, `paTemperatureCelsius`,
  `packetLossPercent`, `jitterMs`, `sampleRateHz`, `udpPacketsSeen`; `stationTelemetryVersion`
  4.
- Produces `transmitSettingsVersion` 6: the PA pages' keys (`hardware/<mac>/pa/...`,
  `hardware/<mac>/paCalibration/...`) accepted off the air and applied live.
- The auto-calibrate sweep (A8) stays greyed with the remote transmit reason.

**Acceptance:**
- **B5.15:** Setup > PA > PA Gain (profiles, per-band gains, adjust matrix, max power), Watt
  Meter (calibration, Reset PA values) and PA Values are enabled in a remote window, show the
  Core's values, and change them on the Core at once; PA Values shows the Core's readings.
- **B6.5:** a remote window's System tile shows the PA row from the Core (G2 PA volts, G2E
  supply volts, HL2 PA temperature, as the local window shows for that board); Radio Status's
  PA Temp and PA Current show the Core's; Remote Diagnostics adds PA voltage, packet loss,
  jitter, sample rate and UDP packets seen beside the existing radio throughput and RTT; each
  says "from the Core" (R-R3-32); a stale or absent reading shows as unavailable, never 0.
- **Passing (PA Voltage never set):** Radio Status's PA Voltage is set, locally from the
  radio and remotely from the Core.
- **Passing (HW Volts, Amps and Temperature meters):** the container meters bound to them
  move, locally and remotely, from the same readings.

**Verification:** tests first for the telemetry fields (present, absent, stale). Named tests,
conformance, surface, wording. Bench (pending): the G2 at the Rock shows its PA volts in a
remote window's System tile; the HL2 at the Pi 4 shows its PA temperature.

**Execution note (advisory):** opus. After Task 5. Shares all three big files.

- [ ] **Step 1:** Telemetry fields and tests; the PA keys' live apply.
- [ ] **Step 2:** The banner, Radio Status, Remote Diagnostics, the PA pages and meters,
  fixtures, documents.

## Task 7: PureSignal arming from a remote window

**Requirements:** R-R3-49; R-R3-21; D53 (PureSignal is a shared setting), D60 and ruling 7.8
(it waits while on the air).

**Files:**
- Modify: `src/core/session/SessionCommandDispatcher.cpp` (`:667-673`),
  `src/models/RadioModel.cpp` (PureSignal's operational permission predicate: a window's
  arming request is permitted off the air on a receive-only Core; the correction itself still
  runs only during a transmission), `src/core/session/StationClient.cpp` (`:1411-1413`:
  `canActuate` from the new version and the Core's on-air state), `src/core/session/StationServer.cpp`
  (`transmitSettingsVersion` 7)
- Modify: `src/gui/PsForm.cpp` (`:928-957`), `src/gui/applets/PureSignalApplet.cpp`
  (`:383-408`), `src/gui/applets/TxApplet.cpp` (`:2253-2266`, PS-A),
  `src/gui/containers/ContainerButtonDispatcher.cpp` (`:155-163`), `src/gui/MainWindow.cpp`
- Modify: the link document (section 9.1's PureSignal sentence), `surface.json`, fixtures
- Test: `tests/tst_remote_puresignal_arming.cpp` (new), the existing ps3 dispatcher tests

**Interfaces:**
- Consumes the existing verbs `ps3.single`, `ps3.automatic`, `ps3.applyCurrent`,
  `ps3.restoreCorrection` and the PS-A switch (`transmit.pureSig` and the PureSignal settings
  object). Under `transmitSettingsVersion` 7 each is accepted off the air on a receive-only
  Core; `ps3.twoTone` stays refused with the remote transmit reason (A3).

**Acceptance:**
- **B5.17:** PsForm and the PureSignal applet's Single Cal, Automatic, Apply current
  correction and Restore a saved correction, and PS-A on the TX applet and the container, are
  enabled in a remote window off the air and arm or apply on the Core; the PureSignal state the
  Core reports follows. None keys the radio (a test asserts MOX stays off).
- On the air, each is refused with the on-air reason (D60) and greys in the window.
- On a Core below version 7, they keep today's reason.

**Verification:** transmit boundary: tests first. Named tests, conformance, wording. Bench
(pending): on the G2 at the Rock, arm Automatic from a remote window; the Core's PureSignal
state shows armed; nothing transmits.

**Execution note (advisory):** opus. After Task 6. Shares `MainWindow.cpp` and
`RadioModel.cpp`.

- [ ] **Step 1:** Tests, the dispatcher gate, the predicate, the version.
- [ ] **Step 2:** The window's controls, fixtures, documents.

## Task 8: Tuner Genius: relays, tune memory recall, Advanced, LAN scan, address and diagnostics

**Requirements:** R-R3-49; R-R3-47 and R-R3-22 (accessories through documented commands);
D53, D60, ruling 7.8.

**Files:**
- Modify: `src/core/session/SessionCommandDispatcher.{h,cpp}`, `src/core/session/StationServer.{h,cpp}`
  (`remoteTgxlControlVersion` 3), `src/models/RadioModel.{h,cpp}` (`moveTgxlRelayForStation`,
  `setTgxlAddressForStation`, the LAN scan through the Core's `LanDiscovery`),
  `src/core/session/StationClient.{h,cpp}`, `src/core/session/IStationLink.h`
- Modify: `src/gui/applets/TunerApplet.{h,cpp}` (`:152-170, 453-457, 769-816`),
  `src/gui/setup/CatNetworkSetupPages.cpp` (`:1294-1299, 1391-1392, 1461-1462, 1514-1518`),
  `src/gui/MainWindow.cpp` (`:6842-6861, 9668-9693, 12433-12443`)
- Modify: the accessory control document, the link document, `surface.json` (regen),
  fixtures (`verbs-tgxl-control.json` extended)
- Test: `tst_tgxl_station_identity`, `tst_remote_peripherals`, `tst_station_accessory_state`

**Interfaces:**
- Produces under `remoteTgxlControlVersion` 3:
  - `moveTgxlRelay {relay, direction}`: `relay` i64 0 (C1), 1 (L) or 2 (C2), `direction` i64
    -1 or 1; applied through the Core's `TunerModel::adjustRelay`, which sends the local
    applet's line `tune relay=<relay> move=<move>` (`TgxlConnection::adjustRelay`,
    `TgxlConnection.cpp:954-959`). It moves one matching relay and keys nothing.
  - `scanTgxlLan {}`: the Core listens for Tuner Genius announcements for the local Scan LAN
    dialog's own window and answers with `values` `devicesJson` (utf8, a JSON array of
    `{"address","port","model","serial","nickname"}`).
  - `setTgxlAddress {host, port}`: saves `TGXL_ManualIp` and `TGXL_ManualPort` for the Core's
    radio without dialling; the address checks and reasons are `configureTgxl`'s.
- Refusals: the on-air reason for `moveTgxlRelay`; "The Core is not connected to the Tuner
  Genius." for it with no tuner admitted; "The request to move a Tuner Genius relay was not
  understood." for bad arguments; `bfab2b9e`'s minor and Core-ownership reasons.
- Produces (client): `requestTgxlRelayMove(int relay, int direction)`, `requestTgxlLanScan()`,
  `requestTgxlAddress(QString host, int port)`.
- The accessory document's sentence "TUNE (the autotune) and the relay nudges still wait for
  remote transmit, because they put a carrier on the air" becomes: TUNE and the tune-memory
  recall wait (a tune carrier); a relay move keys nothing and waits only while on the air.

**Acceptance:**
- **B1.3:** a mouse-wheel nudge on C1, L or C2 in a remote window moves the Core's tuner
  relay; the bar follows the tuner's `relayC1`/`relayL`/`relayC2`, not the wheel; on the air,
  scrolling is off with the on-air reason and a command is refused.
- **B1.4:** right-click > Recall tune memory is enabled in a remote window (it copies the
  stored values into the bars and sends nothing, as locally).
- **B1.5:** right-click > Open TGXL Advanced... opens Setup at CAT & Network > 4O3A > Tuner
  Genius XL, in local and remote windows.
- **Passing (navigation):** Open PGXL Advanced (Power Genius applet), Open TGXL Advanced and
  the PGXL Interlock navigation open their CAT & Network > 4O3A tabs (`SetupDialog.cpp:1345-1348,
  1413-1423`), not Setup's first page; a test drives each.
- **B1.10 (Tuner Genius row):** Setup > 4O3A > Peripherals > Scan LAN for the Tuner Genius
  lists what the Core hears and fills Host and Port from a pick.
- **B1.11 (Tuner Genius row):** a Host or Port typed without pressing Connect reaches the Core
  through `setTgxlAddress` when editing finishes, and when Setup closes with an unsent edit;
  it is kept across a window restart; nothing is dialled.
- **B1.12 (Tuner Genius):** Copy diagnostics to clipboard in a remote window copies the
  Core's connection data (the mirrored `tuner` object and `accessoryData`'s `tgxl*`
  counters), not this computer's idle socket.

**Verification:** accessory commands: tests first for the refusals (a fake tuner connection
records the lines; none sent on a refusal). Named tests, the three conformance runners, surface,
wording. Bench (pending): the operator nudges C1 on his TGXL from a remote window and scans the
LAN; the local applet on the Core's computer shows the same relay.

**Execution note (advisory):** opus. Shares all three big files.

- [ ] **Step 1:** Refusal tests, `moveTgxlRelay`, `scanTgxlLan`, `setTgxlAddress`, the version,
  the documents.
- [ ] **Step 2:** The applet, the Peripherals row, the navigation fix, diagnostics copy,
  fixtures.

## Task 9: Power Genius: OPERATE and STANDBY, LAN scan, address and diagnostics

**Requirements:** R-R3-49; R-R3-47, R-R3-22; D53 (the amp), D60, ruling 7.8.

**Files:**
- Modify: `src/core/session/SessionCommandDispatcher.{h,cpp}`, `src/core/session/StationServer.{h,cpp}`
  (`remotePgxlControlVersion` 4), `src/models/RadioModel.{h,cpp}` (`setPgxlOperateForStation`
  sending the local line through the Core's `PgxlConnection`, `setPgxlAddressForStation`, the
  scan), `src/core/session/StationClient.{h,cpp}`, `src/core/session/IStationLink.h`
- Modify: `src/gui/applets/AmpApplet.{h,cpp}` (`:167-173, 227-233`),
  `src/gui/setup/FourO3APage.{h,cpp}` (`:453-459`: the Operate button gets its handler),
  `src/gui/setup/CatNetworkSetupPages.cpp` (`:1326-1331` and the Power Genius Scan LAN),
  `src/gui/MainWindow.cpp` (`:12352-12370`, `:12486-12497`)
- Modify: the accessory control document, the link document, `surface.json` (regen), fixtures
  (`verbs-pgxl-control.json`, new)
- Test: `tests/tst_pgxl_station_control.cpp` (new), `tst_remote_peripherals`

**Interfaces:**
- Produces under `remotePgxlControlVersion` 4:
  - `setPgxlOperate {on}`: sends the local applet's line, `operate=1` or `operate=0`
    (`MainWindow.cpp:12361-12370`, the bench-fix comment there), through the Core's
    `PgxlConnection`. `accepted` means the line left; the amp's report returns on
    `amplifier` (`state`, `operate`).
  - `scanPgxlLan {}`, `setPgxlAddress {host, port}`: as Task 8's for the Tuner Genius, with
    `configurePgxl`'s address checks.
- Refusals: the on-air reason; "The Core is not connected to the Power Genius."; "The request
  to put the Power Genius in operate or standby was not understood."; the minor and ownership
  reasons. A raw `amplifier` `operate` write stays refused (a current app uses the command).
- Produces (client): `requestPgxlOperate(bool)`, `requestPgxlLanScan()`,
  `requestPgxlAddress(QString, int)`.

**Acceptance:**
- **B1.6:** the Power Genius applet's OPERATE and Setup > CAT & Network > 4O3A > PowerGenius
  XL > Operate put the Core's amp in operate or standby off the air; each shows the amp's
  reported state; on the air, disabled with the on-air reason. The 4O3A tab's Operate button
  works in a local window too (it had no handler).
- **B1.10 (Power Genius row), B1.11 (Power Genius row):** as Task 8's rows, for the PGXL.
- **B1.12 (Power Genius):** Copy diagnostics copies the Core's `amplifier` and `accessoryData`
  `pgxl*` data.
- Nothing is sent to the amp on a refusal (a fake connection records every line).

**Verification:** tests first (refusals, the exact line). Named tests, conformance runners,
surface, wording. Bench (pending): the operator operates and stands by his PGXL from a remote
window with the radio idle.

**Execution note (advisory):** opus. After Task 8 (same files). Shares all three big files.

- [ ] **Step 1:** Tests, `setPgxlOperate`, scan, address, version, documents.
- [ ] **Step 2:** Applet, 4O3A tab handler, Peripherals row, diagnostics copy, fixtures.

## Task 10: RF-Kit RF2K-S: OPERATE, antennas, TCI mode, address and diagnostics

**Requirements:** R-R3-49; R-R3-47; R-R3-48; D53, D60, ruling 7.8.

**Files:**
- Modify: `src/core/session/SessionCommandDispatcher.{h,cpp}`, `src/core/session/StationServer.{h,cpp}`
  (`remoteRfKitControlVersion` 4), `src/models/RadioModel.{h,cpp}`,
  `src/core/StationRfKitController.{h,cpp}` (`:147-167`), `src/models/RfKitModel.{h,cpp}`
  (counters), `src/core/session/StationClient.{h,cpp}`, `src/core/session/IStationLink.h`,
  the accessory data model (`rfkit*` counters)
- Modify: `src/gui/applets/Rf2ksApplet.{h,cpp}` (`:266-281, 557-559`),
  `src/gui/setup/RfKitPage.cpp` (`:280-282, 485-512, 730-733`), `src/gui/MainWindow.cpp`
  (`:6796-6807`)
- Modify: the accessory control document, the link document, `surface.json` (regen),
  fixtures (`verbs-rfkit.json` extended)
- Test: `tests/tst_rfkit_station_control.cpp` (new), `tst_remote_peripherals`

**Interfaces:**
- Produces under `remoteRfKitControlVersion` 4:
  - `setRfKitOperate {on}`: `Rf2ksConnection::setOperateMode("OPERATE" | "STANDBY")` on the
    Core, as the local applet does.
  - `setRfKitAntenna {port}`: internal antenna 1 to 4 through `setActiveAntenna`, as the local
    applet does; refused for an antenna the amp lists as absent or disabled ("This antenna is
    not available on the RF-Kit amplifier.").
  - `setRfKitTciMode {}`: `setOperationalInterface("TCI")`, as the local page's "Set amp to
    TCI mode".
  - `setRfKitAddress {host, port}`: saves `RfKit_ManualIp` and `RfKit_ManualPort` without
    dialling, with `configureRfKit`'s checks.
- Produces on `accessoryData` (Outbound, `accessoryDataVersion` 2): `rfkitConnectedSinceMs`,
  `rfkitPollsOk`, `rfkitPollsFailed`, `rfkitReconnectCount`, `rfkitLastPollMs` (i64), from
  the counters `RfKitModel.h:53-92` keeps locally.
- Refusals: the on-air reason for all four but `setRfKitAddress`; "The Core is not connected
  to the RF-Kit amplifier."; bad-argument reasons in the document's style; minor and
  ownership reasons.

**Acceptance:**
- **B1.7:** OPERATE on the RF-Kit applet works in a remote window off the air and follows the
  amp's `operate`.
- **B1.8:** ANT 1 to 4 switch the Core's amp and follow `activeAntennaNumber`.
- **B1.9:** Setup > RF-Kit > "Set amp to TCI mode" is enabled in a remote window and sets the
  amp's interface; `operationalInterface` shows `TCI` after.
- **B1.11 (RF-Kit row):** Setup > RF-Kit Host and Port with Save reach the Core without
  dialling.
- **B1.12 (RF-Kit):** Copy diagnostics and Setup > RF-Kit > Live diagnostics show the Core's
  connection counts.
- RF-Kit TUNE and BYPASS stay hidden (`UnbuiltFeature::RfkitTune`).

**Verification:** tests first (refusals, the REST requests a fake amp records). Named tests,
conformance, surface, wording. Bench (pending): with an RF2K-S when one is available.

**Execution note (advisory):** opus. After Task 9. Shares all three big files.

- [ ] **Step 1:** Tests, the four commands, the counters, versions, documents.
- [ ] **Step 2:** Applet, page, diagnostics, fixtures.

## Task 11: VFO flag, RX applet and container buttons

**Requirements:** R-R3-49; R-R3-21; R-R3-44 (VAX in a remote window).

**Files:**
- Modify: `src/gui/VfoWidget.cpp` (`:1944-1960, 3518-3521`), `src/gui/applets/RxApplet.cpp`
  (`:1462-1490`), `src/gui/MainWindow.cpp` (`:10066-10071, 12780-12790`)
- Test: `tst_remote_slice_commands` (the test `authenticatedRemoteControlsCannotWriteXitButRitStillWorks`
  changes to prove XIT works), `tst_remote_gui_gating`

**Interfaces:**
- Consumes the two-way slice properties `xitEnabled`, `xitHz` (`MirrorPolicy.cpp:177-178`) and
  `txAntenna` (`:117`). No link change.

**Acceptance:**
- **B2.1:** XIT (button, offset, zero) on the VFO flag and the RX applet's XIT row work in a
  remote window and follow the Core's slice; they are not tied to the transmit gate. The
  renamed test proves XIT and RIT both write.
- **B2.3:** the container Antenna button box's TX antenna buttons (6 to 8) write the slice's
  `txAntenna` as the VFO flag does, with no toast.
- **B2.6:** the VAX first-run prompt for new virtual cables runs in a remote window as in a
  local one.

**Verification:** GUI only; the updated tests. Bench (pending): on the G2 at the Rock, XIT
on in a remote window; a local window on the Core shows it.

**Execution note (advisory):** opus (the gate rewiring). Shares `MainWindow.cpp`.

- [ ] **Step 1:** Tests, the three changes, commit.

## Task 12: Transmit antennas and relays

**Requirements:** R-R3-46 (narrowed by the ruling of 2026-09-24 as in Task 6); R-R3-49;
D53, D60, ruling 7.8; D61 (the transmit antenna applies at key-down).

**Files:**
- Modify: `src/core/session/MirrorPolicy.cpp` (`AlexAntennaFacade` `txAntennas`,
  `blockTxAnt2`, `blockTxAnt3`, `rxOutOnTx`, `ext1OutOnTx`, `ext2OutOnTx`, `rxOutOverride`
  become Bidirectional), `src/core/session/StationServer.cpp` (`radioHardwareVersion` 5; the
  on-air refusal on these), `src/models/RadioModel.cpp`, `src/core/AlexAntennaFacade.cpp`
  (`:206-207`)
- Modify: `src/gui/setup/AntennaAlexAntennaControlTab.cpp` (`:798-807`), `src/gui/VfoWidget.cpp`
  (`:611-627, 3522`), `src/gui/MainWindow.cpp` (`:1931, 2124-2125, 10493, 10523-10524`)
- Modify: the link document (section 7.1's `alexAntennas`), `surface.json` (regen), fixtures
- Test: `tests/tst_remote_tx_antennas.cpp` (new), `tst_remote_step_attenuator` or the existing
  Alex remote tests

**Interfaces:**
- Produces the seven properties Bidirectional under `radioHardwareVersion` 5; a write on the
  air is refused with the on-air reason; the Core applies each through `AlexController` as the
  local tab does.

**Acceptance:**
- **B4.1:** Antenna Control's TX antenna grid, Block TX on ANT2 and ANT3, RX out on TX, EXT1
  and EXT2 out on TX and RX out override change the Core's values off the air and show them.
- **B2.2:** the VFO flag's BYPS (on the boards that show it) sets the Core's `rxOutOnTx` and
  its lit state follows the Core's value, not this computer's `AlexController`.
- On the air, each greys with the on-air reason.

**Verification:** transmit-path hardware: tests first (refusal on the air; the P1 and P2 wire
bytes the Core then sends match the local path; `tst_p1_regression_freeze` and
`tst_p2_regression_freeze` pass). Bench (pending): on the G2 at the Rock, change the 20 m TX
antenna from a remote window; a local window on the Core shows it.

**Execution note (advisory):** opus. Shares all three big files.

- [ ] **Step 1:** Tests, directions, refusal, version.
- [ ] **Step 2:** The tab and the flag, fixtures, documents.

## Task 13: OC transmit pins, User Dig Out and transmit calibration

**Requirements:** R-R3-46; R-R3-49; the ruling of 2026-09-24; C5 and C6 settled by R-R3-49
(see "Questions for the operator").

**Files:**
- Modify: `src/core/session/StationServer.cpp` (`hardware/<mac>/oc/tx/...`,
  `hardware/<mac>/oc/actions/...`, `hardware/<mac>/cal/{txDisplayOffset,paSens,paOffset}` onto
  `isTransmitSettingKeyAcceptedOffAir`), `src/models/RadioModel.cpp` (the existing live reload
  of `oc/` and `cal/` covers them; the receive-only N2ADR half-apply is revisited so a full
  preset applies off the air)
- Modify: `src/gui/setup/OcOutputsHfTab.cpp` (`:164-172, 195, 260, 358, 665-670`),
  `src/gui/setup/OcOutputsSwlTab.cpp` (`:276-280`), `src/gui/setup/OcOutputsTab.cpp`
  (`:214-223`), `src/gui/setup/CalibrationTab.cpp` (`:611-615`),
  `src/gui/setup/AntennaAlexAlex1Tab.cpp` (`:939-948`), `src/gui/setup/Hl2OptionsTab.cpp`
  (`:580-584`), `src/gui/UnbuiltFeatures.h`
- Modify: the link document (section 8's settings table), fixtures
- Test: `tests/tst_remote_oc_cal.cpp` (new), `tst_unbuilt_features`

**Interfaces:**
- Consumes Task 1's off-air list and `transmit.userDigOut` (already mirrored; writable off
  the air since Task 1). Adds `UnbuiltFeature::AlexTxFilterOptions` (Alex-1 Filters: HPF
  bypass on TX, HPF bypass on PureSignal, Disable 6 m LNA on TX, the LPF band edges) and
  `UnbuiltFeature::Hl2TxTiming` (HL2 Options: TX buffer latency, PTT hang), hidden in local
  and remote windows.

**Acceptance:**
- **B4.2:** OC Outputs' TX pins per band, pin actions, Reset OC defaults, the SWL TX group
  and its Reset, and User Dig Out change the Core's values off the air; the Core's radio
  gets the new OC byte at once.
- **B4.3:** Calibration's TX Display Cal and Volts/Amps Calibration change the Core's values
  and take effect at once.
- **C5, C6:** those controls are hidden in both windows through the list, with the values
  users saved kept in the settings file.
- On the air, each write is refused and greys.

**Verification:** tests first (refusal on the air; the OC byte the Core's codec composes).
Bench (pending): on the HL2 at the Pi 4 and the G2 at the Rock, change a TX OC pin from a
remote window; the Core's next C&C frame carries it (the Core's debug log).

**Execution note (advisory):** opus. After Task 12. Shares `StationServer.cpp` and
`RadioModel.cpp`.

- [ ] **Step 1:** Tests, keys, the hidden entries, commit.

## Task 14: HL2 I/O board and link quality

**Requirements:** R-R3-46; R-R3-32 (telemetry names its source); R-R3-49.

**Source first:** HL2 I2C and the I/O board outputs from mi0bot-Thetis (the I2C read and
write paths and the output register the local `IoBoardHl2` already cites); the bandwidth
monitor's throttle and gap counting from the local `HermesLiteBandwidthMonitor`'s cites.

**Files:**
- Modify: `src/core/session/SessionCommandDispatcher.{h,cpp}`, `src/core/session/StationServer.cpp`
  (`radioHardwareVersion` 6; `stationTelemetryVersion` 5), `src/models/RadioModel.cpp`,
  `src/core/session/MirrorPolicy.cpp` (`IoBoardHl2Facade` `outputs` Outbound),
  `src/core/session/StationTelemetry.{h,cpp}`, `src/core/session/StationClient.{h,cpp}`
- Modify: `src/gui/setup/Hl2OptionsTab.cpp` (`:153-156, 517-573`), `src/gui/setup/Hl2IoBoardTab.cpp`
  (`:200, 251-258`), `src/gui/setup/RadioStatusPage.cpp` (`:757-785`),
  `src/gui/setup/DiagnosticsPhaseHPages.cpp` (`:105-117`)
- Modify: the link document, `surface.json` (regen), fixtures
- Test: `tests/tst_remote_hl2_io.cpp` (new), `tst_remote_telemetry`

**Interfaces:**
- Produces under `radioHardwareVersion` 6: `requestIoBoardI2c {bus, address, register, write,
  value}` (`write` bool; `value` i64 0 to 255, ignored on a read) whose `command.result`
  `values` carry `value` (i64) for a read, answered when the radio answers or refused after the
  local tool's own timeout ("The radio did not answer the I2C request."); `setIoBoardOutput
  {pin, on}` (the local Pin Control's action); `IoBoardHl2Facade` `outputs` (i64, one bit per
  output) Outbound.
- Produces `StationRadioTelemetry` `hl2RxBytesPerSecond`, `hl2TxBytesPerSecond`,
  `hl2Throttled` (bool), `hl2SequenceGaps` (i64), under `stationTelemetryVersion` 5.
- The I2C write and the output pins are refused on the air (they sit in the radio's
  transmit path on the N2ADR board).

**Acceptance:**
- **B4.4:** HL2 Options' I2C Read/Write tool reads and writes through the Core's radio and
  shows the read value; Pin Control switches the Core's outputs; the output LED strip follows
  `outputs`.
- **B4.5:** HL2 I/O's bandwidth monitor and OC indicator, Radio Status's Connection Quality
  card and Diagnostics > Connection Quality > Live Counters show the Core's HL2 figures
  ("from the Core"), not 0 B/s.

**Verification:** tests first (a fake I/O board answers or times out; refusal on the air).
Bench (pending): on the HL2 at the Pi 4, read the I/O board's version register from a remote
window, toggle an output pin, watch the bandwidth card.

**Execution note (advisory):** opus. After Tasks 6 and 13. Shares `StationServer.cpp` and
`RadioModel.cpp`.

- [ ] **Step 1:** Tests, verbs, telemetry, versions.
- [ ] **Step 2:** The tabs and cards, fixtures, documents.

## Task 15: Meters from the Core

**Requirements:** R-R3-13 (remote meters read the Core's readings, never local DSP);
R-R3-49.

**Source first:** Thetis `MeterManager.cs` / `console.cs` for the ADC and AGC meter types
(`ADC_REAL`, `ADC_IMAG`, `AGC_GAIN`, `AGC_AV`, `AGC_PK` or as named there) and the WDSP
`GetRXAMeter` ids the local `MeterPoller` already uses for them (`MeterPoller.cpp:416-461`).

**Files:**
- Modify: `src/core/meters/SliceMeterPump.{h,cpp}` (the five readings; the polling delay key
  re-read live), `src/models/SliceModel.{h,cpp}` (Outbound properties),
  `src/core/session/MirrorPolicy.cpp`, `src/core/session/StationServer.cpp`
  (`meterReadingsVersion` 1), `src/models/RadioModel.cpp` (`:789`, the pump's interval from a
  window's write)
- Modify: `src/gui/meters/MeterPoller.cpp` (`:333-336`), `src/gui/meters/ItemGroup.cpp`
  (`:921-955`), `src/gui/setup/MultimeterPage.cpp` (`:363-370`), `src/gui/MainWindow.cpp`
- Modify: the link document, `surface.json` (regen), fixtures
- Test: `tst_remote_meter_poller` (extended), `tests/tst_slice_meter_pump_readings.cpp` (new)

**Interfaces:**
- Produces Slice properties (Outbound) under `meterReadingsVersion` 1: `adcPeakDbfs`,
  `adcAverageDbfs`, `agcGainDb`, `agcPeakDb`, `agcAverageDb` (f64), refreshed at the pump's
  rate like `signalPeakDbm`.
- The Core re-reads the Multimeter polling delay key when a window writes it (S) and applies
  it to its pump at once, clamped to the pump's existing [10, 2000] ms.

**Acceptance:**
- **B2.5:** container meters bound to ADC Peak, ADC Average, AGC Gain, AGC Peak and AGC
  Average move in a remote window from the Core's readings; on a Core below version 1 they
  show unavailable, not a frozen value.
- **B3.11:** Setup > Display > Multimeter > Polling delay changes the Core's pump rate at
  once (a test counts the Core's polls before and after).

**Verification:** tests first (readings present, rate change). Bench (pending): on the G2 at
the Rock, an AGC Gain meter moves with band noise in a remote window.

**Execution note (advisory):** opus. Shares all three big files.

- [ ] **Step 1:** Tests, readings, live rate, version, documents, fixtures, meters.

## Task 16: DSP facts from the Core

**Requirements:** R-R3-49; R-R3-21; R-R3-40 (the Core's DSP capability, not this computer's).

**Files:**
- Modify: `src/core/session/StationServer.cpp` (`dspInfoVersion` 1), `src/models/RadioModel.{h,cpp}`
  (Outbound `noiseReductionMethods`, `dspOptionsLastApplyMs`), `src/models/SliceModel.{h,cpp}`
  (Outbound `minNotchWidthHz`), `src/core/session/SessionCommandDispatcher.{h,cpp}`
  (`dsp.filterResponse`), `src/core/session/MirrorPolicy.cpp`, `src/core/session/StationClient.{h,cpp}`
- Modify: `src/gui/VfoWidget.cpp` (`:1559-1565`), `src/gui/setup/DspSetupPages.cpp`
  (`:1243-1252, 1304-1311, 1367-1374, 2780-2808`), `src/gui/MainWindow.cpp` (`:2637-2656`),
  `src/gui/SpectrumWidget.cpp` (`:3935, 8296-8331`), `src/gui/setup/DspOptionsPage.cpp`
  (`:582-598, 628-647`)
- Modify: the link document, `surface.json` (regen), fixtures
- Test: `tests/tst_remote_dsp_info.cpp` (new), `tst_remote_slice_commands`

**Interfaces:**
- Produces under `dspInfoVersion` 1:
  - `radio` `noiseReductionMethods` (utf8, comma-separated from `nr1`, `nr2`, `nr3`, `nr4`,
    `dfnr`, `mnr`, `anf`: the ones the Core's build and platform can run, read from the same
    compile flags and platform checks `RxChannel.cpp:309-331` uses locally).
  - `radio` `dspOptionsLastApplyMs` (i64, the Core's last DSP Options apply time in ms, 0
    before any).
  - Slice `minNotchWidthHz` (f64, the Core's channel's minimum, as the local TNF page reads
    it).
  - `dsp.filterResponse {sliceId, highResolution}` whose result `values` carry `startHz`,
    `stepHz` (f64) and `magnitudesDbJson` (utf8, a JSON array), computed from the Core's
    channel as the local filter graph computes it.

**Acceptance:**
- **B2.4:** the VFO flag and Setup > DSP > NR/ANF offer MNR and DFNR by the Core's
  `noiseReductionMethods`; a Mac window never offers MNR on a Linux Core; a Linux window
  offers MNR on a Mac Core that runs it; on a Core below version 1, MNR and DFNR are shown
  disabled with "This Core does not say which noise reduction it can run. Updating the Core
  may help."
- **B3.4:** notch width presets are checked against the Core's `minNotchWidthHz`; the TNF page
  shows it.
- **B3.6:** "High-resolution filter characteristics in filter graph" is enabled in a remote
  window and draws the Core's curve.
- **B3.8:** "Time to last change" shows the Core's apply time.

**Verification:** unit and conformance. Bench (pending): a Mac window on the Rock's Core does
not offer MNR; the filter graph draws for the G2.

**Execution note (advisory):** opus. Shares all three big files.

- [ ] **Step 1:** Tests, properties, the filter response verb, version.
- [ ] **Step 2:** The flag, pages and notch presets, fixtures, documents.

## Task 17: Panadapter display requests

**Requirements:** R-R3-01, R-R3-04, R-R3-08 (the display request and its grant); R-R3-11
(no second local calibration); R-R3-12 (the GUI keeps its own smoothing); R-R3-41 (the Core
places its own threads); R-R3-49.

**Files:**
- Modify: `src/gui/RemoteMediaController.cpp` (`:217-225, 276, 313, 323-328, 3483-3509`),
  `src/gui/SpectrumWidget.cpp` (`:1914-1929, 2219-2232, 3375-3376, 4167-4175, 5306-5312`),
  `src/gui/setup/DisplaySetupPages.cpp` (`:170-221, 750-762, 1199-1220`), `src/gui/MainWindow.cpp`
  (`:2332-2356, 4484, 4556`)
- Modify (decimation): `src/core/session/media/DaemonMediaController.cpp`, the Core's FFT
  engine setup, `src/core/session/StationServer.cpp` (`spectrumGrantVersion` 2)
- Modify: `docs/architecture/2026-09-20-remote-media-control-v1.md` (the `decimation` field,
  the dBm window rule), the link document, `surface.json` (regen), fixtures and media vectors
- Test: `tst_remote_media_controller`, `tst_remote_spectrum_render`, `tst_remote_spectrum_context`

**Interfaces:**
- The subscribe request's `minDbm` and `maxDbm` become the pan's own dBm range widened by the
  waterfall's low and high levels, so the 256 levels span what the pan shows; a range change
  re-subscribes at most once per frame period (debounced over a drag).
- `spectrumGrantVersion` 2 adds the request field `decimation` (i64, the values the local
  Rendering > Decimation offers), applied to the endpoint's engine; a peer below 2 does not
  send it.
- The window uses the granted FFT size (today only logged) for its Hz/bin readout and
  normalise offset, and the granted frame rate (after the display budget) for its averaging
  and peak-decay time constants.

**Acceptance:**
- **B3.3:** a signal above 0 dBm is drawn at its level, not flat-topped; over a 20 dB range
  the steps are no coarser than 0.1 dB; the dBm strip's arrows, drag, range zoom, wheel and
  Ref Level / Dyn Range all do so.
- **B3.2:** the Hz/bin readout and Normalize trace follow the Core's granted FFT size (at
  16384 no 6 dB error) and follow zoom.
- **B3.7:** Rendering > Decimation reaches the Core's engine for this pan.
- **B3.9:** Spectrum Defaults opens on the Core's stored FFT size, window, Hz/bin target and
  FPS, and its readouts use the granted values.
- **B3.10:** Spectrum and Waterfall Avg Time and the peak hold and blob decay use the frame
  rate actually sent; when the budget lowers it, the times stay right.
- **C10 (settled):** Calibration & Peak Hold > Cal Offset is disabled in a remote window with
  "The Core calibrates the display for its radio." (R-R3-11).
- **C11 (settled):** Thread > Display Thread Priority is disabled in a remote window with "The
  Core sets its own display thread priority." (R-R3-41).

**Verification:** a media request change: vectors and fixtures first. Named tests, the media
runner, surface, wording. Bench (pending): on the G2 at the Rock, a strong local signal above
0 dBm in a remote window, and a 20 dB range with no visible steps.

**Execution note (advisory):** opus. Shares `MainWindow.cpp` and `StationServer.cpp`.

- [ ] **Step 1:** The dBm window, granted size and rate, tests.
- [ ] **Step 2:** Decimation on the wire, the Spectrum Defaults page, C10, C11, documents,
  fixtures.

## Task 18: Pan and slice interaction

**Requirements:** R-R3-09 (context changes do not lose display state); R-R3-24 and R-R3-34
(a restored layout is never blank without a reason); R-R3-49; the operator's rule that a
button on a pan acts on that pan.

**Files:**
- Modify: `src/gui/MainWindow.cpp` (`:3157-3164, 3821-3826, 5732-5785, 12100-12108,
  12129-12130`), `src/models/RadioModel.cpp` (`:8695-8747`), `src/gui/RemoteMediaController.cpp`
  (`:1866-1875, 1944-1965`), `src/gui/SpectrumOverlayPanel.cpp` (`:731-733`),
  `src/gui/SpectrumWidget.cpp` (`:8675`), `src/core/session/StationServer.cpp`
  (`setSustainableSliceLimit`)
- Test: `tst_remote_media_controller`, `tests/tst_pan_actions_per_pan.cpp` (new)

**Source first:** a spot's left-click action from AetherSDR's spot overlay (the port it came
from), cited `[@sha]`.

**Acceptance:**
- **B3.1:** the overlay BAND flyout on a pan whose slice is not the active one changes that
  pan's slice (`onBandButtonClicked(slice, band)`), local and remote.
- **B3.5:** selecting the other slice's flag on a pan with two slices on one receiver keeps
  the waterfall, its rewind history and the 3D stack.
- **C8 (settled):** a restored pan with no Core slice shows "No slice here yet. Add one with
  +RX." (plain words, passing the wording sweep) instead of a blank pan.
- **Passing:** the Display flyout's "Grid Lines" toggles the grid; the Display flyout and the
  Clarity badge work on every pan, each acting on its own pan; a left-click on a spot does
  what AetherSDR's does (cited); Pan Layout and +PAN use the Core's advertised slice limit
  in a remote window (and the Core's own board limit locally).

**Verification:** GUI and one media path; tests first for B3.1 and B3.5. Bench (pending): two
pans on the G2 at the Rock; BAND on the second pan changes only its slice.

**Execution note (advisory):** opus. Shares `MainWindow.cpp` and `RadioModel.cpp`.

- [ ] **Step 1:** Tests, B3.1, B3.5, C8.
- [ ] **Step 2:** The passing items, commit.

## Task 19: Record streams and spot sources at the Core

**One owner (controller, 2026-09-24):** the Core-side work here is the iPhone plan's station Task 21, which this session owns. Run that task's text and add this task's window rows to its acceptance, in one implementer and one set of commits. Do not build the Core side twice.

**Requirements:** R-IOS-25 (the station's spot clients); the iPhone plan's Task 21 station
half (remote design sections 6.1a and 6.4); R-R3-49. This task builds that half; see "Plan
text to change".

**Files:**
- Create: `src/core/session/RecordStream.{h,cpp}`, `src/core/SpotSourceHost.{h,cpp}`
- Modify: `src/gui/MainWindow.cpp` (the spot clients started at `MainWindow.cpp:834-840` and
  `:1009` move into `SpotSourceHost`, used in-process by a local window and by the Core),
  `src/models/RadioModel.cpp` (`:2946-2959`, `restoreSpotClientAutoStartState` runs on the
  Core), `src/core/settings/SettingsScope.cpp` (`:385-398, 479`), `src/core/settings/SettingsProxy.cpp`
  (`:112-127`), `src/models/SpotModel.{h,cpp}`, `src/core/session/SessionMessages.{h,cpp}`,
  `src/core/session/SessionCommandDispatcher.cpp`, `src/core/session/StationServer.cpp`,
  `src/core/daemon/DaemonApp.cpp`, `src/gui/SpotHubDialog.cpp`
- Modify: the link document (a Record streams section), `surface.json`, fixtures
- Test: `tests/tst_record_stream.cpp`, `tests/tst_spot_source_host.cpp`,
  `tests/tst_remote_spots.cpp` (the names Task 21 gave them)

**Interfaces (Task 21's, unchanged):** verbs `records.subscribe {stream, backlog}` and
`records.unsubscribe {stream}`, capability `recordStreamVersion` 1; `record.batch {stream,
generation, reset, upserts: [{id, fields}], removes: [id]}`; stream `spots` (fields `timeUtc`,
`frequencyHz`, `call`, `mode`, `source`, `spotter`, `comment`, `band`, `dxccColour`,
`dxccPriority`; backlog the newest 500); stream `spotConsole:<source>` (backlog 200); object
`spotSources`; verbs `spots.connect {source}`, `spots.disconnect {source}`,
`spots.sendCommand {source, text}`, `spots.clearAll {}`. The WSJT-X and SpotCollector
listeners and their ports are Window scope (they listen on this computer); the other sources'
settings are Station scope.

**Acceptance:**
- **B7.1:** the Core starts every source whose Auto-Connect / Auto-Start is on, with no window;
  a remote window shows the Core's spots and never opens its own cluster login; a second
  window does not open another.
- **B7.2:** with no Core session, the Spot Hub's Station-scoped settings are disabled with
  "Connect to the Core to change these." (Setup's own words); the WSJT-X and SpotCollector
  ports are this computer's and editable.
- A local window behaves exactly as before (its spot tests pass); a slow subscriber's
  backlog is bounded.

**Verification:** unit and integration, tests first for the stream bounds. Bench (pending): the
Rock's Core logged into the DX cluster with no window open; a remote window shows its spots.

**Execution note (advisory):** opus. Moves live code out of `MainWindow`. Before Tasks 20 to
23. Shares all three big files.

- [ ] **Step 1:** The record stream mechanism with tests.
- [ ] **Step 2:** `SpotSourceHost`, scopes, the remote window's spots, fixtures, documents.

## Task 20: FreeDV Reporter at the Core

**One owner (controller, 2026-09-24):** the Core-side work here is the iPhone plan's station Task 22, which this session owns. Run that task's text and add this task's window rows to its acceptance, in one implementer and one set of commits. Do not build the Core side twice.

**Requirements:** R-IOS-26 (the callsign, never the station label), D33; the iPhone plan's
Task 22; R-R3-49.

**Files:**
- Modify: `src/core/SpotSourceHost.{h,cpp}` (runs `FreeDVReporterClient`),
  `src/models/FreeDVStationModel.{h,cpp}`, `src/core/settings/SettingsScope.cpp`,
  `src/gui/FreeDVReporterDialog.{h,cpp}`, `src/core/session/SessionCommandDispatcher.cpp`,
  `src/core/session/StationServer.cpp`, `src/models/RadioModel.cpp` (`:2150-2186, 12930-12936,
  12959, 13019`: the reporter follows the Core's own slices), `src/gui/SpotHubDialog.cpp`
  (`:1824-1840`), `src/gui/MainWindow.cpp` (`:11756-11758`)
- Modify: the link document, `surface.json`, fixtures
- Test: `tests/tst_freedv_reporter_station.cpp` (Task 22's name)

**Interfaces (Task 22's):** stream `freedvStations` (the 14 fields of `FreeDVStationModel`
plus `transmitting`, `receivingFrom`, `messageChangedAtMs`); verbs `freedv.setMessage {text}`,
`freedv.sendQsy {callsign, frequencyHz}`, `freedv.setHidden {on}`; gated on
`recordStreamVersion` 1 and a new `stationFreedvVersion` 1 (Task 22 names no capability).

**Acceptance:**
- **B7.3:** the Core registers with `StationCallsign`, grid and message from its own
  settings; a remote window's dialog is listed at once, with distance and heading, no Save &
  Propagate needed; a test asserts the station label never appears in a message.
- **B7.4:** the reported frequency follows the Core's RADE slice, and a switch into RADE shows
  the station.

**Verification:** unit with a fake Socket.IO server. Bench (pending): the live reporter shows the
Rock's station with the window closed.

**Execution note (advisory):** opus. After Task 19. Shares all three big files.

- [ ] **Step 1:** The client at the Core, the stream, the verbs, tests, the dialog.

## Task 21: The Core's radio from the window

**One owner (controller, 2026-09-24):** the Core-side work here is the iPhone plan's station Task 25, which this session owns. Run that task's text and add this task's window rows to its acceptance, in one implementer and one set of commits. Do not build the Core side twice.

**Requirements:** R-IOS-18 (Manage Radios); the iPhone plan's Task 25 (`stationRadios`,
`station.selectRadio`, `station.rescanRadios`, the This Core page's Change radio) and the Core's
choice order written there; R-R3-38; R-R3-49.

**Files:**
- Create: `src/core/station/StationRadios.{h,cpp}`, `src/gui/setup/ThisCorePage.{h,cpp}`
  (Change radio only; Task 25 adds devices and Add a device)
- Modify: `src/core/daemon/DaemonApp.cpp`, `src/core/daemon/DaemonConfig.{h,cpp}`,
  `src/core/session/SessionCommandDispatcher.cpp`, `src/core/session/StationServer.cpp`,
  `src/models/RadioModel.cpp` (`:4414, 4429`), `src/gui/MainWindow.cpp` (`:1200-1203,
  11202-11207, 11426-11433, 11504-11512, 11556-11564`), `src/gui/SetupDialog.cpp`
- Modify: the link document, `surface.json`, fixtures
- Test: `tests/tst_station_radios.cpp`, `tests/tst_this_core_page.cpp` (Task 25's names)

**Interfaces:** stream `stationRadios` `{id, name, model, mac, address, protocol, inUse}` and
verbs `station.selectRadio {mac}`, `station.rescanRadios {}` as Task 25 writes them (refused
while on the air or mid-switch; saved once the chosen radio connects; the Core restarts its
radio run; windows reconnect by themselves with the Core's reason, "The Core is switching to
<radio name>. This app reconnects by itself.", `session.end` retryable with code
`radioChanging`: the operator's ruling of 2026-09-26; the chooser's answer and the other
devices' notices go out on the restart turn, before the ends, and a change dropped there
because the radio is on the air is answered refused with the on-air reason and tells
nobody: the coordinator's ruling on follow-up N3), gated on a new `stationRadiosVersion` 1; plus `station.setRadioModel {mac, model}` (the
local Edit radio's model override, saved for that MAC on the Core and applied at the next
connect) and `station.forgetRadio {mac}` (refused for the radio in use).

**Acceptance:**
- **B6.2:** from a remote window, Radio > Connections... and the station block and title bar
  menus offer Change radio, Edit radio and Forget radio for the Core's radio; Change radio
  lists `stationRadios` and switches through `station.selectRadio`; Edit radio overrides the
  model; the choice survives a Core restart.
- **B6.3:** the title bar's Copy IP address and Copy MAC address copy the Core's radio's.

**Verification:** tests first (the choice order, refusals). Bench (pending): on the Rock's
Core, list radios from a remote window; select the G2 again; nothing else changes.

**Execution note (advisory):** opus. After Task 19. Choosing the Core's radio can leave it
without one: flag for earlier review. Shares all three big files.

- [ ] **Step 1:** Radios, choice order, the verbs, tests.
- [ ] **Step 2:** The page and menus, fixtures, documents.

## Task 22: Support bundle and the Core's log

**One owner (controller, 2026-09-24):** the Core-side work here is the iPhone plan's station Task 25, which this session owns. Run that task's text and add this task's window rows to its acceptance, in one implementer and one set of commits. Do not build the Core side twice.

**Requirements:** the iPhone plan's Task 25 (`support.collect`); R-R3-49.

**Files:**
- Modify: `src/core/session/SessionCommandDispatcher.cpp`, `src/core/session/StationServer.cpp`,
  `src/gui/SupportBundle.cpp` (`:35-47`), `src/gui/SupportDialog.cpp` (`:42`),
  `src/gui/setup/DiagnosticsPhaseHPages.cpp` (`:371-375`), `src/gui/MainWindow.cpp`
- Modify: the link document, `surface.json`, fixtures
- Test: `tests/tst_support_bundle.cpp` (Task 25's name), `tests/tst_remote_core_log.cpp` (new)

**Interfaces:** verb `support.collect {}` (result `bundle`, a base64 ZIP of at most 2 MiB: the
Core's recent log, its configuration with secrets removed, versions, a telemetry snapshot), as
Task 25 writes it; stream `coreLog` (Task 19's mechanism, backlog 200 lines); verb
`support.setLogCategories {categories}` (utf8 comma list of the Core's logging categories the
Support dialog shows); all gated on a new `supportBundleVersion` 1.

**Acceptance:**
- **B6.4:** Tools > Support Bundle... in a remote window includes the Core's bundle beside
  this computer's; `radio-info.json` describes the Core's radio; the logging checkboxes turn
  the Core's categories on and off. A test scans the bundle for keys, tokens, pairing codes and
  device keys and finds none.
- **B6.6:** Setup > Diagnostics > Logs > Recent Log shows the Core's log (and this
  computer's, labelled), and Refresh re-reads it.

**Verification:** tests first (the secrets scan, the 2 MiB cap). Bench (pending): a bundle from
a remote window on the Pi 4's Core contains the Core's log.

**Execution note (advisory):** opus. After Task 21. Shares `MainWindow.cpp` and
`StationServer.cpp`.

- [ ] **Step 1:** `support.collect`, the log stream, categories, tests, the dialog and page.

## Task 23: TCI in a remote window

**Requirements:** R-R3-42 (a remote window's TCI; its "raw I/Q refused until remote transmit"
is settled by the operator's decision of 2026-09-24 on Q2, that raw I/Q joins remote
transmit's scope with its bandwidth charged to the window's display share; remote transmit
exists, so this task carries it); R-R3-37 (the bytes are accounted in the display budget);
R-R3-48 (the Core's station TCI server); the iPhone plan's Task 25 (`tciClients`); R-R3-49.
Thetis publishes a receiver's I/Q at the hardware rate, not resampled
(`TCIServer.cs:925-943 [v2.10.3.15]`, `getPublishedIQSampleRate`: the largest hardware rate,
at least 48000 and at most 384000), for each receiver a client asked for, or every receiver
while Always stream IQ is on (`TCIServer.cs:5802-5809 [v2.10.3.15]`, `wantsIQStream`).

**Files:**
- Modify: `src/gui/applets/TciApplet.cpp` (`:427-476, 584-604`), `src/core/TciSwitch.cpp`
  (`:218-236`), `src/gui/setup/CatNetworkSetupPages.cpp` (`:873-913`), `src/gui/MainWindow.cpp`
  (`:9578`), `src/core/StationTciController.{h,cpp}` (`:36-45, 177`), `src/models/StationTciModel.h`
  (`:38-42`), `src/core/session/SessionCommandDispatcher.cpp` (`:375, 1340-1356`),
  `src/core/session/StationServer.cpp` (`stationTciVersion` 2, `remoteIqVersion` 1),
  `src/core/TciServer.{h,cpp}` (`:486, 2377-2385`; the remote window's I/Q path),
  `src/core/TciProtocol.cpp` (`handleIqStartStopCommand`)
- Modify (raw I/Q): `src/core/session/media/DaemonMediaController.{h,cpp}` (the Core's I/Q
  tap per slice, the same raw I/Q a local `TciServer::onRawIqDataReceived` takes),
  `src/core/session/media/DisplayBudgetSplit.{h,cpp}` (the charge),
  `src/gui/RemoteMediaController.{h,cpp}`
- Modify: the accessory control document (the `stationTci` object), the link document, the
  media control document (a "Raw I/Q (iq-stream)" section), `surface.json`, vectors and
  fixtures
- Test: `tests/tst_remote_station_tci.cpp` (new), `tests/tst_remote_tci_iq.cpp` (new),
  `tst_tci_remote_window`, `tst_tci_iq_roundtrip`, `tst_link_conformance_media`, the existing
  TCI switch tests

**Interfaces:**
- Produces under `stationTciVersion` 2: stream `tciClients` `{id, name, address,
  subscriptions, transmitting, lastCommand}` (Task 25's fields) for the Core's station
  server; verb `disconnectStationTciClient {id}`; verb `setStationTciOptions
  {emulateExpertSdr3, emulateSunSdr2Pro, cwluBecomesCw, sendInitialState}` (bools, the
  Core's `TciEmulateExpertSDR3Protocol`, `TciEmulateSunSDR2Pro`, `TciCwluBecomesCw`,
  `TciSendInitialFrequencyStateOnConnect`), and `stationTci` gains those four as Outbound
  properties.
- Where the Core's server listens stays `station_bind` in `nereusd.conf`; the page shows it
  read-only.
- Produces media capability `remoteIqVersion` 1 (appended after the last minor-11 entry; 0
  without media). A window that sees it may add `remoteIqVersion` to its media `start`; only
  then:
  - GUI-to-Core `iq-stream` with exactly `op`, `connectionId`, `sliceId` (the Core's slice,
    TCI receiver N being slice N as for audio), `revision` (nonzero uint32, rising per slice)
    and `enabled` (boolean).
  - Core-to-GUI `iq-stream-context` with exactly `op`, `connectionId`, `sliceId`, `revision`,
    `enabled`, `generation` (uint32), `sampleRateHz` (the slice's receiver rate, sent as
    Thetis sends it, not resampled) and `reason` (empty, or why it is not sent).
  - The I/Q itself: interleaved float32 pairs as the receiver delivers them, on its own stream
    of the media peer, framed as the media document's new section writes it (a stream id, a
    sequence number, a sample count); the Core starts it only while a request for that slice
    is enabled.
  - The stream's bytes are charged to that window's display share first; the window's pans
    take what is left (frame rate lowered first, then pixels, R-R3-37), never below one frame
    per second a visible pan (ruling C7). When that floor cannot be kept, the context says
    `enabled` false with the reason "The link to the Core is too busy to send raw I/Q for this
    receiver."
  - `iq-stream` is a media operation, not a setting: it is answered on and off the air, as
    receiver audio is.
- The window's TCI server: an app's `iq_start` for receiver N asks the Core for slice N's
  stream (the first app to ask starts it, the last `iq_stop` or disconnect releases it) and
  feeds the frames to the subscribed apps as a local window's I/Q tap does, with Swap I/Q
  applied here; Always stream IQ keeps the request while the server runs; `iq_samplerate`
  reports the context's rate. A refusal goes to `operatorNotice()`, never onto the TCI wire.

**Acceptance:**
- **B8.1:** the TCI applet's Enable Server goes through `TciSwitch` (saved; not undone at the
  next link event), and its log line names the right server.
- **B8.2:** the bottom-bar indicator, the TCI applet, the TCI Clients applet (with
  Disconnect), TCI Server status and Show Log, and the log window show the Core's station
  server beside this window's own, each labelled; the RF2K-S following the band shows as a
  Core client and can be disconnected.
- **B8.3:** Setup > CAT & Network > TCI Server gains a "The Core's TCI server" group whose
  Compatibility and Send initial state options change the Core's server; the window's own
  server's options stay this computer's (R-R3-42).
- **B8.4:** in a remote window on a Core at `remoteIqVersion` 1, a TCI app's `iq_start 0`
  receives slice 0's raw I/Q from the Core at the receiver's rate, in order and without gaps
  over 60 s of a test stream (sequence numbers checked); Swap I/Q swaps it; Always stream IQ
  keeps it flowing with no app asking; `iq_stop` from the last app stops the Core's stream
  (the Core's tap count returns to 0).
- **B8.4, the budget:** with I/Q running, the window's pans drop frame rate first and never
  below one frame per second; a share too small for the floor gets `enabled` false with the
  reason above, and the app gets no I/Q.
- **B8.4, older Core:** on a Core below `remoteIqVersion` 1 the IQ Stream options are disabled
  with "This Core does not send raw I/Q to this window. Updating the Core may help." and
  `iq_start` sends nothing.
- **B8.4, older window:** a peer that did not declare `remoteIqVersion` gets today's wire
  (fixture).

**Verification:** unit; tests first for the options and the disconnect, then vectors for the
I/Q operations and the budget cases. Named tests, `tst_link_conformance_media`, surface,
wording. Bench (pending): with the RF2K-S following the band on the Core's TCI server, a remote
window lists it; on the G2 at the Rock, a TCI I/Q app (for example SDR-Console or CW Skimmer
through TCI) in a remote window decodes from the Core's raw I/Q.

**Execution note (advisory):** opus. After Task 19. Shares `MainWindow.cpp` and
`StationServer.cpp`.

- [ ] **Step 1:** The stream, the verbs, the options, tests.
- [ ] **Step 2:** The applets, page and indicator, fixtures, documents.
- [ ] **Step 3:** Raw I/Q: the media operations, the Core's tap, the budget charge, the window's
  TCI path, vectors, fixtures, documents.

## Task 24: Setup diagnostics and preferences

**Requirements:** R-R3-49; R-R3-21; R-R3-17 and R-R3-38 (the configured Core and
connecting); R-R3-44 (VAX in a remote window).

**Files:**
- Modify: `src/models/RadioModel.cpp` (`:16183-16185`: settings validation runs for the
  Core's settings in a remote window and after a Core snapshot), `src/gui/setup/GeneralSetupPages.cpp`
  (`:30-53`), `src/gui/MainWindow.cpp` (`:12623`), the Core target store
  (`CoreTargetStore`), `src/gui/SetupDialog.cpp` (`:1027-1032`),
  `src/gui/setup/GeneralOptionsPage.cpp` (`:506-523`), `src/gui/PsaIndicatorWidget.cpp`
  (`:243-257`), `src/gui/setup/AudioAdvancedPage.cpp` (`:242, 462-482`),
  `src/gui/setup/RadioStatusPage.cpp` (`:646, 657`), `src/gui/setup/DiagnosticsPhaseHPages.cpp`
  (`:203, 218`)
- Test: `tests/tst_remote_setup_diagnostics.cpp` (new), `tst_unbuilt_features`

**Acceptance:**
- **B6.7:** Settings Validation's issue list, Re-validate and Radio Status > Settings Hygiene
  validate the Core's settings in a remote window (and Re-validate re-runs `validate()`
  locally too: passing bug).
- **B6.8:** in a remote window, Startup & Preferences' Auto-connect reads and writes a flag
  for this Core in `CoreTargetStore`, and the window honours it at launch.
- **B6.9:** Hide feedback level and Swap red and blue PS-A feedback colours apply to the FB
  indicator at once in a remote window, and a click on the FB label updates the boxes.
- **B6.10:** Reset all audio to defaults turns the headphones off (their default state),
  moves MON back and rebuilds the VAX outputs, locally and remotely, with no restart. (JJ's
  ruling, 2026-09-28: reset turns headphones off, as the code does; it does not reopen them.)
- **Passing:** Radio Status and Settings Validation's Reset to defaults and Forget this
  radio pass the radio's MAC (the Core's in a remote window, through Task 21's verbs there);
  Uptime counts from the radio's connect; Send IQ to VAX loses its remote hard-disable, so the
  unbuilt-feature hide alone governs it until it is built.

**Verification:** tests first for the MAC and validation cases. Bench (pending): on the G2 at
the Rock, a remote window with auto-connect off does not reconnect at launch.

**Execution note (advisory):** opus. After Task 21. Shares `MainWindow.cpp` and
`RadioModel.cpp`.

- [ ] **Step 1:** Tests and each item, commit.

## Task 25: Dead controls in both windows

**Requirements:** R-R3-49 (every visible control does what its label says; an unbuilt one is
hidden through the one list).

**Files:** `src/models/RadioModel.{h,cpp}` (`handleGanymedeTrip`), `src/core/safety/TxInhibitMonitor.{h,cpp}`
(`setUserIoReader`), `src/gui/meters/ItemGroup.cpp`, `src/gui/meters/MeterItem.cpp`,
`src/gui/meters/FilterDisplayItem.cpp`, `src/gui/meters/ClickBoxItem.cpp`,
`src/gui/meters/MeterWidget.cpp`, the container dispatcher, the Audio Devices card
(`src/gui/setup/AudioDevicesPage.cpp` or where it lives), Colors & Theme, the Multimeter page,
`src/gui/setup/GeneralOptionsPage.cpp`, the TX Display page, `src/gui/UnbuiltFeatures.h`,
`src/gui/MainWindow.cpp`. Test: `tst_unbuilt_features`, one test per wired item.

**Acceptance:**
- **An audit table first, in the report:** for each item below, whether its data source
  exists in NereusSDR today (named: the class and signal, or "none, checked by grep of ...") and
  the outcome: **wire** (connect it to that source, citing Thetis or AetherSDR where the
  behaviour comes from them) or **hide** (a new `UnbuiltFeature` entry naming it, in local and
  remote windows; saved values stay in the settings file). An item whose wiring needs a new
  Thetis port beyond connecting an existing source is **hide**, with the port named in the
  entry's comment.
- Items: the PA trip badge (`handleGanymedeTrip` has no caller); the TX Inhibit badge (no
  `setUserIoReader` caller; source-first from Thetis's user input TX inhibit); the PBSNR meter
  binding; `FilterDisplayItem` and `ClickBoxItem` in containers; the voice, macro, band-stack
  and filter-context container signals; the Devices card's Bit depth, Auto-match, Monitor TX
  input and Tone check; Colors & Theme > Waterfall Low Level Color; Multimeter Averaging
  window; General > Options Region, Extended, Receive Only and Prevent TX on a different band;
  TX Display's "TX Grid Scale" placeholder.
- After the task no item in the table is visible and inert in either window
  (`tst_unbuilt_features` extended).
- RADE end-of-over decodes are not in this task (a question below).

**Verification:** the audit table, then a test per wired item. Bench (pending): the operator
looks over the table and the changed Setup pages.

**Execution note (advisory):** opus. After Task 24. Shares `MainWindow.cpp` and
`RadioModel.cpp`.

- [ ] **Step 1:** The audit table in the report.
- [ ] **Step 2:** Wire or hide each item, tests, commit.

## Task 26: The R3 control matrix

**Requirements:** R-R3-21 (a visible-control inventory distinguishes implemented behaviour
from unavailable features).

**Files:** `docs/architecture/2026-09-20-remote-daemon-r3-verification/remote-controls.md`.

**Acceptance:**
- The seven stale rows the sweep names are rewritten to what the code now does: line 83
  (Protocol Info works after the handshake), 116 (the Phone/CW mic profile, level, PROC, AM
  carrier and DEXP work through the Core off the air; CW and FM pages hidden as unbuilt), 125
  (the PowerGenius XL tab is a live Core view plus `PgxlAdvancedPage`; OPERATE through Task
  9), 140 (overlay ATT and zoom work remotely), 154 (Startup & Preferences auto-connect,
  callsign and grid are wired; auto-connect per Core since Task 24), 159 (the PA pages are
  shown and work off the air since Task 6), 169 (DSP > CW APF and AM and FM squelch work
  remotely).
- Every row this plan changed names its task's evidence and "hardware pending" until the
  operator's bench lines are checked.

**Verification:** `python3 scripts/render-link-tables.py --check`; no em dash.

**Execution note (advisory):** sonnet (documentation). After Task 33; Task 34 (the bench)
follows it.

- [ ] **Step 1:** Rewrite the rows, commit.

## Task 27: The transmit display's skirt, found and fixed at its cause (row 15)

**Requirements:** A11 (the pan while the Core transmits: the picture must be right before it
is sent anywhere); R-R3-49; row 15 of `docs/architecture/tx-display-verification/README.md`
(a skirt about 35 dB down 66 Hz from the peak with Blackman-Harris 4T selected, already in
WDSP's raw `GetPixels` output, bench 2026-08-05). This runs in the local window first: both
windows draw what this analyzer makes.

**Source first** (Thetis v2.10.3.15 at `3759d096`; read each before comparing):
- `Console/HPSDR/specHPSDR.cs:504-643` (`initAnalyzer`, the path Thetis uses for the transmit
  panadapter): `CLIP_FRACTION` 0.04 (`:529`), overlap `ceil(fft_size - sample_rate /
  frame_rate)` (`:532`), the zoom and pan span clips, `max_w` from `KEEP_TIME` 0.1
  (`:487`, `:585`), and the `SetAnalyzer` call (`:624-643`); its fields `spur_eliminationtion_ffts`
  1 (`:70`), `data_type` 1 (`:80`), `window_type` 4 (`:134`), `kaiser_pi` 14.0 (`:145`),
  `stitches` 1 (`:206`), `frame_rate` 15 (`:335`), `_pixel_out` 2 (`:471`).
- `Console/HPSDR/specHPSDR.cs:738-805` (`CalcSpectrum`: span clips by filter edges, `sclip` 0),
  which Thetis uses for the transmit display only from `UpdateTXDisplayVars`
  (`console.cs:8024-8059`) in the SPECTRUM, HISTOGRAM and SPECTRASCOPE modes; in PANADAPTER and
  PANAFALL the transmit analyzer is set by `initAnalyzer` through `CalcTXDisplayFreq`
  (`console.cs:7967-7971`). NereusSDR's `TxAnalyzer::applySetAnalyzer` follows `CalcSpectrum`
  on the panadapter; the comparison says whether that matters.
- `Console/radio.cs:2605-2624` (`BufferSize`: the transmit display's `BlockSize` is the TX DSP
  buffer size and its `SampleRate` 96000), `console.cs:20197` (`FrameRate` = `wdspFps`),
  `setup.cs:18149-18210` (the TX Display controls; FFT size `4096 * 2^slider` at `:18179`).
- The tap: `wdsp/TXA.c:394-403` (`create_siphon`, size `dsp_size`, 16384 buffered), `:586`
  (`xsiphon` after the ALC meter, before PureSignal's `xiqc`, the CFIR and the output
  resampler), `:659` (`setSamplerate_siphon` at `dsp_rate`), `:733` (`setSize_siphon`
  `dsp_size`); `Console/cmaster.cs:544-545` (`TXASetSipMode` 1, `TXASetSipDisplay` =
  `cmaster.inid(1, 0)`); `ChannelMaster/cmaster.c:192-198` (`XCreateAnalyzer(in_id, &rc,
  262144, 1, 1, "")`).
If the cause turns out to sit inside WDSP's analyzer itself (Thetis's own argument set shows
the same skirt on the same samples), stop with NEEDS_CONTEXT and the numbers: nothing in WDSP
is changed without the operator.

**Files:**
- Modify: `src/core/TxAnalyzer.{h,cpp}` (the argument seam below; the fix if the cause is the
  analyzer's set-up)
- Modify, only where the comparison puts the cause: `src/gui/MainWindow.cpp` (the MOX edge's
  block size, window or pixel count), `src/core/TxChannel.{h,cpp}` and
  `src/core/TxWorkerThread.{h,cpp}` (block continuity or rate into the siphon),
  `src/core/WdspEngine.cpp` (the siphon's mode, display and rate)
- Modify: `docs/architecture/tx-display-verification/README.md` (row 15 and its section: the
  cause, the fix, status "FIXED, bench re-check")
- Test: `tests/tst_tx_analyzer_skirt.cpp` (new, labels `core REALTIME`: real WDSP, FFTW
  planning), `tst_tx_analyzer_settings`, `tst_tx_display_window`

**Interfaces:**
- Produces `struct TxAnalyzerArgs` in `TxAnalyzer.h` (`nPixout`, `nFft`, `typ`, `sz`, `bfSz`,
  `winType` (int), `pi` (double), `ovrlp`, `clp` (int), `fscLin`, `fscHin` (double), `nPix`,
  `nStch`, `calset` (int), `fmin`, `fmax` (double), `maxW` (int), `sampleRateHz` (double)),
  every value `SetAnalyzer` and `SetDisplaySampleRate` are handed, and
  `TxAnalyzerArgs TxAnalyzer::currentArgs() const`, filled by the same code
  `applySetAnalyzer` uses (one computation, read by the call and by the test).

**Acceptance:**
- **The comparison, in the report first:** a table of every `TxAnalyzerArgs` field with
  NereusSDR's value at TUNE in LSB, TX filter 100 to 2900 Hz, on a 1200-pixel pan at the
  default zoom, beside Thetis's value from the cites above for the same state (FFT size 32768
  from slider 3, block size the TX DSP buffer, 96000 Hz, 15 frames a second, and the window
  selected), each row marked same or different with the Thetis line. The tap row compares the
  TX channel's siphon (position, `dsp_rate`, `dsp_size`, mode and display) with `TXA.c` and
  `cmaster.cs`, read from NereusSDR's channel set-up, not assumed.
- **Synthetic tone:** `tst_tx_analyzer_skirt` creates the analyzer at display 5 as
  `cmaster.c:192-198` does, applies `currentArgs()` with window 1 (Blackman-Harris 4T), and
  feeds a continuous complex tone at -600 Hz of amplitude 0.99999 (Thetis's `MAX_TONE_MAG`) in
  blocks of the TX channel's `dspBlockFrames()` through `Spectrum0`, as the siphon does. The
  pixel 66 Hz either side of the peak pixel is at least 90 dB below the peak, and so is every
  pixel beyond it out to the window's edge.
- **Through the TX channel:** the same test opens a real TX channel (the
  `tst_dsp_control_transmit` pattern, no radio), runs the TUNE generator with the settings the
  TUNE path gives it, and pumps blocks the way `TxWorkerThread` does; the same 90 dB holds, and
  the test counts that every block reaches the siphon once, in order, with none skipped or
  repeated.
- Both cases fail before the fix when the cause is on NereusSDR's side (the report shows the
  failing numbers), and pass after it. The fix changes only what the comparison marked
  different or the continuity count caught; every other argument stays as it is.
- Rows 1 to 8 of the matrix still hold in the unit tests that cover them
  (`tst_tx_analyzer_settings`, `tst_tx_display_window`), and `bf_sz` still equals the TX
  channel's `dsp_size`.

**Verification:** a cause first, then the fix: the comparison table, then the two skirt cases
red, then green. Build `NereusSDR`, `tst_tx_analyzer_skirt`, `tst_tx_analyzer_settings`,
`tst_tx_display_window`; run each by exact name with `QT_QPA_PLATFORM=offscreen` and
`--no-tests=error`. Bench (pending, the operator's): row 15 re-checked on the G2 in a local
window, TUNE into a dummy load, Blackman-Harris 4T: the skirt at 66 Hz is at least 90 dB
down, and an A/B screenshot against Thetis on the same radio and frequency agrees.

**Execution note (advisory):** opus. Before Task 28 (the Core sends what this analyzer makes).
Touches `MainWindow.cpp` only if the cause is there.

- [ ] **Step 1:** The argument seam, the comparison table, the two skirt cases (red).
- [ ] **Step 2:** The fix at the cause, the cases green, row 15 written up, commit.

## Task 28: The Core sends the transmit display

**Requirements:** A11 (the panadapter while the Core transmits); R-R3-49; R-R3-01 and R-R3-08
(the display request and its grant); R-R3-37 (display bytes stay in the budget); R-IOS-13 (the
keyed view on a remote device). Thetis shows the transmit analyzer, never the receiver, on
the transmitting receiver's display while keyed and display duplex is off:
`console.cs:24281-24338 [v2.10.3.15]` (DisplayThread, `if (bLocalMox && !_display_duplex)`
then `GetPixels(cmaster.inid(1, 0), 0, ...)` for the trace and `GetPixels(cmaster.inid(1, 0),
1, ...)` for the waterfall). While keyed, the transmit display follows XIT when duplex is off:
`console.cs:22069-22150 [v2.10.3.15]` (`getLowHighForRXn`, "xit, only when txing", `if
(chkXIT.Checked && !_display_duplex)`).

**Files:**
- Create: `src/core/TxDisplayFeed.{h,cpp}` (one owner of the TX analyzer's view, start and stop,
  for every viewer)
- Modify: `src/core/TxAnalyzer.{h,cpp}` (`clampViewToBaseband`), `src/models/RadioModel.{h,cpp}`
  (`txDisplayFeed()`, made in `setTxAnalyzer`), `src/core/daemon/DaemonApp.cpp` (its MOX
  start and stop move into the feed), `src/core/session/TransmitStateFacade.{h,cpp}` and
  `src/core/session/MirrorPolicy.cpp` (`highSwr`, `swrWindBackLatched`)
- Modify: `src/core/session/media/DaemonMediaController.{h,cpp}`,
  `src/core/session/media/DaemonSpectrumSource.{h,cpp}` (the transmitting pan's endpoints take
  their frames from the feed while keyed), `src/core/session/StationServer.cpp`
  (`txDisplayVersion` 1), `src/gui/RemoteMediaController.{h,cpp}` (declares it at `start`,
  sends `txMinDbm`/`txMaxDbm`, reads `transmit`, hands transmit frames on; drawing them is
  Task 29)
- Modify: `docs/architecture/2026-09-20-remote-media-control-v1.md` (a "Transmit display"
  section after "Display subscriptions"), the link document (section 6.3's `txDisplayVersion`
  note, section 18.8's `txState` table), `surface.json` (regen), the media vectors under
  `tests/data/link/v1/media/` and the session fixtures, written from traces
  (`NEREUS_LINK_TRACE_DIR` with the conformance runners)
- Test: `tests/tst_tx_display_feed.cpp` (new), `tests/tst_remote_tx_display.cpp` (new),
  `tst_remote_media_controller`, `tst_remote_spectrum_context`, `tst_link_conformance_media`,
  `tst_link_conformance_session`

**Interfaces:**
- Produces `struct TxDisplayView { double carrierHz; int lowHz; int highHz; int pixels; }` and
  `static TxDisplayView TxAnalyzer::clampViewToBaseband(double carrierHz, double centreHz,
  double spanHz, int pixels)`: the view clamped inside the siphon's +/-48 kHz baseband, its edges
  relative to the carrier and quantised to 100 Hz, a span under 1000 Hz left as the last good
  one: the rule `MainWindow`'s `syncTxAnalyzerToView` applies today, moved here unchanged.
- Produces `class TxDisplayFeed : public QObject` (owned by `RadioModel`,
  `TxDisplayFeed* RadioModel::txDisplayFeed() const`, null without a TX analyzer):
  - `int addViewer(double centreHz, double spanHz, int pixels, bool local)`,
    `void updateViewer(int id, double centreHz, double spanHz, int pixels)`,
    `void removeViewer(int id)`.
  - The governing viewer sets the analyzer's view: the local viewer when there is one (a
    desktop window running its own DSP, or one hosting a Core), otherwise the lowest id.
    `TxDisplayView currentView() const`, `bool isGoverning(int id) const`.
  - On the MOX rise (`MoxController::moxStateChanged`, any source) it sets the analyzer's
    block size from `TxChannel::dspBlockFrames()`, the carrier from
    `RadioModel::txFrequencyForSlice(txBoundSlice())`, the view and pixel count from the
    governing viewer, and starts it; on the fall it stops it and clears the window (the
    analyzer runs on every key, viewers or not, as `DaemonApp::createTxAnalyzer` does today).
    While keyed it recomputes the carrier and view on every change of the transmit slice's
    frequency, `xitEnabled` or `xitHz`, and on a viewer's update.
  - Signals: `void viewChanged(const TxDisplayView& view)`, `void traceReady(const
    QVector<float>& dbm)`, `void waterfallReady(const QVector<float>& dbm)` (the analyzer's
    `txFftReady` and `txWaterfallReady`), `void keyedChanged(bool keyed)`.
- Produces capability `txDisplayVersion` 1 (appended after the last entry of the minor-11
  block; 0 below minor 11 and on a station with no TX analyzer). A window that sees it may add
  `txDisplayVersion` (a whole number of at least 1) to its media `start`, as
  `receiverAudioVersion`; only then:
  - `subscribe` may carry `txMinDbm` and `txMaxDbm` (finite, `txMinDbm < txMaxDbm`, inside -400
    to 100; the transmit quantisation window; absent, the endpoint's `minDbm`/`maxDbm`).
  - Every `context` for that peer carries one more field, `transmit` (boolean).
  - While the Core's radio is keyed, each endpoint of that peer whose slice is the transmit
    slice or shares its pan on the Core (`panKey`) is a viewer of the feed: it is sent a new
    context with `transmit` true, `sourceCentreHz` the carrier, `sampleRateHz` 96000,
    `centreHz` and `spanHz` the feed's view (carrier plus `(lowHz + highHz) / 2`, `highHz -
    lowHz`), `traceSamples` and `waterfallSamples` the view's pixels, `grantedFftSize` the
    analyzer's FFT size, `fps` the analyzer's output rate, `minDbm`/`maxDbm` the transmit window,
    and `limit` `shared` when it does not govern; then trace and waterfall frames from
    `traceReady` and `waterfallReady`, encoded by `DisplayCodec` as receive frames are. No
    receive frame is sent to that endpoint until the fall, which sends a context with
    `transmit` false and resumes receive frames (the window asks a keyframe as after any
    context).
  - A later `subscribe` from that endpoint while keyed updates its viewer (a pan or zoom on the
    transmitting pan moves the analyzer's view when it governs).
  - Transmit frames are charged to the display budget exactly as receive frames of the same
    size.
- A peer that did not declare it gets exactly today's wire: no `transmit` field, and receive
  frames while keyed.
- Produces on `txState`, Outbound, under `txDisplayVersion` 1: `highSwr` and
  `swrWindBackLatched` (bool), the values `RadioModel` hands the local window's
  `setHighSwrOverlay` (`RadioModel.cpp:1537-1543`), for Task 29's high-SWR border.

**Acceptance:**
- **Rise:** a declaring window with a pan on the transmit slice; the Core keyed through its
  `MoxController` against a test `TxChannel` (the Global Constraints' pattern). Within one frame
  period the endpoint gets a context with `transmit` true, the carrier from
  `txFrequencyForSlice` and the view clamped by `clampViewToBaseband`; the frames it gets decode
  to the pixels the feed emitted (a test seam emits known pixels); no receive frame reaches that
  endpoint between the rise and the fall (counted: 0).
- **Fall:** a context with `transmit` false, then receive frames again, with the receive
  centre, span and window the endpoint had before the rise.
- **Other pans:** an endpoint on a pan without the transmit slice keeps receive frames through
  the whole key.
- **Row 16 (XIT, Core side):** changing XIT or the transmit slice's frequency while keyed
  sends a renewed context with the new centre within one frame period, and the frames follow
  it.
- **Several viewers:** two endpoints on the transmitting pan asking different spans: the
  lower id governs, the other's context says `limit` `shared` with the governing view; when the
  governing one leaves, the other governs and its context says `none`. With a local viewer (a
  desktop window hosting the Core), the local one governs.
- **Baseband:** a requested view past +/-48 kHz from the carrier comes back clamped; a span
  under 1000 Hz keeps the last good view.
- **Older peers:** a peer that did not declare `txDisplayVersion` gets byte-for-byte today's
  contexts and receive frames (a fixture from the trace proves it); a peer below minor 11 never
  sees the capability; a Core without a TX analyzer sends 0.
- **Refusals:** `txMinDbm` at or above `txMaxDbm`, or outside -400 to 100, is refused as a
  request the Core cannot read, as the receive window is.
- **Budget:** a keyed endpoint's bytes per second stay within its share; a lowered share
  lowers the transmit frame rate as it does the receive one.

**Verification:** a media wire change: vectors and fixtures first (from traces), then the
feed. Build `NereusSDR`, `nereusd` and the named tests; run them by exact name, then
`tst_link_conformance_media`, `tst_link_conformance_session`, `tst_link_surface_manifest`,
`render-link-tables.py --check` and the wording sweep. Bench (pending): with Task 29, rows 19
and 20 of the matrix.

**Execution note (advisory):** opus. After Task 27. Shares `StationServer.cpp` and
`RadioModel.cpp`.

- [ ] **Step 1:** `clampViewToBaseband`, `TxDisplayFeed` and its tests; `DaemonApp` onto the
  feed.
- [ ] **Step 2:** The capability, the subscribe fields, the transmit context and frames, the
  `txState` flags, the documents, vectors and fixtures.

## Task 29: One MOX display controller for both windows

**Requirements:** A11 (the transmit view in a remote window: the transmit spectrum and
waterfall, the red border, the TX filter overlay, the high-SWR border, the waterfall stop and
the transmitting pan's tuning while keyed); R-R3-49 (parity both ways); R-R3-12 (the GUI keeps
its TX pause and palette); row 16 of the verification matrix (XIT while keyed). Thetis
(v2.10.3.15): the waterfall's own transmit levels, colour scheme and low colour while keyed
(`display.cs:6420-6427`, `TXWFAmpMin` -70 and `TXWFAmpMax` 30 at `display.cs:1917-1937`); the
MOX edge caches the waterfall minimum and purges buffers (`display.cs:1575-1597`); the transmit
grid while keyed (`SpectrumGridMaxMoxModified`, `display.cs:1782-1790`); XIT while keyed
(`console.cs:22069-22150`, `getLowHighForRXn`).

**Files:**
- Create: `src/gui/MoxDisplayController.{h,cpp}`, `src/gui/TxDisplaySource.{h,cpp}` (the
  interface and its local and remote sources)
- Modify: `src/gui/MainWindow.{h,cpp}` (the MOX lambda at `:5185-5620` becomes a call into the
  controller; `dispatchFftFrameToPans`' suppression reads the controller's pan),
  `src/gui/RemoteMediaController.{h,cpp}` (the transmit frames and context to the remote
  source; the receive frames of the transmitting pan held while keyed),
  `src/gui/SpectrumWidget.{h,cpp}` (only what the remote source needs to feed the existing
  `updateSpectrumFromTxPixels` and `pushTxWaterfallRow`)
- Test: `tests/tst_mox_display_controller.cpp` (new), `tst_spectrum_widget_mox_overlay`,
  `tst_tx_display_window`, `tst_remote_spectrum_render`

**Interfaces:**
- Produces `class ITxDisplaySource` with `virtual void beginTransmitView(SpectrumWidget* pan,
  double carrierHz) = 0`, `virtual void requestView(double centreHz, double spanHz, int
  pixels) = 0`, `virtual void endTransmitView(SpectrumWidget* pan) = 0`,
  `virtual bool available() const = 0`; `class LocalTxDisplaySource` (a viewer of
  `RadioModel::txDisplayFeed()`, `local` true, its trace and waterfall into the pan) and
  `class RemoteTxDisplaySource` (the pan's endpoint in `RemoteMediaController`: re-subscribes
  with the view and the transmit window, draws the transmit frames; `available()` is the
  Core's `txDisplayVersion` of at least 1).
- Produces `class MoxDisplayController : public QObject`:
  - `MoxDisplayController(PanadapterStack* pans, RadioModel* model, QObject* parent)`,
    `void setSource(ITxDisplaySource* source)`.
  - `void setKeyed(bool keyed, int txSliceId)`: the rise and fall. A local window calls it
    from `MoxController::moxStateChanged` with `txBoundSlice()`; a remote window from the
    mirrored `txState`'s `keyed` and `txSliceId` (`TransmitState::stateChanged`), or, on a
    Core that sends no `txState` to it, from `radio.transmitting` and the slice whose
    `txSlice` is true.
  - `QString transmitPanId() const` (empty unless keyed), `void carrierChanged(double
    carrierHz)` (row 16's live path, from the feed's `viewChanged` locally and the transmit
    context remotely).
  - On the rise, on the pan hosting the transmit slice (never the active pan, never a
    fallback): `setMoxOverlay(true)` (red border, TX grid, TX palette and TX waterfall levels),
    the saved receive rate and DDC centre, the view moved to the carrier, Clarity paused,
    `setTxExternalWaterfall(true)`, waterfall AGC reset and averaging cleared, then
    `source->beginTransmitView`; the view-window signal (`txViewWindowChanged`) goes to
    `source->requestView`. The fall undoes each of them, on the pan recorded at the rise.
  - The high-SWR border on the transmitting pan follows `txState`'s `highSwr` and
    `swrWindBackLatched` in a remote window, as `RadioModel` drives it locally.
- User-visible string: "This Core does not send its transmit display. Updating the Core may
  help." (the transmitting pan's status line in a remote window on a Core below
  `txDisplayVersion` 1).

**Acceptance:**
- **The local window unchanged:** every existing TX display test passes, and a new test
  drives `setKeyed` in a local window and checks the same widget calls, in the same order, as
  the old lambda made (a recorded list), on the pan hosting the transmit slice.
- **Remote rise:** in a remote window on a Core at `txDisplayVersion` 1, `txState.keyed`
  going true for the slice on pan 2 puts the red border, TX grid, TX palette and TX waterfall
  levels (-70 to 30 by default) on pan 2 only, and pan 2 draws the transmit frames at the
  context's centre and span; pan 1 keeps drawing its receive frames.
- **Remote fall:** `keyed` false restores pan 2's receive grid, palette, levels, rate and
  centre exactly (the values before the rise), and receive frames draw again.
- **Older Core:** on a Core below version 1 the transmitting pan still takes the red border
  and TX grid, draws no receive frame while keyed (its trace and waterfall hold), and shows
  "This Core does not send its transmit display. Updating the Core may help."; the full-width
  band of receiver leakage never reaches the waterfall.
- **Row 16 (XIT while keyed), both windows:** changing XIT or its offset while keyed moves
  the trace, the TX filter overlay and the view to the new carrier within one frame period
  (locally from the feed, remotely from the renewed context); un-keying restores as before.
- **Tuning while keyed:** a click, wheel or spot click on the transmitting pan while keyed does
  in a remote window what it does in a local window while keyed (one test drives both).
- **High SWR:** `txState.highSwr` true with `swrWindBackLatched` true shows the high-SWR border
  with fold-back on the transmitting pan of a remote window, as a local window shows it.
- **Layout change while keyed (row 14):** changing the pan layout while keyed, then un-keying,
  leaves no pan showing transmit, in either window.

**Verification:** tests first for the recorded local sequence (it must match before the lambda
moves), then the remote cases. Build `NereusSDR` and the named tests; run by exact name.
Bench (pending): matrix rows 19, 20, 21 and 22.

**Execution note (advisory):** opus. After Task 28. Touches `MainWindow.cpp`.

- [ ] **Step 1:** The recorded-sequence test, the controller and the local source; the lambda
  moves.
- [ ] **Step 2:** The remote source, the older-Core case, row 16 in both windows, high SWR,
  commit.

## Task 30: Setup > Display > TX Display from a remote window

**Requirements:** A12 (Setup > Display > TX Display); R-R3-49; R-R3-21; R-R3-10 (a local
window keeps its own analyzer). Thetis: the TX Display controls set the transmit display's
analyzer at once (`setup.cs:18149-18210 [v2.10.3.15]`), keyed or not; the matrix's row 6
checks that live.

**Files:**
- Modify: `src/gui/setup/DisplaySetupPages.{h,cpp}` (`TxDisplayPage`: in a remote window its
  nine analyzer controls write the Core's keys and show the Core's values),
  `src/gui/SetupDialog.cpp`, `src/core/session/StationServer.cpp` (the nine keys accepted from a
  window on a receive-only Core, `txDisplayVersion` 2), `src/models/RadioModel.{h,cpp}`
  (`applyRemoteTxDisplaySetting`), `src/core/TxAnalyzer.{h,cpp}` (`reloadSetting`)
- Modify: the link document (section 6.3's `txDisplayVersion` note; section 8's settings
  sentence for the nine keys), `surface.json` (regen), fixtures
- Test: `tests/tst_remote_tx_display_settings.cpp` (new), `tst_tx_analyzer_settings`

**Interfaces:**
- The nine Station-scoped keys (`SettingsScope.cpp:469-477`): `DisplayTxFftSize`,
  `DisplayTxWindowType`, `DisplayTxPanDetector`, `DisplayTxPanAveraging`,
  `DisplayTxPanAvTimeMs`, `DisplayTxPanNormalize`, `DisplayTxWfDetector`,
  `DisplayTxWfAveraging`, `DisplayTxWfAvTimeMs`.
- Produces `void RadioModel::applyRemoteTxDisplaySetting(const QString& key)` and
  `void TxAnalyzer::reloadSetting(const QString& key)`: the Core, on a window's
  `settings.write` of one of the nine, applies it to its analyzer through the setter the local
  page calls (`setFftSize`, `setWindowType` and the rest), at once, keyed or not.
- Produces `txDisplayVersion` 2 (a revision: the Core applies the nine live). The
  on-the-air rule does not apply to these nine writes (Q4 below, its recommendation taken):
  they change only the Core's display analyzer, never the radio, and a local window changes
  them mid-transmission (row 6).
- The page's other groups (the TX waterfall levels, palette, low colour and gradient, the TX
  grid) stay this window's own: Window-scoped, drawn by this window.
- User-visible string: "This Core does not apply transmit display settings from this app.
  Updating the Core may help." (the nine controls on a Core below version 2).

**Acceptance:**
- **Remote:** each of the nine controls in a remote window shows the Core's value, and a
  change reaches the Core's `TxAnalyzer` at once (a test reads the analyzer's own getter and
  `analyzerConfigCount()`, not the settings file), on a receive-only Core too; a second window
  sees the new value.
- **On the air:** a change while the Core is keyed is applied (the Window change of row 6
  works remotely), and nothing reaches the radio.
- **Older Core:** below version 2 the nine are disabled with the reason above; the Window-scoped
  groups still work.
- **Local:** a local window drives its own analyzer exactly as today; a desktop window hosting
  a Core shows a remote window's change on its own transmit display (one analyzer).
- The bin-width readout uses the Core's FFT size and 96000 Hz.

**Verification:** tests first for the live apply and the older-Core case. Named tests,
`tst_link_conformance_session`, `tst_link_surface_manifest`, wording. Bench (pending): matrix
row 23.

**Execution note (advisory):** opus. After Task 29. Shares `StationServer.cpp` and
`RadioModel.cpp`.

- [ ] **Step 1:** Tests, `reloadSetting`, the Core's live apply, the version, the page,
  documents, fixtures.

## Task 31: DUP (display duplex) in both windows

**Requirements:** A11; R-R3-49 (parity both ways). Thetis v2.10.3.15:
- `console.cs:15390-15395` (`_display_duplex`, false by default); `console.cs:37555-37575`
  (`chkRX2SR_CheckedChanged`, "chkRX2SR is the DUPlex button"), saved with the console's
  check boxes (`console.cs:3278-3288`, `addControlState`); the container's `DUP` button
  (`ucOtherButtonsOptionsGrid.cs:540`, "Duplex mode, view the tx rx").
- `console.cs:24281-24338` (DisplayThread): with DUP on, the receiver stays on the display
  while keyed.
- While keyed with DUP on: the transmit grid (`display.cs:1782-1790`, `localMox` does not read
  DUP) and the transmit waterfall levels, colours and low colour (`display.cs:6420-6427`) still
  apply; the TX filter is drawn against the receive span (`display.cs:4564-4594`,
  `getFilterXPositions`); the receive trace's calibration adds the receive calibration and the
  transmit attenuator offset to the transmit calibration (`display.cs:4820-4850`, `RX1Offset`,
  with its tags `//[2.10.1.0] MW0LGE fix issue #137` and `//[2.10.3.6]MW0LGE att_fix // change fixes #482`, kept verbatim in the port); XIT does not
  move the display (`console.cs:22144`); noise blanking is turned off while keyed and restored
  after (`console.cs:29179`, `:29213`); changing DUP while keyed resets blob maxima and spectrum
  peaks (`display.cs:514-521`); the IMD overlay needs DUP (`display.cs` `_show_imd_measurements
  && displayduplex`, already at `SpectrumWidget.cpp:3945`).
- DUP acts on the first receiver only (`display.cs:4619-4629`, `isRxDuplex`): here, the pan
  hosting the transmit slice.

**Files:**
- Modify: `src/gui/MoxDisplayController.{h,cpp}` (with DUP on, no transmit view: the overlay
  only), `src/gui/SpectrumWidget.{h,cpp}` (`setDisplayDuplex` default false; the filter span and
  calibration rules above), `src/gui/MainWindow.{h,cpp}` (the View menu item, the setting),
  `src/gui/meters/OtherButtonItem.cpp` and `src/gui/containers/ContainerButtonDispatcher.cpp`
  (the DUP button), `src/gui/UnbuiltFeatures.{h,cpp}` (the DUP button leaves `Fdx`; the status
  bar's FDX, which names full duplex, stays hidden as unbuilt), `src/models/RadioModel.{h,cpp}`
  (noise blanking off while keyed with DUP on, local and on the Core)
- Modify: `src/core/TxDisplayFeed.cpp`, `src/core/session/media/DaemonMediaController.cpp`
  (the `duplex` field), `src/core/session/StationServer.cpp` (`txDisplayVersion` 3),
  `src/gui/RemoteMediaController.cpp`
- Modify: the media control document's "Transmit display" section, the link document,
  `surface.json` (regen), vectors and fixtures
- Test: `tests/tst_display_duplex.cpp` (new), `tst_mox_display_controller`,
  `tst_remote_tx_display`, `tst_unbuilt_features`, `tst_spectrum_widget_mox_overlay`

**Interfaces:**
- Produces the Window-scoped setting `DisplayDuplex` ("True"/"False", default "False" as
  Thetis), `bool MoxDisplayController::displayDuplex() const` and
  `void setDisplayDuplex(bool on)`; `void displayDuplexChanged(bool on)`.
- Controls: the container `DUP` button (built), and View > "Show receiver while transmitting
  (DUP)", checkable; both follow the one setting.
- Produces `txDisplayVersion` 3: `subscribe` may carry `duplex` (boolean, absent false). While
  keyed, an endpoint with `duplex` true is not a viewer of the feed and keeps receive frames;
  its contexts say `transmit` false.
- With DUP on while keyed, noise blanking on the transmit slice is off and restored at the
  fall, on the Core for a remote holder, as `console.cs:29179` and `:29213` do.
- User-visible string: "This Core does not show the receiver while transmitting for this
  app. Updating the Core may help." (DUP's controls in a remote window below version 3,
  disabled).

**Acceptance:**
- **DUP off (default):** the transmitting pan shows the transmit display, as after Task 29.
- **DUP on, local and remote:** keyed, the transmitting pan keeps the receiver's trace and
  waterfall, with the red border, the transmit grid and the transmit waterfall levels; the TX
  filter overlay sits against the receive span; XIT does not move the view; the IMD overlay
  shows during two-tone only with DUP on.
- **Calibration:** with DUP on and keyed, the receive trace is offset by the transmit
  calibration plus the receive calibration plus the transmit attenuator offset (a test with
  known offsets).
- **Noise blanking:** NB on before the key is off while keyed with DUP on and back on after;
  with DUP off it is untouched.
- **Mid-key change:** toggling DUP while keyed switches the pan between the two views within
  one frame period and resets blob maxima and spectrum peaks.
- **Older Core:** below version 3, DUP's controls in a remote window are disabled with the
  reason above and the pan behaves as DUP off.
- The FDX status-bar label stays hidden (`tst_unbuilt_features`); the DUP button is visible
  and works.
- The setting survives a restart of the window.

**Verification:** tests first for the calibration and noise blanking rules. Named tests, the
media runner, surface, wording. Bench (pending): matrix row 24.

**Execution note (advisory):** opus. After Task 30. Shares all three big files.

- [ ] **Step 1:** The setting, the controller branch, the widget rules, the button and menu,
  tests.
- [ ] **Step 2:** The `duplex` field, the version, NB on the Core, documents, fixtures.

## Task 32: The transmit monitor to the device that holds transmit

**Requirements:** R-IOS-13 (the iPhone plan's Task 36: with MON on, the audio sent to the
remote device carries the transmit monitor while keyed); R-R3-49; parity both ways (MON works
from any window as it does locally). The iPhone plan's Part F lists it as not yet on the link.
Thetis: `console.cs:29040-29066 [v2.10.3.15]` (`chkMON_CheckedChanged`) and
`audio.cs:407-424 [v2.10.3.15]` (`Audio.MON`: `SetAAudioMixWhat` for the transmitter's stream,
at `SetAAudioMixVol` 0.5).

**Files:**
- Modify: `src/core/AudioEngine.{h,cpp}` (the TX monitor block, as `txMonitorBlockReady`
  scales it, offered to the Core's media sender), `src/core/session/media/DaemonAudioSender.{h,cpp}`
  and `src/core/session/media/DaemonMediaController.{h,cpp}` (the holder's chosen stream),
  `src/core/session/StationServer.cpp` (`txMonitorAudioVersion` 1)
- Modify: `src/gui/RemoteMediaController.{h,cpp}` (declares it, sends `monitor-audio`),
  `src/gui/MainWindow.cpp` (the MON output choice pushed on change and at start),
  `src/gui/applets/TxApplet.cpp` (MON SPEAKERS/PHONES in a remote window)
- Modify: the media control document (a "Transmit monitor (monitor-audio)" section), the link
  document (section 6.3), `surface.json` (regen), vectors and fixtures
- Test: `tests/tst_remote_tx_monitor.cpp` (new), `tst_remote_audio_session`,
  `tst_link_conformance_media`

**Interfaces:**
- Produces capability `txMonitorAudioVersion` 1 (appended after the last minor-11 entry; 0
  without media). A window that sees it may add `txMonitorAudioVersion` to its media `start`.
- Produces GUI-to-Core media operation `monitor-audio` with exactly `op`, `connectionId`,
  `revision` (nonzero uint32, rising) and `route` (`speakers`, `headphones` or `none`),
  answered by one `monitor-audio-context` with `op`, `connectionId`, `revision` and `route` as
  applied.
- The Core: while its radio is keyed, `monEnabled` is true and this device holds transmit
  (`txState`'s `holderDeviceId` is this session's device), it adds the TX monitor, at
  `monitorVolume`, to this device's main stream (`speakers`) or its headphones stream
  (`headphones`, when it declared `headphonesMixVersion`, else its main stream); `none` sends
  it nowhere. No other device's stream carries it. While a remote device holds transmit the
  Core's own speakers and headphones leave MON out, so it plays only on that device; while the
  station device holds it (a window hosting the Core) the Core's outputs play it as today (the
  operator's MON ruling of 2026-09-26).
- The desktop sends its MON output choice (`audio/TxMonitor/Output`) as `route`; the phone's
  rule (MON in headphones only) is its own (see "Plan text to change").
- User-visible string: "This Core does not send the transmit monitor. Updating the Core may
  help." (MON SPEAKERS/PHONES in a remote window below version 1; MON itself still turns on the
  Core's monitor tap, Task 2, so the holder's stream carries it, but the Core's own output of it
  is quiet while the holder is a remote device, by the operator's MON ruling).

**Acceptance:**
- With MON on and a test tone through the Core's TX chain, the keyed holder's main stream
  carries it at `monitorVolume` within 1 dB; with `route` `headphones` it moves to the
  headphones stream; MON off, or `route` `none`, carries nothing.
- A second device on the Core, not holding transmit, receives no monitor audio.
- A peer that did not declare the capability gets today's wire (fixture).
- Un-keying ends the monitor audio within one audio frame.
- A local window's MON is unchanged, and a local window at the Core that holds transmit still
  hears it locally; with a remote holder the Core's own output of MON is silent while the
  holder's stream carries it.

**Verification:** tests first (holder only, route, levels). Named tests, the media runner,
surface, wording. Bench (pending): matrix row 25.

**Execution note (advisory):** opus. After Task 31. Shares `MainWindow.cpp` and
`StationServer.cpp`.

- [ ] **Step 1:** The Core's tap and route, the operation, tests.
- [ ] **Step 2:** The window's choice and applet, documents, fixtures.

## Task 33: The CFC bar chart and PA Values' transmit readings in a remote window

**Requirements:** R-R3-49; R-R3-32 (a reading names its source); R-IOS-13 (the keyed view);
the iPhone plan's Part F, which lists both as not yet on the link. Thetis:
`frmCFCConfig.cs:393-447 [v2.10.3.15]` (`timerTick`: `GetTXACFCOMPDisplayCompression` every
50 ms while the form is visible, `binsPerHz` over 48000 Hz). The PA readings and their scaling
are the ones `PaValuesPage` already uses (`PaTelemetryScaling`, citing Thetis
`console.cs` `computeAlexFwdPower`); the Core sends the raw values, never a new formula.

**Files:**
- Modify: `src/core/session/TransmitStateFacade.{h,cpp}` and `src/core/session/MirrorPolicy.cpp`
  (`forwardAdcRaw`, `reflectedAdcRaw`), `src/core/session/StationServer.{h,cpp}`
  (`txReadingsVersion` 1; the `txCfcCompression` record stream in `setUpRecordStreams`),
  `src/models/RadioModel.cpp` (the Core fills them from its `RadioConnection::paTelemetryUpdated`
  and `TxChannel::getCfcDisplayCompression`)
- Modify: `src/gui/applets/TxCfcDialog.{h,cpp}` (in a remote window the bar chart reads the
  stream while open), `src/gui/setup/PaSetupPages.cpp` (`PaValuesPage` in a remote window)
- Modify: `src/gui/applets/TxApplet.cpp` (the RF Pwr and SWR bars in a remote window),
  `src/gui/meters/MeterPoller.{h,cpp}` (the container meters' transmit bindings in a remote
  window), and the remote window's feed of `RadioStatus::powerChanged` from `txState`
- Modify: the link document (section 6.3, section 7.7's stream table, section 18.8's
  `txState`), `surface.json` (regen), fixtures
- Test: `tests/tst_remote_tx_readings.cpp` (new), `tst_remote_pa_pages`, `tst_pa_values_page`

**Interfaces:**
- Produces capability `txReadingsVersion` 1, sent right after `txStateVersion` and only with
  it.
- Produces on `txState`, Outbound, under version 1: `forwardAdcRaw`, `reflectedAdcRaw` (i64,
  the radio's raw forward and reflected power readings, refreshed with the other `txState`
  meters, keyed or not).
- Produces the record stream `txCfcCompression` (capacity 1, one record, `id` "0"):
  `atMs` (number) and `binsDbTenths` (string: the 1025 values of
  `TxChannel::kCfcDisplayBinCount`, each rounded to a tenth of a dB, as little-endian int16
  in base64). The Core reads `getCfcDisplayCompression` every 50 ms (Thetis's interval) only
  while at least one peer subscribes and its radio is keyed with CFC on, and publishes a new
  record when WDSP says new data is ready.
- In a remote window, PA Values' forward (raw) power, forward and reflected RF voltage and the
  forward and reflected ADC readings come from `forwardAdcRaw` and `reflectedAdcRaw` through
  the same scaling the local page uses, with the Core's `hpsdrModel`; forward power,
  reflected power and SWR from `txState`; ADC overload from the mirrored step attenuator's
  `overloadAdc0` and `overloadAdc1`. Each says it is the Core's (R-R3-32).
- **A remote window's transmit meters read the Core while keyed** (found on the operator's G2
  bench on 2026-09-26: the Core's G2 read about 4 W forward on TUNE while the remote window's
  RF Pwr and SWR bars stayed at zero). The TX applet's RF Pwr and SWR bars, and every
  container meter bound to a transmit reading (forward power, reflected power, SWR, ALC,
  compression, mic), take their values from `txState` (`forwardPowerWatts`,
  `reflectedPowerWatts`, `swr`, `alcDb`, `micLevelDb`), or from the record or property that
  carries each reading `txState` does not, with the same scaling, smoothing and peak handling
  as a local window. A reading no property carries yet stays disabled with its reason, never
  0 (the not-sent list below).
- User-visible string: "This Core does not send this reading. Updating the Core may help."
  (below version 1, replacing "The Core sends this reading when this window can transmit.").

**Acceptance:**
- With the Core keyed and CFC on, the remote CFC dialog's bar chart draws the same bars as
  the Core's own values over the same frequency range (a test compares against the Core's
  `getCfcDisplayCompression` output); closing the dialog unsubscribes, and the Core stops
  reading.
- PA Values in a remote window shows the Core's readings, with the local page's peak and
  minimum tracking, and each value matches the local page's scaling of the same raw inputs
  (one test feeds both).
- Below version 1, each of those readings shows unavailable with the reason above; never 0.
- Keyed with a fake radio reporting forward power, the remote TX applet's RF Pwr bar and a
  container Power meter show the same value a local window shows for the same reading (one
  test feeds both); the SWR bar and a container SWR meter likewise. Unkeyed, they fall as a
  local window's do.
- The container meter bindings Task 39 names (`remoteTxBindingsNotSent`) that no property
  carries yet stay disabled with their reason; the rest of A9 stays with the iPhone plan's
  Task 39.

**Verification:** tests first for the stream and the scaling match. Named tests,
`tst_link_conformance_session`, surface, wording. Bench (pending): matrix rows 26 and 28.

**Execution note (advisory):** opus. After Task 32. Shares `StationServer.cpp` and
`RadioModel.cpp`.

- [ ] **Step 1:** Tests, the properties, the stream, the version.
- [ ] **Step 2:** The dialog and the page, documents, fixtures.

## Task 34: Bench the transmit display and keyed view, then land them

**Requirements:** A11, A12; R-R3-49; the verification matrix
(`docs/architecture/tx-display-verification/README.md`, rows 9 to 28). The trunk's transmit
display (PR #317, merged into the trunk 2026-09-20) is not on `main`; it lands with this
work, once benched.

**Files:** `docs/architecture/tx-display-verification/README.md` (statuses, the operator's
dates and notes), `docs/architecture/2026-09-20-remote-daemon-r3-verification/remote-controls.md`
(the rows these tasks changed).

**Acceptance:**
- The controller deploys the Core to the Pi 4 (the HL2) and the Rock (the G2) and relaunches
  the window, and the operator runs rows 9 to 18 in a local window and rows 19 to 28 in a
  remote window, on both radios where the row names no radio class. Rows 11 (ORION class) and
  13 (HERMES class with PureSignal) run on the radio class they name, or stay pending with that
  reason.
- Each row's status is his: PASS with the date, or FAIL with his words, which become a fix
  task in this plan before landing.
- When every row he can run has passed, the controller drafts the pull request that takes the
  transmit display to `main` and waits for his go before posting it.

**Verification:** the operator's bench; nothing here is verified until he says so.

**Execution note (advisory):** the controller and the operator. Last.

- [ ] **Step 1:** Deploy, relaunch, hand the operator the rows.
- [ ] **Step 2:** Record his results; fix tasks for any FAIL; the landing pull request draft.

---

## Row coverage

Every B row and its home. A row split across tasks names each part.

| Row | Task(s) |
| --- | --- |
| B1.1, B1.2 | Done in `bfab2b9e` (with `d553327f`, `50c7c319`) |
| B1.3, B1.4, B1.5 | 8 |
| B1.6 | 9 |
| B1.7, B1.8, B1.9 | 10 |
| B1.10 | 8 (Tuner Genius row), 9 (Power Genius row) |
| B1.11 | 8 (Peripherals TGXL), 9 (Peripherals PGXL), 10 (RF-Kit) |
| B1.12 | 8 (TGXL), 9 (PGXL), 10 (RF-Kit, with counters) |
| B2.1, B2.3, B2.6 | 11 |
| B2.2 | 12 |
| B2.4 | 16 |
| B2.5 | 15 |
| B3.1, B3.5 | 18 |
| B3.2, B3.3, B3.7, B3.9, B3.10 | 17 |
| B3.4, B3.6, B3.8 | 16 |
| B3.11 | 15 |
| B4.1 | 12 |
| B4.2, B4.3 | 13 |
| B4.4, B4.5 | 14 |
| B5.1 | 1 (RF Power, TX filter), 2 (tune power, VOX level and delay, MON, monitor level, MON output, LEV, EQ, CFC), 3 (profile), 4 (EQ and CFC editors) |
| B5.2 | 2 (mic level, PROC, AM carrier, DEXP), 3 (mic profile) |
| B5.3, B5.7, B5.8, B5.18 | 3 |
| B5.4, B5.9, B5.11, B5.13 | 4 |
| B5.5, B5.10 | 1 |
| B5.6 | 2 |
| B5.12, B5.14, B5.16 | 5 |
| B5.15 | 6 |
| B5.17 | 7 |
| B6.1 | Question Q1 |
| B6.2, B6.3 | 21 |
| B6.4, B6.6 | 22 |
| B6.5 | 6 |
| B6.7, B6.8, B6.9, B6.10 | 24 |
| B7.1, B7.2 | 19 |
| B7.3, B7.4 | 20 |
| B8.1, B8.2, B8.3 | 23 |
| B8.4 | 23 (raw I/Q from the Core; Q2 decided) |

In passing: the Advanced navigation targets (8); the Pan Layout and +PAN limit, Grid Lines,
the Display flyout and Clarity badge per pan, spot left-click (18); PA Voltage, the HW Volts,
Amps and Temperature meters (6); Reset to defaults and Forget this radio, Re-validate, Uptime,
Send IQ to VAX (24); the PA trip and TX Inhibit badges, PBSNR, `FilterDisplayItem`,
`ClickBoxItem`, the container signals, the Devices card items, Waterfall Low Level Color,
Multimeter Averaging window, General > Options' four settings, TX Grid Scale (25); RADE
end-of-over decodes (question Q3).

The transmit display and keyed view: row 15's skirt (27); the Core's transmit display, A11
(28, 29); Setup > Display > TX Display, A12 (30); display duplex (31); the transmit monitor
(32); the CFC bar chart and PA Values' transmit readings (33); matrix rows 9 to 28 and the
landing on `main` (34); a remote window's TCI raw I/Q (23).

## The A rows

Each stays with remote transmit; the task that covers it is in the iPhone plan
(`codex/lane-b`, `docs/architecture/2026-09-23-iphone-app-plan.md`), Part F and Part G,
except A11 and A12, which show what the Core does while on the air and are built here.

| Row | Covered by |
| --- | --- |
| A1 MOX | Task 35 (`tx.key`, `tx.unkey`) on Tasks 33 and 34, with Tasks 37 and 38 |
| A2 TUNE | Task 35 (`tx.tune`) |
| A3 two-tone (TX applet, PsForm, PureSignal applet) | Task 35 (`tx.twoTone`); `ps3.twoTone` by ruling 7.7, taking transmit first (Task 77) |
| A4 VOX arming | Tasks 35, 36 and 40 (`voxEnabled`); ruling 8.4 |
| A5 container TUN, MOX, 2TON | Task 35 |
| A6 Tuner Genius TUNE | Task 42 (`tuner.tune`) with Task 35 |
| A7 TGXL auto-recall on band or antenna change | Task 42, once its text names it (see below); Task 34 lifts the receive-only block |
| A8 PA auto-calibrate sweep | Task 35 (`tx.tune`), once its text names it (see below) |
| A9 transmit readouts | Task 39, once its text adds the compression and ALC gain readings and the Radio Status page (see below) |
| A10 AM Mod Monitor applet | Task 39, once its text names it (see below) |
| A11 the panadapter while the Core transmits | This plan: Tasks 27, 28, 29 and 31, benched in Task 34 |
| A12 Setup > Display > TX Display | This plan: Task 30 |
| A13 TCI transmit from apps | Tasks 35, 36 and 39; D58 |

## Plan text to change

For the controller, in the plans named; nothing here is a task of this plan.

- **iPhone plan Task 42** (`:3850-3913` on `codex/lane-b`): its tuner operate, bypass and
  antenna verbs exist as `setTgxlOperate`, `setTgxlBypass`, `setTgxlAntenna` (`bfab2b9e`); its
  amp and RF-Kit verbs are built by this plan's Tasks 9 and 10 as `setPgxlOperate`,
  `setRfKitOperate`, `setRfKitAntenna`. Task 42 keeps `tuner.tune` (A6), the TGXL tune-memory
  recall on a band or antenna change (A7, add it) and the PGXL standby around a TGXL tune.
  Its acceptance "A session without transmit permission gets every verb refused" becomes
  "every verb waits while the holder is on the air (D60) and follows ruling 7.8"; its
  MainWindow item (the Power Genius buttons through the Core) is done by Task 9. The
  several-devices design's table in section 7.1 names the built commands instead of
  `amp.operate`, `tuner.operate`, `tuner.bypass`, `tuner.antenna`, `rfkit.antenna`, and its
  13.1 row for Task 42 says the same.
- **iPhone plan Task 40** (`:3726-3768`): its controls are built by this plan's Tasks 1 to 5
  under the setter-matching names (`tunePower`, `monEnabled`, `cpdrOn`, `cpdrLevelDb`,
  `txLevelerOn`, `txEqEnabled`, `txEqBandsJson`, `cfcEnabled`, `micGainDb`, `filterLow`,
  `filterHigh`, `activeTxProfile`, `txProfilesJson`, `txProfile.select`) in place of
  `tunePowerWatts`, `levelerEnabled`, `eqEnabled`, `eqBandGainsDb`, `cpdrEnabled`,
  `txFilterLowHz`, `txFilterHighHz`; `voxEnabled` and `micMuted` remain its own. What is left
  for Task 40: moving those setters onto Task 32's transmit lane, and the holder rule of
  ruling 7.7 (another device refused while transmit is held), with Task 34. It no longer
  requires Task 32 to make them work. The several-devices 13.1 row for Task 40 is unchanged
  in substance.
- **iPhone plan Task 21** (`:2258-2321`): its station half is built by this plan's Task 19,
  with the names Task 21 gave; Task 21 keeps its phone consumer only.
- **iPhone plan Task 22** (`:2323-2361`): built by this plan's Task 20; it adds
  `stationFreedvVersion` 1, which Task 22 did not name.
- **iPhone plan Task 25** (`:2439-2534`): `stationRadios`, `station.selectRadio`,
  `station.rescanRadios` and This Core's Change radio (Task 21 here, with
  `stationRadiosVersion` 1, `station.setRadioModel` and `station.forgetRadio`),
  `support.collect` (Task 22 here, `supportBundleVersion` 1, with `coreLog` and
  `support.setLogCategories`) and `tciClients` (Task 23 here, under `stationTciVersion` 2)
  are built here. Task 25 keeps the tools' availability, the `vax` object, the devices list
  and Add a device, the choice order's announcement and TXT field, and the confirm step once
  Task 75 lands.
- **iPhone plan Task 20** (`:2199-2256`): the desktop's half of B3.2 and B3.10 is fixed here
  in the window (Task 17) from the granted size and rate. Task 20's display extras are the
  phone's, with one exception (JJ's ruling of 2026-09-28): a remote window's waterfall AGC
  and NF-AGC ask the Core for their levels (`waterfallLevels` mode `agc` or
  `noiseFloorAgc`, displayExtrasVersion 1) and colour against the levels the NSDX datagram
  carries, as the phone does, instead of running the follower on rows the Core has already
  clamped to the dBm window; the window then holds the pan and the stored levels, so AGC
  settling asks the Core nothing. The desktop asks for no other extras (R-R3-12 keeps its own
  smoothing; Clarity keeps the Core's `noise-floor` operation).
- **iPhone plan Task 35:** its `tx.tune` names the PA auto-calibrate sweep (A8) as a consumer.
- **iPhone plan Task 39:** `txState` gains the compression and ALC gain readings (A9), and
  its desktop half names Setup > Diagnostics > Radio Status's Forward, Reflected, SWR, PTT
  source and Mode (A9) and the AM Mod Monitor applet (A10). The remote panadapter's transmit
  view (A11) and Setup > Display > TX Display (A12) are built by this plan's Tasks 27 to 31
  instead; Task 39 and Task 78 drop them. `txState` gains `highSwr` and `swrWindBackLatched`
  (Task 28), `forwardAdcRaw` and `reflectedAdcRaw` (Task 33), which its section 18.8 table
  should list.
- **iPhone plan Part F's "Part F gains" list** (lane B, after Task 40): its four items now have
  owners here. 1, a remote window's TCI raw I/Q: Task 23 (`remoteIqVersion` 1). 2, the TX
  monitor to a remote holder: Task 32 (`txMonitorAudioVersion` 1, `monitor-audio`). 3, a remote
  pan showing the transmit spectrum while keyed: Tasks 28 and 29 (`txDisplayVersion`). 4, the
  CFC bar chart and PA Values' transmit readings: Task 33 (`txReadingsVersion` 1,
  `txCfcCompression`). The list should say so.
- **iPhone plan Task 54 (the keyed view):** the phone may declare `txDisplayVersion` at its
  media `start` and then draws the transmit frames on its pan while the Core is keyed, with its
  own orange TX filter; without declaring it keeps today's receive frames (Task 28's older-peer
  rule). It may send `duplex` (version 3) if it offers the choice.
- **iPhone plan Task 36 (the monitor):** its sentence "with MON on, the station's audio sent to
  the remote device carries the transmit monitor while keyed" is built by this plan's Task 32.
  The phone declares `txMonitorAudioVersion` and sends `monitor-audio` with `route`
  `headphones` while its output is headphones and `none` otherwise (its rule: MON in
  headphones only).
- **R-R3-42** (the R3 plan's requirement table): "transmit and raw I/Q are refused in plain
  words until remote transmit" becomes "transmit is refused in plain words until remote
  transmit; raw I/Q comes from the Core, charged to the window's display share (the operator's
  decision of 2026-09-24; this plan's Task 23)".
- **The accessory control document** ("What waits for remote transmit" and "Switching the
  Tuner Genius"): Tasks 8 to 10 rewrite them as they land; the Core-owned accessories plan
  (`2026-09-23-r3-core-owned-accessories-plan.md:60-62`) and the document's own
  `:955-980` (the sweep's cite) follow.
- **R-R3-46's last sentence** ("Transmit-side hardware follows the transmit permission until
  remote transmit") is narrowed by the ruling of 2026-09-24; the R3 plan's requirement table
  should say "waits only while the radio is on the air".
- **The R3 control matrix:** Task 26.

## Questions for the operator

The controller asks them one at a time. Each has options, a recommendation and why.

**Q1 (B6.1). File > Profiles > Import / Export and Diagnostics > Export All Settings / Import
All Settings in a remote window: what should they carry?**
- (a) Export both computers' settings into one file; Import restores this computer's part at
  once and sends the Core's part, which the Core saves and applies by restarting its radio
  connection (refused while on the air, and while another device is connected once several
  devices land).
- (b) Export both; Import restores only this computer's part and shows the Core's part as
  "restore it on the Core".
- (c) Keep both as this computer's only, and say so on the page.
Recommendation: (a). It is the only option where the button does what its label says for a
remote window (R-R3-49), and the Core already has the retire-and-reconnect path
`station.selectRadio` uses. Caveat: an import changes shared settings for every device, so
after Task 75 it goes through the confirm step.

**Q2 (B8.4). TCI raw I/Q (`iq_start`) from a remote window.** R-R3-42 refuses it until remote
transmit, so the sweep's "should work now" meets an approved requirement.
- (a) Build it now: a new I/Q stream from the Core to the window for TCI apps, with its own
  capability and a bandwidth charge in the display budget.
- (b) Keep R-R3-42: I/Q waits for remote transmit, and Task 23 disables the IQ Stream options
  with the reason.
Recommendation: (b) for now, because the link has no raw I/Q stream and one receiver's I/Q at
192 kHz is several times today's per-window budget on the Rock and Pi 4 (R-R3-37); but it is
your rule that gaps become scope, so if you choose (a) it becomes Task 27 of this plan.
**Decided (2026-09-24):** raw I/Q joins remote transmit's scope (Part F), with its bandwidth
charged to that window's display share. Remote transmit now exists, so Task 23 builds it
(`remoteIqVersion` 1); no Task 27 is added for it.

**Q3 (passing). RADE end-of-over decodes never reach `RxDecodeModel`.** End-of-over callsigns
are not decoded on receive or sent on transmit at all today.
- (a) Build end-of-over (FreeDV's own format) now.
- (b) Hide the callsign surfaces through the unbuilt-feature list until it is built.
Recommendation: (b): nothing produces a decode to wire, so wiring the model alone changes
nothing a user sees, and building the codec is a separate port.

**C1. The VFO flag's TX badge and "Make this the TX slice" in a remote window.**
- (a) Work now among the window's own slices, off the air (the ruling of 2026-09-24), through
  a Core verb; Task 34 later makes it the holder's (ruling 8.10).
- (b) Wait for Task 34's `tx.setTxSlice`, as the several-devices design gives it to the
  transmit holder.
Recommendation: (a). Choosing the TX slice keys nothing, and your ruling today covers it;
with transmit unheld, 8.12 keeps the binding where it is, so there is no holder to conflict
with until Task 34.

**C2. Mic source (Phone/CW combo and VAX button, TX Input > Mic Source, the TX applet's
badge).**
- (a) Now: show the Core's mic source on the badge and let the window choose among the radio's
  own inputs; "PC mic" and "VAX" wait for Task 36's microphone line, disabled with that reason.
- (b) Leave all of it greyed until Task 36.
Recommendation: (a): the badge shows the truth at once and the radio's inputs key nothing;
"PC mic" in a remote window means this computer's mic, which only Task 36 carries.

**C3. The VAX applet's TX gain row.**
- (a) Enable it (it writes only this computer's `audio/TxGain`).
- (b) Keep it greyed with "VAX sends to the Core with remote transmit." until Task 36.
Recommendation: (b): enabled, it would move a gain that feeds nothing, which R-R3-49 forbids.

**C4. Radio Info > Sample rate on a multi-receiver Protocol 2 Core.**
- (a) Change every receiver, as a local window's `setSampleRateLive` does.
- (b) Keep changing only the first receiver's stream.
Recommendation: (a): it matches the local window and Thetis; once several devices land, D53
and ruling 7.1 put it through the confirm step.

**C5. Alex-1 Filters' HPF bypass on TX, HPF bypass on PureSignal, Disable 6 m LNA on TX and
the LPF band edges.** Settled by the code and R-R3-49: nothing reads these keys in a local
window either, so they are hidden in both windows as unbuilt (Task 13).

**C6. HL2 Options' TX buffer latency and PTT hang.** Settled the same way: the wire always
sends 12 and 20 locally too (`P1RadioConnection.cpp:2600-2601`); hidden as unbuilt (Task 13).

**C7. Background pans paused in a multi-pan remote layout.**
- (a) Keep pausing them until the budget fits.
- (b) Hold every visible pan at a minimum of 1 frame per second, with D54's "Sharing" chip.
Recommendation: (b): R-R3-37 says "reduce background FPS then pixels", not stop, and D54
drops frame rates first; a paused pan looks broken.

**C8. A restored pan with no Core slice.** Settled by R-R3-34 ("cannot retain an apparently
configured but empty second pan without explanation") and R-R3-24: it shows a plain reason
(Task 18).

**C9. The System tile's CPU in a remote window.**
- (a) Show the Core's CPU, labelled "Core".
- (b) Show both, this computer's and the Core's.
- (c) Keep this computer's.
Recommendation: (b): R-R3-32 asks every value to name its source, and both matter (the
Core's DSP load and this window's rendering); the Core's is already in telemetry.

**C10. Cal Offset.** Settled by R-R3-11 (the remote GUI applies no second calibration):
disabled in a remote window with a reason (Task 17).

**C11. Display Thread Priority.** Settled by R-R3-41 (the Core places its own threads):
disabled in a remote window with a reason (Task 17).

**C12. Grid & Scales dB Max and dB Min per band in a remote window.**
- (a) The Core's per-band values win: the window writes each band's values to the Core's pan
  and a band crossing uses them.
- (b) The window's per-band values win: on a band crossing the window sends its values.
Recommendation: (a): `PanadapterModel` on the Core already owns per-band grids, so one store
serves every window; it needs your bench check, since which side moves the pan decides what
you see today.

**C13. View > Performance Overlay in a remote window.**
- (a) Show the Core's radio-link drops and underruns from telemetry beside this window's own
  counters, each labelled.
- (b) Hide the counters it cannot fill in a remote window.
Recommendation: (a): the overlay is for finding drops, and in a remote window they happen on
the Core.

**C14. Tools > Test antenna switch toast and Test TX-bound re-route (developer builds).**
- (a) Hide them in remote windows.
- (b) Keep them greyed with the transmit reason.
Recommendation: (a): they fake local events the Core never had, and R-R3-49 prefers hidden to
greyed-and-inert.

**Q4 (Task 30). Setup > Display > TX Display's analyzer settings while the radio is on the air,
from a remote window.** The Global Constraints refuse every write this plan adds while the
Core is on the air.
- (a) Exempt these nine keys, as ruling M4 exempts `records.*` and `spots.*`: they change only
  the Core's display analyzer and never reach the radio, and a local window changes them
  mid-transmission (matrix row 6).
- (b) Refuse them while on the air, like every other setting.
Recommendation: (a). Parity both ways: with (b) a remote window could not do what row 6 checks
a local one does, and the only time the analyzer draws is while on the air.

**Q5 (Task 31). DUP's default and where its control lives.**
- (a) Default off, as Thetis (`console.cs:15390`); the control is the container DUP button plus
  View > "Show receiver while transmitting (DUP)".
- (b) Default on, keeping today's IMD overlay behaviour (`SpectrumWidget` starts with duplex
  true), with the same two controls.
Recommendation: (a). The transmit display is what DUP off shows, and it is the view this work
exists to deliver; Thetis starts there. Caveat: with (a) the PureSignal two-tone IMD overlay
appears only with DUP on, as in Thetis, which changes what a local window shows today; the
View menu item is new UI for your review.

## Controller rulings on the questions (2026-09-24)

The operator's standing rule, that a remote window does everything a local window does, and
his ruling of 2026-09-24, that settings that key nothing work whenever nobody transmits,
settle these. Each takes its recommendation. He may overrule any of them.

- **C1:** yes. "Make this the TX slice" works among the window's own slices while off the air.
- **C2:** yes. Mic source shows the Core's value and offers the radio's own inputs; "PC mic" and
  "VAX" follow Task 36, which carries the audio.
- **C3:** yes. VAX TX gain stays greyed with its reason until Task 36.
- **C4:** yes. A remote sample-rate change applies to every receiver, as in a local window.
- **C7:** yes. Background pans keep a floor of 1 frame per second instead of pausing.
- **C9:** yes. The System tile shows both computers' CPUs, each labelled.
- **C12:** yes. The Core's per-band dB max and min win. There is a bench row for it.
- **C13:** yes. The performance overlay shows the Core's drops, labelled.
- **C14:** yes. The developer test entries are hidden in remote windows.
- **Q1:** yes. Settings import and export in a remote window carry both computers' settings, and
  the Core applies its part through a radio reconnect.

Q2 (TCI raw I/Q) was decided by the operator on 2026-09-24 (above) and is planned in Task 23.
Q3 (RADE end-of-over callsigns), Q4 and Q5 go to the operator, one at a time; Tasks 30 and 31
are written to their recommendations, and the controller confirms each before dispatching
that task.
