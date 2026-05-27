# Phase 3F Sub-Epic F: Wideband Extended Pan — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Make the wideband ADC stream usable as the "extension" of any DDC pan when zoomed out beyond its native bandwidth. After this plan lands, operators can scroll-wheel a pan past ~1.5 MHz and see wideband data fill the wings while a listenable I/Q island stays in the centre. Click in the wings retunes the DDC; click in the centre retunes the slice within the existing DDC.

**Architecture:** `P2RadioConnection` gains `packetbuf[23]` wideband enable byte plumbing + actual receive-path decode (replaces stub at `P2RadioConnection.cpp:1608`). Per-ADC frame accumulator buffers 32 packets × 512 samples → 16384 real samples. New `WidebandFftEngine` (real-input FFTW3 r2c, 16384-pt). `SpectrumWidget::paintExtendedPan` composites listenable I/Q centre + wideband wings with dashed boundary indicators. Zoom-gesture coordination auto-bypasses Alex BPF on the active ADC.

**Tech Stack:** C++20, Qt6 (QPainter, QRhiWidget for spectrum, signals/slots), FFTW3 (real-to-complex plan), QtTest.

**Parent design:** [docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md](2026-05-26-phase3f-multi-pan-multi-slice-design.md) §7 (Wideband Extended Pan)

**Prereqs:** Sub-Epics A + B + C + D + E complete.

**Estimated effort:** 6 working days, 16 tasks, ~85 bite-sized steps.

---

## File Structure

### Files to create

| File | Purpose |
|---|---|
| `src/core/WidebandFftEngine.{h,cpp}` | Real-input FFTW3 16384-pt FFT, per-ADC |
| `src/core/WidebandFrameAccumulator.{h,cpp}` | Buffer 32 packets/frame, sequence-error handling |
| `tests/tst_wideband_frame_accumulator.cpp` | Frame assembly + sequence-error padding |
| `tests/tst_wideband_fft_engine.cpp` | FFT output bin count + Nyquist verification |
| `tests/tst_p2_wideband_enable_byte.cpp` | composeCmdGeneral writes packetbuf[23] correctly |

### Files to modify

| File | Purpose |
|---|---|
| `src/core/P2RadioConnection.h` | Add `m_wbEnableMask`, `setWidebandEnabled(adc, on)`, `widebandFrameReady(adc, samples)` signal |
| `src/core/P2RadioConnection.cpp` | Write `packetbuf[23]` in composeCmdGeneralLegacy; replace stub at line 1608 with real packet decode |
| `src/core/codec/CodecContext.h` | Add `p2WbEnableMask` field threaded from `P2RadioConnection::wbEnableMask()` |
| `src/core/codec/P2CodecOrionMkII.cpp` | Read `ctx.p2WbEnableMask` for `buf[23]` in `composeCmdGeneral` |
| `src/gui/SpectrumWidget.{h,cpp}` | `paintExtendedPan()` method with listenable island + wing rendering; zoom-state derives wideband-extension-requested |
| `src/models/SliceModel.{h,cpp}` | `widebandExtensionRequested` setter wired via zoom signal |
| `src/core/accessories/AlexController.cpp` | `setWidebandActive(adc, on)` propagates BPF bypass |
| `src/gui/PanadapterApplet.{h,cpp}` | Per-pan "Extended view" right-click toggle |

---

## Task 1: Add CmdGeneral byte 23 (wb_enable) wideband enable plumbing

**Files:** Modify `src/core/P2RadioConnection.{h,cpp}` (composeCmdGeneralLegacy + the codec-driven composeCmdGeneral), `src/core/codec/CodecContext.h`, `src/core/codec/P2CodecOrionMkII.cpp`

> **Plan revision note (2026-05-27):** Task 1 originally targeted composeCmdRx byte 23. That was wrong: CmdRx byte 23 is rx[1].rx_adc (RX1 ADC selector) per Thetis network.c:1118, not the wideband enable mask. The mask lives in CmdGeneral byte 23 per Thetis network.c:879. Caught by source-first audit during implementation.

The design identifies this gap explicitly: NereusSDR's composeCmdGeneralLegacy + the codec-driven composeCmdGeneral both hardcode buf[23]=0 with a // wb_enable placeholder. This task wires it to the real per-ADC mask. Thetis network.c:879 [v2.10.3.15] defines the byte; CmdRx byte 23 is unrelated (rx[1].rx_adc).

