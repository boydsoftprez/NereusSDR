# Native check fixes implementation plan

> **Execution:** run with `crew` under `yonder-cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort
> follow the risk-based policy. No review between tasks; one whole-branch
> review at the end (shared with the remote audio status plan).

**Goal:** fix what the September 22 native checks on the running remote GUI
found: a saved receiver that was refused after a Core restart, a restore
message full of internal words, an S-meter that shows a placeholder number
when there is no reading, and developer test menu items that are live in a
remote session.

**Architecture:** the Core's startup restore adopts the placement rule live
operation and radio recovery already use (a slice on a pan may have its own
receiver); restore messages become plain English; the meter and menu fixes
are GUI-only.

**Tech stack:** C++20, Qt6, `RadioModel` receive-layout restore,
`SliceStreamAllocator`, `DaemonApp`, `SMeterWidget`, `MeterPoller`,
`MainWindow` remote-role gating, Qt Test.

**Spec:** R3 plan requirements R-R3-34 (restore receive slices, explicit
actionable failures), R-R3-13 (disconnect clears the live reading), R-R3-21
and R-R3-25 (visible remote controls act on their owner or are disabled with
a reason; receive-only sessions cannot initiate TX-coupled accessory
commands). Evidence: `/Users/j.j.boyd/.config/nereus/work/r3-native-checks-2026-09-22.md`
(observations) and the restore diagnosis summarized under "What already
exists".

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`.
  Never edit or build in `/Users/j.j.boyd/NereusSDR` or any other checkout.
- Never stage or commit `docs/architecture/2026-09-22-optional-microphone-capture-design.md`
  or `docs/architecture/2026-09-22-optional-microphone-capture-plan.md` (the
  controller commits them). Stage explicit paths only.
- Commits: GPG-signed (`git commit -S`), hooks run with
  `NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`; never `--no-gpg-sign`, never
  `--no-verify`. No `Co-Authored-By` trailer. No em-dash characters in
  commit messages or docs.
- Keep source-first attribution intact in every touched file (the S-meter
  and Max Bin code carry Thetis cites and the -400 sentinel comes from
  Thetis `if (max_bin > -400f)`); do not remove or reword existing cites or
  author tags. This plan ports no new Thetis logic.
- `src/core` and `src/models` stay GUI-free. Use `AppSettings`, never
  `QSettings`. C++ style per CLAUDE.md (braces, no raw new/delete,
  `m_camelCase`, `kPascalCase`).
- Operator strings are plain English: no "stream", "owner", "admitted",
  "pan-N", "epoch" or other internal words, no doubled punctuation, and no
  source cites inside user-visible strings.
- Tests: build the exact targets first, then run
  `ctest --test-dir build-integration -R '^(<targets>)$' --no-tests=error --output-on-failure`.
  Record `uptime` load averages with timings. Do not run the unfiltered
  suite. Never weaken an existing assertion except where a task states the
  intended behavior change.
- Hardware (Rock, Saturn, the running GUI) is off limits to implementers.

## What already exists

Restore diagnosis (read-only, 2026-09-22):
- Live pane changes: `MainWindow::applyPanLayout("1")`
  (`src/gui/MainWindow.cpp:10683-10737`) calls
  `RadioModel::rehomeSlicesToPans` (`src/models/RadioModel.cpp:17070-17089`),
  which writes `panKey` on remote slices; `panKey` is two-way
  (`src/core/session/MirrorPolicy.cpp:113`) and the Core applies it
  (`StationClient.cpp:2025-2072` -> Core `propertyWrite`). On the Core a
  pan-key change only schedules a save (`RadioModel.cpp:6042-6046`); the
  slice keeps its own receiver.
- Save: `captureReceiveLayout` (`RadioModel.cpp:13250-13273`) stores
  `panKey`, frequency and mode per slice plus `radeRxOwnerId`; it does not
  record receiver sharing.
