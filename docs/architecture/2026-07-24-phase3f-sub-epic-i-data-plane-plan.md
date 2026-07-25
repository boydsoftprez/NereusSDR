# Phase 3F Sub-Epic I: Data-Plane Completion: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Connect the multi-slice data plane so every user DDC the hardware offers is usable, slices can share one DDC or take separate DDCs as their frequencies demand, and each slice gets its own audio and its own animating panadapter.

**Architecture:** Two distinct concepts, previously conflated. A **DDC stream** is one hardware DDC plus its ReceiverManager receiver, its FFTEngine, its panadapter window, and its noise blanker. A **slice** is one WDSP RX channel plus a shift offset. Slices bind to a stream many-to-one: any number of slices whose frequencies fall inside a stream's window share that stream's I/Q, and a slice that falls outside every active window claims a free DDC. Streams and channels are pre-allocated at connect; binding is what changes at runtime.

**Tech Stack:** C++20, Qt6, WDSP (TAPR v1.29 vendored), FFTW3.

---

## Source basis

### One WDSP, many channels; nobody opens channels at runtime

- **Thetis** `cmaster.cs:502-516 [v2.10.3.15]`: `SetRadioStructure(8, cmRCVR=5, cmXMTR=1, cmSubRCVR=2, ...)` then `CreateRadio()` opens all 10 RX channels plus TX at startup. WDSP holds `MAX_CHANNELS 32` in one instance (`comm.h:104`).
- **deskhpsdr** `radio.c:1256-1259 [@f3d857c]`: "To be on the safe side, we create ALL receiver panels here", looping `rx_create_receiver(CHANNEL_RX0 + i, ...)` into `OpenChannel(rx->id, ...)` (`receiver.c:1048`). `radio_change_receivers()` only toggles `displaying`.

### One DDC stream drives N slices, and what they share

Authoritative in `ChannelMaster/cmaster.h:75-82 [v2.10.3.15]`:

```c
struct _rcvr
{
    int ch_outrate;
    int ch_outsize;
    double* audio[cmMAXSubRcvr];    // audio buff, per subrx
    volatile long run_pan;          // run panadapter
    ANB panb;                       // noiseblanker, per receiver
    NOB pnob;                       // noiseblanker II, per receiver
} rcvr[cmMAXrcvr];
```

Read that carefully, because it defines the whole sharing contract:

| Resource | Scope | Why |
|---|---|---|
| I/Q input | per stream | one DDC, one sample flow |
| Noise blanker (`panb` / `pnob`) | **per stream** | NB operates on raw I/Q before demodulation |
| Panadapter (`run_pan`) | **per stream** | the window is the DDC's |
| Audio (`audio[subrx]`) | **per slice** | each slice demodulates independently |
| Mode / filter / AGC / shift | per slice | each is its own WDSP channel |

So slices sharing a DDC share the noise blanker and the panadapter window, and differ in everything downstream of demodulation. That is not a limitation we are choosing; it is what the hardware and WDSP topology impose.

### The window constraint

Thetis tunes a sub-receiver by oscillator offset and refuses to let it leave the parent's window, `console.cs:31913-31925 [v2.10.3.15]`:

```csharp
int rx2_osc = (int)(radio.GetDSPRX(0, 0).RXOsc - diff);
if (rx2_osc > -sample_rate_rx1 / 2 && rx2_osc < sample_rate_rx1 / 2)
{
    radio.GetDSPRX(0, 1).RXOsc = rx2_osc;
}
else chkEnableMultiRX.Checked = false;
```

`//MW0LGE [2.7.0.9] only when RX'ing. Fixes issue where multirx would be outside sample area after a tx` is the inline tag on that branch; preserve it verbatim when porting the guard.

The offset setter itself, `radio.cs:1409-1425`, is `SetRXAShiftFreq` + `RXANBPSetShiftFrequency`. **NereusSDR already has this** as `RxChannel::setShiftFrequency` (`RxChannel.h:606`), which cites `radio.cs:1417-1418`. No new WDSP wrapper is needed.

### Deliberate divergence from Thetis

| Aspect | Thetis | NereusSDR Sub-Epic I | Why |
|---|---|---|---|
| Slices per stream | 2 (`cmSubRCVR`) | up to `maxSlices` | Product requirement: A through D on one DDC. WDSP imposes no such cap; `cmSubRCVR` is a Thetis structure choice. |
| User DDC streams | 2 in practice (RX1 / RX2) | every user DDC the SKU exposes (5 on G2) | Same reason. Thetis leaves DDC4-6 idle on Saturn-class; the codec already maps them (`P2CodecSaturn.cpp:222`, `kSliceToDdc[5] = {2,3,4,5,6}`). |
| Slice leaves window | disables Multi-RX | promote to a free DDC; reject only when none free | Strictly better operator outcome, and we have DDCs Thetis never uses. |

Record these three in the design doc (Task 11); they are the kind of divergence CLAUDE.md requires be stated, not silently taken.

### No upstream aggregate-bandwidth guard exists

Neither Thetis nor deskhpsdr enforces a total-bandwidth budget in software. Limits are structural: user DDC count, `maxSlices`, the per-SKU rate ladder, and on P1 a single shared rate across receivers (deskhpsdr `radio.c`: "Make sure RX2 shares the sample rate with RX1 when running P1"). Do not invent an aggregate check; enforce the structural limits, which is what "stay within the limits of the respective hardware" means here.

## Invariants this plan establishes

1. **WDSP RX channel id == slice index** (`0 .. maxSlices-1`). One channel per slice, always.
2. **Stream index == ReceiverManager receiver index == FFTEngine key** (`0 .. userDdcCount-1`).
3. **Stream index != DDC number.** The codec owns the mapping (`kSliceToDdc`), and it is published back as `SliceModel::ddcIndex`.
4. A slice with `streamIndex() < 0` is unbound and feeds nothing.

Note the change from the earlier draft of this plan: the old "chain" concept fused stream and channel 1:1, which cannot express slices sharing a DDC. Streams and channels are now separate pools with different sizes.

## What already exists (do not rebuild)

- `WdspEngine` holds `std::map<int, std::unique_ptr<RxChannel>>`; `createRxChannel(channelId, ...)` is N-capable.
- `RxChannel::setShiftFrequency(double)` (`RxChannel.h:606`), the Thetis `RXOsc` port.
- `ReceiverManager::feedIqData` emits `iqDataForReceiver(logicalIndex, samples)` with the right per-DDC index.
- `FFTEngine(int receiverId)`; `fftReadyLinear(receiverId, ...)`; `SpectrumWidget::updateSpectrumLinear(receiverId, ...)`.
- `MasterMixer` handles N slices; `AudioEngine::rxBlockReady(sliceId, ...)` live at `AudioEngine.cpp:957`.
- `RadioModel::invokeCodecDdcAssignment()` + all six codecs + `P2RadioConnection::applyDdcAssignment`. Zero callers.
- `FFTRouter` topology map, unit-tested.
- `BoardCapabilities::maxSlices` (`BoardCapabilities.h:238`).

## File Structure

### Files to create

| File | Responsibility |
|---|---|
| `src/core/SliceStreamAllocator.h/.cpp` | Pure allocation policy: which stream should host a slice at frequency F, given active streams and hardware limits. No Qt dependencies beyond QObject-free structs, so it is exhaustively unit-testable. |
| `tests/tst_slice_stream_allocator.cpp` | Window fit, promotion, exhaustion, retune migration |
| `tests/tst_rx_dsp_worker_multi_slice.cpp` | One stream fanning to N slices without cross-talk |
| `tests/tst_stream_pool_binding.cpp` | Pool lifecycle and slice binding through RadioModel |
| `tests/tst_fft_engine_pool.cpp` | Per-stream engine dispatch and co-hosted pan topology |

### Files to modify

| File | Change |
|---|---|
| `src/core/BoardCapabilities.h/.cpp` | Add `userDdcCount` + per-SKU values |
| `src/models/SliceModel.h/.cpp` | `streamIndex` + `shiftOffsetHz` properties |
| `src/models/RxDspWorker.h/.cpp` | Per-stream accumulators; fan each drain to every bound slice |
| `src/models/RadioModel.h/.cpp` | Stream pool, allocator wiring, codec invocation, per-slice tuning |
| `src/gui/MainWindow.h/.cpp` | FFTEngine per stream, `rebuildFftRouting`, dispatch |
| `src/core/codec/*.cpp` | Populate `DdcAssignment::sliceDdc` |
| `src/core/DdcAssignment.h` | Add `sliceDdc[5]` |
| `docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md` | §3 + §4 amendments |
| `docs/architecture/2026-05-26-phase3f-verification/README.md` | New rows for sharing |

---

## Task 1: Add `userDdcCount` to BoardCapabilities

**Files:**
- Modify: `src/core/BoardCapabilities.h:238` (next to `maxSlices`)
- Modify: `src/core/BoardCapabilities.cpp` (every SKU initializer)
- Test: `tests/tst_board_capabilities_phase3f.cpp` (extend)

- [ ] **Step 1: Write the failing test**

Append to the existing `tests/tst_board_capabilities_phase3f.cpp`:

```cpp
    void user_ddc_count_matches_design_doc_table()
    {
        // Design doc §2 "Resolved values per SKU". User DDCs are the DDCs
        // available for operator slices after PS/diversity reservations.
        QCOMPARE(capsForModel(HPSDRModel::ANAN_G2).userDdcCount, 5);       // DDC2-6
        QCOMPARE(capsForModel(HPSDRModel::HermesLite).userDdcCount, 1);    // DDC0 only
        QCOMPARE(capsForModel(HPSDRModel::HermesII).userDdcCount, 2);      // DDC0-1
        QCOMPARE(capsForModel(HPSDRModel::Hermes).userDdcCount, 4);        // DDC0-3
        QCOMPARE(capsForModel(HPSDRModel::Metis).userDdcCount, 3);         // DDC0-2
    }

    void user_ddc_count_never_exceeds_max_slices()
    {
        // A SKU may host more slices than DDCs (slices share a DDC), but
        // never more DDCs than slices, since a stream with no slice is idle.
        for (HPSDRModel m : allSupportedModels()) {
            const auto& c = capsForModel(m);
            QVERIFY2(c.userDdcCount <= c.maxSlices,
                     qPrintable(QStringLiteral("model %1").arg(static_cast<int>(m))));
        }
    }
```

Match the helper names already used in that file; if it uses a different accessor than `capsForModel`, use the existing one.

- [ ] **Step 2: Run test to verify it fails**

```bash
W=/Users/j.j.boyd/NereusSDR/.worktrees/phase3f-sub-epic-a-foundation
cmake --build $W/build --target tst_board_capabilities_phase3f -j8
```

Expected: FAIL to compile, `no member named 'userDdcCount'`.

- [ ] **Step 3: Add the field**

In `src/core/BoardCapabilities.h`, immediately after line 238:

```cpp
    // Phase 3F Sub-Epic I: DDCs available for operator slices, after the
    // per-SKU PureSignal / diversity reservations. On 2-ADC P2 boards
    // DDC0+DDC1 are reserved as a synced pair, so user DDCs are DDC2-6.
    // Design doc §2 "Resolved values per SKU" is the authority; the
    // per-board codec's slice-to-DDC table must agree (for example
    // P2CodecSaturn::kSliceToDdc = {2,3,4,5,6}).
    //
    // This is a distinct axis from maxSlices: several slices can share one
    // DDC when their frequencies fall inside its window, so maxSlices can
    // legitimately exceed userDdcCount.
    int  userDdcCount {0};
```

