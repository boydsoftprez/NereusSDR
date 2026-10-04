# Current CAT TX/global/audio/peripheral mapping evidence

Research only, 2026-10-04. Production base **27716f5d6700e1e7d1808acfe478756249828e8c**, supplied by the lead; design rebased head **19a90f263**. This audit supersedes every Nereus API/absence claim from the previous **e342467fb** report. The historical report is preserved only at `/tmp/nereus-cat-tx-mapping-evidence-e342467fb.md` and is not evidence for current capabilities.

Production root: `/Users/j.j.boyd/.codex/worktrees/thetis-cat/NereusSDR`. Upstream root: `/Users/j.j.boyd/Thetis/Project Files/Source/Console`, pinned **Thetis v2.10.3.15 / 3759d096**. All Nereus paths below resolve against the production root; source paths resolve against the upstream root. Re-read current CLAUDE.md, HOW-TO-PORT.md and fast-test-loop.md; no worktree AGENTS.md was present, so the supplied AGENTS instructions remain authoritative. No product edits, git operations, builds, tests or agents were performed.

Current contracts: `/tmp/nereus-cat-tx-contracts.json`, 172 active commands; metadata `/tmp/nereus-cat-tx-contracts-metadata.json`. Counts: **Faithful 14, Adapted 35, SourceInert 38, Unavailable 85**. Every row contains code/family/outcome/targetApi/scale/readContract/setContract/reason/sourceRange. No covered command is left unreviewed. The complete419 inventory now has349 active/70 inactive; whitespace-active ZZHW belongs to the receiver worker. Receiver worker owns the complete ZZRM row; the TX subset below is supplied separately.

`Unavailable` means no ordinary-endpoint functional mapping within the approved design, including explicit controller-integration exclusions. It does not assert that similarly named runtime machinery never exists. Every absent capability rejects both source-supported forms as `?;` before mutation; write-only commands have no invented read state. `SourceInert` preserves pinned disabled/constant/error behavior, including exact harmless setters and errors. Thetis range/clamp/rounding, numeric grammar and formatter details in the JSON remain source-grounded. Protocol-owned RigIdentity, GUID routing, verbose errors and global AI settings are not fake radio capabilities.

## Authoritative current deltas

- **Global receive-only** is now implemented: `RadioModel.h:1330-1375`, `RadioModel.cpp:28028-28049`; `applyTxKeyBlock` at27954 sends effective RXOnly to MoxController. **ZZTI** is faithful write-only0/1 and uses `setRxOnly(bool)`; source `CATCommands.cs:6727-6745` has no query. Setting1 unkeys and gates all sources;0 never defeats a forced receive-only SKU.
- **Power limits** now exist and affect drive: `TransmitModel.h:553-576`, `RadioModel.cpp:6981-6996`, `TransmitModel.cpp:1962-1981`. PC/ZZPC report `power()` when LimitReportedPower=False; True reports `powerSliderLimitEnabled() ? min(power(),powerLimit()) : power()`. This is a read-only derivation of actual existing constraint logic, not measured RF watts. The option's True default can now be functional. Never call `setPowerUsingTargetDbm(false)` as a getter. CAT setters still validate/clamp against source0..100 and real SKU slider ceiling because `setPower(int)` itself does not clamp.
- **Tune power** uses the same source enum and actual TX band. For ZZTO DriveSlider use the PC rule; TuneSlider reports `tunePowerForTxBand()` constrained by `tunePowerLimit()` only when LimitReportedPower=True; Fixed reports `tunePower()` without constraining. `TransmitModel.h:738-755` supplies known-band state. Its `setTunePowerForTxBand(int)` changes the source to TuneSlider, so source-preserving ZZTO writes use `setTunePowerForBand(Band,int)` after confirming the current bound band. Preserve HL2 normal90/tune99 limits.
- **PA volts** now have live optional readback: `RadioModel.h:1637-1644`, `RadioModel.cpp:8330-8347`; `RadioConnection.h:252-253`, `.cpp:252-257`. ZZRV uses `paReadings().paVolts` on the five pinned source real-voltage model branches, formats minimum2 integer digits/1 decimal. Missing reading is `?;`, never0. Other non-HPSDR source branches still return source constant00.0; HPSDR errors. Do not replace PA drain volts with supply volts or broaden the pinned model list merely because a new SKU has telemetry.
- **Ganymede trip safety** is now functional: `RadioModel.cpp:25833-25873`, global gate27954. ZZZA remains unavailable on ordinary CAT endpoints because device-origin protection messages are a separate integration under the approved scope. The old claim that it only changes a mirror or fails to gate MOX is superseded.
- **TX EQ/CFC** now use complete profiles. `RadioModel.cpp:33432-33496` selects legacy vs parametric TX EQ;33362-33422 binds complete CFC profiles. ZZEB remains the ten-band legacy gain bank and preserves frequencies/selected EQ mode. Fully validate all36 characters before mutation, then coalesce the synchronous setter batch so DSP receives only the final complete profile. Source003/three-band remains rejected; pinned eqform array overrun is explicitly fixed. Do not mutate CFC to emulate EQ or PhoneDX. ZZET remains actual EQ enable.
- **DSP buffer families corrected after receiver reconciliation:** ZZHR/ZZHU use the receiver worker's verified live RX group fanout; ZZHT/ZZHX use the same scheduler's TX half, `RadioModel.h:4340/.cpp:29248-29313`, and actual `TxChannel.h:2197 txDspBlockSize()` plus onModeChanged2212/cpp5632-5693. Store real PhoneTx/DigTx group preference then coalesced off-air/lane-safe apply, retaining buffer<=filter and actual readback. ZZHV has a source-inert setter but a supported CwRx query (source3314-3332), so it is Adapted with exact no-mutation setter. Only ZZHA audio-backend buffer lacks an equivalent. Do not confuse these live per-mode WDSP buffers with deferred AudioEngine global DSP preferences.
- **CW/CWX** remains unavailable: `CwxApplet.cpp:22` marks all controls NYI, `MoxController.cpp:2932-2936` rejects CW keying, `UnbuiltFeatureList.h:60` lists Cwx. `SliceModel.cpp:2322-2353` CWPitch is a filter computation, not a runtime keyer API. CodecContext CW bitfields alone are not a production model feature. Reject KY before its upstream automatic-mode side effect; static/stored TCI CW state is not implementation.
- **VAC/VAX** remains an explicit unavailable topology adaptation. Current `StationVaxFacade.h:14-28` describes four output channels/single TX level; `DaemonApp.cpp:273` disables VAX outputs. `AudioEngine.cpp:3141-3154` RX gain0..1 cannot represent source positive40dB; TX gain3170-3176 still has no pull-side consumer (`AudioEngine.h:993-994,1497-1501`). No guessed pair mapping or driver/cable index. Current remote VAX routing is real but does not establish source two-VAC-pair equivalence.
- **CPU** now has a real Linux host sampler (`core/daemon/HostTelemetrySampler.h:31-54,100-136`, optional system/process percentages; cached900ms, absent initial/platform data). ZZCU remains unavailable because no existing model exposes source-selected 0.8/0.2-smoothed CPUPercSmoothed (`console.cs:26258-26274`). This is an exact-capability limit, not the old incorrect no-sampler claim; no fabricated cross-platform CPU0.

## TX ownership and existing fences

`RadioModel::ownsLocalDsp()` at `RadioModel.h:1228` is the authority predicate; Role::Local includes desktop hardware owner and daemon. Role::Remote is a GUI mirror and must create no CAT listener or model-writing service. No CAT extension to the station remote protocol is within scope.

Current baseline entry points:

- `MoxController::onCatPtt(bool)`, header1083/cpp2820-2846. PollPTT keys only from receive/outside manual key, refuses TX block, releases only the station Cat source and respects existing keying gate. `KeyerIdentity::station(PttMode::Cat)` cpp549-555 sets program=True. CAT is a station program, never a synthetic paired remote device.
- `RadioModel::setTune(bool,const KeyerIdentity&)`, header5134/cpp26325-26336; real admission26403-26430 precedes tune state/mode/power changes. For CAT tune use station identity/sourceManual with program=True so it cannot take another device's transmit. Plain tune is an operator request. `setTune(false)` ends any tune; coordinator must guard it.
- `TwoToneController::setActive(bool,const KeyerIdentity&)`, header314/cpp197-204; cpp252 asks `admitKey` before prep. For CAT set program=True on station/sourceNone identity. `isActive`, `isActivationInFlight`, `keyer` at header290-302. Plain start is an operator request; off ends any current test and must be guarded.
- `TxSliceArbiter::requestHandoff(int,const QByteArray&)` header125 admits the station requester's owned slice under existing access/frozen rules. Do not use unscoped overload to bypass owner checks. Confirm `txBoundSliceChanged(int,int)` header211 and final stable ID, not a true return alone (handoff may wait on unkey).
- `RadioModel::setMox(bool)` cpp26971 is the TCI shim, `setMoxFromButton(bool)` cpp27064 is operator/manual and remote-screen routing. Neither is a CAT replacement.

`StationServer.cpp:2477-2760` installs the existing keying gate for source/device ownership, program requests, fencing and taking. CAT must not install another gate or bypass it. Remote watchdog2772-2800 stops only the device it watches through normal `stopAllTx`; CAT claim teardown must observe that stop, never re-key afterward. `RadioModel::otherDeviceHoldsRefusal()` (`RadioModel.h:3703-3710`) is an available local view of another device's hold. Profile selection has a checked wrapper `selectTxProfileForStation(name,reason,takenOnAir=false)` at header3908/cpp6556-6576; use that wrapper and truthful permission, not direct manager bypass. Source bank read uses sorted `profileNames()` (`MicProfileManager.cpp:910-920`).

Observe actual phase/state: `MoxController.h:1327-1333` hardwareFlipped/txReady/rxReady; refusal1270-1276; diagnostic state1478; `RadioModel.h:5607-5616` transmitStopped/keyedByChanged. Confirmation follows live source, accepted admission and final target; never a remembered session claim alone. All command dispatch, claim accounting and model calls run on the model event-loop thread. Typed TxChannel setters retain their existing DSP-lane scheduling/atomics; no new direct WDSP calls or RX-path edits.

### Proposed minimal extension (not existing production API)

