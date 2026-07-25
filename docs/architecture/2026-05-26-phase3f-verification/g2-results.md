# Phase 3F Bench Results: ANAN-G2 (Saturn)

Sub-Epic H Task 2 deliverable. Records what the G2 bench sessions actually
found, so findings survive between sessions instead of living only in commit
subjects.

**SKU:** ANAN-G2 / Saturn (2 ADC, 7 DDC, user DDC2-6, maxSlices 5)
**Host:** macOS (Apple Silicon)
**Branch:** `feature/phase3f-sub-epic-a-foundation`

## Session log

| Session | Date | Build at | Outcome |
|---|---|---|---|
| 1 | 2026-06-03 | `d564a772` | Bugs 1-3 found + fixed same session |
| 2 | 2026-07-24 | `f2bfd383` | Bugs 4-7 found. Root cause: data plane never wired. |
| 3 | 2026-07-25 | (desk) | Sub-Epic I implemented. Bugs 4-7 closed. Bench re-run pending. |

---

## Session 1 (2026-06-03): fixed

| # | Symptom | Root cause | Fix |
|---|---|---|---|
| 1 | Slice B's VFO flag landed on Slice A's pan, stacked on the top pan | `sliceAdded` handler picked the target `SpectrumWidget` from a transitional dynamic property and fell back to the active pan; no migration path when a slice's pan changed later | `a6ae08e8`: `spectrumForSlice()` + `panKey` migration, ported from AetherSDR `MainWindow::spectrumForSlice` |
| 2 | A flag could not be closed once opened | `VfoWidget::m_closeBtn` existed but was never wired to `removeSlice` | `cf31c157`: wire close button, hide it on Slice A |
| 3 | No way to switch which slice the RX applet targets | No per-slice tab row | `f2bfd383`: per-slice RX applet tab row + active-slice switching |

---

## Session 2 (2026-07-24): findings, all closed in Session 3

All four findings below share **one root cause**: Phase 3F landed the
multi-pan/multi-slice UI layer and the supporting components, but the data
plane that carries samples from a second DDC through to a second pan and a
second audio path was never connected.

| # | Symptom | Root cause | Status |
|---|---|---|---|
| 4 | Second pan never animates | Exactly one `FFTEngine(0)` exists (`MainWindow.cpp:1735`), statically connected at construction to `activeSpectrumWidget()` (`MainWindow.cpp:1838`), which resolves to pan 0 permanently. That `connect` is the only call site of `updateSpectrumLinear` in the tree. Pan 1+ has a fully built `SpectrumWidget` that is never handed a frame. | Fixed (Session 3) |
| 5 | Slice C sits on 20 m and will not leave | `SliceModel::m_frequency{14225000.0}` (`SliceModel.h:916`) is the ctor default, 14.225 MHz USB. Nothing assigns a frequency to a new slice, and `wireSliceSignals()` binds only `m_activeSlice`'s `frequencyChanged` to hardware, at connect time. Slice B+ never reach `ReceiverManager::setReceiverFrequency`. | Fixed (Session 3) |
| 6 | Slice C produces no audio | `RadioModel::addSlice()` creates a `SliceModel` and wires UI signals only. No `createReceiver()`, no DDC mapping, no WDSP channel. One RX channel is ever created (`createRxChannel(0, ...)`), and `RxDspWorker.cpp:216` hardcodes `rxChannel(0)`. `setActiveSlice()` only flips a bool. | Fixed (Session 3) |
| 7 | Radio is never told to enable a second DDC | `RadioModel::invokeCodecDdcAssignment()` (`RadioModel.cpp:10889`) builds the 5-slice config, runs the per-board codec, and pushes wire bytes to `P2RadioConnection::applyDdcAssignment()`. It has **zero callers**. The whole Sub-Epic B codec layer is unreachable. | Fixed (Session 3) |

### Supporting dead-code findings

- `FFTRouter::onFftFrame` is never called from `src/` (only `tests/tst_fft_router_fanout.cpp`), and `fftFrameForPan` is never connected. The topology map is populated at `MainWindow.cpp:1619` and never read.
- That same line registers `slice->ddcIndex()`, which is always `-1`. `SliceModel::setDdcIndex()` has zero callers in the tree.

### Latent corruption risk (not yet operator-visible)

`RxDspWorker::processIqBatch(int receiverIndex, ...)` accepts a receiver
index and ignores it: one accumulation buffer pair, one hardcoded
`rxChannel(0)`. **If a second DDC starts streaming before this is fixed, its
samples are appended into Slice A's accumulator and corrupt Slice A's
audio.** Any work that enables a second DDC must fix this first.

