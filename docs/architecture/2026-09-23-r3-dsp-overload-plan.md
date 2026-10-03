# R3 DSP overload resilience implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.
> Tasks 1 and 2 change the lock and the shutdown path every WDSP channel shares
> and Task 8 changes where the Core's threads run: they are candidates for an
> earlier independent review, which the operator decides.

**Goal:** a Core whose signal processing cannot keep up (today: the premium
neural noise reduction on a Rock 5C) stays connected and controllable, never
frees memory a slow block still uses, steps the noise reduction back by itself
with a plain reason, bounds its receive delay, shows each receiver's processing
load, runs its busy DSP work on its fastest cores, and never leaves a reconnect
hanging halfway. The model's real cost on the Core's hardware, and how much a
relaxed floating-point build would save, are measured, not assumed.

**Architecture:** WDSP gains one NereusSDR file (`dsplock.c`) that gives
control threads a bounded turn at each channel's DSP lock, lets channel
shutdown wait for its worker, and keeps per-channel load counters; the pinned
upstream files change only at the calls into it. NereusSDR reads the counters
without locks, bounds its own input queue, reports the load in telemetry, and
a Core-side governor lowers the NNR model through a lock-free request the
worker applies at its next block. A Linux-only placement helper pins DSP
threads to the fastest cores from sysfs data. The station client and server
both give up on a handshake that does not finish in time.

**Tech stack:** C (WDSP, `third_party/wdsp/src`), C++20, Qt 6, Linux sysfs and
scheduler APIs, Qt Test (off-screen), CMake.

**Spec:** [R3 plan](2026-09-20-remote-daemon-r3-plan.md) requirements R-R3-39,
R-R3-40 and R-R3-41 (added with this plan) plus R-R3-16, R-R3-17, R-R3-32 and
R-R3-33. Operator decisions of 2026-09-23: fair turn-taking for the DSP lock
(A) and automatic step-back of noise-reduction models that cannot keep up
(B) now; a separate DSP-control thread (C) before remote transmit (R4);
fast-core placement, the reconnect deadline and a measure-only speed test
approved as part of this work.

**Evidence (2026-09-23, Rock 5C, RK3588S, Linux 6.1):** with the premium
model the Core's RX worker ran at 100% of a 2.35 GHz Cortex-A76 for 25 minutes
with 0-1 voluntary waits per 10 s; the standard model runs at about 50% with
waits. The GUI declared the link dead at 08:55:25 and 08:57:32; the reconnect
after 08:57:32 connected but did not finish its handshake for 8.7 minutes. A
model switch at 09:10:10 dropped the session again. The Core logs "Raised
thread priority was refused" at startup: the vendor kernel has
`CONFIG_RT_GROUP_SCHED=y` and cgroup2's root enables the cpu controller, so
real-time policies return EPERM for every service; the unit also has no
`LimitNICE`. Private scout notes with full line references:
`/Users/j.j.boyd/.config/nereus/work/g10-nnr-stall-lock-scout-2026-09-23.md`,
`nnr-lock-stepback-scout-2026-09-23.md`, `thread-placement-scout-2026-09-23.md`
(same directory).

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`),
  never `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash
  characters in commit messages, docs or new operator strings. Stage explicit
  paths only. Every commit names its R-R3 IDs.
- WDSP (`third_party/wdsp/src`) is Warren Pratt NR0V's GPL code. Keep every
  upstream header and comment. New logic goes in NereusSDR files
  (`dsplock.c`/`dsplock.h`, the retained `linux_port.c`/`linux_port.h`, the
  already-modified `nnr.c`); pinned upstream files change only where they must
  call into it. Every changed or new WDSP file is recorded in the same commit
  in `docs/attribution/WDSP-PROVENANCE.md` (its disposition row, the file
  counts, and the "Reviewed downstream integrations" table), and both
  `python3 scripts/audit-wdsp-headers.py` and
  `python3 scripts/verify-thetis-headers.py --all-kinds` pass. Changed upstream
  files carry a "NereusSDR modifications" block like `nnr.c:27-30` (date,
  J.J. Boyd KG4VCF, AI tooling disclosure). New WDSP-tree files follow the
  header style of the retained NereusSDR WDSP files.
- No change to DSP algorithms, constants, numerics or defaults. The only WDSP
  behaviour changes are lock scheduling, shutdown waiting, load counters and
  the runtime NNR limit. The benchmark's relaxed build is a measurement
  artifact that never ships.
- WDSP code must also build with MSVC, which nobody can build here: use only
  APIs WDSP already uses on Windows (`Interlocked*`, `Sleep`, `SwitchToThread`,
  `QueryPerformanceCounter`) and say in the report that Windows is unverified.