- [ ] **Step 1: Write failing test**

Create `tests/tst_p2_wideband_enable_byte.cpp`:

```cpp
#include <QtTest/QtTest>
#include "core/P2RadioConnection.h"

using namespace NereusSDR;

class TestP2WidebandEnableByte : public QObject {
    Q_OBJECT
private slots:
    void compose_cmd_general_writes_packetbuf_23_when_wideband_enabled()
    {
        P2RadioConnection conn;
        conn.setWidebandEnabled(0, true);  // enable ADC0 wideband

        quint8 buf[60] = {0};
        conn.composeCmdGeneralForTest(buf);

        // packetbuf[23] should have bit 0 set (ADC0 enabled).
        QCOMPARE(quint8(buf[23] & 0x01), quint8(0x01));
    }

    void compose_cmd_general_writes_0_when_no_wideband()
    {
        P2RadioConnection conn;
        quint8 buf[60] = {0};
        conn.composeCmdGeneralForTest(buf);
        QCOMPARE(quint8(buf[23]), quint8(0x00));
    }

    void per_adc_enable_independent()
    {
        P2RadioConnection conn;
        conn.setWidebandEnabled(0, true);
        conn.setWidebandEnabled(1, true);
        quint8 buf[60] = {0};
        conn.composeCmdGeneralForTest(buf);
        QCOMPARE(quint8(buf[23] & 0x03), quint8(0x03));  // both bits set
    }
};

QTEST_MAIN(TestP2WidebandEnableByte)
#include "tst_p2_wideband_enable_byte.moc"
```

Register: `nereus_add_test(tst_p2_wideband_enable_byte)`.

- [ ] **Step 2: Run + verify failure**

Expected: `'setWidebandEnabled' is not a member of 'P2RadioConnection'`.

- [ ] **Step 3: Add to P2RadioConnection.h**

```cpp
public slots:
    /// Phase 3F Sub-Epic F Task 1: enable the wideband ADC stream for the
    /// given ADC index. Bit N of m_wbEnableMask corresponds to ADCN.
    /// See Thetis network.c:879 [v2.10.3.15] for the wire format (CmdGeneral
    /// byte 23). Triggers CmdGeneral send when in Connected state.
    void setWidebandEnabled(int adcIndex, bool on);

public:
    /// Phase 3F Sub-Epic F Task 1: read current mask (for CodecContext threading).
    quint8 wbEnableMask() const { return m_wbEnableMask; }

private:
    quint8 m_wbEnableMask {0};  // Phase 3F Sub-Epic F Task 1
```

- [ ] **Step 4: Implement in P2RadioConnection.cpp**

```cpp
void P2RadioConnection::setWidebandEnabled(int adcIndex, bool on)
{
    if (adcIndex < 0 || adcIndex >= 8) { return; }
    const quint8 bit = quint8(1 << adcIndex);
    const quint8 newMask = on ? (m_wbEnableMask | bit) : (m_wbEnableMask & ~bit);
    if (newMask == m_wbEnableMask) { return; }
    m_wbEnableMask = newMask;
    if (m_state == ConnectionState::Connected) {
        sendCmdGeneral();
    }
}
```

In `composeCmdGeneralLegacy` (currently `buf[23] = 0; // wb_enable` at line ~2261), replace with:

```cpp
    // From Thetis network.c:879 [v2.10.3.15] - wb_enable mask, bit N = ADCN.
    buf[23] = char(m_wbEnableMask);
```

In `P2CodecOrionMkII::composeCmdGeneral` (currently `buf[23] = 0; // wb_enable` at line ~170), replace with:

```cpp
    // From Thetis network.c:879 [v2.10.3.15] - wb_enable mask, bit N = ADCN.
    buf[23] = quint8(ctx.p2WbEnableMask);
```

Extend `CodecContext` with `quint8 p2WbEnableMask {0};` and populate it in `P2RadioConnection::buildCodecContext` from `m_wbEnableMask`.

- [ ] **Step 5: Run + commit**

