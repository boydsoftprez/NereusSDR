# RX Stream Switch and Re-rate in One Step Implementation Plan

> **Execution:** run with `crew` (`/crew <this file>`) under `cost-aware-execution`.
> Requirements and acceptance cases are binding; test order and review effort follow
> the risk-based policy; UI evidence follows `ui-verification`. No review between
> tasks; one whole-branch review at the end.

**Goal:** when a slice moves to a stream at another sample rate, the DSP worker's new
stream binding and the slice channel's new WDSP input rate and block size take effect in
one quiesced step, so no chunk from the new stream ever reaches the channel at the old
geometry (the 27 ms gap PR 630 found on the Zephyrus G2E Core).

**Architecture:** `republishStreamBindings` stops posting `RxDspWorker::setStreamSlices`
straight to the worker. It stages the binding; `applyStreamDspGeometry` delivers every
staged binding. When the geometry is out of step, the bindings go inside the existing
receive-lane barrier, after the worker is quiesced and before the feed is reconnected,
next to `setStreamInputChunk` and `applyRxChannelRateOnLane`. When it is in step, they go
as a plain receive-lane job, so they can never overtake an earlier barrier. PR 630's
input-size guard in `RxChannel::processIq` stays as a safety net.

**Tech Stack:** C++20, Qt6 (queued and blocking-queued invocations, QtTest),
`DspControlThread` (the receive lane, one FIFO thread), `RxDspWorker` on its own thread,
WDSP `fexchange2` via `RxChannel`.

**Spec:** no separate spec. JJ's ruling of 2026-10-25, relayed by the manager session:
"yes on sample rate, in this release not later" (release 2026.10.2), on "make the stream
switch and the rate change one step, as Thetis does, so there is no 27 ms gap (a
threading change), as its own PR". PR 630 (merged, `e8e805e57`) is the background.

## Why, from Thetis (v2.10.3.15)

Read for this plan. Thetis never moves a DSP channel between streams: a receive
channel is `chid(stream, subrx)` (`ChannelMaster/cmsetup.c:176`), fixed for life. What it
does change is a stream's rate, and it changes everything that geometry touches under
the one lock its receive loop holds:

- `xcmaster` takes `pcm->update[stream]` around the whole receive block, including
  `fexchange0 (chid (stream, j), ...)` (`ChannelMaster/cmaster.c:340-404`, the call at `:366`).
- `SetXcmInrate` takes the same `pcm->update[in_id]` (`cmaster.c:457`, released at `:507`), then sets
  `xcm_insize = getbuffsize (rate)` (`:461`), the ring's drain size
  `SetCMRingOutsize` (`:462`), and for every sub-receiver
  `SetInputSamplerate` / `SetInputBuffsize` (`:473-476`, the calls at `:474-475`) before it lets go.
- `SetCMRingOutsize` itself shuts the inbound gate, parks the CM thread and flushes the
  ring before it changes the outsize (`ChannelMaster/cmbuffs.c:170-187`).
- The operator path also stops the channels and the DDCs around it
  (`Console/setup.cs:7042-7090`: `SetChannelState(..., 0, 1)`, `EnableRx(.., 0)`,
  `Thread.Sleep(20)`, then the rate, then the DDCs and channels back on).

