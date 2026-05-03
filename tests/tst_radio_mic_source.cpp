// =================================================================
// tests/tst_radio_mic_source.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original test file. No Thetis port at this layer.
//
// Exercises RadioMicSource — the TxMicRouter implementation that drains
// a lock-free SPSC ring fed by RadioConnection::micFrameDecoded
// (Phase 3M-1b Task F.2).
//
// Strategy: construct a TestRadioConnection (minimal concrete subclass
// that stubs all pure-virtuals and exposes emitMicFrame()), connect it
// to RadioMicSource, then drive the signal synchronously via
// emitMicFrame(). Because the connection uses Qt::DirectConnection, the
// onMicFrame slot fires on the calling (test) thread, making the push/
// pull interaction single-threaded and deterministic.
//
// TestRadioConnection is duplicated from tst_radio_connection_mic_frame_signal
// rather than shared — each test binary is self-contained.
//
// Plan: 3M-1b F.2. Master design §5.2.1.
// =================================================================
//
// Modification history (NereusSDR):
//   2026-04-27 — Original test for NereusSDR by J.J. Boyd (KG4VCF),
//                 Phase 3M-1b Task F.2, with AI-assisted implementation
//                 via Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest/QtTest>

#include "core/audio/RadioMicSource.h"
#include "core/RadioConnection.h"

#include <array>

using namespace NereusSDR;

// ---------------------------------------------------------------------------
// TestRadioConnection — minimal concrete subclass of RadioConnection.
// Stubs all pure-virtual slots as no-ops.
// Exposes emitMicFrame() so tests can fire micFrameDecoded synchronously.
// ---------------------------------------------------------------------------
class TestRadioConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit TestRadioConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {}

    // Pure-virtual slot stubs.
    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTT(bool) override {}
    void setMicXlr(bool) override {}
    void sendTxIq(const float*, int) override {}

    // Test helper: fires micFrameDecoded synchronously (DirectConnection).
    void emitMicFrame(const float* samples, int frames)
    {
        emit micFrameDecoded(samples, frames);
    }
};

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------
class TstRadioMicSource : public QObject {
    Q_OBJECT

private:
    // Build a RadioMicSource + TestRadioConnection pair.
    struct Harness {
        TestRadioConnection conn;
        RadioMicSource src;

        Harness() : conn(), src(&conn) {}
    };

private slots:

    // ── 1. Empty ring → zero-fill ─────────────────────────────────────────
    // No data pushed. pullSamples must fill the destination with zeros
    // and return n (the requested count).

    void pullSamples_emptyRing_zeroFills()
    {
        Harness h;
        std::array<float, 8> dst;
        dst.fill(9.9f);

        const int got = h.src.pullSamples(dst.data(), 8);
        QCOMPARE(got, 8);
        for (int i = 0; i < 8; ++i) {
            QCOMPARE(dst[i], 0.0f);
        }
    }

    // ── 2. Partial ring → data then zero-fill ────────────────────────────
    // Push 4 samples, pull 8. First 4 must be the data; remaining 4 zero.

    void pullSamples_partialRing_returnsZeroFilledRest()
    {
        Harness h;
        std::array<float, 4> in{1.0f, 2.0f, 3.0f, 4.0f};
        h.conn.emitMicFrame(in.data(), 4);

        QCOMPARE(h.src.ringFillForTest(), 4);

        std::array<float, 8> dst{};
        const int got = h.src.pullSamples(dst.data(), 8);
        QCOMPARE(got, 8);
        // First 4 from ring.
        QCOMPARE(dst[0], 1.0f);
        QCOMPARE(dst[1], 2.0f);
        QCOMPARE(dst[2], 3.0f);
        QCOMPARE(dst[3], 4.0f);
        // Rest zero-filled.
        QCOMPARE(dst[4], 0.0f);
        QCOMPARE(dst[5], 0.0f);
        QCOMPARE(dst[6], 0.0f);
        QCOMPARE(dst[7], 0.0f);
    }

    // ── 3. Full drain ────────────────────────────────────────────────────
    // Push n samples, pull exactly n. Ring must be empty afterwards.