```bash
cmake --build build --target tst_p2_wideband_enable_byte && ctest --test-dir build -R tst_p2_wideband_enable_byte -V 2>&1 | tail -10
git add src/core/P2RadioConnection.{h,cpp} src/core/codec/CodecContext.h src/core/codec/P2CodecOrionMkII.cpp tests/tst_p2_wideband_enable_byte.cpp tests/CMakeLists.txt
git commit -m "feat(3f-f): P2 wideband enable byte (CmdGeneral byte 23) plumbing"
```

---

## Task 2: Create `WidebandFrameAccumulator`

**Files:** Create `src/core/WidebandFrameAccumulator.{h,cpp}`, `tests/tst_wideband_frame_accumulator.cpp`

Per design: 32 packets × 512 samples = 16384 floats per frame. Sequence errors zero-pad partial frames (Thetis network.c:572-600 [v2.10.3.15] behaviour).

- [ ] **Step 1: Write failing test**

```cpp
#include <QtTest/QtTest>
#include <QSignalSpy>
#include "core/WidebandFrameAccumulator.h"

using namespace NereusSDR;

class TestWidebandFrameAccumulator : public QObject {
    Q_OBJECT
private slots:
    void accumulates_32_packets_into_one_frame()
    {
        WidebandFrameAccumulator acc;
        QSignalSpy spy(&acc, &WidebandFrameAccumulator::frameReady);

        // Push 32 packets, each 512 16-bit BE samples worth of payload (raw bytes).
        // Each packet payload = 1024 bytes (512 × 2).
        QByteArray payload(1024, char(0x10));
        for (int seq = 0; seq < 32; ++seq) {
            acc.pushPacket(seq, payload);
        }

        QCOMPARE(spy.count(), 1);
        const auto frame = spy.first().first().value<QVector<float>>();
        QCOMPARE(frame.size(), 32 * 512);  // 16384
    }

    void sequence_error_zero_pads_partial_frame()
    {
        WidebandFrameAccumulator acc;
        QSignalSpy spy(&acc, &WidebandFrameAccumulator::frameReady);

        QByteArray payload(1024, char(0x10));
        acc.pushPacket(0, payload);
        acc.pushPacket(1, payload);
        // Skip 2, jump to 3 → sequence error
        acc.pushPacket(3, payload);
        // Accumulator should fire frameReady with zero-padded remainder
        // (Thetis behaviour: emit current state, reset)

        // Verify a frame was emitted (even partial)
        QVERIFY(spy.count() >= 1);
    }
};

QTEST_MAIN(TestWidebandFrameAccumulator)
#include "tst_wideband_frame_accumulator.moc"
```

Register: `nereus_add_test(tst_wideband_frame_accumulator)`.

- [ ] **Step 2: Create header**

```cpp
#pragma once
#include <QObject>
#include <QByteArray>
#include <QVector>

namespace NereusSDR {

class WidebandFrameAccumulator : public QObject {
    Q_OBJECT
public:
    explicit WidebandFrameAccumulator(QObject* parent = nullptr);
    void pushPacket(int sequenceNumber, const QByteArray& payload);

signals:
    void frameReady(QVector<float> samples);

private:
    static constexpr int kSamplesPerPacket = 512;
    static constexpr int kPacketsPerFrame = 32;
    static constexpr int kSamplesPerFrame = kSamplesPerPacket * kPacketsPerFrame;

    int             m_expectedSeq {0};
    QVector<float>  m_frameBuffer;  // current frame being assembled
    bool            m_inFrame {false};

    void emitAndReset();
    void decode512Samples(const QByteArray& payload, QVector<float>::iterator out);
};

} // namespace NereusSDR
```

- [ ] **Step 3: Implement**

