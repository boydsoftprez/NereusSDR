# R3 polish batch implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.

**Goal:** close the findings parked during the September 22 native checks and
reviews: plain, unbroken operator wording; meters that show no reading instead
of a stale or floor number; no audio restart at connect; no repeated network
toast; a Connections window that keeps its size; and two Core robustness items.

**Architecture:** five independent tasks in existing files. Wording and
non-breaking spaces use the house pattern from 75b0e9be and 85c4ab27 (a
`\u00A0` escape between a number and its unit). That escape is safe in both
`tr()` and `QStringLiteral`: Qt 6's `Qt6::Platform` interface target adds
`-utf-8` for MSVC consumers (`QtFlagHandlingHelpers.cmake:392-409`,
`QtPlatformTargetHelpers.cmake:69`), so the Windows release build reads and
emits UTF-8 like GCC and Clang. Container meter items learn the same "no reading" rule the S-meter and slice
flags already use before the poller feeds them the sentinel. The remote audio
receiver discards a connect-time backlog instead of treating it as overflow.
The P2 established-silence check follows Thetis's wait semantics (loss only
when no datagram is waiting).

**Tech stack:** C++20, Qt 6 widgets and Qt Test, NereusSDR meter items,
`RemoteAudioReceiver`, `StationClient`, `P2RadioConnection`.

**Spec:** parked rulings in the crew ledgers of the remote audio status and
native-check plans (September 22), native-check gap G6
(`~/.config/nereus/work/r3-native-checks-2026-09-22.md` is private; the
findings are restated below), R-R3-06, R-R3-07, R-R3-13, R-R3-17, R-R3-29,
R-R3-34.

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`),
  never `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash
  characters. Stage explicit paths only. Every commit names its R-R3 IDs.
- Non-breaking spaces between a number and its unit: write the escape
  `\u00A0` in the source (the house pattern), never a raw non-breaking
  space character, so the source stays readable and diffable.
- Operator strings are plain English with no internal words (stream, owner,
  schema, JSON, window, generation, pipe).
- Task 5 changes ported Thetis logic in `src/core/P2RadioConnection.cpp`
  (registered in `docs/attribution/THETIS-PROVENANCE.md`, header already
  carries `network.c` and `netInterface.c`): follow CLAUDE.md's source-first
  protocol, cite `// From Thetis ChannelMaster/network.c:<lines> [v2.10.3.15]`,
  and keep every existing inline tag. If a needed Thetis fact cannot be found,
  stop and report; never infer it.
- Tests: build exact targets, then
  `QT_QPA_PLATFORM=offscreen ctest --test-dir build-integration -R '^(<targets>)$' --no-tests=error --output-on-failure`
  (drop the prefix once the off-screen default has landed). Record load
  averages with timing. No unfiltered suite. Never build the `NereusSDR`
  target; the controller builds the app at gates. Hardware is off limits.

## What already exists

Read-only investigation, 2026-09-22 (line numbers at 136a71eb; `RadioModel.cpp`
has moved by about +9 near 4300-4500 and +14 near 13400 since):
- Restore refusal strings `RadioModel.cpp:4483, 4486` put `%2 MHz` in `tr()`.
  The earlier fixes wrote `\u00A0` in `QStringLiteral`
  (`src/gui/RemoteAudioStatus.cpp:145,151,175,183`,
  `src/core/SliceStreamAllocator.cpp:162-163`) and in `tr()`
  (`src/gui/RemoteTelemetryController.cpp:354, 360, 362, 366`).
- `plainReceiveLayoutProblem` (`RadioModel.cpp:4333-4383`) maps both
  "invalid RADE receive owner" (`src/core/ReceiveLayoutStore.cpp:291`, a
  damaged record) and "RADE receive owner is not a RADE slice" (`:145`) to
  the "not in RADE mode" sentence, because its step 7 matches "RADE receive
  owner".
- Save refusals `RadioModel.cpp:13460-13462, 13479, 13480` reach an Error toast
  (`MainWindow.cpp:5041-5046`); `tests/tst_receive_layout_native.cpp:147`
  checks only that the text is non-empty.
- `SliceStreamAllocator.cpp:76-78` joinStream reason names "receiver window
  (stream %1)"; `tests/tst_slice_stream_allocator.cpp` has no joinStream case.