Current identities distinguish station/remote device/source, but not two CAT endpoints or an obsolete station activation. `MoxController::setMox(false,keyer)` cpp568-594 compares only deviceId. Accepted idempotent keys can return without updating currentKeyer. A generation token remains necessary for claim cleanup and pending continuations; it must supplement existing admission, not implement another station ownership manager or global TX refcount.

Concrete proposal for the lead's Task4:

1. Add observational `quint64 requestTag{0}` to KeyerIdentity, excluded from operator==/permission identity, like its existing session qualifier is excluded. CAT uses its nonzero activation token; existing UI/TCI/radio callers default0. Do not encode tokens as synthetic deviceId or station-session credentials.
2. Add `quint64 acceptedRequestGeneration() const` and signal `void requestAccepted(const KeyerIdentity& requester, quint64 generation, bool requestedOn)` to MoxController. A single orchestration helper `quint64 observeAcceptedRequest(const KeyerIdentity& requester, bool requestedOn)` advances the counter only after the existing admission accepts an intent and before an idempotent early return; it performs no gate/admission or keying itself. Instrument accepted TUNE and two-tone repeats as well as MOX/PTT. Rejected requests do not revoke a valid activation. Idempotent operator/TCI intents with tag0 supersede old CAT generations even if currentKeyer remains unchanged.
3. Preserve existing `onCatPtt(bool)`; add `void onCatPtt(bool pressed,const KeyerIdentity& requester)` carrying a copied station/Cat/program requester into the CAT level and PollPTT, since the old bool-only method cannot carry a request tag into delayed work. The old bool API delegates with the original station/Cat/tag0 identity. Runtime source fences remain unchanged.
4. Pending handoff/tune/two-tone steps capture tag+accepted generation; before any continuation or re-key, check the current accepted observation, target existence/binding and existing ownership gates. A same-tag internal stage may advance observation and update its owned generation; a different tag or tag0 accepted external intent revokes it. Reentrant stop/target-change must invalidate before a callback can dispatch. Never queue a refused CAT press for a future automatic key.
5. Add guarded normal-off wrappers `bool RadioModel::endTuneIfRequest(quint64 requestTag,quint64 expectedAcceptedGeneration)` and `bool TwoToneController::endIfRequest(quint64 requestTag,quint64 expectedAcceptedGeneration)`. Check latest accepted stamp and latched actual cycle tag before calling existing normal off path. RadioModel must latch the active tune requester for the cycle; its existing m_tuneKeyer is only a scoped start pointer and cannot serve as a lifetime token. Stale/zero/mismatched requests returnFalse without mutation. CAT PTT final release similarly checks own generation and actual Cat level/source before ordinary onCatPtt(False). Do not use deviceId-only setMox(False,keyer) as token cleanup.
6. Existing global StopAllTx and teardown stopNow remain global emergency/lifecycle stops. Their observation cancels all CAT claims first. No broad global refcount, gate replacement or change to baseline public operator APIs.

This is the minimum proposed observation/guard surface, distinct from APIs verified above. The implementation plan must test accepted idempotent takeover from UI/TCI between CAT start and cleanup, async old two-tone completion after a new operator key, and stale tune-off after a newer cycle. The lead may choose owner-scoped equivalents if they preserve all these boundaries.

## Service creation and start/stop hooks

Own CatService on the authoritative RadioModel/model thread, but construct it cold: constructor must not restore/open listeners. Service-level ownsLocalDsp checks alone do not establish policy readiness.

- Daemon: `DaemonApp.cpp:299` installs receive-only station policy before callbacks; station bind/accessory/TCI setup follows. After `m_stationHost->start()` at460 returns, station server/ownership/keying gate wiring exists (`StationHost.cpp:150-170`). Call explicit `CatService::startConfigured()` there, with independent disabled/default transport settings. If remotePort0/invalid skips station server (`StationHost.cpp:126-142`), the daemon policy still gates keys; do not rely on a nonexistent server. Lead decides exact shared hosting-ready hook across this case.
- Desktop: exact common hook is `GuiSessionCoordinator::installDesktopStation()` at269-274, after `m_desktopRuntime->restore()` and the model/generation guard, before `beginStationHandoverEditTracking()`. `GuiDesktopStationRuntime::restore()` at233-272 validates `ownershipAvailable` and activates `StationSliceOwnershipPolicy` at245 even when RunCore=False; with hosting enabled it then completes `DesktopStationController::start(true)`/StationHost gate setup before returning. Start CAT only when local role and profile/policy establishment succeeded. Capture restore/ownership readiness explicitly; do not infer ready from the mere existence of a runtime or a failed host config. This names the standalone RunCore=False and hosted path without MainWindow/RadioModel constructor autostart. A remote GUI model does not enter this local installation and cannot start service.
- Daemon stop: explicit CAT quiesce/stopAll first, before `stopAllTx` in `DaemonApp.cpp:488`, StationHost quiesce503 and retirement/reset529-531. This blocks reentrant queued CAT ingress and clears claims before the existing global TX stop.
- Desktop common retirement stop: CAT quiesce/claims invalidation in `GuiSessionCoordinator::retireWindow()` before runtime reset/window retirement at185-192, and `GuiDesktopStationRuntime::stop()` at662 before controller stop. Hosting additionally stops CAT before `stopAllTx` in `DesktopStationController.cpp:99`; existing host quiesce/stop follows. MainWindow aboutToQuit at1713-1727 and closeEvent17757-17765 are defensive stop hooks before actual radio disconnect. Radio handover, model retirement, station disconnect and reconnect must not preserve claim or automatic re-key. A transport stop must release only its sessions' valid activation while the model is alive; late callbacks are generation-invalidated.
- Build: add actual CAT source files to CORE_SOURCES/MODEL_SOURCES as appropriate so `NereusCore` (`CMakeLists.txt:2023-2035`) owns them; `nereusd` links NereusCore only (`:2210`). No GUI include/linking workaround. Qt network/serial capability checks belong to supported backend configuration, not fabricated transport success.

Planned tests (not run during research): new `tst_cat_service_lifecycle` with QCoreApplication, real Role::Local/Remote model, explicit cold construction/start-after-policy hooks, daemon shutdown/handover and repeated start/stop; new `tst_cat_tx_coordinator` verifies session ownership/token/request observation and real interlock/refusal cases; new command contract tests exercise every supported/inert/unavailable row and reject-no-mutation. Existing relevant targets: `tst_mox_controller_ptt_sources`, `tst_mox_controller_ptt_source_dispatch`, `tst_transmit_holder`, `tst_daemon_app`, `tst_desktop_station_controller`, `tst_tx_meter_index`, `tst_tx_meter_reading`, `tst_tx_meter_pump`, `tst_tx_eq_set_curve`, `tst_transmit_model_set_power_using_dbm`, `tst_core_has_no_gui_includes`. Follow fast-test-loop: build selected targets first, never assume ctest rebuilds; do not default to whole suite. QCoreApplication test must link NereusCore and enforce R1; current Core's preexisting Qt Widgets/GuiPrivate dependency comments in CMakeLists2254-2260 are not authorization for new GUI dependencies.

## ZZRM TX subset supplied to receiver worker

Source `CATCommands.cs:6012-6065` queries RX0/1/2/3 when unkeyed, TX4/5/6/7/8 when keyed. Source selector6 is explicitly Error1. Source fixed-width PadLeft20 is stripped by CATParser's TrimStart, then command+selector are prepended. Preserve actual framed suffix echo; do not invent20 wire spaces.

- **4 ALC average:** `TxChannel.h:682-686` typed `txMeter(TxMeterType::AlcAvg)`; `WdspTypes.h:280` maps to actual WDSP TXA_ALC_AV13; `TxChannel.cpp:722-780` schedules lane refresh and reads atomic cache, or performs direct getter only on allowed lane. Source `console.cs:17629-17639` computes `max(-20.0, -CalculateTXMeter(ALC))`; CalculateTXMeter already negates raw, so CAT returns `max(-20.0, float(rawAlcAvg))`, F1+` dB`. Do not use `getAlcMeter()` peak, or return negated CalculateTXMeter without the second negation. Require connected TX channel/initialized DSP, never expose -400/zero cache as unavailable capability success.
- **5 forward /7 reverse watts:** actual `RadioStatus.h:121/124`, `.cpp:96-97` calibrated forwardPowerWatts/reflectedPowerWatts. Source fixed0+` W`. Use existing calibration/scaling, not raw ADC or power slider. Guard connected hardware and correct active TX branch.
- **8 SWR:** actual `RadioStatus.h:128/.cpp:98` swrRatio; current noise-floor/clamp implementation `.cpp:213-245` floors1 and caps99 rather than upstream50. Keep live model behavior as explicit adaptation; format F1+` : 1`.

## Complete covered-family decisions

The following groups cover all172 rows. Each sourceRange is the exact pinned method range already stored per row; where a group has multiple contracts, read the JSON for each action/read distinction and exact widths. All target/read/set decisions have been requalified against current production; source-inert behavior is retained regardless of newer similarly named features.

### Automatic reporting

Commands: `AI`, `ZZAI`. Outcome: Adapted. Global `AiEnabled` and permission `AllowKenwoodAi` preserve Thetis enablement; transport/GUID remains session-owned, but sessions have no independent enabling state.

`AI`: Thetis CAT/CATCommands.cs:105-130 [v2.10.3.15].

Target: CatService global AiEnabled setting and AllowKenwoodAi permission; serial1..4/TCP route options, endpoint channel mappings and actual model frequency/arbiter signals.

Read: AllowKenwoodAi=True: AI0; or AI1; reads actual service-global AiEnabled shared by every endpoint; permissionFalse -> ?; no mutation.

Set: Exactly1 width-valid unsigned numeric suffix with AllowKenwoodAi=True:0 sets global AiEnabled=False, other numeric value True; silent. PermissionFalse or malformed -> ?; no mutation. Enablement applies to every eligible routed endpoint/client, not a session subscription.

Evidence: Settled global Thetis enablement/permission retained. Actual FA/FB11Hz and ZZSW0/1 model changes only, no invented IF burst. Destination route options serial1..4/TCP from console51307-51328;200ms per UID MessageFloodControl53886-53949, implemented as per-channel/per-FA-FB-ZZSW latest-value timer coalescing with injected test clock (no main-thread sleep). Multiendpoint channels map independently but eligible clients receive broadcasts regardless of which session enabled AI; sessions own transport/GUID, no independent AI bit.

