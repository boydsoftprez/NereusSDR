# Remote Daemon R2: State Mirror, Settings Proxy, and the Secure Session, Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Drive a real radio from a GUI process that owns no DSP, over `wss`, on one host. Every operator control, every Setup page, and the per-slice S-meter work end to end. Nothing renders and nothing makes sound.

**Architecture:** `RadioModel` gains a `Role`. In `Role::Remote` it never enters `connectToRadio`, which already yields a DSP-free model, and five local authorities that keep running regardless are explicitly suppressed. A generic `StateMirror` reflects **144 `Q_PROPERTY` declarations across five models** (`MeterModel` is excluded as dead code, addendum section 3) in both directions with a default-deny direction table. `AppSettings` gains an internal mutator funnel and then a delegating backend, so 593 call sites and 47 Setup pages stay unedited. A `wss` transport carries the section 7.0 handshake.

**Tech Stack:** C++20, Qt6 (Core / Gui / Network / WebSockets / Multimedia / SerialPort), **OpenSSL libcrypto (new)**, CMake + Ninja, Qt Test.

**Design docs:** [R2/R3 addendum](2026-08-03-remote-daemon-r2-r3-design-addendum.md) is authoritative. Parent: [remote daemon architecture](2026-07-28-remote-daemon-architecture-design.md). Sibling: [identity and pairing](2026-08-02-remote-station-identity-and-pairing-design.md). Predecessor: [R1 plan](2026-08-02-remote-daemon-r1-plan.md), complete.

**Base:** `a7324cdd`. Every line cite was derived against it and re-verified against the tree on 2026-08-03.

---

## Global Constraints

- **R2 does not ship.** The release unit is R2 plus R3 (addendum section 1). No task may introduce an interim wire format, placeholder rendering, or temporary UI that R3 deletes.
- **Local direct mode must not regress.** `Role` defaults to `Local`; every remote branch is gated on `Role::Remote`. Three tasks change local behaviour deliberately (**3, 12, 13**) and task 16 is the gate that proves nothing else did.
- **Verification: use `-R`, not `-L`, and always `--no-tests=error`.** Only four ctest labels exist in this tree (`core`, `models`, `gui`, `unclassified`), derived mechanically at `tests/CMakeLists.txt:65-88`. **`ctest -L settings` prints "No tests were found!!!" and exits 0**, so a label gate silently passes having run nothing. Every ctest invocation in this plan therefore names tests by regex and carries `--no-tests=error`.
- **Per `docs/development/fast-test-loop.md:24-33`, build the target before running it.** `cmake --build build --target <test>` then `ctest -R <test> --no-tests=error`.
- **No `goto`, no raw `new`/`delete`, no `#define` for constants, braces on all control flow.** Classes `PascalCase`, methods `camelCase`, constants `kPascalCase`, members `m_camelCase`.
- **Platform guards use `Q_OS_WIN` / `Q_OS_MAC` / `Q_OS_LINUX`.**
- **Settings go through `AppSettings`, never `QSettings`.**
- **All commits GPG-signed.** No `Co-Authored-By: Claude` trailer.
- **No em-dash or en-dash characters** anywhere.
- **Every task ends green** before you commit.

## Decisions taken, so no task re-opens them

| Decision | Choice | Where |
| --- | --- | --- |
| Release unit | R2 plus R3. R2 alone is an engineering gate | Addendum 1 |
| TLS certificate generation | **Link OpenSSL libcrypto.** Qt6 has no X.509 generation API | Addendum 10.1 |
| `isConnected()` ordering | **Approved.** Hoist `setConnectionState(Disconnected)` above teardown | Addendum 10.2 |
| Daemon profile default | Reserved `daemon` profile, **warn on collision, do not exit** | Addendum 10.3 |
| Daemon profile migration | **Accept the reset.** R1 is unreleased, so there is nothing to migrate | Addendum 10.3 |
| Setup-dialog offline gate | `ready()` **and** a non-empty snapshot **or** an explicit first-run seed marker written by the daemon at task 1 | Addendum 11 risk 5 |
| Test seam | **Build it** (task 2), and it precedes every task that consumes it | Addendum 10.4 |
| Record streams | Deferred to R3 | Addendum 3, 10 |
| Pairing and connect UI | Its own planning unit. R2 ships `--station` and `--token` plus one Setup field group | Identity design 10.3 |
| Control-channel heartbeat | **In R2.** `QWebSocket` ping/pong, configured at task 18, tested at task 19 | Maintainer, 2026-08-08 |
| Wire encoding | Negotiated dense ordinal, JSON text on the reliable channel | Addendum 6 |
| Spot-client ownership | Task 5 gates the client's collectors off as a **safety** measure. Which side owns each collector stays **open** | Task 5 step 5a |
| Risk 9 issues | **Drafted into the tree, not filed.** The maintainer files them | Task 5 step 6 |
| Hardware bench rows | **Run them on the ANAN-G2E only.** Maintainer authorised the G2E on 2026-08-05 and **explicitly excluded the ANAN-G2** | Tasks 16, 20 |

### Bench hardware: the G2E, never the G2

The maintainer authorised bench work against the **ANAN-G2E (HermesC10)** and
**explicitly excluded the ANAN-G2**, which is a different radio on the same LAN.
An earlier smoke run in this branch reached the G2 by accident, which is why this
is written down rather than left in conversation.

Before any bench row connects to anything:

1. Run discovery and **list every responder with its MAC and board type**. Send
   **both** probes: the P1 packet alone misses P2 boards, and the G2E is P2. See
   `RadioDiscovery::scanAllNics`, which sends a 63-byte P1 and a 60-byte P2
   probe on every NIC.
2. **The board byte is the identification, not the MAC.** The G2E answers `0x14`
   (`HPSDRHW::HermesC10`); the G2 answers `0x0A`. Those cannot be confused, so
   the board byte alone is sufficient to tell the authorised radio from the
   excluded one. Pin by MAC because a connection needs an address, but **derive
   the decision from the board byte**.
3. **Do not ask the maintainer to confirm a MAC address.** That was tried on
   2026-08-08 and it is a bad question: it asks a human to recall from memory
   something the hardware states more reliably in its own discovery reply.
4. If no responder answers `0x14`, **stop**. Do not fall back to whatever
   answered. Report which radios did respond and let the maintainer decide.

This applies to task 16 step 3 and every row of task 20 step 5.

## Task dependency graph

Numeric order is authoritative for dispatch. The edges below are the real
constraints; where they permit parallelism, a scheduler may use it.

```
 1 daemon profile ──┬──► 15 proxy
                    ├──► 17 TLS
                    └──► 20 gui gating
 2 test harness ────┬──► 3 connected state ──► 4 role+seam ──► 5 authorities ──┐
                    ├──► 12 meter pump                                          │
                    └──► 20 gui gating                                          │
 6 slice identity ──► 7 mirror core ──► 8 inbound ──► 9 lifecycle ──► 10 snapshot ──► 11 verbs
                                                          ▲                          │
                                                          └──────────────────────────┤
13 settings funnel ──► 15 proxy ◄── 14 classify                                      │
                          │                                                          │
 3,12,13,15 ──────────► 16 local-mode gate ──► 17 TLS ──► 18 wss session ◄────────────┘
                                                              ▲   ▲
                                                    5 ────────┘   └──── 15
                                                              │
                                                           19 link loss
                                                              │
                                                           20 gui gating + acceptance
```

