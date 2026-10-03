# Remote Daemon R2 and R3: Design Addendum

**Status:** Design addendum, pending approval.
**Date:** 2026-08-03
**Author:** J.J. Boyd (KG4VCF), with AI-assisted drafting via Anthropic Claude Code

> **Parent:** [2026-07-28-remote-daemon-architecture-design.md](2026-07-28-remote-daemon-architecture-design.md)
> **Sibling:** [2026-08-02-remote-station-identity-and-pairing-design.md](2026-08-02-remote-station-identity-and-pairing-design.md)
> **Predecessor:** [2026-08-02-remote-daemon-r1-plan.md](2026-08-02-remote-daemon-r1-plan.md), complete.
>
> The parent specifies R2 in prose across sections 6, 7 and 15 and leaves six
> things unresolved that an implementer would have to invent mid-task. This
> document closes them. Every claim below was re-derived against the R1 branch
> tip `a7324cdd`, which is the merge of `origin/main` into remote-daemon R1 and
> is R2's base.

---

## 1. Release framing: R2 does not ship, R2 plus R3 does

**Decision, 2026-08-03.** The release unit is **R2 plus R3**, delivering
internet-capable remote receive. The planning unit is one phase at a time.

R2 on its own produces a working control surface with a **blank panadapter, a
blank waterfall, and silent speakers**, because spectrum endpoints and codecs
are R3 deliverables by parent section 15. That is not a shippable artifact and
must not be treated as one.

Three consequences the R2 plan has to carry:

1. **No R2 task may optimise for standing alone.** No interim wire format, no
   placeholder rendering, no temporary UI that R3 deletes.
2. **The R2 acceptance run is an engineering gate, not a release candidate.**
   Testers are told in advance that the display is blank and the speakers are
   silent, so a blank display is not filed as a defect.
3. **A raw uncompressed media path was considered and rejected.** Uncompressed
   reduced spectrum is roughly 1 Mbit/s per panadapter and uncompressed 48 kHz
   mono PCM roughly 770 kbit/s, which is free on a LAN and would have made R2
   demonstrable months earlier. It was rejected because it ships a throwaway
   wire format on the critical path and lands a LAN-only product whose first
   real-network test comes later. The codec type is still negotiated from day
   one so R3 adds `opus` and the display codec as negotiated types rather than
   as a wire break.

---

## 2. R2 scope, stated as what the operator sees

**What R2 builds.** `StateMirror` (property reflection over `QMetaObject` with
object identity, direction classification and lifecycle), `SettingsProxy`
(classification, bulk snapshot, write-through), the section 7.0 handshake with
capability exchange and token authentication, TLS, and a plain WebSocket
transport with no ICE and no codecs.

**What the demo is.** Two processes on one host. `nereusd` holds the
`RadioConnection`, the `WdspEngine`, the audio device and the FFT pool. The GUI
launches with `--station wss://localhost:PORT` and constructs a `RadioModel`
that never calls `connectToRadio`.

On screen the operator turns the VFO and a real radio retunes. Band, mode,
filter, AGC, NR, NB, SNB, APF, squelch, RIT and XIT, antenna, add slice, remove
slice, TX slice display, and every Setup page reading station settings all work,
and the daemon's resulting state comes back and moves the GUI. The per-slice
S-meter needles are live.

**What the demo is not.** No spectrum trace, no waterfall, no sound, no MOX (TX
is R4 in its entirety), no spot lists over the link, no station discovery or
pairing UI, no meter rate negotiation.

**Local direct mode is untouched.** Every existing `RadioModel` construction
site, including `MainWindow.cpp:452` and `DaemonApp.cpp:52`, keeps its current
behaviour, because the new `Role` parameter defaults to `Local` and every
remote branch is gated on `Role::Remote`.

### 2.1 The trap that would let R2 pass with its deliverable missing

`nereusd` currently defaults to the **shared** settings directory:
`server_main.cpp:193-195` calls `AppSettings::setProfileOverride` only when
`--profile` is non-empty, and `DaemonConfig.cpp:127-143` returns empty for an
absent argument, with `DaemonConfig.h:108-113` calling that deliberate.

On one host both binaries therefore resolve to the same `NereusSDR.settings`
file, so a remote-mode GUI would read the station's real settings correctly
**with `SettingsProxy` entirely unimplemented**. R2 could pass its own
verification and the gap would first appear on a genuinely remote client in R5.

Reserving a daemon profile is therefore task 1, not packaging polish.

---

## 3. Corrections to the parent design, with evidence

**Property counts are wrong in both directions.** A multi-line-aware parse of
the six model headers gives **148** `Q_PROPERTY` declarations, not 139, and
**24** without a WRITE accessor, not 28. Per model: `SliceModel` 107 with 3
read-only (`active` at `SliceModel.h:174`, `txSlice` at `:175`, `sliceLetter`
at `:181`), `TransmitModel` 15 with **0**, `TunerModel` 13 with 13,
`RadioModel` 5 with 4, `MeterModel` 4 with 4, `PanadapterModel` 4 with 0. The
doc's `SliceModel` row (98 and 4) and `TransmitModel` row (15 and 3) are both
wrong. Live instance count is roughly **570** on a 5-slice SKU, not 500. Hermes
Lite 2 is a 5-slice SKU too (`BoardCapabilities.cpp:842`).

**`MeterModel` is dead code, and mirroring it does not settle section 8.1.** Its
four setters have zero callers anywhere in `src` or `tests`; the connect meant
to write it is an empty lambda with four `Q_UNUSED` parameters at
`RadioModel.cpp:8601-8611`. Mirroring it would ship four construction defaults
(`MeterModel.h:36-39`) forever. Live TX and PA telemetry lives on
`RadioStatus`, which declares zero `Q_PROPERTY`, is invisible to `StateMirror`,
and is absent from section 6.1's coverage table. Drop `MeterModel` from that
table and add a `RadioStatus` row.