- [ ] **Step 4: Populate every SKU**

In `src/core/BoardCapabilities.cpp`, add `.userDdcCount` beside each existing `.maxSlices` line, per design doc §2:

| SKU initializer | `.userDdcCount` |
|---|---|
| `kMetis` | `3` |
| `kHermes` | `4` |
| `kHermesII` | `2` |
| `kAngelia` | `5` |
| `kOrion` | `5` |
| `kOrionMkII` | `5` |
| `kHermesC10` | `5` |
| `kHermesLite` | `1` |
| `kHermesLiteRxOnly` | `1` |
| `kSaturn` | `5` |
| `kSaturnMKII` | `5` |
| `kAndromeda` | `5` |

- [ ] **Step 5: Run test to verify it passes**

```bash
cmake --build $W/build --target tst_board_capabilities_phase3f -j8 && ctest --test-dir $W/build -R tst_board_capabilities_phase3f --output-on-failure
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add src/core/BoardCapabilities.h src/core/BoardCapabilities.cpp tests/tst_board_capabilities_phase3f.cpp
git commit -S -m "feat(3f-i): BoardCapabilities gains userDdcCount

Distinct axis from maxSlices: slices can share a DDC when their
frequencies fall inside its window, so maxSlices may exceed
userDdcCount. Values from design doc §2.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 2: SliceStreamAllocator: the placement policy

Pure logic, no Qt signals, no hardware. This is where "share a DDC or take a new one" is decided, so it gets the heaviest test coverage in the plan.

**Files:**
- Create: `src/core/SliceStreamAllocator.h`, `src/core/SliceStreamAllocator.cpp`
- Create: `tests/tst_slice_stream_allocator.cpp`
- Modify: `src/CMakeLists.txt`, `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/tst_slice_stream_allocator.cpp`:

```cpp
// =================================================================
// tests/tst_slice_stream_allocator.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. The window-fit rule ports Thetis
// console.cs:31920 [v2.10.3.15]; the promote-instead-of-
// disable behaviour is a documented divergence (design doc §3).
//
// Phase 3F Sub-Epic I Task 2.
// =================================================================
#include <QtTest/QtTest>
#include "core/SliceStreamAllocator.h"

using namespace NereusSDR;

class TestSliceStreamAllocator : public QObject {
    Q_OBJECT
private slots:
    void slice_joins_a_stream_whose_window_contains_it()
    {
        SliceStreamAllocator alloc;
        alloc.configure(/*userDdcCount*/ 5, /*maxSlices*/ 5);
        // Stream 0 active, centred 14.200 MHz, 192 kHz wide: +-96 kHz.
        alloc.activateStream(0, 14200000.0, 192000);

        const auto r = alloc.placeSlice(14225000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::JoinedExisting);
        QCOMPARE(r.streamIndex, 0);
        QCOMPARE(r.shiftOffsetHz, 25000.0);
    }

    void slice_outside_every_window_claims_a_free_stream()
    {
        SliceStreamAllocator alloc;
        alloc.configure(5, 5);
        alloc.activateStream(0, 14200000.0, 192000);

        const auto r = alloc.placeSlice(7150000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::NewStream);
        QCOMPARE(r.streamIndex, 1);
        QCOMPARE(r.shiftOffsetHz, 0.0);   // new stream centres on the slice
    }

    void four_slices_share_one_stream_when_all_fit()
    {
        SliceStreamAllocator alloc;
        alloc.configure(5, 5);
        alloc.activateStream(0, 14200000.0, 192000);

        // A through D, all inside +-96 kHz of 14.200.
        for (double f : {14150000.0, 14180000.0, 14225000.0, 14260000.0}) {
            const auto r = alloc.placeSlice(f);
            QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::JoinedExisting);
            QCOMPARE(r.streamIndex, 0);
        }
        QCOMPARE(alloc.activeStreamCount(), 1);
    }

    void edge_of_window_is_excluded()
    {
        SliceStreamAllocator alloc;
        alloc.configure(5, 5);
        alloc.activateStream(0, 14200000.0, 192000);

        // Exactly +96 kHz is the Nyquist edge. Thetis uses a strict
        // inequality (console.cs:31920), so this must NOT join.
        const auto r = alloc.placeSlice(14200000.0 + 96000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::NewStream);
    }

    void exhausted_streams_reject_with_a_reason()
    {
        SliceStreamAllocator alloc;
        alloc.configure(/*userDdcCount*/ 1, /*maxSlices*/ 5);
        alloc.activateStream(0, 14200000.0, 192000);

        const auto r = alloc.placeSlice(7150000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::Rejected);
        QVERIFY(!r.reason.isEmpty());
    }

    void retune_inside_the_window_only_moves_the_shift()
    {
        SliceStreamAllocator alloc;
        alloc.configure(5, 5);
        alloc.activateStream(0, 14200000.0, 192000);
        alloc.placeSlice(14225000.0);

        const auto r = alloc.retuneSlice(/*currentStream*/ 0,
                                         /*isSoleOccupant*/ false,
                                         14180000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::JoinedExisting);
        QCOMPARE(r.streamIndex, 0);
        QCOMPARE(r.shiftOffsetHz, -20000.0);
    }

    void sole_occupant_leaving_its_window_retunes_the_stream()
    {
        SliceStreamAllocator alloc;
        alloc.configure(5, 5);
        alloc.activateStream(0, 14200000.0, 192000);

        // Only slice on stream 0: cheaper to move the DDC than to burn
        // another one.
        const auto r = alloc.retuneSlice(0, /*isSoleOccupant*/ true, 7150000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::RetunedStream);
        QCOMPARE(r.streamIndex, 0);
        QCOMPARE(r.shiftOffsetHz, 0.0);
    }

    void co_hosted_slice_leaving_its_window_migrates_to_a_free_stream()
    {
        SliceStreamAllocator alloc;
        alloc.configure(5, 5);
        alloc.activateStream(0, 14200000.0, 192000);

        // Another slice still needs stream 0 where it is, so this one moves.
        const auto r = alloc.retuneSlice(0, /*isSoleOccupant*/ false, 7150000.0);

        QCOMPARE(r.outcome, SliceStreamAllocator::Outcome::NewStream);
        QCOMPARE(r.streamIndex, 1);
    }
};

QTEST_MAIN(TestSliceStreamAllocator)
#include "tst_slice_stream_allocator.moc"
```

Append to `tests/CMakeLists.txt`:

```cmake
nereus_add_test(tst_slice_stream_allocator)
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build $W/build --target tst_slice_stream_allocator -j8
```

Expected: FAIL, `'core/SliceStreamAllocator.h' file not found`.

- [ ] **Step 3: Write the header**

Create `src/core/SliceStreamAllocator.h`:

```cpp
// =================================================================
// src/core/SliceStreamAllocator.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original policy class. The window-fit rule
// ports Thetis console.cs:31913-31925 [v2.10.3.15]; the
// stream topology mirrors ChannelMaster cmaster.h:75-82 (one _rcvr
// drives cmMAXSubRcvr channels off one I/Q input). Thetis has no
// equivalent allocator because it hard-codes RX1 -> DDC2 and
// RX2 -> DDC3; NereusSDR allocates across every user DDC the SKU has.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-07-24  J.J. Boyd / KG4VCF  Phase 3F Sub-Epic I Task 2.
//                                    Slice-to-DDC-stream placement
//                                    policy. AI-assisted transformation
//                                    via Anthropic Claude Code.
// =================================================================
#pragma once

#include <QString>
#include <QVector>

namespace NereusSDR {

/// Decides which DDC stream should host a slice at a given frequency.
///
/// A stream is one hardware DDC plus its receiver, FFT engine, panadapter
/// window, and noise blanker. Slices bind to a stream many-to-one: any
/// number whose frequencies fall inside the stream's window share its I/Q
/// (ChannelMaster cmaster.h:75-82, one `_rcvr`, `audio[cmMAXSubRcvr]`).
///
/// Pure policy: no Qt signals, no hardware, no WDSP. Everything it needs
/// is passed in, so the whole decision surface is unit-testable.
class SliceStreamAllocator {
public:
    enum class Outcome {
        JoinedExisting,   ///< Fits an active stream's window; set shift only.
        NewStream,        ///< Claimed a free DDC, centred on the slice.
        RetunedStream,    ///< Sole occupant; moved its stream's centre instead.
        Rejected          ///< No stream fits and none free. `reason` explains.
    };

    struct Placement {
        Outcome outcome{Outcome::Rejected};
        int     streamIndex{-1};
        double  shiftOffsetHz{0.0};   ///< slice freq minus stream centre
        double  newStreamCentreHz{0.0}; ///< set for NewStream / RetunedStream
        QString reason;               ///< human-readable, for Rejected
    };

    /// Size the allocator to the connected SKU. Clears all stream state.
    void configure(int userDdcCount, int maxSlices);

    /// Mark a stream active at a centre frequency and sample rate.
    void activateStream(int streamIndex, double centreHz, int sampleRateHz);

    /// Mark a stream idle (its last slice went away).
    void deactivateStream(int streamIndex);

    /// Where should a brand-new slice at `frequencyHz` go?
    Placement placeSlice(double frequencyHz) const;

    /// Where should an existing slice go after retuning to `frequencyHz`?
    /// `isSoleOccupant` decides whether moving the stream's centre is
    /// allowed: if other slices depend on this window, it is not.
    Placement retuneSlice(int currentStream,
                          bool isSoleOccupant,
                          double frequencyHz) const;

    int  streamCount() const { return m_streams.size(); }
    int  activeStreamCount() const;
    bool isStreamActive(int streamIndex) const;
    double streamCentreHz(int streamIndex) const;
    int  streamSampleRateHz(int streamIndex) const;

    /// Default rate for a newly claimed stream. Callers override per SKU.
    void setDefaultSampleRateHz(int rateHz) { m_defaultRateHz = rateHz; }

private:
    struct Stream {
        bool   active{false};
        double centreHz{0.0};
        int    sampleRateHz{0};
    };

    /// True when `frequencyHz` is strictly inside the stream's window.
    /// Strict on both sides, matching Thetis console.cs:31920
    /// [v2.10.3.15]:
    ///   if (rx2_osc > -sample_rate_rx1 / 2 && rx2_osc < sample_rate_rx1 / 2)
    /// A slice exactly at the Nyquist edge is aliased, so it does not fit.
    bool windowContains(const Stream& s, double frequencyHz) const;

    int firstFreeStream() const;

    QVector<Stream> m_streams;
    int m_maxSlices{0};
    int m_defaultRateHz{192000};
};

} // namespace NereusSDR
```

- [ ] **Step 4: Write the implementation**

Create `src/core/SliceStreamAllocator.cpp`:

```cpp
// =================================================================
// src/core/SliceStreamAllocator.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See SliceStreamAllocator.h for
// the full rationale and Modification history block.
//
// =================================================================

#include "core/SliceStreamAllocator.h"

#include <cmath>