- Container meters: remote `pollRemoteRxMeters` pushes -140 to SignalPeak and
  SignalAvg with no reading (`src/gui/meters/MeterPoller.cpp:398-399`); local
  `poll()` returns early once the RX channel is gone (`:329`), so items freeze
  on their last value. `SMeterWidget.cpp:201` and `VfoLevelBar.cpp:44` already
  treat -400 as "no reading". Numeric items: NeedleItem (clamps to -127,
  `MeterItem.cpp:1349, 1830-1843`), TextItem (`:1227`; CompactHBar idle text
  threshold -140 at `ItemGroup.cpp:496-497`), BarItem value and peak text
  (`MeterItem.cpp:483-514, 615, 629`), SignalTextItem (`SignalTextItem.cpp:158, 254`),
  TextOverlayItem `%VALUE%` (`TextOverlayItem.cpp:101-103`), HistoryGraphItem
  axis (`HistoryGraphItem.cpp:241-264`).
- Connect-time burst: `ensureSpeakersOpen()` blocks the GUI thread at audio
  start; the transport's 2 ms drain stalls; about 63 RTP packets arrive at once
  (`LibDataChannelMediaTransport.cpp:696-714`); `RemoteAudioReceiver::submit`
  (`RemoteAudioReceiver.cpp:455-473`) overflows its 8-packet queue
  (`AudioJitterBuffer.h:16`), admits nothing (`:299-302`) and the controller
  restarts audio (`RemoteMediaController.cpp:436-452`).
- Every failed dial toasts "Station link lost: %1" (`MainWindow.cpp:1118-1127`)
  forever at 60 s; the same reason is already shown persistently (Connections
  window, Core panel, title bar). `MainWindow` already de-duplicates the
  receive layout warning with `m_lastReceiveLayoutWarning` (`MainWindow.h:891`,
  `MainWindow.cpp:1097-1110`).
- `ConnectionSelector` (`src/gui/ConnectionSelector.cpp`): `updateActions()`
  (364-383) shows or hides five of nine buttons in one row on row selection;
  the dialog's minimum width grows and Qt enlarges it (seen: 820x572 to 1156x710).
- `tests/tst_session_link_loss.cpp:490-573`
  `autoReconnectUsesOwnedCancellableTimerWithExponentialBackoff` polls with
  `QTRY_COMPARE` against a 200 ms backoff (531, 535, 539, 568); 9982cbed fixed
  the sibling test by waiting on the signal.
- `src/core/audio/RealtimeAudioPriority.cpp` logs SCHED_FIFO (229) and nice(-5)
  (275-279) refusals at info level per thread; `nice()` failures are judged
  from `errno` without clearing it first (254, 263, 275).
- `P2RadioConnection.cpp:2271-2290` declares established silence when its
  3000 ms timer fires, measured from the last processed packet, without
  checking for datagrams already waiting. Thetis's read loop times out only
  when its wait finds nothing (`ChannelMaster/network.c:655-667 [v2.10.3.15]`).

## Task 1: Plain, unbroken receive-layout wording

**Requirements:** R-R3-34.

**Files:**
- Modify: `src/models/RadioModel.cpp` (restore strings, `plainReceiveLayoutProblem`
  order), `src/core/SliceStreamAllocator.cpp` (joinStream reason)