**Section 7.1's `requestHandoff` claim is false and cites the wrong line.**
`TxSliceArbiter.h:59` is part of `setMacAddress` and `load()`, describing a
legacy key migration. `requestHandoff` is declared at `TxSliceArbiter.h:91` as
`bool requestHandoff(int sliceId)`, documented at `:87-88` as taking "the stable
sliceId", and `TxSliceArbiter.cpp:200-212` shows the positional key was already
migrated to `hardware/<mac>/TxBoundSliceId`. The arbiter already complies with
the rule section 7.1 says it violates.

**Section 6.1's structural-gap cites are stale but the substance holds.**
`RadioModel::slices()` is at `RadioModel.h:487`, not `:446`. `sliceAdded` and
`sliceRemoved` are at `:2306-2307`, not `:2184-2185`. All three gaps are real:
`SliceModel` has no `sliceIndex` `Q_PROPERTY` (plain accessor at
`SliceModel.h:477`), `slices()` is a plain getter, and neither lifecycle signal
is any property's NOTIFY.

**Section 6.2's file total is wrong:** 593 `AppSettings::instance()` call sites
across **92** files, not 88.

**Section 6.1's echo-suppression sentence names a pattern that does not exist in
the model layer.** All 545 `QSignalBlocker` uses and all 22 files carrying
`m_updatingFromModel` are under `src/gui`; `blockSignals` over `src/core` and
`src/models` returns nothing. Echo suppression is in fact largely structural:
`SliceModel`'s setters are uniformly change-guarded (`setFrequency` at
`SliceModel.cpp:199-217` via `qFuzzyCompare`, `setDspMode`'s guarded emit at
`:406-411`), so applying an already-current value produces no NOTIFY and a loop
terminates in one hop.

**Section 8.4's two named hazards are both wrong about the current code.**
Entering RADE does **not** swap the DSP channel object: `SliceModel.cpp:236`
states "WDSP RxChannel stays alive in EVERY mode" and `:239` that "RadeChannel
is created ALONGSIDE RxChannel", with `:231-232` recording that the swap was the
original Phase 3R design and that K-bench replaced it before v0.5.0. Nothing is
inserted into the TX path either: `RadeTxHpf80` and `RadeTx48to16` are permanent
members at `TxWorkerThread.h:372-373`, selected per block by a
`std::atomic<TxPath>` at `:442`. The real hazard is the opposite: that atomic is
written from exactly one site, a `moxStateChanged` lambda that early-returns on
release (`RadioModel.cpp:7927-7947`), so a mode change made while keyed never
reaches the TX path at all.

**Section 9.4's per-band grid resolution has no mechanism under it.**
`SliceModel::bandChanged` is a plain signal at `SliceModel.h:920` and
`SliceModel` has no `band` property, so a property-enumerating mirror cannot see
it. R2 closes this with a one-line
`Q_PROPERTY(NereusSDR::Band band READ band NOTIFY bandChanged)` over the
existing `m_currentBand`, which `setFrequency` already maintains at
`SliceModel.cpp:211-214`.

**Section 6.1a's "the fault log is the same shape" is false.** `FaultLog` is a
10-entry prepend-only ring (`FaultLog.cpp:19`, `:33-40`) persisted as one
AppSettings JSON string (`:92-108`), emitting a single coarse `changed()`
carrying no record (`FaultLog.h:59`). It exposes no deltas. It is
settings-shaped, and it needs a public reload plus defined ordering against
`RadioModel` construction before `SettingsProxy` can carry it, because `load()`
is private and called only from the constructor.

**Section 6.3's non-partitionability example is the wrong one.** `Slice<N>/` and
`hardware/<mac>/` are two clean prefixes that both classify daemon-side, so they
refute nothing. The real counterexamples straddle the process boundary:
`DisplayFftSize`, `DisplayFftWindow`, `DisplayHzPerBinTarget` and
`DisplaySpectrumFps` feed the daemon-side `FftEnginePool`
(`MainWindow.cpp:1476-1501`) while `DisplayNoiseFloorColor`
(`SpectrumWidget.cpp:926`) is client render state; `TciServerPort` configures
`src/core/TciServer.cpp` while `TciLogWindowGeometry` is client window state.
Same prefix, opposite sides. The conclusion stands, the reason must change. Also
fix the cite: the live key is `hardware/%1/TxBoundSliceId`
(`TxSliceArbiter.cpp:192`); `TxBoundSliceIndex` is only a legacy read fallback.

**Section 6.4's "blocked on 6.1a" conflates two things.** All six spot-client
sources are in `CORE_SOURCES` (`CMakeLists.txt:587-593`), all four record models
in `MODEL_SOURCES` (`:714-718`), and `nereusd` links `NereusCore` alone
(`:1145`), so the daemon instantiates every collector today. What is blocked is
**client visibility**, not daemon placement. Two real daemon defects were found
and are unrelated to the mirror:
`RadioModel::restoreSpotClientAutoStartState` (`RadioModel.cpp:2343`) has one
production caller and it is `MainWindow.cpp:771`, so `nereusd` builds seven
collectors and starts none; and `FreeDVStationModel` has no expiry in core at
all, its only sweeper being `FreeDVReporterDialog::onIdleSweepTick`. **File both
as issues now.**

**Sections 7.2 and 7.3 contradict each other on meter reliability.** Line 925
puts meters on "coalesced, reliable"; line 991 puts them on the unreliable Media
envelope with a rationale at 994. Section 7.3 is the later reasoned statement
and governs from R3 onward. Moot for R2, which has only a reliable transport.

**Section 15 R1 says "five `addSliceOnPan` call sites in MainWindow.cpp".**
There are four: `:2223`, `:5586`, `:9259`, `:9285`.

---

## 4. Client architecture: role, null local backend, four suppressed authorities

**R2 does not subclass `RadioModel` and does not build a general
`IRadioBackend`.** `RadioModel.h` is 3940 lines and `RadioModel.cpp` 15425;
subclassing means choosing which of hundreds of members become virtual and
creates a second class whose eventual deletion is itself the section 5 collapse.
A per-parameter backend is the 300-to-500-verb design section 6 already rejects,
and the TCI `Q_INVOKABLE` block at `RadioModel.h:1928-2203` shows what that
looks like at 85 verbs while still covering a fraction, positionally, in a way
section 7.1 forbids.