- Real-time paths (the WDSP worker, RxDspWorker's processing, the audio pump)
  gain no mutex waits, allocations, logging or blocking calls per block beyond
  what a task specifies; values shared across threads are atomics.
- The operator's saved settings are never rewritten by automatic behaviour; a
  step-back is a runtime limit.
- Operator strings are plain English: "Standard" and "Premium" are the model
  names the GUI already uses; no slot numbers, protocol, capability or roadmap
  names.
- NereusSDR-original files carry the house header plus
  `// no-port-check: NereusSDR-original. <reason>`.
- Tests run off-screen by default. Build exact targets, then
  `ctest --test-dir build-integration -R '^(<targets>)$' --no-tests=error --output-on-failure`.
  No unfiltered suite (the controller runs it). Never build the `NereusSDR`
  target. Record `uptime` load averages with any timing.
- Hardware is off limits to implementers: the Rock (192.168.109.106), the
  radios, the G2's Raspberry Pi and the running GUI. Device steps belong to the
  controller.

## What already exists

Read-only investigation at 5478544e (scout notes above):
- WDSP worker loop `third_party/wdsp/src/main.c:37-59`: waits on
  `Sem_BuffReady`, takes `ch[channel].csDSP` (:40), runs `dexchange` and
  `xrxa`/`xtxa`, releases (:58). On Linux/macOS `CRITICAL_SECTION` is a
  recursive pthread mutex with no handoff (`linux_port.h:46`,
  `linux_port.c:57-77`); on Windows it is native with spin count 2500
  (`channel.c:54-55`). 273 `EnterCriticalSection(&ch[..].csDSP)` sites in 40
  files; NereusSDR calls about 200 of them from the daemon's event loop, which
  is also the Core's session I/O, heartbeat and 10 ms Opus audio pump
  (`DaemonAudioSender.cpp:28-30`).
- NereusSDR opens RX channels with `bfo=1` (`WdspEngine.cpp:467-480`), so
  WDSP lets the caller lead by at most a block; the backlog lives in
  RxDspWorker's unbounded queued connection (`RadioModel.cpp:10303-10305`,
  reconnected at 4979, 16533, 16711; one DSP thread for every slice,
  `RxDspWorker.cpp:603-652`). Nothing measures per-channel load.
- `pre_main_destroy` (`channel.c:103-111`) clears `run`, releases the
  semaphore and sleeps 25 ms before buffers and `csDSP` are freed; a block
  longer than 25 ms is a use-after-free risk on close and on buffer-size or
  rate rebuilds.
- NNR: `nnr.c` (already NereusSDR-modified, block at :27-30),
  `setModel_nnr` (:625-648) is a pointer swap and memsets; status structs in
  `nnr_compat.h:27-52`; `NnrAdapter.cpp:24-99`; RxChannel caches
  `m_nnrTuning` (`RxChannel.cpp:1098-1157`, default slot 0 at
  `NnrSettings.h:18`); `SliceModel::applyNnrSettings` skips equal requests
  (`SliceModel.cpp:1134`); appliers `RadioModel.cpp:3919-3946`; mirror fields
  `MirrorPolicy.cpp:146-171` (tuning bidirectional; actual slot, status and
  last error outbound) applied via `StationClient.cpp:2021-2023`; GUI
  `NnrControls.cpp:414-440` ("Standard"/"Premium" at :102-103),
  `VfoWidget.cpp:1432-1446, 2337`, `RxDashboard.cpp:288-294`. Premium is about
  2.2 times the standard model's compute.
- Threads: every WDSP thread comes from `wdsp_beginthread`
  (`linux_port.c:240-272`); RX channels 0-4, TX 5 and PureSignal 6 each have
  a worker and a flush thread. Priority: `RealtimeAudioPriority.cpp` tries
  SCHED_FIFO 98 (DspThread) and `nice(-5)`/`nice(-3)` (connection, spectrum);
  the WDSP worker's own attempt is Windows-only. No affinity code exists.
  Unit template `packaging/nereusd.service.in` (`LimitRTPRIO=99` at :174,
  `RestrictRealtime` deliberately unset at :101-102).
- Telemetry: `kSessionProtocolMinor = 10` (`SessionMessages.h:177`),
  `stationTelemetryVersion` (`StationCapabilities.h:124`), host section and
  Core tab from the host telemetry plan.
- Tools: `tools/nereus-media-probe.cpp` built `EXCLUDE_FROM_ALL`
  (`CMakeLists.txt:2119-2122`); tests link `wdsp_static` directly
  (`tests/CMakeLists.txt:285-297`, `tst_linux_port_wait`, `tst_wdsp_nnr`).
