# Thetis CAT Compatibility Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. When JJ selects crew, use its sequential workers, task ledger and consolidated review cadence instead of stacking execution workflows.

**Goal:** Deliver native Thetis CAT command/control compatibility and setup, with an explicit outcome for every upstream descriptor and real model effects for every advertised operation.

**Architecture:** A Qt event-loop service owned by `RadioModel` separates parsing, command families, endpoint bindings, PTT ownership and byte transports. Commands act through typed production model APIs; four logical channels use stable primary/secondary slice identities and share one transmitter. Hamlib rigctld is a separate final delivery using the same bindings and PTT policy.

**Tech Stack:** C++20, existing Qt6 Core/Network/Widgets/Test, optional Qt6 SerialPort, POSIX PTYs on macOS/Linux. No new package, driver, WDSP algorithm or remote protocol.

**Spec:** [Approved design](2026-10-04-thetis-cat-design.md), approved by JJ on 2026-10-04.

## Global Constraints

- Thetis v2.10.3.15, commit `3759d096`; use absolute upstream paths when working in a managed worktree. Never pull pinned gateware.
- Core/model code includes no GUI header. Model access occurs on the main/model thread. No new DSP thread or audio-callback mutex.
- `AppSettings`, PascalCase keys, and `"True"`/`"False"` booleans. Never persist transient PTT, registrations, meters or antenna state.
- Endpoint assignments use stable slice IDs and do not track GUI focus. Queries/connections do not create slices.
- Listener enablement defaults off; effective first Thetis TCP default `127.0.0.1:13013`; welcome default off.
- The first channel seeds primary A and secondary B only if those identities exist. Additional bindings require selection.
- All command widths, formatting, handler ranges/scaling and inline constants come from the source. No guessed enum values, hardware facts or WDSP signatures.
- Exact source headers, every upstream inline comment/tag, modification history and same-commit provenance are required for ports. Signed commits, no hook bypass.
- C++ naming, Qt ownership, platform guards and logging follow AGENTS/CONTRIBUTING. Use `lcCat` for CAT warnings.
- Read `docs/development/fast-test-loop.md`; build named test targets before `ctest`. No arbitrary sleeps or whole-suite iteration.
- The service owns no radio discovery/autoconnect policy. Starting or restoring CAT cannot key or connect a radio implicitly.

## Review Focus

1. One client disappears while another holds PTT: release only the departed claim, and never a newer non-CAT activation (Task 4).
2. A removed slice's list position is reused: the endpoint stays invalid instead of controlling the new occupant (Tasks 3/5).
3. Fragmented text/GUID commands and oversized noise: preserve payload and recover at a terminator without crossing clients (Tasks 2/8).
4. A getter seems plausible because a TCI stub cached a value: assert actual production model/audio effect or report unavailable (Tasks 5–7).
5. Serial PTT is asserted on startup or reopening: sample CTS/DSR, require release/new assertion, and never interpret opening as keying (Task 9).

## Baseline and delivery boundaries

The rebased design commit is `19a90f263` on `codex/thetis-cat-design`; implementation baseline is Core Controller candidate `27716f5d6700e1e7d1808acfe478756249828e8c`, from `codex/release-candidate-2026-10`. JJ requested this update on 2026-10-04. The earlier audit used `e342467fb`; current-tip mapping decisions supersede that historical research. Main checkout contains unrelated work; do not copy, stage or remove it. Rebase/integration belongs to the lead when this branch is ready.

The checked-in [source inventory](cat/2026-10-04-command-inventory.csv) contains all 419 descriptors and source lines; [its companion](cat/2026-10-04-command-inventory.md) records the XML fingerprint and extraction limits. It is factual input, not runtime coverage. The complete [planning mapping matrix](cat/2026-10-04-command-mapping.csv) adds a reviewed outcome, target API, scale and read/set contract for every descriptor. Task 1 copies those decisions into runtime fixtures before any handler is advertised.

Mapping evidence: [RX/VFO/DSP](cat/2026-10-04-rx-mapping-evidence.md) and [TX/global/peripherals](cat/2026-10-04-tx-mapping-evidence.md). These contain exact command lists, production signatures, source scaling and confirmed capability gaps. Current-tip addenda and the complete matrix supersede historical absence findings from the earlier checkout. Decisions below supersede alternatives still listed in the research annexes.

This baseline builds `NereusCore` and `NereusSDRLib` shared libraries plus the real headless `nereusd`; `DaemonApp` owns its local-authority RadioModel. Existing `src/core/spectrum/ISpectrumSink.h` and `tst_core_has_no_gui_includes` already enforce the model/GUI boundary. Reuse them. There is no prerequisite GUI extraction in this plan. CAT must run in the authoritative local-role model (standalone desktop or daemon), never start a second listener in a remote-role window; `ownsLocalDsp()` is the existing role gate. Remote station-wire configuration is a separately stated capability gap unless an existing supported API provides it, not a reason to invent remote shadow state.

Dedicated Andromeda/Aries/Ganymede workflows, MIDI scripts and missing recording/CWX/VAC features remain separate integrations as in the approved spec. Their descriptors are accounted for with exact unavailable or existing readback behavior. Do not claim full functional parity from the number of classified rows.

Delivery A = Tasks 1–7 (catalogue, model semantics, ownership); B = Tasks 8–12 (native transports and UI); C = Task 13 (separate rigctld dialect); final integration = Task 14. Execute dependencies in order; smaller family assignments can be serial worker tasks under the same contracts.

## File map and shared interfaces

Create `src/core/cat/` for `CatTypes`, `CatCommandCatalog`, `CatParser`, `CatCommandRouter`, `CatModelAdapter`, `CatSession`, `CatSettings`, `CatTxCoordinator`, `CatService`, `CatRxCommands`, `CatDspCommands`, `CatTxCommands`, `CatGlobalCommands`, `CatStreamFramer`, `CatTcpTransport`, `CatSerialTransport`, `CatPtyTransport`, `CatReporter` and `RigctlProtocol` (`.h/.cpp` where needed). Keep family dispatch tables separate from transport logic.

Modify `RadioModel.h/.cpp` for service lifetime and only necessary typed accessors; `MoxController.h/.cpp` only for the accepted-request ownership observation defined in Task 4; `src/core/daemon/DaemonApp.cpp` and `src/gui/GuiSessionCoordinator.cpp` and `GuiDesktopStationRuntime.cpp` for policy-ready start/stop; `LogCategories.h/.cpp` for `lcCat`; existing CAT setup/applet, `SetupDialog.h/.cpp`, `MainWindow.h/.cpp`, CMake/resources, attribution and status docs for integration. Add focused Qt tests and fixture data under `tests/data/cat/`.

Use these names/types consistently:

```cpp
enum class CatVfo { Primary, Secondary };
enum class CatForm { Get, Set };
enum class CatResultKind { Payload, Silence, Wire, Error };
enum class CatOutcome { Faithful, Adapted, Inactive, SourceInert, Unavailable };
struct CatBinding {
    int primarySliceId{-1}; std::optional<int> secondarySliceId;
    quint64 primaryIncarnation{0}; std::optional<quint64> secondaryIncarnation;
}; // Incarnations are runtime-only: capture on explicit binding/start, never persist.
struct CatRequest { QByteArray code; QByteArray suffix; CatForm form; };
struct CatCommandResult { CatResultKind kind; QByteArray data; int verboseErrorCode{0}; };
struct CatValidation { std::optional<CatRequest> request; QByteArray error; int verboseErrorCode{0}; QByteArray errorCommand; };
struct CatSessionContext { quint64 sessionId; int channel; bool verboseErrors{false}; bool transmitAllowed{true}; };
```

`CatDescriptor` carries `code`, `active`, `setWidth`, `getWidth`, `answerWidth`, suffix-validation kind, outcome and handler family. Negative widths disable a form. Where get/set lengths match, the matrix records the upstream branch precedence rather than assuming a getter.