namespace NereusSDR {

void SliceStreamAllocator::configure(int userDdcCount, int maxSlices)
{
    m_streams.clear();
    m_streams.resize(qMax(0, userDdcCount));
    m_maxSlices = qMax(0, maxSlices);
}

void SliceStreamAllocator::activateStream(int streamIndex, double centreHz,
                                          int sampleRateHz)
{
    if (streamIndex < 0 || streamIndex >= m_streams.size()) { return; }
    Stream& s = m_streams[streamIndex];
    s.active       = true;
    s.centreHz     = centreHz;
    s.sampleRateHz = sampleRateHz;
}

void SliceStreamAllocator::deactivateStream(int streamIndex)
{
    if (streamIndex < 0 || streamIndex >= m_streams.size()) { return; }
    m_streams[streamIndex].active = false;
}

bool SliceStreamAllocator::windowContains(const Stream& s,
                                          double frequencyHz) const
{
    if (!s.active || s.sampleRateHz <= 0) { return false; }
    const double halfWindow = static_cast<double>(s.sampleRateHz) / 2.0;
    const double offset     = frequencyHz - s.centreHz;
    // Strict both sides. From Thetis console.cs:31920 [v2.10.3.15]:
    //   if (rx2_osc > -sample_rate_rx1 / 2 && rx2_osc < sample_rate_rx1 / 2)
    // MW0LGE [2.7.0.9] only when RX'ing. Fixes issue where multirx would be
    // outside sample area after a tx  [original inline comment from
    // console.cs:31913]
    return offset > -halfWindow && offset < halfWindow;
}

int SliceStreamAllocator::firstFreeStream() const
{
    for (int i = 0; i < m_streams.size(); ++i) {
        if (!m_streams.at(i).active) { return i; }
    }
    return -1;
}

SliceStreamAllocator::Placement
SliceStreamAllocator::placeSlice(double frequencyHz) const
{
    Placement p;

    // 1. Prefer sharing: an active stream whose window already covers this
    //    frequency costs no extra DDC and no extra bus bandwidth.
    for (int i = 0; i < m_streams.size(); ++i) {
        if (windowContains(m_streams.at(i), frequencyHz)) {
            p.outcome       = Outcome::JoinedExisting;
            p.streamIndex   = i;
            p.shiftOffsetHz = frequencyHz - m_streams.at(i).centreHz;
            return p;
        }
    }

    // 2. Otherwise claim a free DDC and centre it on the slice.
    const int free = firstFreeStream();
    if (free >= 0) {
        p.outcome           = Outcome::NewStream;
        p.streamIndex       = free;
        p.shiftOffsetHz     = 0.0;
        p.newStreamCentreHz = frequencyHz;
        return p;
    }

    // 3. Every DDC is in use and none covers this frequency. Thetis would
    //    simply disable Multi-RX here (console.cs:31924); we surface the
    //    hardware limit instead so the UI can explain it.
    p.outcome = Outcome::Rejected;
    p.reason  = QStringLiteral(
        "All %1 receiver DDCs are in use and none covers %2 MHz. "
        "Retune or remove a slice, or widen a DDC's sample rate.")
        .arg(m_streams.size())
        .arg(frequencyHz / 1.0e6, 0, 'f', 4);
    return p;
}

SliceStreamAllocator::Placement
SliceStreamAllocator::retuneSlice(int currentStream,
                                  bool isSoleOccupant,
                                  double frequencyHz) const
{
    Placement p;

    // Still inside its own window: nothing moves but the shift oscillator.
    if (currentStream >= 0 && currentStream < m_streams.size()
        && windowContains(m_streams.at(currentStream), frequencyHz)) {
        p.outcome       = Outcome::JoinedExisting;
        p.streamIndex   = currentStream;
        p.shiftOffsetHz = frequencyHz - m_streams.at(currentStream).centreHz;
        return p;
    }

    // Sole occupant: moving the DDC is cheaper than burning another one,
    // and no other slice depends on this window staying put.
    if (isSoleOccupant && currentStream >= 0
        && currentStream < m_streams.size()) {
        p.outcome           = Outcome::RetunedStream;
        p.streamIndex       = currentStream;
        p.shiftOffsetHz     = 0.0;
        p.newStreamCentreHz = frequencyHz;
        return p;
    }

    // Co-hosted: this slice must leave. Same policy as a fresh placement.
    return placeSlice(frequencyHz);
}

int SliceStreamAllocator::activeStreamCount() const
{
    int n = 0;
    for (const Stream& s : m_streams) {
        if (s.active) { ++n; }
    }
    return n;
}

bool SliceStreamAllocator::isStreamActive(int streamIndex) const
{
    if (streamIndex < 0 || streamIndex >= m_streams.size()) { return false; }
    return m_streams.at(streamIndex).active;
}

double SliceStreamAllocator::streamCentreHz(int streamIndex) const
{
    if (streamIndex < 0 || streamIndex >= m_streams.size()) { return 0.0; }
    return m_streams.at(streamIndex).centreHz;
}

int SliceStreamAllocator::streamSampleRateHz(int streamIndex) const
{
    if (streamIndex < 0 || streamIndex >= m_streams.size()) { return 0; }
    return m_streams.at(streamIndex).sampleRateHz;
}

} // namespace NereusSDR
```

Register the new source in `src/CMakeLists.txt` beside the other `src/core` entries.

- [ ] **Step 5: Run test to verify it passes**

```bash
cmake --build $W/build --target tst_slice_stream_allocator -j8 && ctest --test-dir $W/build -R tst_slice_stream_allocator --output-on-failure
```

Expected: PASS, 8 of 8 tests.

- [ ] **Step 6: Commit**

```bash
git add src/core/SliceStreamAllocator.h src/core/SliceStreamAllocator.cpp src/CMakeLists.txt tests/tst_slice_stream_allocator.cpp tests/CMakeLists.txt
git commit -S -m "feat(3f-i): SliceStreamAllocator placement policy

Decides whether a slice shares an active DDC stream (window fit, shift
offset only) or claims a free DDC. Window rule ports the strict
inequality from Thetis console.cs:31920; promoting to a free DDC instead
of disabling Multi-RX is a documented divergence.

Pure policy class, no Qt signals or hardware, so the whole decision
surface is unit-tested.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 3: SliceModel gains streamIndex and shiftOffsetHz

**Files:**
- Modify: `src/models/SliceModel.h` (near `ddcIndex` at line 183), `src/models/SliceModel.cpp`
- Test: `tests/tst_slice_model_phase3f_properties.cpp` (extend)

- [ ] **Step 1: Write the failing test**

Append to the existing properties test:

```cpp
    void stream_index_defaults_to_unbound()
    {
        SliceModel s;
        QCOMPARE(s.streamIndex(), -1);
    }

    void stream_index_change_emits_once()
    {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::streamIndexChanged);
        s.setStreamIndex(2);
        s.setStreamIndex(2);           // idempotent
        QCOMPARE(spy.count(), 1);
        QCOMPARE(s.streamIndex(), 2);
    }

    void shift_offset_round_trips()
    {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::shiftOffsetHzChanged);
        s.setShiftOffsetHz(-25000.0);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(s.shiftOffsetHz(), -25000.0);
    }
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build $W/build --target tst_slice_model_phase3f_properties -j8
```

Expected: FAIL to compile, `no member named 'streamIndex'`.

- [ ] **Step 3: Add the properties**

In `src/models/SliceModel.h`, beside the `ddcIndex` Q_PROPERTY at line 183:

```cpp
    // Phase 3F Sub-Epic I: which DDC stream hosts this slice. Many slices
    // may share one stream when their frequencies fall inside its window
    // (ChannelMaster cmaster.h:75-82, one `_rcvr` drives N sub-receiver
    // channels off one I/Q input). -1 = unbound, feeds nothing.
    //
    // Distinct from ddcIndex, which is the hardware DDC number the codec
    // picked for this stream. streamIndex is the logical index shared by
    // ReceiverManager, FFTEngine, and the FFTRouter topology.
    Q_PROPERTY(int streamIndex READ streamIndex WRITE setStreamIndex
               NOTIFY streamIndexChanged)

    // Phase 3F Sub-Epic I: this slice's offset from its stream's centre,
    // pushed into WDSP via RxChannel::setShiftFrequency (the Thetis RXOsc
    // port, radio.cs:1409-1425). Zero when the slice sits on the DDC
    // centre. Always within +-sampleRate/2 by construction; the allocator
    // never produces an out-of-window offset.
    Q_PROPERTY(double shiftOffsetHz READ shiftOffsetHz WRITE setShiftOffsetHz
               NOTIFY shiftOffsetHzChanged)
```

Accessors beside `ddcIndex()` at line 448:

```cpp
    int  streamIndex() const { return m_streamIndex; }
    void setStreamIndex(int idx);
    double shiftOffsetHz() const { return m_shiftOffsetHz; }
    void setShiftOffsetHz(double hz);
```

Signals beside `ddcIndexChanged` at line 810:

```cpp
    void streamIndexChanged(int idx);
    void shiftOffsetHzChanged(double hz);
```

Members beside `m_ddcIndex` at line 938:

```cpp
    int    m_streamIndex{-1};     // Phase 3F Sub-Epic I: -1 = unbound
    double m_shiftOffsetHz{0.0};  // offset from stream centre
```

In `src/models/SliceModel.cpp`, beside `setDdcIndex` at line 615:

```cpp
void SliceModel::setStreamIndex(int idx)
{
    if (m_streamIndex == idx) { return; }
    m_streamIndex = idx;
    emit streamIndexChanged(idx);
}

void SliceModel::setShiftOffsetHz(double hz)
{
    if (qFuzzyCompare(m_shiftOffsetHz, hz)) { return; }
    m_shiftOffsetHz = hz;
    emit shiftOffsetHzChanged(hz);
}
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cmake --build $W/build --target tst_slice_model_phase3f_properties -j8 && ctest --test-dir $W/build -R tst_slice_model_phase3f_properties --output-on-failure
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add src/models/SliceModel.h src/models/SliceModel.cpp tests/tst_slice_model_phase3f_properties.cpp
git commit -S -m "feat(3f-i): SliceModel gains streamIndex + shiftOffsetHz

streamIndex is the logical DDC stream hosting this slice, many-to-one so
slices can share a DDC. shiftOffsetHz is its offset from that stream's
centre, destined for RxChannel::setShiftFrequency.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 4: RxDspWorker fans one stream to every bound slice

This supersedes the single-accumulator design and is the corruption guard. It must land before anything enables a second DDC.

**Files:**
- Modify: `src/models/RxDspWorker.h:267-268`, `src/models/RxDspWorker.cpp:176-350`
- Create: `tests/tst_rx_dsp_worker_multi_slice.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/tst_rx_dsp_worker_multi_slice.cpp`:

```cpp
// =================================================================
// tests/tst_rx_dsp_worker_multi_slice.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic I Task 4: per-stream accumulation, per-slice fan-out.
// =================================================================
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/RxDspWorker.h"

using namespace NereusSDR;

class TestRxDspWorkerMultiSlice : public QObject {
    Q_OBJECT
private slots:
    void streams_do_not_share_an_accumulator()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        QSignalSpy spy(&worker, &RxDspWorker::chunkDrained);

        const QVector<float> two{0.1f, 0.1f, 0.2f, 0.2f};   // 2 samples
        worker.processIqBatch(0, two);
        worker.processIqBatch(1, two);