- CI: "WDSP header census drift detector" and license-marker steps
  (`.github/workflows/ci.yml`) run the two scripts named above.

## Task 1: Fair turn-taking for each channel's DSP lock

**Requirements:** R-R3-39.

**Files:**
- Create: `third_party/wdsp/src/dsplock.c`, `third_party/wdsp/src/dsplock.h`
- Modify: `third_party/wdsp/src/comm.h` (redirect `EnterCriticalSection`
  after the platform include), `third_party/wdsp/src/main.c` (worker enter and
  leave through dsplock), `third_party/wdsp/src/linux_port.c` (use the real
  call where the redirect must not apply), `third_party/wdsp/CMakeLists.txt`,
  `docs/attribution/WDSP-PROVENANCE.md`, `src/core/wdsp_api.h` (test seam)
- Test: `tests/tst_wdsp_dsp_turn_taking.cpp` (new, links `wdsp_static` like
  `tst_linux_port_wait`), `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing.
- Produces:
  ```c
  /* dsplock.h (WDSP-internal) */
  void WdspEnterCS (LPCRITICAL_SECTION cs);   /* target of the redirect */
  void WdspWorkerEnter (int channel);         /* replaces main.c:40 */
  void WdspWorkerLeave (int channel);         /* replaces main.c:58 */
  /* exported test seam, default 0 = off */
  PORT void WDSPSetTestBlockDelayUs (int channel, int microseconds);
  ```
  Waiter counts live in `dsplock.c` (one per channel), not in `struct _ch`.

**Acceptance:**
- Red first: on the unchanged tree the new test opens a real RX channel, sets
  a 20 ms test block delay (busy-wait inside the locked section), and feeds
  input at ten times real time with `bfo` off so the worker never idles. From a
  control thread it times 50 single calls to a csDSP getter (for example
  `GetRXAAGCTop`) and one burst of 14 consecutive csDSP calls. Record the
  worst latencies before the change. After the change: worst single call
  within 30 ms; the burst within 20 ms plus the budget plus 10 ms; worker
  throughput at least 90% of a baseline without control calls measured in the
  same test.
- A thread entering a channel's `csDSP` announces itself; before retaking the
  lock the worker waits while announced waiters exist, for at most
  `min(block period / 4, 20 ms)` per block, with a 1 ms grace so a burst of
  calls stays together, pausing about 50 us between checks. Named constants.
  With no waiters the worker adds one atomic read and no sleep.
- `csDSP` is recognised by address for every channel; every other lock goes
  straight to the platform call. Recursive acquisitions, the worker's own
  nested calls, the flush thread (`channel.c:137`), PureSignal's use of the TX
  channel lock, and TX channels behave as before.
- The test seam costs one relaxed load per block when off and is documented
  as test-only.
- Provenance updated; both scripts pass. Report states Windows is unverified.

**Verification:** reachability of the Core; red first; real channel
integration test.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_wdsp_dsp_turn_taking tst_linux_port_wait tst_wdsp_nnr tst_wdsp210_cfc_compat nereusd -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_wdsp_dsp_turn_taking|tst_linux_port_wait|tst_wdsp_nnr|tst_wdsp210_cfc_compat)$' --repeat until-fail:3 --no-tests=error --output-on-failure
python3 scripts/audit-wdsp-headers.py && python3 scripts/verify-thetis-headers.py --all-kinds
```
Also build and run every existing test whose name matches
`wdsp|rx_channel|tx_channel|nnr|nb_family|pure_?signal|ps_|sample_rate`
(list them in the report).

**Execution note (advisory):** opus. First task.

- [ ] **Step 1:** Write the turn-taking test and record the red numbers.
- [ ] **Step 2:** Add dsplock, the redirect and the worker calls; run the
  commands; update provenance; commit.

## Task 2: Channel shutdown waits for its worker

**Requirements:** R-R3-39.

**Files:**
- Modify: `third_party/wdsp/src/dsplock.c`, `dsplock.h` (worker-exit signal
  per channel), `third_party/wdsp/src/main.c` (signal after the loop ends),
  `third_party/wdsp/src/channel.c` (`pre_main_destroy` waits instead of
  `Sleep (25)`), `docs/attribution/WDSP-PROVENANCE.md`
- Test: `tests/tst_wdsp_dsp_turn_taking.cpp` (or a new
  `tests/tst_wdsp_channel_shutdown.cpp`)

**Interfaces:**
- Consumes: Task 1's `WDSPSetTestBlockDelayUs`.
- Produces: `void WdspWaitWorkerExit (int channel);` (WDSP-internal).