**The mechanism is a null local backend plus a narrow station link, and most of
it is already true.** `RadioModel` gains `enum class Role { Local, Remote }`
with a constructor overload defaulting to `Local`. In `Remote` role it never
calls `connectToRadio`, and that single omission already yields a DSP-free,
socket-free model: `RadioConnection` is created only at `RadioModel.cpp:6321`,
`RxDspWorker` only at `:8491`, `WdspEngine::initialize` only at `:6288`,
`AudioEngine::start` only at `:6285`, and `wireSliceSignals` returns at
`:9429-9431` when `m_connection` is null, so none of its 87 `connect(slice, ...)`
lambdas and none of its 72 `rxChannel` touches install.

**The quantitative case.** The GUI binds to `SliceModel` in 466 references
across 40 files, 126 `connect` statements naming a `SliceModel` signal, and 402
member calls through slice pointers, all of which keep compiling unchanged. The
GUI's direct reach-throughs into DSP-owning objects total roughly **77** call
sites across 14 files (`connection()` 34 in 8 files, `wdspEngine()` 17 in 3,
`audioEngine()` 17 in 7, `receiverManager()` 9 in `MainWindow`). 77 against 402
is why mirroring the models and nulling the backend beats abstracting the model
layer.

### 4.1 Four local authorities keep running and each fights the daemon

**This is the most important correction to make before writing code.** "Null
connection means inert" is not true.

**First and worst, the stream allocator.** `RadioModel::addSlice` wires a
`frequencyChanged` handler **unconditionally, outside `wireSliceSignals`**, at
`RadioModel.cpp:4658-4688`. It calls `bindSliceToStream`, whose only bail-out is
a stream-pool size check at `:4215`. Applying station capabilities calls
`configureStreamPool`, which sizes the allocator at `:3054`. From that moment an
inbound mirrored frequency delta makes the client run its own
`SliceStreamAllocator`: it overwrites `setStreamIndex` (`:4354`),
`setShiftOffsetHz` (`:4355`) and `setSampleRateHz`, which are three of the eight
never-apply-inbound properties; it calls `syncReceiverToStream` and
`ReceiverManager::forceHardwareFrequency`; and on disagreement it **rolls the
operator's VFO back** to a locally computed value and emits
`sliceRetuneRejected` (`:4677-4681`). In `Remote` role `bindSliceToStream` must
return false immediately and the allocator must never be sized.

**Second, `SliceModel`'s one reach-through into the DSP engine.** `setDspMode`
does `qobject_cast<RadioModel*>(parent())` at `SliceModel.cpp:290` and creates or
destroys a real `RadeChannel` through roughly `:344`.
`WdspEngine::createRadeChannel` has no `isInitialized` guard
(`WdspEngine.cpp:659-684`), so a mirrored RADE mode delta would construct a live
vocoder and call `start()` on a machine with no DSP role. This is the **only**
model-to-engine reach-through anywhere in `src/models` outside `RadioModel`, so
one role check at `:291` closes the class.

**Third, `TxSliceArbiter`.** Constructed unconditionally at `RadioModel.cpp:767`,
runs from `addSlice` and `removeSlice`, and writes `SliceModel::setTxSlice` at
`TxSliceArbiter.cpp:95`, `:127`, `:178`, `:181`. On a client a mirrored slice-add
would make it independently pick a TX slice, while `txSlice` is a no-WRITE
property so the mirror cannot apply the daemon's answer over it.

**Fourth, the spot collectors.** `MainWindow.cpp:771` calls
`restoreSpotClientAutoStartState` at GUI startup with no dependence on
connection state, and `RadioModel`'s constructor has already built
`DxClusterClient` (`:1670`), the RBN instance (`:1675`), `WsjtxClient` (`:1677`),
`SpotCollectorClient`, `PotaClient`, `FreeDVReporterClient` (`:1681`) and
`PskReporterClient` (`:1711`). Unaddressed, **an R2 client and its daemon both
connect to the same DX cluster and both upload to PSK Reporter under one
callsign.**

**One thing that is not blocked**, recorded so nobody spends R2 on it: MOX.
`RadioModel::setMox` (`:12579`) routes through `MoxController`, which already
carries a pre-check callback (`MoxController.h:272-273`, installed at
`RadioModel.cpp:9080`) emitting `moxRejected(QString)` and already pinned by
`tests/tst_band_plan_guard_mox_rejection.cpp`. Any R2 or R4 TX gate routes
through that callback rather than building a parallel one, or remote refusals go
silent in the existing status-bar path.

---

## 5. Connected state: the one derivation R2 must change

`RadioModel::isConnected()` is computed from the pointer, not stored state:
`RadioModel.cpp:2476-2479` returns `m_connection && m_connection->isConnected()`.
A remote client that must not own a `RadioConnection` therefore reports
`connected == false` forever, and the property is READ plus NOTIFY with no WRITE
at `RadioModel.h:229`, so no generic apply can patch it.

**This is not cosmetic.** `RadioModel::maxSlices()` short-circuits on it and
returns **1** (`:2975-2982`), so a remote client believes every station is
single-slice regardless of the capability descriptor, and the `addSliceOnPan`
cap at `:4974` enforces that belief. Fourteen GUI sites gate on `isConnected()`
and twenty-five wire `connectionStateChanged`.

**The fix already exists in the class.** `m_connectionState` is a stored member
(`RadioModel.h:3318`), `connectionState()` is public (`:1433`), and
`setConnectionState` is the sole emitter of `connectionStateChanged`
(`:11743-11758`). There is in-tree precedent at `RadioModel.cpp:2692-2697`:
"Deliberately `m_connectionState` rather than `isConnected()`: the latter
requires a live RadioConnection object."