- Radio recovery: `bindUnboundSlices` recovery branch
  (`RadioModel.cpp:4262-4294`) gives a slice outside its pan's receiver range
  a fresh receiver.
- Startup restore: `bindReceiveLayoutSlices` (`RadioModel.cpp:4303-4366`)
  forces a later slice on an already-restored pan to join that pan's first
  receiver (`:4322-4325`) through `SliceStreamAllocator::joinStream`
  (`src/core/SliceStreamAllocator.cpp:59-69`, strict half-sample-rate window
  `:37-49`); a miss removes the slice (`:4359-4361`) and builds the message
  (`:4364`). The allocator reason already ends in a period
  (`SliceStreamAllocator.cpp:66-67`), which with the template produces "..".
  `completeReceiveLayoutStartup` (`:13217-13229`) then calls
  `activateRestoredRadeReceiveOwner` (`:13171-13183`), which reports the
  RADE owner line when the owner slice is gone. After a failed restore the
  layout is protected and save paths skip it (`:13377`, `:13404`, `:13449`).
- Real record that failed (Rock, saved before the 14:39 restart):
  `{"radeRxOwnerId":1,"slices":[{"id":0,"panKey":"pan-0","frequencyHz":14290000,"dspMode":1},{"id":1,"panKey":"pan-0","frequencyHz":7227600,"dspMode":12}],"version":1}`.
  Startup log: `Placement: slice 0 ... NewStream stream=0`, `slice 1 freq=7.2276
  MHz ... Rejected stream=-1`, `streams= 5`.
- Tests: `tests/tst_receive_layout_runtime.cpp` restores hand-written
  records, including two pans on two bands (`:125`, `:194`);
  `:379 laterPanMemberCannotBorrowAnotherPansWindow` currently expects a
  `pan-0` slice at 7.201 MHz to be refused. `tests/tst_daemon_radio_recovery.cpp:161-224`
  covers slices on different pans only.

GUI facts:
- S-meter: `MeterPoller` feeds `-400.0` when there is no remote reading
  (`src/gui/meters/MeterPoller.cpp:391`, `:430`; Thetis sentinel handling at
  `:487-495`); `SMeterWidget` prints `"%1 dBm"` from its display value
  (`src/gui/SMeterWidget.cpp:851`, `:1001`). While disconnected the operator
  saw "-400 dBm" and "S1".
- Tools menu test entries: `MainWindow.cpp:7542-7575` add "Test antenna
  switch toast" (calls `RadioModel::emitAntennaAutoSwitched`) and "Test
  TX-bound re-route dialog" (calls `RadioModel::requestTxBoundReRoute`),
  intentionally visible until the conflict-detection state machine ships.
  Remote role gating lives in `MainWindow::applyRemoteRoleGating()`
  (`MainWindow.cpp:10096-10174`); refusals use `transmitControlsPermitted()`
  and the existing remote reason text.

---

## Task 1: Startup restore uses the live placement rule, with plain messages

**Requirements:** R-R3-34.

**Files:**
- Modify: `src/models/RadioModel.cpp` (`bindReceiveLayoutSlices`, restore message building, `activateRestoredRadeReceiveOwner` wording), `src/core/SliceStreamAllocator.cpp` (reason text without trailing punctuation or with the template not adding one)
- Test: `tests/tst_receive_layout_runtime.cpp` (update `:379`, add cases), a daemon-path regression in `tests/tst_receive_layout_runtime.cpp` or `tests/tst_daemon_radio_recovery.cpp` (whichever already constructs `DaemonApp`)

**Interfaces:**
- Consumes: nothing new.
- Produces: no new public API. Behavior: a saved slice whose frequency is
  outside every receiver already restored for its pan gets its own new
  receiver and keeps its saved pan id; it is refused only when no receiver
  can be created (board limit reached) or its saved values are invalid.