`CatGlobalConfig` stores the exact global CAT/PTT preference fields in the settings schema (typed bool/int/QString values); serialized booleans remain strings. `CatSettings::global() const -> CatGlobalConfig` and `setGlobal(const CatGlobalConfig&) -> bool` provide validated, persisted global configuration; `CatService::globalConfig() const -> CatGlobalConfig` and `applyGlobalConfig(const CatGlobalConfig&) -> bool` apply it once and emit `globalConfigurationChanged()`. No PTT level or runtime incarnation is saved.

`CatEndpointConfig` represents a logical channel, `CatBinding` and independently enabled TCP, serial, PTY and rigctld transport configurations. `CatService` owns four channels; TCP/serial/PTY sessions for a channel resolve the same configured slices. Four rigctld slots share these bindings but have distinct listeners and protocol framing.

## Settings schema

Use `Cat/Channels/N/` (`N=1..4`) and the exact leaf names below. The service owns read/write; UI calls `applyChannelConfig` once, rather than saving and restarting independently. Unset additional ports are a Nereus adaptation requiring explicit input before enablement.

| Leaf | Default / meaning |
| --- | --- |
| `PrimarySliceId`, `SecondarySliceId` | Seed 0/1 for first channel only when present; otherwise -1. Additional channels -1/-1. -1 secondary means unavailable VFO B. |
| `TcpEnabled`, `SerialEnabled`, `PtyEnabled`, `RigctldEnabled` | `"False"`. |
| `TcpBindAddress`, `RigctldBindAddress` | `"127.0.0.1"`. |
| `TcpPort` | First channel 13013; others 0/unset. |
| `RigctldPort` | 0/unset; choose before enabling. No invented Hamlib default. |
| `SerialDevice` | Empty system device path/name. |
| `SerialBaud`, `SerialParity`, `SerialDataBits`, `SerialStopBits` | 115200, `"None"`, 8, `"1"`, from `setup.cs:351-374`. String stop/parity names are translated explicitly to Qt enums. |
| `PtyDialect` | `"Thetis"`; optional `"Rigctld"`. |

Global `Cat/` leaves: `SendWelcome=False`, `RigIdentity=TS-2000`, `AllowKenwoodAi=False`, `AiEnabled=False`, `AiSerial1=True`, `AiSerial2=False`, `AiSerial3=False`, `AiSerial4=False`, `AiTcp=True`, `DigitalReportsSideband=False`, `RecenterVfo=False`, `SerialNumber=0000-0000`, `LimitReportedPower=True`, `RttyOffsetAEnabled=False`, `RttyOffsetBEnabled=False`, `RttyDiguHz=2125`, `RttyDiglHz=2125`. Booleans use quoted string values in implementation. Sources: `setup.cs:351-385,5902-5932`, `setup.Designer.cs:59358-59534,59587-59636`, `console.cs:17707`. RTTY offset range is -3000..3000 Hz. `LimitReportedPower` uses existing `TransmitModel::powerLimit()` and `powerSliderLimitEnabled()` for the source nonmutating constrained-power readback; when enabled, constrain the actual drive to the active limit exactly as the source does. Never invoke a power setter to answer a query.

Global `Cat/Ptt/`: `Enabled=False`, `DeviceSource=None` (None, CAT1..4, Physical), `SerialDevice=""`, `UseCts=False`, `UseDsr=False`, `Channel=1` (logical channel1..4 for a separate physical device). Separate physical PTT device uses the same source-grounded serial-format defaults. Output RTS/DTR assertions are not inferred from these input flags. Optional hardware operations unavailable on a platform remain unavailable without discarding saved settings.

## Focused verification commands