**The two definitions are not currently equivalent, and the window is 24 lines
wide.** In `teardownConnection`, `teardownWorkerThreadedConnection` takes
`RadioConnection*&` and executes `conn = nullptr` at `RadioModel.cpp:11624`,
while `setConnectionState(Disconnected)` does not run until `:11648`. Between
them the old expression is false and the new one true, and `RadioModel::setTune`
(`:12150`) would let TUNE proceed inside it.

**The remedy is to hoist `setConnectionState(Disconnected)` above the teardown
call.** This is a real ordering change in local direct mode and is therefore a
maintainer decision (section 10), not applied silently. The comment at
`:11642-11647` explains the original placement; hoisting still achieves its goal
because the call is unconditional either way.

**One trap for whoever writes the test.** `injectConnectionForTest`
(`RadioModel.h:1472`) is a one-line assignment with no signal wiring, so
`connectionStateChanged` never reaches `onConnectionStateChanged` and
`m_connectionState` stays `Disconnected`. A test built on that seam shows
`isConnected()` false while the mock reports Connected, which is the opposite of
an equivalence proof. Separately, the 24 test files using that seam survive the
change only because none asserts `isConnected()` directly and the antenna guards
read `m_connection->isConnected()` (`:9692`, `:10604`) rather than the model
method. That is luck and the plan records it as a verified fact.

---

## 6. StateMirror: schema, ordinals, direction, forwarder

Lives in a new `src/core/session/` inside `NereusCore`. The core boundary guard
tolerates it: `tests/tst_core_has_no_gui_includes.cpp:290` asserts only
`scanned > 100`, and 22 files in `src/core` already include `models/` headers.

**Enumeration.** A `QMetaObject` walk from `propertyOffset()` to
`propertyCount()`, built once per class into a cached schema shared by all
instances, so five slices pay one walk. Each record carries the name, a dense
declaration-order ordinal, the metaobject index (for read and write, never
serialised), the metatype, an encoding kind and the notify signal index.