So in Thetis the drain size, the channel's input geometry and the set of channels the
stream feeds are never seen apart by the loop that runs `fexchange`. NereusSDR's
equivalent of "which channels a stream feeds" is `RxDspWorker::m_streamSlices`, and it
is the one part that today changes outside the quiesce. CLAUDE.md forbids a mutex in the
audio path, so the quiesce the barrier already does (disconnect the feed, blocking
`resetAccumulator` on the worker's thread, reconnect) plays the part of
`pcm->update[stream]`; this plan moves the binding inside it.

## Global Constraints

- Read `CLAUDE.md`, `CONTRIBUTING.md` and `docs/development/fast-test-loop.md` first.
  They bind every task.
- Source-first: this is NereusSDR plumbing that keeps a Thetis invariant, not a port of
  new upstream logic. Cite Thetis where a comment states the invariant, with
  `[v2.10.3.15]` (`git -C ../Thetis describe --tags` reads `v2.10.3.15`). Use only the
  cites in "Why, from Thetis" above; do not invent others. No new file headers or
  provenance rows are needed unless a new file is created; a new test file takes the
  header style of `tests/tst_rx_rate_change_non_finite.cpp`.
- Every `//` comment inside existing ported logic that is moved stays verbatim
  (`scripts/verify-inline-tag-preservation.py` runs in pre-commit).
- Every touched source or test file gets a "Modification history (NereusSDR)" line in
  that file's existing style, for example
  `2026-10-25 - stream bindings reach the DSP worker inside the geometry barrier. J.J. Boyd (KG4VCF), AI-assisted via Claude Code.`
- C++ style: no `goto`, no raw `new`/`delete`, `constexpr` constants named
  `kPascalCase`, braces on all control flow, `m_camelCase` members, `auto` only when the
  type is obvious, `qCWarning(lcCategory)` for errors, no exceptions.
- Threads: cross-thread traffic stays auto-queued signals or `QMetaObject::invokeMethod`
  as today. Never hold a mutex in the audio or DSP path. Never a blocking-queued call
  from the event loop to the DSP worker when a receive lane exists (R-R3-39: the lane
  waits, the event loop does not).
- A lane job never touches a destroyed `RxDspWorker`: it reaches the worker only through
  the same check `runOnDspWorkerFromLane` uses (`m_rxWorkerTarget`, under its mutex,
  worker pointer still the attached one), or posts inside the existing barrier exactly
  as its `setStreamInputChunk` posts do today. Confirm the teardown order
  (`teardownConnection`: `abandonSampleRateChange`, `drainReceiveLane`, worker
  destruction, `releaseStreamBindings`) before relying on it.
- Don't remove code you didn't add. PR 630's guard (`RxChannel::wdspInputSize`,
  `takeInputSizeMismatch`, the "I/Q chunk(s) silenced" log line and its info / warning
  threshold) stays as it is; only comments that describe the gap as expected are
  updated to say it is now a safety net.
- No user-facing default, DSP parameter, UI or signal-routing change beyond the order
  described here.
- Commits are GPG-signed (`git commit -S`); never `--no-gpg-sign`. No `Co-Authored-By`
  trailer. If signing fails, stop and report BLOCKED. Never rebase or force-push.
- American spelling and no em dashes in all new text.
- **Build lanes: at most two compiling agents on this Mac.** Follow
  `~/.config/nereus/work/pr-review-20261021/build-lane-lock.md` exactly: write and
  self-review all code first without building; take `build-lane.lock` or
  `build-lane-2.lock` by `mkdir`; wait for one with a background Monitor or a single
  until-loop polling every 30 s, never chained foreground sleeps; `nice -n 20`, `-j 6`;
  release the lock the moment the last build or test command ends.
- `df -h /System/Volumes/Data` before any configure or build; stop and report under
  40 GB free.
- One build folder, reused for every cycle: `build-rate-step/` at the worktree root,
  configured once with the network denied (the configure command below). The controller
  deletes it at hand-off.

  ```bash
  DEPS=~/.config/nereus/work/pr-merge-review/review-deps
  ARGS=()
  for d in "$DEPS"/*-src; do
    n=$(basename "$d" -src | tr '[:lower:]' '[:upper:]')
    ARGS+=("-DFETCHCONTENT_SOURCE_DIR_${n}=${d}")
  done
  nice -n 20 sandbox-exec -p '(version 1)(allow default)(deny network*)' \
    cmake -B build-rate-step -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DNEREUS_BUILD_TESTS=ON \
      -DENABLE_DFNR=OFF -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
      -DOPUS_URL=/Users/j.j.boyd/.config/nereus/work/deps-cache/nereus-core-pins-0368ff16/opus/opus-940d4e5-with-model.zip \
      -DOPUS_URL_HASH=SHA256=740a5220180aee546f97030118517eedafa7956fdfbc8e8432d91f2d20aac4b7 \
      "${ARGS[@]}"
  nice -n 20 sandbox-exec -p '(version 1)(allow default)(deny network*)' \
    cmake --build build-rate-step -j 6 --target <targets>
  ```

  Grep the configure output for "Performing download step"; it must not appear.