**Acceptance:**
- Red first: with a 60 ms test block delay and the worker busy, `CloseChannel`
  returns while the worker is still inside its block (observable through the
  exit signal or a test counter). After: every path through
  `pre_main_destroy` (close and each rebuild that calls it; list them in the
  report) returns only after the worker has left its loop.
- The wait never frees a running worker's buffers: it waits until the worker
  exits, writing one log line (WDSP's `dprintf`) per 2000 ms of waiting
  (named constant). An idle worker exits at once (no added delay).
- Existing sample-rate and buffer-size tests pass (name them in the report).
- Provenance updated; both scripts pass.

**Verification:** memory safety on the DSP path; red first.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_wdsp_dsp_turn_taking tst_linux_port_wait tst_wdsp_nnr nereusd -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_wdsp_dsp_turn_taking|tst_wdsp_channel_shutdown|tst_linux_port_wait|tst_wdsp_nnr)$' --repeat until-fail:3 --output-on-failure
python3 scripts/audit-wdsp-headers.py && python3 scripts/verify-thetis-headers.py --all-kinds
```
Plus the same name-pattern sweep as Task 1.

**Execution note (advisory):** opus. Requires Task 1.

- [ ] **Step 1:** Shutdown test, red.
- [ ] **Step 2:** Worker-exit wait; run the commands; update provenance; commit.

## Task 3: A reconnect that never finishes setting up gives up and retries

**Requirements:** R-R3-16, R-R3-17.

**Files:**
- Modify: `src/core/session/StationClient.{h,cpp}`,
  `src/core/session/StationServer.{h,cpp}`, and
  `src/gui/RemoteConnectionController.cpp` only if the status text needs it
- Test: `tests/tst_station_session.cpp` or a new
  `tests/tst_station_handshake_deadline.cpp` (reuse `tests/fakes`)

**Interfaces:**
- Consumes: nothing.
- Produces: `kStationHandshakeDeadlineMs = 30000` shared by client and server,
  injectable for tests.

**Acceptance:**
- First establish, with the existing harness, which handshake step a GUI waits
  on when the Core's event loop is blocked during the hello (the 2026-09-23
  incident: after a heartbeat timeout at 08:57:32 the reconnect connected and
  sat for 8.7 minutes; the Core's audio context for that peer consumed nothing
  and dropped 9075 blocks). Name the step in the report.
- GUI: from transport connected until the handshake completes, the deadline
  runs; on expiry the client closes the link, records the reason "The station
  did not finish connecting." and schedules the next attempt with the normal
  backoff. The window's persistent status shows the reason; Cancel during the
  wait stops it; a handshake that completes in time is unaffected.
- Core: a peer that has not completed its handshake within the deadline is
  detached with one log line and its media context retired.
- Tests: a Core that accepts and never answers (the GUI times out and
  retries); a GUI that connects and never sends its hello (the Core detaches
  it); a Core blocked for longer than the deadline during the hello and then
  responsive (the next attempt succeeds). Short deadlines through the
  injectable value; no real 30 s waits.

**Verification:** connection recovery; test-first.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_station_session tst_station_handshake_deadline tst_remote_connection_controls tst_gui_connection_controller -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_station_session|tst_station_handshake_deadline|tst_remote_connection_controls|tst_gui_connection_controller)$' --repeat until-fail:3 --output-on-failure
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Reproduce and name the stuck step; deadline tests, red.
- [ ] **Step 2:** Client and server deadlines; run the commands; commit.

## Task 4: The display error test waits for a pending frame

**Requirements:** R-R3-03, R-R3-05 (the requirements the test covers).

**Files:**
- Modify: `tests/tst_daemon_media_controller.cpp`
  (`realDisplayErrorIsCountedAndLoggedOnce`, around :1140-1152)

**Interfaces:** none.

**Acceptance:**
- The full off-screen suite at 5478544e failed this case once
  ("no display frame was waiting to be sent", `:1152`) under load 10-28; it
  passes alone. The fixed `QTest::qWait(200)` after `feedRadio(0.3125)`
  assumes a frame is pending. Replace it with a wait on an observable
  condition that a frame is pending before the far end stops; the case still
  proves a real send error is counted and logged once.
- Passes `--repeat until-fail:10` while a heavy build runs alongside (record
  the load averages).

**Verification:** test reliability.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_daemon_media_controller -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^tst_daemon_media_controller$' --repeat until-fail:10 --output-on-failure
```

**Execution note (advisory):** opus (small).

- [ ] **Step 1:** Condition-based wait; loaded repeat run; commit.

## Task 5: Measure each receiver's DSP load and bound its input delay