**Encoding kind comes from `metaType().flags() & QMetaType::IsEnumeration`,
never from `QMetaProperty::isEnumType()`.** Verified against the real
declarations: for a plain `enum class` used as a property type, `isEnumType()`
returns **false** while the `IsEnumeration` flag returns true, because none of
the nine NereusSDR enums in these headers is `Q_ENUM` or in a `Q_NAMESPACE`
(`WdspTypes.h:307-322` carries seven `Q_DECLARE_METATYPE` lines and no `Q_ENUM`;
`DSPMode`, `AGCMode` and `FmTxMode` are not even `Q_DECLARE_METATYPE`'d). Six
other core classes **do** use `Q_ENUM` (`AudioEngine.h:183`,
`TxInterlockPolicy.h:37`, `MmioEndpoint.h:90` and `:98`, `ITransportWorker.h:32`,
`TxInhibitMonitor.h:126`), so the blanket rule is correct for the six models and
would change classification if any of those joins the mirror later.

**Type surface is small.** Exactly seven C++ kinds across all six models: `int`
44, `double` 39, `bool` 38, `QString` 8, `float` 7, `QChar` 1, plus nine
distinct enums across 11 declarations. No containers, no pointers, no custom
structs. Wire kinds needed are i64, f64, bool and utf8; float widens to f64 and
enums encode as their underlying int. The single `QChar` is `sliceLetter`, which
is CONSTANT and derived, and is excluded from the mirror entirely, so `QChar`
never reaches the wire.

**Wire representation is a session-scoped dense ordinal negotiated by a schema
message, not a name and not a raw metaobject index.** Raw index is provably
unstable: nine `SliceModel` properties were inserted mid-declaration at
`SliceModel.h:332-355` on 2026-07-30, shifting every later index. Names cost 10
to 26 bytes per delta on a surface that bursts roughly 50 deltas per slice on a
band restore. The schema message sends names once per class per session, so
version skew is caught by name comparison at handshake and deltas stay compact.
R2 encodes as JSON text on the reliable control channel, a deliberate staged
divergence from section 7.2's "compact native"; because the ordinal dictionary
exists from day one, R3 swaps the codec without touching the object model.

**Generic NOTIFY subscription uses a mechanism that already ships here.**
`MainWindow.cpp:2374-2393` walks a name-keyed property list, resolves through
`indexOfProperty`, and connects `QMetaProperty::notifySignal()` to a
zero-argument slot with `Qt::UniqueConnection`;
`tests/tst_pan_status_overlay.cpp:381-399` already guards it. Generalised: one
watcher per mirrored instance, a single zero-argument slot, connected to every
property's notify signal with `Qt::UniqueConnection`. That flag is what makes
shared notifiers free, and they are common: `filterChanged` notifies both
`filterLow` and `filterHigh`, `TunerModel::stateChanged` notifies four
properties, `relayChanged` three, `RadioModel::infoChanged` three,
`PanadapterModel::levelChanged` two.

Inside the slot, `senderSignalIndex()` maps back through the schema to the dirty
ordinals, and the mirror **re-reads each with `QMetaProperty::read`** rather
than taking the signal argument. Re-reading is mandatory, not stylistic:
notifiers are one-to-many, and `setDspMode` rewrites `filterLow` and
`filterHigh` as a side effect and emits `filterChanged` separately
(`SliceModel.cpp:406-411`).

**Two things the first task must prove rather than assume.** Signal arities 0, 1
and 2 all appear (`TunerModel::stateChanged` none, `frequencyChanged` one,
`SliceModel::filterChanged(int, int)` two), and the
`connect(sender, QMetaMethod, receiver, QMetaMethod)` form with a zero-argument
receiver was verified only for arity 1. Qt permits a receiver with fewer
parameters, but the arity-2 case is the one the design leans on hardest. And
**CONSTANT properties have no NOTIFY**, so a purely notify-enumerating watcher
silently skips them; `sliceLetter` is already in that hole and the proposed
`sliceIndex` would join it, so `track()` needs an explicit snapshot path for
no-NOTIFY properties.

### 6.1 Direction: four classes, default deny

Lives in a **single default-deny table in `MirrorPolicy`**, not in
`Q_CLASSINFO` on the model headers and not in a naming convention. A property
not named in the table is Outbound, so a newly added `Q_PROPERTY` mirrors
read-only until someone opts it in. Nine landed in four days in July 2026; that
is the drift rate the table must survive, and default-deny converts a silent
allocator desynchronisation into a visibly missing remote control. A golden-list
guard test makes drift loud at `ctest` time.

**Bidirectional.** Operator-settable. Mirrored outbound, inbound write applied
through `QMetaProperty::write`.

**Outbound only, daemon-authoritative but writable in C++.** Exactly eight
names, all verified to carry WRITE: `chainIndex` (`SliceModel.h:183`),
`ddcIndex` (`:185`), `streamIndex` (`:194`), `shiftOffsetHz` (`:203`), `panKey`
(`:207`), `sampleRateHz` (`:227`), `widebandExtensionRequested` (`:244`),
`psPaused` (`:247`). The client uses a command verb instead, and the rejection
reason names the verb.

**Inbound only, read-only telemetry.** The 24 properties with no WRITE, applied
through a per-model `applyMirroredValue(name, variant)` hook and refused
outbound. **`txSlice` belongs here, not in the derived set**: `SliceModel.h:175`
has no WRITE. `RadioModel::connected` is the single exception, handled by
section 5's re-pointing instead.

**Client owned.** `panKey` alone, and the parent design has it backwards: pan
ids are minted GUI-side (`PanadapterStack.cpp:42`) and
`RadioModel::addPanadapter()` has no production caller. But the daemon is not
neutral either: `connectToRadio` stamps `setPanKey("pan-0")` on Slice A at
`RadioModel.cpp:5977-5981` and `DaemonApp.cpp:114` calls it. And `panKey` steers
the allocator: `:4608-4611` derives `openingANewPan` from `slicesOnPan` and
passes it as `preferOwnStream`, so "addSlice then write panKey" and
"addSliceOnPan(panId)" produce different stream placements for the same end
state. R2 mirrors `panKey` outbound so a reconnecting client can restore its
layout, applies it inbound, and routes pan-affecting creation through
`addSliceOnPan`.

**Echo suppression is an `m_applying` guard around inbound apply, and it must
cover more than the obvious.** `RadioModel::addSlice` installs two peer-mirror
lambdas unconditionally, outside `wireSliceSignals`, at
`RadioModel.cpp:4708-4721` (nbMode across peers on a stream) and `:4746-4760`
(NB1/NB2 tuning), with a comment at `:4702-4707` stating they are wired there so
they hold whether or not a radio is attached. Both already carry re-entrancy
guards, so the risk is not recursion: it is that `peer->setNbMode()` emits on the
peer object, which a generic forwarder would send outbound as a delta the daemon
never asked for.

---

## 7. Object identity, lifecycle, and the connect-time snapshot

`ObjectRef` is a type tag plus a stable id. For slices the id is
`SliceModel::sliceIndex()`, never a list position. That requires
`Q_PROPERTY(int sliceIndex READ sliceIndex CONSTANT)` over the accessor at
`SliceModel.h:477`. Safe: no QML anywhere in the tree, no dynamic property of
that name, no test asserts a property count, CONSTANT already has precedent on
the same backing member at `:181`, and `setSliceIndex` has one production call
site (`RadioModel.cpp:4518`) which runs before `sliceAdded` fires.

**Ids are minted lowest-free and reused** (`:4514-4518` scans upward from 0) and
`removeSlice` never renumbers survivors (`:4914`). **The reuse hazard is sharper
than the design describes:** `removeSlice` calls `slice->deleteLater()` and only
then emits `sliceRemoved` (`:4965-4966`), so the dying object outlives the
signal and a same-turn `addSlice` can remint the freed id while the corpse is
alive. The mirror keys its registry by `QPointer` and detaches inside the
`sliceRemoved` handler rather than relying on `QObject::destroyed`. A create for
a live id is a protocol error.

**`activeSliceChanged` must not drive lifecycle.** It carries a list position:
`setActiveSlice(int index)` resolves `m_slices.at(index)` and re-emits that index
at `:5621`, and the other two sites pass a literal 0 or -1 (`:4854`, `:4960`).
R2 does not need it, because `SliceModel::active` is already a mirrored
read-only property maintained at all three sites, so active-slice identity rides
the ordinary delta path keyed by slice id. An id-based `activeSliceIdChanged(int)`
should be added alongside the positional signal because the existing one is a
live trap, but nothing in R2 depends on it.

**Attach on `sliceAdded`, not at construction, and this is what makes ordering
work without a buffering layer.** `addSlice` performs a long list of mutations
first: TX arbiter resync (`:4549`), a frequency and mode seed from the active
slice (`:4566-4567`), a stream bind writing `setStreamIndex` and
`setShiftOffsetHz` and possibly moving another stream's centre (`:4612` reaching
`:4349-4355`), possible self-assignment as active (`:4853`), and
`wireSliceSignals` (`:4868`), all before `emit sliceAdded` at `:4870`. Five or
more property changes therefore name the new slice before any create could be
sent. Because the mirror only forwards from objects it tracks, none of it leaks,
and the create message carries a **full property snapshot of a settled object**
rather than a delta stream describing a history no client ever had.

**Rejected creation is first-class.** `addSlice` can fail after partial
construction and roll back entirely, returning -1 and deleting the slice, having
already emitted `sliceAddRejected` from inside `bindSliceToStream` (`:4288`,
rollback at `:4612-4625`). A second and more common rejection is at `:4985`,
where `addSliceOnPan` refuses before `addSlice` is entered. Neither produces an
`object.create`, so a rejected add is indistinguishable from one that never
started, which is correct. The command result carries the reason.

**Removal order is fixed by the code.** `removeSlice` hands TX off before
removal (`:4907-4912`) and stops external diversity when the victim is slice 0
(`:4898-4900`), so the client sees `txSlice` move off the victim while it still
exists, then the freed stream's binding change, then `object.destroy` last.