```cpp
#include "core/WidebandFrameAccumulator.h"

namespace NereusSDR {

WidebandFrameAccumulator::WidebandFrameAccumulator(QObject* parent)
    : QObject(parent)
    , m_frameBuffer(kSamplesPerFrame, 0.0f)
{
}

void WidebandFrameAccumulator::pushPacket(int seq, const QByteArray& payload)
{
    if (payload.size() < kSamplesPerPacket * 2) { return; }  // malformed

    if (!m_inFrame) {
        if (seq != 0) { return; }  // wait for seq=0 to start a frame
        m_inFrame = true;
        m_expectedSeq = 0;
        m_frameBuffer.fill(0.0f);
    }

    if (seq != m_expectedSeq) {
        // Sequence error: zero-pad remainder, emit current frame, reset
        emitAndReset();
        return;
    }

    // Decode 512 16-bit BE samples → float [-1, 1] at offset (seq * 512)
    decode512Samples(payload, m_frameBuffer.begin() + (seq * kSamplesPerPacket));

    m_expectedSeq++;
    if (m_expectedSeq >= kPacketsPerFrame) {
        emitAndReset();
    }
}

void WidebandFrameAccumulator::emitAndReset()
{
    emit frameReady(m_frameBuffer);
    m_inFrame = false;
    m_expectedSeq = 0;
}

void WidebandFrameAccumulator::decode512Samples(const QByteArray& payload, QVector<float>::iterator out)
{
    // Thetis network.c:567-571 [v2.10.3.15]: 16-bit BE samples in upper 16 of 32-bit word,
    // divide by 2^31 for float [-1, 1]. Equivalent: (int16)(b0<<8 | b1) / 32768.0.
    const char* bytes = payload.constData();
    for (int i = 0; i < kSamplesPerPacket; ++i) {
        const qint16 sample = (qint16(quint8(bytes[i * 2])) << 8) | quint8(bytes[i * 2 + 1]);
        *out++ = float(sample) / 32768.0f;
    }
}

} // namespace NereusSDR
```

- [ ] **Step 4: Commit**

```bash
cmake --build build --target tst_wideband_frame_accumulator && ctest --test-dir build -R tst_wideband_frame_accumulator -V 2>&1 | tail -10
git add src/core/WidebandFrameAccumulator.{h,cpp} tests/tst_wideband_frame_accumulator.cpp tests/CMakeLists.txt src/core/CMakeLists.txt
git commit -m "feat(3f-f): WidebandFrameAccumulator (32-packet frame, seq-error padding)"
```

---

## Task 3: Replace stub wideband receive path in P2RadioConnection

**Files:** Modify `src/core/P2RadioConnection.{h,cpp}` (line 1608+ stub)

- [ ] **Step 1: Add per-ADC accumulators + signal**

In P2RadioConnection.h:
```cpp
public:
    /// Phase 3F: wideband frame ready (after 32 packets assembled for ADC N).
    Q_SIGNAL void widebandFrameReady(int adcIndex, QVector<float> samples);

private:
    std::array<WidebandFrameAccumulator*, 8> m_wbAccumulators {};
```

In constructor:
```cpp
    for (int i = 0; i < 8; ++i) {
        m_wbAccumulators[i] = new WidebandFrameAccumulator(this);
        connect(m_wbAccumulators[i], &WidebandFrameAccumulator::frameReady,
                this, [this, i](const QVector<float>& samples) {
            emit widebandFrameReady(i, samples);
        });
    }
```

- [ ] **Step 2: Replace stub at line 1608**

Find the existing `case 2..9: break;` block. Replace:

```cpp
        case 2:  // 1027: wideband ADC0
        case 3:  // 1028: wideband ADC1
        case 4:  // 1029: wideband ADC2
        case 5:  // 1030: wideband ADC3
        case 6:  // 1031: wideband ADC4
        case 7:  // 1032: wideband ADC5
        case 8:  // 1033: wideband ADC6
        case 9:  // 1034: wideband ADC7
        {
            // From Thetis network.c:550-603 [v2.10.3.15] wideband ADC data.
            if (data.size() != 1028) { break; }  // malformed packet
            const int adcId = portIdx - 2;
            if (adcId < 0 || adcId >= 8) { break; }

            // First 4 bytes = sequence number (big-endian)
            const quint32 seq = (quint8(data[0]) << 24) | (quint8(data[1]) << 16) |
                                (quint8(data[2]) << 8)  | quint8(data[3]);
            // Remaining 1024 bytes = 512 × 16-bit BE samples
            const QByteArray payload = data.mid(4);
            m_wbAccumulators[adcId]->pushPacket(int(seq), payload);
            break;
        }
```

- [ ] **Step 3: Commit**

