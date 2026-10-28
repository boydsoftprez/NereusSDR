# Remote Transmit Hold Then Cut Implementation Plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** When a remote device's link or microphone audio goes quiet, the Core first
holds the radio silent while staying keyed, and unkeys only if the silence lasts: hold
at 400 ms (link) or 250 ms (mic), cut at 3000 ms (link) or 10 s with no audio packets
(any mode), a closed link cut at once.

**Architecture:** A per-reason RF hold mask on `TxChannel` sends all-zero I/Q blocks
while any hold is on, without touching MOX, the run sequence or the RF gate, so it
reopens by itself. `RemoteTxWatchdog` and `RemoteTxCutGuard` gain a hold stage at
`kLinkHoldMs` before their cut at `kLinkCutMs`; `RemoteMicReceiver` and
`StarvationPolicy` gain a hold at 250 ms and a no-audio cut at 10 s. `StationServer`
wires the holds to the TX channel, publishes them on `TransmitState`, and logs each
hold; the Core window, the desktop remote window and the phone show "Held" in amber.

**Tech Stack:** C++20, Qt6 (QtTest), the Core's session and media paths
(`StationServer`, `DaemonMediaController`, `RemoteMicReceiver`), `TxChannel`'s TX
worker thread, the station mirror (`MirrorPolicy`, `tests/data/link/v1/surface.json`),
`MainWindow`'s `StatusBadge`; Swift (NereusKit, Swift Testing, the app's XCTest shot
tests) for Task 8.

**Spec:** [Remote transmit stop: hold first, cut later](2026-10-26-remote-transmit-stop-design.md),
approved by JJ on 2026-10-26 ("yes for that hold then cut", relayed by the manager
session), with the amber Held state. Every decision is settled there or in the
"Decisions settled for this plan" section below; this is how it gets built.

## Global Constraints

- Read `CLAUDE.md`, `CONTRIBUTING.md` and `docs/development/fast-test-loop.md` first.
  They bind every task.
- No upstream code is ported. Thetis, piHPSDR, AetherSDR and the gateware were read for
  facts only (spec section 4). New files carry `no-port-check: NereusSDR-original.` in
  their header; no attribution header, no provenance row.
- Every touched source or test file gets a "Modification history (NereusSDR)" line in
  the file's existing style and order (some files list newest first, some oldest
  first: follow the file), for example
  `2026-10-26: Remote transmit hold then cut: ... J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.`
  New files start the block.
- C++ style: no `goto`, no raw `new`/`delete` (Qt parent or `unique_ptr`), `constexpr`
  constants named `kPascalCase`, braces on all control flow, `m_camelCase` members,
  `auto` only when the type is obvious, `qCWarning(lcCategory)` for errors, no
  exceptions. Platform guards `Q_OS_WIN` / `Q_OS_MAC` / `Q_OS_LINUX` only.
- Rule R1: nothing under `src/core/` or `src/models/` includes a GUI header.
- Cross-thread values are `std::atomic`; never hold a mutex in the audio callback or
  the TX worker's block path. The cut guard's callbacks run on its own thread with its
  mutex held and must not block or wait on another thread (apart from the TX
  channel's existing short sender spin).
- This is the transmit safety path. Never weaken a stop the spec keeps: a closed
  session cuts at once; RF stops within 400 ms of link silence (250 ms of mic silence
  in AM, SAM, FM, DRM, RADE); every key still ends by 3000 ms of link silence or 10 s
  without mic audio packets; TUNE and two-tone are never held or cut by the mic. Never
  key the radio, start a tune or touch JJ's station from a task. Hardware evidence is
  JJ's, with his go each time.
- Settings go through `AppSettings`, never `QSettings`. This plan adds no setting.
- Don't remove code you didn't add, except where a step below names the change
  (`kLinkLossDeadlineMs` is replaced by `kLinkHoldMs` and `kLinkCutMs`, by the spec).
- Old peers keep working: new `TransmitState` properties are appended after the last
  existing one, so earlier ordinals stay put; `txStateVersion()` stays 2 (an older
  client ignores unknown properties, as with `stopEpoch` and `highSwr`).
- American spelling and no em-dashes in all new text (comments, test names, strings,
  docs). No source cites in user-visible strings.
- Commits are GPG-signed (`git commit -S`); never `--no-gpg-sign`. No
  `Co-Authored-By` trailer. If signing fails, stop and report BLOCKED. Never rebase or
  force-push. The pre-commit hook needs the upstream clones; export first:
  `export NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis NEREUS_MI0BOT_DIR=/Users/j.j.boyd/mi0bot-Thetis NEREUS_DESKHPSDR_DIR=/Users/j.j.boyd/deskhpsdr NEREUS_FREEDV_DIR=/Users/j.j.boyd/freedv-gui`.
- Branch: `claude/tx-stop-design` (PR 668), checked out at
  `/Users/j.j.boyd/NereusSDR-crew/2026-10-26-tx-hold-then-cut/trunk`. It already
  carries `origin/main` and PR 652 (head `c1ae7e7c0`, the session keepalive copy)
  merged in. Implementers work there, one at a time.
- Builds and tests are admitted by load: wrap every compile or test run in
  `~/.config/nereus/work/lane-admit.sh take <name>` / `release <name>` (a `trap` on
  EXIT releases it), run long commands in the background, and stop if free disk is
  under 40 GB. Swift app tests also take `/tmp/nereus-ios-full.lock` first
  (ios/README.md).
- Build dir: `build/` in the trunk, reused by every task. Configure once, with the
  network denied, never downloading:

  ```bash
  DEPS=~/.config/nereus/work/pr-merge-review/review-deps
  ARGS=()
  for d in "$DEPS"/*-src; do
    n=$(basename "$d" -src | tr '[:lower:]' '[:upper:]')
    ARGS+=("-DFETCHCONTENT_SOURCE_DIR_${n}=${d}")
  done
  sandbox-exec -p '(version 1)(allow default)(deny network*)' \
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON \
      -DENABLE_DFNR=OFF -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
      -DOPUS_URL=/Users/j.j.boyd/.config/nereus/work/deps-cache/nereus-core-pins-0368ff16/opus/opus-940d4e5-with-model.zip \
      -DOPUS_URL_HASH=SHA256=740a5220180aee546f97030118517eedafa7956fdfbc8e8432d91f2d20aac4b7 \
      "${ARGS[@]}"
  ```

  Then grep the configure output for "Performing download step"; it must not appear.
- Build single test targets, never the whole suite by default:
  `cmake --build build --target <t1> <t2>` then
  `ctest --test-dir build -R '^(<t1>|<t2>)$' --output-on-failure` (ctest sets
  offscreen and a per-test home). Tests labeled `realtime` run alone, never beside
  another ctest run. A direct binary run sets `HOME`, `CFFIXED_USER_HOME` and
  `NEREUS_TEST_HOME` to one scratch folder, `QT_QPA_PLATFORM=offscreen`, and passes
  `-maxwarnings 0`.
- Swift tests: `ios/scripts/swift-test.sh --jobs 2 --filter <SuiteOrTest>`. App tests:
  `ios/scripts/generate-project.sh`, then
  `xcodebuild -project ios/NereusSDR.xcodeproj -scheme NereusSDR -destination 'platform=iOS Simulator,name=iPhone 17' -collect-test-diagnostics never -parallel-testing-enabled NO -jobs 2 test -only-testing:<Target/Class>`.
- A failed test is never ignored or retried away. A failure under load is a finding:
  record the load average and the assertion.
- JJ's machines (the STRIX, the Zephyrus, the Rock, his phone) and his NereusSDR
  settings are off limits; logs are read-only.

## Decisions settled for this plan

The spec leaves these to the plan. Each follows from the spec and the code; none
changes the approved behavior.

1. **How a hold silences the radio.** `closeRfGate()` cannot be the hold: only a new
   key reopens it (it bumps `m_runSequence`, and the reopen re-runs channel-on and
   emits `rfGateOpened`, which the TGXL path acts on). The hold is a separate mask:
   while any bit is set, the TX worker still runs `fexchange0` and sends a block of the
   usual size and timing, with every sample zero. The radio's TX stream never
   underruns (an HL2 drops PTT on an empty buffer), WDSP and RADE state keep running,
   and the AM monitor tap gets the same zeros the radio gets. The MON siphon is
   unchanged.
2. **Three hold reasons, each released only by its owner:** `Link` (the main-thread
   watchdog), `Mic` (starvation), `Guard` (the off-main cut guard). The gate is silent
   while any is set. All are cleared when MOX returns to receive, so a new key always
   starts unheld.
3. **What ends a link hold.** Only a counted keepalive (newer sequence, current epoch,
   any path). The cut guard, which sees raw "tx" channel receipts, releases its own
   `Guard` bit on any receipt that brings its silence back under `kLinkHoldMs`; the
   `Link` bit still needs the main thread's counted keepalive.
4. **A watch with VOX armed and no key** has nothing to hold: only its cut applies (VOX
   that device armed turns off at `kLinkCutMs`, not at 400 ms).
5. **The late turn.** Each stage (hold and cut) keeps today's one extra event-loop turn
   for keepalives queued behind a main-thread stall, once per overdue period.
6. **What a client sees.** `TransmitState` gains four appended properties: `rfHeld`
   (bool), `rfHeldReason` (`"link"` or `"mic"`, `"link"` when both; empty when not
   held), `rfHoldSerial` (quint64, advances when a hold ends) and `rfLastHoldMs`
   (qint64, the length of the hold that ended last), all on a new `holdChanged`
   signal. They describe the keyer's transmission only (a hold for a device that is
   not `keyedBy` is not published). `txStateVersion()` stays 2.