**Connect-time snapshot is one uninterrupted event-loop turn** on the daemon's
model thread: clear the dirty set, send the schema message per class, send
`object.create` plus a full property bag per live instance, send the
snapshot-complete marker, resume flushing. Nothing can be processed between
those steps because `RadioModel` and every `SliceModel` live on one thread, so
no update is lost and no delta can precede its create. **That guarantee holds
only because everything is single-threaded, and the plan states it as an
invariant** so a future threading change does not quietly void it.

**Outbound coalescing is a per-tick dirty set flushed on a timer**, values
re-read at flush time so latest-wins is structural. Not optional:
`SliceModel::restoreFromSettings` runs 75 setters (`SliceModel.cpp:1839`
onward), so one band-button press produces roughly 250 messages on a 5-slice SKU
without it. `TciVfoCoalescer` is the existing implementation of exactly this
shape and should be generalised; its value type is `QString`, so `StateMirror`
needs a sibling rather than a literal reuse.

**One note for R3.** `TciSendQueue`, which section 7.2 designates for reuse,
stores `QQueue<QString>` and its entire API is `QString` (`TciSendQueue.h:59`,
`:65`, `:80-82`), and its Binary bucket is effectively dead because TCI binary
frames call `QWebSocket::sendBinaryMessage` directly (`TciServer.cpp:290`,
`:2561`). Reuse for binary payloads is a type change, not a drop-policy change.

---

## 8. SettingsProxy: the mutator funnel comes first

**The mechanism is delegation inside `AppSettings`, not a rename.** It gains a
non-owning `ISettingsBackend*` and a one-branch delegation in its accessors;
`setRemoteBackend(nullptr)` is the default and keeps today's path
byte-identical. That is what lets 593 call sites across 92 files and all 47
Setup pages stay unedited.

**The delegation cannot be added to five accessors and called done, and this is
the load-bearing correction.** `setHardwareValue` and `hardwareValue` do **not**
route through `setValue` and `value`: `AppSettings.cpp:921-925` builds the key
and calls `m_settings.insert` directly, and `:927-936` calls `m_settings.constFind`
directly. That is 49 plus 46 call sites covering `hardware/<mac>/*`, the number
one Station prefix and **92 percent of the keys in a real settings file**. A
five-accessor delegation would leave a remote client reading its own empty local
hardware section while the daemon never saw a client write. The same gap covers
`clearHardwareValues` (`:951-960`) and the whole `radios/*` family (`saveRadio`
`:770-786`, `forgetRadio` `:798` and `:814`, `setLastConnected` `:897` and
`:899`, `setDiscoveryProfile` `:913`, `setModelOverride` `:982`, reads at
`:851-878`, `:891`, `:906`, `:969`). In total `AppSettings.cpp` has **23 direct
`m_settings` mutation points and only four are inside `setValue`, `remove` and
`clear`.**

R2 therefore does a contained internal refactor **first**: route every mutator
and reader through `setValue`, `value` and `remove` internally. Roughly 23 edits
inside one `.cpp` with zero call-site changes, making both the delegation branch
and the daemon-side change hook complete by construction. Note
`AppSettings::instance()` is not the only construction path: an explicit
`AppSettings(filePath)` constructor exists at `AppSettings.h:110` and every
isolated test uses it, so the backend and hook are singleton-scoped.

**`allKeys()` also needs rerouting**, because three `src/core` consumers
prefix-scan it (`SettingsHygiene.cpp:142` and `:289`,
`ExternalVariableEngine.cpp:299` and `:354`, `AudioEngine.cpp:1851`). On a
remote client each would scan an empty store and silently do nothing, and
`SettingsHygiene` is the daemon's station-value validator.

**Bulk entry points.** `snapshot(prefixes)` returns fully qualified keys with
stored strings, unlike `hardwareValues` which strips the prefix (`:944`).
`applySnapshot` merges rather than replaces, so a client allowlist gap cannot
wipe daemon keys. `QString` rather than `QVariant`, because `setValue` already
collapses to `QString` (`:687`) and `value()` returns `QVariant(QString)`
(`:680`). **Absent keys must stay absent**, because `value()` returns the
caller's default for an unwritten key (`:676-683`) and Setup pages depend on it:
`GeneralOptionsPage.cpp:265` expects "United States" back when `Region` was never
set. The client cache therefore carries a proven-unset set alongside the map.

**Do not snapshot the store.** A measured settings file is **15,201 keys and
3,186,789 bytes** across five MACs, of which 13,970 are under `hardware/` and
11,564 are `hardware/<mac>/tx/profile/*`. Scope to the connected MAC plus global
daemon-scoped prefixes, roughly **2,900 keys and 190 KiB**. `hardware/oc/*` must
be included explicitly, because `oc` is a literal segment and not a MAC
(`AppSettings.cpp:1105` already special-cases `hardware/oc/n2adrFilter`).

**Classification is a pure function of the key text, not a prefix list**, because
neither a prefix list nor a glob can express the Display and Tci splits:
`DisplayFftSize` and `DisplayNoiseFloorColor` differ only in a mid-key word. The
function strips per-pan suffixes first (`SpectrumWidget.cpp:577-584` appends
`_<panIndex>` past pan zero), then evaluates an ordered rule table: explicit
exceptions, then prefix rules, then whole-key rules for the roughly 85
unprefixed keys (`Region`, `CWPitch`, `SwrProtection*`, `SnbDefault*`,
`NbDefault*`, `Notch*`, `PGXL_*`, `TGXL_*`, `RfKit_*`), then a default of
**OperatorLocal**.

Defaulting local rather than Station is deliberate: a client-local key leaking
into the station store gets written by every client that connects, while a
station key failing to cross shows as a setting that will not stick. Both are
bad; the second is recoverable.