**Acceptance:**
- The exact record above restores both slices: slice 1 at 7,227,600 Hz in
  RADE_U on a different receiver from slice 0, panKey `pan-0` kept, RADE
  receive owner 1 active, restore state accepted with no operator message.
- Placement order and joining are unchanged for slices that fit: a later
  slice inside an existing receiver's window on its pan still joins that
  receiver (existing `:125`, `:194` cases unchanged). A slice never joins a
  receiver that belongs to a different pan.
- `laterPanMemberCannotBorrowAnotherPansWindow` (`:379`) now expects the
  7.201 MHz `pan-0` slice restored on its own new receiver, and still
  asserts it did not join the other pan's receiver. A new variant on a
  board whose receiver limit is already used expects refusal.
- Daemon-path regression built through live use, not a hand-written record:
  start `DaemonApp` for an ANAN-G2 capability board; add a slice on a second
  pan; tune it to 7,227,600 Hz RADE_U; apply `panKey = "pan-0"` through the
  same mirrored-write path a GUI uses; assert it keeps its own receiver
  live; stop; reload settings from disk; start again; assert the accepted
  restore described in the first bullet. This test fails on the current
  tree.
- Messages (exact wording; letter = the slice letter the GUI shows, `A`
  for slice id 0 upward; panadapter number = pan index + 1):
  refusal for no free receiver: "Receiver <L> (<MHz with 4 decimals> MHz
  <mode name>) could not be restored because all of the radio's receivers
  are in use. Add it again with +RX after closing another receiver.";
  invalid saved values keep their current meaning in plain words; when the
  refused slice carried RADE audio, append once: "RADE audio from receiver
  <L> stays off until that receiver is back."; the message ends once with
  "Your saved layout is kept." No ".." anywhere, no "stream", "owner",
  "admitted" or "pan-N". A test asserts the exact text for the no-free-
  receiver refusal with and without the RADE sentence.

**Verification:** consequential state restore: the daemon-path regression
fails first on the current tree, then passes; unit cases for the rule and
the messages.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_receive_layout_runtime tst_receive_layout_store tst_receive_layout_hydration tst_daemon_radio_recovery tst_slice_rehome_on_layout_shrink -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_receive_layout_runtime|tst_receive_layout_store|tst_receive_layout_hydration|tst_daemon_radio_recovery|tst_slice_rehome_on_layout_shrink)$' --no-tests=error --output-on-failure
```
If any of those target names does not exist, use the actual names from
`tests/CMakeLists.txt` that cover receive-layout store, runtime, hydration
and daemon recovery, and list them in the report. Hardware (pending,
controller/operator): after install, a Core restart restores the operator's
two-band layout.

**Execution note (advisory):** opus (Core restore state and placement).

- [ ] **Step 1:** Write the daemon-path regression and the updated `:379`
  expectation; confirm the regression fails on the current tree.
- [ ] **Step 2:** Apply the live placement rule in
  `bindReceiveLayoutSlices` and rewrite the messages; run the commands;
  commit.

## Task 2: The S-meter shows no reading instead of a placeholder number

**Requirements:** R-R3-13.

**Files:**
- Modify: `src/gui/SMeterWidget.cpp` (and `.h` if a helper is needed)
- Test: the existing S-meter widget test target (find it in `tests/CMakeLists.txt`, e.g. a `tst_smeter*` target) or `tests/tst_remote_meter_poller.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: no API change.

**Acceptance:**
- When the value the widget is given is the no-reading sentinel (any value
  at or below -400 dBm) or not finite, the dBm readout shows "-- dBm", the
  S-unit readout shows "--", and the needle rests at the scale minimum
  without peak-hold markers; the next real value restores normal display.
  This applies in both remote and local modes (local already uses the same
  sentinel from Max Bin).
- Values above -400 dBm display exactly as today (unchanged formatting and
  needle math). Existing S-meter tests pass unchanged.