**Requirements:** R-R3-40.

**Files:**
- Modify: `third_party/wdsp/src/dsplock.{c,h}` (per-channel load counters in
  `WdspWorkerEnter`/`WdspWorkerLeave`, exported reader),
  `docs/attribution/WDSP-PROVENANCE.md`, `src/core/wdsp_api.h`,
  `src/core/RxChannel.{h,cpp}` (reader), `src/models/RxDspWorker.{h,cpp}`
  (enqueue timestamps, delay bound), `src/models/RadioModel.{h,cpp}` (wiring
  and a per-receiver load accessor for Tasks 6 and 7)
- Test: `tests/tst_wdsp_dsp_turn_taking.cpp` (counters), a new
  `tests/tst_rx_dsp_worker_input_delay.cpp` (delay bound), the existing
  `tst_rx_dsp_worker_thread`, `tst_rx_dsp_worker_multi_slice` and
  `tst_rx_dsp_worker_buffer_sizing`

**Interfaces:**
- Consumes: Task 1's worker enter and leave, `WDSPSetTestBlockDelayUs`.
- Produces:
  ```c
  typedef struct { long long blocks; long long busyNs; long long lateBlocks;
                   long long maxBlockUs; int blockPeriodUs;
                   long long currentBlockNs; long long readNs; } WdspChannelLoad;
  PORT int GetChannelDspLoad (int channel, WdspChannelLoad* out); /* 0 = ok */
  ```
  and in NereusSDR a per-receiver snapshot `{ double load; /* busy time over
  wall time between two reads: the change in busyNs + currentBlockNs over the
  change in readNs; 1.0 = the worker never left its blocks, cannot keep up */
  qint64 lateBlocks; /* diagnostic only, not overload */ qint64 maxBlockUs;
  qint64 inputDelayMs; qint64 droppedInputMs; }`
  (Amended 2026-09-23 by the receiver load reading hotfix,
  `2026-09-23-r3-receiver-load-hotfix-plan.md`: the load was mean block time /
  block period, raised to a block's time so far / block period whenever a
  sample landed inside a late block, which read frame-based noise reduction
  as overload.)
  reachable from RadioModel by slice ID (exact C++ names are the
  implementer's; record them in the report for Tasks 6 and 7).

**Acceptance:**
- The worker times each block (lock acquired to release) with a monotonic
  clock (`CLOCK_MONOTONIC`, `QueryPerformanceCounter` on Windows); cumulative
  counters are atomics readable without `csDSP`; reading never blocks the
  worker; the cost is two clock reads per block.
- With the test block delay, the measured load matches delay / block period
  within 10%.
- RxDspWorker stamps each queued I/Q batch; when the delay between enqueue
  and processing exceeds `kDspInputDelayLimitMs = 500` it skips batches until
  the delay falls below `kDspInputDelayResumeMs = 250`, counting skipped
  milliseconds per receiver and writing one plain log line per episode
  ("Receive processing fell behind; skipped N ms of input to catch up.").
  Red first: with a slowed processing stub the delay grows without limit on
  the unchanged tree. Under normal load nothing is skipped.
- Provenance updated; both scripts pass.

**Verification:** ordinary feature on a real-time path; unit plus real-channel
integration.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_wdsp_dsp_turn_taking tst_rx_dsp_worker_input_delay tst_rx_dsp_worker_thread tst_rx_dsp_worker_multi_slice tst_rx_dsp_worker_buffer_sizing nereusd -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_wdsp_dsp_turn_taking|tst_rx_dsp_worker_input_delay|tst_rx_dsp_worker_thread|tst_rx_dsp_worker_multi_slice|tst_rx_dsp_worker_buffer_sizing)$' --repeat until-fail:3 --output-on-failure
```
Plus the Task 1 name-pattern sweep and `radio_model`.

**Execution note (advisory):** opus. Requires Task 1.

- [ ] **Step 1:** Counters and reader with tests.
- [ ] **Step 2:** Input delay bound, red then green; run the commands; commit.

## Task 6: Show receiver processing load in Network Diagnostics

**Requirements:** R-R3-40, R-R3-32, R-R3-33.

**Files:**
- Modify: `src/core/daemon/DaemonTelemetryController.{h,cpp}`,
  `src/core/session/StationTelemetry.{h,cpp}`,
  `src/core/session/SessionMessages.h`, `src/core/session/StationCapabilities.{h,cpp}`,
  `src/core/session/StationServer.cpp`, `src/core/session/StationClient.cpp`,
  `src/gui/TelemetryHistory.{h,cpp}`, `src/gui/RemoteDiagnosticsDialog.{h,cpp}`,
  `src/gui/RemoteTelemetryController.cpp`
- Test: `tests/tst_station_telemetry.cpp`, `tests/tst_daemon_telemetry.cpp`,
  `tests/tst_station_session.cpp`, `tests/tst_telemetry_history.cpp`,
  `tests/tst_remote_diagnostics.cpp`, `tests/tst_remote_telemetry.cpp`

**Interfaces:**
- Consumes: Task 5's per-receiver snapshot.
- Produces: telemetry section `receivers` (per slice: slice ID, load percent,
  input delay ms, skipped input ms), `stationTelemetryVersion = 3`, session
  minor 11 (`kReceiverLoadSessionProtocolMinor = 11`; `kSessionProtocolMinor`
  becomes 11).

**Acceptance:**
- Version 3 peers exchange the section; older peers receive exactly today's
  telemetry (golden comparison); both decode directions work; absent means not
  measured.
- The Core tab gains "Receiver processing": one series per receiver in percent
  of real time, a reference line at 100% labelled "Cannot keep up", tooltip
  in plain words; without the section it shows the tab's existing
  "does not report" wording pattern.
- The 60 s soak log line adds each receiver's load and input delay.

**Verification:** protocol plus GUI functional checks off-screen.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_station_telemetry tst_daemon_telemetry tst_station_session tst_telemetry_history tst_remote_diagnostics tst_remote_telemetry nereusd -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_station_telemetry|tst_daemon_telemetry|tst_station_session|tst_telemetry_history|tst_remote_diagnostics|tst_remote_telemetry|tst_daemon_media_controller)$' --no-tests=error --output-on-failure
```