**Reads are synchronous out of the cache and never touch the network.** Writes
are optimistic with an origin tag echoed back on the daemon's broadcast, so a
slider drag is not rolled back value by value by its own echo. Offline before
the handshake, proxied reads would return caller defaults, so a Setup dialog
built at that moment would bake ship defaults into 187 widgets and the first
interaction would write them to the station. R2 gates the Setup dialog on
`ready()`, the same gate section 12.2 puts on TX. Offline after the handshake,
reads keep serving the last snapshot and writes are **dropped rather than
queued**, because the daemon store can move underneath a queued write and blind
replay silently reverts it; reconnect re-snapshots and reports what was not
applied.

**One key R2 does not pretend to resolve.** `DisplaySpectrumFps` is read twice in
`MainWindow` for two consumers on two sides of the split: `:1482` folds it into
`FftPoolConfig` (daemon FFT production rate) and `:3499-3507` pushes it into
`SpectrumWidget::setDisplayFps` (client paint timer). A three-valued scope enum
cannot encode "both, meaning different things at each end", and parent section 17
item 3 already declares the Display page split a design pass. R2 classifies the
four `FftPoolConfig` keys as Station and records this one as a known unresolved
straddle.

---

## 9. Meters in R2

R2 gives the per-slice S-meter a model home and nothing else.

`SliceModel` gains read-only `signalStrengthDbm`, default **-140.0**, matching
the existing fallbacks at `MeterPoller.cpp:307` and `TciServer.cpp:331`. A small
core class `SliceMeterPump` writes it: a direct extraction of
`MeterPoller::pollSliceSMeters` (`MeterPoller.cpp:362-372`) with the emit
replaced by a model write plus its own `QTimer`, roughly sixty lines. Every call
it makes already happens from `NereusCore` today: `TciServer` runs the same read
on its own 200 ms timer at `TciServer.cpp:308-339`, **which is a working
headless meter poller shipping today** and is the proof that meter polling does
not need the GUI. `RxChannel::getMeter` is documented lock-free and safe from a
meter timer (`RxChannel.h:233`).

The meter then rides `StateMirror` as an ordinary read-only property on the
reliable Control envelope. No new wire type, no meter-specific message, no rate
negotiation. When R3 lands the Media envelope the transport class changes
without touching the model, which is the strongest argument for a property over
a bespoke message.

**Three things the pump must replicate or local direct mode changes.**
`MeterPoller::poll` returns early while transmitting (`:290-292`) so per-slice
S-meters freeze during MOX; `RadioStatus::isTransmitting` is the source. The
Multimeter page drives `MeterPoller::setIntervalMs` through
`RadioModel::meterPoller()` (`MultimeterPage.cpp:232`, `:264`, `:281`), so the
pump's interval must come from the same setter or the operator's delay slider
silently stops working. And `pollSliceSMeters` skips a slice with no WDSP
channel rather than writing a fallback, so R2 chooses **-140.0** for wire
determinism and states it.

**The GUI side is a net deletion of roughly 40 lines**, and it fixes a real bug:
today the active slice's flag is written twice per tick because `poll()` calls
`pollSMeter` at `:336` and `pollSliceSMeters` at `:337` and both reach the same
`VfoWidget` (`MainWindow.cpp:8132` and `:1071`), with the second winning, which
contradicts the comment at `MeterPoller.cpp:441-446`. The connect at
`MainWindow.cpp:8132` must be deleted in the same change or the double write
survives.

**TX and PA telemetry stays out of R2.** `RadioStatus` carries live forward
power, reflected power, SWR, PA temperature, PA current and MOX state (written
at `RadioModel.cpp:2039` and `:8970`) and declares zero `Q_PROPERTY`. Two of its
signals are multi-argument (`powerChanged` three, `pttChanged` a `PttSource`
plus a bool at `RadioStatus.h:165-166`), which is a mirror capability rather
than a declaration change, and every value reads zero outside transmit. It
belongs with R4.

---

## 10. Open decisions

Each needs the maintainer. None is inferable from the code.

1. **TLS and certificate provisioning.** Qt6 has **no API that generates an
   X.509 certificate**; `QSslCertificate` only parses. `git grep` for
   `QSslCertificate`, `QSslKey`, `QSslConfiguration`, `SecureMode` and OpenSSL
   across `src/` and `CMakeLists.txt` returns **zero hits**. The options are
   shelling out to the `openssl` binary (breaks the Windows portable ZIP and the
   NSIS installer), linking libcrypto directly (a new hard dependency on three
   platforms, plus `QSslSocket::supportsSsl()` handling where Qt is built
   against Schannel), or emitting minimal DER/PKCS#8 by hand. This is the single
   largest under-specification in the phase and needs its own task ahead of the
   transport task.
2. **The `isConnected()` ordering change** (section 5), which alters the
   observable ordering of a local disconnect.
3. **Daemon settings profile migration.** Task 1 moves `nereusd` to a reserved
   profile, so an existing deployment starts from an empty store and loses PA
   profiles, Alex maps, step-attenuator state and per-MAC `tx/profile` keys.
   Either a one-shot migration copying the daemon-scoped subset on first start,
   or accept the reset because R1 is unreleased. Note the weaker form of task 1
   recommended by review: default to the reserved profile and **warn loudly** on
   collision rather than exiting non-zero, so "both on one box sharing settings"
   remains a supported configuration.
4. **The test seam for connected-state equivalence.**
   `tests/tst_daemon_app.cpp:11-35` documents that `connectToRadio` contains a
   synchronous nested `QEventLoop` on FFTW wisdom, over five minutes cold, that
   QtTest's watchdog aborts, and that **no test in the suite calls it**. Either
   add a task making `RadioModel` connectable in a test (wisdom bypass plus
   `P1FakeRadio` wiring), or accept an assertion that proves less.

### 10.1 R3 decisions, which block planning R3 but not R2

5. **Per-slice audio encoding or one master stream** (parent section 16).
6. **One shared FFT per stream, or two** (parent section 17 item 4).
7. **Verify the section 9.3 Opus mode analysis** against the vendored
   `opus_encoder.c` (parent section 17 item 7).