    void pullSamples_fullDrain_returnsAllSamples()
    {
        Harness h;
        std::array<float, 6> in{0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.6f};
        h.conn.emitMicFrame(in.data(), 6);

        std::array<float, 6> dst{};
        const int got = h.src.pullSamples(dst.data(), 6);
        QCOMPARE(got, 6);
        for (int i = 0; i < 6; ++i) {
            QCOMPARE(dst[i], in[i]);
        }
        QCOMPARE(h.src.ringFillForTest(), 0);
    }

    // ── 4. FIFO order preserved ──────────────────────────────────────────
    // Push 16 samples with distinct values. Pull must return them in
    // the same order (FIFO, not LIFO or randomized).

    void pullSamples_fifoOrder_preserved()
    {
        Harness h;
        constexpr int kN = 16;
        std::array<float, kN> in{};
        for (int i = 0; i < kN; ++i) { in[i] = static_cast<float>(i + 1); }
        h.conn.emitMicFrame(in.data(), kN);

        std::array<float, kN> dst{};
        h.src.pullSamples(dst.data(), kN);
        for (int i = 0; i < kN; ++i) {
            QCOMPARE(dst[i], in[i]);
        }
    }

    // ── 5. onMicFrame pushes to ring ─────────────────────────────────────
    // After emitMicFrame(n), ringFillForTest() must equal n.

    void onMicFrame_pushesToRing()
    {
        Harness h;
        QCOMPARE(h.src.ringFillForTest(), 0);

        std::array<float, 10> buf{};
        h.conn.emitMicFrame(buf.data(), 10);
        QCOMPARE(h.src.ringFillForTest(), 10);
    }

    // ── 6. onMicFrame overflow → drops excess, records drop count ────────
    // Fill the ring completely, then push 5 more. The 5 must be dropped
    // and ringDroppedForTest() must equal 5.

    void onMicFrame_overflow_dropsExtra_recordsDropCount()
    {
        Harness h;
        // Fill the ring to capacity (4096 slots).
        constexpr int kCap = RadioMicSource::kRingCapacity;
        std::vector<float> chunk(kCap, 1.0f);
        h.conn.emitMicFrame(chunk.data(), kCap);
        QCOMPARE(h.src.ringFillForTest(), kCap);
        QCOMPARE(h.src.ringDroppedForTest(), 0);

        // Push 5 more — all should be dropped.
        std::array<float, 5> extra{2.0f, 2.0f, 2.0f, 2.0f, 2.0f};
        h.conn.emitMicFrame(extra.data(), 5);
        QCOMPARE(h.src.ringDroppedForTest(), 5);

        // Ring fill must still be kCap.
        QCOMPARE(h.src.ringFillForTest(), kCap);
    }

    // ── 7. onMicFrame null samples → no-op ──────────────────────────────
    // Passing nullptr samples must not crash or change ring state.

    void onMicFrame_nullSamples_noOp()
    {
        Harness h;
        h.conn.emitMicFrame(nullptr, 8);
        QCOMPARE(h.src.ringFillForTest(), 0);
        QCOMPARE(h.src.ringDroppedForTest(), 0);
    }

    // ── 8. onMicFrame zero frames → no-op ───────────────────────────────
    // Passing frames=0 must not crash or change ring state.

    void onMicFrame_zeroFrames_noOp()
    {
        Harness h;
        std::array<float, 4> buf{1.0f, 2.0f, 3.0f, 4.0f};
        h.conn.emitMicFrame(buf.data(), 0);
        QCOMPARE(h.src.ringFillForTest(), 0);
        QCOMPARE(h.src.ringDroppedForTest(), 0);
    }

    // ── 9. Round-trip push and pull ──────────────────────────────────────
    // Push two batches of different values, pull both back.
    // Verifies that successive pushes and pulls work end-to-end.

    void roundTrip_pushAndPull()
    {
        Harness h;
        std::array<float, 4> a{0.1f, 0.2f, 0.3f, 0.4f};
        std::array<float, 3> b{0.5f, 0.6f, 0.7f};
        h.conn.emitMicFrame(a.data(), 4);
        h.conn.emitMicFrame(b.data(), 3);

        QCOMPARE(h.src.ringFillForTest(), 7);

        std::array<float, 7> dst{};
        h.src.pullSamples(dst.data(), 7);
        QCOMPARE(dst[0], 0.1f);
        QCOMPARE(dst[1], 0.2f);
        QCOMPARE(dst[2], 0.3f);
        QCOMPARE(dst[3], 0.4f);
        QCOMPARE(dst[4], 0.5f);
        QCOMPARE(dst[5], 0.6f);
        QCOMPARE(dst[6], 0.7f);
        QCOMPARE(h.src.ringFillForTest(), 0);
    }