```bash
cmake --build build && ctest --test-dir build 2>&1 | tail -5
git add src/core/P2RadioConnection.{h,cpp}
git commit -m "feat(3f-f): P2 wideband receive path - decode + accumulate (replaces stub)"
```

---

## Task 4: Create `WidebandFftEngine`

**Files:** Create `src/core/WidebandFftEngine.{h,cpp}`, `tests/tst_wideband_fft_engine.cpp`

16384-pt real-to-complex FFTW3 plan. Output: 8192 dBm bins covering 0..61.44 MHz.

- [ ] **Step 1: Write failing test**

```cpp
#include <QtTest/QtTest>
#include "core/WidebandFftEngine.h"

using namespace NereusSDR;

class TestWidebandFftEngine : public QObject {
    Q_OBJECT
private slots:
    void fft_produces_8192_bins_for_16384_real_samples()
    {
        WidebandFftEngine engine;
        engine.setAdcSampleRateHz(122880000);

        QVector<float> samples(16384, 0.5f);
        QVector<float> bins;
        engine.computeFft(samples, bins);

        QCOMPARE(bins.size(), 8192);  // real-to-complex output is N/2 + 1, drop DC = 8192
    }

    void bin_width_is_7p5_kHz_for_122mhz_adc()
    {
        WidebandFftEngine engine;
        engine.setAdcSampleRateHz(122880000);
        const double binWidth = engine.binWidthHz();
        QVERIFY(qAbs(binWidth - 7500.0) < 1.0);  // 122.88M / 16384 = 7500 Hz
    }
};

QTEST_MAIN(TestWidebandFftEngine)
#include "tst_wideband_fft_engine.moc"
```

Register.

- [ ] **Step 2: Create header**

```cpp
#pragma once
#include <QObject>
#include <QVector>
#include <fftw3.h>

namespace NereusSDR {

class WidebandFftEngine : public QObject {
    Q_OBJECT
public:
    explicit WidebandFftEngine(QObject* parent = nullptr);
    ~WidebandFftEngine() override;

    void setAdcSampleRateHz(double rateHz) { m_adcRateHz = rateHz; }
    double binWidthHz() const;

    /// Compute FFT: real samples in, dBm bins out (size 8192 for 16384 input).
    void computeFft(const QVector<float>& realSamples, QVector<float>& dbmBins);

private:
    static constexpr int kFftSize = 16384;
    static constexpr int kOutputBins = 8192;
    double         m_adcRateHz {122880000};
    fftwf_plan     m_plan {nullptr};
    float*         m_input {nullptr};
    fftwf_complex* m_output {nullptr};
};

} // namespace NereusSDR
```

- [ ] **Step 3: Implement**

```cpp
#include "core/WidebandFftEngine.h"
#include <cmath>

namespace NereusSDR {

WidebandFftEngine::WidebandFftEngine(QObject* parent) : QObject(parent)
{
    m_input = fftwf_alloc_real(kFftSize);
    m_output = fftwf_alloc_complex(kFftSize / 2 + 1);
    m_plan = fftwf_plan_dft_r2c_1d(kFftSize, m_input, m_output, FFTW_MEASURE);
}

WidebandFftEngine::~WidebandFftEngine()
{
    if (m_plan) { fftwf_destroy_plan(m_plan); }
    if (m_input) { fftwf_free(m_input); }
    if (m_output) { fftwf_free(m_output); }
}

double WidebandFftEngine::binWidthHz() const
{
    return m_adcRateHz / double(kFftSize);
}

void WidebandFftEngine::computeFft(const QVector<float>& realSamples, QVector<float>& dbmBins)
{
    if (realSamples.size() != kFftSize) { return; }

    std::copy(realSamples.cbegin(), realSamples.cend(), m_input);
    fftwf_execute(m_plan);

    dbmBins.resize(kOutputBins);
    for (int i = 0; i < kOutputBins; ++i) {
        const float re = m_output[i + 1][0];  // skip DC at bin 0
        const float im = m_output[i + 1][1];
        const float mag2 = re * re + im * im;
        // Convert magnitude squared → dBm; scale factor depends on calibration
        dbmBins[i] = (mag2 > 0.0f) ? (10.0f * std::log10(mag2)) : -200.0f;
    }
}

} // namespace NereusSDR
```

- [ ] **Step 4: Commit**

