# R3 receiver load reading hotfix implementation plan

> **Execution:** run with `crew` under `cost-aware-execution`. One task. Runs in the
> hotfix worktree `/Users/j.j.boyd/.codex/worktrees/nereus-hotfix/NereusSDR` (branch
> `codex/hotfix-load`, build directory `build-hotfix`), based on the integration head
> `59c94b25`; the controller carries it into the integration branch afterwards.

**Goal:** a receiver's processing load reads what the processor really spends on it,
so noise reduction stops switching itself off on a Core that has plenty of headroom.
On the Rock 5C the Core reports about 0.9 while the receive worker uses about 27% of a
core with noise reduction on, and the noise reduction step-back trips within seconds.

**Architecture:** the sampler today divides the time a block has already run by one
block period whenever a sample lands inside a long block. Frame-based noise reduction
makes one long block every 12 blocks at the default buffer size (a 16 kHz hop of 256
samples is 768 samples at 48 kHz, against a 64-sample block), so about 30% of samples
read between 1.0 and 4.8. The fix measures busy time over wall time: the WDSP load call
also returns the time of the read, and the sampler divides the change in busy time
(finished blocks plus the running block so far) by the change in read time. Late
blocks stay a diagnostic and stop counting as overload in the display governor.

**Tech stack:** C (vendored WDSP), C++20, Qt Test (off-screen).

**Spec:** the investigation of 2026-09-23 (root cause with file:line, reproduction
table, proposed fix, failing test design):
`/Users/j.j.boyd/.config/nereus/work/load-debug/FINDINGS.md`; its harness is
`/Users/j.j.boyd/.config/nereus/work/load-debug/harness/loadrepro.c` and its run logs
are in `/Users/j.j.boyd/.config/nereus/work/load-debug/logs/`. R3 plan requirements
R-R3-40 (the Core measures each receiver's load; a model that cannot keep up steps
down; the cost is measured, not assumed) and R-R3-37 (CPU-adaptive display limits).

## Global Constraints

- Work only in the hotfix worktree and its build directory; never checkout, rebase,
  merge, reset or push; never touch the integration, lane B or checkpoint worktrees.
- Commits: GPG-signed with hooks (`NEREUS_THETIS_DIR=/Users/j.j.boyd/Thetis`), never
  `--no-gpg-sign` or `--no-verify`, no `Co-Authored-By`, no em-dash characters. Stage
  explicit paths only. Every commit names its R-R3 IDs.
- Vendored WDSP files carry a modification history in their headers; every addition
  to `third_party/wdsp/src/dsplock.{c,h}` and `src/core/wdsp_api.h` gets its line there,
  in the form the existing 2026-09-23 entries use.
- Behaviour that stays: the step-back thresholds and timings in `NnrLoadGovernor`, the
  display governor's thresholds, the input delay check, the telemetry field names and
  the session wire format. Only the load's definition and the late-block rule change.
- No audio devices, no hardware. Tests: prefix every ctest and test binary with
  `QT_QPA_PLATFORM=offscreen`. Build exact targets, then
  `ctest -R '^(...)$' --no-tests=error --output-on-failure`. No unfiltered suite. Never
  build the `NereusSDR` target. The Mac is shared with a live operator test: build
  with -j6 at most.

## Task 1: Measure busy time over wall time

**Requirements:** R-R3-40, R-R3-37.

**Files:**
- Modify: `third_party/wdsp/src/dsplock.{c,h}` (`WdspChannelLoad` gains `readNs`,
  stamped by `GetChannelDspLoad` from the same clock read that gives
  `currentBlockNs`; new test-only `WDSPSetTestPeriodicDelayUs(int channel, int
  microseconds, int everyBlocks)` beside the existing test delays),
  `src/core/wdsp_api.h` (the mirrored struct field and the new declaration),
  `src/core/RxChannel.{h,cpp}` (carry `readNs`), `src/models/RadioModel.cpp` (pass it
  to the sampler), `src/models/ReceiverDspLoadSampler.{h,cpp}` (load = change in
  `busyNs + currentBlockNs` over change in `readNs`; remove the "time so far over one
  period" rule; the header's definition says so), `src/models/NnrLoadGovernor.h` (its
  definition of the load), `src/core/session/media/DisplayLoadGovernor.{h,cpp}` (late
  blocks no longer block the calm test), `docs/architecture/2026-09-23-r3-dsp-overload-plan.md`
  (the Task 5 interface lines that define the old ratio)
- Test: `tests/tst_receiver_dsp_load_sampler.cpp` (reverse
  `aLateBlockInProgressRaisesTheLoad`, which asserts the defect; the stuck-block case
  expects at least 0.9 instead of above 1.0), `tests/tst_display_load_governor.cpp`
  (late blocks with a calm load let the display recover), `tests/tst_nnr_load_governor.cpp`
  (unchanged thresholds still pass), and a new real-channel test
  `tests/tst_receiver_dsp_load_frames.cpp` registered in `tests/CMakeLists.txt` with the
  labels the neighbouring DSP load tests use

**Interfaces:**
- Produces: `WdspChannelLoad::readNs`; `WDSPSetTestPeriodicDelayUs`; the sampler's new
  load definition (busy time over wall time between two reads). Consumers that already
  read the load (the step-back governor, the display governor inputs, telemetry) are
  unchanged in form.

**Acceptance:**
- The new test runs a real receive channel with the Core's settings (buffer 64, the
  Core's rates), fed in real time, sampled every 500 ms for 10 s through the real
  `ReceiverDspLoadSampler` and `NnrLoadGovernor`, with the worker's CPU clock (captured
  through the thread-start hook) as the reference:
  - 9 ms of delay every 12th block (frame-like): every sample is under 0.75 and within
    0.1 of the CPU share, and the governor never steps back. This fails before the fix.
  - 900 us every block: within 10% of its CPU share.
  - Two block periods of work (2666 us) every block (real overload; was 1400 us until
    2026-09-29, too close to one block period to keep the worker fed on a busy machine):
    reads at least 0.95 and the governor steps back.
  - One 1.5 s block: reads at least 0.95.
- A worker stuck in one block reads about 1.0, not more.
- Late blocks during a calm load no longer hold the display at a reduced setting.
- Uniform loads read as before (the existing sampler and governor cases that do not
  assert the defect still pass unchanged).

**Verification:** build `tst_receiver_dsp_load_sampler`, `tst_nnr_load_governor`,
`tst_display_load_governor`, `tst_receiver_dsp_load_frames` and any other test whose
sources include `dsplock.h`, `wdsp_api.h`, `RxChannel.h` or `ReceiverDspLoadSampler.h`
(grep `tests/CMakeLists.txt` and `tests/`), then run them by exact name. The new test
is real time: run it three times to show it is steady. Hardware (pending, operator
checkpoint): the Rock with noise reduction on no longer steps back.

**Execution note (advisory):** opus.

- [ ] **Step 1:** The failing real-channel test (fails at about 2 s today); the
  `readNs` field and busy-over-wall load; the late-block rule; the definitions; commit.