- Build single test targets, never the whole suite. Run each with
  `ctest --test-dir build-rate-step -R '^<name>$' --output-on-failure`, one binary at a
  time. A direct binary run sets `HOME`, `CFFIXED_USER_HOME` and `NEREUS_TEST_HOME` to
  one scratch folder, `QT_QPA_PLATFORM=offscreen`, and passes `-maxwarnings 0`.
- A failed test is never ignored or retried away. A failure under load is a finding:
  record the load average and the assertion.
- Never touch a radio, a Core or a bench machine. The live check is JJ's.

## What already exists

- `src/models/RadioModel.cpp`
  - `republishStreamBindings(int)` (:13600): posts `setStreamSlices` queued to
    `m_dspWorker` at once, then emits `streamBindingsChanged`.
  - `republishAllStreamBindings()` (:13353), called from the retroactive push after the
    worker is built (:22053).
  - `applyStreamDspGeometry()` (:13623): early-out when in step; with `m_rxLane`, one
    `postBarrier` that disconnects `iqDataForReceiverStamped` from
    `RxDspWorker::processStampedIqBatch`, runs `resetAccumulator` through
    `runOnDspWorkerFromLane`, queues `setStreamInputChunk` per stream, calls
    `WdspEngine::applyRxChannelRateOnLane` per channel, and reconnects; without a lane,
    the same steps synchronously on the event loop. Returns with
    `m_streamGeometryPending = true` while `m_sampleRateChangeInFlight`.
  - Callers that republish bindings: `republishAllStreamBindings`; the rate-change
    re-placement loop (:14562, followed by `applyStreamDspGeometry` at :14576);
    `bindSliceToStream` (:15234 / :15237, followed by `applyStreamDspGeometry` at
    :15251); slice removal (:17027, not followed by it). The implementer greps for every
    caller again; line numbers move.
  - `finishSampleRateChange` (:32919) calls `applyStreamDspGeometry` when
    `m_streamGeometryPending`; `abandonSampleRateChange` (:32971) clears it; teardown
    (:28657, :28937 `releaseStreamBindings`).
  - `runOnDspWorkerFromLane` (:17476), `dspWorkerThreadRunning`, `m_rxWorkerTarget`.
- `src/models/RadioModel.h`: declarations (:7599 `republishStreamBindings`, :7662
  `applyStreamDspGeometry` with its long doc comment, :7949 `m_streamGeometryPending`),
  test seams `attachDspWorkerOnThreadForTest`, `waitForReceiveLaneForTest`,
  `configureStreamPoolForTest`, `receiveLane()` (:4889).
- `src/models/RxDspWorker.{h,cpp}`: `setStreamSlices` (:1387), `setStreamInputChunk`
  (:1399), `resetAccumulator` (:1454), the comment at :1007 pointing at the ordering.
- `src/core/DspControlThread.{h,cpp}`: `post`, `postKeyed`, `postBarrier`; one thread,
  FIFO; keyed jobs never cross a barrier.
- `src/core/RxChannel.{h,cpp}`: PR 630's guard in `processIq` (:3015 comment block),
  `wdspInputSize()`, `takeInputSizeMismatch(InputSizeMismatch&)`.
- `src/core/WdspEngine.cpp`: `applyRateOnLane` (:1321) sets `wdspInputSize` after
  `SetInputBuffsize`.
- Tests: `tests/tst_stream_pool_binding.cpp` (worker on its own thread with a lane at
  :1540-1590, including a held worker), `tests/tst_rx_rate_change_non_finite.cpp`,
  `tests/tst_set_sample_rate_live.cpp`, `tests/tst_rx_dsp_worker_multi_slice.cpp`,
  `tests/tst_dsp_control_receive.cpp`, `tests/tst_rx_channel_stop_feed.cpp`.

## File Structure