        // Shared accumulator would total 4 and drain. Per-stream: neither
        // reaches 4, so nothing drains.
        QCOMPARE(spy.count(), 0);
    }

    void a_stream_drains_at_its_own_threshold()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        QSignalSpy spy(&worker, &RxDspWorker::chunkDrainedForStream);

        const QVector<float> four{0.1f, 0.1f, 0.2f, 0.2f,
                                  0.3f, 0.3f, 0.4f, 0.4f};
        worker.processIqBatch(3, four);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 3);
        QCOMPARE(spy.at(0).at(1).toInt(), 4);
    }

    void every_slice_bound_to_a_stream_is_offered_the_drain()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        // Slices 0, 2 and 3 all live on stream 1 (they share a DDC).
        worker.setStreamSlices(1, QVector<int>{0, 2, 3});

        QSignalSpy spy(&worker, &RxDspWorker::sliceProcessed);

        const QVector<float> four{0.1f, 0.1f, 0.2f, 0.2f,
                                  0.3f, 0.3f, 0.4f, 0.4f};
        worker.processIqBatch(1, four);

        QCOMPARE(spy.count(), 3);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QCOMPARE(spy.at(1).at(0).toInt(), 2);
        QCOMPARE(spy.at(2).at(0).toInt(), 3);
    }

    void a_stream_with_no_slices_processes_nothing()
    {
        RxDspWorker worker;
        worker.setBufferSizes(4, 64);
        QSignalSpy spy(&worker, &RxDspWorker::sliceProcessed);

        const QVector<float> four{0.1f, 0.1f, 0.2f, 0.2f,
                                  0.3f, 0.3f, 0.4f, 0.4f};
        worker.processIqBatch(2, four);   // no setStreamSlices for 2

        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestRxDspWorkerMultiSlice)
#include "tst_rx_dsp_worker_multi_slice.moc"
```

Append to `tests/CMakeLists.txt`:

```cmake
nereus_add_test(tst_rx_dsp_worker_multi_slice)
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build $W/build --target tst_rx_dsp_worker_multi_slice -j8
```

Expected: FAIL to compile, `no member named 'setStreamSlices'`.

- [ ] **Step 3: Replace the shared accumulator with per-stream state**

In `src/models/RxDspWorker.h`, replace lines 267-268:

```cpp
    QVector<float>   m_iqAccumI;
    QVector<float>   m_iqAccumQ;
```

with:

```cpp
    // ── Phase 3F Sub-Epic I Task 4: per-stream accumulation ─────────────
    //
    // Before this, one shared accumulator pair served every caller and
    // processIqBatch's receiverIndex argument was ignored, so a second
    // DDC's samples were appended into Slice A's stream and corrupted its
    // audio.
    //
    // Keyed by DDC stream index. Each stream's drained chunk is then fed
    // to every slice bound to that stream, mirroring ChannelMaster's
    // `struct _rcvr` which holds one I/Q input and one noise blanker but
    // `audio[cmMAXSubRcvr]` outputs (cmaster.h:75-82
    // [v2.10.3.15]).
    //
    // Touched only on the DSP thread, so no lock is needed. m_streamSlices
    // is written from the main thread via a queued setStreamSlices call,
    // which lands on the DSP thread's event loop, so it is also DSP-thread
    // only at the point of use.
    struct StreamAccum {
        QVector<float> i;
        QVector<float> q;
    };
    std::unordered_map<int, StreamAccum>     m_accums;
    std::unordered_map<int, QVector<int>>    m_streamSlices;
```

Add `#include <unordered_map>`.

Add to public slots:

```cpp
    /// Declare which slice indices are bound to a DDC stream. Called from
    /// the main thread on every slice bind / unbind / migration; queued, so
    /// the map is only ever touched on the DSP thread.
    void setStreamSlices(int streamIndex, const QVector<int>& sliceIndices);

    /// Drop all per-stream accumulation and binding state (disconnect).
    void resetStreams();
```

Add to signals:

```cpp
    /// Per-stream companion to chunkDrained, carrying the originating
    /// stream. chunkDrained is kept for existing single-slice subscribers.
    void chunkDrainedForStream(int streamIndex, int samples);

    /// Emitted once per slice per drained chunk, after that slice's WDSP
    /// channel has run. Test seam for the fan-out.
    void sliceProcessed(int sliceIndex, int samples);
```

- [ ] **Step 4: Rewrite the drain to fan out**

In `src/models/RxDspWorker.cpp`, replace the accumulate block (lines 203-208) with:

```cpp
    StreamAccum& acc = m_accums[receiverIndex];
    acc.i.reserve(acc.i.size() + numSamples);
    acc.q.reserve(acc.q.size() + numSamples);
    for (int i = 0; i < numSamples; ++i) {
        acc.i.append(interleavedIQ[i * 2]);
        acc.q.append(interleavedIQ[i * 2 + 1]);
    }
```

Replace the drain loop. The old loop ran one channel; the new one runs every slice bound to this stream against the same input chunk:

```cpp
    const QVector<int> slices = m_streamSlices.count(receiverIndex)
                                    ? m_streamSlices.at(receiverIndex)
                                    : QVector<int>{};

    while (acc.i.size() >= inSize) {
        if (m_wdspEngine != nullptr && m_audioEngine != nullptr) {
            // Every slice on this stream demodulates the SAME I/Q chunk
            // through its own WDSP channel, differing by shift offset,
            // mode, filter and AGC. This is ChannelMaster's one-_rcvr-many-
            // subrx topology (cmaster.h:75-82 [v2.10.3.15]).
            for (int sliceIdx : slices) {
                // Invariant: WDSP channel id == slice index.
                RxChannel* rxCh = m_wdspEngine->rxChannel(sliceIdx);
                if (rxCh == nullptr) { continue; }

                QVector<float> outI(inSize);
                QVector<float> outQ(inSize);
                rxCh->processIq(acc.i.data(), acc.q.data(),
                                outI.data(), outQ.data(), inSize, outSize);

                // RADE owns one channel and one speaker path, so it stays
                // bound to slice 0 until multi-slice RADE lands (v0.5.2
                // documented limitation). Without this gate a secondary
                // slice would steal the fork and silence its own audio.
                RadeChannel* radeCh =
                    (sliceIdx == 0)
                        ? m_radeChannel.load(std::memory_order_acquire)
                        : nullptr;

                if (radeCh == nullptr) {
                    if (m_interleavedOut.size() < outSize * 2) {
                        m_interleavedOut.resize(outSize * 2);
                    }
                    float* interleaved = m_interleavedOut.data();
                    for (int i = 0; i < outSize; ++i) {
                        interleaved[i * 2 + 0] = outI[i];
                        interleaved[i * 2 + 1] = outQ[i];
                    }
                    // MasterMixer sums every registered slice into the one
                    // global output (design doc §3 "Audio routing").
                    m_audioEngine->rxBlockReady(sliceIdx, interleaved, outSize);
                }

                emit sliceProcessed(sliceIdx, inSize);
            }
        } else {
            // No engines wired (tests): still honour the fan-out contract
            // so the signal sequence is observable.
            for (int sliceIdx : slices) {
                emit sliceProcessed(sliceIdx, inSize);
            }
        }

        acc.i.remove(0, inSize);
        acc.q.remove(0, inSize);
        emit chunkDrained(inSize);
        emit chunkDrainedForStream(receiverIndex, inSize);
    }
```

Preserve the existing RADE decimation block and the anti-VOX fork that follow, moving them inside the `sliceIdx == 0` path since both are single-slice by design today.

- [ ] **Step 5: Implement the new slots**

```cpp
void RxDspWorker::setStreamSlices(int streamIndex,
                                  const QVector<int>& sliceIndices)
{
    m_streamSlices[streamIndex] = sliceIndices;
}

void RxDspWorker::resetStreams()
{
    m_accums.clear();
    m_streamSlices.clear();
}
```

- [ ] **Step 6: Run test to verify it passes**

```bash
cmake --build $W/build --target tst_rx_dsp_worker_multi_slice -j8 && ctest --test-dir $W/build -R tst_rx_dsp_worker_multi_slice --output-on-failure
```

Expected: PASS, 4 of 4 tests.

- [ ] **Step 7: Verify the working audio path is unchanged**

```bash
ctest --test-dir $W/build -R "rx_dsp|audio|slice|rade" --output-on-failure
```

Expected: all previously-passing tests still pass.

- [ ] **Step 8: Commit**

```bash
git add src/models/RxDspWorker.h src/models/RxDspWorker.cpp tests/tst_rx_dsp_worker_multi_slice.cpp tests/CMakeLists.txt
git commit -S -m "fix(3f-i): RxDspWorker per-stream accumulate, per-slice fan-out

processIqBatch accepted a receiverIndex and ignored it: one shared
accumulator and a hardcoded rxChannel(0). Any second DDC would have
appended into Slice A's stream and corrupted its audio.

Accumulation is now keyed by DDC stream, and each drained chunk is fed
to every slice bound to that stream through its own WDSP channel. This
is ChannelMaster's one-_rcvr-many-subrx topology (cmaster.h:75-82):
shared I/Q and noise blanker, independent audio per slice.

RADE and the anti-VOX fork stay gated to slice 0.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 5: Pre-allocate streams and channels at connect

**Files:**
- Modify: `src/models/RadioModel.h`, `src/models/RadioModel.cpp` (`connectToRadio`, near line 4004)
- Create: `tests/tst_stream_pool_binding.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/tst_stream_pool_binding.cpp`:

```cpp
// =================================================================
// tests/tst_stream_pool_binding.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic I Task 5-7: stream pool + slice binding.
// =================================================================
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestStreamPoolBinding : public QObject {
    Q_OBJECT
private slots:
    void pool_sizes_to_the_sku()
    {
        RadioModel model;
        model.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5,
                                  /*defaultRateHz*/ 192000);
        QCOMPARE(model.streamPoolSize(), 5);
    }

    void first_slice_activates_stream_zero()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int idx = model.addSlice();
        SliceModel* s = model.slices().at(idx);

        QCOMPARE(s->streamIndex(), 0);
        QCOMPARE(s->shiftOffsetHz(), 0.0);
    }

    void same_band_slices_share_one_stream()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14225000.0);

        QCOMPARE(model.slices().at(b)->streamIndex(), 0);
        QCOMPARE(model.slices().at(b)->shiftOffsetHz(), 25000.0);
        QCOMPARE(model.activeStreamCount(), 1);
    }

    void cross_band_slices_take_separate_streams()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        QVERIFY(model.slices().at(b)->streamIndex() != 0);
        QCOMPARE(model.activeStreamCount(), 2);
    }

    void four_slices_fit_one_ddc_on_one_band()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const double base = 14200000.0;
        for (double off : {0.0, -40000.0, 25000.0, 60000.0}) {
            const int i = model.addSlice();
            model.slices().at(i)->setFrequency(base + off);
        }

        QCOMPARE(model.activeStreamCount(), 1);
        for (SliceModel* s : model.slices()) {
            QCOMPARE(s->streamIndex(), 0);
        }
    }

    void exhausting_ddcs_rejects_with_a_reason()
    {
        RadioModel model;
        // One DDC, but room for several slices: the second slice on a
        // different band has nowhere to go.
        model.configureStreamPool(/*userDdcCount*/ 1, /*maxSlices*/ 5, 192000);

        QSignalSpy spy(&model, &RadioModel::sliceAddRejected);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(7150000.0);

        QVERIFY(spy.count() >= 1);
    }
};