    // ── 10. Multiple cycles preserve FIFO across ring boundary ───────────
    // Push and pull 1024-sample batches 8 times. This exercises the index
    // wraparound at 4096 (kRingCapacity). FIFO order must be preserved.

    void multipleCycles_fifo_preserved()
    {
        Harness h;
        constexpr int kBatch = 1024;
        constexpr int kCycles = 8;  // 8 × 1024 = 8192 total samples > 4096

        for (int cycle = 0; cycle < kCycles; ++cycle) {
            // Fill a batch with recognizable values.
            std::vector<float> in(kBatch);
            for (int i = 0; i < kBatch; ++i) {
                in[i] = static_cast<float>(cycle * kBatch + i);
            }
            h.conn.emitMicFrame(in.data(), kBatch);

            std::vector<float> out(kBatch, -1.0f);
            h.src.pullSamples(out.data(), kBatch);

            for (int i = 0; i < kBatch; ++i) {
                QCOMPARE(out[i], in[i]);
            }
        }
    }

    // ── 11. Null connection → constructor ok, zero-fills ─────────────────
    // RadioMicSource with nullptr connection must construct without crashing
    // and pullSamples must return zero-filled silence.

    void nullConnection_constructorOk()
    {
        RadioMicSource src(nullptr);
        std::array<float, 4> dst{9.9f, 9.9f, 9.9f, 9.9f};
        const int got = src.pullSamples(dst.data(), 4);
        QCOMPARE(got, 4);
        for (float v : dst) { QCOMPARE(v, 0.0f); }
    }

    // ── 12. pullSamples always returns n (even on underrun) ──────────────
    // The audio pipeline expects a full buffer count. pullSamples must
    // return exactly n regardless of how many samples were available.

    void pullSamples_returnAlwaysN()
    {
        Harness h;
        // Push only 3, pull 10 — should return 10 (with 7 zero-filled).
        std::array<float, 3> in{1.0f, 2.0f, 3.0f};
        h.conn.emitMicFrame(in.data(), 3);

        std::array<float, 10> dst{};
        const int got = h.src.pullSamples(dst.data(), 10);
        QCOMPARE(got, 10);
        // First 3 from ring.
        QCOMPARE(dst[0], 1.0f);
        QCOMPARE(dst[1], 2.0f);
        QCOMPARE(dst[2], 3.0f);
        // Rest zero.
        for (int i = 3; i < 10; ++i) { QCOMPARE(dst[i], 0.0f); }
    }

    // ── 13. Null dst → returns 0 ─────────────────────────────────────────
    // pullSamples with nullptr dst must return 0 (programming-error guard).

    void pullSamples_nullDst_returnsZero()
    {
        Harness h;
        std::array<float, 4> in{1.0f, 2.0f, 3.0f, 4.0f};
        h.conn.emitMicFrame(in.data(), 4);

        QCOMPARE(h.src.pullSamples(nullptr, 4), 0);
        // Ring must still hold the 4 samples (not consumed).
        QCOMPARE(h.src.ringFillForTest(), 4);
    }

    // ── 14. Via TxMicRouter base pointer → vtable correct ────────────────
    // Construct via base pointer and verify dispatch still works.

    void viaBasePointer_dispatchWorks()
    {
        Harness h;
        std::array<float, 2> in{0.25f, 0.75f};
        h.conn.emitMicFrame(in.data(), 2);

        TxMicRouter* router = &h.src;
        std::array<float, 2> dst{};
        const int got = router->pullSamples(dst.data(), 2);
        QCOMPARE(got, 2);
        QCOMPARE(dst[0], 0.25f);
        QCOMPARE(dst[1], 0.75f);
    }
};

QTEST_APPLESS_MAIN(TstRadioMicSource)
#include "tst_radio_mic_source.moc"