`ZZAI`: Thetis CAT/CATCommands.cs:1223-1247 [v2.10.3.15].

Target: CatService global AiEnabled setting and AllowKenwoodAi permission; serial1..4/TCP route options, endpoint channel mappings and actual model frequency/arbiter signals.

Read: AllowKenwoodAi=True: ZZAI0; or ZZAI1; reads actual service-global AiEnabled shared by every endpoint; permissionFalse -> ?; no mutation.

Set: Exactly1 width-valid unsigned numeric suffix with AllowKenwoodAi=True:0 sets global AiEnabled=False, other numeric value True; silent. PermissionFalse or malformed -> ?; no mutation. Enablement applies to every eligible routed endpoint/client, not a session subscription.

Evidence: Settled global Thetis enablement/permission retained. Actual FA/FB11Hz and ZZSW0/1 model changes only, no invented IF burst. Destination route options serial1..4/TCP from console51307-51328;200ms per UID MessageFloodControl53886-53949, implemented as per-channel/per-FA-FB-ZZSW latest-value timer coalescing with injected test clock (no main-thread sleep). Multiendpoint channels map independently but eligible clients receive broadcasts regardless of which session enabled AI; sessions own transport/GUID, no independent AI bit.

### FM TX parameters

Commands: `CN`, `CT`, `OF`, `OS`, `ZZFD`, `ZZOS`, `ZZOT`, `ZZTA`, `ZZTB`, `ZZYC`. Outcomes: Unavailable.

Pinned source: CN 151-154 [v2.10.3.15]; CT 157-160 [v2.10.3.15]; OF 685-688 [v2.10.3.15]; OS 691-694 [v2.10.3.15]; ZZFD 2771-2792 [v2.10.3.15]; ZZOS 5369-5380 [v2.10.3.15]; ZZOT 5384-5400 [v2.10.3.15]; ZZTA 6620-6645 [v2.10.3.15]; ZZTB 6648-6671 [v2.10.3.15]; ZZYC 8503-8526 [v2.10.3.15].

Target: None: no functional production binding.

Current SliceModel.h1294-1302 only stores/signals CTCSS mode/value/offset; no RadioModel subscribers or TxChannel CTCSS/deviation/direction/FM-mic API. New TransmitModel.fmTxOffsetMhz/ForBand (h578-586,cpp1462-1500; RadioModel6981-7005) persists band value but has no runtime frequency-offset/direction binding. All forms unavailable before mutation; no settings-only success.

### Rig identification

Commands: `ID`. Outcomes: Faithful.

Pinned source: ID 291-313 [v2.10.3.15].

Target: CatService compatibility setting RigIdentity (protocol state,not RadioModel).

Setup rig identity changes reported ID only,not command availability

`ID`: ID<configured3digit code>; Set/action: No set form; ?;no mutation Scale: PowerSDR900,TS-50S013,TS-2000019,TS-480020;invalid configured enum default019.

### CWX

Commands: `KS`, `KY`, `ZZKM`, `ZZKO`, `ZZKS`, `ZZKY`, `ZZSS`. Outcomes: Unavailable.

Pinned source: KS 468-499 [v2.10.3.15]; KY 502-562 [v2.10.3.15]; ZZKM 3539-3556 [v2.10.3.15]; ZZKO 3509-3535 [v2.10.3.15]; ZZKS 3559-3579 [v2.10.3.15]; ZZKY 3582-3645 [v2.10.3.15]; ZZSS 6470-6474 [v2.10.3.15].

Target: None: no functional production binding.

Current CwxApplet.cpp22 explicitly all controls NYI; MoxController.cpp2932-2936 still rejects CW PTT; UnbuiltFeatureList.h60 lists Cwx. No functional queue/macros backend; TCI static/stored CW states are not implementation. Reject KY before automatic mode-change side effect.

### Mic gain

Commands: `MG`, `ZZMG`. Outcomes: Adapted.

Pinned source: MG 594-616 [v2.10.3.15]; ZZMG 4177-4206 [v2.10.3.15].

Target: TransmitModel::micGainDb()/setMicGainDb(int); real micPreampChanged wiring.

Legacy micGain(float) is different model field. .NET midpoint-even rounding and asymmetric source scale are explicit Current base evidence: TransmitModel.cpp592-615; RadioModel.cpp7019-7027 actual mic preamp wiring.

`MG`: MG returns actual converted signed value formatted standard width3; ZZMG returns sign+2 zero-padded magnitude; no silent clamp of read state Set/action: MG3 numeric; ZZMG signed/unsigned source allowed3 or source nSet+1 (parser exception must match); clamp to dB range; silent. Reject whole invalid payload before setter Scale: ZZMG dB-50..70; MG input clamp0..100 then .NET midpoint-even round(n/1.43); query midpoint-even round(db/0.7).

`ZZMG`: MG returns actual converted signed value formatted standard width3; ZZMG returns sign+2 zero-padded magnitude; no silent clamp of read state Set/action: MG3 numeric; ZZMG signed/unsigned source allowed3 or source nSet+1 (parser exception must match); clamp to dB range; silent. Reject whole invalid payload before setter Scale: ZZMG dB-50..70; MG input clamp0..100 then .NET midpoint-even round(n/1.43); query midpoint-even round(db/0.7).

### TX monitor

Commands: `MO`, `ZZMO`. Outcomes: Faithful.

Pinned source: MO 619-640 [v2.10.3.15]; ZZMO 4253-4273 [v2.10.3.15].

Target: TransmitModel::monEnabled()/setMonEnabled(bool); RadioModel live AudioEngine wiring.

MO delegates ZZMO; source silently ignores non0/1 width-valid setter Current base evidence: TransmitModel.h1495/cpp2979-2987; RadioModel.cpp7031-7041 real AudioEngine monitor enable.

`MO`: MO0; or MO1; actual monEnabled Set/action: Source width1 suffix0/1 sets boolean; other width-valid numeric suffix returns silent without mutation; bad framing ?; Scale: Boolean0/1.

`ZZMO`: ZZMO0; or ZZMO1; actual monEnabled Set/action: Source width1 suffix0/1 sets boolean; other width-valid numeric suffix returns silent without mutation; bad framing ?; Scale: Boolean0/1.

### TX power

Commands: `PC`, `ZZPC`. Outcomes: Adapted.

Pinned source: PC 698-717 [v2.10.3.15]; ZZPC 5570-5592 [v2.10.3.15].

Target: TransmitModel::power()/setPower(int), powerLimit(), powerSliderLimitEnabled(); RadioModel powerChanged -> drivePowerScroll(); rfPowerSliderMaxFor(HPSDRModel).

Current production limits are live: TransmitModel.h553-576; RadioModel.cpp6981-6996 sets active-band limits and reapplies drive. Exact existing constrain formula TransmitModel.cpp1962-1981 permits read-only derivation. LimitReportedPower True default now functional; no setPowerUsingTargetDbm(false) query, which mutates state. Input SKU clamp remains adapter responsibility because setPower(int) itself does not clamp (cpp499-506).

`PC`: PC<zero-pad3 value>; LimitReportedPower=True: powerSliderLimitEnabled ? min(power(),powerLimit()) : power(); False: power(); read-only actual slider/limit derivation, not measured RF watts Set/action: Exactly3 numeric suffix; clamp0..live ceiling then setPower, silent. Malformed ?; no mutation Scale: Input integer clamp0..100 source then clamp to live SKU rfPowerSliderMaxFor (HL2=90,others100); no guessed step quantization.

`ZZPC`: ZZPC<zero-pad3 value>; LimitReportedPower=True: powerSliderLimitEnabled ? min(power(),powerLimit()) : power(); False: power(); read-only actual slider/limit derivation, not measured RF watts Set/action: Exactly3 numeric suffix; clamp0..live ceiling then setPower, silent. Malformed ?; no mutation Scale: Input integer clamp0..100 source then clamp to live SKU rfPowerSliderMaxFor (HL2=90,others100); no guessed step quantization.

### Legacy processor compatibility

Commands: `PR`. Outcomes: SourceInert.

Pinned source: PR 721-731 [v2.10.3.15].

Target: None; pinned source constant.

Source active read0 only;do not redirect to ZZCP

`PR`: PR0; Set/action: Any setter ?;no mutation Scale: No scaling.

`PR`: PR0; Set/action: Any setter ?;no mutation

### Connection power

Commands: `PS`, `ZZPS`. Outcomes: Adapted.

Pinned source: PS 734-757 [v2.10.3.15]; ZZPS 5706-5728 [v2.10.3.15].

Target: RadioModel::powerOn()/isConnected(); disconnectFromRadio(); tokenized CAT coordinator cleanup.

Thetis softpower separate from connection;Nereus connection ispower. Chosenradio reconnection notavailable;explicit maintainer lockedmixedcontract Current base evidence: RadioModel.h5519 powerOn and real disconnect; same approved on-disconnected failure/no autodiscovery contract retained.

`PS`: PS0; or PS1; reads actual connected state Set/action: Exactly1 suffix0 disconnects actualradio and cleans claims;1 when connected idempotent silent;1 when disconnected ?;no autodiscovery/no mutation;other width-valid numeric suffix silent no mutation,malformed?; Scale: Boolean0/1.

`ZZPS`: ZZPS0; or ZZPS1; reads actual connected state Set/action: Exactly1 suffix0 disconnects actualradio and cleans claims;1 when connected idempotent silent;1 when disconnected ?;no autodiscovery/no mutation;other width-valid numeric suffix silent no mutation,malformed?; Scale: Boolean0/1.

### Quick memory

Commands: `QI`, `ZZQM`, `ZZQR`, `ZZQS`. Outcomes: Unavailable.

Pinned source: QI 760-765 [v2.10.3.15]; ZZQM 5795-5798 [v2.10.3.15]; ZZQR 5801-5805 [v2.10.3.15]; ZZQS 5808-5812 [v2.10.3.15].

Target: None: no functional production binding.

Current production core/models have no quick tuning-memory model; TuneMemoryStore remains tuner solution data, MemoryLock/MemoryPressure RAM. QI/ZZQM/ZZQR/ZZQS every form ?; without session shadow or partial tuning restore.

### TX ownership

Commands: `RX`, `TX`, `ZZTX`. Outcomes: Adapted.

Pinned source: RX 822-827 [v2.10.3.15]; TX 970-975 [v2.10.3.15]; ZZTX 6961-6981 [v2.10.3.15].