8. **The section 11 attribution decision**, an R3 deliverable so the implementer
   is not resolving a compliance question mid-task.

---

## 11. Risks carried into the plan

1. **The stream allocator re-arming on the client** is the largest single risk.
   Suppression must land before capability apply, and the guard test must cover
   it, or an inbound frequency delta rolls the operator's VFO back.
2. **The `StateMirror` core task has no smaller predecessor to prove the
   approach.** The generic forwarder was verified only for arity-1 signals;
   arity 0 and arity 2 are both live. Prove the arity-2 case and the enum
   encode path in the first hour as a go/no-go, not at the end.
3. **The `AppSettings` mutator funnel touches the persistence layer every
   existing test depends on.** A mistake in the `radios/*` helpers would corrupt
   saved-radio state for local users with no remote involvement. Run the
   settings and connection labels immediately after.
4. **`setSampleRateLive` blocks its caller for at least 40 ms of
   `QThread::msleep`** (`RadioModel.cpp:13571`, `:13611`, `:13674`). If it runs
   on the session's socket-read path the daemon stops servicing heartbeats
   mid-sequence and section 12.1's watchdog reads it as link failure. It must be
   queued to the `RadioModel` thread, and the plan states which thread the
   session read loop lives on.
5. **`SettingsProxy::ready()` is not sufficient as the Setup dialog gate.** On a
   freshly reserved daemon profile the snapshot is legitimately empty and
   `ready()` is true with no values, which lets 187 widget constructors bake
   ship defaults into the station store. The gate needs a second condition or
   the daemon needs a first-run seed, and that interacts with decision 3.
6. **`Q_PROPERTY` drift is fast**: nine landed mid-declaration in four days. The
   default-deny table makes the failure visible rather than silent, but a
   feature author who forgets the table entry ships a knob that does not work
   remotely. The golden-list guard test is the only thing that catches it.
7. **The most likely half-working outcome is `SettingsProxy` passing every test
   and never carrying a Setup page.** A completeness test scanning only
   `src/core` and `src/models` misses that nearly every Station key is *written*
   from `src/gui/setup/`, so a misclassified key reads locally, writes locally,
   sticks in the widget, survives a relaunch, and never reaches the station. Two
   cheap fixes, take both: extend extraction to `src/gui/setup/` with the
   inverse assertion that any key appearing in both a Setup page and a core
   consumer must classify Station, and log every proxied read that resolved
   locally so the acceptance run produces a scannable list.
8. **Two GUI behaviours change in local direct mode** and need eyeballing on a
   bench, not just in tests: the S-meter double write disappears, and the
   disconnect ordering moves if decision 2 is approved.
9. **Deferring record streams leaves two daemon defects live**: `nereusd` builds
   seven spot collectors and starts none, and `FreeDVStationModel` grows without
   bound headless. File both as issues now.

---

## 12. R3 notes from the piHPSDR comparison (2026-08-08)

**Status: reference notes, not R2 scope.** The maintainer evaluated
`dl1ycf/pihpsdr` (`@4aa95c5`, 2026-08-06, GPLv3-or-later) as a possible
alternative base and **concluded we keep our architecture**. Their core is not
GUI-free: `struct RECEIVER` holds `GtkWidget *panel`, `*panadapter` and
`*waterfall` directly (`src/receiver.h:190-192`), 150 of 219 source files
include `gtk/gtk.h`, and there is no automated test suite. Ours is already split
into `NereusCore` / `NereusGui` with `nereusd` as a real target. All three of
those facts were verified against the clone on 2026-08-08.

**Do not implement any of the following in R2.** They are evidence for whoever
plans R3's display codec and media path.

### 12.1 Piggyback fast telemetry onto the spectrum frame

`SPECTRUM_DATA` (`src/client_server.h:695-726`) carries the S-meter (`rxlvl`),
AGC, ALC, SWR, mic peak, PureSignal status and all four VFO frequencies **inside
the same packet as the spectrum samples**, sent every 150 ms
(`src/server_thread.c:1131`). One packet where our design sends several, and the
meters always match the trace they arrived with.

That is worth measuring against our `SliceMeterPump` plus `StateMirror` path
before R3 locks the codec. Note the R2 finding it interacts with:
`signalStrengthDbm` now has a NOTIFY on the mirrored surface, so a connected
daemon already produces up to one delta per slice per pump tick, 10 Hz at the
default interval, with the coalescer as the only thing between that and the link.

### 12.2 Uncompressed fallback

`src/server_thread.c:217-245`: when `compress()` does not return `Z_OK` they set
`compressed = 0` and send the raw byte array rather than dropping the frame.
Cheap robustness our codec should copy.

### 12.3 Opus tiering as a UX option

`src/client_thread.c:246-283` offers three user-selectable tiers: 32 kbit
`OPUS_APPLICATION_VOIP` with signal VOICE, 64 kbit `OPUS_APPLICATION_AUDIO` with
signal MUSIC, and 96 kbit AUDIO with MUSIC, all at complexity 5. Our section 9.3
specifies a single 24 kbit/s at complexity 10.

The CPU tradeoff is the interesting part: complexity 5 against 10 matters on
Pi-class hardware, which is the deployment R1 benched on.

### 12.4 Where we are ahead, so R3 does not regress toward their design

Their quantizer is fixed at exactly 1 dB per step, `(int)sample + 200` clamped to
0-255 against a hard-wired -200 to +55 dBm window (`src/server_thread.c:241-245`),
and will stair-step visibly on a weak-signal waterfall. Ours is 8 bits over a
**configurable** window, roughly 0.41 dB per step at typical settings.

They also send **one** plane. We send two, because our trace and waterfall have
independent detectors and averaging. **Do not adopt their single-plane format.**

### 12.5 Explicitly not adopted

No architecture change and no piHPSDR code. In particular, do not port their
command set: our generic `StateMirror` over 144 `Q_PROPERTY` declarations is
deliberately better than their 114 hand-enumerated `CMD_*` types, because a new
property mirrors for free rather than costing two handlers.
