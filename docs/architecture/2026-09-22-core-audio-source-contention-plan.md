# Core audio source contention fix implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. Requirements and
> acceptance cases are binding; one whole-branch review at the end of the batch.

**Goal:** the Core stops discarding a whole 40 ms packet of mixed audio every 2 to 4
seconds when the DSP callback loses a lock race with the session-thread consumer.

**Architecture:** the single producer (the master mixer tap on the DSP thread)
assembles each 1920-frame packet in producer-owned state and takes the bridge lock
only to hand over a finished packet; if the hand-over lock is busy, the finished
packet is kept and retried on the next callback instead of being discarded. Consumer
reads (`isRunning`) become atomic. Drops are counted by cause.

**Tech stack:** C++20, Qt6, `DaemonAudioSource` bridge, `DaemonAudioSender`,
`DaemonMediaController`, Qt Test with threads.

**Spec:** R3 plan R-R3-06 (one 48 kHz stereo master captured after the mixer; no
network or codec work on the DSP callback) and R-R3-07 (audio loss handled
continuously). Evidence: read-only investigation of 2026-09-22 (below) and the
operator's live Core logs.

## Global Constraints

- Work only in `/Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR`,
  branch `codex/integrate-r2-main`, build directory `build-integration`.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`),
  never `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash characters.
- The DSP callback path must never block: no blocking lock, no allocation, no logging,
  no Qt signal emission on the producer path. `try_lock` or lock-free only. The
  engine tap gate that keeps the producer out during `start()`/`stop()` stays.
- Keep the packet grid, RTP timestamp and sequence semantics exactly: timestamps
  are base + sample position (a genuinely lost packet still advances the timestamp
  by 1920 and the sequence stays contiguous); the ring stays bounded (4 blocks).
- No change to Opus settings, mixer cadence, block sizes or thread priorities.
- NereusSDR-original code; no Thetis port involved. Keep existing file headers.
- Tests: build exact targets, `ctest -R` with `--no-tests=error`; record load
  averages; no unfiltered ctest. Hardware is off limits to implementers.

## What already exists

Investigation (read-only, 2026-09-22), paths in the checkout:
- `src/core/session/media/DaemonAudioSource.cpp`: `Bridge::m_dropCount`. Producer
  path: `try_to_lock` fails because the consumer holds `m_mutex` (:105-111), sets
  `m_discontinuity` and returns (dominant path); ring full with 4 blocks drops the
  newest completed block (:163-168). The frame position is reserved before the lock
  (:101-103); after a miss the next accepted callback discards the partly assembled
  packet and skips to the next 1920-frame boundary (:127-148, :190-194), so one lost
  64-frame callback costs one whole 40 ms packet. `start()` resets without counting
  (:20-27); stopped returns without counting (:113).
- Producer cadence: ANAN-G2 Protocol 2 at 192 kHz delivers 64-frame WDSP callbacks
  (`RadioModel.cpp:10207`): about 750 `try_lock` per second, 30 per packet, one
  producer thread (`MasterMixer.cpp:299`; RADE speech is queued onto it,
  `RadioModel.cpp:6721`).
- Consumer: `DaemonAudioSender` drains every 10 ms (max 4 blocks); per packet cycle
  it takes `m_mutex` about 6 times (`isRunning` at drain top :90, `takeBlock` twice
  :44/:57, `isRunning` in the packetReady lambda `DaemonMediaController.cpp:1713`,
  `isRunning` after emit :122, a final empty `takeBlock`), and copies 15,360 bytes
  under the lock in `takeBlock` (:57-66).
- Field evidence: 1.0-1.5 % of packets lost (for example 480 of 42,814 in one
  context); `lastTimestamp/1920 - lastSequence` grows exactly by `sourceDropEvents`;
  the GUI conceals each hole with 40 ms of Opus PLC.
- Tests: `tests/tst_daemon_audio_source.cpp` pins the one-miss-one-packet behavior in
  `partialIngressLossResumesOnOriginalPacketGrid` (:202, via `dropIngressForTest(64)`)
  and the ring-full rule in `boundedQueueDropsNewestCompletedBlock` (:233). No test
  runs a real producer thread against a consumer thread. MasterMixer has a
  `m_drainAdmissionHookForTest` pattern for holding a thread inside a critical
  section.