Target: CatTxCoordinator session/generation claims -> MoxController::onCatPtt(bool); KeyerIdentity::station(PttMode::Cat) program=true; TxSliceArbiter::requestHandoff(int,const QByteArray&) for station requester.

Current CAT PollPTT keys only from receive, respects manual/source ownership, TX inhibit/PA trip/RXOnly and existing StationServer keying gate (MoxController.cpp2820-2846,1532-1567,1716-1793; StationServer.cpp2477-2760). Coordinator must clear claim/held CAT level after refusal, stop, target loss or supersession; never retain a deferred rekey. Existing identity has no per-CAT token; station deviceId alone cannot distinguish sessions or later source activations. Guard final release with exact owned activation generation/source/live target; never replace keying gate or call RadioModel TCI/button shims.

`RX`: No read form; ?; for invalid suffix Set/action: RX; releases only this session claim; final valid owning claim releases activation; silent; unrelated activation unchanged Scale: No suffix.

`TX`: No read form; ?; for invalid suffix Set/action: TX; acquires claim only idle/compatible same-target claim and confirmed handoff; accepted activation silent, rejected ?;; never direct key bit Scale: No suffix.

`ZZTX`: ZZTX0; or ZZTX1; reads accepted/live CAT activation OR actual MOX, not remembered request Set/action: Suffix0 releases own claim,1 acquires validated claim; accepted silent; other suffix ?; no mutation Scale: Boolean0/1.

### Application lifecycle

Commands: `ZZBY`. Outcomes: Unavailable.

Pinned source: ZZBY 1636-1648 [v2.10.3.15].

Target: None: no functional production binding.

Source Console.Close closes whole GUI;no approved headless/global close API. No QCoreApplication::quit remote hook

`ZZBY`: ?; for read form; no mutation Set/action: ?; for set/action form; no mutation Scale: None.

### CW keyer/display

Commands: `ZZCB`, `ZZCD`, `ZZCF`, `ZZCI`, `ZZCL`, `ZZCM`, `ZZCS`, `ZZQK`. Outcomes: Unavailable.

Pinned source: ZZCB 1651-1667 [v2.10.3.15]; ZZCD 1671-1694 [v2.10.3.15]; ZZCF 1697-1725 [v2.10.3.15]; ZZCI 1728-1750 [v2.10.3.15]; ZZCL 1753-1773 [v2.10.3.15]; ZZCM 1776-1798 [v2.10.3.15]; ZZCS 1879-1900 [v2.10.3.15]; ZZQK 5776-5792 [v2.10.3.15].

Target: None: no functional production binding.

Current MoxController.h1145 documents CW keyer/sidetone/QSK deferred; cpp2932-2936 rejects. CWPitch AppSettings only influences SliceModel filter edges (cpp2322-2353), no functional keyer/pitch/sidetone or CW-frequency-display API. CodecContext raw P2 CW fields have no production model setters; reject every form before mutation.

### TX compressor

Commands: `ZZCP`. Outcomes: Faithful.

Pinned source: ZZCP 1858-1876 [v2.10.3.15].

Target: TransmitModel::cpdrOn()/setCpdrOn(bool).

Real CPDR runtime signal->TxChannel Current base evidence: TransmitModel.h1977/cpp4205-4216; RadioModel.cpp7371-7374 actual CPDR runtime.

`ZZCP`: ZZCP0; or ZZCP1; Set/action: Source width1 suffix0/1 writes; other width-valid value silent no mutation; malformed ?; Scale: Boolean0/1.

### TX compressor level

Commands: `ZZCT`. Outcomes: Faithful.

Pinned source: ZZCT 1903-1926 [v2.10.3.15].

Target: TransmitModel::cpdrLevelDb()/setCpdrLevelDb(int).

Source CPDRMin/Max actual runtime control matches model0..20 Current base evidence: TransmitModel.h1978/cpp4218-4231 clamp0..20dB; RadioModel.cpp7377-7380 actual DSP gain.

`ZZCT`: ZZCT<zero-pad2 actual dB>; Set/action: Exactly2 numeric suffix; clamp0..20; silent; malformed ?; Scale: Clamp0..20dB; source console.Designer.cs6042-6043.

### CPU telemetry

Commands: `ZZCU`. Outcomes: Unavailable.

Pinned source: ZZCU 1929-1934 [v2.10.3.15].

Target: None: no functional production binding.

Current Core has Linux-only HostTelemetrySampler/SharedHostSampler (core/daemon/HostTelemetrySampler.h31-54,100-136; StationTelemetry.h141-142), raw optional system/process CPU, first sample absent and macOS/Windows disabled. No existing RadioModel API for source selectable 0.8/0.2 smoothed CPUPercSmoothed (Thetis console26258-26274); current CAT contract remains ?;, no fake cross-platform CPU0 or undocumented raw-for-smoothed replacement.

`ZZCU`: ?; for read form; no mutation Set/action: ?; for set/action form; no mutation Scale: None.

### DDUtil compound status

Commands: `ZZDU`. Outcomes: Unavailable.

Pinned source: ZZDU 2387-2446 [v2.10.3.15].

Target: None: no functional production binding.

Source2387-2452 composes CWX,display,multiRX+other status into126;required production fields absent. Reject whole read instead of mixfake state

`ZZDU`: ?; for read form; no mutation Set/action: ?; for set/action form; no mutation Scale: None.

### Phone DX

Commands: `ZZDX`. Outcomes: Unavailable.

Pinned source: ZZDX 2454-2467 [v2.10.3.15].

Target: None: no functional production binding.

Current core/models have no independent CATPhoneDX/chkDX state or signal binding. Live CFC profile and CPDR APIs are separate processors (RadioModel.cpp33362-33422/7371-7380), not a DX checkbox substitute. Both forms unavailable.

`ZZDX`: ?; for read form; no mutation Set/action: ?; for set/action form; no mutation Scale: None.

### TX EQ gains

Commands: `ZZEB`. Outcomes: Adapted.

Pinned source: ZZEB 2549-2587 [v2.10.3.15].

Target: TransmitModel::txEqNumBands()=10, txEqPreamp()/setTxEqPreamp(int), txEqBand(i)/setTxEqBand(i,int); existing txEqProfileChanged and RadioModel legacy/parametric binder.

Fix pinned upstream handler creates nb+1 array while eqform.cs2207-2292 reads21 entries. No3-band projection; preserve real frequencies and txEqUseLegacy selection. New production txEqProfileChanged binder RadioModel.cpp33432-33496 routes selected legacy or parametric curve; legacy gain edits remain real legacy-bank state if parametric selected. Fullvalidate before any setter; publish only complete final profile to DSP (coalesce existing synchronous setters on model event-loop turn). No CFC mutation: complete CFC binder33362-33422 is distinct.

`ZZEB`: ZZEB010<11 actual dB fields>; total36 payload; frequencies not included Set/action: Exactly36 chars with bandcount010 and11 valid signed3-char values; fully validate all before setters; update gain values only; silent; bandcount003/other/invalid ?; no mutation Scale: Payload36 chars:010+eleven3-char dB fields(preamp+bands0..9); each clamp-12..15; positive000..015 negative-01..-12.

### Verbose CAT errors

Commands: `ZZEM`. Outcomes: Adapted.

Pinned source: ZZEM 2590-2609 [v2.10.3.15].

Target: CatSession verbose protocol flag backed service compatibility preference.

Session-local adaptation from global source parser prevents oneclient changing otherclients errors;source ProcessError1553-1605 formats verbose strings

`ZZEM`: ZZEM0;or ZZEM1; actual protocol state Set/action: Exactly1 suffix0/1 writes current session flag;silent;other ?;no mutation Scale: Boolean0/1.

### TX EQ enable

Commands: `ZZET`. Outcomes: Faithful.

Pinned source: ZZET 2632-2648 [v2.10.3.15].

Target: TransmitModel::txEqEnabled()/setTxEqEnabled(bool).

Live TxChannel EQ run wiring exists Current base evidence: TransmitModel.cpp3541-3548; RadioModel.cpp7274-7280 and33432-33496 actual selected-curve processing.

`ZZET`: ZZET0; or ZZET1; actual EQ run preference Set/action: Exactly1 suffix0/1; silent; other ?; no mutation Scale: Boolean0/1.

### Legacy model indicator

Commands: `ZZFM`. Outcomes: Adapted.

Pinned source: ZZFM 2918-2937 [v2.10.3.15].

Target: RadioModel::hardwareProfile().model;explicit enum branches.

Do not substitute BoardCapabilities::hasAlex which is option visibility,not installed/present state. Source HPSDR/HERMES depends mutable AlexPresent

`ZZFM`: Connected listed0/1 model ->ZZFM0;or ZZFM1;. HPSDR/HERMES ?;because mutable AlexPresent unavailable;all other models ?; Set/action: No set form; ?;no mutation Scale: ANAN10/10E->0;ANAN100/100B/100D/200D/7000D/8000D/ANVELINAPRO3/G2/G2_1K->1.

### FlexWire inert read

Commands: `ZZFV`, `ZZFW`. Outcomes: SourceInert.

Pinned source: ZZFV 3011-3028 [v2.10.3.15]; ZZFW 3031-3051 [v2.10.3.15].

Target: None;commented upstream hardware read.

Source initializes val0;parser echoes address suffix then00

`ZZFV`: ZZFV<original 2hex suffix>00; Set/action: No set form; ?;no mutation

`ZZFW`: ZZFW<original 2hex suffix>0000; Set/action: No set form; ?;no mutation

### FlexWire inert write

Commands: `ZZFX`, `ZZFY`. Outcomes: SourceInert.

Pinned source: ZZFX 3054-3070 [v2.10.3.15]; ZZFY 3073-3092 [v2.10.3.15].

Target: None;commented upstream hardware write.

Pinned disabled hardware operations deliberately inert

`ZZFX`: No read form; ?; Set/action: Exactly source4/6hex payload;valid silent no mutation;invalid ?;

`ZZFY`: No read form; ?; Set/action: Exactly source4/6hex payload;valid silent no mutation;invalid ?;

### Directed-client registration

Commands: `ZZGA`, `ZZGR`. Outcomes: Adapted.

Pinned source: ZZGA 3101-3117 [v2.10.3.15]; ZZGR 3121-3137 [v2.10.3.15].

Target: CatSession GUID registrations and CatService directed routes;serial validation response without stored registrations.