QTEST_MAIN(TestStreamPoolBinding)
#include "tst_stream_pool_binding.moc"
```

Append to `tests/CMakeLists.txt`:

```cmake
nereus_add_test(tst_stream_pool_binding)
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build $W/build --target tst_stream_pool_binding -j8
```

Expected: FAIL to compile, `no member named 'configureStreamPool'`.

- [ ] **Step 3: Declare the pool API**

In `src/models/RadioModel.h`, public section near `maxSlices()` (line 328):

```cpp
    // ── Phase 3F Sub-Epic I: DDC stream pool ────────────────────────────
    //
    // A stream is one hardware DDC plus its ReceiverManager receiver, its
    // FFTEngine, its panadapter window, and its noise blanker. Streams are
    // opened once at connect and reused, mirroring Thetis CreateRadio
    // (cmaster.cs:516 [v2.10.3.15]) and deskhpsdr's create-all
    // loop (radio.c:1259 [@f3d857c]).
    //
    // Slices bind many-to-one: several whose frequencies fall inside one
    // window share it, differing only by shift offset. An idle stream
    // costs memory; its DDC stays out of the ddcEnable bitmask so the
    // radio never streams it.

    /// Size the pool to the connected SKU and clear all bindings.
    void configureStreamPool(int userDdcCount, int maxSlices,
                             int defaultRateHz);

    int streamPoolSize() const;
    int activeStreamCount() const;

    /// Slice indices currently bound to a stream, in ascending order.
    QVector<int> slicesOnStream(int streamIndex) const;

signals:
    /// Phase 3F Sub-Epic I: a stream's slice set changed. MainWindow
    /// rebuilds FFT routing and RadioModel republishes to RxDspWorker.
    void streamBindingsChanged(int streamIndex, const QVector<int>& sliceIndices);

    /// A stream was activated or retuned; its FFTEngine and panadapter
    /// window must follow.
    void streamCentreChanged(int streamIndex, double centreHz, int sampleRateHz);
```

Private:

```cpp
    NereusSDR::SliceStreamAllocator m_streamAllocator;

    /// Push the current slice set for `streamIndex` to RxDspWorker and
    /// emit streamBindingsChanged. Called after every bind / unbind.
    void republishStreamBindings(int streamIndex);
```

Add `#include "core/SliceStreamAllocator.h"`.

- [ ] **Step 4: Implement the pool**

In `src/models/RadioModel.cpp`:

```cpp
// ── Phase 3F Sub-Epic I: DDC stream pool ───────────────────────────────

void RadioModel::configureStreamPool(int userDdcCount, int maxSlices,
                                     int defaultRateHz)
{
    m_streamAllocator.configure(userDdcCount, maxSlices);
    m_streamAllocator.setDefaultSampleRateHz(defaultRateHz);
}

int RadioModel::streamPoolSize() const
{
    return m_streamAllocator.streamCount();
}

int RadioModel::activeStreamCount() const
{
    return m_streamAllocator.activeStreamCount();
}

QVector<int> RadioModel::slicesOnStream(int streamIndex) const
{
    QVector<int> out;
    for (int i = 0; i < m_slices.size(); ++i) {
        if (m_slices.at(i) && m_slices.at(i)->streamIndex() == streamIndex) {
            out.append(i);
        }
    }
    return out;
}

void RadioModel::republishStreamBindings(int streamIndex)
{
    const QVector<int> bound = slicesOnStream(streamIndex);
    if (m_dspWorker) {
        QMetaObject::invokeMethod(m_dspWorker, "setStreamSlices",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, streamIndex),
                                  Q_ARG(QVector<int>, bound));
    }
    emit streamBindingsChanged(streamIndex, bound);
}
```

- [ ] **Step 5: Open the pool's hardware at connect**

In `connectToRadio`, immediately after the existing `createRxChannel(0, ...)` block near line 4004:

```cpp
    // ── Phase 3F Sub-Epic I: open the stream pool and the channel pool ──
    //
    // Two pools with different sizes, because slices can share a DDC:
    //   streams  = caps.userDdcCount   (one per hardware DDC we may use)
    //   channels = caps.maxSlices      (one WDSP channel per slice)
    //
    // Thetis opens all 10 RX channels in CreateRadio (cmaster.cs:516
    // [v2.10.3.15]); deskhpsdr opens every receiver in one
    // loop (radio.c:1259 [@f3d857c]). Neither opens a channel at runtime.
    configureStreamPool(caps.userDdcCount, maxSlices(), wdspInputRate);

    for (int ch = 1; ch < maxSlices(); ++ch) {
        if (!m_wdspEngine->rxChannel(ch)) {
            m_wdspEngine->createRxChannel(ch, wdspInSize, 4096,
                                          wdspInputRate, 48000, 48000);
        }
    }
    for (int st = 1; st < caps.userDdcCount; ++st) {
        if (m_receiverManager->receiverConfig(st).receiverIndex < 0) {
            m_receiverManager->createReceiver();
        }
    }
    qCInfo(lcConnection) << "Sub-Epic I: streams=" << caps.userDdcCount
                         << "channels=" << maxSlices();
```

- [ ] **Step 6: Run test to verify it passes**

```bash
cmake --build $W/build --target tst_stream_pool_binding -j8 && ctest --test-dir $W/build -R tst_stream_pool_binding --output-on-failure
```

Expected: FAIL still on the binding tests, which Task 6 implements. `pool_sizes_to_the_sku` must pass. Record which pass.

- [ ] **Step 7: Commit**

```bash
git add src/models/RadioModel.h src/models/RadioModel.cpp src/core/SliceStreamAllocator.h tests/tst_stream_pool_binding.cpp tests/CMakeLists.txt
git commit -S -m "feat(3f-i): pre-allocate DDC stream pool and WDSP channel pool

Two pools with different sizes: userDdcCount streams (one per hardware
DDC) and maxSlices WDSP channels (one per slice), because slices can
share a DDC. Both opened once at connect per Thetis CreateRadio and
deskhpsdr's create-all loop.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 6: Bind slices through the allocator

**Files:**
- Modify: `src/models/RadioModel.cpp` (`addSlice` line 2847, `removeSlice` line 2913)
- Test: `tests/tst_stream_pool_binding.cpp` (already written in Task 5)

- [ ] **Step 1: Run the Task 5 tests to confirm which still fail**

```bash
ctest --test-dir $W/build -R tst_stream_pool_binding --output-on-failure
```

Expected: `first_slice_activates_stream_zero`, `same_band_slices_share_one_stream`, `cross_band_slices_take_separate_streams`, `four_slices_fit_one_ddc_on_one_band`, `exhausting_ddcs_rejects_with_a_reason` all FAIL.

- [ ] **Step 2: Add the bind helper**

In `src/models/RadioModel.h` private:

```cpp
    /// Phase 3F Sub-Epic I: run the allocator for `slice` at `frequencyHz`
    /// and apply the result (stream binding, shift offset, stream centre,
    /// codec recompute). Returns false and emits sliceAddRejected when the
    /// hardware has no room.
    bool bindSliceToStream(SliceModel* slice, double frequencyHz);
```

In `src/models/RadioModel.cpp`:

```cpp
bool RadioModel::bindSliceToStream(SliceModel* slice, double frequencyHz)
{
    if (!slice) { return false; }

    const int  previousStream = slice->streamIndex();
    const bool soleOccupant   =
        previousStream >= 0 && slicesOnStream(previousStream).size() == 1;

    const auto placement =
        (previousStream < 0)
            ? m_streamAllocator.placeSlice(frequencyHz)
            : m_streamAllocator.retuneSlice(previousStream, soleOccupant,
                                            frequencyHz);

    using Outcome = NereusSDR::SliceStreamAllocator::Outcome;

    if (placement.outcome == Outcome::Rejected) {
        emit sliceAddRejected(placement.reason);
        return false;
    }

    if (placement.outcome == Outcome::NewStream
        || placement.outcome == Outcome::RetunedStream) {
        // Claim or move the DDC, then centre it on the slice.
        m_streamAllocator.activateStream(
            placement.streamIndex, placement.newStreamCentreHz,
            m_connectionSampleRateHz > 0 ? m_connectionSampleRateHz : 192000);

        if (m_receiverManager) {
            m_receiverManager->setReceiverFrequency(
                placement.streamIndex,
                static_cast<quint64>(placement.newStreamCentreHz));
        }
        emit streamCentreChanged(
            placement.streamIndex, placement.newStreamCentreHz,
            m_streamAllocator.streamSampleRateHz(placement.streamIndex));
    }

    slice->setStreamIndex(placement.streamIndex);
    slice->setShiftOffsetHz(placement.shiftOffsetHz);

    // Push the offset into WDSP. RxChannel::setShiftFrequency is the
    // Thetis RXOsc port (radio.cs:1409-1425 [v2.10.3.15]):
    // SetRXAShiftFreq + RXANBPSetShiftFrequency.
    if (m_wdspEngine) {
        if (RxChannel* ch = m_wdspEngine->rxChannel(slice->sliceIndex())) {
            ch->setShiftFrequency(placement.shiftOffsetHz);
        }
    }

    // A stream the slice just left may now be empty.
    if (previousStream >= 0 && previousStream != placement.streamIndex) {
        if (slicesOnStream(previousStream).isEmpty()) {
            m_streamAllocator.deactivateStream(previousStream);
        }
        republishStreamBindings(previousStream);
    }
    republishStreamBindings(placement.streamIndex);

    // The active-DDC set may have changed, so the codec must recompute.
    requestDdcAssignment();
    return true;
}
```

- [ ] **Step 3: Call it from addSlice**

In `addSlice`, after `m_slices.append(slice);` (line 2865):

```cpp
    // ── Phase 3F Sub-Epic I Task 6: bind to a DDC stream ────────────────
    //
    // A fresh SliceModel carries the 14.225 MHz ctor default
    // (SliceModel.h:916), which on the bench read as "Slice C is stuck on
    // 20 m". Seed from the active slice first so a new slice opens on the
    // band the operator is working (and therefore usually shares the
    // active slice's DDC, costing no extra hardware), then bind.
    if (m_activeSlice && m_activeSlice != slice) {
        slice->setFrequency(m_activeSlice->frequency());
        slice->setDspMode(m_activeSlice->dspMode());
    }
    bindSliceToStream(slice, slice->frequency());

    // Retuning re-runs the allocator: the slice may stay on its stream
    // (shift only), move its stream's centre if it is the sole occupant,
    // or migrate to another DDC.
    connect(slice, &SliceModel::frequencyChanged, this,
            [this, slice](double freq) { bindSliceToStream(slice, freq); });
```

Remove the now-superseded frequency wiring inside `wireSliceSignals()` (lines 6831-6836), leaving only the active-slice FreeDV Reporter publish:

```cpp
    // Sub-Epic I Task 6: the hardware push moved into bindSliceToStream so
    // every slice gets it. What stays here is active-slice-only reporting:
    // FreeDV Reporter advertises the operator's listening frequency.
    connect(slice, &SliceModel::frequencyChanged, this, [this](double freq) {
        if (m_freeDvReporter && m_freeDvReporter->isConnected()) {
            publishFreedvFrequencyDwelled(static_cast<quint64>(freq));
        }
    });