- Test: `tests/tst_receive_layout_runtime.cpp`, `tests/tst_receive_layout_native.cpp`,
  `tests/tst_slice_stream_allocator.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: no new API.

**Acceptance:**
- Both restore refusal strings keep a non-breaking space between the
  frequency and "MHz"; the five existing expectations in
  `tst_receive_layout_runtime.cpp` are updated to the exact new text.
- `plainReceiveLayoutProblem`: "not a RADE slice" maps to the "not in RADE
  mode" sentence; "invalid RADE receive owner" maps to the damaged record
  sentence ("The saved receive layout could not be loaded. It is damaged or was
  written by a different version of this program. The configured receivers are
  used instead. Your saved layout is kept."). New data rows: owner 2 changed to
  5 gives the damaged sentence; the owner's mode changed from RADE to USB gives
  the "not in RADE mode" sentence. `restoreMessageProblem` still finds no
  internal words.
- Save refusals: `tst_receive_layout_native.cpp:147` compares the exact RADE
  sentence; a new runtime case (pan key "pan-01" on a live slice, then flush,
  modelled on `failedAtomicSaveRetainsLiveLayoutForRetry`) expects exactly
  "Your receivers were not saved. It places a receiver on a panadapter this
  program does not recognize."
- joinStream reason reads "The radio receiver this panadapter uses does not
  cover %1 MHz. Retune into its range, or give this receiver a panadapter of
  its own." with the frequency to 4 decimals and a non-breaking space before
  "MHz"; a new `tst_slice_stream_allocator` case pins it.

**Verification:** wording: exact-string tests; the mapping rows fail first on
the current tree.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_receive_layout_runtime tst_receive_layout_native tst_receive_layout_store tst_slice_stream_allocator -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_receive_layout_runtime|tst_receive_layout_native|tst_receive_layout_store|tst_slice_stream_allocator)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Add the new cases; confirm the mapping rows fail on the
  current tree.
- [ ] **Step 2:** Fix the strings and the mapping; run the commands; commit.

## Task 2: Container meters show no reading as "--"

**Requirements:** R-R3-13.

**Files:**
- Modify: `src/gui/meters/MeterItem.cpp` (and its header if a shared helper is
  added), `src/gui/meters/SignalTextItem.cpp`, `src/gui/meters/TextOverlayItem.cpp`,
  `src/gui/meters/HistoryGraphItem.cpp`, `src/gui/meters/ItemGroup.cpp`
  (CompactHBar idle threshold), `src/gui/meters/MeterPoller.cpp`
- Test: `tests/tst_remote_meter_poller.cpp`, a new
  `tests/tst_meter_item_no_reading.cpp`

**Interfaces:**
- Consumes: the existing sentinel convention (-400 means no reading;
  `SMeterWidget.cpp:201`, `VfoLevelBar.cpp:44`).
- Produces: one shared predicate (for example `bool isNoMeterReading(double dbm)`
  next to the meter items) used by every numeric item.

**Acceptance:**
- Given -400, NeedleItem rests its needle at the scale minimum and shows "--"
  where it shows a number; TextItem, BarItem (value and peak text),
  SignalTextItem and TextOverlayItem `%VALUE%` show "--" (with the unit where
  the item shows one today, as the S-meter does); HistoryGraphItem skips the
  sample for its axis and line. A real -140 reading still displays as a number.
- Only after the items handle it: the remote poll feeds -400 to SignalPeak and
  SignalAvg when there is no reading (instead of -140), and the local poll
  feeds -400 once the RX channel is gone instead of leaving items frozen.
- Remote disconnect sequence in `tst_remote_meter_poller` sees "--" on the
  bound TextItems; a new item test covers each numeric item type with -400,
  -140 and a normal reading.

**Verification:** display behaviour with functional checks through the real
poller and items (off-screen); native appearance is the operator's checkpoint.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_meter_poller tst_meter_item_no_reading -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_meter_poller|tst_meter_item_no_reading)$' --no-tests=error --output-on-failure
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R 'meter' --no-tests=error --output-on-failure
```
(The last line runs every existing test with "meter" in its name, after
building them with `cmake --build ... --target all_tests` or the individual
targets it lists; there is no `meters` label.)

**Execution note (advisory):** opus.

- [ ] **Step 1:** Item test first (fails today); then the shared predicate
  and item changes.
- [ ] **Step 2:** Poller feeds; run the commands; commit.

## Task 3: No audio restart from a connect-time backlog

**Requirements:** R-R3-06, R-R3-07.

**Files:**
- Modify: `src/core/session/media/RemoteAudioReceiver.{h,cpp}`
- Test: `tests/tst_remote_audio_receiver.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: a telemetry counter for packets discarded at start (name it in the
  style of the receiver's existing counters) shown wherever the existing
  receiver counters are logged.

**Acceptance:**
- Until the first packet of a context is admitted, `submit()` discards the
  oldest queued packet instead of raising overflow; when the worker first
  admits, it keeps only the newest packets up to the receiver's normal starting
  fill (name the existing constant; if the receiver has none, keep only the
  newest packet) and resets the jitter buffer to the oldest kept timestamp, so
  no "needs a fresh context after a stream gap" restart follows.
- A burst of 63 packets delivered at once before the first admission starts
  playback without an overflow restart, with every packet not kept counted as
  discarded at start and continuous timestamps from the kept packets onward.
- After the first admission the existing overflow rule is unchanged:
  `arrivalBurstsRemainBounded` and the existing overflow tests pass unchanged.

**Verification:** real-time audio path, consequential: the burst regression
fails first on the current tree.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_remote_audio_receiver tst_remote_audio_session tst_remote_audio_status -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_remote_audio_receiver|tst_remote_audio_session|tst_remote_audio_status)$' --no-tests=error --output-on-failure
```
Hardware (pending, operator): the GUI log at connect shows no "arrival queue
exceeded its latency bound" line and no immediate audio restart.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Burst regression (fails today).
- [ ] **Step 2:** Start-time discard and jitter reset; run the commands; commit.