TCP source303-351 canonical frame;serial parser1535-1544 duplicates command+GUID twice because handler returns40. Normalize serial to TCP canonical as explicit correction;never persist client registrations

`ZZGA`: No read form; ?;no mutation Set/action: Valid36 GUID -> ZZGA<canonical lowercase GUID>; TCP registers/removes ID,ordinary serial validates/replies only;malformed?;no mutation Scale: Exactly36-char GUID canonical lowercase.

`ZZGR`: No read form; ?;no mutation Set/action: Valid36 GUID -> ZZGR<canonical lowercase GUID>; TCP registers/removes ID,ordinary serial validates/replies only;malformed?;no mutation Scale: Exactly36-char GUID canonical lowercase.

### DEXP enable

Commands: `ZZGE`. Outcomes: Faithful.

Pinned source: ZZGE 3140-3162 [v2.10.3.15].

Target: TransmitModel::dexpEnabled()/setDexpEnabled(bool); queued TxChannel::setDexpRun.

Thetis NoiseGate checkbox invokes OtherButtonId.DEXP console.cs28925-28932 Current base evidence: TransmitModel.cpp3101; RadioModel.cpp7416-7420 actual SetDEXPRun binding.

`ZZGE`: ZZGE0; or ZZGE1; actual DEXP preference Set/action: Exactly1 suffix0/1;silent;other ?;no mutation Scale: Boolean0/1.

### Legacy noise gate level

Commands: `ZZGL`. Outcomes: Unavailable.

Pinned source: ZZGL 3165-3197 [v2.10.3.15].

Target: None: no functional production binding.

Source NoiseGate slider-160..0 scroll only labels/focus; no separate production level/readback. VOX threshold is different

`ZZGL`: ?; for read form; no mutation Set/action: ?; for set/action form; no mutation Scale: None.

### Audio/DSP buffer (reconciled RX and audited TX fanout)

`ZZHA` — Unavailable. Thetis CAT/CATCommands.cs:3243-3257 [v2.10.3.15].

Target: None.

Read: Every read form returns ?; without mutation. Return ?; before mutation, no invented readback.

Set: Every setter/action form returns ?; before any mutation. Return ?; before mutation, no feature or session state update.

Scale: None

Evidence: Source audio-backend buffer selection has no equivalent live AudioEngine buffer config/readback; WDSP IQ input buffer is different, no architecture extension. Verified production evidence from receiver worker: AudioEngine.h no selectable backend buffer/readback equivalent to Thetis AudioBufferSize; IQ/WDSP block size not backend size

`ZZHR` — Adapted. Thetis CAT/CATCommands.cs:3260-3275 [v2.10.3.15].

Target: AppSettings::value()/setValue("DspOptionsBufferSizePhoneRx/CwRx/DigRx"); RadioModel::scheduleRemoteDspOptionsApply(const QString&); RxChannel::onModeChanged(DSPMode), dspBlockSize().

Read: If primary currently in group and live WDSP channel exists, query actual dspBlockSize through source Width2Index; otherwise query actual persisted per-group configuration. Source unlisted width returnsindex0 (including existing64), explicitly lossy code.

Set: Require connected local Core and off-air station gate before changing setting. Validate source form and convert source index0..6=>256..16384, other legal single digit=>256. Store genuine global mode-group preference then call existing scheduleRemoteDspOptionsApply(key) so production coalescer applies every live slice in that mode group, with its existing on-air deferral and lane quiescence. Success silence; getter reads actual persisted group preference and source Buffer2Index (64/unlisted=>0), never last request. Resolve stable endpoint incarnation and existing Core ownership/freeze permission first; no implicit creation, control claim, or GUI-focus change.

Scale: Source index0..6->[256,512,1024,2048,4096,8192,16384]samples; source other unsigned1digit defaults256. Existing filter>=buffer clamp retained. Mode groupPhone USB/LSB/DSB/AM/SAM.

Evidence: Existing global per-mode settings plus Core fanout now remove old primary-only apply limitation. Direct rebuildDspOptionsForMode hardcodesRX0 and also TX, so do not use it. Source default remains native64 until CAT setter changes existing preference; no user default change. Verified production evidence from receiver worker: RadioModel.h:4340; RadioModel.cpp:29248-29350 coalesced group fanout; RxChannel.cpp:3433-3466 bufferclamp /3586-3677 laneapply

`ZZHT` — Adapted. Thetis CAT/CATCommands.cs:3278-3293 [v2.10.3.15].

Target: AppSettings::value()/setValue("DspOptionsBufferSizePhoneTx"); RadioModel::scheduleRemoteDspOptionsApply(const QString&); TxChannel::txDspBlockSize()/onModeChanged(DSPMode).

Read: ZZHT<source Width2Index>; if bound TX slice in Phone group and connected initialized TX channel, query actual txDspBlockSize(); otherwise query actual persisted PhoneTx preference (native default64 -> source fallbackindex0). No remembered request echo.

Set: Connected local Core, resolved stable endpoint/owner/freeze permissions and stationOnAirRefusal()==false required before mutation. Exactly1 unsigned decimal digit; map source0..6 and defaultother to256. Store real DspOptionsBufferSizePhoneTx then scheduleRemoteDspOptionsApply(key), silent. Production coalescer defers if keyed before apply; invalid/blocked -> ?; no mutation.

Scale: Source index0..6 -> 256,512,1024,2048,4096,8192,16384 samples; other legal single-digit index ->256. Existing buffer<=filter clamp retained.

Evidence: Current production TX fanout is live: RadioModel.h4340/cpp29248-29313 groups pending TX settings and applies only off-air to bound TX mode; TxChannel.h2197/2212, cpp5632-5693 reads per-mode genuine AppSettings then applies existing lane-quiesced buffer/filter rules. Source/native defaults remain unchanged until setter; no direct rebuildDspOptionsForMode (hardcodesRX0 and TX) and no AudioEngine backend-buffer substitute. Phone group USB/LSB/AM/SAM/DSB; Dig group DIGL/DIGU/SPEC/DRM from TxChannel.cpp5601-5621; FM separate and not mapped here.

`ZZHU` — Adapted. Thetis CAT/CATCommands.cs:3296-3311 [v2.10.3.15].

Target: AppSettings::value()/setValue("DspOptionsBufferSizePhoneRx/CwRx/DigRx"); RadioModel::scheduleRemoteDspOptionsApply(const QString&); RxChannel::onModeChanged(DSPMode), dspBlockSize().

Read: If primary currently in group and live WDSP channel exists, query actual dspBlockSize through source Width2Index; otherwise query actual persisted per-group configuration. Source unlisted width returnsindex0 (including existing64), explicitly lossy code.

Set: Require connected local Core and off-air station gate before changing setting. Validate source form and convert source index0..6=>256..16384, other legal single digit=>256. Store genuine global mode-group preference then call existing scheduleRemoteDspOptionsApply(key) so production coalescer applies every live slice in that mode group, with its existing on-air deferral and lane quiescence. Success silence; getter reads actual persisted group preference and source Buffer2Index (64/unlisted=>0), never last request. Resolve stable endpoint incarnation and existing Core ownership/freeze permission first; no implicit creation, control claim, or GUI-focus change.

Scale: Source index0..6->[256,512,1024,2048,4096,8192,16384]samples; source other unsigned1digit defaults256. Existing filter>=buffer clamp retained. Mode groupCw CWL/CWU.

Evidence: Existing global per-mode settings plus Core fanout now remove old primary-only apply limitation. Direct rebuildDspOptionsForMode hardcodesRX0 and also TX, so do not use it. Source default remains native64 until CAT setter changes existing preference; no user default change. Verified production evidence from receiver worker: RadioModel.h:4340; RadioModel.cpp:29248-29350 coalesced group fanout; RxChannel.cpp:3433-3466 bufferclamp /3586-3677 laneapply

`ZZHV` — Adapted. Thetis CAT/CATCommands.cs:3314-3329 [v2.10.3.15].

Target: AppSettings::value()/setValue("DspOptionsBufferSizePhoneRx/CwRx/DigRx"); RadioModel::scheduleRemoteDspOptionsApply(const QString&); RxChannel::onModeChanged(DSPMode), dspBlockSize() (read-only CW RX alias; no write to TX or RX setting).

Read: ZZHV<source Width2Index of actual CwRx configuration>; identical actual read selection/guard to ZZHU, including native64/unlisted widths ->0; query does not read CwTx.

Set: Exactly1 unsigned decimal digit invokes source Index2Width mapping then returns silence without any mutation; hardware/model/settings/DSP untouched. Other malformed widths/suffix -> ?; no mutation.

Scale: Source index0..6->[256,512,1024,2048,4096,8192,16384]samples; source other unsigned1digit defaults256. Existing filter>=buffer clamp retained. Mode groupCw CWL/CWU.

Evidence: Pinned handler3314-3332 has both CW TX writes commented and query explicitly DSPBufCWRX; preserve harmless inert setter and real CW RX query, not guessed CWTX read. New production CwRx readback follows finalized receiver ZZHU contract RadioModel.h:4340; RadioModel.cpp:29248-29350 coalesced group fanout; RxChannel.cpp:3433-3466 bufferclamp /3586-3677 laneapply

`ZZHX` — Adapted. Thetis CAT/CATCommands.cs:3350-3365 [v2.10.3.15].

Target: AppSettings::value()/setValue("DspOptionsBufferSizeDigTx"); RadioModel::scheduleRemoteDspOptionsApply(const QString&); TxChannel::txDspBlockSize()/onModeChanged(DSPMode).

Read: ZZHX<source Width2Index>; if bound TX slice in Dig group and connected initialized TX channel, query actual txDspBlockSize(); otherwise query actual persisted DigTx preference (native default64 -> source fallbackindex0). No remembered request echo.

Set: Connected local Core, resolved stable endpoint/owner/freeze permissions and stationOnAirRefusal()==false required before mutation. Exactly1 unsigned decimal digit; map source0..6 and defaultother to256. Store real DspOptionsBufferSizeDigTx then scheduleRemoteDspOptionsApply(key), silent. Production coalescer defers if keyed before apply; invalid/blocked -> ?; no mutation.

Scale: Source index0..6 -> 256,512,1024,2048,4096,8192,16384 samples; other legal single-digit index ->256. Existing buffer<=filter clamp retained.