**Execution note (advisory):** opus. Requires Task 5.

- [ ] **Step 1:** Section, negotiation, Core wiring with tests.
- [ ] **Step 2:** History, graph, soak line; run the commands; commit.

## Task 7: Step noise reduction back when a receiver cannot keep up

**Requirements:** R-R3-40.

**Files:**
- Create: `src/core/dsp/NnrLoadGovernor.{h,cpp}`
- Modify: `third_party/wdsp/src/nnr.c` and `nnr_compat.h` (lock-free limit
  request applied in `xnnr` at the next block; applied limit in the status),
  `src/core/wdsp_api.h`, `src/core/dsp/NnrAdapter.{h,cpp}`,
  `src/core/RxChannel.{h,cpp}` (clamp; stale cache fix),
  `src/models/SliceModel.{h,cpp}` (runtime property, not persisted),
  `src/models/RadioModel.{h,cpp}` (owns the governor),
  `src/core/session/MirrorPolicy.cpp`, `src/core/session/StationClient.cpp`,
  `src/core/session/SessionCommandDispatcher.*` (a "try again" command, minor
  11), `src/gui/widgets/NnrControls.cpp`, `src/gui/widgets/VfoWidget.cpp`,
  `src/gui/widgets/RxDashboard.cpp`, `docs/attribution/WDSP-PROVENANCE.md`
- Test: a new `tests/tst_nnr_load_governor.cpp`, `tests/tst_wdsp_nnr.cpp`,
  `tests/tst_nnr_controls.cpp`, `tests/tst_nnr_settings.cpp`,
  `tests/tst_nnr_radio_persistence.cpp`, and the station mirror test that
  covers the NNR outbound fields (find it by `nnrActualModelSlot`)

**Interfaces:**
- Consumes: Task 5's per-receiver load; Task 6's minor 11.
- Produces:
  ```c
  /* limit: 0 none, 1 standard model only, 2 off */
  PORT void RequestRXANNRLimit (int channel, int limit); /* never takes csDSP */
  ```
  and a slice runtime property for the limit (outbound mirror field).

**Acceptance:**
- The governor checks every 500 ms. For a receiver running NNR whose load is
  at least 0.90 (`kNnrStepDownLoad`) averaged over 2 s
  (`kNnrStepDownHoldMs`), it lowers one level: premium to standard, standard
  to off; after a step it waits 5 s (`kNnrStepSettleMs`) before judging
  again; it never raises a level by itself. A receiver overloaded with NNR off
  gets no step.
- The request returns immediately even while the worker is busy (proved with
  the test block delay); the worker applies it at its next block under the
  lock it already holds.
- Saved settings are untouched. The limit is runtime only and clears on "Try
  again", on the operator choosing a model or turning NNR off and on, and on a
  Core restart.
- Stale cache fix, red first: today `setActiveNr` re-applies RxChannel's
  cached tuning, which defaults to the standard model and updates only on
  success, so WDSP can run Standard while the slice shows Premium, and
  re-selecting Premium is then ignored by the equality check. After the fix
  the running model always equals the saved choice clamped by the limit, and
  the "actual" readback tells the truth.