```bash
cmake --build build --target tst_wideband_fft_engine && ctest --test-dir build -R tst_wideband_fft_engine -V 2>&1 | tail -10
git add src/core/WidebandFftEngine.{h,cpp} tests/tst_wideband_fft_engine.cpp tests/CMakeLists.txt src/core/CMakeLists.txt
git commit -m "feat(3f-f): WidebandFftEngine (16384-pt real-input FFTW3 r2c)"
```

---

## Task 5: Wire wideband frame → FFT engine → SpectrumWidget

**Files:** Modify `src/models/RadioModel.{h,cpp}` (new accessor `widebandFftEngine(int adc)`)

- [ ] **Step 1: RadioModel owns 2 WidebandFftEngine instances (one per ADC)**

```cpp
private:
    std::array<WidebandFftEngine*, 2> m_widebandFftEngines {};
```

In constructor:
```cpp
    for (int i = 0; i < 2; ++i) {
        m_widebandFftEngines[i] = new WidebandFftEngine(this);
    }
```

Public accessor:
```cpp
public:
    WidebandFftEngine* widebandFftEngine(int adc) const {
        return (adc >= 0 && adc < 2) ? m_widebandFftEngines[adc] : nullptr;
    }
```

- [ ] **Step 2: Wire P2RadioConnection::widebandFrameReady to FFT**

In RadioModel where P2RadioConnection is constructed:
```cpp
    if (auto* p2 = qobject_cast<P2RadioConnection*>(m_connection)) {
        connect(p2, &P2RadioConnection::widebandFrameReady, this,
                [this](int adcIdx, const QVector<float>& samples) {
            if (adcIdx >= 2) { return; }  // only ADC0/1 supported in v1
            QVector<float> bins;
            m_widebandFftEngines[adcIdx]->computeFft(samples, bins);
            emit widebandSpectrumReady(adcIdx, bins);
        });
    }
```

Add signal:
```cpp
signals:
    void widebandSpectrumReady(int adcIndex, QVector<float> dbmBins);
```

- [ ] **Step 3: Commit**

```bash
cmake --build build
git add src/models/RadioModel.{h,cpp}
git commit -m "feat(3f-f): RadioModel wires wideband frames to FFT engine + emits spectrum"
```

---

## Task 6-10: SpectrumWidget extended-pan rendering

**Files:** Modify `src/gui/SpectrumWidget.{h,cpp}`

This is the substantial visual work. ~5 tasks broken down:

- [ ] **Task 6**: Add `m_widebandBins` member + setter slot, hook to `RadioModel::widebandSpectrumReady`
- [ ] **Task 7**: Compute pan visible range vs DDC range. When zoomed beyond DDC → set `m_extendedMode = true`
- [ ] **Task 8**: In paintEvent / paintGL, when extended mode: render wideband bins as background, render DDC bins overlaid on listenable island
- [ ] **Task 9**: Render dashed boundary lines at the edges of the listenable region
- [ ] **Task 10**: Add zoom-state signal that triggers `SliceModel::setWidebandExtensionRequested(true)` on the slice associated with this pan

Each task follows TDD pattern: failing visual smoke test (or property test), implement, verify.

(Detailed step-by-steps follow the same pattern as preceding plans. Condensed here for brevity.)

---

## Task 11: AlexController auto-bypass on wideband-extension-requested

**Files:** Modify `src/core/accessories/AlexController.cpp` + `src/models/RadioModel.cpp`

- [ ] **Step 1: Wire SliceModel::widebandExtensionRequestedChanged → AlexController::setWidebandActive**

In RadioModel slice-added handler:
```cpp
    connect(slice, &SliceModel::widebandExtensionRequestedChanged, this,
            [this, slice](bool on) {
        if (m_alexController) {
            m_alexController->setWidebandActive(slice->chainIndex(), on);
        }
        // Also send to P2 connection to flip packetbuf[23] enable bit
        if (auto* p2 = qobject_cast<P2RadioConnection*>(m_connection)) {
            p2->setWidebandEnabled(slice->chainIndex(), on);
        }
    });
```

- [ ] **Step 2: Commit**

```bash
git add src/models/RadioModel.cpp
git commit -m "feat(3f-f): wideband-extension-requested auto-bypasses Alex BPF + enables wb stream"
```