---

## Session 3 (2026-07-25): Sub-Epic I closes Bugs 4-7

Desk implementation, not a bench run. All four Session 2 findings are addressed
in code with unit coverage; the bench re-run is what converts them to verified.

| # | Symptom | Fixed by | Commit |
|---|---|---|---|
| 4 | Second pan never animates | One `FFTEngine` per DDC stream, and frames dispatched to every pan subscribed to that stream via the (previously dead) `FFTRouter` topology | `f6a81666`, `7daadd6e` |
| 5 | Slice C sits on 20 m and will not leave | New slices seed frequency and mode from the active slice instead of the 14.225 MHz ctor default, and every slice's tuning now reaches hardware through `bindSliceToStream` rather than only `m_activeSlice` at connect time | `7d589d10` |
| 6 | Slice C produces no audio | Per-stream accumulation with per-slice fan-out through each slice's own WDSP channel, and `rxBlockReady(sliceIdx, ...)` instead of a hardcoded 0 | `f8241703` |
| 7 | Radio is never told to enable a second DDC | `invokeCodecDdcAssignment` finally has a caller, the codec is indexed by stream rather than slice, and each stream's DDC is routed back to its logical receiver | `f2820e60`, `740deb06` |

### Additional defects found and fixed while implementing

| Symptom | Why it mattered | Commit |
|---|---|---|
| Noise blanker mutated the shared I/Q chunk in place | Once slices share a stream, one slice's blanker corrupts what every later slice sees. Upstream keeps one `ANB` / `NOB` per `_rcvr`. | `2588be39` |
| `fexchange2` can return leaving output buffers untouched | Hoisting the per-slice output buffers would have leaked stale audio on flush and restart transitions. | `f8241703` |
| Hermes-class boards asserted DDC2 on the first VFO turn | `P2CodecOrionMkII` was selected by `default:` for Hermes / HermesII / HermesC10, whose primary DDC is 0. Would have killed RX on a G2E. | `133c30bd`, `60ecfccc` |
| Sole-occupant retune reset the stream to the connection default rate | Silently discarded any per-stream width the operator had set. | `b18cc391` |
| `SliceModel::receiverIndex` became a stale duplicate of `streamIndex` | Three sites read it as the logical receiver index; it was never updated on migration and never set for Slice B+. | `414c164f` |

---

## Matrix rows unblocked by Sub-Epic I

These were untestable in Session 2 because the feature had no implementation.
They are now implemented and awaiting a bench run:

- Row 1 (slice creation up to maxSlices)
- Row 2 (slice removal restores chain BPF)
- Rows 3-8 (per-slice sample rate 48 k through 1536 k)
- Row 9 (slice band change / per-band memory load) for Slice B+
- Rows 10-11 (AetherSDR overlay: same-band and cross-band slices)
- Rows 12-14 (antenna routing) for Slice B+
- Row 15 (TxSliceArbiter handoff): a second stream now exists to hand off to
- Rows 48-60 (new, added by Sub-Epic I)

Rows covering Slice A only (single-slice regression, Alex policy on chain 0,
click-in-island retune) were valid throughout and are unaffected.

### Bench walk to run first

1. **Shared DDC:** Slice A on 14.200, then add B, C, D at 14.180 / 14.225 /
   14.260. All four flags on one pan, all four produce audio, one active DDC.
2. **Separate DDCs:** retune B to 7.150. B claims a second DDC and a second pan
   animates on 40 m while A, C, D stay on 20 m.
3. **Limit:** keep adding cross-band slices past `userDdcCount` and confirm the
   rejection names the hardware limit rather than failing silently.
4. **Regression:** single-slice operation on Slice A, unchanged.

---

## What shipped and works

Confirmed on the bench across both sessions:

- Multi-pan layout templates and pan persistence
- Per-slice VFO flags: creation, pan routing, close button, bidirectional
  state sync with their `SliceModel`
- Per-slice RX applet tab row and active-slice switching
- Single-slice (Slice A) operation, unchanged from v0.5.2

## Next

Data-plane completion is tracked as Sub-Epic I. See
`docs/architecture/2026-07-24-phase3f-sub-epic-i-data-plane-plan.md`.
Sub-Epic H Tasks 3-4 (HL2, G2E, HermesII bench runs) and Task 9 (release PR)
stay blocked until Sub-Epic I lands, because the multi-slice rows are the
bulk of the matrix.