- Operator text, local and remote: "Noise reduction is using the Standard
  model. This computer could not keep up with Premium." and "Noise reduction
  was turned off. This computer could not keep up." with a "Try again"
  action in NnrControls; the VFO flag's NNR control and the RX dashboard show
  a small indicator with the same tooltip. Remote GUIs get the field and the
  command at minor 11; older GUIs see nothing new and log no warnings.
- Provenance updated for `nnr.c`/`nnr_compat.h`; both scripts pass.

**Verification:** consequential automatic state change; unit plus integration.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_nnr_load_governor tst_wdsp_nnr tst_wdsp_dsp_turn_taking tst_nnr_controls tst_nnr_settings tst_nnr_radio_persistence tst_station_session nereusd -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_nnr_load_governor|tst_wdsp_nnr|tst_wdsp_dsp_turn_taking|tst_nnr_controls|tst_nnr_settings|tst_nnr_radio_persistence|tst_station_session)$' --repeat until-fail:3 --output-on-failure
python3 scripts/audit-wdsp-headers.py && python3 scripts/verify-thetis-headers.py --all-kinds
```

**Execution note (advisory):** opus. Requires Tasks 5 and 6.

- [ ] **Step 1:** Stale cache regression (red), then the fix.
- [ ] **Step 2:** WDSP limit request and governor with tests.
- [ ] **Step 3:** Mirror, command and GUI text; run the commands; commit.

## Task 8: Run DSP work on the fastest cores

**Requirements:** R-R3-41.

**Files:**
- Create: `src/core/platform/ThreadPlacement.{h,cpp}`,
  `tests/tst_thread_placement.cpp`, `tests/tst_wdsp_thread_hook.cpp`
- Modify: `third_party/wdsp/src/linux_port.{c,h}` (thread-start trampoline in
  `wdsp_beginthread`, `WDSPSetThreadStartHook`, thread names, macOS QoS for
  workers), `src/core/wdsp_api.h`, `src/core/WdspEngine.cpp`
  (register/deregister, `setChannelActive`), `src/core/RxChannel.cpp`
  (activation), `src/core/TxWorkerThread.cpp`, `src/models/RxDspWorker.cpp`,
  `src/core/spectrum/FftEnginePool.cpp`, `src/models/RadioModel.cpp`
  (connection thread), `src/server_main.cpp`, the nereusd configuration
  loader (a `threadPlacement` key, `auto` or `off`, default `auto`),
  `packaging/nereusd.service.in` (`LimitNICE`), `docs/attribution/WDSP-PROVENANCE.md`,
  `CMakeLists.txt`, `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces:
  ```c
  PORT void WDSPSetThreadStartHook (void (*hook)(int kind, int channel));
  ```
  and `ThreadPlacement` with a pure `plan(topology, demand)` and an apply
  layer behind an interface.

**Acceptance:**
- Topology from an injectable root: `cpu/online`, `cpuN/cpu_capacity`,
  `cpufreq/policy*/related_cpus`, SMT siblings; the allowed mask is read once
  before any pinning (respects systemd CPUAffinity/AllowedCPUs).
- `plan()`: fast tier = capacity at least 90% of the maximum; fast CPUs ordered
  by capacity, filling one clock domain before the next; dedicated cores go to
  each active RX worker (lowest channel first), the DSP thread, the TX worker,
  then the TX worker thread while transmitting; housekeeping is the slow tier
  when it has at least 2 CPUs, otherwise every online CPU not reserved.
  Uniform computers reserve the highest-numbered physical cores, never CPU0,
  never an SMT sibling, at most N-2 (N-1 with 2 or 3 CPUs), none with 1.
  Fixture expectations:

  | Fixture | Expected |
  |---|---|
  | RK3588S: 405 x4 (0-3), 1024 (4-5), 982 (6-7); domains 0-3, 4-5, 6-7 | RX0 4, DSP thread 5, RX1 6, TX 7; housekeeping 0-3 |
  | Raspberry Pi 4 or 5: 4 equal CPUs, one domain | RX0 3, DSP thread 2; housekeeping 0-1 |
  | x86, 8 CPUs, no capacity files | RX0 7, DSP thread 6; housekeeping 0-5 |
  | x86 with SMT siblings n and n+4 | never both siblings of one core reserved |
  | 2 CPUs | RX0 1; everything else 0 |
  | 1 CPU; online "0-3,6-7"; restricted allowed mask; missing or garbage files | no-op or uniform rules as appropriate, one log line |