Evidence: Current production TX fanout is live: RadioModel.h4340/cpp29248-29313 groups pending TX settings and applies only off-air to bound TX mode; TxChannel.h2197/2212, cpp5632-5693 reads per-mode genuine AppSettings then applies existing lane-quiesced buffer/filter rules. Source/native defaults remain unchanged until setter; no direct rebuildDspOptionsForMode (hardcodesRX0 and TX) and no AudioEngine backend-buffer substitute. Phone group USB/LSB/AM/SAM/DSB; Dig group DIGL/DIGU/SPEC/DRM from TxChannel.cpp5601-5621; FM separate and not mapped here.

### Rig identification reset

Commands: `ZZID`. Outcomes: Faithful.

Pinned source: ZZID 3369-3383 [v2.10.3.15].

Target: CatService compatibility setting RigIdentity=PowerSDR.

Pinned get-style handler mutates setting and returns empty;not identity read

`ZZID`: ZZID; sets actual compatibility identity PowerSDR900; silent Set/action: Nonempty suffix ?;no mutation Scale: No suffix action.

### Installed-options inert

Commands: `ZZIO`. Outcomes: SourceInert.

Pinned source: ZZIO 3444-3447 [v2.10.3.15].

Target: None;source constant.

Pinned source returns000;no invented hardware options

`ZZIO`: ZZIO000; Set/action: No set form; ?;no mutation Scale: None.

`ZZIO`: ZZIO000; Set/action: No set form; ?;no mutation

### Recording/playback

Commands: `ZZJP`, `ZZJQ`, `ZZJR`, `ZZJS`. Outcomes: Unavailable.

Pinned source: ZZJP 4614-4713 [v2.10.3.15]; ZZJQ 4714-4779 [v2.10.3.15]; ZZJR 4506-4613 [v2.10.3.15]; ZZJS 4495-4504 [v2.10.3.15].

Target: None: no functional production binding.

Current core/session RecordStream is telemetry records, ModMonitorRecord is DSP feedback; neither audio ARP recording/playback slots. No ARP slot engine in core/models. Every JR/JP/JQ/JS form ?; with no stop-before-validation or fake zero busy state.

### PureSignal auto calibration

Commands: `ZZLI`. Outcomes: Faithful.

Pinned source: ZZLI 3865-3889 [v2.10.3.15].

Target: RadioModel::pureSignal() -> PureSignal::isAutoCalEnabled()/setAutoCalEnabled(bool).

PS-A preference differs correctionsBeingApplied and TransmitModel wire mirror Current base evidence: RadioModel.h3235 late-bound PureSignal; PureSignal.h182-183/cpp584-640 actual PS engine preference, not correction mirror.

`ZZLI`: ZZLI0; or ZZLI1; actual coordinator auto-cal preference; null/unsupported ?; Set/action: Exactly1 suffix0/1; live-capable coordinator required; silent;bad/null/unsupported ?;no mutation Scale: Boolean0/1.

### Controller text

Commands: `ZZMF`. Outcomes: Unavailable.

Pinned source: ZZMF 4023-4042 [v2.10.3.15].

Target: None: no functional production binding.

Global titlebar multifunction text30 has no core/model display interface;R1 forbids GUI access

`ZZMF`: ?; for read form; no mutation Set/action: ?; for set/action form; no mutation Scale: None.

### TX meter display selector

Commands: `ZZMT`. Outcomes: Unavailable.

Pinned source: ZZMT 4319-4339 [v2.10.3.15].

Target: None: no functional production binding.

Current MeterModel.h has readings only; RadioModel.h5119/cpp26464-26471 explicitly defers TX display selection/save/restore. Typed TxMeterType readings do not implement mutable GUI TX meter selector. Both forms ?; without shadow enum.

`ZZMT`: ?; for read form; no mutation Set/action: ?; for set/action form; no mutation Scale: None.

### Radio channel memory

Commands: `ZZMV`, `ZZMW`, `ZZMX`, `ZZMY`, `ZZMZ`. Outcomes: Unavailable.

Pinned source: ZZMV 4362-4373 [v2.10.3.15]; ZZMW 4376-4391 [v2.10.3.15]; ZZMX 4394-4415 [v2.10.3.15]; ZZMY 4418-4445 [v2.10.3.15]; ZZMZ 4448-4493 [v2.10.3.15].

Target: None: no functional production binding.

Current core/models have no MemoryList/MemoryRecord store or full channel restoration API. ZZMW source deletes record despite XML label; ZZMX is dispatched at CATParser.cs965-967 (case label has no whitespace); the earlier claimed dispatch defect was an extraction error. Actual native memory storage remains unavailable. Every form ?; before mutation.

### Antenna routing

Commands: `ZZOA`, `ZZOC`. Outcomes: Adapted.

Pinned source: ZZOA 5237-5263 [v2.10.3.15]; ZZOC 5273-5299 [v2.10.3.15].

Target: Endpoint primary / actual TX-bound SliceModel::setRxAntenna()/setTxAntenna(ANT1..3); RadioModel Alex connections; alexController() real read.

Use existing slice routing to clear RX-only mux/reconcile TX owner;read actual Alex state after setter,never key or persist antenna via CAT Current base evidence: SliceModel.h777/780; RadioModel.cpp22190-22262 actual Alex RX/TX routing; AlexController.h261-262 actual antenna read.

`ZZOA`: ZZOA<actual Alex rxAnt/txAnt at resolved band>; missing target/incompatible board ?; Set/action: Exactly1 numeric suffix;clamp1..3;capability/block/missing target validate before intent setter;accepted silent,refusal ?;no mutation Scale: Integer clamp1..3;OA primary band;OC arbiter TX-bound band.

`ZZOC`: ZZOC<actual Alex rxAnt/txAnt at resolved band>; missing target/incompatible board ?; Set/action: Exactly1 numeric suffix;clamp1..3;capability/block/missing target validate before intent setter;accepted silent,refusal ?;no mutation Scale: Integer clamp1..3;OA primary band;OC arbiter TX-bound band.

### Legacy peripheral compatibility

Commands: `ZZOB`, `ZZOD`, `ZZOE`, `ZZOF`, `ZZOG`, `ZZOH`, `ZZOJ`, `ZZOV`, `ZZOW`. Outcomes: SourceInert.

Pinned source: ZZOB 5266-5270 [v2.10.3.15]; ZZOD 5302-5306 [v2.10.3.15]; ZZOE 5309-5313 [v2.10.3.15]; ZZOF 5316-5320 [v2.10.3.15]; ZZOG 5324-5328 [v2.10.3.15]; ZZOH 5331-5335 [v2.10.3.15]; ZZOJ 5337-5341 [v2.10.3.15]; ZZOV 5429-5433 [v2.10.3.15]; ZZOW 5436-5440 [v2.10.3.15].

Target: None;pinned source Error1/code7.

Live source unconditionally rejects;do not activate another similarly named antenna/accessory capability

`ZZOB`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZOD`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZOE`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZOF`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZOG`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZOH`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZOJ`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZOV`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZOW`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

### Aries controller

Commands: `ZZOX`, `ZZOZ`. Outcomes: Unavailable.

Pinned source: ZZOX 4143-4156 [v2.10.3.15]; ZZOZ 4161-4174 [v2.10.3.15].

Target: None: no functional production binding.

Current AlexController.h330/cpp634 documents Aries clamp deferred; no real Aries tune/erase inbound response integration. TGXL/Apollo/tuner APIs are different devices; controller commands remain separate integration and unavailable ordinary endpoints.

### Primary input voltage

Commands: `ZZRV`. Outcomes: Adapted.

Pinned source: ZZRV 6134-6156 [v2.10.3.15].

Target: RadioModel::hardwareProfile().model; RadioModel::paReadings().paVolts optional<double> calibrated volts.

New live readback RadioModel.h1637-1644/cpp8330-8347; RadioConnection.h252-253 caches actual userADC0 volts, cpp252-257 convertMkiiPaVolts. Connection P1.cpp5387/P2 telemetry and RadioModel.cpp19780-19783 wire updates. Do not substitute supply volts or fake zero for absent real sensor.

`ZZRV`: Connected source listed ANAN7000D/ANAN8000D/ANVELINAPRO3/ANAN_G2/ANAN_G2_1K with paReadings().paVolts -> ZZRV<00.0 format>; listed board with absent reading or HPSDR/disconnected -> ?;. Other connected non-HPSDR models -> ZZRV00.0; exactly source inert branch, including models not added to pinned real-reading branch. Set/action: No set form; ?;no mutation Scale: Listed source real-voltage boards: fixed1 decimal with minimum2 integer digits (00.0V); source constant00.0 on other non-HPSDR boards.

### CAT serial identity

Commands: `ZZSN`. Outcomes: Unavailable.

Pinned source: ZZSN 6343-6350 [v2.10.3.15].

Target: None: no functional production binding.

Source Setup txtZZSN is configured9-char text;RadioInfo exposes no hardware serial,Flex derived accessory serial is synthetic incompatible shape. No fabricated serial

`ZZSN`: ?; for read form; no mutation Set/action: ?; for set/action form; no mutation Scale: None.

### TX filter display

Commands: `ZZTF`. Outcomes: Unavailable.

Pinned source: ZZTF 6674-6700 [v2.10.3.15].

Target: None: no functional production binding.

Source ShowTXFilter differs GUI waterfall overlay preference; no core/model interface, R1 forbids GUI include

`ZZTF`: ?; for read form; no mutation Set/action: ?; for set/action form; no mutation Scale: None.

### TX filter

Commands: `ZZTH`, `ZZTL`. Outcomes: Adapted.

Pinned source: ZZTH 6704-6724 [v2.10.3.15]; ZZTL 6748-6768 [v2.10.3.15].

Target: TransmitModel::filterHigh()/setFilterHigh(int), filterLow()/setFilterLow(int); existing filterChanged -> TxChannel queued bandpass.

Actual model maintains low<=high by swap, may alter both cutoffs; preserved current behavior documented Current base evidence: TransmitModel.cpp4273-4300 swap-on-commit; RadioModel.cpp7516 actual mode-aware TX bandpass binding.

`ZZTH`: ZZTH<zero-pad5 actual high>; or ZZTL<zero-pad4 actual low>; Set/action: Source TH5/TL4 numeric; clamp then typed setter, silent; malformed ?; Scale: TH Hz clamp500..20000;TL Hz clamp0..2000; model swap-on-commit when crossing opposite boundary.