- Test: feed -400.0, NaN and -130.0 and assert the readout text and that
  -130.0 is unchanged from today's formatting.

**Verification:** GUI presentation: widget-level test of the text and
needle state; native look is pending the next operator checkpoint.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_meter_poller -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^tst_remote_meter_poller$' --no-tests=error --output-on-failure
```
plus the S-meter widget test target you identified.

**Execution note (advisory):** sonnet.

- [ ] **Step 1:** Add the sentinel test, implement the no-reading display,
  run the commands, commit.

## Task 3: Developer test menu items are disabled in remote sessions

**Requirements:** R-R3-21, R-R3-25.

**Files:**
- Modify: `src/gui/MainWindow.cpp` (keep pointers to the two test actions; gate them in `applyRemoteRoleGating()`)
- Test: `tests/tst_remote_gui_gating.cpp`

**Interfaces:**
- Consumes: existing `applyRemoteRoleGating()`, `transmitControlsPermitted()`.
- Produces: no API change.

**Acceptance:**
- In a remote (Core-connected) role both "Test antenna switch toast" and
  "Test TX-bound re-route dialog" are disabled with the same tooltip wording
  the other unavailable remote transmit controls use; in local operation
  they behave exactly as today.
- Neither action can reach `emitAntennaAutoSwitched` or
  `requestTxBoundReRoute` while remote (test triggers the action while
  disabled and asserts no signal).
- Test in `tst_remote_gui_gating` for enabled locally, disabled remotely,
  and re-enabled after returning to local mode if that transition is
  already covered by the harness.

**Verification:** control-surface gating: functional test of the actions
in both roles.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_gui_gating -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^tst_remote_gui_gating$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** sonnet.

- [ ] **Step 1:** Gate the actions, add the test, run the commands, commit.

## Task 4: RADE transmit filter applies to a slice restored in RADE mode

**Requirements:** R-R3-24 (hydration shows the station's actual state),
R-R3-21.

Diagnosis (read-only, 2026-09-22): `TransmitModel::filterLow/filterHigh`
(`src/models/TransmitModel.h:306-307`, setters persist per radio
`TransmitModel.cpp:2924-2947`) are mirrored both ways
(`src/core/session/MirrorPolicy.cpp:258-259`), so the GUI shows the Core's
values. The only code that applies RADE's transmit passband is the
`SliceModel::dspModeChanged` handler in `RadioModel::wireSliceSignals()`
(`src/models/RadioModel.cpp:11139-11149`: entering RADE_U/RADE_L sets
650/2350, leaving sets 100/3900). At Core boot, `addSliceImpl()` restores a
saved slice's mode with `SliceModel::restoreReceiveState()`
(`RadioModel.cpp:5668-5672`) before `wireSliceSignals(slice)`
(`RadioModel.cpp:6041`) attaches that handler, so a slice restored already in
RADE-U never applies 650/2350 and the persisted 100/3900 stays. Correct RADE-U
passband is 650..2350 Hz (`src/core/WdspTypes.h:165-167`,
`SliceModel.cpp:1918-1926`, `2021-2025`).

**Files:**
- Modify: `src/models/RadioModel.cpp` (factor the handler body into one helper and apply it once after wiring when the slice is already in a RADE mode, under exactly the same conditions the handler uses)
- Test: `tests/tst_receive_layout_hydration.cpp`

**Interfaces:**
- Consumes: nothing new.
- Produces: no public API change (a private helper is fine).

**Acceptance:**
- Pre-seed the persisted transmit filter to 100/3900 for the test radio,
  hydrate a slice straight into RADE_U, and assert `filterLow() == 650` and
  `filterHigh() == 2350` immediately after hydration returns, with no mode
  change. Same for RADE_L (use the values the existing handler applies for
  RADE_L).
- A slice hydrated into a non-RADE mode leaves the transmit filter exactly
  as loaded (no 100/3900 write).
- The existing edge-triggered behavior (enter RADE, leave RADE) is
  unchanged; existing tests (`tst_rade_tx_filters`, `tst_tx_bandwidth_persistence`,
  `tst_radio_model_push_tx_mode_and_bandpass`) pass unchanged.
- Whatever condition the handler uses to decide which slice drives the
  transmit filter (for example only the transmit slice) is reused exactly;
  if it has none, record that in the report rather than inventing one.

**Verification:** hydration state: regression test first (fails on the
current tree), then the fix.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_receive_layout_hydration tst_rade_tx_filters tst_tx_bandwidth_persistence tst_radio_model_push_tx_mode_and_bandpass -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_receive_layout_hydration|tst_rade_tx_filters|tst_tx_bandwidth_persistence|tst_radio_model_push_tx_mode_and_bandpass)$' --no-tests=error --output-on-failure
```
Hardware (pending, controller/operator): after install, the GUI's TX BW
shows 650-2350 Hz right after launch with slice A in RADE-U.