- nereusd only; the GUI is unchanged. At startup, after settings initialise
  and before any other thread exists, the main thread is pinned to the
  housekeeping set so later threads (including libdatachannel's) inherit it.
  WDSP workers are promoted to their dedicated core when their channel becomes
  active and returned when it stops; thread IDs are deregistered at channel
  close. FFTW wisdom planning runs on a fast core.
- Priority: DSP roles get `nice -10` (`kDspNice`) when `RLIMIT_NICE` allows
  it; no real-time policy for WDSP workers; with placement on, the Linux nice
  calls for housekeeping roles are skipped. The unit gains the `LimitNICE`
  value that permits -10 (check systemd's syntax). One plain startup line
  says which cores do signal processing, which do everything else, and whether
  priority was raised or not permitted; the existing refusal warning is not
  duplicated.
- macOS (desktop app): WDSP worker threads get QoS user-interactive and a
  name through the same hook. Windows unchanged.
- Tests: fixture table; apply layer with a recording fake; a Linux-only smoke
  test (skipped elsewhere); WDSP hook test (fires once per worker and flush
  thread with the right channel and kind, on a thread other than the test's).
- Provenance updated for `linux_port.{c,h}`; both scripts pass.

**Verification:** scheduling of every Core thread; unit and fixture tests here,
hardware on the Rock by the controller (pending): `ps -L -o
tid,comm,psr,cls,rtprio,ni,pcpu -p $(pidof nereusd)` shows the RX worker fixed
on cpu 4 at nice -10, spectrum and networking on 0-3, and flat
`se.nr_migrations` while receiving. Expected moves (final review Minor 3,
recorded rather than changed; `transmitKeyMovesOnTheRk3588s`): each key of
one slice moves the RX0 worker from 4 to 0-3, the DSP thread from 5 to 4,
the TX worker from 0-3 to 5 and the TX pump from 0-3 to 6; each unkey
reverses them. No other thread moves. With `thread_placement = off` nothing
moves, and the busy signal processing threads still show nice -10.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_thread_placement tst_wdsp_thread_hook tst_realtime_audio_priority nereusd -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_thread_placement|tst_wdsp_thread_hook|tst_realtime_audio_priority)$' --no-tests=error --output-on-failure
python3 scripts/audit-wdsp-headers.py && python3 scripts/verify-thetis-headers.py --all-kinds
```

**Execution note (advisory):** opus.

- [ ] **Step 1:** Topology and plan with fixtures.
- [ ] **Step 2:** WDSP hook, registry, apply, startup wiring, unit; run the
  commands; commit.

## Task 9: Measure the NNR model's cost (no shipped change)

**Requirements:** R-R3-40.

**Files:**
- Create: `tools/nereus-nnr-bench.c`
- Modify: `CMakeLists.txt` (targets `nereus-nnr-bench` and
  `nereus-nnr-bench-relaxed`, both `EXCLUDE_FROM_ALL`; the relaxed one links an
  OBJECT library of `third_party/wdsp/src/nnet.c` compiled with
  `-fno-math-errno -fassociative-math -fno-signed-zeros -fno-trapping-math`
  ahead of `wdsp_static`), and the Rock build script
  `/Users/j.j.boyd/.config/nereus/work/rock-docker/container/build-in-container.sh`
  (build both after nereusd and stage them in `$stage/usr/local/bin`; the
  script is outside the repository and is not committed; report the change)

**Interfaces:** none (a standalone tool).

**Acceptance:**
- `nereus-nnr-bench [--seconds N] [--slot 0|1] [--cpu N] [--dump FILE]
  [--compare A B] [--flush-to-zero]` forces the built-in models
  (`SetNNRModelPathSlot(k, "")`), creates NNR as `RXA.c:339-351` does with a
  4096-sample buffer, feeds deterministic synthetic voice plus noise, and
  prints per slot: frames, microseconds per 16 ms frame (mean, p99, max),
  share of one core, and FFTW planning time separately.
- `--compare` prints the maximum absolute difference and the SNR in dB
  between two dumps; `--flush-to-zero` sets flush-to-zero for the benchmark
  thread (AArch64 FPCR.FZ, x86 MXCSR FTZ and DAZ).
- A local run on this Mac is in the report: both builds, both models, and the
  comparison.
- Nothing in the Core's build or package changes.

**Verification:** measurement tool; the numbers are the deliverable.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target nereus-nnr-bench nereus-nnr-bench-relaxed -j6
```
Hardware (controller, pending): both tools on the Rock, pinned to a fast and a
slow core, results recorded in the ledger.

**Execution note (advisory):** opus.

- [ ] **Step 1:** Tool, targets, local numbers; staging; commit.