## Task 1: Hand over finished packets without losing audio to lock contention

**Requirements:** R-R3-06, R-R3-07.

**Files:**
- Modify: `src/core/session/media/DaemonAudioSource.h`, `src/core/session/media/DaemonAudioSource.cpp`
- Modify (only if needed for the counters or atomic running flag): `src/core/session/media/DaemonAudioSender.{h,cpp}`, `src/core/session/media/DaemonMediaController.cpp` (diagnostics line fields)
- Test: `tests/tst_daemon_audio_source.cpp` (and `tst_daemon_audio_sender` / `tst_daemon_audio_session` if counters change)

**Interfaces:**
- Consumes: nothing new.
- Produces: `DaemonAudioSourceTelemetry` gains separate counters for contention
  retries (a hand-over deferred because the lock was busy), contention losses (audio
  actually lost because a finished packet could not be handed over before the next
  one completed), ring-full drops, and invalid-ingress drops; the existing total
  `dropCount`/`sourceDropEvents` remains the sum of actual losses so existing
  consumers keep their meaning. The periodic Core diagnostics line prints the split.

**Acceptance:**
- The producer assembles the current packet in producer-owned storage without taking
  the bridge lock per callback. When a packet completes it tries the lock once
  (non-blocking); on success it moves the packet into the ring; on failure it keeps
  the completed packet pending and retries on each following callback. Audio is lost
  only if a second packet completes while the first is still pending (count it as a
  contention loss and keep the packet-grid/timestamp semantics), or when the ring is
  full (existing rule).
- `isRunning()` and any other hot-path state reads the consumer does on every drain
  are atomic loads, not mutex acquisitions. The copy out in `takeBlock` stays bounded
  and does not happen while the producer must wait (the producer never waits).
- New test `consumerCriticalSectionDoesNotLoseIngress`: using a test-only hook inside
  `takeBlock`'s locked section (MasterMixer `m_drainAdmissionHookForTest` pattern),
  complete block 0 with a frame-index ramp, hold a consumer thread inside the hook,
  feed several 64-frame callbacks, release it, finish two more packets; assert zero
  losses, positions 0, 1920, 3840 and a continuous ramp across every boundary. This
  test fails on the current tree.
- A paced two-thread soak: a real producer thread at 64 frames per 1.333 ms against a
  tight `takeBlock` consumer loop for 10 s (or a shorter configurable duration in CI
  with the same assertion) asserts zero contention losses and a continuous ramp.
- `partialIngressLossResumesOnOriginalPacketGrid` and
  `boundedQueueDropsNewestCompletedBlock` keep their meaning: an explicitly injected
  ingress loss and a genuinely full ring still drop exactly as today (update only the
  counter name they read if the split requires it, never the behavior they pin).
- Session and sender tests pass unchanged; the periodic diagnostics line still
  contains `sourceDropEvents=` with the summed losses, plus the split counters.

**Verification:** real-time path, consequential: the contention regression fails
first on the current tree, then passes; the soak runs clean at the machine's load.
```sh
cmake --build /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration --target tst_daemon_audio_source tst_daemon_audio_sender tst_daemon_audio_session tst_daemon_media_controller tst_remote_audio_session -j6
ctest --test-dir /Users/j.j.boyd/.codex/worktrees/nereus-r2-integration/NereusSDR/build-integration -R '^(tst_daemon_audio_source|tst_daemon_audio_sender|tst_daemon_audio_session|tst_daemon_media_controller|tst_remote_audio_session)$' --no-tests=error --output-on-failure
```
Hardware (pending, controller/operator): after install on the Rock, the Core's
periodic line shows contention losses near zero over 10 minutes of receive, and the
GUI's "Gaps filled" stays flat.

**Execution note (advisory):** opus (real-time audio path, threads).

- [ ] **Step 1:** Add the hook and the contention regression; confirm it fails.
- [ ] **Step 2:** Move assembly to producer-owned state with deferred hand-over,
  atomic running flag and split counters; add the soak; run the commands; commit.