Configure once in this worktree: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON`. Reconfigure after adding sources/test registrations. Build each test group before running it. For the space-separated target list in the table below, run this exact shell pattern, substituting only that row's list:

```sh
cat_test_targets=(tst_cat_catalog)
cmake --build build --target "${cat_test_targets[@]}"
cat_test_regex="^($(IFS='|'; printf '%s' "${cat_test_targets[*]}"))$"
ctest --test-dir build -R "$cat_test_regex" --output-on-failure
```

A red step must show the specified missing implementation or behavioral assertion, not a dependency/environment error. A green step requires all selected tests executed and passed, with none missing. Every new target is registered with `nereus_add_test` and uses the existing settings sandbox. Use ephemeral localhost TCP ports in tests; never bind the user's configured listener or contact a real radio.

| Task | Target list |
| --- | --- |
| 1 | `tst_cat_catalog` |
| 2 | `tst_cat_parser tst_cat_catalog` |
| 3 | `tst_cat_bindings tst_cat_settings tst_cat_headless` |
| 4 | `tst_cat_tx_ownership tst_mox_controller_ptt_source_dispatch tst_tx_slice_arbiter tst_tx_slice_binding_invariant tst_band_plan_guard_mox_rejection tst_radio_model_set_tune tst_two_tone_controller` |
| 5 | `tst_cat_rx_commands tst_tx_frequency_follows_tx_slice tst_slice_rit_xit tst_xit_offset_application` |
| 6 | `tst_cat_dsp_commands tst_slice_squelch tst_rxchannel_squelch tst_slice_agc_advanced tst_rxchannel_agc_advanced tst_slice_auto_agc tst_slice_nb_persistence` |
| 7 | `tst_cat_tx_commands tst_cat_coverage tst_cat_rx_commands tst_cat_dsp_commands` |
| 8 | `tst_cat_tcp tst_cat_reporting tst_cat_tx_ownership` |
| 9 | `tst_cat_serial tst_cat_serial_ptt tst_cat_tx_ownership` |
| 10 | `tst_cat_pty tst_cat_tcp` |
| 11 | `tst_cat_setup tst_cat_applet tst_cat_settings tst_cat_headless` |
| 12 | All introduced CAT tests plus `tst_cat_integration tst_core_has_no_gui_includes` and Task 4 regressions. |
| 13 | `tst_cat_rigctld tst_cat_bindings tst_cat_tx_ownership tst_cat_pty` |
| 14 | `cmake --build build --target NereusSDR nereusd all_tests`, then `ctest --test-dir build --output-on-failure` once. |

For Task 9/12 without SerialPort: `cmake -S . -B build-no-serial -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON -DCMAKE_DISABLE_FIND_PACKAGE_Qt6SerialPort=ON`, then build `tst_cat_serial tst_cat_settings tst_cat_headless` in that directory and run `ctest --test-dir build-no-serial -R '^(tst_cat_serial|tst_cat_settings|tst_cat_headless)$' --output-on-failure`. Verify all three execute and pass their unavailable-state contracts.

## Task 1: Catalogue and behavior contracts

**Files:** Create `resources/cat/CATStructs.xml`, `src/core/cat/CatTypes.h`, `CatCommandCatalog.h/.cpp`, `tests/data/cat/compatibility.csv`, `tests/tst_cat_catalog.cpp`; create `resources/cat.qrc` (add to `CORE_RESOURCES`, available to nereusd), `CMakeLists.txt`, `tests/CMakeLists.txt`, `src/core/LogCategories.h/.cpp`, `docs/attribution/THETIS-PROVENANCE.md`. Create `lcCat` here before any catalogue diagnostics use it.

**Interfaces:** `CatCommandCatalog::descriptors() const -> const QList<CatDescriptor>&`; `find(const QByteArray& code) const -> const CatDescriptor*`; `maximumRequestBytes() const -> qsizetype`. Catalogue owns descriptor lifetime for parser/router use.

- [ ] Add the catalogue test: `QCOMPARE(catalog.descriptors().size(), 419)`, active count 349, standard count 39, whitespace-active `ZZHW`, inactive count 70, inactive `AN`, `FA` widths 11/0/11, `ZZEA` widths 36/0/36, and unique command codes.
- [ ] Register `nereus_add_test(tst_cat_catalog)`, configure tests and run it to observe the missing catalogue failure.
- [ ] Copy the pinned XML bytes verbatim; load the embedded resource with `QXmlStreamReader`, fail service initialization on missing/invalid catalogue, log through `lcCat`. Add exact project-level source attribution and provenance in this same commit.
- [ ] Copy the planning matrix decisions into runtime compatibility CSV `Command,Active,Forms,Widths,SourceRange,Family,TargetApi,Scale,Outcome,ReadContract,SetContract,Fixture`. Materialize each enabled read/set request and its concrete preconditions now; inactive/unavailable rows and verified static/source-inert cases have explicit reply fixtures. Family-dependent positive replies stay explicitly pending until the owning Tasks5–7 supply production-model fixtures before advertising those handlers. Pending cases never count as passing execution coverage. Read the cited handler/helpers while transcribing exact fixture bytes. Do not change a mapping silently: record any newly discovered source mismatch and its correction in the report.
- [ ] Record that `ZZMX` is dispatched at `CATParser.cs:965-967` (the earlier extractor missed `case"ZZMX"`), inactive `AN`, actual inert `PR/ZZFX/ZZTS/ZZNM` behavior where confirmed, and unavailable CWX/VAC/controller/recording operations. Inert successful queries are preserved only when the source actually returns that value; absent functionality returns `?;` for both forms, without mutation.
- [ ] Derive the active request cap from descriptor fields: max suffix 36 + prefix 4 + terminator 1 = 41 bytes. Disabled negative-width forms do not imply unbounded input. Keep reply limits separate; variable-length source replies are not truncated to 41 bytes.
- [ ] Run `cmake --build build --target tst_cat_catalog`, then `ctest --test-dir build -R '^tst_cat_catalog$' --output-on-failure`; check extraction/XML fingerprint, full matrix row coverage and provenance. Commit signed: `feat: add source-grounded CAT catalogue and contracts`.

## Task 2: Parser and result formatting

**Files:** Create `CatParser.h/.cpp`, `CatCommandRouter.h/.cpp`, `tests/tst_cat_parser.cpp`; add fixtures under `tests/data/cat/`, register sources/test.

**Interfaces:** `CatParser::validate(const QByteArray&) const -> CatValidation`; `formatValidationError(const CatValidation&, const CatSessionContext&) const -> QByteArray`; `format(const CatDescriptor&, const CatRequest&, const CatCommandResult&, const CatSessionContext&) const -> QByteArray`; `CatCommandRouter::execute(const CatRequest&, CatSessionContext&) -> CatCommandResult`. Router family registration uses `registerHandler(const QByteArray&,std::function<CatCommandResult(const CatRequest&,CatSessionContext&)>) -> bool`. Families register their exact codes and cannot register an inactive descriptor. Unavailable commands use the matrix rejection contract.

- [ ] Add fixtures asserting `fa;` validates as `FA`, `FA00014074000;` as a setter, malformed width/non-numeric/inactive `AN` returns `?;`, and valid text/EQ/GUID payload case remains unchanged.
- [ ] Add formatter assertions: descriptor ID + payload `019` gives `ID019;`; accepted FA set gives empty bytes; bad fixed-width payload gives `O;`; preframed results do not receive another prefix/terminator. Assert malformed requests never call the router.
- [ ] Build/run `tst_cat_parser` and observe the missing parser failure.
- [ ] Port `CATParser.cs:119-409,410-592` and `CATParser.cs:1535-1611` result/error framing faithfully, including request-suffix echo where specified and leading-space trimming except ML/MN, including explicit suffix exceptions and get/set precedence from Task 1. Use checked numeric conversion, no exceptions. Port verbose error-code text/format from the actual ProcessError routine. ZZEM owns the calling session's `verboseErrors` flag (an explicit isolation adaptation from Thetis's shared parser); default false and not persisted. Normal source error strings are `?;`, `E;`, `O;`. Validation retains the source error code and command context in `CatValidation`; `formatValidationError` applies the calling session's verbose flag even when no request exists. `errorCommand` is the source-normalized command-plus-suffix context (or original frame for a bad command name), so verbose failures remain attributable without calling the router.
- [ ] Validate every matrix request fixture through the real parser and format supplied result fixtures, without pretending command families already exist. Test duplicate, unknown and inactive router registrations are rejected. Exact full registry/descriptor coverage and production execution belong to Task 7.
- [ ] Build/run `tst_cat_parser` and `tst_cat_catalog`; run attribution checks and commit signed: `feat: parse and format Thetis CAT commands`.

## Task 3: Bindings, settings and service ownership

**Files:** Create `CatModelAdapter.h/.cpp`, `CatSettings.h/.cpp`, `CatSession.h/.cpp`, `CatService.h/.cpp`, `tests/tst_cat_bindings.cpp`, `tests/tst_cat_settings.cpp`, `tests/tst_cat_headless.cpp`; modify `RadioModel.h/.cpp`, `src/core/daemon/DaemonApp.cpp`, `src/gui/GuiSessionCoordinator.cpp`, `src/gui/GuiDesktopStationRuntime.cpp`, logging and build lists.

**Interfaces:** `CatModelAdapter(RadioModel&)`; `snapshotBinding(const CatBinding&) const -> CatBinding`; `mayRead(const CatBinding&,CatVfo) const -> bool`; `mayChange(const CatBinding&,CatVfo,const QByteArray& property) const -> bool`; `resolveSlice(const CatBinding&, CatVfo) const -> SliceModel*`; `transmitModel() const -> TransmitModel&`; `radioModel() const -> RadioModel&`. `CatSettings::load(AppSettings&, const RadioModel&) -> QList<CatEndpointConfig>` and `save(AppSettings&, const CatEndpointConfig&) -> void`. `RadioModel::catService() const -> CatService*`. `CatService::applyChannelConfig(int, const CatEndpointConfig&) -> bool`; `channelConfig(int) const -> CatEndpointConfig`; `startConfigured() -> void`; `stopAll() -> void`. Construction is inert. Tester sessions set `CatSessionContext::transmitAllowed=false`; Task7 key/tune/two-tone actions must enforce it, while ordinary native sessions use true. Hosting-ready call sites start restored listeners only after existing station transmit/receive-only policy is installed; stop before model retirement or station handover.

- [ ] Test primary/secondary resolution with stable IDs0/2 and captured incarnations. Remove0, recreate slice ID0 with a new incarnation: the old binding resolves null until explicit rebind. Missing secondary/query never calls `addSliceOnPan`. Test owner, held-for-device, genuinely unclaimed/no-listeners and foreign-device slices against existing station access rules.
- [ ] Implement admission using public `SliceAccessPolicy::maySee/mayChange/stationMayChangeUnclaimed`, `SliceOwnership::stationDevice/refOf/matches/controlRevision`. Reads require visibility; mutations require existing station control or permitted truly-unclaimed access, without adoption or Take control. For frequency/dspMode/filterLow/filterHigh/txAntenna/band/xitEnabled/xitHz, reuse `StationSliceFreeze::frozenSlice/refusal` with actual station-keyed MOX and TX-bound slice (same public predicate used by RadioModel diversity). Test on-air refusal before mutation and unrelated-slice access. Revalidate captured incarnation/control revision before composite writes after synchronous callbacks. Global buffer/DSP configuration uses existing `RadioModel::stationOnAirRefusal(QString*)`. Do not call private StationServer admission methods.
- [ ] Test exact default schema, restore/save string booleans, invalid port/device combinations, preserved settings when SerialPort is absent, and no PTT or radio-connect signal from restoration.
- [ ] Register `nereus_add_test(tst_cat_headless CORE_ONLY)`: a `QCoreApplication` test constructing/destructing local-role RadioModel/service and a primed `DaemonApp` start/stop/restart without `MainWindow` or widgets. A remote-role model never starts listeners or touches local DSP. Use the existing daemon test seams, not network discovery or a real radio. Build/run the three targets to observe missing functionality failures.
- [ ] Construct one service in `RadioModel` after its MOX/arbiter wiring, with Qt parent ownership. Call `startConfigured()` only after `DaemonApp::start` has installed receive-only policy and `StationHost::start` has returned, or after successful `GuiDesktopStationRuntime::restore()` returns to `GuiSessionCoordinator::installDesktopStation` with station ownership policy established. This covers standalone desktop with RunCore=False; with hosting enabled, StationHost has already installed its keying gate. Preserve the existing lifetime/generation checks around restore. Call `stopAll()` at DaemonApp stop before `stopAllTx`, at `GuiSessionCoordinator::retireWindow` before resetting its runtime/window, and in `GuiDesktopStationRuntime::stop` before controller stop. Stop again defensively before model destruction/handover; it is idempotent. Remote-role construction stays inert. DaemonApp needs no second service instance. Service holds `QPointer` references where lifetime can end and clears sessions on stop. Expose typed model operations; never call compatibility shims whose setters do not reach real state.
- [ ] Reject endpoint and PTT-ingress configuration changes while running; stop explicitly, clear protocol-local state, apply config, and restart once. Runtime global CAT preferences (RTTY reporting, identity, AI routes and other non-ingress options) use `applyGlobalConfig` while started only when every PTT field is unchanged, preserving validation and exactly one `globalConfigurationChanged` notification. Permanent retirement rejects both kinds of updates. Radio disconnect invalidates TX and clears claims but may leave a configured TCP listener available for explicit disconnected responses.
- [ ] Build/run the three targets; commit signed: `feat: add CAT bindings settings and model-owned service`.

## Task 4: Transmit ownership and guarded release

**Files:** Create `CatTxCoordinator.h/.cpp`, `tests/tst_cat_tx_ownership.cpp`; modify `CatService`, `MoxController.h/.cpp`, `RadioModel.h/.cpp`, `TwoToneController.h/.cpp`, `tests/tst_mox_controller_ptt_source_dispatch.cpp`.

**Interfaces:** `CatTxCoordinator(RadioModel&)`; `requestPtt(quint64 sessionId,int sliceId) -> bool`; `releasePtt(quint64 sessionId) -> void`; `requestTxSelection(quint64 sessionId,int sliceId) -> bool`; `cancelSession(quint64) -> void`; `cancelAll() -> void`. Add `CatTransmitKind { Ptt, Tune, TwoTone }`, `requestTransmit(quint64 sessionId,int sliceId,CatTransmitKind) -> bool` and `releaseTransmit(quint64 sessionId) -> void`. PTT methods delegate to Ptt kind. Claims refer to a nonzero monotonically increasing activation tag, kind and target incarnation. Tune/two-tone are exclusive to one session; only Ptt claims combine on the same target.

**Existing Core authority:** Use `KeyerIdentity::station(PttMode::Cat)` for PTT; tune/two-tone use station Manual identity with `program=true`. Keep station device ID and empty session credentials. Never replace `setKeyingGate`, use an operator button method, manufacture a remote device identity or trigger Take transmit. Current `RadioModel::setMox` is a TCI shim, and `setMoxFromButton` is operator control. Reuse existing CAT PollPTT, `RadioModel::setTune(bool,const KeyerIdentity&)` and `TwoToneController::setActive(bool,const KeyerIdentity&)`. Baseline source-release protection already exists; there is no unconditional-release defect to fix here.

**Accepted-request observation (new, diagnostic only):** Extend `KeyerIdentity` with `quint64 requestTag{0}`, excluded from permission/identity equality. Existing callers keep0; CAT sets its activation tag. Add `MoxController::acceptedRequestGeneration() const -> quint64`, signal `requestAccepted(const KeyerIdentity&,quint64 generation,bool requestedOn)` and helper `observeAcceptedRequest(const KeyerIdentity&,bool requestedOn) -> quint64`. The helper advances a generation only for an intent already accepted by existing admission, before its idempotent return; it does not admit or key. Return the captured generation from before signal emission, so callers can detect supersession by synchronous callbacks. Observe accepted MOX/PTT and tune/two-tone intents, including idempotent operator requests; rejected requests do not revoke ownership. Preserve existing safety-effect and phase-signal ordering. Call sites check their returned generation before proceeding after reentrant notification.

Preserve `onCatPtt(bool)` and add `onCatPtt(bool,const KeyerIdentity&)` carrying station/Cat/program identity into its CAT level and PollPTT. The old method delegates with tag0; existing source fences/ordering remain. Latched tune and two-tone cycles copy the request tag into delayed work. Add `RadioModel::endTuneIfRequest(quint64 tag,quint64 expectedAcceptedGeneration) -> bool` and `TwoToneController::endIfRequest(quint64 tag,quint64 expectedAcceptedGeneration) -> bool`. They validate the actual cycle tag and current accepted stamp before the existing off/restore path; mismatches return false without mutation. Do not use deviceId-only `setMox(false,keyer)` as activation cleanup. Cancelled pending CAT callbacks are discarded before any rekey; cleanup/restoration may touch only their still-owned cycle, never a newer tone or drive state.

**Narrow implementation resolutions from current-Core source study:** Read `.crew/2026-10-04-thetis-cat-plan/task-4-seams.md` before editing. Mox acceptance observation belongs after unchanged `runMoxSafetyEffects` and before its idempotent return, capturing the incremented generation before notification and checking continuation ownership after every reentrant phase notification. Explicit accepted TCI ON while CAT is already keyed must observe despite the polling path not reaching MOX acceptance; periodic polls and rejected/non-owning releases do not create generations. Preserve typed identities through OFF and CAT polling, including when no gate is installed.

Add `MoxController::discardCatPttIfRequest(quint64 tag) -> bool` for a superseded tagged CAT-held input: require a nonzero matching latched CAT tag, clear that input/requester/refused-held bookkeeping without polling or key/unkey, and retire its stale CAT release association. Clearing stale `PttMode::Cat` to None is authorized if needed, with reentrant generation checks; alternatively use an association guard if the existing dispatch requires it. Legacy tag0 behavior stays as implemented. Test later unrelated VOX/mic/TCI polls cannot unkey the superseding accepted intent. Normal owned release still uses typed `onCatPtt(false,requester)`.

Tune/two-tone accepted repeats observe then adopt the existing cycle identity without resaving temporary mode/power or restarting tones. Rejected repeats retain the old cycle. Delayed activation, release and restoration capture a cycle serial/tag/accepted stamp and validate before effects and after reentrant calls. Supersession invalidates old pending work without unkeying/restoring a newer activation; guarded OFF alone is insufficient after its stamp mismatches. Global shutdown keeps existing authority. Queued connection work validates on the model thread before dispatch, with no cross-thread model reads introduced.

- [ ] Test two clients on one slice: disconnecting one keeps CAT MOX. Refuse conflicting targets and selection changes while any source is keyed or handoff is pending. A late CAT release after microphone/TCI takeover leaves the newer source keyed; a later event cannot rekey stale CAT.
- [ ] Test interlock/gate rejection, foreign-device TX ownership, radio/slice removal, incarnation reuse, external handoff, startup/restoration and reentrant cancellation before activation. Use `QSignalSpy` and existing compressed timer seams; no sleeps or real radio.
- [ ] Test accepted idempotent operator/TCI requests during CAT PTT/tune/two-tone supersede CAT even if `currentKeyer()`/MOX do not change. Rejects preserve the existing claim. Test a reentrant accepted request during observation aborts the older continuation; off/restore and delayed two-tone continuation cannot alter the newer activation.
- [ ] Build/run `tst_cat_tx_ownership` and source dispatch test to observe missing session/generation functionality. Do not assert a preexisting release bug that current Core has fixed.
- [ ] Accept first CAT claim only when connected, `state()==MoxState::Rx`, `!isMox()`, no pending arbiter handoff, valid incarnation and existing station transmit access. Reserve tag/kind/target; call `TxSliceArbiter::requestHandoff(int,const QByteArray&)` with `SliceOwnership::stationDevice()`. Confirm return, actual bound target, no pending handoff, unchanged generation and idle state before keying. Read controller acceptance before keeping claims. The current arbiter can complete keyed handoffs asynchronously through UnkeyGate: CAT refuses an unconfirmed move and never queues automatic TX.
- [ ] Watch accepted request stamps with a different tag, external unkey, arbiter changes outside the validated CAT selection, disconnection, slice removal and control/ownership changes. Clear claims/reservations and invalidate pending work. Same-tag internal steps refresh the activation's accepted stamp; each pending step validates its captured stamp/tag immediately before side effects. A generic recursion-depth flag cannot identify ownership. Reject reentrant CAT requests.
- [ ] On final PTT release, verify the coordinator tag/stamp, actual station Cat source and current target before typed `onCatPtt(false,requester)`. Tone releases use guarded wrappers. Cancellation during own final release is idempotent. Global Core shutdown still uses its existing `stopAllTx` authority.
- [ ] Build/run ownership/source dispatch plus existing arbiter/binding/band-plan and RadioModel tune/two-tone regressions. Commit signed: `feat: enforce CAT transmit ownership and safe cleanup`.

## Task 5: Frequency, mode, offsets, split and filter edges

**Files:** Create `CatRxCommands.h/.cpp`, `tests/tst_cat_rx_commands.cpp`; extend fixture/matrix entries and router registration.

**Interfaces:** `CatRxCommands(CatModelAdapter&, CatTxCoordinator&, CatSettings&)`; `execute(const CatRequest&, CatSessionContext&) -> CatCommandResult`. Concrete methods use `SliceModel::setFrequency(double Hz)`, `setDspMode(DSPMode)`, `setFilter(int,int)`, `setStepHz(int)` and existing RIT/XIT setters; transmit selection exclusively uses Task 4. Nereus model frequencies are Hz; Thetis's internal MHz representation does not cross this boundary.

- [ ] For production slices A/B initialized to 14074000/7074000 Hz, assert `FA; -> FA00014074000;`, `FB; -> FB00007074000;`; set FA to 14075000 Hz and verify model frequency/signals and query. Missing B returns `?;` and creates no slice.
- [ ] Test source mode codes without casts to Nereus enums, filter-edge lookup extremes, step-based UP/DN, RIT/XIT signs and enablement, and stable binding reuse. Test RTTY query/set inverse transformations at ±2125 Hz in DIGU/DIGL and disabled-offset behavior.
- [ ] Route RTTY preference setters through the service's runtime-global update contract; test updates during a live session and exactly one configuration notification, plus rejection without persistence for changed PTT ingress fields while running. Re-run CAT settings/headless targets for this service predicate change.
- [ ] Build/run `tst_cat_rx_commands` to observe failures, then implement commands in the RX mapping annex and Task 1 matrix, including standard aliases and source-preserved status field assembly. Omit IF/MD's first-500-request 50 ms blocking delay (`CATCommands.cs:56-58,319-322,568-571`) as a documented pacing adaptation; keep command order/bytes without main-thread sleeps.
- [ ] Map FA/FB and RX1/RX2 operations according to each handler, not to `setVfoHz(rx,chan)` or GUI focus. Preserve FR's source-inert setter silence/query 0; FT/ZZSP select actual primary/secondary TX through Task 4. Adapt ZZSW's TX assignment switch through the same confirmed selection operation; it never swaps frequencies. Query derives selection from the arbiter and returns unavailable when the bound TX slice is neither configured target.
- [ ] Use existing `RadioModel::onBandButtonClicked(SliceModel*,Band)` on the resolved stable slice. Do not add the old-baseline proposed extraction or temporarily switch focus. Verify actual frequency/band readback for the source reply/refusal contract. Preserve same-band/lock checks, per-band frequency/mode/DSP restore and stream-rate behavior. Test a CAT-bound slice different from GUI focus: it alone changes band. Do not temporarily switch active GUI focus.
- [ ] Use real `SliceModel` filter edges. Filter preset identity/VAR storage, RX1 subreceiver and memories remain unavailable. CTUN adapts source per-receiver checkbox behavior to the real shared-stream pin via `RadioModel::requestStreamCtunPinned(int,bool)` and live stream-state readback, with cohost behavior covered by tests; never use the TCI cached CTUN stub.
- [ ] Reject GUI-owned display commands in the display/mode evidence annex, including grid storage with no production model→widget subscription. Keep `RecenterVfo` disabled without an existing non-GUI model operation; do not call `SpectrumWidget` from core. Record this UI-owned capability gap in the report.
- [ ] Build/run `tst_cat_rx_commands`, `tst_tx_frequency_follows_tx_slice`, `tst_slice_rit_xit` and `tst_xit_offset_application`; commit signed: `feat: implement CAT tuning mode and offset commands`.

## Task 6: RX DSP, gains and squelch

**Files:** Create `CatDspCommands.h/.cpp`, `tests/tst_cat_dsp_commands.cpp`; update matrix/fixtures/router.

**Interfaces:** `CatDspCommands(CatModelAdapter&)`; `execute(const CatRequest&, CatSessionContext&) -> CatCommandResult`.

- [ ] Test existing admitted `RadioModel::invokeDiversityAsStationDevice(const SessionMessage&)` eight-argument diversity transaction for ZZDE (state revision, source/target slice ID, incarnation and control revision). Refusal leaves hardware/model state untouched; never use raw diversity setters. Test production AGC/NR/NB/ANF/APF toggles and parameters through actual SliceModel signals and the existing model-to-RxChannel wiring. Assert source ranges/scaling from the annex rather than assuming a TCI name means the same units.
- [ ] Test AG/ZZAG master output against `AudioEngine::volume`, ZZLA/ZZLE against primary/secondary slice AF, and reject RX1-subreceiver ZZLC/ZZLD. A readback follows UI/model changes, not last CAT set.
- [ ] Pin squelch distinctions: AM level SQL maps `amsqThresh`; VSQL slider maps `ssqlThresh` 0..100 and actual conversion /100; FM uses source `console.cs:47279-47330`. Add fixtures for both input extremes and no out-of-range mutation.
- [ ] Build/run `tst_cat_dsp_commands` to observe failures, implement the vetted receive/DSP matrix using typed setters and exact source constants. Buffer commands reuse `scheduleRemoteDspOptionsApply` and the existing off-air/lane-quiescence scheduler: RX groups for ZZHR/ZZHU/ZZHW, TX groups for ZZHT/ZZHX. ZZHV preserves its source-inert setter and queries actual CW RX. ZZHA remains unavailable because audio-backend buffering is a distinct capability. Keep WDSP algorithms untouched; record use of existing RX parameter wiring in the verification report.
- [ ] Reject RX EQ ZZEA/ZZER because current `RxChannel.cpp:3205-3225` only stores EQ state without a WDSP effect. For preamp/attenuation, use controller-bound `StepAttenuatorFacade` per-slice APIs, which resolve the correct ADC: `preampModeForSlice/setPreampModeForSlice`, `attenuationDbForSlice/setAttenuationDbForSlice`. Cover distinct ADCs and same-ADC sharing; source-inert preamp setter behavior remains inert where specified by the handler.
- [ ] Preserve confirmed inert source commands and unusual source behavior such as ZZVL cycling lock state instead of interpreting requested 0/1. Record each intentional Nereus difference separately.
- [ ] Build/run this target and `tst_slice_squelch`, `tst_rxchannel_squelch`, `tst_slice_agc_advanced`, `tst_rxchannel_agc_advanced`, `tst_slice_auto_agc` and `tst_slice_nb_persistence`; commit signed: `feat: implement source-grounded CAT receive DSP controls`.

## Task 7: TX/global families and complete dispatch coverage

**Files:** Create `CatTxCommands.h/.cpp`, `CatGlobalCommands.h/.cpp`, `tests/tst_cat_tx_commands.cpp`, `tests/tst_cat_coverage.cpp`; update matrix/router and verification report.

**Interfaces:** Both families expose `execute(const CatRequest&, CatSessionContext&) -> CatCommandResult`; TX family consumes adapter/coordinator/settings, global family consumes adapter/settings. The router has `registeredCodes() const -> QList<QByteArray>` for coverage verification.

- [ ] Test ID replies `900/013/019/020` for source identities, global serial number, supported power/mic/DEXP/TX EQ/monitor/profile operations against actual TransmitModel/audio state, and RX/TX through Task 4.
- [ ] Correct the earlier ZZSN unavailable mapping: `CATCommands.cs:6343-6350`, `setup.cs:4012-4015` and `setup.Designer.cs:59476-59484` prove that the getter returns editable CAT setup text, default `0000-0000`, rather than a hardware serial API. Read the actual configured `CatGlobalConfig::serialNumber`, preserve the descriptor's nine-character reply contract and absent setter form, and record the source interpretation correction in metadata, fixtures and mapping evidence. Test a nondefault configured value and invalid reply length; do not claim a measured hardware serial number.
- [ ] For ZZTU and ZZUT, use Task 4's exclusive Tune/TwoTone kind and existing identity-aware orchestration and Task 4 guarded off wrappers. Disconnect restores the operation's tone/timers/power but does not release a newer owner. PureSignal single calibration requires existing live capability and idle TX conditions; it does not invent a PTT claim.
- [ ] Lock mixed PS/ZZPS behavior: query actual `isConnected` as a documented power adaptation; off calls `disconnectFromRadio` and clears claims; on while already connected is silent/idempotent, on while disconnected returns `?;`. Never choose/discover/connect an arbitrary radio.
- [ ] Correct ZZEB's upstream array mismatch by validating the complete 36-character ten-band payload before changing model preamp/gains, preserving current band frequencies. Reject three-band payloads; keep existing model dB range -12..15. Test a bad final gain leaves all eleven values unchanged. Numeric ZZTP profiles map to the sorted real profile bank, not synthetic defaults.
- [ ] Test unavailable CWX, VAC, recording, memory and controller actions return their exact contracts and emit no unrelated side effects. Preserve source inert readbacks only where Task 1 proves them. Verify `ZZMX` reaches its explicit unavailable memory-store contract; its upstream dispatch exists.
- [ ] Build/run TX and coverage targets to observe missing families; implement mappings verified in the TX/global annex and remaining per-command Task 1 contracts. All 349 active descriptors get a registered outcome handler, including explicit unavailable handlers; all 70 inactive descriptors remain inactive.
- [ ] Map ZZTI to live `RadioModel::setRxOnly/rxOnly` and test actual MOX refusal for every source. Preserve global station receive-only policy and guarded readback; do not substitute `TxInhibitMonitor::notifyRxOnly`. ZZRV reads live `paReadings().paVolts`. Dedicated controller/PA-trip workflows remain outside this port as classified in the matrix.
- [ ] Add `CatModelAdapter::readRxMeter(const CatBinding&,CatVfo,RxMeterType,double& value) const -> bool`; use the resolved live RX channel and `RadioModel::rxMeterOffsetDbForSlice(int)` for SignalPeak/SignalAvg. Preserve source selector distinctions: ZZRM2/3 use ADC peak/average (`dsp.cs:959-965` aliases ADC_REAL/ADC_IMAG to RXA_ADC_PK/RXA_ADC_AV), rather than complex I/Q components. Use existing CoreSliceMeterPump/channel cache patterns, without GUI MeterPoller or a new audio-thread polling path. Missing/not-ready channel returns unavailable. RADE still has a real WDSP receive frontend: channel/readiness checks decide availability, not a blanket mode rejection. Add deterministic formatting/calibration tests and a live WDSP getter integration check.
- [ ] For ZZRM4 use `TxChannel::txMeter(TxMeterType::AlcAvg)` and `calculateTxMeter(ThetisTxMeterType::Alc,raw)` to preserve source sign/scaling. Selectors5/7 use connected live `RadioStatus::forwardPowerWatts/reflectedPowerWatts`; selector8 uses `swrRatio`, with source F0 watts/F1 ratio formatting. Extended framing echoes the request selector and trims the handler's 20-character padding. Record existing SWR clamp/telemetry as adaptations. Cover native peak/average distinctions instead of substituting ALC peak.
- [ ] Assemble supported combined status only from real model/facade/stream readback as classified in the current matrix. ZZXN/ZZXO pack real primary/secondary DSP and routed preamp fields; `ZZXV` remains unavailable because VFO-sync state has no production equivalent; reject the whole reply rather than fill guessed zeroes. Nonrepresentable RADE/Thetis mode enum collisions return `?;`, never a raw integer cast.
- [ ] Coverage test loads all CSV rows, checks exact descriptor/registry equality, executes every fixture, and uses real production model tests for every supported family. An unavailable outcome needs a concrete reason; no generic default stub may satisfy coverage.
- [ ] Build/run `tst_cat_tx_commands`, `tst_cat_coverage` and prior family targets; check attribution/provenance and commit signed: `feat: complete CAT command family dispatch and coverage`.

## Task 8: TCP sessions, framing, GUIDs and reporting

**Files:** Create `CatStreamFramer.h/.cpp`, `CatTcpTransport.h/.cpp`, `CatReporter.h/.cpp`, `tests/tst_cat_tcp.cpp`, `tests/tst_cat_reporting.cpp`; extend service/session.

**Interfaces:** `CatStreamFramer::feed(const QByteArray&) -> QList<QByteArray>`, `reset() -> void`; oversized frame produces one error frame/event and discards through the next `;`. `CatService::processBytes(quint64 sessionId, const QByteArray&) -> void`; `sendToSession(quint64,const QByteArray&) -> void`; `sendToGuid(const QUuid&,const QByteArray&) -> void`. `CatReporter::setAiEnabled(bool) -> void`; `flushPending() -> void` drains reports whose due time has arrived. A guarded test-only `setClockForTest(std::function<qint64()>) -> void` supplies monotonic milliseconds for deterministic interval tests.

- [ ] Test TCP ephemeral-port lifecycle, occupied bind failure, two isolated sessions, command fragments (`FA` then `;`), coalesced `FA;ID;`, shutdown/restart and disconnect cleanup.
- [ ] Test 41-byte maximum request, oversized unterminated input followed by terminator and valid ID, text spaces preserved, lowercase prefixes, and newline handling. Do not buffer an entire arbitrary read before processing delimiters.
- [ ] Test valid `ZZGA<36-char-guid>;` replies with lowercase canonical GUID and registers TCP session; ZZGR removes it. Reject bad GUID/extra suffix. Serial/PTY returns the same canonical single frame without TCP registration side effects. This deliberately corrects upstream serial duplicate-prefix/GUID framing (`CATParser.cs:1544` adds prefix+suffix to the already prefixed handler return); record/test the divergence. This strict exact-width TCP behavior deliberately fixes upstream's acceptance of trailing junk.
- [ ] Build/run TCP/reporting targets to observe failures; implement main-thread `QTcpServer/QTcpSocket` asynchronous reads/writes, per-client buffers, error handling and optional welcome. Welcome identifies NereusSDR instead of claiming to be Thetis; default off.
- [ ] Port Thetis AI scope as global enablement gated by `AllowKenwoodAi`, with configured serial1..4/TCP routes. Normalize each channel's A/B mapping in emitted `FA/FB/ZZSW` messages; broadcast to eligible clients, not only the command sender. Sources: `CATCommands.cs:1223`, `console.cs:7922,39744,51317`.
- [ ] Preserve the source 200 ms flood-control interval (`console.cs:53886-53957`) as named `kAiIntervalMs`; one main-thread Qt timer tracks the latest pending value per logical channel/message type. Per-FA/FB/ZZSW keys adapt Thetis's global frequency UID so changing both bound VFOs cannot lose one update. Coalesce duplicate values; a setter keeps its source-defined reply and the eligible AI event. Use the injected clock to assert first-send, latest-value, exact interval and disconnect cleanup without sleeps.
- [ ] Build/run both targets plus ownership; commit signed: `feat: add native TCP CAT sessions and reporting`.

## Task 9: Physical serial CAT and CTS/DSR PTT

**Files:** Create `CatSerialTransport.h/.cpp`, `tests/tst_cat_serial.cpp`, `tests/tst_cat_serial_ptt.cpp`; extend service/settings/CMake guards.

**Interfaces:** Transport exposes `start(const CatEndpointConfig&) -> bool`, `stop() -> void`, `writeBytes(const QByteArray&) -> void`; signals `bytesReceived(QByteArray)`, `failed(QString)`, `pttSampled(bool cts,bool dsr)`. `CatService::applyPttSample(int channel,bool cts,bool dsr) -> void` uses a release-armed input latch and one dedicated session claim. CAT1..4 sources use that channel; a separate physical device uses configured Ptt/Channel. Resolve the actual TX-selected primary/secondary binding through Task4; a target outside that binding is refused, without implicit handoff.

- [ ] Add a narrow test transport seam that drives byte/pin/error events without hardware, while a real QSerialPort integration test uses a PTY slave on macOS/Linux. Test config validation, fragmentation, reopening, partial writes, device disappearance and unavailable dependency state.
- [ ] Assert startup cts=true never keys; cts=false then true requests CAT TX; enabling DSR instead applies the same edge rule. Either selected asserted input keeps one claim until both selected inputs release. Error/stop clears claim and rearming state.
- [ ] Build/run both targets to observe failures; implement `QSerialPort` with explicit named enum conversion and checked setters/open results. No hardware flow control when RTS is used explicitly. Confirm accepted configuration through real port getters; an unsupported combination is a visible error.
- [ ] Poll open-port `pinoutSignals()` using a main-thread QTimer because input notifications are not universally available. Use a named 20 ms Nereus sampling interval; this is transport scheduling, not a DSP parameter. Inject samples in unit tests rather than waiting for the timer. Legacy RTS label samples `ClearToSendSignal`; DTR label samples `DataSetReadySignal`, source `SDRSerialPortII.cs:234`.
- [ ] Separate PTT physical device is exclusive; a CAT1..4 PTT source shares that already-owned serial handle instead of opening it twice. Reject duplicate device assignment and an enabled pin mode on an unopened endpoint. Clear claims before close, leave callbacks harmless after stop.
- [ ] Build/run serial/PTT/ownership targets; configure a separate `build-no-serial` with `-DCMAKE_DISABLE_FIND_PACKAGE_Qt6SerialPort=ON` and run unavailable-state coverage. Record real cable testing separately. Commit signed: `feat: add serial CAT and input-pin PTT support`.

## Task 10: Optional POSIX PTY transport

**Files:** Create `CatPtyTransport.h/.cpp`, `tests/tst_cat_pty.cpp`; add platform build guards and service lifecycle.

**Interfaces:** `start(int channel, const CatEndpointConfig&) -> bool`, `stop() -> void`, `slavePath() const -> QString`, byte signals/write contract matching Task 9. Empty path/unavailable state on Windows.

- [ ] Test opening the returned slave sends real bytes to the session/router, concurrent channels do not mix, stop closes descriptors and removes only the service's own path, and reopening has no stale PTT/session state.
- [ ] Build/run `tst_cat_pty` to observe failure; on macOS/Linux use `posix_openpt`, `grantpt`, `unlockpt`, `ptsname` and `QSocketNotifier` with raw terminal settings and nonblocking writes. Check every result and use RAII descriptors. Do not use a global `/tmp` symlink or overwrite an existing file.
- [ ] Expose the actual `/dev/pts/...` or `/dev/ttys...` slave path, without a persistent alias in this delivery. Keep Windows physical/user-supplied virtual COM support in Task 9, with PTY unavailable.
- [ ] Build/run PTY and TCP framing targets on supported platforms; compile unavailable branch on Windows CI. Commit signed: `feat: add optional native CAT PTY transport`.

## Task 11: Live setup, applet, status, log and tester

**Files:** Modify `src/gui/setup/CatNetworkSetupPages.h/.cpp`, `src/gui/applets/CatApplet.h/.cpp`, `SetupDialog.h/.cpp`, `MainWindow.h/.cpp`; create `src/gui/setup/CatLogWindow.h/.cpp`, `tests/tst_cat_setup.cpp`, `tests/tst_cat_applet.cpp`. Register GUI sources/tests.

**Interfaces:** CAT pages receive the service through `RadioModel::catService`; reuse the existing `CatTcpIpPage` and `CatSerialPortsPage` names and current SetupScope patterns; extend constructors to `CatTcpIpPage(RadioModel*,QWidget* parent=nullptr)` and `CatSerialPortsPage(RadioModel*,QWidget* parent=nullptr)`. Create `CatOptionsSetupPage(RadioModel*,QWidget* parent=nullptr)` and `CatPttSetupPage(RadioModel*,QWidget* parent=nullptr)` under the same current setup lifecycle. CAT remains scoped to ThisComputer/local-authority; a remote-role window shows the reason it cannot host this service. Service signals `channelStateChanged(int,QString)`, `clientCountChanged(int,int)`, `ptyPathChanged(int,QString)`, `configurationChanged(int)`, `messageLogged(int,bool,QByteArray)` drive UI. `CatService::testCommand(int,const QByteArray&) -> QByteArray` uses an isolated non-PTT tester session; rejects mutating TX/TUNE actions.

**Live configuration refinement (settled after Tasks 8–10):** Add service-owned `reconfigureChannel(int,const CatEndpointConfig&) -> bool` and `reconfigureGlobal(const CatGlobalConfig&) -> bool`; retain existing low-level stopped-channel/runtime-only-global apply contracts. The return value means accepted desired configuration, not successful activation. Invalid, denied, retired or superseded operations return false before any invalid-request effects; exactly unchanged configuration returns true without persistence, notification, reconnect or binding recapture. A valid enabled configuration whose open/bind fails remains saved and visibly reports its actual failure. Validate the whole proposed tuple and device collisions before teardown. Detach affected sessions, reports, registrations, framing, PTT input and handles, and release their exact claims before external callbacks; adopt/persist/notify once and restart only affected ingress while the operation/run remains current. A stopped service remains intentionally stopped; a surviving started run restarts only affected ingress. Changing one channel must preserve unrelated clients and their live callbacks; channel-local operation/transport identity guards complement the global start/stop generation. Global runtime preference changes do not restart listeners; PTT ingress changes affect/rearm the relevant input only. Reentrant newer configuration wins persistence, and stop/delete/permanent retirement wins lifecycle. Establish synchronous AppSettings hook regression evidence before narrowing CAT write cancellation; do not suppress the global hook or add an AppSettings transaction architecture. Callback-started PTT must see a coherent desired tuple rather than partially saved keys. UI has no independent writer or stopAll/startConfigured cycle.

**Native presentation refinement:** Retain the existing four CAT indicators/path rows and TCP/PTY button layout. The buttons control CAT1 with explicit tooltips; all four channels have individual Setup controls. This wiring choice is flagged in the local PR description. Enable the delivered native Cat built predicate, existing Tools/applet/status and local Setup reachability with affected unbuilt/catalogue regression checks. Catalogue `catControl` remains `where=station` and offered according to builtCat; remote desktop pages/applet remain disabled with a host-local reason, without a new remote configuration operation. SerialPort/platform/rigctld capability gaps are explicit. Preserve VAX/IQ placeholders. Log displays bounded exact request/reply bytes and separate diagnostics. Tester uses a separate real parser/router session and denies keying operations, VOX enable and PS single/calibration.

- [ ] Test enable/config changes invoke one service operation, external updates sync without echoes, absent SerialPort/platform options stay unavailable, actual status/client counts/PTY paths appear, and failed starts do not display success.
- [ ] Test rig identity/RTTY/AI options round-trip exact settings; disabling a transport clears its sessions/PTT. Tester uses the real parser/router but cannot key. Log records request/reply bytes separately from transport diagnostics.
- [ ] Build/run GUI targets to observe failures; wire existing serial controls and add parity/bits/stops, stable slice selectors and native TCP/PTT/options pages. Preserve theme, translated strings, keyboard access and scroll-wheel guards; show physical sampled CTS/DSR wiring beside legacy labels.
- [ ] Enable Tools CAT Control, instantiate/register CatApplet using existing applet host patterns, bind CAT TCP/PTY controls and per-channel status. Preserve unrelated VAX/IQ code; do not implement or remove its placeholders as part of CAT.
- [ ] Replace static CAT indicator with service status. Add bounded log viewing based on existing TCI log-window patterns. Unavailable compatibility options show a reason instead of persisting active-looking behavior with no model effect.
- [ ] Build/run GUI and settings/headless tests; inspect native UI at 1280x800 and scaled display for readable fields and no clipping. Commit signed: `feat: wire CAT setup applet and diagnostics`.

## Task 12: Integration invariants and platform checks

**Files:** Create `tests/tst_cat_integration.cpp`, verification fixtures/report at `docs/architecture/thetis-cat-verification/README.md`; modify test registration and `.github/workflows/ci.yml` only for uncovered new target registration/sharding or a confirmed narrow optional SerialPort compile-coverage gap. Keep SerialPort optional; CI configuration is not execution evidence.

**Interfaces:** Use existing public CAT/service/model interfaces; no new runtime test-only API beyond the documented transport injection and reporter flush seams.

- [ ] Integration test drives real TCP command bytes into production RadioModel and verifies tuning/mode/filter/TX selection and readback, model-to-radio signal intent, AI route isolation and disconnect cleanup. No mock-only successful method lookup.
- [ ] Re-run existing `tst_core_has_no_gui_includes` against the new CAT sources; the test runs without opening an application/radio.
- [ ] Build/run integration and boundary tests plus all focused CAT targets. Verify no SerialPort build, macOS/Linux PTY code, Windows compile, and existing MOX/arbiter/TCI regressions.
- [ ] Record core RX-path contact precisely: model parameter controls reuse existing I/Q→WDSP→audio wiring; no processing algorithm/routing edits. Mark discovery→connect→receive/audio bench testing pending until performed.
- [ ] Commit signed: `test: verify CAT production integration and architecture boundaries`.

## Task 13: Separate Hamlib rigctld delivery

**Files:** Create `RigctlProtocol.h/.cpp`, `tests/tst_cat_rigctld.cpp`; extend service listeners/settings/UI and Aether provenance.

**Interfaces:** `RigctlProtocol(CatModelAdapter&, CatTxCoordinator&, int channel, quint64 sessionId)`; `handleLine(const QString&) -> QString`. Session owns newline framer and command-local extended-response state; it uses the same target selection/PTT coordinator as Thetis sessions.

- [ ] Port supported standard short/long command and response contracts from AetherSDR `RigctlProtocol`, checking Hamlib's primary documentation for ambiguous wire behavior. Test `f/F`, `m/M`, `t/T`, VFO selection, split selection/frequency/mode, RIT/XIT, level/function read/set and unsupported `RPRT` errors, plus extended/pipe response formatting.
- [ ] In tests, VFO B without a configured secondary returns the source-derived unavailable error and creates no slice. Modify Aether's create-on-demand split behavior deliberately to match the approved Nereus contract; document this divergence.
- [ ] Build/run `tst_cat_rigctld` to observe failure; implement a distinct router/framer, four disabled-by-default listener slots with user-chosen nonzero ports. Do not reinterpret Thetis CAT on the same socket or claim FlexRadio control compatibility.
- [ ] Test TCP and optional PTY rigctld sessions share stable IDs and obey identical same-target/conflicting-target/disconnect PTT rules. Enable the additional UI only when this stage is delivered.
- [ ] Build/run rigctld, bindings, ownership and PTY targets; register Aether source attribution/provenance in the same commit. Commit signed: `feat: add slice-bound Hamlib rigctld dialect`.

## Task 14: Final review, acceptance and integration handoff

**Files:** Finish the compatibility CSV and verification README; update `docs/MASTER-PLAN.md`, `docs/development/project-status.md` and `CHANGELOG.md` with actual delivered status and limitations only.

- [ ] Compare every spec requirement against Task 1 matrix, actual production code and fixture results. Review all source headers/inline tags, unit conversions, rejected side effects, slice removal, signal ordering and ownership cancellation.
- [ ] Configure/build the application and named CAT/regression targets, run focused tests, then build `all_tests` and run the full suite once for final completion as required by fast-test-loop. Capture raw results and separate platform compile/hardware evidence. Never describe stale binaries or unperformed tests as passing.
- [ ] Run the exact compliance commands below; expected exit 0 and no new findings. Fix findings without bypassing hooks or recapturing the cite baseline to hide additions.

```sh
export NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis
python3 scripts/verify-thetis-headers.py --all-kinds
python3 scripts/verify-freedv-headers.py
python3 scripts/verify-provenance-sync.py
CHECK_NEW_PORTS_BASE_REF=27716f5d6700e1e7d1808acfe478756249828e8c python3 scripts/check-new-ports.py
CHECK_NEW_PORTS_FULL=1 python3 scripts/check-new-ports.py
python3 scripts/verify-inline-cites.py
python3 scripts/verify-inline-tag-preservation.py
python3 scripts/verify-no-gui-dsp-access.py
python3 scripts/verify-no-captured-slice-spectrum-wiring.py
```
- [ ] Use one integrated reviewer for the whole branch under crew; fix actionable findings with the owning implementer and re-run affected checks. Do not repeat an unchanged full suite unless code changes or a specific concern warrants it.
- [ ] Record real logging/digital-mode client and serial/PTT device checks as performed or explicitly unverified. No automated agent operates a radio merely to satisfy this checklist.
- [ ] Prepare the PR description with source study, port/design decisions, upstream defect corrections, unavailable capability list, RX-path contact and test evidence. Create/attach a PR only in the shipping lane JJ authorizes; no release, merge, deployment or radio operation is implied by this plan.

## Execution and review record

Use an implementation ledger at `docs/architecture/thetis-cat-verification/progress.md` once execution is approved. Record task ownership, explicit worker model/effort, accepted commits, command/test evidence, corrections and next action; token usage is unknown unless tooling reports it. Suggested execution is crew with sequential Sol 6.1 workers, Sol 6.1 high for TX ownership/integration and one final integrated review. The controller verifies actual diffs and tests before accepting worker results.

Planning verification performed: source XML SHA-256 and all419 activity/width records checked; matrix codes exactly equal inventory; counts349 active/70 inactive; maximum active request41 bytes; all named existing regression test files present; git ancestry and GPG signatures checked. Runtime/application tests have not been run for these documentation changes. Planning workers used Sol6.1 high for the separate RX and TX/current-Core audits; token usage and exact elapsed time are unavailable. The lead reconciled overlapping buffers/reporting and performed the plan review.

Planning self-review: all design sections have owning tasks; the requested Core Controller tip already supplies the R1 boundary, daemon and slice-specific band operation; shared interface names and settings have one definition; five review-focus risks have explicit test steps. All419 descriptors have source ranges, widths, outcomes and read/set contracts in the checked-in matrix; Task1 materializes their exact request/reply fixtures as the pre-port gate. The source inventory and planning matrix are complete, but reviewed contracts and implemented functional parity remain distinct.

Primary transport references: [Qt QSerialPort](https://doc.qt.io/qt-6/qserialport.html), [POSIX posix_openpt](https://www.man7.org/linux/man-pages/man3/posix_openpt.3p.html). Use APIs supported by this repository's existing Qt floor; do not require newer buffer APIs or settings-restoration properties merely because the current online documentation lists them.