`ZZTL`: ZZTH<zero-pad5 actual high>; or ZZTL<zero-pad4 actual low>; Set/action: Source TH5/TL4 numeric; clamp then typed setter, silent; malformed ?; Scale: TH Hz clamp500..20000;TL Hz clamp0..2000; model swap-on-commit when crossing opposite boundary.

### TX inhibit

Commands: `ZZTI`. Outcomes: Faithful.

Pinned source: ZZTI 6727-6745 [v2.10.3.15].

Target: RadioModel::setRxOnly(bool); isRxOnly()/rxOnlySetting(); actual MoxController RXOnly gate.

Current global Core setting and effective receive-only state: RadioModel.h1330-1375, cpp28028-28049; applyTxKeyBlock cpp27954 feeds MoxController::setRxOnly. Covers every key source and forced receive-only SKU; CAT does not substitute a session shadow or merely TxInhibitMonitor. Effective forced-SKU restriction remains even after0.

`ZZTI`: No read form in pinned handler; ?; no mutation Set/action: Exactly suffix0 or1 calls RadioModel::setRxOnly(bool), silent. 1 unkeys through live global gate; all other suffix/empty query ?; no mutation. Scale: Write-only Boolean0/1.

### TX monitor level

Commands: `ZZTM`. Outcomes: Adapted.

Pinned source: ZZTM 6771-6791 [v2.10.3.15].

Target: TransmitModel::monitorVolume()/setMonitorVolume(float).

Existing monitor audio path supported; source TXAF also changes CW sidetone/AF slider unavailable, not reproduced Current base evidence: TransmitModel.h1503/cpp2989-3005; RadioModel.cpp7045-7056 AudioEngine normalized0..1 volume. CW sidetone/global AF-slider side effects remain unavailable.

`ZZTM`: ZZTM<zero-pad3 actual integer>; Set/action: Exactly3 numeric clamp0..100, normalized setter; silent; malformed ?; Scale: Input integer clamp0..100 -> n/100.0; query midpoint-even round(actual*100).

### Tune power

Commands: `ZZTO`. Outcomes: Adapted.

Pinned source: ZZTO 6795-6856 [v2.10.3.15].

Target: TransmitModel::tuneDrivePowerSource(), power()/setPower(int), tuneTxBandKnown(), tunePowerForTxBand()/tunePowerForBand(Band)/setTunePowerForBand(Band,int), tunePower()/setTunePower(int), powerLimit()/powerSliderLimitEnabled()/tunePowerLimit().

Live tune-band/limit API TransmitModel.h553-576,738-755,815-816; RadioModel.cpp6981-7005 refreshes band state. Existing constrain formula cpp1962-1981. Keep real source selection; setTunePowerForTxBand(int) also changes source to TuneSlider so use setTunePowerForBand for selected-source CAT write, preserving tuneDrivePowerSource.

`ZZTO`: ZZTO<zero-pad3 value>; DriveSlider: power(), constrained by powerLimit iff LimitReportedPower and powerSliderLimitEnabled; TuneSlider: actual TX-band tunePowerForTxBand(), constrained by tunePowerLimit iff LimitReportedPower; Fixed: tunePower() always unconstrained. Require known TX band for TuneSlider. Set/action: Exactly3 numeric suffix clamps source+existing model bounds, routes selected source setter; silent; no target/invalid ?; Scale: Source0..100; DriveSlider live ceiling HL2=90 else100; TuneSlider HL2=99 else100; Fixed existing model clampHL2=99 else100.

### TX profile

Commands: `ZZTP`. Outcomes: Adapted.

Pinned source: ZZTP 6860-6878 [v2.10.3.15].

Target: RadioModel::micProfileManager()->profileNames()/activeProfileName(); RadioModel::selectTxProfileForStation(const QString&,QString*,bool takenOnAir=false).

Current production sorted real manifest MicProfileManager.cpp910-920/h114-129; selectTxProfileForStation RadioModel.h3908/cpp6556-6576 validates local role, real name and on-air permission before real profile apply. Source count label is misleading: query selectedindex. One actual bank, no factory/mode-specific catalogue invented. Snapshot sorted bank once per operation; setter uses checked selection and propagates refusal, never bypasses fence by direct manager call. Current base evidence: RadioModel.cpp6556-6576; MicProfileManager.cpp910-920 sorted manifest.

`ZZTP`: ZZTP<zero-pad2 actual active-name index>; missing profile/list or unrepresentable index>=100 ?; Set/action: Exactly2 numeric suffix valid actual index<list size;validate before bool manager apply;success silent;bad/missing ?;no mutation Scale: Numeric index0..count-1 current sorted real QStringList; no phantom factory profiles.

### Legacy Flex temperature

Commands: `ZZTS`. Outcomes: SourceInert.

Pinned source: ZZTS 6881-6905 [v2.10.3.15].

Target: RadioModel::hardwareProfile().model; no real ADC read.

Source disabled ADC read,val0->301;do not report actual temperature under this inert command

`ZZTS`: HERMES->ZZTS00301; other model ?; (source verbose code7) Set/action: No set form; ?;no mutation Scale: Source HERMES only.

`ZZTS`: HERMES->ZZTS00301; other model ?; (source verbose code7) Set/action: No set form; ?;no mutation

### Tune ownership

Commands: `ZZTU`. Outcomes: Adapted.

Pinned source: ZZTU 6908-6932 [v2.10.3.15].

Target: CatTxCoordinator -> RadioModel::setTune(bool,const KeyerIdentity&); isTune(); KeyerIdentity::station(PttMode::Manual) with program=true.

New keyed overload RadioModel.h5134/cpp26325-26336; admission cpp26403-26430 before tune state/mode/power mutation. program=true keeps CAT a program request that cannot take another device transmit. Off API stops any current tune, so coordinator verifies exact activation generation/live ownership first; on/refusal/disconnect cleanup must not unkey a later operator/remote tune. Existing settling/restoration cpp27776 onwards retained.

`ZZTU`: ZZTU0; or ZZTU1; reads actual tune lifecycle Set/action: 0 releases own tune claim;1 acquires validated tune claim then real orchestrator; accepted silent, refusal ?; no mutation Scale: Boolean0/1.

### Transverter catalogue

Commands: `ZZUA`. Outcomes: Unavailable.

Pinned source: ZZUA 6984-6988 [v2.10.3.15].

Target: None: no functional production binding.

Source console.cs9655 enumerates14 enabled transverter labels each left-pad5; no transverter catalogue/enable/label model. Generic Band::XVTR seed invalid, not14 configurations

`ZZUA`: ?; for read form; no mutation Set/action: ?; for set/action form; no mutation Scale: None.

### External PA OC gate

Commands: `ZZUP`. Outcomes: Unavailable.

Pinned source: ZZUP 7014-7041 [v2.10.3.15].

Target: None: no functional production binding.

No equivalent Thetis CATxPA configured OC TX pin gate; accessory operate/interlock fields are different

`ZZUP`: ?; for read form; no mutation Set/action: ?; for set/action form; no mutation Scale: None.

### PureSignal single calibration

Commands: `ZZUS`. Outcomes: Faithful.

Pinned source: ZZUS 7043-7047 [v2.10.3.15].

Target: RadioModel::pureSignal()->PureSignal::singleCalibrate().

Does not fabricate auto-cal state; existing coordinator handles lifecycle Current base evidence: PureSignal.h169/cpp473 actual calibration entry; no calibration/read state invented.

`ZZUS`: ZZUS; invokes calibration only when live capable coordinator;accepted silent; unavailable ?; Set/action: No numeric set form; nonempty suffix ?;no mutation Scale: No suffix action.

### Two-tone ownership

Commands: `ZZUT`. Outcomes: Adapted.

Pinned source: ZZUT 7049-7077 [v2.10.3.15].

Target: CatTxCoordinator -> TwoToneController::setActive(bool,const KeyerIdentity&), isActive(), isActivationInFlight(), keyer(); station identity sourceNone with program=true.

New overload TwoToneController.h314/cpp197-204, actual admission cpp252 occurs before start; program=true preserves station program refusal/holder fence. Off stops any current test; coordinator must match exact owned generation/live keyer and cancel pending start before releasing. Never call RadioModel::setMoxFromButton or TransmitModel mirror; activation/refusal signals determine truthful result.

`ZZUT`: ZZUT0; or ZZUT1; actual controller state; null coordinator ?; Set/action: 0 releases own two-tone claim;1 validated acquire+real setActive; silent when accepted, null/refusal ?; no mutation Scale: Boolean0/1.

### VAC audio

Commands: `ZZVA`, `ZZVB`, `ZZVC`, `ZZVD`, `ZZVF`, `ZZVH`, `ZZVI`, `ZZVJ`, `ZZVK`, `ZZVM`, `ZZVO`, `ZZVP`, `ZZVQ`, `ZZVR`, `ZZVT`, `ZZVU`, `ZZVV`, `ZZVW`, `ZZVX`, `ZZVY`, `ZZVZ`, `ZZYA`, `ZZYB`. Outcomes: Unavailable.

Pinned source: ZZVA 6991-7013 [v2.10.3.15]; ZZVB 7085-7117 [v2.10.3.15]; ZZVC 7124-7156 [v2.10.3.15]; ZZVD 7163-7246 [v2.10.3.15]; ZZVF 7322-7344 [v2.10.3.15]; ZZVH 7381-7403 [v2.10.3.15]; ZZVI 7406-7420 [v2.10.3.15]; ZZVJ 7423-7444 [v2.10.3.15]; ZZVK 7447-7469 [v2.10.3.15]; ZZVM 7577-7591 [v2.10.3.15]; ZZVO 7602-7616 [v2.10.3.15]; ZZVP 7619-7641 [v2.10.3.15]; ZZVQ 7644-7658 [v2.10.3.15]; ZZVR 7661-7675 [v2.10.3.15]; ZZVT 7694-7708 [v2.10.3.15]; ZZVU 7715-7804 [v2.10.3.15]; ZZVV 7810-7832 [v2.10.3.15]; ZZVW 7834-7866 [v2.10.3.15]; ZZVX 7873-7905 [v2.10.3.15]; ZZVY 7912-7959 [v2.10.3.15]; ZZVZ 7966-8013 [v2.10.3.15]; ZZYA 8453-8475 [v2.10.3.15]; ZZYB 8478-8500 [v2.10.3.15].

