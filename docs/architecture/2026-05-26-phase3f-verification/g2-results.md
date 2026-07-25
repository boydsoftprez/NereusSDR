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

---

## Session 1 (2026-06-03): fixed

| # | Symptom | Root cause | Fix |
|---|---|---|---|
| 1 | Slice B's VFO flag landed on Slice A's pan, stacked on the top pan | `sliceAdded` handler picked the target `SpectrumWidget` from a transitional dynamic property and fell back to the active pan; no migration path when a slice's pan changed later | `a6ae08e8`: `spectrumForSlice()` + `panKey` migration, ported from AetherSDR `MainWindow::spectrumForSlice` |
| 2 | A flag could not be closed once opened | `VfoWidget::m_closeBtn` existed but was never wired to `removeSlice` | `cf31c157`: wire close button, hide it on Slice A |
| 3 | No way to switch which slice the RX applet targets | No per-slice tab row | `f2bfd383`: per-slice RX applet tab row + active-slice switching |

---

## Session 2 (2026-07-24): open

All four findings below share **one root cause**: Phase 3F landed the
multi-pan/multi-slice UI layer and the supporting components, but the data
plane that carries samples from a second DDC through to a second pan and a
second audio path was never connected.

| # | Symptom | Root cause | Status |
|---|---|---|---|
| 4 | Second pan never animates | Exactly one `FFTEngine(0)` exists (`MainWindow.cpp:1735`), statically connected at construction to `activeSpectrumWidget()` (`MainWindow.cpp:1838`), which resolves to pan 0 permanently. That `connect` is the only call site of `updateSpectrumLinear` in the tree. Pan 1+ has a fully built `SpectrumWidget` that is never handed a frame. | Open |
| 5 | Slice C sits on 20 m and will not leave | `SliceModel::m_frequency{14225000.0}` (`SliceModel.h:916`) is the ctor default, 14.225 MHz USB. Nothing assigns a frequency to a new slice, and `wireSliceSignals()` binds only `m_activeSlice`'s `frequencyChanged` to hardware, at connect time. Slice B+ never reach `ReceiverManager::setReceiverFrequency`. | Open |
| 6 | Slice C produces no audio | `RadioModel::addSlice()` creates a `SliceModel` and wires UI signals only. No `createReceiver()`, no DDC mapping, no WDSP channel. One RX channel is ever created (`createRxChannel(0, ...)`), and `RxDspWorker.cpp:216` hardcodes `rxChannel(0)`. `setActiveSlice()` only flips a bool. | Open |
| 7 | Radio is never told to enable a second DDC | `RadioModel::invokeCodecDdcAssignment()` (`RadioModel.cpp:10889`) builds the 5-slice config, runs the per-board codec, and pushes wire bytes to `P2RadioConnection::applyDdcAssignment()`. It has **zero callers**. The whole Sub-Epic B codec layer is unreachable. | Open |

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

## Matrix rows blocked by the Session 2 root cause

These cannot be ticked on any SKU until the data plane lands. They are not
"fail" results; the feature has no implementation to test.

- Row 1 (slice creation up to maxSlices): slices are created as UI objects only
- Row 2 (slice removal restores chain BPF)
- Rows 3-8 (per-slice sample rate 48 k through 1536 k)
- Row 9 (slice band change / per-band memory load) for Slice B+
- Rows 10-11 (AetherSDR overlay: same-band and cross-band slices)
- Rows 12-14 (antenna routing) for Slice B+
- Row 15 (TxSliceArbiter handoff): arbiter logic exists, but no second chain to hand off to

Rows covering Slice A only (single-slice regression, Alex policy on chain 0,
click-in-island retune) remain valid and are unaffected.

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