```

- [ ] **Step 4: Release in removeSlice**

In `removeSlice`, after `SliceModel* slice = m_slices.takeAt(index);` (line 2936):

```cpp
    // Phase 3F Sub-Epic I Task 6: free the stream if this was its last
    // slice, so the DDC drops out of the ddcEnable bitmask and the radio
    // stops streaming it. The WDSP channel stays open for reuse.
    const int freedStream = slice->streamIndex();
    slice->setStreamIndex(-1);
    if (freedStream >= 0 && slicesOnStream(freedStream).isEmpty()) {
        m_streamAllocator.deactivateStream(freedStream);
    }
    if (freedStream >= 0) {
        republishStreamBindings(freedStream);
    }
    requestDdcAssignment();
```

- [ ] **Step 5: Run tests to verify they pass**

```bash
cmake --build $W/build --target tst_stream_pool_binding -j8 && ctest --test-dir $W/build -R tst_stream_pool_binding --output-on-failure
```

Expected: PASS, 6 of 6 tests.

- [ ] **Step 6: Commit**

```bash
git add src/models/RadioModel.h src/models/RadioModel.cpp
git commit -S -m "feat(3f-i): bind slices to DDC streams through the allocator

addSlice and every frequency change route through bindSliceToStream,
which shares an active DDC when the frequency fits its window, claims a
free DDC when it does not, moves a stream's centre when the slice is its
sole occupant, and rejects with a hardware-limit reason when nothing
fits. Shift offsets go to WDSP via RxChannel::setShiftFrequency.

New slices also seed frequency and mode from the active slice instead of
sitting on SliceModel's 14.225 MHz ctor default, which is what made
Slice C look stuck on 20 m.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 7: Wire the codec DDC assignment

**Files:**
- Modify: `src/core/DdcAssignment.h`, `src/core/codec/*.cpp`, `src/models/RadioModel.cpp`
- Test: `tests/tst_codec_5_slice_assignment.cpp` (extend)

- [ ] **Step 1: Write the failing test**

Append to `tests/tst_codec_5_slice_assignment.cpp`:

```cpp
    void saturn_publishes_per_slice_ddc_mapping()
    {
        P2CodecSaturn codec;
        CodecContext ctx{};
        std::array<SliceConfig, 5> slices{};
        slices[0].live = true; slices[0].sampleRateHz = 192000;
        slices[2].live = true; slices[2].sampleRateHz = 192000;

        const DdcAssignment a = codec.applyDdcAssignment(ctx, slices);

        QCOMPARE(a.sliceDdc[0], 2);    // Slice A -> DDC2
        QCOMPARE(a.sliceDdc[1], -1);   // not live
        QCOMPARE(a.sliceDdc[2], 4);    // Slice C -> DDC4
        // Every published DDC must also be enabled in the bitmask.
        QVERIFY((a.ddcEnable >> a.sliceDdc[0]) & 1);
        QVERIFY((a.ddcEnable >> a.sliceDdc[2]) & 1);
    }
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build $W/build --target tst_codec_5_slice_assignment -j8
```

Expected: FAIL to compile, `no member named 'sliceDdc'`.

- [ ] **Step 3: Add the field**

In `src/core/DdcAssignment.h`, inside `struct DdcAssignment`:

```cpp
    // Phase 3F Sub-Epic I Task 7: per-slice DDC choice. Index = slice
    // index, value = DDC number, -1 when that slice is not live. The wire
    // bytes above are derived from this; keeping it explicit lets
    // RadioModel publish the mapping onto each SliceModel without
    // re-deriving it from the enable bitmask.
    int sliceDdc[5] = {-1, -1, -1, -1, -1};
```

- [ ] **Step 4: Populate it in every codec**

Each codec already runs a live-slice loop that computes the DDC. In `src/core/codec/P2CodecSaturn.cpp:227-241` it reads:

```cpp
    for (int i = 0; i < 5; ++i) {
        if (!slices[i].live) { continue; }
        const int ddc = kSliceToDdc[i];
        a.ddcEnable |= (1 << ddc);
        a.rate[ddc] = slices[i].sampleRateHz;
        ++a.nDdc;
    }
```

Add one line immediately after `const int ddc = kSliceToDdc[i];`:

```cpp
        // Phase 3F Sub-Epic I Task 7: publish the mapping explicitly.
        a.sliceDdc[i] = ddc;
```

Apply the same addition inside the equivalent loop in each remaining codec. Locate them with:

```bash
grep -n "kSliceToDdc\|slices\[i\].live" $W/src/core/codec/*.cpp
```

Codecs to cover: `P2CodecSaturn`, `P2CodecOrionMkII`, `P1CodecStandard`, `P1CodecHl2`, `P1CodecAnvelinaPro3`, `P1CodecRedPitaya`.

- [ ] **Step 5: Call the codec and publish back**

In `src/models/RadioModel.h`, signals:

```cpp
    // Phase 3F Sub-Epic I Task 7: emitted whenever the slice or stream set
    // changes such that the codec must recompute. Test seam;
    // invokeCodecDdcAssignment does the work and no-ops while disconnected.
    void ddcAssignmentRequested();
```

Private method:

```cpp
    void requestDdcAssignment();
```

Implementation:

```cpp
void RadioModel::requestDdcAssignment()
{
    emit ddcAssignmentRequested();
    invokeCodecDdcAssignment();
}
```

In `invokeCodecDdcAssignment`, after `p2conn->applyDdcAssignment(assignment);`:

```cpp
            // Phase 3F Sub-Epic I Task 7: publish the codec's choice back
            // onto each SliceModel. SliceModel::setDdcIndex had zero
            // callers before this, so slice->ddcIndex() was permanently -1.
            for (int i = 0; i < m_slices.size() && i < 5; ++i) {
                if (SliceModel* s = m_slices.at(i)) {
                    s->setDdcIndex(assignment.sliceDdc[i]);
                }
            }
```

- [ ] **Step 6: Run tests to verify they pass**

```bash
cmake --build $W/build -j8 && ctest --test-dir $W/build -R "codec_5_slice|stream_pool" --output-on-failure
```

Expected: PASS on both.

- [ ] **Step 7: Commit**

```bash
git add src/core/DdcAssignment.h src/core/codec src/models/RadioModel.h src/models/RadioModel.cpp tests/tst_codec_5_slice_assignment.cpp
git commit -S -m "feat(3f-i): wire the codec DDC assignment path

invokeCodecDdcAssignment has had zero callers since Sub-Epic B landed,
so the radio was never told to enable a second DDC and the whole
per-board codec layer was unreachable. Call it on every slice or stream
change and publish the per-slice DDC choice back onto each SliceModel.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 8: One FFTEngine per stream

**Files:**
- Modify: `src/gui/MainWindow.h`, `src/gui/MainWindow.cpp:1732-1843`
- Modify: `src/models/RadioModel.h`, `src/models/RadioModel.cpp:6309`
- Create: `tests/tst_fft_engine_pool.cpp`
- Modify: `tests/CMakeLists.txt`

- [ ] **Step 1: Write the failing test**

Create `tests/tst_fft_engine_pool.cpp`:

```cpp
// =================================================================
// tests/tst_fft_engine_pool.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic I Task 8-9: per-stream FFT dispatch topology.
// =================================================================
#include <QtTest/QtTest>
#include "core/FFTRouter.h"

using namespace NereusSDR;

class TestFftEnginePool : public QObject {
    Q_OBJECT
private slots:
    void distinct_streams_reach_distinct_pans()
    {
        FFTRouter router;
        router.mapPanToReceiver(QStringLiteral("pan-0"), 0);
        router.mapPanToReceiver(QStringLiteral("pan-1"), 1);

        QCOMPARE(router.pansForReceiver(0),
                 QList<QString>{QStringLiteral("pan-0")});
        QCOMPARE(router.pansForReceiver(1),
                 QList<QString>{QStringLiteral("pan-1")});
    }

    void slices_sharing_a_stream_share_one_pan_subscription()
    {
        FFTRouter router;
        // Slices A and B both on stream 0, both shown on pan-0. The pan
        // must subscribe once, not twice, or it paints each frame twice.
        router.mapPanToReceiver(QStringLiteral("pan-0"), 0);
        router.mapPanToReceiver(QStringLiteral("pan-0"), 0);

        QCOMPARE(router.pansForReceiver(0).size(), 1);
    }

    void one_stream_can_feed_several_pans_at_different_zooms()
    {
        FFTRouter router;
        router.mapPanToReceiver(QStringLiteral("pan-0"), 0);
        router.mapPanToReceiver(QStringLiteral("pan-1"), 0);

        QCOMPARE(router.pansForReceiver(0).size(), 2);
    }

    void rebuild_drops_stale_subscriptions()
    {
        FFTRouter router;
        router.mapPanToReceiver(QStringLiteral("pan-0"), 0);
        router.mapPanToReceiver(QStringLiteral("pan-0"), 1);
        QCOMPARE(router.receiversForPan(QStringLiteral("pan-0")).size(), 2);

        // A full rebuild clears then re-adds only what the slice set says.
        router.removePan(QStringLiteral("pan-0"));
        router.mapPanToReceiver(QStringLiteral("pan-0"), 0);

        QCOMPARE(router.receiversForPan(QStringLiteral("pan-0")),
                 QList<int>{0});
    }
};

QTEST_MAIN(TestFftEnginePool)
#include "tst_fft_engine_pool.moc"
```

Append to `tests/CMakeLists.txt`:

```cmake
nereus_add_test(tst_fft_engine_pool)
```

- [ ] **Step 2: Run test to verify it passes or fails**

```bash
cmake --build $W/build --target tst_fft_engine_pool -j8 && ctest --test-dir $W/build -R tst_fft_engine_pool --output-on-failure
```

`slices_sharing_a_stream_share_one_pan_subscription` depends on `mapPanToReceiver` already de-duplicating, which `FFTRouter.cpp:16-18` does (`if (!list.contains(panId))`). Expect PASS. These four assertions pin the topology contract the rest of the task relies on; record the result and continue.

- [ ] **Step 3: Replace the single engine with a pool**

In `src/gui/MainWindow.h`, replace `FFTEngine* m_fftEngine{nullptr};` with:

```cpp
    // Phase 3F Sub-Epic I Task 8: one FFTEngine per DDC stream, keyed by
    // stream index. Before this there was a single FFTEngine(0) wired at
    // construction to activeSpectrumWidget(), which resolves to pan 0
    // permanently, so no secondary pan ever received a frame.
    //
    // One engine per STREAM, not per slice: the panadapter belongs to the
    // DDC (ChannelMaster `_rcvr.run_pan`, cmaster.h:79), so slices sharing
    // a DDC share its spectrum and appear as separate flags on it.
    //
    // All engines share m_fftThread. If a 5-stream 1536 kHz bench shows
    // the thread saturating, splitting to one thread per engine is a
    // follow-up needing maintainer sign-off (thread architecture).
    QMap<int, FFTEngine*> m_fftEngines;

    /// Stream 0's engine. Back-compat accessor for call sites that still
    /// address "the" FFT engine (display settings, Max Bin, auto-zoom).
    FFTEngine* primaryFftEngine() const { return m_fftEngines.value(0, nullptr); }

    /// Build and register one FFTEngine for `streamIndex`, configured as
    /// the old single-engine path was, moved onto the shared FFT thread.
    FFTEngine* createFftEngineForStream(int streamIndex);

    /// Re-derive the entire pan-to-stream topology from the current slice
    /// set. Cheap (one pass) and called on slice add / remove / migration
    /// / pan change. Chosen over incremental edits because a pan can host
    /// several slices, so no single change maps onto one subscription.
    void rebuildFftRouting();