## Task 4: One network toast per reason, a steady Connections window, a steady test

**Requirements:** R-R3-17.

**Files:**
- Modify: `src/gui/MainWindow.{h,cpp}` (toast de-duplication),
  `src/gui/ConnectionSelector.cpp` (stable size),
  `tests/tst_session_link_loss.cpp` (signal waits)
- Test: `tests/tst_gui_session_coordinator.cpp`, `tests/tst_connection_selector.cpp`

**Interfaces:**
- Consumes: nothing.
- Produces: no new API.

**Acceptance:**
- "Station link lost: %1" is toasted once per distinct reason: repeated
  failures with the same reason do not toast again; a different reason does; a
  successful handshake or `disconnectFromStation()` clears the memory (same
  pattern as `m_lastReceiveLayoutWarning`). The reconnect-attempt toast follows
  the same rule. The persistent status text is unchanged.
- Selecting rows that show different action buttons never changes the
  Connections window's size, and the window never opens narrower than the full
  row of nine buttons needs. Hidden buttons stay hidden (no disabled
  placeholders) and `controlsRemainReadableAndReachable` still passes. Row
  detail text that does not fit scrolls or wraps inside the existing area
  rather than growing the window.
- `autoReconnectUsesOwnedCancellableTimerWithExponentialBackoff` waits on the
  retry signal for counts 1, 2, 3 and after cancel, like 9982cbed, and its
  comment is corrected.

**Verification:** UI behaviour with functional checks (off-screen), plus the
test-stability change run five times in a row.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_gui_session_coordinator tst_connection_selector tst_session_link_loss -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_gui_session_coordinator|tst_connection_selector)$' --no-tests=error --output-on-failure
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^tst_session_link_loss$' --repeat until-fail:5 --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Toast and window tests (fail today), then the fixes.
- [ ] **Step 2:** Test waits; run the commands; commit.

## Task 5: Core: one clear priority warning; silence only when nothing is waiting

**Requirements:** R-R3-29.

**Files:**
- Modify: `src/core/audio/RealtimeAudioPriority.cpp`, `src/core/P2RadioConnection.cpp`
  (established-silence expiry; stale "~2 s deadman" comment near line 529 that
  has no Thetis source), `src/core/ConnectionState.h` (stale ">5s" comment at
  line 21)
- Test: `tests/tst_p2_established_silence.cpp`, new `tests/tst_realtime_audio_priority.cpp`
  (Linux-only assertions may be compiled out elsewhere; the once-only warning
  logic is testable on every platform through a seam)

**Interfaces:**
- Consumes: nothing.
- Produces: no new API beyond a test seam.

**Acceptance:**
- When real-time scheduling or the nice change is refused, the process logs
  one warning in plain words (for example "Real-time scheduling was refused;
  audio and signal processing threads run at normal priority.") the first time
  only; per-thread details stay at info. `errno` is cleared before each
  `nice()` call so a stale value cannot report a false failure.
- P2: when the silence timer expires but the socket has datagrams waiting, the
  connection does not declare loss; it lets the waiting datagrams be processed
  (which re-arms the timer) and checks again. Loss is declared only when
  nothing is waiting, matching `network.c:655-667 [v2.10.3.15]` (cite it at the
  change). The existing silence tests pass; a new case queues datagrams before
  the timer fires and asserts no `NoDataTimeout`.
- The two stale comments are corrected to what the code and Thetis show (no
  invented timing).

**Verification:** consequential state transition on the radio link: the
queued-datagram regression fails first on the current tree.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_p2_established_silence tst_realtime_audio_priority nereusd -j6
QT_QPA_PLATFORM=offscreen ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_p2_established_silence|tst_realtime_audio_priority)$' --no-tests=error --output-on-failure
```
Hardware (pending, controller): the Rock's journal after install shows the
single warning line.

**Execution note (advisory):** opus (radio link).

- [ ] **Step 1:** Queued-datagram regression (fails today) and the warning
  test.
- [ ] **Step 2:** Implement both; fix the comments; run the commands; commit.