**Load-bearing edges, stated explicitly:** 2 before 3 (the harness is a compile
dependency of task 3's test); 5 before 18 (or capability apply sizes the stream
pool and an inbound frequency delta rolls the operator's VFO back); 15 before 18
(task 18 wires `SettingsProxy` onto the session); 16 before 17 (a red local gate
must stop the phase before libcrypto lands).

Task 13 does **not** depend on task 1. It is a contained `AppSettings` refactor
using an isolated `AppSettings(filePath)`. The two touch the same file, so run
them sequentially to avoid contention, not because of a data dependency.

---

## File Structure

**New:**

| File | Responsibility |
| --- | --- |
| `tests/fakes/ConnectableRadioModel.{h,cpp}` | Wisdom-free `RadioModel` plus `P1FakeRadio` wiring |
| `tests/fakes/LoopbackStationLink.{h,cpp}` | In-process `IStationLink` for pre-transport tests |
| `src/core/session/MirrorSchema.{h,cpp}` | `QMetaObject` walk, dense ordinals, encoding kind, notify map |
| `src/core/session/MirrorPolicy.{h,cpp}` | The default-deny direction table |
| `src/core/session/StateMirror.{h,cpp}` | Forwarder, inbound apply, dirty set, coalescer |
| `src/core/session/ObjectRegistry.{h,cpp}` | `QPointer`-keyed live-object map, id-reuse tolerant |
| `src/core/session/IStationLink.h` | The seam `RadioModel` holds in `Role::Remote` |
| `src/core/session/SessionMessages.{h,cpp}` | Message shapes, JSON codec |
| `src/core/session/StationServer.{h,cpp}` | Daemon: session lifecycle, snapshot, verbs |
| `src/core/session/StationClient.{h,cpp}` | Client: applies mirror, issues verbs |
| `src/core/settings/SettingsScope.{h,cpp}` | `classifySettingsKey`, pure, no I/O |
| `src/core/settings/ISettingsBackend.h` | Delegation interface |
| `src/core/settings/SettingsProxy.{h,cpp}` | Client cache, write-through, proven-unset set |
| `src/core/settings/SettingsProxyServer.{h,cpp}` | Daemon: snapshot, apply, broadcast |
| `src/core/meters/SliceMeterPump.{h,cpp}` | Core per-slice S-meter poller |
| `src/core/security/CertificateStore.{h,cpp}` | libcrypto self-signed cert, fingerprint |
| `src/core/security/TokenStore.{h,cpp}` | Generated token, rate-limited verify |

**Modified:** `src/models/RadioModel.{h,cpp}`, `src/models/SliceModel.{h,cpp}`, `src/models/TunerModel.{h,cpp}` (13 of the 24 no-WRITE properties live here), `src/core/TxSliceArbiter.{h,cpp}`, `src/core/WdspEngine.{h,cpp}`, `src/core/AppSettings.{h,cpp}`, `src/core/daemon/{DaemonApp,DaemonConfig}.{h,cpp}`, `src/server_main.cpp`, `src/gui/MainWindow.{h,cpp}`, `src/gui/meters/MeterPoller.{h,cpp}`, `tests/CMakeLists.txt`, `tests/tst_daemon_config.cpp`, `CMakeLists.txt`.

---

### Task 1: Reserve a daemon settings profile

`nereusd` shares the GUI's settings file, so R2 could pass its own verification with `SettingsProxy` unimplemented (addendum 2.1). This lands first or every later task is measured against a false positive.

**Files:** Modify `src/core/AppSettings.{h,cpp}`, `src/core/daemon/DaemonConfig.{h,cpp}`, `src/server_main.cpp`, `tests/tst_daemon_config.cpp`. Test `tests/tst_daemon_settings_profile.cpp`.

- [ ] **Step 1: Write the failing test against a NereusCore seam, not `main()`.** `server_main.cpp` is linked into no test target (`CMakeLists.txt:1144-1145`). Extend `resolveDaemonProfileArgument` to take a `bool wasSet` and assert on it. Assert: absent resolves to `kDaemonProfileName`; explicitly-empty still shares; the **log directory moves with the settings path**.
- [ ] **Step 2:** Add `AppSettings::kDaemonProfileName` (`"daemon"`).
- [ ] **Step 3: Branch on `parser.isSet(profileOpt)`, not on emptiness.** `profileOpt` is declared with no default (`server_main.cpp:168-176`), so `parser.value()` returns empty for both absent and explicitly-empty and the two cannot be told apart by value. Resolve the string **once** at `:186-195` and pass the same value to both `AppSettings::setProfileOverride` and `CoreInit::initialize` (`:197`), which resolves the log directory independently (`CoreInit.cpp:88-94`).
- [ ] **Step 4:** On path collision, `qCWarning` naming both paths and **which of the two inputs produced it**. Do not exit.
- [ ] **Step 5:** Update the existing test `emptyProfileArgumentMeansNoProfile` (`tests/tst_daemon_config.cpp:183-189`), which pins the exact contract this task inverts. Keep an `explicitlyEmptyProfileArgumentStillShares` case.
- [ ] **Step 6:** Update `DaemonConfig.h:100-129`, whose comment calls the shared default deliberate. Replace with the reserved-profile rationale and a pointer to addendum 2.1.
- [ ] **Step 7:** Write the **first-run seed marker** the Decisions table commits to, so task 15's Setup gate has a second condition to test.
- [ ] **Step 8:** `cmake --build build --target tst_daemon_settings_profile tst_daemon_config && ctest -R 'daemon_settings_profile|daemon_config' --no-tests=error`. Commit.

### Task 2: A connectable `RadioModel` for tests

`connectToRadio` blocks on FFTW wisdom and no test in the suite calls it (`tests/tst_daemon_app.cpp:11-35`). Tasks 3, 12 and 20 all need one, so this precedes them.

**Files:** Create `tests/fakes/ConnectableRadioModel.{h,cpp}`. Modify `src/models/RadioModel.{h,cpp}`, `src/core/WdspEngine.{h,cpp}`, `tests/CMakeLists.txt`.

- [ ] **Step 1: Write the failing test.** Assert a `RadioModel` reaches `ConnectionState::Connected` against `P1FakeRadio` within QtTest's 120 s timeout.
- [ ] **Step 2: Suppress `WdspEngine::initialize()` itself, not merely the wait.** It spawns the `WDSPwisdom` `QThread` **unconditionally** at `WdspEngine.cpp:198-200`, before `connectToRadio`'s `QEventLoop` at `RadioModel.cpp:6309-6316` is ever reached, and `TestSandboxInit` forces a cold config dir on every run. A predicate that only short-circuits `wisdomLoop.exec()` leaves a multi-minute FFTW planner thread running. Prefer an injectable initializer or a `setWisdomReadyForTest()` that marks `m_initialized` before `connectToRadio` runs.
- [ ] **Step 3:** Assert no thread named `WisdomThread` (`WdspEngine.cpp:201`) is live at teardown.
- [ ] **Step 4:** Wire `tests/fakes/P1FakeRadio.{h,cpp}`, which exists and which nothing currently drives through `RadioModel`.
- [ ] **Step 5:** Expose a helper yielding a connected model plus its fake, usable from any test. Task 3 needs it for the teardown-equivalence assertion, task 12 for a live `RxChannel`, task 20 for the Setup-page realization sweep.
- [ ] **Step 6:** `cmake --build build --target tst_connectable_radio_model && ctest -R connectable_radio_model --no-tests=error`. Commit.

### Task 3: Make connected state storage-backed

`isConnected()` derives from the connection pointer, so a remote client reports disconnected forever and `maxSlices()` short-circuits to 1 (addendum 5).

**Files:** Modify `src/models/RadioModel.{h,cpp}`. Test `tests/tst_connected_state_equivalence.cpp`.

- [ ] **Step 1: Write the failing test** using task 2's harness. Assert `isConnected()` and `(m_connection && m_connection->isConnected())` agree at every observable edge of a real connect and teardown.
- [ ] **Step 2:** Change `RadioModel::isConnected()` (`RadioModel.cpp:2476-2479`) to `return m_connectionState == ConnectionState::Connected;`.
- [ ] **Step 3:** Hoist `setConnectionState(ConnectionState::Disconnected)` from `:11648` to before the `teardownWorkerThreadedConnection` call at `:11624`. The call is unconditional either way, so the intent recorded at `:11642-11647` is preserved.
- [ ] **Step 4: Two existing test files depend on the old derivation and will fail.** `tests/tst_p2_ddc_assignment_marshalling.cpp:99`, `:105` and `tests/tst_p2_ddc_mask_ownership.cpp:266`, `:300`, `:345`, `:376` drive the `if (isConnected())` gate on the P2 DDC wire push at `RadioModel.cpp:15412`, and both state the dependency in their own comments (`tst_p2_ddc_assignment_marshalling.cpp:66-70`). **Make `injectConnectionForTest` (`RadioModel.h:1472`) also set `m_connectionState` to `Connected` for a non-null argument and `Disconnected` for nullptr**, so all 24 injection sites keep working.
- [ ] **Step 5:** The direct `m_connection->isConnected()` reads at `:9692` (auto-AGC tick), `:10446` (connect-time frequency seed) and `:10604` (`applyAlexAntennaForBand`) are null-safe but **not** behaviour-neutral: `:15412` is a gate whose truth value changes. Verify each by reading.
- [ ] **Step 6:** `cmake --build build --target tst_connected_state_equivalence && ctest -R 'connected_state_equivalence|p2_ddc' --no-tests=error`. Commit.

### Task 4: `RadioModel::Role`, the station-link seam, and the remote-inert guard

**Files:** Modify `src/models/RadioModel.{h,cpp}`. Create `src/core/session/IStationLink.h`. Test `tests/tst_remote_role_inert.cpp`.

- [ ] **Step 1: Write the failing test so it cannot pass before the code exists.** Drive **both** a `Role::Local` and a `Role::Remote` model through `connectToRadio` using task 2's harness. Assert the Remote model leaves `m_connection` null, `connectionState` Disconnected, `wdspEngine()->rxChannel(n)` null for every n, and `audioEngine()->isRunning()` false (`AudioEngine.h:207`), **while the Local model reaches Connected with all four live.** A freshly constructed Local model satisfies the Remote assertions identically (the constructor allocates `AudioEngine` and `WdspEngine` at `RadioModel.cpp:516-521` but starts neither), so without the Local arm the test is vacuous.
- [ ] **Step 2:** Add `enum class Role { Local, Remote }`, a constructor overload defaulting to `Local`, and `role()`.
- [ ] **Step 3:** Add non-owning `attachStation(IStationLink*)` / `detachStation()`, held the way `m_spectrumSink` is.
- [ ] **Step 4: Add an early return at the top of `RadioModel::connectToRadio` when `role() == Role::Remote`**, with a `qCWarning`. "Never call it" is not enforceable from inside the class. Confirm by reading that this suppresses `RadioConnection` (`:6321`), `RxDspWorker` (`:8491`), `WdspEngine::initialize` (`:6288`), `AudioEngine::start` (`:6285`) and the `wireSliceSignals` connects behind the `:9429-9431` guard.
- [ ] **Step 5:** `cmake --build build --target tst_remote_role_inert && ctest -R remote_role_inert --no-tests=error`. Commit.

### Task 5: Neutralise the five local authorities

A null connection is **not** inert (addendum 4.1). This is the largest correctness risk in the phase.

**Files:** Modify `src/models/RadioModel.cpp`, `src/models/SliceModel.cpp`, `src/core/TxSliceArbiter.{h,cpp}`. Test: extend `tests/tst_remote_role_inert.cpp`.

- [ ] **Step 1: Write five failing assertions.** (a) Apply a mirrored frequency delta to a `Role::Remote` slice after sizing the stream pool; assert `streamIndex`, `shiftOffsetHz` and `sampleRateHz` unchanged and no `sliceRetuneRejected`. (b) Apply a mirrored `dspMode` transition into `RADE_U`; assert `WdspEngine`'s RADE channel map stays empty. (c) Add a slice; assert no `setTxSlice` write. (d) Seed the per-source auto-start keys True on an isolated `AppSettings`, call `restoreSpotClientAutoStartState()` explicitly on a Local and a Remote model, and assert on the **locally observable** client-state flags (`RadioModel.h:1048-1054`), never on a socket reaching a peer. (e) **Struck 2026-08-05, moved to task 12.** `SliceMeterPump` does not exist until task 12 creates it, so this assertion is unimplementable here. Three checks agree: `src/core/meters/SliceMeterPump.*` is absent at task 5; addendum section 4.1 is titled "**Four** local authorities keep running"; and **task 12 step 4b already owns the guard**, under the title "Construct and start the pump only in `Role::Local`". Task 12 carries it. Do not fabricate the class here to satisfy the assertion.
- [ ] **Step 2: The allocator.** In `Role::Remote`, `bindSliceToStream` returns false immediately (guard before `RadioModel.cpp:4215`) and `configureStreamPool` does not size the allocator (`:3054`). The unconditional `frequencyChanged` handler at `:4654-4687` stays wired but becomes inert through the bind guard.
- [ ] **Step 3: The RADE reach-through.** Add a role check at `SliceModel.cpp:290`, ahead of the `qobject_cast<RadioModel*>(parent())` block that runs through `:344`. This is the only model-to-engine reach-through in `src/models` outside `RadioModel`.
- [ ] **Step 4: The TX arbiter.** Suppress the four `setTxSlice` writes at `TxSliceArbiter.cpp:95`, `:127`, `:178`, `:181`. **`TxSliceArbiter` holds only `MoxController*` and `QVector<SliceModel*>*` (`TxSliceArbiter.h:102-103`) and has no route to `RadioModel::role()`**, so add an explicit `setRemote(bool)` set by `RadioModel` at construction. Its persistence is already inert on an empty MAC (`:190` in save, `:198` in load), but do not rely on that.
- [ ] **Step 5: The spot collectors.** Gate `restoreSpotClientAutoStartState` (`RadioModel.cpp:2343`) on `Role::Local`. Today only the GUI calls it (`MainWindow.cpp:771`) and `nereusd` starts none, which is addendum risk 9. Gating the client now means that when the daemon-side defect is fixed the two do not both connect to the same DX cluster and both upload to PSK Reporter under one callsign.
- [ ] **Step 5a: Record that the gate is safety, not architecture.** Add a comment at the gate, and a paragraph to the verification README, stating that step 5 exists to stop duplicate cluster logins and duplicate PSK Reporter uploads under one callsign, and that **which side owns each collector is an open question R2 does not settle**. Read as an ownership decision it puts WSJT-X on the wrong machine. The measured starting position, derived 2026-08-03: `WsjtxClient` binds and **listens** on UDP 2237 (`WsjtxClient.h:103`, `:107`), so its decoder belongs wherever the operator is, which is the **client**; `PskReporterClient`, `FreeDVReporterClient` and `FreeDVRadeReporterBridge` **upload under the operator's callsign** a claim about one physical receiver on a frequency only the station knows, so they belong to the **station**; `DxClusterClient` (also the RBN instance), `PotaClient` and `SpotCollectorClient` are read-only downloads whose only consumer is a GUI panel and whose login, filters and callsign are per-operator, so they are **arguably client**. R2 turns all of them off on the client because it ships no spot delivery over the link, so nothing is lost. Do not extend this into a delivery path.
- [ ] **Step 6: Draft the two addendum risk 9 issues into the tree. Do not file them.** Write both bodies to `docs/architecture/2026-08-03-remote-daemon-r2-open-issues.md`: (a) `nereusd` builds seven spot collectors and starts none, because `restoreSpotClientAutoStartState` (`RadioModel.cpp:2343`) has one production caller and it is `MainWindow.cpp:771`; (b) `FreeDVStationModel` has no expiry in core, its only sweeper being `FreeDVReporterDialog::onIdleSweepTick`, so it grows without bound headless. Each body states the defect, the evidence, and the blast radius. Record in that file that they are **not yet filed** and that filing is the maintainer's call. Cross-reference step 5a, since (a) is the daemon half of the same ownership question.
- [ ] **Step 7:** `cmake --build build --target tst_remote_role_inert && ctest -R remote_role_inert --no-tests=error`. Commit.

### Task 6: Mirrorable slice identity and band

**Files:** Modify `src/models/SliceModel.{h,cpp}`. Test `tests/tst_slice_mirror_identity.cpp`.

- [ ] **Step 1: Write the failing test.** Walk `SliceModel`'s `QMetaObject`; assert `sliceIndex` exists, read it off a slice created after a mid-list removal, assert it equals `sliceIndex()` and not the list position. Assert `band` exists and its notify fires on a band-crossing `setFrequency`.
- [ ] **Step 2:** Add `Q_PROPERTY(int sliceIndex READ sliceIndex CONSTANT)` over the accessor at `SliceModel.h:477`.
- [ ] **Step 3: `SliceModel` has no `band()` accessor.** Only `bandChanged` (`:920`) and `m_currentBand` (`:1052`) exist, so the property does not compile without one. Add `Band band() const { return m_currentBand; }` beside `sliceIndex()`, then `Q_PROPERTY(NereusSDR::Band band READ band NOTIFY bandChanged)`. `setFrequency` already maintains the member (`SliceModel.cpp:211-214`) and `Band` is `Q_DECLARE_METATYPE`'d (`Band.h:182`).
- [ ] **Step 4: Register both in `MirrorPolicy`** and update task 7 step 5's golden list in the same commit: `sliceIndex` CONSTANT-snapshot, `band` Outbound. If `MirrorPolicy` does not exist yet, task 7 owns the entry and this task states the requirement.
- [ ] **Step 5:** Confirm by grep that no QML exists, no dynamic property shares either name, and no test asserts a property count.
- [ ] **Step 6:** `cmake --build build --target tst_slice_mirror_identity && ctest -R slice_mirror_identity --no-tests=error`. Commit.

### Task 7: `StateMirror` core: schema, direction table, forwarder

**Largest task on the critical path. Steps 1 and 2 are a go/no-go spike; do them first and stop if either fails.**

**Files:** Create `src/core/session/{MirrorSchema,MirrorPolicy,StateMirror}.{h,cpp}`. Test `tests/tst_mirror_schema.cpp`, `tests/tst_mirror_forwarder.cpp`.

- [ ] **Step 1: Spike, arity.** Prove `connect(sender, QMetaMethod, receiver, QMetaMethod)` with a zero-argument receiver works for arity 0 (`TunerModel::stateChanged`), arity 1 (`frequencyChanged`) and **arity 2 (`SliceModel::filterChanged(int, int)`)**. Only arity 1 was verified. If arity 2 fails, the whole filter surface needs a different mechanism and this plan changes.
- [ ] **Step 2: Spike, enum codec.** Prove `metaType().flags() & QMetaType::IsEnumeration` is true and `QMetaProperty::isEnumType()` is false for `dspMode` and `agcMode`, and that `QVariant(metaType, &intValue)` writes back through `QMetaProperty::write`.
- [ ] **Step 3: Write the failing schema test.** Assert every enum-typed property reports the Enum kind; assert dense ordinals are declaration-ordered and stable; assert the notify-to-ordinal map handles shared notifiers (`filterChanged` naming two properties).
- [ ] **Step 4:** Implement `MirrorSchema`: walk `propertyOffset()` to `propertyCount()`, cache once per class. **Encoding kind from `metaType().flags()`, never `isEnumType()`.** Wire kinds i64, f64, bool, utf8; float widens to f64; enums as underlying int. **Exclude `sliceLetter`** (CONSTANT, derived, the only `QChar`). **Exclude `MeterModel` entirely**: its four setters have zero callers, the connect meant to write it is an empty lambda with four `Q_UNUSED` at `RadioModel.cpp:8601-8611`, and mirroring it ships four construction defaults forever. Live TX and PA telemetry lives on `RadioStatus`, which declares zero `Q_PROPERTY` and belongs to R4.
- [ ] **Step 5: Implement `MirrorPolicy` as a default-deny table.** Unlisted means Outbound. **Seven properties are Outbound-only**, all verified to carry WRITE: `chainIndex` (`SliceModel.h:183`), `ddcIndex` (`:185`), `streamIndex` (`:195`), `shiftOffsetHz` (`:203`), `sampleRateHz` (`:227`), `widebandExtensionRequested` (`:244`), `psPaused` (`:247`). **`panKey` (`:207`) is Bidirectional**, not Outbound-only: mirrored outbound so a reconnecting client restores its layout, applied inbound under `m_applying`, with pan-affecting creation routed through the `addSliceOnPan` verb (addendum 6.1, "Client owned"). Add the golden-list guard test covering **every** enum-typed and **every** no-WRITE property, not fixed counts. **Any task landing a `Q_PROPERTY` regenerates this list.**
- [ ] **Step 6:** Implement the forwarder: one watcher per instance, single zero-argument slot, `Qt::UniqueConnection` on every notify signal, `senderSignalIndex()` to dirty ordinals, **re-read with `QMetaProperty::read`** rather than taking signal arguments. Add the explicit snapshot path for CONSTANT properties, which have no NOTIFY.
- [ ] **Step 7:** `cmake --build build --target tst_mirror_schema tst_mirror_forwarder && ctest -R mirror_ --no-tests=error`. Commit.

### Task 8: `StateMirror` inbound apply

**Files:** Modify `src/core/session/StateMirror.{h,cpp}`, `src/models/TunerModel.{h,cpp}`. Test `tests/tst_mirror_inbound.cpp`.

- [ ] **Step 1: Write the failing test.** An inbound `frequency` write lands. An inbound `sampleRateHz` write is **rejected** with a reason naming `requestSliceSampleRate` and leaves the property unchanged. An inbound write to a no-WRITE property routes to `applyMirroredValue`. An inbound apply produces **no** outbound delta.
- [ ] **Step 2:** Implement `applyInbound` under an `m_applying` guard.
- [ ] **Step 3: Add the per-model `applyMirroredValue(name, variant)` hook to every model holding no-WRITE properties.** The 24 break down as **`TunerModel` 13** (`TunerModel.h:49-61`, the entire ATU surface), `RadioModel` 3, `SliceModel` 2, plus `RadioModel::connected` handled by task 3's re-pointing. Landing the hook on two models and calling it done leaves the whole tuner inert.
- [ ] **Step 4:** Reject OutboundOnly writes with a verb-naming reason.
- [ ] **Step 5:** Extend the guard to the `nbModeChanged` peer mirror at `RadioModel.cpp:4708-4721` and the `mirrorNbTuning` helper at `:4747-4760` shared by the nine NB-tuning connects from `:4761`. Both are wired unconditionally and would otherwise emit peer deltas the daemon never asked for.
- [ ] **Step 6:** `cmake --build build --target tst_mirror_inbound && ctest -R mirror_inbound --no-tests=error`. Commit.

### Task 9: Slice lifecycle, tolerant of id reuse

**Files:** Create `src/core/session/ObjectRegistry.{h,cpp}`. Test `tests/tst_mirror_lifecycle.cpp`.

- [ ] **Step 1: Write the failing test.** With A(0) B(1) C(2), remove 1 and add again **in the same event-loop turn**, re-minting id 1 while the corpse is alive. Assert `destroy{Slice,1}` strictly precedes `create{Slice,1}` and no delta references the dead object.
- [ ] **Step 2:** Key the registry by `QPointer`, detach inside the `sliceRemoved` handler, never rely on `QObject::destroyed` (`removeSlice` calls `deleteLater()` before emitting, `RadioModel.cpp:4965-4966`).
- [ ] **Step 3:** Attach on `sliceAdded`, **not** at construction. This is what lets `object.create` carry a settled snapshot rather than the five-plus property changes `addSlice` performs before `:4870`.
- [ ] **Step 4:** Treat a create for a live id as a protocol error.
- [ ] **Step 5:** Assert removal order: `txSlice` moves off the victim while it exists, then the stream binding change, then `object.destroy`.
- [ ] **Step 6:** Assert a rejected add produces **no** `object.create`: the `:4288` reject emit with the `:4612-4625` rollback, and the `:4985` refusal before `addSlice` is entered.
- [ ] **Step 7:** `cmake --build build --target tst_mirror_lifecycle && ctest -R mirror_lifecycle --no-tests=error`. Commit.

### Task 10: Connect-time snapshot with a completion marker

**Files:** Modify `src/core/session/StateMirror.{h,cpp}`, create `src/core/session/SessionMessages.{h,cpp}`. Test `tests/tst_mirror_snapshot.cpp`.

- [ ] **Step 1: Write the failing test.** Three slices and a recording sink. Assert exact order: N schema messages, three `object.create` each with a full property bag, the snapshot-complete marker, then deltas. Assert a property changed mid-build arrives as a delta **after** the marker, never lost and never before its create.
- [ ] **Step 2:** Implement `attachSession` as **one uninterrupted event-loop turn** on the daemon's model thread.
- [ ] **Step 3:** Add the outbound coalescer: per-tick dirty set, values re-read at flush. `SliceModel::restoreFromSettings` runs roughly 75 setters (`SliceModel.cpp:1839-2150`) on the slice being restored, and `onBandButtonClicked` restores the active slice only (`RadioModel.cpp:5641`, `:5683`), plus the P1 radio-wide rate cascade at `:13664-13666`. Generalise the `TciVfoCoalescer` shape; its value type is `QString`, so this is a sibling, not a reuse.
- [ ] **Step 4:** Record the single-thread guarantee as an explicit invariant in the header.
- [ ] **Step 5:** `cmake --build build --target tst_mirror_snapshot && ctest -R mirror_snapshot --no-tests=error`. Commit.

### Task 11: Command verbs and results

**Files:** Modify `src/core/session/SessionMessages.{h,cpp}`. Create `tests/fakes/LoopbackStationLink.{h,cpp}`. Test `tests/tst_session_verbs.cpp`.

- [ ] **Step 1: Write the failing test** through the `SessionMessages` codec plus in-process dispatch over `LoopbackStationLink`. **There is no transport until task 18**, so "over the wire" here means the codec and dispatch, not a socket. With A(0) B(1) C(2), remove B so ids and positions diverge, invoke `setActiveSliceById(2)` (`RadioModel.h:770`) and assert slice **C** becomes active, not A.
- [ ] **Step 2:** Add `command.invoke` and `command.result` bound to the id-based entry points that exist: `requestSliceSampleRate` (`RadioModel.h:686`), `addSlice` (`:741`), `removeSlice` (`:745`), `addSliceOnPan` (`:779`).
- [ ] **Step 3:** Add an id-based `activeSliceIdChanged(int)` alongside the positional `activeSliceChanged`, which resolves `m_slices.at(index)` and re-emits the index (`RadioModel.cpp:5621`). R2 does not depend on it; it is added because the positional one is a live trap.
- [ ] **Step 4: Queue `setSampleRateLive` to the `RadioModel` thread.** It blocks its caller for at least 40 ms of `QThread::msleep` (`:13571`, `:13611`, `:13674`); on the session read path that stalls every inbound command. State in the header which thread the session read loop lives on.
- [ ] **Step 5:** The command result reports the **actual scope** of what it did, since a per-slice rate request escalates into the station-wide 12-step sequence (`:4136-4141`).
- [ ] **Step 6:** `cmake --build build --target tst_session_verbs && ctest -R session_verbs --no-tests=error`. Commit.

### Task 12: The per-slice S-meter gets a model home

Depends on task 2 for the harness and task 4 for the Role gate. Deliberately **not** behind task 7.

**Files:** Create `src/core/meters/SliceMeterPump.{h,cpp}`. Modify `src/models/SliceModel.{h,cpp}`, `src/gui/MainWindow.cpp`, `src/gui/meters/MeterPoller.{h,cpp}`. Test `tests/tst_slice_meter_pump.cpp`.

- [ ] **Step 1: Write the failing test.** Assert `signalStrengthDbm` is not writable, has a notify, starts at **-140.0**, emits once per distinct value. Assert a slice with no WDSP channel reads -140.0. Assert the pump stops while transmitting.
- [ ] **Step 2:** Add `SliceModel::signalStrengthDbm`, read-only telemetry, default -140.0, matching `MeterPoller.cpp:307` and `TciServer.cpp:331`.
- [ ] **Step 3:** Extract `MeterPoller::pollSliceSMeters` (`MeterPoller.cpp:362-372`) into `SliceMeterPump` with its own `QTimer`. `TciServer.cpp:308-339` already does this headless and is the proof it needs no GUI.
- [ ] **Step 4: Use `RadioStatus::isTransmitting` (`RadioStatus.h:142`) as the MOX gate**, matching the headless precedent at `TciServer.cpp:415-417` which covers MOX from any PTT source. Note that `MeterPoller`'s own `m_inTx` gate (`:290-292`) is fed only by `MoxController::moxStateChanged`, so the two are not the same flag; use the former in both the test and the code.
- [ ] **Step 4b: Construct and start the pump only in `Role::Local`.** `RadioModel` constructs `WdspEngine` unconditionally (`:516-521`), so on a remote client the pump would run a 10 Hz timer against a channel-less engine and clobber every mirrored needle with the -140.0 fallback. In `Role::Remote`, `signalStrengthDbm` is written exclusively by the mirror's inbound path.
- [ ] **Step 5:** Source the interval from the same setter the Multimeter page drives (`MultimeterPage.cpp:232`, `:264`, `:281`), or the operator's delay slider silently stops working.
- [ ] **Step 6:** Rebind the per-flag lambda at `MainWindow.cpp:1069-1075` to `signalStrengthDbmChanged`. Delete `refreshMeterPollerSlices` (`:2138-2146` and its call sites `:2152`, `:2786`, `:2788`), `pollSliceSMeters`, `setSliceChannels`, and `sliceSmeterUpdated`.
- [ ] **Step 7: Delete the connect at `MainWindow.cpp:8132`, and carry its behaviour across.** This intentionally reverts the source-tracking fix recorded at `MeterPoller.cpp:441-446`: `pollSMeter` emits with whatever source `SMeterWidget::rxMode()` selects (SignalPeak / SignalAvg / MaxBin), while `pollSliceSMeters` is SignalAvg-only by design (`:360-361`). **Give `SliceMeterPump` the same `rxMode()`-driven source selector** or the flag and the analog needle diverge by the 3 to 15 dB that comment records. Then state whether `smeterUpdated`, now with zero consumers, is deleted.
- [ ] **Step 8: Register `signalStrengthDbm` in `MirrorPolicy`** as Inbound-only telemetry via `applyMirroredValue`, and update task 7 step 5's golden list. If task 12 lands before task 7, task 7 owns the entry.
- [ ] **Step 9:** Steps 6 and 7 are `MainWindow`-internal with no unit coverage; the once-per-tick assertion is task 16 step 3's bench row. `cmake --build build --target tst_slice_meter_pump && ctest -R 'slice_meter_pump|meter_poller' --no-tests=error`. Commit.

### Task 13: Funnel every `AppSettings` mutator through `setValue`

Mandatory before any proxy. `AppSettings.cpp` has 23 direct `m_settings` mutation points and **only three** are inside `setValue` (`:687`), `remove` (`:692`) and `clear` (`:707`); the fourth `m_settings.clear()` at `:579` is `load()`'s corrupt-file fallback and needs its own decision.

**Files:** Modify `src/core/AppSettings.{h,cpp}`. Test `tests/tst_settings_mutator_funnel.cpp`.

- [ ] **Step 1: Write the failing test.** Install a recording change hook on an isolated `AppSettings(filePath)`; assert it fires for `setValue`, `remove`, `setHardwareValue`, `clearHardwareValues`, `saveRadio`, `forgetRadio`, **`clearSavedRadios`**, `setLastConnected`, `setDiscoveryProfile` and `setModelOverride`. **The hook is a `std::function<void(const QString& key)>` setter added to `AppSettings.h`; `AppSettings` has no `Q_OBJECT` (`AppSettings.h:103`) so it cannot be a signal.**
- [ ] **Step 2:** Route `setHardwareValue` (`:921-925`, `m_settings.insert` direct) and `hardwareValue` (`:927-936`, `constFind` direct) through `setValue` and `value`. Roughly 46 call sites each, covering 92 percent of the keys in a real settings file.
- [ ] **Step 3:** Route `clearHardwareValues` (`:951-960`) and the `radios/*` family: `saveRadio` (`:770-786`), `forgetRadio` (`:798`), **`clearSavedRadios` (`:803-817`, which the `:814` cite actually names)**, `setLastConnected` (`:897`, `:899`), `setDiscoveryProfile` (`:913`), `setModelOverride` (`:982`), and the direct reads at `:823` (savedRadios map walk), `:844` (savedRadio contains probe), `:851-878`, `:891`, `:906`, `:969`.
- [ ] **Step 4:** Route `allKeys()`. Four consumers prefix-scan it: `SettingsHygiene.cpp:142`, `:289`; `ExternalVariableEngine.cpp:299`, `:354`; `AudioEngine.cpp:1851`; and one GUI consumer, `AudioAdvancedPage.cpp:540`, which runs in remote mode and also depends on the delegation.
- [ ] **Step 5:** Zero changes to `AppSettings` **callers**. The header gains only the hook setter and the backend seam. Verify with `git diff --stat`.
- [ ] **Step 6:** `cmake --build build --target tst_settings_mutator_funnel && ctest -R 'settings_mutator_funnel|app_settings|radio_store|spot_settings' --no-tests=error`. A mistake in the `radios/*` helpers corrupts saved-radio state for local users with no remote involvement. Commit.

### Task 14: `classifySettingsKey` and its completeness gate

**Files:** Create `src/core/settings/SettingsScope.{h,cpp}`. Modify `tests/CMakeLists.txt`. Test `tests/tst_settings_scope.cpp`.

- [ ] **Step 1: Write the failing table-driven test.** `classify("DisplayFftSize")` is Station while `classify("DisplayNoiseFloorColor")` is OperatorLocal. `classify("TciServerPort")` is Station while `TciLogWindowGeometry` is OperatorLocal. **`hardware/oc/pennyExtCtrl`** (`OcOutputsHfTab.cpp:373`) is Station, demonstrating that `oc` is a literal segment and not a MAC. Per-pan suffixes strip before matching.
- [ ] **Step 2:** Implement as a **pure function over `QStringView`**, no I/O, no singleton, linking against one `.cpp`. Ordered rule table, first match wins: explicit exceptions, then prefixes, then whole-key rules for the roughly 85 unprefixed keys, then default **OperatorLocal**.
- [ ] **Step 3:** Strip the `_<panIndex>` suffix first (`SpectrumWidget.cpp:577-584`).
- [ ] **Step 4: The completeness test.** Extract every key literal passed to an `AppSettings` accessor from `src/core`, `src/models` **and `src/gui/setup/`**. Assert core and model keys classify non-local. Assert the inverse: **any key appearing in both a Setup page and a core consumer must classify Station.** Without the `src/gui/setup/` half, a misclassified key reads locally, writes locally, sticks in the widget, survives a relaunch, and never reaches the station. Register with `target_compile_definitions(tst_settings_scope PRIVATE NEREUS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")` (precedent `tests/CMakeLists.txt:4474-4476`) and copy the minimum-files-scanned guard from `tst_core_has_no_gui_includes.cpp:287-291`, whose own header documents that a mis-rooted scan finds zero offenders and passes.
- [ ] **Step 5:** Seed the exemption list, itself the interesting artifact: `audio/*` is read and written from `src/core` (`AudioEngine.cpp:811`, `:840`, `:1794`, `:1805`, `:1825`; `AudioDeviceConfig.cpp:78-97`) and `DaemonApp.cpp:219-220` writes `audio/Speakers/DeviceName`. Justify including `hardware/oc/*` with the live `usbBcd` / `extPa` / `pennyExtCtrl` keys (`OcOutputsHfTab.cpp:268`, `:274`, `:279`, `:318`, `:324`, `:373`).
- [ ] **Step 6:** Record `DisplaySpectrumFps` as a **known unresolved straddle**, read at `MainWindow.cpp:1482` into `FftPoolConfig` and at `:3499-3507` into the client paint timer. Classify the four `FftPoolConfig` keys Station and do not pretend the exception list closes it.
- [ ] **Step 7:** `cmake --build build --target tst_settings_scope && ctest -R settings_scope --no-tests=error`. Commit.

### Task 15: `SettingsProxy` and `SettingsProxyServer`

**Files:** Create `src/core/settings/{ISettingsBackend.h,SettingsProxy,SettingsProxyServer}`. Modify `src/core/AppSettings.{h,cpp}`. Test `tests/tst_settings_proxy.cpp`.

- [ ] **Step 1: Write the failing routing test.** Install a fake backend handling only `hardware/`; assert `value("hardware/x/y")` reaches it, `hardwareValue(mac, k)` reaches it, and an OperatorLocal key does not. Assert `setRemoteBackend(nullptr)` leaves today's path byte-identical.
- [ ] **Step 2:** Add `ISettingsBackend` and `AppSettings::setRemoteBackend` (non-owning, nullptr default) with one-branch delegation in `value`, `setValue`, `contains`, `remove` and `allKeys`. **Reads are synchronous out of the cache and never touch the network.** Record as a header invariant and assert it: no event-loop spin, no socket wait inside `value()`. `AppSettings::value` runs inside widget and model constructors, so a blocking read deadlocks the GUI.
- [ ] **Step 3:** Add `snapshot(prefixes)` returning **fully qualified** keys (unlike `hardwareValues`, which strips, `:944`) and `applySnapshot` that **merges rather than replaces**. Value type is `QString`, not `QVariant`: `setValue` already collapses to QString (`:687`) and `value()` returns `QVariant(QString)` (`:680`).
- [ ] **Step 4:** Carry a **proven-unset set** alongside the value map. `value()` returns the caller's default for an unwritten key (`:676-683`) and Setup pages depend on it: `GeneralOptionsPage.cpp:265` expects "United States" when `Region` was never set.
- [ ] **Step 5: Scope the snapshot.** A measured file is 15,201 keys and 3.19 MB across five MACs. Snapshot the connected MAC plus global daemon prefixes, roughly 2,900 keys and **250 to 340 KiB** depending on key encoding. Include `hardware/oc/*` explicitly.
- [ ] **Step 6:** Writes are optimistic with an **origin tag** echoed on the daemon broadcast, so a slider drag is not rolled back by its own echo. Rejection reverts the cache and emits `SettingsProxy::valueRejected(QString key, QVariant restored)`; `AppSettings` has no signals, so the proxy owns it.
- [ ] **Step 7:** Gate the Setup dialog on the Decisions-table condition: `ready()` **and** a non-empty snapshot **or** the first-run seed marker task 1 step 7 writes. `ready()` alone is insufficient because task 1 gives the daemon an empty profile, so a legitimately empty snapshot would let 187 widget constructors bake ship defaults into the station store.
- [ ] **Step 8:** Offline after handshake: reads serve the last snapshot, writes are **dropped, not queued**. Reconnect re-snapshots and reports what was not applied.
- [ ] **Step 9:** Log every proxied read that resolved locally, so the acceptance run produces a scannable list.
- [ ] **Step 10: `FaultLog` decision.** `FaultLog::load()` is private and called only from the constructor (`FaultLog.h:61-62`), so it cannot re-read after a snapshot lands and on a remote client would show nothing forever. Either make it public as `reload()` and call it on snapshot completion with a stated ordering against `RadioModel` construction, or state that R2 deliberately declines to carry the fault log and cite the addendum sentence being declined. Note also that `AppSettings`'s pre-existing `m_stationSettings` pair (`setStationValue :721`, `setStationName :731`, `stationValue :712`) has zero call sites, is out of scope, and its name collides with task 14's Station scope class.
- [ ] **Step 11:** `cmake --build build --target tst_settings_proxy && ctest -R settings_proxy --no-tests=error`. Commit.

### Task 16: Local direct mode regression gate

Three tasks changed local behaviour on purpose (3, 12, 13). This task owns proving nothing else did, and a red result stops the phase before libcrypto lands.

- [ ] **Step 1:** `cmake --build build --target tests_models` then `ctest -L models --no-tests=error` (a real label), plus the targeted set below.

  **Corrected 2026-08-06.** The regex this step originally named contained a component, `radio_store`, that matches **zero** tests in this tree, so the saved-radio coverage it was meant to pin would have been silently skipped. `--no-tests=error` does not catch that: it only fires when the *whole* selector matches nothing, and the other components matched. This is the same disease as the `-L settings` trap, one step milder. Measured at `10d1d35c`, the real names are `tst_connection_panel_saved_radios` (saved-radio round-trip) and `tst_settings_hygiene` (the station-value validator that prefix-scans `allKeys()`). Use:

  ```
  ctest --test-dir build -R 'settings_mutator_funnel|app_settings|connection_panel_saved_radios|settings_hygiene|spot_settings|connected_state_equivalence|p2_ddc|slice_meter_pump|meter_poller|settings_scope|settings_proxy' --no-tests=error
  ```

  **Before trusting it, print what it selected** with `ctest --test-dir build -N -R '<same regex>'` and confirm every component contributed at least one test. `settings_scope` and `settings_proxy` are created by tasks 14 and 15, so they legitimately match nothing until those land; every other component must match.
- [ ] **Step 2:** Full `ctest --no-tests=error`. All green.
- [ ] **Step 3: Bench, local direct mode, no daemon. Run it on the ANAN-G2E.** Follow the "Bench hardware: the G2E, never the G2" procedure above: discover, list responders with MAC and board type, pin the G2E's MAC, confirm with the maintainer, connect. Record the result in the verification README against the pinned MAC. Connect to the G2E. Verify: per-slice S-meter needles move and the **active slice's flag is written once per tick, not twice**; **set the analog S-Meter to Peak and to MaxBin and confirm the flag bar agrees with the needle** (task 12 step 7); the Multimeter delay slider still changes meter cadence; disconnect and reconnect cleanly, with the reordered state transition causing no visible change; saved radios survive a relaunch. Steps 1 and 2 are the automated half of this gate and **are** executed; a red result there still stops the phase.
- [ ] **Step 4:** Record in `docs/architecture/2026-08-03-remote-daemon-r2-verification/README.md`. Commit.

### Task 17: TLS certificate provisioning via libcrypto

**Files:** Create `src/core/security/CertificateStore.{h,cpp}`. Modify `CMakeLists.txt`. Test `tests/tst_certificate_store.cpp`.

- [ ] **Step 1: Write the failing test.** Generate a self-signed certificate and key on first run; assert both parse back through `QSslCertificate` and `QSslKey`, the fingerprint is stable across reloads, and a second run reuses rather than regenerates. **Guard the file-permission assertion with `#ifdef Q_OS_UNIX` and `QSKIP` on Windows**, where `QFile::permissions()` mirrors owner bits into group and other.
- [ ] **Step 2:** Add OpenSSL libcrypto to `CMakeLists.txt` for all three platforms.
- [ ] **Step 3:** Implement generation, storage beside the daemon profile, and fingerprint export in the form the identity design displays at pairing time.
- [ ] **Step 4:** Handle `QSslSocket::supportsSsl()` false, which occurs where Qt is built against Schannel rather than OpenSSL on Windows. Fail with a clear message naming the cause.
- [ ] **Step 5:** `cmake --build build --target tst_certificate_store && ctest -R certificate_store --no-tests=error`. Commit.
- [ ] **CI gate, not a test step:** the three release artifacts must still build (Linux AppImage, macOS DMG, Windows portable ZIP and NSIS installer). Confirm on the PR, not locally.

### Task 18: The `wss` session

**Files:** Create `src/core/session/{StationServer,StationClient}.{h,cpp}`, `src/core/security/TokenStore.{h,cpp}`. Test `tests/tst_station_session.cpp`.

- [ ] **Step 1: Write the failing test.** Client and daemon in one process complete the full section 7.0 sequence over `wss` on loopback. Assert a **major** version mismatch refuses with both versions named; a **minor** mismatch negotiates down; a bad token is refused and rate-limited; a second authenticated connection **preempts** the first and the displaced session is told why. **`QSKIP` the `wss` slots when `!QSslSocket::supportsSsl()`, naming the Qt TLS backend, and keep the message-order assertions on a non-TLS in-process link so the protocol half is covered unconditionally.**
- [ ] **Step 2:** Implement the transport with **no ICE and no codecs**. TLS from task 17.
- [ ] **Step 2a: Configure the control-channel heartbeat. Added 2026-08-08 by maintainer directive, reversing the Decisions-table deferral to R4.** A TCP connection that dies silently (laptop lid, cell handoff, NAT timeout) never produces a close, so the daemon would sit believing a dead client is alive. Parent section 12.1 calls the TX watchdog a hard requirement and the mechanism it depends on had been deferred into the same release that first enables TX, giving it zero soak time before becoming load-bearing. Landing it now buys months of soak.

  **This is not an interim wire format.** RFC 6455 ping/pong is transport-level, so the global constraint against formats R3 deletes does not apply. Nothing here is temporary.

  **Follow the in-tree precedent, do not invent one.** `TciServer.cpp:129-141` already runs a 20 s `QTimer` calling `QWebSocket::ping()`, itself ported from Thetis `TCIServer.cs:2650-2654 [v2.10.3.13]`. Copy that shape.

  For sanity-checking your interval only, a shipping configuration on real internet links is piHPSDR's 15 s heartbeat with a 30 s receive timeout and a 5 s send timeout (`server_thread.c:925-934 [@4aa95c5]`, verified 2026-08-08). Our 20 s `TciServer` precedent is the closer match; do not copy their numbers blindly.
- [ ] **Step 3:** Implement `TokenStore`: generated, never user-chosen, rate-limited verify.
- [ ] **Step 4: On first run the daemon prints the generated token and the task 17 certificate fingerprint to `qCInfo`** and stores them under the reserved daemon profile. Parent 7.1 requires the distribution mechanism be specified before R2, and without it the acceptance run cannot authenticate.
- [ ] **Step 5:** Implement the capability descriptor. Advertise **effective** limits, not board limits (parent 4.5).
- [ ] **Step 6: Apply the handshake outcome on the client.** On completion the client writes the station's board capabilities, `userDdcCount` and effective slice limit into its `RadioModel` and **drives the connection state to Connected**. `setConnectionState` is private (`RadioModel.h:2613`) and the only public entry today is the test seam at `:1438`, so name the production entry point this step adds. Without it, task 3's storage-backed `isConnected()` is never true remotely, `maxSlices()` keeps returning 1 (`RadioModel.cpp:2975-2982` via `boardCapabilities()` defaulting to `HPSDRHW::Unknown`), 14 GUI sites still read disconnected, and task 20's add-slice acceptance cannot pass.
- [ ] **Step 7:** Wire `StateMirror` and `SettingsProxy` onto the session. **Task 5 must already have landed**, or capability apply sizes the stream pool.

  **Added 2026-08-06, three items task 15 handed forward that this step owns.** Each is recorded here because the place task 15 recorded it is a header this step has no reason to open.

  - **Call `FaultLog::reload()` on the first `SettingsProxy::snapshotApplied`**, on both of `RadioModel`'s instances (`RadioModel.cpp:1143-1144`). Task 15 made the private `load()` public as `reload()` precisely for this and proved it works end to end, but left the wiring here. Without it a remote client's fault list is empty forever, and task 20's acceptance would not catch it because noticing needs PGXL or TGXL hardware.
  - **Install the backend after `CoreInit`'s schema migrations.** `AppSettings::load()` bulk-populates directly and cannot leak, but the migrations at `AppSettings.cpp:1138`, `:1171` and `:1220` do use `setValue()`, so a backend installed before them would push the local file's migrated keys to the station.
  - **Do not set `SettingsProxy::ready()` before model construction.** Several models do `contains()`-then-seed against Station-classified prefixes in their constructors (`SliceModel.cpp:2241`, `:2316`; `NotchModel.cpp:662`; `FilterPresetStore.cpp:83`; `TciServer.cpp:114-118`). They are prevented from writing ship defaults into the station store **only** because writes are dropped while not ready.
- [ ] **Step 8:** Assert schema version skew is caught by name comparison at handshake, which the parent describes in prose and assigns to nothing.
- [ ] **Step 9:** `cmake --build build --target tst_station_session && ctest -R station_session --no-tests=error`. Commit.

### Task 19: Link loss, daemon restart, and reconnect

Ctrl-C on `nereusd` is the first thing anyone does on a bench, and nothing currently defines what the client does with a mirror of objects that no longer exist.

**Files:** Modify `src/core/session/{StationClient,StateMirror}.{h,cpp}`. Test `tests/tst_session_link_loss.cpp`.

- [ ] **Step 1: Write the failing test.** Kill the daemon mid-session. Assert the client tears down the mirror registry, **drives connection state back to Disconnected** (the inverse of task 18 step 6), enters a defined stale state rather than showing stale values as live, keeps serving the `SettingsProxy` cache for reads, drops writes, and reconnects with a fresh snapshot and a bumped epoch.
- [ ] **Step 2:** Implement mirror teardown and the epoch bump.
- [ ] **Step 3:** Use an **owned single-shot `QTimer` with a cancellable slot**, not static `QTimer::singleShot`. Parent section 13 records that `PgxlConnection` and `TgxlConnection` get this wrong and that cancellability matters more here because this subsystem gates a transmitter.
- [ ] **Step 3a: Test that a silently-dead peer is detected, not just a clean close. Added 2026-08-08 by maintainer directive.** Step 1 kills the daemon, which produces a TCP close and is the easy case. This step covers the case that motivated pulling the heartbeat into R2: a peer that stops responding **without** closing, which is what a laptop lid, a cell handoff or a NAT timeout actually produces.

  Simulate a peer that accepts the connection and then goes silent, and assert detection happens within the configured timeout rather than never. **A test that only kills the process proves nothing about this path**, because the close does the work. Task 18 step 2a configures the interval; assert against that configuration rather than hardcoding a duplicate number.
- [ ] **Step 4:** Assert a reconnect after a daemon restart with different state converges without a client relaunch.
- [ ] **Step 5:** `cmake --build build --target tst_session_link_loss && ctest -R session_link_loss --no-tests=error`. Commit.

### Task 20: Remote-mode GUI gating, `--station`, and the acceptance run

**Files:** Modify `src/gui/MainWindow.{h,cpp}`, one Setup page group. Test `tests/tst_remote_gui_gating.cpp`.

- [ ] **Step 1a: Write the failing Setup-dialog test.** **No test in the suite constructs `MainWindow`, and three test banners say it cannot be done** (`tst_notch_hit_test.cpp:800-802`, `tst_mainwindow_status_bar_safety.cpp:8-13`, `tst_pan_active_slice_sync.cpp:59-60`). Instead construct `SetupDialog` against a `Role::Remote` `RadioModel`, the pattern already used at `tests/tst_setup_dialog_lazy_pages.cpp:138-139`, realize every registered page, and assert no null dereference and no reach-through to a null `connection()` or `wdspEngine()`.
- [ ] **Step 1b:** Pin the `MainWindow` gating entry points by name off `MainWindow::staticMetaObject`, the seam `tst_notch_hit_test.cpp:812-825` uses.
- [ ] **Step 2:** Add `--station wss://host:port` and `--token`, plus one Setup field group carrying both. No connect screen, no discovery, no pairing UI.
- [ ] **Step 3: Enumerate the gating sites before sizing. Roughly 77 across 14 files:** `connection()` 34, `wdspEngine()` 17, `audioEngine()` 17, `receiverManager()` 9. **`audioEngine()` and `receiverManager()` return non-null inert objects in remote role** (constructed unconditionally at `RadioModel.cpp:516-521`), so they fail **silently** rather than crashing, and a null-dereference assertion cannot catch them. The capability-driven gate is the only thing that does. Gate by **one mechanism**, not per-page. Produce the enumerated list as a plan artifact.
- [ ] **Step 4:** Gate `NetworkDiagnosticsDialog` and the connection UI. **Also refuse MOX in `Role::Remote` through the existing `MoxController` pre-check callback** (installed at `RadioModel.cpp:9080`) with a reason naming R4. MOX is reached via `RadioModel::setMox` (`:12579`), not through `connection()` or `wdspEngine()`, so step 3's enumeration misses it, and it is the only operator control that would otherwise key a transmitter from a phase with no TX path.
- [ ] **Step 5: The acceptance run. Run it on the ANAN-G2E.** Follow the "Bench hardware: the G2E, never the G2" procedure above and reuse the MAC pinned at task 16 step 3. Two processes, one host, the G2E on the daemon. Verify the token round-trip, then VFO, band, mode, filter, AGC, NR, NB, SNB, APF, squelch, RIT, XIT, antenna, add slice, remove slice, TX slice display, and Setup pages round-trip. Verify the per-slice S-meter is live. **Verify the panadapter is blank, the waterfall is blank, and the speakers are silent, and record that as expected.** Spot collectors are expected inert. **Execute now, without a radio, the subset that does not need one:** both processes launch, the `wss` handshake completes, the token round-trip succeeds, a bad token is refused, the snapshot arrives, and the Setup gate opens. Record which rows ran and which are PENDING; do not blur the two.
- [ ] **Step 6:** Scan the proxied-read-resolved-locally log from task 15 step 9 for keys that should have been Station.
- [ ] **Step 6a: One hostile-value row, added 2026-08-06.** Task 15's review established that inbound Station writes reach the daemon's store **unvalidated**: `SettingsHygiene` is invoked once on connect (`RadioModel.cpp:12026`), only reports rather than clamps, and reports to two GUI diagnostics pages `nereusd` does not link. Task 15 added a bounds check for the step attenuator specifically, because `StepAttenuatorController::loadForMac` (`:1067`) demonstrably bypasses the clamp at `:217-218` that `setAttenuation()` applies. **The general gap is deliberately left open for a later phase.** Send one out-of-range Station value from the client and record what the daemon does with it. This row is expected to document a gap, not to pass.
- [ ] **Step 7: Re-run the task 16 gate and confirm the task 17 release artifacts still build**, because tasks 17 and 20 both landed after that gate and both have large local blast radius. The automated half (task 16 steps 1 and 2) is re-run here, and the task 16 step 3 bench is re-run here too, on the same pinned G2E MAC, because tasks 17 and 20 both landed after that gate. Release artifacts are confirmed on the PR, not locally.
- [ ] **Step 8:** Record the matrix in the verification README. Full `ctest --no-tests=error`. Commit.

---

## Self-Review

- [ ] The test harness (task 2) precedes every task that consumes it: 3, 12, 20.
- [ ] No ctest invocation uses a label that does not exist, and every one carries `--no-tests=error`.
- [ ] No task optimises for R2 shipping alone.
- [ ] Task 5 lands before task 18, so the allocator cannot re-arm on the client.
- [ ] Task 15 lands before task 18, which wires the proxy onto the session.
- [ ] Task 12 does not depend on task 7.
- [ ] The completeness test in task 14 scans `src/gui/setup/`, not only core.
- [ ] The three tasks changing local behaviour (3, 12, 13) are upstream of the task 16 gate, and task 20 step 7 re-runs it after the two later high-blast-radius tasks.
- [ ] Task 18 step 6 applies the handshake outcome, without which task 3 is inert.
- [ ] Every new `Q_PROPERTY` (three: `sliceIndex`, `band`, `signalStrengthDbm`) has an owning `MirrorPolicy` entry.
- [ ] `setSampleRateLive` is queued off the session read path (task 11 step 4).
- [ ] No em-dash or en-dash anywhere in this file.