```

Private slots:

```cpp
    /// Fan one stream's FFT frame out to every pan subscribed to it.
    void dispatchFftFrameToPans(int streamIndex,
                                const QVector<float>& binsLinear,
                                double windowEnb,
                                double dbmOffset);
```

- [ ] **Step 4: Implement the factory and the loop**

In `src/gui/MainWindow.cpp`:

```cpp
FFTEngine* MainWindow::createFftEngineForStream(int streamIndex)
{
    auto* engine = new FFTEngine(streamIndex);
    engine->setSampleRate(768000.0);
    engine->setFftSize(4096);

    auto& s = AppSettings::instance();
    const int persistedFps =
        s.value(QStringLiteral("DisplayFps"), QStringLiteral("30")).toString().toInt();
    engine->setOutputFps(persistedFps);

    const int persistedFftSize =
        s.value(QStringLiteral("DisplayFftSize"),
                QString::number(engine->fftSize())).toString().toInt();
    engine->setFftSizeBaseline(persistedFftSize);
    engine->setFftSize(persistedFftSize);

    const int defaultWin = static_cast<int>(engine->windowFunction());
    const int persistedWin =
        s.value(QStringLiteral("DisplayFftWindow"),
                QString::number(defaultWin)).toString().toInt();
    engine->setWindowFunction(static_cast<WindowFunction>(persistedWin));

    engine->moveToThread(m_fftThread);
    connect(m_fftThread, &QThread::finished, engine, &QObject::deleteLater);
    connect(engine, &FFTEngine::fftReadyLinear,
            this, &MainWindow::dispatchFftFrameToPans);

    m_fftEngines.insert(streamIndex, engine);
    return engine;
}
```

In `setupFftPipeline`, replace the single construction with:

```cpp
    // One engine per stream the SKU can host. Engines for idle streams
    // never receive I/Q, so they cost nothing at runtime.
    const int streamCount = m_radioModel ? m_radioModel->streamPoolSize() : 1;
    for (int st = 0; st < qMax(1, streamCount); ++st) {
        createFftEngineForStream(st);
    }
```

Delete the old `connect(m_fftEngine, &FFTEngine::fftReadyLinear, activeSpectrumWidget(), ...)` at lines 1837-1838. Update the `fftReady` subscribers at lines 2006 / 2015 / 2029 and `m_radioModel->setFftEngine(...)` at 1843 to use `primaryFftEngine()`. Find stragglers with:

```bash
cmake --build $W/build -j8 2>&1 | grep -n "m_fftEngine" | head -20
```

- [ ] **Step 5: Feed each engine from its own stream**

In `src/models/RadioModel.h` signals:

```cpp
    // Phase 3F Sub-Epic I Task 8: stream-tagged companion to rawIqData,
    // which is kept for existing single-slice subscribers.
    void rawIqDataForStream(int streamIndex, const QVector<float>& samples);
```

In `RadioModel.cpp`, the Step 2a fork at line 6309 currently discards the index:

```cpp
        Q_UNUSED(receiverIndex);
        emit rawIqData(samples);
```

Replace with:

```cpp
        emit rawIqData(samples);
        emit rawIqDataForStream(receiverIndex, samples);
```

In `MainWindow::setupFftPipeline`, replace the old single `connect(..., &RadioModel::rawIqData, m_fftEngine, &FFTEngine::feedIQ)` with:

```cpp
    connect(m_radioModel, &RadioModel::rawIqDataForStream, this,
            [this](int streamIndex, const QVector<float>& samples) {
        if (FFTEngine* engine = m_fftEngines.value(streamIndex, nullptr)) {
            QMetaObject::invokeMethod(engine, [engine, samples]() {
                engine->feedIQ(samples);
            }, Qt::QueuedConnection);
        }
    });
```

- [ ] **Step 6: Follow stream centre and rate changes**

```cpp
    connect(m_radioModel, &RadioModel::streamCentreChanged, this,
            [this](int streamIndex, double centreHz, int sampleRateHz) {
        if (FFTEngine* engine = m_fftEngines.value(streamIndex, nullptr)) {
            QMetaObject::invokeMethod(engine, [engine, sampleRateHz]() {
                engine->setSampleRate(static_cast<double>(sampleRateHz));
            }, Qt::QueuedConnection);
        }
        // The pans showing this stream must recentre with it.
        if (m_panStack && m_radioModel) {
            if (auto* router = m_radioModel->fftRouter()) {
                for (const QString& panId : router->pansForReceiver(streamIndex)) {
                    if (SpectrumWidget* sw = m_panStack->spectrum(panId)) {
                        sw->setDdcCenterFrequency(centreHz);
                        sw->setSampleRate(sampleRateHz);
                    }
                }
            }
        }
    });
```

- [ ] **Step 7: Commit**

```bash
git add src/gui/MainWindow.h src/gui/MainWindow.cpp src/models/RadioModel.h src/models/RadioModel.cpp tests/tst_fft_engine_pool.cpp tests/CMakeLists.txt
git commit -S -m "feat(3f-i): one FFTEngine per DDC stream

A single FFTEngine(0) was wired at construction to activeSpectrumWidget(),
which resolves to pan 0 permanently, so no secondary pan ever received a
frame. Build one engine per stream and feed each from its own DDC.

One engine per stream, not per slice: the panadapter belongs to the DDC
(ChannelMaster _rcvr.run_pan), so slices sharing a DDC share its spectrum
and appear as separate flags on it.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 9: Dispatch frames to subscribed pans

**Files:**
- Modify: `src/gui/MainWindow.cpp`

- [ ] **Step 1: Implement the dispatcher and the rebuild**

```cpp
void MainWindow::dispatchFftFrameToPans(int streamIndex,
                                        const QVector<float>& binsLinear,
                                        double windowEnb,
                                        double dbmOffset)
{
    if (!m_panStack || !m_radioModel) { return; }
    auto* router = m_radioModel->fftRouter();
    if (!router) { return; }

    // FFTRouter is the topology oracle rather than a signal hop:
    // pansForReceiver is public and unit-tested, and routing through its
    // own signal would add a queued hop on the render path for no gain.
    // One stream can feed N pans (different zoom levels of the same I/Q),
    // which is the AetherSDR overlay model the router was designed for.
    for (const QString& panId : router->pansForReceiver(streamIndex)) {
        if (SpectrumWidget* sw = m_panStack->spectrum(panId)) {
            sw->updateSpectrumLinear(streamIndex, binsLinear,
                                     windowEnb, dbmOffset);
        }
    }
}

void MainWindow::rebuildFftRouting()
{
    if (!m_radioModel) { return; }
    auto* router = m_radioModel->fftRouter();
    if (!router) { return; }

    // Clear every pan's subscriptions, then re-add from live slices.
    // PanadapterStack::allApplets (PanadapterStack.h:70) and
    // PanadapterApplet::panId (PanadapterApplet.h:65) both already exist.
    if (m_panStack) {
        for (auto* applet : m_panStack->allApplets()) {
            if (applet) { router->removePan(applet->panId()); }
        }
    }

    for (SliceModel* slice : m_radioModel->slices()) {
        if (!slice) { continue; }
        const int stream = slice->streamIndex();
        if (stream < 0) { continue; }          // unbound slice feeds nothing
        const QString panId = slice->panKey();
        if (panId.isEmpty()) { continue; }
        // mapPanToReceiver de-duplicates (FFTRouter.cpp:17), so two slices
        // sharing a stream and a pan produce one subscription, not two.
        router->mapPanToReceiver(panId, stream);
    }
}
```

- [ ] **Step 2: Call rebuildFftRouting from every topology change**

Replace the `mapPanToReceiver(targetPan, slice->ddcIndex())` call at `MainWindow.cpp:1619` with `rebuildFftRouting();`.

Add the same call as the last statement of:
- the `panKeyChanged` lambda at `MainWindow.cpp:1661`, after `createSliceFlag(slice, dest);`
- the `sliceRemoved` handler
- a new connection to `RadioModel::streamBindingsChanged`:

```cpp
    connect(m_radioModel, &RadioModel::streamBindingsChanged, this,
            [this](int, const QVector<int>&) { rebuildFftRouting(); });
```

- [ ] **Step 3: Build and run the full suite**

```bash
cmake --build $W/build -j8 && ctest --test-dir $W/build --output-on-failure 2>&1 | tail -20
```

Expected: full suite green.

- [ ] **Step 4: Bench check on the G2**

```bash
pkill -f NereusSDR; open $W/build/NereusSDR.app
```

Connect to the G2, then walk both sharing modes:

**Shared DDC (A through D on one DDC):**
1. Slice A on 14.200 MHz.
2. Add Slices B, C, D at 14.180 / 14.225 / 14.260.
3. All four flags appear on pan 0, all four produce audio.
4. Bottom bar / Setup shows **one** active DDC.

**Separate DDCs:**
5. Retune Slice B to 7.150 MHz.
6. Slice B claims a second DDC; a second pan shows 40 m and animates.
7. Slices A, C, D stay on the first DDC, still on 20 m.
8. Tuning Slice C moves only its flag, not pan 0's centre.

**Hardware limit:**
9. Keep adding cross-band slices until the DDCs run out; confirm the rejection message names the limit rather than failing silently.

- [ ] **Step 5: Commit**

```bash
git add src/gui/MainWindow.cpp
git commit -S -m "feat(3f-i): dispatch FFT frames to subscribed pans

MainWindow::dispatchFftFrameToPans consults the FFTRouter topology and
pushes each stream's frame to every pan subscribed to it. FFTRouter's map
was populated but never read; onFftFrame had no callers outside tests.

Topology is re-derived wholesale by rebuildFftRouting on any slice or
binding change, because a pan can host several slices and no single
add or remove maps cleanly onto one subscription edit.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 10: Per-slice sample rate and the P1 shared-rate clamp

**Files:**
- Modify: `src/models/RadioModel.cpp` (`bindSliceToStream`)
- Test: `tests/tst_stream_pool_binding.cpp` (extend)

- [ ] **Step 1: Write the failing test**

```cpp
    void changing_a_stream_rate_rewidens_its_window()
    {
        RadioModel model;
        model.configureStreamPool(5, 5, 192000);

        const int a = model.addSlice();
        model.slices().at(a)->setFrequency(14200000.0);

        // 14.400 is outside +-96 kHz but inside +-384 kHz.
        model.setStreamSampleRate(0, 768000);

        const int b = model.addSlice();
        model.slices().at(b)->setFrequency(14400000.0);

        QCOMPARE(model.slices().at(b)->streamIndex(), 0);
        QCOMPARE(model.activeStreamCount(), 1);
    }