**Execution note (advisory):** sonnet.

- [ ] **Step 1:** Add the regression, confirm it fails, apply the helper,
  run the commands, commit.

## Task 5: macOS local network permission and an actionable reconnect reason

**Requirements:** R-R3-17 (failed attempts leave a useful reason and a
recovery action), R-R3-38 (network and interface changes), R-R3-10.

Evidence (2026-09-22): the operator's GUI lost its Core session at 16:49 when
the Mac moved from ZeroTier to the station LAN (en0); for three hours every
reconnect failed with "Host unreachable" every 60 s while the Core port
answered from the shell and the Core stayed healthy. The bundle Info.plist has
no `NSLocalNetworkUsageDescription`; the ad-hoc-signed development app is most
likely blocked by macOS Local Network privacy, which reports EHOSTUNREACH.

**Files:**
- Modify: `CMakeLists.txt` (the macOS POST_BUILD step that already injects `NSMicrophoneUsageDescription` with PlistBuddy, `CMakeLists.txt:1618-1629`, also sets `NSLocalNetworkUsageDescription`)
- Modify: the code that turns a station connection error into operator text (find where "Station connection error" reasons are produced for the Core connection panel and banner, in `src/core/session/StationClient.*` and/or `src/gui/RemoteConnectionController.*`)
- Test: a unit test of the reason mapping (new or existing remote connection test target)

**Interfaces:**
- Consumes: nothing new.
- Produces: a pure helper that maps a connection failure (socket error kind or text, target host) to operator text; no API change elsewhere.

**Acceptance:**
- The built bundle's Info.plist contains `NSLocalNetworkUsageDescription` =
  "NereusSDR connects to your radios and NereusSDR Core stations on your local network."
  (set with Set-or-Add like the microphone key, so rebuilds stay idempotent).
- On macOS, when a connect attempt to a private or link-local address
  (IPv4 10/8, 172.16/12, 192.168/16, 169.254/16; IPv6 fc00::/7, fe80::/10)
  fails as host-unreachable, the operator reason reads exactly:
  "Can't reach the Core at <host>. If this Mac is on the same network as the Core, macOS may be blocking NereusSDR from your local network: allow it in System Settings, Privacy & Security, Local Network, then press Connect."
  The raw socket error still goes to the log. Other errors and non-private
  hosts keep today's text. Non-macOS builds keep today's text.
- Reconnect backoff behavior is unchanged.
- Tests cover private IPv4, link-local IPv6, a public address (unchanged
  text) and a non-host-unreachable error (unchanged text).

**Verification:** unit test of the mapping; the Info.plist key is verified
by the controller's final bundle build (do not build the app bundle in this
task while the operator's GUI runs from it); native behavior (the macOS
prompt and a working LAN reconnect) is pending the operator checkpoint.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Add the Info.plist key and the reason mapping with its
  test; run the covering tests; commit.