| Modify | Change |
| --- | --- |
| `src/models/RadioModel.h` | A staged-bindings member (stream index to slice list, main thread only); updated doc comments on `republishStreamBindings` and `applyStreamDspGeometry`. |
| `src/models/RadioModel.cpp` | `republishStreamBindings` stages; `applyStreamDspGeometry` delivers (in the barrier, as a lane job, or direct without a lane); callers that republish without a following `applyStreamDspGeometry` get one; `releaseStreamBindings` drops staged entries; the 2026-10-23 comment in `bindSliceToStream` rewritten. |
| `src/models/RxDspWorker.cpp` | Comment at :1007 updated to the new ordering. |
| `src/core/RxChannel.{h,cpp}` | Comments only: the guard is a safety net; a silenced chunk now means the ordering broke. |
| `tests/tst_stream_pool_binding.cpp` (or a new `tests/tst_rx_rate_switch_one_step.cpp` registered in `tests/CMakeLists.txt`, implementer's choice by fit) | The acceptance tests below. |

## Task 1: Stream bindings reach the worker inside the geometry step

**Requirements:** JJ's 2026-10-25 ruling above. The Thetis invariant in "Why, from
Thetis": the loop that runs a channel's exchange never sees the stream's channel set,
drain size and channel input geometry apart.

**Files:**
- Modify: `src/models/RadioModel.h`, `src/models/RadioModel.cpp`,
  `src/models/RxDspWorker.cpp` (comment), `src/core/RxChannel.h` / `.cpp` (comments)
- Test: `tests/tst_stream_pool_binding.cpp` or a new
  `tests/tst_rx_rate_switch_one_step.cpp` plus `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: nothing new.
- Produces: no new public API. `republishStreamBindings` keeps its signature and still
  emits `streamBindingsChanged(streamIndex, bound)` at once on the event loop. New
  private state: staged bindings, `QMap<int, QVector<int>>` or equivalent, main thread
  only. If a test needs to see the worker's bindings, a read-only test seam on
  `RxDspWorker` (for example `QVector<int> streamSlicesForTest(int) const`, DSP thread
  only) may be added.

**Acceptance:**
- Staging. `republishStreamBindings(st)` records `slicesOnStream(st)` as the staged
  binding for `st` (a later call for the same stream replaces it) only when
  `m_dspWorker` is set, as today's guard does (defect F1: never claim a delivery that
  went nowhere). It no longer posts to the worker itself.
- Delivery, out of step. When `applyStreamDspGeometry` posts its barrier, every staged
  binding moves into the barrier and is queued to the worker
  (`setStreamSlices`) after the quiesce (`resetAccumulator` through
  `runOnDspWorkerFromLane`) and before the reconnect, in the same place as the
  `setStreamInputChunk` posts. The staged set is then empty. The first batch the worker
  processes after the reconnect sees the new binding, the new drain size and the
  channel's new `wdspInputSize()` together. The no-lane synchronous path does the same:
  bindings queued after its blocking `resetAccumulator`, before its reconnect.
- Delivery, in step. When `applyStreamDspGeometry` takes its early-out (nothing to
  re-rate), staged bindings are still delivered: with a lane, as one `post` job on the
  receive lane that queues `setStreamSlices` to the worker, so it lands after any barrier
  already queued; without a lane, queued straight to the worker as today.
- Delivery, rate change in flight. While `m_sampleRateChangeInFlight`, staged bindings
  stay staged and `m_streamGeometryPending` is set; `finishSampleRateChange`'s call to
  `applyStreamDspGeometry` delivers them. `abandonSampleRateChange` and
  `releaseStreamBindings` (teardown) drop them.
- No lost binding. Every caller of `republishStreamBindings` reaches
  `applyStreamDspGeometry` before it returns to the event loop: add the call where it is
  missing (at least `republishAllStreamBindings`' callers or its own end, and slice
  removal after the freed stream's republish). A slice removed from a stream reaches the
  worker's binding.
- Worker lifetime. No lane job dereferences or posts to a worker that is no longer the
  attached one (Global Constraints).
- Tests, each red at `origin/main` (`3d70aa3d4`) where marked and green with the change;
  record the red evidence (assertion and values) in the report:
  1. **Rate change while chunks flow (red at main).** Two streams at different rates
     (stream 0 at 1536 kHz, block 2048; stream 1 at 192 kHz, block 256, via
     `configureStreamPoolForTest` and the codec path the existing pool tests use), the
     DSP worker on its own thread, a receive lane, real `RxChannel`s, chunks flowing on
     both streams. Hold the receive lane with a job that waits on a semaphore (posted
     through `model.receiveLane()`), move slice B from stream 0 to stream 1, keep feeding
     stream 1 batches for several blocks, then release the lane and wait for it. Assert:
     slice B's channel's `takeInputSizeMismatch` reports 0 silenced chunks, no
     non-finite output from B's channel, and B is fed stream 1 chunks after the lane
     releases (its channel processed at least one chunk of 256 at
     `wdspInputSize() == 256`). At main the guard records the window's 256-sample chunks
     against 2048.
  2. **An in-step republish cannot overtake the barrier (red at main or by
     construction; say which).** With the lane held as above, move slice B onto stream 1
     (out of step, so the binding rides the barrier), then put slice C onto stream 1 in
     step. After the lane releases, the worker's stream 1 binding is {B, C}, never {B}
     left last.
  3. **Removal reaches the worker.** Remove a slice from a stream it shared; after the
     lane drains, the worker's binding for that stream no longer lists it.
  4. **Single-rate radio unchanged.** A bind on a radio where every stream runs at one
     rate posts no barrier from `applyStreamDspGeometry` (the early-out holds) and the
     binding still reaches the worker.
- Existing tests stay green: `tst_stream_pool_binding`, `tst_rx_rate_change_non_finite`,
  `tst_set_sample_rate_live`, `tst_rx_dsp_worker_multi_slice`,
  `tst_dsp_control_receive`, `tst_rx_channel_stop_feed`. A test that asserted the old
  immediate delivery is updated only if its assertion was about timing this plan
  changes, with the reason in the report; never weakened to pass.

**Verification:** high tier (core RX path and a threading-order change, the repository's
own safety boundary). Regression tests 1 and 2 written first and run red at
`origin/main` before the fix, in the same build folder (check out the test on a scratch
commit or run it against the unfixed tree, then apply the fix). Then the listed binaries
one at a time:

```bash
nice -n 20 sandbox-exec -p '(version 1)(allow default)(deny network*)' \
  cmake --build build-rate-step -j 6 --target tst_stream_pool_binding tst_rx_rate_change_non_finite \
    tst_set_sample_rate_live tst_rx_dsp_worker_multi_slice tst_dsp_control_receive tst_rx_channel_stop_feed
ctest --test-dir build-rate-step -R '^tst_stream_pool_binding$' --output-on-failure
```

(and each of the others, plus `tst_rx_rate_switch_one_step` if created). Also
`python3 scripts/verify-inline-tag-preservation.py` and the pre-commit hook. Hardware
evidence is **pending** and stays pending: a live slice split onto a slower stream on a
P2 radio, checked by JJ on the bench (no "I/Q chunk(s) silenced" line, no non-finite
count, S-meter steady).

**Execution note (advisory):** opus. Touches the core RX path (I/Q to WDSP to audio) and
thread ordering: a candidate for an earlier independent review, named to JJ through the
manager, not added. No networking, config apply, secrets or device reachability. No
prerequisites; one task, not parallel.

- [ ] **Step 1:** Re-read the code paths above at the current line numbers and the
  teardown order; list every `republishStreamBindings` caller in the report.
- [ ] **Step 2:** Write tests 1 and 2 against the unchanged tree; take a build lane;
  build and run them; record red. Release the lane.
- [ ] **Step 3:** Implement staging and delivery as in Acceptance; update the comments
  (RadioModel.h doc on `applyStreamDspGeometry`, the 2026-10-23 comment in
  `bindSliceToStream`, RxDspWorker.cpp:1007, RxChannel's guard comments) so none still
  says the binding reaches the worker ahead of the re-rate; add tests 3 and 4.
- [ ] **Step 4:** Self-review the diff, then take a build lane; build and run every
  listed binary one at a time; all green. Release the lane.
- [ ] **Step 5:** Modification-history lines, inline-tag script, one GPG-signed commit
  (or a small series), report with the red and green evidence and the caller list.