```

- [ ] **Step 2: Run test to verify it fails**

```bash
cmake --build $W/build --target tst_stream_pool_binding -j8
```

Expected: FAIL to compile, `no member named 'setStreamSampleRate'`.

- [ ] **Step 3: Implement**

In `src/models/RadioModel.h` public:

```cpp
    /// Phase 3F Sub-Epic I Task 10: change a stream's DDC sample rate.
    /// Widening the window can let more slices share this stream;
    /// narrowing it can push slices out, so every slice on the stream is
    /// re-run through the allocator afterwards.
    ///
    /// On Protocol 1 all receivers share one rate (deskhpsdr radio.c:
    /// "Make sure RX2 shares the sample rate with RX1 when running P1"),
    /// so this applies the rate to every stream on a P1 connection.
    void setStreamSampleRate(int streamIndex, int rateHz);
```

```cpp
void RadioModel::setStreamSampleRate(int streamIndex, int rateHz)
{
    const bool isP1 =
        qobject_cast<NereusSDR::P1RadioConnection*>(m_connection) != nullptr;

    auto applyTo = [this, rateHz](int st) {
        m_streamAllocator.activateStream(
            st, m_streamAllocator.streamCentreHz(st), rateHz);
        emit streamCentreChanged(st, m_streamAllocator.streamCentreHz(st),
                                 rateHz);
    };

    if (isP1) {
        // P1 has one rate for every receiver.
        for (int st = 0; st < m_streamAllocator.streamCount(); ++st) {
            if (m_streamAllocator.isStreamActive(st)) { applyTo(st); }
        }
    } else {
        applyTo(streamIndex);
    }

    // A narrower window may have pushed slices out; re-run each through
    // the allocator so they migrate or claim a DDC as needed.
    for (SliceModel* s : m_slices) {
        if (s && s->streamIndex() >= 0) {
            bindSliceToStream(s, s->frequency());
        }
    }
    requestDdcAssignment();
}
```

- [ ] **Step 4: Run test to verify it passes**

```bash
cmake --build $W/build --target tst_stream_pool_binding -j8 && ctest --test-dir $W/build -R tst_stream_pool_binding --output-on-failure
```

Expected: PASS, 7 of 7 tests.

- [ ] **Step 5: Commit**

```bash
git add src/models/RadioModel.h src/models/RadioModel.cpp tests/tst_stream_pool_binding.cpp
git commit -S -m "feat(3f-i): per-stream sample rate with P1 shared-rate clamp

Widening a stream's window lets more slices share it; narrowing pushes
slices out, so every slice on the stream is re-run through the allocator.
P1 applies one rate to all receivers, matching deskhpsdr's explicit
same-rate enforcement.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 11: Amend the design doc

**Files:**
- Modify: `docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md:93-95`

- [ ] **Step 1: Replace the §3 lifecycle opening**

Replace:

```markdown
### Lifecycle: on-demand (AetherSDR pattern)

Slices are created and destroyed by operator action, not pre-allocated. Mechanics:
```

with:

```markdown
### Lifecycle: on-demand slices over a pre-allocated stream pool

**Amended 2026-07-24 (Sub-Epic I).** The original text read "Slices are
created and destroyed by operator action, not pre-allocated," which
conflated the operator-visible lifecycle with the implementation
mechanics, and it assumed one slice per DDC. Both are corrected here.

**Operator-visible lifecycle (unchanged, AetherSDR pattern):** slices are
created and destroyed by operator action, letters are assigned in creation
order and freed on destroy, and the cap check rejects past `maxSlices`.

**Implementation (corrected, Thetis / deskhpsdr pattern).** Two pools open
at connect and are never resized at runtime:

- `userDdcCount` **DDC streams**, each one hardware DDC plus its
  ReceiverManager receiver, FFTEngine, panadapter window, and noise
  blanker.
- `maxSlices` **WDSP RX channels**, one per possible slice.

Slices bind to streams **many-to-one**. A slice whose frequency falls
inside an active stream's window joins it and is tuned by a shift offset
(`RxChannel::setShiftFrequency`, the Thetis `RXOsc` port at
`radio.cs:1409-1425 [v2.10.3.15]`). A slice that fits no
active window claims a free DDC. When no DDC is free and none fits, the
add is rejected with a message naming the limit.

What slices sharing a stream share, and what they do not, is fixed by
ChannelMaster's `struct _rcvr` (`cmaster.h:75-82
[v2.10.3.15]`): one I/Q input, one noise blanker (`panb` /
`pnob`), one panadapter (`run_pan`), but `audio[cmMAXSubRcvr]`, an
independent audio output per slice. So co-hosted slices share the
spectrum window and the noise blanker, and keep their own mode, filter,
AGC, shift and audio.

Why pre-allocation: the original model has no upstream to port from.
Thetis opens all 10 RX channels plus TX in `CreateRadio()`
(`cmaster.cs:502-516`, `cmRCVR=5`, `cmSubRCVR=2`), and deskhpsdr opens
every receiver in one loop at startup (`radio.c:1256-1259 [@f3d857c]`).
Neither touches a WDSP channel at runtime. AetherSDR's on-demand model is
right for AetherSDR because its slices live on the FlexRadio server;
NereusSDR owns the DSP locally, so on-demand would mean opening a WDSP
channel on a UI click. That is also the pattern that crashed in PR #219,
where destroying live channels invalidated seven raw-pointer holders.

**Documented divergences from Thetis:**

| Aspect | Thetis | NereusSDR | Why |
|---|---|---|---|
| Slices per stream | 2 (`cmSubRCVR`) | up to `maxSlices` | Product requirement: A through D on one DDC. WDSP imposes no such cap (`MAX_CHANNELS 32`); `cmSubRCVR` is a Thetis structure choice. |
| User DDC streams | 2 in practice (RX1 / RX2) | every user DDC the SKU exposes (5 on G2) | Thetis leaves DDC4-6 idle on Saturn-class; our codec already maps them (`P2CodecSaturn` `kSliceToDdc = {2,3,4,5,6}`). |
| Slice leaves its window | disables Multi-RX (`console.cs:31924`) | promote to a free DDC; reject only when none free | Better operator outcome, and we have DDCs Thetis never uses. |

Idle streams cost memory only. Their DDCs stay out of the `ddcEnable`
bitmask, so the radio never streams them, exactly as Thetis's
`UpdateDDCs` gates its pre-opened channels.

Mechanics:
```

- [ ] **Step 2: Commit**

```bash
git add docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md
git commit -S -m "docs(3f-i): amend design doc §3 for the stream pool model

The original §3 conflated operator-visible lifecycle with implementation
mechanics and assumed one slice per DDC. Operator behaviour is unchanged;
slices now bind many-to-one onto a pre-allocated DDC stream pool. Records
the three divergences from Thetis.

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Task 12: Extend the bench matrix and record results

**Files:**
- Modify: `docs/architecture/2026-05-26-phase3f-verification/README.md`
- Modify: `docs/architecture/2026-05-26-phase3f-verification/g2-results.md`

- [ ] **Step 1: Add rows for DDC sharing**

Append to the matrix, numbering from the current last row:

```markdown
| 48 | Two same-band slices share one DDC (one active DDC) | [ ] | n/a | [ ] | [ ] | |
| 49 | Four slices (A-D) share one DDC on one band | [ ] | n/a | [ ] | n/a (2-slice cap) | |
| 50 | Co-hosted slices keep independent mode / filter / AGC | [ ] | n/a | [ ] | [ ] | |
| 51 | Co-hosted slices share the noise blanker (expected) | [ ] | n/a | [ ] | [ ] | per cmaster.h `_rcvr.panb` |
| 52 | Slice retuned out of window claims a second DDC | [ ] | n/a | [ ] | [ ] | |
| 53 | Sole-occupant slice retunes its DDC rather than claiming one | [ ] | [ ] | [ ] | [ ] | |
| 54 | Widening a DDC rate re-admits an out-of-window slice | [ ] | [ ] | [ ] | n/a (192 cap) | |
| 55 | DDC exhaustion rejects with a message naming the limit | [ ] | [ ] | [ ] | [ ] | |
| 56 | All userDdcCount DDCs usable simultaneously | [ ] | n/a (1) | [ ] | [ ] | G2 = 5 |
| 57 | P1 rate change applies to every active receiver | n/a | [ ] | n/a | [ ] | deskhpsdr parity |
```

- [ ] **Step 2: Run the full regression sweep**

```bash
cmake --build $W/build -j8 && ctest --test-dir $W/build --output-on-failure 2>&1 | tail -20
```

Record the pass count.

- [ ] **Step 3: Update g2-results.md**

Move findings 4-7 into a "Session 3 (resolved)" table with the fixing commit for each, and replace the "Matrix rows blocked" section with the actual tick state from the bench walk in Task 9 Step 4.

- [ ] **Step 4: Commit**

```bash
git add docs/architecture/2026-05-26-phase3f-verification/
git commit -S -m "docs(3f-i): bench matrix rows for DDC sharing + Sub-Epic I results

Co-Authored-By: Claude Opus 5 <noreply@anthropic.com>"
```

---

## Deferred, with reasons

- **Per-stream DSP threads.** Every stream shares one `RxDspWorker` on one DSP thread, and all FFT engines share one FFT thread. Thetis runs a DSP thread per receiver. With N slices per stream the per-drain work is now N `processIq` calls, so this is the most likely place a 5-slice 1536 kHz G2 bench pushes back. Splitting is thread architecture and needs maintainer sign-off per CLAUDE.md.
- **Per-slice noise blanker.** NB is per stream by hardware topology (`cmaster.h:79-81`), so co-hosted slices share NB settings. Giving each slice its own would mean giving each its own DDC, defeating the sharing. Surface it in the UI (grey the NB controls on co-hosted slices with a tooltip) rather than pretending it is per-slice.
- **Multi-slice RADE.** Gated to slice 0, matching the v0.5.2 documented limitation.
- **Anti-VOX aamix.** Stays single-slice. The real aamix port was always scoped to arrive with multi-pan; it is unblocked now but out of scope here.
- **Sub-receiver audio panning.** Thetis pans sub-receivers left/right in the stereo field for dual-watch. `MasterMixer::setSliceGain(sliceId, gain, pan)` already supports it; wiring it to a UI control is a follow-up.

---

## Summary

| Task | Deliverable |
|---|---|
| 1 | `BoardCapabilities::userDdcCount` per SKU |
| 2 | `SliceStreamAllocator` placement policy (share / claim / retune / reject) |
| 3 | `SliceModel::streamIndex` + `shiftOffsetHz` |
| 4 | `RxDspWorker` per-stream accumulate, per-slice fan-out (corruption guard) |
| 5 | Stream pool + WDSP channel pool pre-allocated at connect |
| 6 | Slices bound through the allocator; shift offsets pushed to WDSP |
| 7 | Codec DDC assignment finally called; `ddcIndex` published |
| 8 | One `FFTEngine` per stream |
| 9 | Frames dispatched to subscribed pans (second pan animates) |
| 10 | Per-stream sample rate with P1 shared-rate clamp |
| 11 | Design doc §3 amended with the three divergences |
| 12 | Bench matrix rows 48-57 for sharing |

**Bench success criteria, both modes:**

1. **Shared:** Slices A through D on one band all sit on one DDC, all produce audio, all show as flags on one pan, and only one DDC reports active.
2. **Separate:** Retuning Slice B to another band gives it its own DDC and its own animating pan, while A, C and D stay put.
3. **Limit:** Adding cross-band slices past `userDdcCount` is rejected with a message naming the hardware limit, not a silent failure.