7. **Showing "Held".** The TX indicator shows "Held" in amber while `rfHeld` is true.
   A client that never saw `rfHeld` true for a hold (its link was the thing that went
   quiet) shows "Held" for `min(rfLastHoldMs, 3000)` ms from when it learns the hold
   ended, only while still keyed. The words and amber come from the existing styles:
   `StatusBadge::Variant::Warn` on the desktop, `ChromeColours.buttonOnAmber*` on the
   phone. The stop reasons and their wording are unchanged.
8. **Logging.** `ControlLog` (`lcControlLog`, "nereus.station") logs each hold's start
   and end: `Transmit hold for <hex>: no keepalive for <N> ms; RF off, still keyed` (or
   `no microphone audio for <N> ms`), and
   `Transmit hold for <hex> ended after <N> ms: <keepalives returned | audio returned | cut | the key ended | link closed>`.
   The cut guard's own hold is not published or logged separately; the main
   watchdog's hold covers it once the main thread runs.

## What already exists

- `src/core/safety/RemoteTxWatchdog.{h,cpp}`: `kKeepaliveIntervalMs = 100`,
  `kLinkLossDeadlineMs = 400`, `kLateCheckSlackMs = 5`, `kFirstReconnectMs = 1000`, the
  ordering `static_assert`s (`.h:128-135`); `Watch {keyed, voxArmed, epoch, lastMs,
  lastSequence, lateTurnGiven}`; `Hooks {clock, startTimer, stopTimer, stop,
  deviceName}`; `setKeyed`, `setVoxArmed`, `released`, `keepalive(deviceId, sequence,
  epoch, Path, ageMs)` (`.cpp:119-160`, trips an expired watch at `:133-136`),
  `linkClosed`, `onTimer` (`:179-217`), `tripQuiet(deviceId, judgedSilentMs)` (`:170`),
  `trip` (`:219-243`), `reschedule` (`:245-274`, capped at `kLinkLossDeadlineMs+1`);
  signals `tripped(deviceId, linkClosed, silentMs)` and `keepaliveHeard`.
- `src/core/safety/RemoteTxCutGuard.{h,cpp}`: one `std::thread` with a mutex and
  condition variable; `CutFn(mediaEpoch, armSerial, silentMs)` called once per arm on
  the guard thread with its mutex held; `kDeadlineMs = kLinkLossDeadlineMs`; `arm`,
  `disarm`, `heardOffMain(epoch, at)` (starts judging), `heardOnMain(at)`,
  `isJudging`, `armSerial`. Wired at `StationServer.cpp:3078-3096`; the cut path is
  `onTransmitCutOffMain` → `finishTransmitCutOnMain` (`:10601-10673`) → `tripQuiet` and
  `RadioModel::catchUpTransmitCut` (`RadioModel.cpp:30152`).
- `src/core/safety/TransmitCutLine.{h,cpp}`: `setTargets(RadioConnection*, TxChannel*)`
  (main), `cut()` (any thread: `closeRfGate()` then `cutTransmit()`), under a mutex.
- `src/core/TxChannel.{h,cpp}`: `std::atomic<bool> m_running` (the RF gate; also
  "keyed"), `m_runSequence`, `m_txIqSendersInFlight`; `closeRfGate()` (`.h:690`),
  `isRfGateOpen()`, `isRfGateOpenForSequence`, private `closeRfGateAndWaitForSender`
  (`.cpp:694`); the block path `driveOneTxBlockFromInterleaved` (`.cpp:3786`) with the
  sender section at about `.cpp:3905-3916` (`sendTxIq`, then the AM monitor tap, then
  `sip1OutputReady`).
- `src/models/RadioModel.{h,cpp}`: `txChannel()` (`.h:3981`), `openTxRfGate()`
  (`.cpp:35213`), `stopTransmitNow` (`:30030`), `catchUpTransmitCut` (`:30152`), the TX
  time-out driven only by `MoxController::moxChanged` (`:2851-2858`).