---

## Task 12: Click-in-wing retunes DDC; click-in-island retunes slice

**Files:** Modify `src/gui/SpectrumWidget.cpp` (mouse handlers)

- [ ] **Step 1: In mouseReleaseEvent (or click handler), determine if click was in listenable island or wing**

```cpp
void SpectrumWidget::onPanClicked(double clickedFreqHz)
{
    if (!m_extendedMode) {
        // Standard behaviour: retune slice within DDC
        emit sliceRetuneRequested(clickedFreqHz);
        return;
    }

    // Extended mode: check if click was in listenable island or wing
    const double ddcCenterHz = m_ddcCenterMhz * 1e6;
    const double ddcHalfBwHz = (m_ddcSampleRateHz / 2.0);
    if (qAbs(clickedFreqHz - ddcCenterHz) <= ddcHalfBwHz) {
        // Inside listenable island: retune slice within DDC
        emit sliceRetuneRequested(clickedFreqHz);
    } else {
        // In wing: retune the entire DDC to this frequency
        emit ddcRetuneRequested(clickedFreqHz);
    }
}
```

- [ ] **Step 2: Wire `ddcRetuneRequested` in MainWindow → updates slice's frequency (which propagates to codec via existing path)**

- [ ] **Step 3: Commit**

```bash
git add src/gui/SpectrumWidget.cpp src/gui/MainWindow.cpp
git commit -m "feat(3f-f): click-in-wing retunes DDC, click-in-island retunes slice"
```

---

## Task 13: Per-pan "Extended view" right-click toggle

**Files:** Modify `src/gui/PanadapterApplet.{h,cpp}`

Add right-click context menu on the pan itself (separate from VFO flag's context menu).

- [ ] **Step 1: Add `contextMenuEvent`**

```cpp
void PanadapterApplet::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    QAction* extAct = menu.addAction(QStringLiteral("Extended view (wideband wings)"));
    extAct->setCheckable(true);
    extAct->setChecked(m_extendedViewEnabled);
    connect(extAct, &QAction::toggled, this, &PanadapterApplet::setExtendedViewEnabled);

    menu.exec(event->globalPos());
}
```

- [ ] **Step 2: Persistence per-pan**

`Pan<N>_ExtendedView` AppSettings key.

- [ ] **Step 3: Commit**

```bash
git add src/gui/PanadapterApplet.{h,cpp}
git commit -m "feat(3f-f): per-pan Extended view right-click toggle (default on)"
```

---

## Task 14-15: Bench smoke test + retrospective

- [ ] Test on G2: enable wideband, zoom pan past 1.5 MHz, verify wideband wings render
- [ ] Test click-in-wing retunes DDC
- [ ] Verify BPF auto-bypasses on wideband-active
- [ ] Append retrospective to design doc
- [ ] Commit checkpoint

---

## Task 16: Open Sub-Epic F PR

```bash
git push -u origin HEAD
gh pr create --title "Phase 3F Sub-Epic F: Wideband Extended Pan"
```

---

## Sub-Epic F Completion Criteria

- `P2RadioConnection` `packetbuf[23]` enable byte plumbed
- Wideband receive path decodes packets (replaces stub)
- `WidebandFrameAccumulator` assembles frames with seq-error handling
- `WidebandFftEngine` produces 8192 dBm bins from 16384 real samples
- `SpectrumWidget` `paintExtendedPan` composites listenable + wideband regions
- Click-in-wing retunes DDC; click-in-island retunes slice
- AlexController auto-bypasses BPF on wideband active
- Per-pan "Extended view" toggle persists
- 3 new test files, all green
- Bench-verified on G2

Ready for **Sub-Epic G (Full Diversity Port)** to begin.

---

## References

- Design §7 (Wideband Extended Pan), §2 (widebandAdcs capability)
- Thetis `network.c:550-603 [v2.10.3.15]` (wideband receive)
- Thetis `network.c:880-882 [v2.10.3.15]` (packetbuf[23] enable byte)
- Thetis `netInterface.c:1458-1463 [v2.10.3.13]` (wideband default config)
- Sub-Epic A `widebandAdcs` capability field
- Sub-Epic B `AlexController::setWidebandActive`