Target: None: no functional production binding.

Current StationVaxFacade.h14-28 exposes four VAX outputs and single TX level; nereusd publishes none (DaemonApp.cpp273). No approved two-VAC-pair topology/driver/device index/IQ correction adapter. AudioEngine.cpp3141-3154 RX gain clamps0..1, cannot represent source+40dB; cpp3170-3176 TX gain storage+signal only, header993-994/1497-1501 marks unapplied and no production gain consumer found. All source VAC forms remain unavailable; new remote/local VAX routing does not establish VAC semantic equivalence.

### VOX enable

Commands: `ZZVE`. Outcomes: Faithful.

Pinned source: ZZVE 7249-7270 [v2.10.3.15].

Target: TransmitModel::voxEnabled()/setVoxEnabled(bool); MoxController listening gates.

Existing real VOX path; active keying still follows tokenized CAT/non-CAT ordering policy Current base evidence: TransmitModel.cpp3025; RadioModel.cpp2704/18727 and7061 onward actual mode/VOX threshold/run/DEXP key callback wiring.

`ZZVE`: ZZVE0; or ZZVE1; actual enable preference Set/action: Exactly1 suffix0/1;silent; other ?; no mutation Scale: Boolean0/1.

### VOX sensitivity

Commands: `ZZVG`. Outcomes: Faithful.

Pinned source: ZZVG 7347-7378 [v2.10.3.15].

Target: TransmitModel::voxThresholdDb()/setVoxThresholdDb(int).

Thetis console.VOXSens is threshold not voxGainScalar; source range -80..0 verified Designer6018-6019 Current base evidence: TransmitModel.cpp3038-3062; RadioModel.cpp2732 and7061 actual dB-to-linear attack threshold path.

`ZZVG`: ZZVG<zero-pad4 actual inverse-scaled threshold>; Set/action: Exactly4 numeric suffix; threshold conversion then setter;silent;malformed ?; Scale: n clamp0..1000; dB=int truncation toward0(-80+80*n/1000.0); read int((db+80)*1000/80).

### Application version

Commands: `ZZVN`, `ZZZV`. Outcomes: Adapted.

Pinned source: ZZVN 7595-7599 [v2.10.3.15]; ZZZV 8653-8662 [v2.10.3.15].

Target: QCoreApplication::applicationVersion().

Source Common.GetFileVersion derives full executing assembly FileVersion,Common.cs661-690;source parser matching nAns12 would leave longer unframed,correct to full Nereus frame

`ZZVN`: ZZVN<actual version padded at least12>; parser explicit formatter permits longer actual version without truncation Set/action: No set form; ?;no mutation Scale: Actual Nereus dotted version PadLeft12 with0; no Thetis version spoof.

`ZZZV`: ZZZV<actual Nereus version without semicolons>; Set/action: No set form; ?;no mutation Scale: Strip semicolons.

### Legacy Flex mixer

Commands: `ZZWA`, `ZZWB`, `ZZWC`, `ZZWD`, `ZZWE`, `ZZWF`, `ZZWG`, `ZZWH`, `ZZWJ`, `ZZWK`, `ZZWL`, `ZZWM`, `ZZWN`, `ZZWO`, `ZZWP`, `ZZWQ`, `ZZWR`, `ZZWS`, `ZZWT`, `ZZWU`, `ZZWV`, `ZZWW`. Outcomes: SourceInert.

Pinned source: ZZWA 8016-8020 [v2.10.3.15]; ZZWB 8023-8027 [v2.10.3.15]; ZZWC 8030-8034 [v2.10.3.15]; ZZWD 8037-8041 [v2.10.3.15]; ZZWE 8045-8049 [v2.10.3.15]; ZZWF 8052-8056 [v2.10.3.15]; ZZWG 8059-8063 [v2.10.3.15]; ZZWH 8066-8070 [v2.10.3.15]; ZZWJ 8074-8078 [v2.10.3.15]; ZZWK 8081-8085 [v2.10.3.15]; ZZWL 8088-8092 [v2.10.3.15]; ZZWM 8095-8099 [v2.10.3.15]; ZZWN 8102-8106 [v2.10.3.15]; ZZWO 8109-8113 [v2.10.3.15]; ZZWP 8116-8120 [v2.10.3.15]; ZZWQ 8123-8127 [v2.10.3.15]; ZZWR 8130-8134 [v2.10.3.15]; ZZWS 8137-8141 [v2.10.3.15]; ZZWT 8145-8149 [v2.10.3.15]; ZZWU 8152-8156 [v2.10.3.15]; ZZWV 8159-8163 [v2.10.3.15]; ZZWW 8166-8170 [v2.10.3.15].

Target: None;pinned source Error1/code7.

Every pinned live handler unconditionally rejects;Nereus PC mic/speaker volume are different capabilities

`ZZWA`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWB`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWC`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWD`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWE`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWF`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWG`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWH`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWJ`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWK`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWL`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWM`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWN`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWO`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWP`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWQ`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWR`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWS`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWT`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWU`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWV`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

`ZZWW`: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation Set/action: ?; source verbose mode ZZEM:<code><suffix>:Feature Not Available; no mutation

### Hardware audio amplifier

Commands: `ZZXA`. Outcomes: Unavailable.

Pinned source: ZZXA 8730-8747 [v2.10.3.15].

Target: None: no functional production binding.

Current core/models have no Thetis EnableAudioAmplifier register setter. BoardCapabilities HasAudioAmplifier comments describe hardware only; speaker mute/monitor level differ. Both forms unavailable.

`ZZXA`: ?; for read form; no mutation Set/action: ?; for set/action form; no mutation Scale: None.

### VOX hang

Commands: `ZZXH`. Outcomes: Adapted.

Pinned source: ZZXH 8243-8269 [v2.10.3.15].

Target: TransmitModel::voxHangTimeMs()/setVoxHangTimeMs(int).

Input4000 reads2000;input0 reads1. Do not widen existing DSP parameter range Current base evidence: TransmitModel.cpp3064 clamps1..2000ms; RadioModel.cpp2773 forwards hang; source0..4000 remains explicit adaptation.

`ZZXH`: ZZXH<zero-pad4 actual ms>; Set/action: Exactly4 numeric suffix; clamp source then model;silent; malformed ?; Scale: Source0..4000ms then existing model clamp1..2000ms.

### Ganymede controller

Commands: `ZZZA`. Outcomes: Unavailable.

Pinned source: ZZZA 4128-4138 [v2.10.3.15].

Target: None: no functional production binding.

Current RadioModel::handleGanymedeTrip(int) is functional and feeds actual MoxController::setPaTripped every asserted message (RadioModel.cpp25833-25873; global applyTxKeyBlock27954). Old safety absence is superseded. This controller-owned incoming message remains unavailable on ordinary serial/TCP CAT by approved separate-integration scope; do not permit arbitrary clients to impersonate a Ganymede protection device or clear real trips.

`ZZZA`: ?; for read form; no mutation Set/action: ?; for set/action form; no mutation Scale: None.

### Andromeda controller

Commands: `ZZZD`, `ZZZE`, `ZZZP`, `ZZZS`, `ZZZU`. Outcomes: Unavailable.

Pinned source: ZZZD 4046-4056 [v2.10.3.15]; ZZZE 4088-4103 [v2.10.3.15]; ZZZP 4107-4124 [v2.10.3.15]; ZZZS 4074-4084 [v2.10.3.15]; ZZZU 4060-4070 [v2.10.3.15].

Target: None: no functional production binding.

Current core/models expose no attached Andromeda identity/front-panel button/encoder/LED dispatch. Controller-owned protocol must be a separate integration; reject ordinary endpoint forms before mutation.

### Physical model identity

Commands: `ZZZM`. Outcomes: Faithful.

Pinned source: ZZZM 8642-8651 [v2.10.3.15].

Target: RadioModel::hardwareProfile().model; CatModelAdapter explicit HPSDRModel enum-name switch.

Do not use translated displayName marketing string or default preconnect HERMES as actual hardware

`ZZZM`: Connected valid model ->ZZZM<actual enum token>; disconnected/unknown sentinel ?; Set/action: No set form; ?;no mutation Scale: Exact enum spellings HPSDR,HERMES,ANAN10,ANAN10E,ANAN100,ANAN100B,ANAN100D,ANAN200D,ORIONMKII,ANAN7000D,ANAN8000D,ANAN_G2,ANAN_G2_1K,ANVELINAPRO3,HERMESLITE,REDPITAYA,ANAN_G2E.

### MIDI integration

Commands: `ZZZN`, `ZZZO`, `ZZZW`. Outcomes: Unavailable.

Pinned source: ZZZN 8683-8703 [v2.10.3.15]; ZZZO 8705-8728 [v2.10.3.15]; ZZZW 8664-8681 [v2.10.3.15].

Target: None: no functional production binding.

Current core/models contain no Midi2Cat wheel/quick-split controller integration. Pinned source requires that controller; standard CAT slice split does not satisfy this capability. Reject both/action forms.

### Endpoint lifecycle

Commands: `ZZZZ`. Outcomes: Adapted.

Pinned source: ZZZZ 8552-8556 [v2.10.3.15].

Target: CatService requesting ordinary serial endpoint stop/claims cleanup.

Source closes firstSiolisten indiscriminately;adapt requesting serial only,not unrelated otherendpoint

`ZZZZ`: Serial ZZZZ; requests orderly current endpoint close after pending response boundary;silent. TCP ?;no mutation Set/action: Nonempty suffix ?;no mutation Scale: No suffix action.

## Remaining blockers

No covered command remains unreviewed or delegated to a future mapping pass. The proposed observation/off guards, CAT service implementation, calls at the verified host-ready desktop hook and their tests are planned work, not existing production API. Unsupported CWX, VAC semantic adaptation, radio/quick memory, ARP recording and ordinary-endpoint controller integrations remain explicit `Unavailable` contracts; their separate feature work is outside this CAT scope. Upstream source notices/comments/constants must be preserved in actual ports, with HOW-TO-PORT provenance and inline-tag checks in the same introduction commit.

Task4 source-study resolution (2026-10-04): the approved plan uses station Manual/program identities for tune and two-tone, station Cat/program for PTT, and bool-bearing accepted-intent observation. Its narrow exact-tag held-CAT discard and cycle continuation guards supersede earlier proposals here; existing permission identities, gates and global shutdown authority remain in charge.