- `src/core/safety/StarvationPolicy.{h,cpp}`: `StarvationAction {KeepKeyed, Unkey}`;
  `actionFor(DSPMode)` (`KeepKeyed` for LSB, USB, DSB, CWL, CWU, DIGL, DIGU, SPEC;
  `Unkey` for AM, SAM, FM, DRM, RADE modes and an unknown mode); `Hooks
  {transmitMode, microphoneUnused, stopAllTx, deviceName}`; `stopMessage` ("No
  microphone audio arrived from %1, so the Core stopped transmitting.");
  `onStarved(deviceId, starved)`.
- `src/core/session/media/RemoteMicReceiver.{h,cpp}`: `RemoteMicConfig::kMaxDepthMs =
  120`, `kStarvationMs = 250` (`.h:211`), the ordering assert at `.h:234`; atomic
  `m_lastAudioMs` (moved by every accepted packet), `checkStarvation` (`.cpp:1374-1391`,
  emits `starved(true)` once), `starved(false)` on audio (`:1150-1153`),
  `setWatching(false)` and `stop()`. Watching is decided by
  `DaemonMediaController::refreshMicWatching` (`DaemonMediaController.cpp:3366-3378`);
  `starved` reaches `StationServer::remoteMicStarved` (`StationServer.cpp:10710-10718`)
  via `DaemonMediaController.cpp:3310-3315`.
- `src/core/session/StationServer.{h,cpp}`: watchdog hooks (`:3024-3049`), `tripped`
  to `ControlLog::watchdogStopped` (`:3052-3062`), starvation hooks (`:3108-3124`,
  `microphoneUnused` = TUNE or two-tone), `followKeyedForWatchdog` (`:10488-10553`),
  keepalive inputs (session `:4190-4198`, auxiliary `:2632-2636`, "tx" channel
  `:10688-10708`), `noteKeepaliveForCutGuard` (`:10560`), `linkClosed`
  (`:5351-5371`), `recordTransmitStop`, `txStateVersion()` = 2 (`.h:1639`).
- `src/core/session/TransmitStateFacade.{h,cpp}` (`TransmitState`): property glossary
  `.h:7-61`, `Q_PROPERTY`s ending with `cfcPeakDb` (`.h:213`), signals `stateChanged`,
  `stopChanged`, `holderChanged`, `metersChanged`; stop codes `kStopLinkLost`,
  `kStopMicStarved`. Mirrored under `"txState"` (`StationServer.cpp:1619, 7892`);
  directions in `MirrorPolicy.cpp` (TransmitState block ending about `:1171`).
- `src/core/session/ControlLog.{h,cpp}`: `lcControlLog` "nereus.station",
  `watchdogStopped` (`.cpp:697-724`), the keepalive gap line (`:686-692`).
- Desktop: `MainWindow` `m_txStatusBadge` (`MainWindow.cpp:14101-14169`, follows
  `MoxController::moxStateChanged`, TX Inhibit icon `:14474-14509`);
  `StatusBadge::Variant::Warn` (`StatusBadge.h:26`); the Core window reaches its
  server through `m_desktopStationController->server()` (`:2742-2746`); the remote
  window's `TransmitState` is the one `StationClient` fills (`StationClient.cpp:4044-4051`).
- Phone: `ios/NereusKit/Sources/NereusLink/TransmitStateReport.swift`,
  `ios/NereusApp/Main/TransmitModel.swift` (`:1717-1740` decodes txState by name),
  `ios/NereusApp/Main/TxPill.swift`, `ios/NereusApp/Main/PttButton.swift`,
  `ChromeColours.buttonOnAmber/Border/Text`; the name `held` is taken
  (`KeyedOverlay` `Content.held`, `TransmitModel.heldNote`), so code says `rfHeld`.
- Link surface: `tests/data/link/v1/surface.json` (TransmitState about `:8659`),
  regenerated by `tst_link_surface_manifest_regen`, checked by
  `tst_link_surface_manifest`; tables rendered with
  `python3 scripts/render-link-tables.py` (CI runs `--check`). Link spec:
  `docs/architecture/2026-09-23-station-link-v1.md` §18.7 (`:8666`) and §18.8
  (`:8838`).
- Tests: `tst_remote_tx_watchdog` (CORE_ONLY), `tst_remote_tx_cut_guard` (REALTIME),
  `tst_transmit_cut_line`, `tst_starvation_policy`, `tst_remote_mic_receiver`,
  `tst_tx_link_loss_each_path` (REALTIME), `tst_remote_transmit_client`,
  `tst_remote_keying`, `tst_transmit_state_facade`, `tst_station_control_log`,
  `tst_tgxl_core_tune` (split into parts), `tst_stop_transmit_now` (a TxChannel with a
  fake connection), `tst_mainwindow_status_bar_safety`, `tst_link_surface_manifest`,
  `tst_mirror_schema`.

## File Structure

Create:

| Path | Responsibility |
| --- | --- |
| `tests/tst_tx_rf_hold.cpp` | the TX channel's hold mask: zero blocks, block cadence, no gate or sequence change, release, clearing |

Modify:

| Path | Change |
| --- | --- |
| `src/core/TxChannel.{h,cpp}` | `RfHold` reasons, the hold mask, zero blocks in the sender section |
| `src/models/RadioModel.{h,cpp}` | `setRemoteRfHold`, `isRemoteRfHeld`, `remoteRfHoldChanged`; holds cleared when MOX returns to receive |
| `src/core/safety/RemoteTxWatchdog.{h,cpp}` | `kLinkHoldMs`, `kLinkCutMs`, the hold stage, new hooks and signals, the ordering asserts |
| `src/core/safety/RemoteTxCutGuard.{h,cpp}` | hold stage at `kHoldMs`, cut at `kCutMs`, `HoldFn` |
| `src/core/safety/TransmitCutLine.{h,cpp}` | `hold(bool)` (the `Guard` bit) |
| `src/core/safety/StarvationPolicy.{h,cpp}` | `Hold` in place of `Unkey`, `holdRf` hook, `onNoAudio` |
| `src/core/session/media/RemoteMicReceiver.{h,cpp}` | `kNoAudioCutMs`, `noAudio` signal, the second check; ordering assert |
| `src/core/session/media/DaemonMediaController.{h,cpp}` | forwards `noAudio` |
| `src/core/session/StationServer.{h,cpp}` | wires holds to the TX channel, the cut guard's hold, the mic hold and no-audio cut; publishes the hold |
| `src/core/session/TransmitStateFacade.{h,cpp}` | `rfHeld`, `rfHeldReason`, `rfHoldSerial`, `rfLastHoldMs`, `holdChanged` |
| `src/core/session/MirrorPolicy.cpp` | four Outbound rows |
| `src/core/session/ControlLog.{h,cpp}` | `transmitHoldStarted`, `transmitHoldEnded` |
| `tests/data/link/v1/surface.json`, `docs/architecture/2026-09-23-station-link-v1.md` | the new properties, §18.7 and §18.8 prose |
| `docs/architecture/2026-10-26-remote-transmit-stop-design.md` | status: approved, built by this plan |
| `src/gui/MainWindow.{h,cpp}` | the TX badge shows "Held" |
| `ios/NereusKit/Sources/NereusLink/TransmitStateReport.swift`, `ios/NereusApp/Main/TransmitModel.swift`, `ios/NereusApp/Main/TxPill.swift` | the phone's Held state |
| `tests/CMakeLists.txt` | `tst_tx_rf_hold` |
| tests listed per task | updated timings and new cases |

## Task 1: The TX channel's RF hold

**Requirements:** spec 5.2 (a hold closes RF, everything else stays keyed, reopens on
the next TX block); decisions 1 and 2.

**Files:**
- Modify: `src/core/TxChannel.h`, `src/core/TxChannel.cpp`
- Modify: `src/models/RadioModel.h`, `src/models/RadioModel.cpp`
- Create: `tests/tst_tx_rf_hold.cpp`
- Modify: `tests/CMakeLists.txt` (register `tst_tx_rf_hold` with the same helper and
  options as `tst_stop_transmit_now`)

**Interfaces:**
- Consumes: nothing.
- Produces, in `TxChannel`:
  ```cpp
  enum class RfHold : quint32 { Link = 1u << 0, Mic = 1u << 1, Guard = 1u << 2 };
  // Any thread. On: returns once no block being sent at the call is still inside
  // sendTxIq, so no non-zero sample reaches the connection after it returns
  // (the same short wait as closeRfGateAndWaitForSender). Off: never waits.
  void setRfHold(RfHold reason, bool on) noexcept;
  bool isRfHeld() const noexcept;           // any bit set
  quint32 rfHoldMask() const noexcept;
  void clearRfHolds() noexcept;             // every bit off
  ```
  In `RadioModel` (main thread):
  ```cpp
  void setRemoteRfHold(TxChannel::RfHold reason, bool on);
  bool isRemoteRfHeld() const;
  signals: void remoteRfHoldChanged(bool held);   // the mask went zero to non-zero or back
  ```

**Acceptance:**
- With the gate open and `setRfHold(Link, true)`, every block the fake connection
  receives has the usual sample count and all samples exactly `0.0f`; blocks keep
  arriving at the same cadence as unheld (count blocks over a fixed number of TX
  worker iterations, held and unheld: equal).
- During a hold `isRfGateOpen()` stays true, `m_runSequence` (observed through
  `isRfGateOpenForSequence` with the run's sequence) is unchanged, and `rfGateOpened`
  is not emitted.
- `setRfHold(Link, false)` with no other bit set: the next block carries the TX
  channel's real output (non-zero for a non-zero input).
- `Link` and `Mic` both on, `Link` off: still zero blocks; `Mic` off: real output.
- `closeRfGate()` during a hold: no block at all reaches the connection, as today.
- The AM monitor tap receives the same zero block the connection does while held.
- `RadioModel::setRemoteRfHold` emits `remoteRfHoldChanged(true)` once on the first
  bit and `(false)` once when the last clears; MOX going to receive clears every bit
  and emits `(false)` if any was set; the TX time-out timer's state is untouched by a
  hold (same remaining time with and without a 1 s hold).

**Verification:** consequential state transition on the transmit path: tests first
for the mask's contract. Unit layer with a fake connection, as `tst_stop_transmit_now`
does. Commands: build and run `tst_tx_rf_hold` and `tst_stop_transmit_now`, then
`tst_dsp_control_transmit` and `tst_tx_time_out_timer` for regressions.

**Execution note (advisory):** opus. Keying path: in early review A (after Task 3).
No prerequisites. Not parallel (hub file `RadioModel.cpp`).

- [ ] **Step 1:** Write `tst_tx_rf_hold` with the acceptance cases, using the fake
  connection pattern from `tst_stop_transmit_now`; see it fail to compile or fail.
- [ ] **Step 2:** Add the mask (`std::atomic<quint32>`), the API above, and the zero
  block in the sender section: after the existing `m_running` re-check, if the mask is
  non-zero, zero `m_outInterleavedFloat` for `outN` complex samples before `sendTxIq`
  and the AM tap. `setRfHold(on)` uses the same in-flight wait as
  `closeRfGateAndWaitForSender`. Nothing else in the block path changes.
- [ ] **Step 3:** Add the `RadioModel` wrappers and clear the holds where MOX returns
  to receive; document on `TxChannel` that the hold never touches `m_running`,
  `m_runSequence` or `rfGateOpened`.
- [ ] **Step 4:** Run the commands above; commit.

## Task 2: Two stages in the watchdog

**Requirements:** spec 5.1 (link hold 400 ms, link cut 3000 ms, link closed at once,
the new ordering chain), 5.2 (the cut is today's stop), 7 (watchdog unit cases);
decisions 3, 4, 5.

**Files:**
- Modify: `src/core/safety/RemoteTxWatchdog.h`, `src/core/safety/RemoteTxWatchdog.cpp`
- Modify: `src/core/session/media/RemoteMicReceiver.h` (its ordering assert only)
- Modify: `src/core/safety/RemoteTxCutGuard.h` (rename only: `kDeadlineMs` now
  `RemoteTxWatchdog::kLinkCutMs`, so it compiles; Task 3 adds its hold)
- Modify: every other user of `kLinkLossDeadlineMs` in `src/` and `tests/`, including
  `tests/tools/nereus_rendezvous_peer.cpp`, by name only, mapping each to the meaning
  it had (a stop bound becomes `kLinkCutMs`); timing expectations in other tests are
  Task 3's and Task 6's
- Test: `tests/tst_remote_tx_watchdog.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces, in `RemoteTxWatchdog`:
  ```cpp
  static constexpr int kLinkHoldMs = 400;    // RF off, still keyed
  static constexpr int kLinkCutMs = 3000;    // today's stop
  // kLinkLossDeadlineMs is removed.
  static_assert(RemoteMicConfig::kMaxDepthMs < RemoteMicConfig::kStarvationMs, ...);
  static_assert(RemoteMicConfig::kStarvationMs < kLinkHoldMs,
                "the mic line holds before the link does");
  static_assert(kLinkHoldMs < kFirstReconnectMs,
                "RF is off before a client's first reconnect (hold then cut design 5.1)");
  static_assert(kFirstReconnectMs < kLinkCutMs, "a short outage is held, not cut");
  static_assert(kKeepaliveIntervalMs * 3 < kLinkHoldMs,
                "three keepalives in a row may be lost without a hold");

  enum class HoldEnd { Heard, Cut, KeyEnded, LinkClosed };
  struct Hooks { ...existing...;
      // The device's quiet link is held (true) or no longer (false). Main thread.
      std::function<void(const QByteArray& deviceId, bool held)> hold; };
  bool isHeld(const QByteArray& deviceId) const;
  signals:
  void holdStarted(const QByteArray& deviceId, qint64 silentMs);
  void holdEnded(const QByteArray& deviceId, qint64 lengthMs, RemoteTxWatchdog::HoldEnd how);
  ```
  `tripQuiet` keeps its signature and is the cut.

**Acceptance (fake clock and timer, as the existing tests):**
- A keyed watch with no keepalive: no hold at 399 ms, `hold(id, true)` and
  `holdStarted(id, ≥400)` at 401 ms; no stop at 2999 ms; at 3001 ms `stop` with
  today's `stopMessage`, `tripped(id, false, ≥3000)`, `hold(id, false)` and
  `holdEnded(id, ~2600, Cut)`, in that order (stop before the hold is released, so no
  real sample leaves between them).
- A counted keepalive at 1500 ms during the hold: `hold(id, false)`,
  `holdEnded(id, ~1100, Heard)`, the device still watched, and a new hold only at
  1500 + 401 ms.
- During a hold, a keepalive with an old sequence, a repeated sequence or an epoch
  older than the key's ends nothing and is not counted.
- `released(id)` (or `setKeyed(id, false)`) during a hold: `hold(id, false)`,
  `holdEnded(.., KeyEnded)`, no stop, the watch gone.
- `linkClosed(id)` during a hold: stop at once, `holdEnded(.., LinkClosed)`.
- A VOX-armed, unkeyed watch: no hold at 401 ms; at 3001 ms the stop (VOX disarm)
  as today.
- `tripQuiet(id)` (the cut guard) during a hold: the cut, `holdEnded(.., Cut)`.
- The late turn: a check that runs more than `kLateCheckSlackMs` late gives one turn
  before the hold and, separately, one before the cut.
- `QCOMPARE(RemoteTxWatchdog::kLinkHoldMs, 400)`, `kLinkCutMs == 3000`; the ordering
  test reads the new chain 120 < 250 < 400 < 1000 < 3000.

**Verification:** consequential state transition: tests first. Unit layer. Commands:
build and run `tst_remote_tx_watchdog`; build every target that failed to compile
from the rename (`cmake --build build --target all_tests` once, in the background) and
record which tests now fail on timing for Task 3 and Task 6, without changing their
expectations.

**Execution note (advisory):** opus. Keying path: in early review A. Prerequisite
none (independent of Task 1). Not parallel (rename touches many files).

- [ ] **Step 1:** Rewrite the timing cases of `tst_remote_tx_watchdog` to the
  acceptance list; see them fail.
- [ ] **Step 2:** `Watch` gains `held`, `heldSinceMs` and a second late-turn flag;
  `onTimer` and `reschedule` work on two deadlines per watch (hold at `lastMs +
  kLinkHoldMs + 1` for keyed watches not yet held, cut at `lastMs + kLinkCutMs + 1`),
  dropping the `kLinkLossDeadlineMs+1` cap in favor of the earliest pending stage;
  `keepalive()` trips only past `kLinkCutMs` and ends a hold only after its sequence
  and epoch checks pass; `trip`, `update`'s erase and `linkClosed` end a hold with
  the matching `HoldEnd`.
- [ ] **Step 3:** The constants, asserts and renames; `RemoteMicReceiver.h:234`'s
  assert compares against `kLinkHoldMs`.
- [ ] **Step 4:** Run the commands; write the list of timing failures into the task
  report; commit.

## Task 3: The cut guard holds first

**Requirements:** spec 5.1 (cut guard holds at 400 ms, cuts at 3000 ms), 5.4 (a
stalled main thread), 7 (cut guard cases); decisions 2 and 3.

**Files:**
- Modify: `src/core/safety/RemoteTxCutGuard.h`, `src/core/safety/RemoteTxCutGuard.cpp`
- Modify: `src/core/safety/TransmitCutLine.h`, `src/core/safety/TransmitCutLine.cpp`
- Modify: `src/core/session/StationServer.cpp` (the guard's construction at about
  `:3078-3096` only: pass the hold function, which calls the cut line's `hold`)
- Test: `tests/tst_remote_tx_cut_guard.cpp`, `tests/tst_transmit_cut_line.cpp`

**Interfaces:**
- Consumes: `TxChannel::setRfHold(RfHold::Guard, bool)` (Task 1);
  `RemoteTxWatchdog::kLinkHoldMs`, `kLinkCutMs` (Task 2).
- Produces:
  ```cpp
  // RemoteTxCutGuard
  // Called on the guard's thread with its mutex held, like CutFn; must not block.
  using HoldFn = std::function<void(bool held)>;
  static constexpr qint64 kHoldMs = RemoteTxWatchdog::kLinkHoldMs;
  static constexpr qint64 kCutMs = RemoteTxWatchdog::kLinkCutMs;   // replaces kDeadlineMs
  RemoteTxCutGuard(CutFn cut, HoldFn hold);
  bool isHolding() const;   // tests and logs
  // TransmitCutLine, any thread, non-blocking apart from setRfHold's short spin:
  void hold(bool held);     // the TX channel's Guard bit only; never cutTransmit
  ```

**Acceptance:**
- Armed and judging, no receipt: `hold(true)` once at `kHoldMs + 1` (±the existing
  test tolerance), the cut once at `kCutMs + 1`, then `hold(false)`; nothing more for
  that arm.
- A `heardOffMain` (or `heardOnMain`) receipt during the hold: `hold(false)` on the
  guard thread, judging continues, a later silence holds again; the cut does not run.
- `disarm()` during a hold: `hold(false)` before `disarm()` returns; no cut.
- A new `arm()` during a hold of the previous arm: the old hold is released first.
- With the main thread held for 3500 ms (the existing `cutsOnTimeWithMainHeld`
  shape): RF held from about 400 ms and cut at about 3000 ms, judged off the main
  thread; `noFalseCutWithStreamAliveWhileMainHeld` still holds nothing and cuts
  nothing.
- `TransmitCutLine::hold(true)` sets only the `Guard` bit; `cut()` still closes the
  gate and latches `cutTransmit`; `hold` with no targets does nothing.
- `tst_transmit_cut_line`'s bound moves to `kLinkCutMs + 100`, and it gains a case
  that the hold bound is `kLinkHoldMs + 100`.

**Verification:** consequential, threaded: tests first. `tst_remote_tx_cut_guard` is
REALTIME: run it alone. Commands: build and run `tst_remote_tx_cut_guard` (alone),
then `tst_transmit_cut_line`.

**Execution note (advisory):** opus. Keying path and a new callback on a realtime
thread: early review A runs after this task, over Tasks 1 to 3. Prerequisites: Tasks 1
and 2.

- [ ] **Step 1:** Update the guard tests to the acceptance list; see them fail.
- [ ] **Step 2:** Two-stage loop in `run()`: wait to `lastHeard + kHoldMs + 1`, call
  `hold(true)`, wait to `lastHeard + kCutMs + 1` or a receipt; on a receipt within the
  hold, `hold(false)` and keep judging; on the cut, call `m_cut` then `hold(false)`.
  `disarm` and a re-`arm` release a hold under the mutex.
- [ ] **Step 3:** `TransmitCutLine::hold`; pass it from `StationServer`'s guard
  construction.
- [ ] **Step 4:** Run the commands; commit.

## Task 4: The microphone holds, then cuts at 10 s

**Requirements:** spec 5.1 (mic hold 250 ms in AM, SAM, FM, DRM, RADE; 10 s
no-audio cut in every mode while keyed on a mic line; TUNE and two-tone exempt), 5.3
(the no-audio stop sentence), 7 (starvation cases).

**Files:**
- Modify: `src/core/safety/StarvationPolicy.h`, `src/core/safety/StarvationPolicy.cpp`
- Modify: `src/core/session/media/RemoteMicReceiver.h`,
  `src/core/session/media/RemoteMicReceiver.cpp`
- Modify: `src/core/session/media/DaemonMediaController.h`,
  `src/core/session/media/DaemonMediaController.cpp` (forward `noAudio` beside
  `starved`, `:3310-3315`)
- Test: `tests/tst_starvation_policy.cpp`, `tests/tst_remote_mic_receiver.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces:
  ```cpp
  // StarvationPolicy
  enum class StarvationAction { KeepKeyed, Hold };   // Hold replaces Unkey: same modes
  struct Hooks { ...existing...;
      std::function<void(bool held)> holdRf; };       // the Mic hold
  bool onStarved(const QByteArray& deviceId, bool starved);   // never stops now; returns false
  bool onNoAudio(const QByteArray& deviceId);   // the 10 s cut; true when it stopped
  bool isHolding() const;
  // RemoteMicReceiver
  // RemoteMicConfig: static constexpr int kNoAudioCutMs = 10000;
  signals: void noAudio();   // once per quiet period, kNoAudioCutMs after the last packet, while watching
  // DaemonMediaController: a signal carrying the device id beside the existing
  // starved forward, named micNoAudio(const QByteArray& deviceId)
  ```

**Acceptance:**
- `actionFor`: `Hold` for AM, SAM, FM, DRM, RADE and RADE2 modes and an unknown mode;
  `KeepKeyed` for LSB, USB, DSB, CWL, CWU, DIGL, DIGU, SPEC (the same split as today's
  `Unkey`/`KeepKeyed`).
- `onStarved(id, true)` in AM: `holdRf(true)`, no `stopAllTx`; `onStarved(id, false)`:
  `holdRf(false)`. In USB: neither. With `microphoneUnused` true (TUNE, two-tone):
  neither, in any mode.
- `onNoAudio(id)` in USB and in AM: `stopAllTx(stopMessage(name))`, returns true, and
  releases a mic hold if one is on; with `microphoneUnused` true: nothing, returns
  false.
- Receiver: watching, no packets: `starved(true)` at 250 ms, `noAudio()` once at
  10000 ms (not at 9999); a packet at 5000 ms resets both (no `noAudio` until 15000
  ms of the new silence); `setWatching(false)` and `stop()` cancel a pending
  `noAudio`.
- `QCOMPARE(RemoteMicConfig::kNoAudioCutMs, 10000)`; `kStarvationMs` stays 250.

**Verification:** consequential: tests first. Unit layer with the receiver's existing
fake clock. Commands: build and run `tst_starvation_policy` and
`tst_remote_mic_receiver`.

**Execution note (advisory):** opus. Keying path: in early review B (after Task 6).
Prerequisite: none (can follow Task 3 directly). Not parallel.

- [ ] **Step 1:** Update the two test files to the acceptance list; see them fail.
- [ ] **Step 2:** The receiver's second check, scheduled from the same last-packet
  time and starvation generation as `checkStarvation`.
- [ ] **Step 3:** The policy's `Hold` and `onNoAudio`; the media controller forward.
- [ ] **Step 4:** Run the commands; commit.

## Task 5: The Core wires and publishes the holds

**Requirements:** spec 5.2, 5.3 (the Core publishes the hold in txState and logs it),
6 (PR 652's keepalives on every path stand); decisions 4, 6, 8.

**Files:**
- Modify: `src/core/session/StationServer.h`, `src/core/session/StationServer.cpp`
- Modify: `src/core/session/TransmitStateFacade.h`,
  `src/core/session/TransmitStateFacade.cpp`
- Modify: `src/core/session/MirrorPolicy.cpp`
- Modify: `src/core/session/ControlLog.h`, `src/core/session/ControlLog.cpp`
- Modify: `tests/data/link/v1/surface.json` (regenerated),
  `docs/architecture/2026-09-23-station-link-v1.md` (§18.7, §18.8 and the rendered
  table), `docs/architecture/2026-10-26-remote-transmit-stop-design.md` (its Status
  line: "Approved by JJ on 2026-10-26; built by
  2026-10-26-remote-transmit-hold-then-cut-plan.md.")
- Test: `tests/tst_transmit_state_facade.cpp`, `tests/tst_station_control_log.cpp`,
  `tests/tst_link_surface_manifest.cpp` (data only), `tests/tst_mirror_schema.cpp` if it
  lists TransmitState properties

**Interfaces:**
- Consumes: `RadioModel::setRemoteRfHold`, `remoteRfHoldChanged` (Task 1); the
  watchdog's `hold` hook, `holdStarted`, `holdEnded`, `HoldEnd` (Task 2); the guard's
  `HoldFn` (Task 3, already wired); `StarvationPolicy::Hooks::holdRf`, `onNoAudio`,
  `DaemonMediaController::micNoAudio` (Task 4).
- Produces:
  ```cpp
  // TransmitState, appended after cfcPeakDb in this order:
  Q_PROPERTY(bool rfHeld READ rfHeld NOTIFY holdChanged)
  Q_PROPERTY(QString rfHeldReason READ rfHeldReason NOTIFY holdChanged)   // "link", "mic", ""
  Q_PROPERTY(quint64 rfHoldSerial READ rfHoldSerial NOTIFY holdChanged)
  Q_PROPERTY(qint64 rfLastHoldMs READ rfLastHoldMs NOTIFY holdChanged)
  void setRfHold(bool held, const QString& reason);   // Core side; ending a hold advances
                                                      // rfHoldSerial and sets rfLastHoldMs
  signals: void holdChanged();
  // ControlLog
  void transmitHoldStarted(const QByteArray& deviceId, const QString& what, qint64 silentMs);
  void transmitHoldEnded(const QByteArray& deviceId, qint64 lengthMs, const QString& how);
  ```

**Acceptance:**
- The watchdog's `hold` hook sets `RfHold::Link` through `RadioModel` only while that
  device is `keyedBy`; `hold(id, false)` always clears it. The starvation `holdRf`
  hook sets and clears `RfHold::Mic` the same way. `micNoAudio` reaches
  `StarvationPolicy::onNoAudio` for the keyer, with the same `remoteRadioMicKeyActive`
  skip `remoteMicStarved` has.
- `TransmitState`: a link hold for the keyer sets `rfHeld` true with reason `"link"`
  and emits `holdChanged` once; its end sets `rfHeld` false, advances `rfHoldSerial`
  by 1 and sets `rfLastHoldMs` to the hold's length; a mic hold reads `"mic"`; both at
  once read `"link"` and `rfHeld` falls only when both end; MOX to receive during a
  hold ends it (`HoldEnd` "the key ended"); a hold for a device that is not the keyer
  publishes nothing.
- The cut at 3 s sets the stop exactly as today's 400 ms stop did (`kStopLinkLost`,
  the same `stopText`, `stopSerial` advanced); the 10 s no-audio cut records
  `kStopMicStarved` with `StarvationPolicy::stopMessage`.
- `ControlLog` writes the two line shapes in decision 8, with the device id in hex as
  the existing lines do; `watchdogStopped` is unchanged.
- `surface.json` gains the four properties after `cfcPeakDb`; `tst_link_surface_manifest`
  passes; `python3 scripts/render-link-tables.py --check` passes after rendering;
  `txStateVersion()` is still 2.
- The link spec's §18.7 states hold at 400 ms, cut at 3000 ms, mic hold at 250 ms, the
  10 s no-audio cut, and §18.8 lists the four properties with one line each.

**Verification:** consequential state transition and a wire surface: tests first for
the facade and the log; the surface manifest is the wire check. Commands: build and
run `tst_transmit_state_facade`, `tst_station_control_log`,
`tst_link_surface_manifest`, `tst_mirror_schema`; regenerate the surface with
`tst_link_surface_manifest_regen` (read its comment for how), then
`python3 scripts/render-link-tables.py` and `--check`.

**Execution note (advisory):** opus. Keying path and the station wire: in early review
B. Prerequisites: Tasks 1 to 4. Not parallel (hub file `StationServer.cpp`).

- [ ] **Step 1:** Facade and log tests for the acceptance cases; see them fail.
- [ ] **Step 2:** The four properties, `setRfHold`, the mirror rows; the log lines.
- [ ] **Step 3:** The wiring in `StationServer` (hooks, `holdStarted`/`holdEnded` to the
  log and the facade, `remoteRfHoldChanged` ending a published hold on MOX to receive,
  `micNoAudio`).
- [ ] **Step 4:** Regenerate the surface, render the tables, update the two docs; run
  the commands; commit.

## Task 6: End-to-end timing and the hold in real runs

**Requirements:** spec 6 (each path severed alone keeps transmitting), 7 (integration
cases); the ordering "RF is off before a client's first reconnect" and "a reconnect
finds transmit stopped".

**Files:**
- Test: `tests/tst_tx_link_loss_each_path.cpp`, `tests/tst_remote_transmit_client.cpp`,
  `tests/tst_remote_keying.cpp`, `tests/tst_transmit_state_facade.cpp` (the
  watchdog-stop case's loop length), `tests/tst_tgxl_core_tune.cpp` (new cases in the
  part that holds the TUNE cycle cases), and any other test Task 2's report listed as
  failing on timing
- Modify: none in `src/` unless a test exposes a defect, which is fixed in the file
  that owns it and named in the report

**Interfaces:**
- Consumes: everything from Tasks 1 to 5.
- Produces: nothing.

**Acceptance:**
- `tst_tx_link_loss_each_path`:
  - `dataChannelSeveredStopsWithin500ms` is renamed
    `dataChannelSeveredAloneKeepsTransmitting` (it already keeps transmitting after
    PR 652) and asserts no hold and no stop over 4 s.
  - `webSocketSeveredStopsWithin500ms` becomes `everyPathSeveredHoldsThenCuts`: with
    every path severed, the TX channel is held (`isRfHeld`) between 400 and 500 ms
    while MOX stays on, and the stop lands between 3000 and 3100 ms with today's
    reason; the turn and relay variants follow the same rename and keep their QSKIP.
  - A new case: every path severed for 1500 ms, then restored: held from about 400 ms,
    unheld within 200 ms of the first counted keepalive, never stopped, `rfHoldSerial`
    advanced by 1 and `rfLastHoldMs` about 1100.
  - `webSocketClosingStopsAtOnce` is unchanged (cut at once).
  - A new case: the device signs in again while held; the old session ends and
    transmit is stopped at once (link closed), not at 3 s.
  - `aStalledCoreStillCutsTransmitOffMain` holds the main thread 3500 ms and expects
    RF held from about 400 ms and the cut from the guard at about 3000 ms;
    `aLiveChannelKeepsTheKeyWhenMainIsHeld` still expects no hold and no cut.
  - `voxArmedGoesOffWhenTheLinkGoesQuiet`: VOX off between 3000 and 3100 ms.
  - `starvationOnALiveLinkStopsAmAndNotUsb` becomes
    `starvationOnALiveLinkHoldsAmThenCutsEveryMode`: AM held within 300 ms of the last
    mic packet and stopped at 10 s; USB not held and stopped at 10 s.
- `tst_remote_transmit_client`: each case that waited 600 ms for a stop runs past
  3000 ms and expects one stop; the release fence still blocks every path, and every
  open path still gets each sequence (PR 652's cases).
- `tst_remote_keying::keepalivesHoldAKeyAndTheWatchdogStopsItWithoutThem`: held by
  500 ms, stopped by 3100 ms.
- `tst_transmit_state_facade::aQuietLinkIsTheWatchdogsStopWithItsReason` waits past
  3000 ms.
- `tst_tgxl_core_tune`: a 1 s link hold during a remote device's Tuner Genius tune
  leaves the cycle running and MOX on, with the carrier held silent and back after;
  a 3 s silence ends the cycle with its usual reason; a hold changes neither the
  PGXL nor the TGXL state the tests already observe.
- A hold during a key does not restart or pause the TX time-out (one case, in
  whichever of these files already drives the time-out; Task 1 checked the timer
  itself).

**Verification:** integration layer, realtime tests run alone. Commands: build the
listed targets; run the non-realtime ones together, then `tst_tx_link_loss_each_path`
and `tst_remote_tx_cut_guard` alone; then build `all_tests` and run
`ctest --test-dir build -LE realtime --output-on-failure` followed by
`ctest --test-dir build -L realtime --output-on-failure`, in the background, and put
both summary lines in the report.

**Execution note (advisory):** opus. Keying path: early review B runs after this task,
over Tasks 4 to 6. Prerequisites: Tasks 1 to 5.

- [ ] **Step 1:** Update and add the cases; run them.
- [ ] **Step 2:** Fix any defect a case exposes in the file that owns it.
- [ ] **Step 3:** The full suite as above; commit.

## Task 7: "Held" on the Core window and the desktop remote window

**Requirements:** spec 5.3 (TX indicator shows "Held" in amber); decision 7.

**Files:**
- Modify: `src/gui/MainWindow.h`, `src/gui/MainWindow.cpp`
- Test: `tests/tst_mainwindow_status_bar_safety.cpp`

**Interfaces:**
- Consumes: `TransmitState::rfHeld`, `rfHoldSerial`, `rfLastHoldMs`, `holdChanged`
  (Task 5). On the Core window the `TransmitState` is
  `m_desktopStationController->server()->transmitState()`; on the remote window it is
  the client's mirrored one.
- Produces: nothing.

**Acceptance:**
- Keyed and `rfHeld` true: the TX badge reads "Held" with `StatusBadge::Variant::Warn`;
  `rfHeld` false while keyed: back to "TX" with `Variant::Tx`; unkeyed: "TX" with
  `Variant::Off`, as today.
- A remote window that sees `rfHoldSerial` advance without having seen `rfHeld` true
  for it, while keyed: "Held" for `min(rfLastHoldMs, 3000)` ms, then "TX"; if MOX
  drops first, `Variant::Off` at once.
- TX Inhibit's icon still paints over the badge in every state.
- The transmit button and MOX display are unchanged during a hold (they follow MOX).

**Verification:** UI per `ui-verification`: the badge's state machine in
`tst_mainwindow_status_bar_safety` (functional), plus one offscreen capture of the
status bar in the Held state in both themes, saved under the crew workspace's
`evidence/` folder for the controller to show JJ. Commands: build and run
`tst_mainwindow_status_bar_safety` and `tst_status_badge`.

**Execution note (advisory):** opus. Not on the keying path. Prerequisite: Task 5.
Could run in a worktree beside Task 8 (no shared files), but runs one at a time unless
JJ asks for speed.

- [ ] **Step 1:** The badge cases; see them fail.
- [ ] **Step 2:** Connect the badge to the right `TransmitState` in each window kind.
- [ ] **Step 3:** Run the tests and take the captures; commit.

## Task 8: "Held" on the phone

**Requirements:** spec 5.3 (the phone shows "Held" on its TX indicator, the transmit
button stays lit); decision 7.

**Files:**
- Modify: `ios/NereusKit/Sources/NereusLink/TransmitStateReport.swift`
- Modify: `ios/NereusApp/Main/TransmitModel.swift` (decode the four properties by
  name, defaulting to unheld)
- Modify: `ios/NereusApp/Main/TxPill.swift`
- Test: the NereusKit test that covers `TransmitStateReport` decoding (or a new case in
  `ios/NereusKit/Tests/NereusLinkTests/`), `ios/NereusApp/Tests/TransmitScreenTests.swift`,
  `ios/NereusApp/Tests/TransmitDisplayShotTests.swift`

**Interfaces:**
- Consumes: the four txState property names from Task 5 (`rfHeld`, `rfHeldReason`,
  `rfHoldSerial`, `rfLastHoldMs`).
- Produces: `TransmitStateReport.rfHeld: Bool`, `rfHeldReason: String`,
  `rfHoldSerial: UInt64`, `rfLastHoldMs: Int64`.

**Acceptance:**
- A txState without the new keys decodes as unheld (an older Core).
- Keyed and `rfHeld`: the TX pill reads "Held" in the amber `ChromeColours`, the clock
  keeps counting from key-down, Stop still works; the PTT button stays in its keyed
  look.
- Seeing `rfHoldSerial` advance without having seen `rfHeld` for it while keyed:
  "Held" for `min(rfLastHoldMs, 3000)` ms, then "TX".
- A shot test of the pill in the Held state in light and dark.

**Verification:** UI per `ui-verification`: Swift unit tests for decoding and the
display rule, a shot test for the look. Commands:
`ios/scripts/swift-test.sh --jobs 2 --filter TransmitStateReport` (or the suite the
case lands in), then the app tests `-only-testing:` the two app test classes, under
`/tmp/nereus-ios-full.lock`. Installing on JJ's phone is not part of this task.

**Execution note (advisory):** opus. Not on the keying path. Prerequisite: Task 5.

- [ ] **Step 1:** Decoding and display-rule tests; see them fail.
- [ ] **Step 2:** The report fields, the decode, the pill.
- [ ] **Step 3:** The shot test; run the commands; commit.

## Early independent reviews

JJ, through the manager on 2026-10-26, asked for the keying tasks to get an early
independent review. The controller dispatches `crew-reviewer` (opus) twice, before
the next task starts:

- **Review A** after Task 3, over Tasks 1 to 3 (the TX channel's hold, the watchdog's
  stages, the cut guard): threading, every path that can leave RF on, every path that
  can leave a hold stuck, and the acceptance cases.
- **Review B** after Task 6, over Tasks 4 to 6 (the mic stages, the wiring and
  publication, the end-to-end timing).

Findings from each go to one fix wave before the next task. The whole-branch review
at the end still runs.

## Final checks

On the finished branch, once: `all_tests` built, then
`ctest --test-dir build -LE realtime --output-on-failure` and
`ctest --test-dir build -L realtime --output-on-failure`, both summary lines in the
ledger; `python3 scripts/render-link-tables.py --check`; the Swift package tests and
the two app test classes from Task 8.

## Hardware evidence (JJ, pending until he runs it)

Not crew tasks. The controller hands these to JJ through the manager, and none is
claimed until he has seen it:

- Mac remote window over the web relay: VOX, a voice over, TUNE and two-tone; the
  Core log shows holds with lengths and no cuts.
- During a TUNE, pull the client's network: RF gone within half a second, transmit off
  at 3 s with the reason shown, a new press works after reconnect.
- PureSignal two-tone across a short hold: PS stays calibrated (zero blocks during a
  hold are new to the PS feedback path).
- The 2026-10-23 setup (PGXL in operate, the STRIX cable): holds and their lengths for
  an evening, with the ping loop from the RF note running.
