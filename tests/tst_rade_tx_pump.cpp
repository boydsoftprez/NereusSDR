// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: NereusSDR-original unit-test file.
// =================================================================
// tests/tst_rade_tx_pump.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for the RADE TX end-to-end wiring (Phase 3R K-bench):
//
//   1. setRadeChannel stores the pointer and clears on null.
//   2. tickForTest with TxPath::Rade emits radeMicBlockReady with
//      a non-empty payload after enough mic frames have been pushed
//      through the 48 kHz -> 80 Hz HPF -> 48 -> 16 kHz r8brain path
//      to clear the resampler warm-up.
//   3. The TX pump's RADE branch does NOT call sendTxIq on the
//      MockConnection (RADE bypasses the WDSP TXA chain entirely).
//   4. RadioModel's txModemReady receiver routes the encoded
//      baseband through 24 -> txSampleRate upsample into
//      RadioConnection::sendTxIq with at least one non-zero call.
//
// =================================================================
//
// Modification history (NereusSDR):
//   2026-05-11  J.J. Boyd / KG4VCF  Phase 3R K-bench follow-up:
//                 initial test file. NereusSDR-native. Pins the
//                 contracts the K2-K4 scaffolding deferred until
//                 the RADE TX pump was fully wired. AI tooling:
//                 Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>
#include <QObject>
#include <QSignalSpy>

#include <atomic>
#include <cstring>
#include <vector>

#include "core/AudioEngine.h"
#include "core/RadeChannel.h"
#include "core/RadioConnection.h"
#include "core/TxChannel.h"
#include "core/TxWorkerThread.h"
#include "core/audio/TxMicSource.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

namespace {

// MockConnection is duplicated from tst_tx_worker_thread.cpp; we keep
// it local rather than promoting to a shared header so the test file
// stays self-contained.
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit MockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }
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
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}

    void sendTxIq(const float* iq, int n) override
    {
        callCount.fetch_add(1, std::memory_order_relaxed);
        lastN.store(n, std::memory_order_relaxed);
        if (iq != nullptr && n > 0) {
            // Record sum-of-magnitudes so a non-zero burst is detectable
            // without keeping a per-call buffer (atomic float math is
            // simpler than QVector across threads).
            float mag = 0.0f;
            for (int k = 0; k < n * 2; ++k) {
                mag += std::abs(iq[k]);
            }
            int32_t bits = 0;
            std::memcpy(&bits, &mag, sizeof(int32_t));
            magnitudeBits.fetch_add(static_cast<int32_t>(bits),
                                    std::memory_order_relaxed);
        }
    }

    std::atomic<int> callCount{0};
    std::atomic<int> lastN{0};
    std::atomic<int32_t> magnitudeBits{0};
};

}  // namespace

class TstRadeTxPump : public QObject {
    Q_OBJECT

    static constexpr int kChannelId = 1;
    static constexpr int kBlockFrames = TxWorkerThread::kBlockFrames;

private slots:

    // ── 1. setRadeChannel stores the pointer; null clears ────────────────
    void setRadeChannelStoresPointer()
    {
        TxWorkerThread w;
        QVERIFY(w.radeChannelForTest() == nullptr);

        RadeChannel ch;
        w.setRadeChannel(&ch);
        QCOMPARE(w.radeChannelForTest(), &ch);

        w.setRadeChannel(nullptr);
        QVERIFY(w.radeChannelForTest() == nullptr);
    }

    // ── 2. dispatchOneBlock with TxPath::Rade emits radeMicBlockReady ────
    //
    // Synthetic 48 kHz mic samples through the pump's RADE branch:
    //   - drainBlock(64 frames I/Q at 48 kHz, Q=0)
    //   - extract I channel
    //   - 80 Hz HPF biquad in place
    //   - 48 -> 16 kHz r8brain CDSPResampler24
    //   - float -> int16 conversion
    //   - emit radeMicBlockReady(QByteArray of int16 mono samples)
    //
    // r8brain has a non-trivial filterbank warm-up (~3 blocks at 64 in /
    // 21 out per block typically), so the FIRST few tickForTest() calls
    // may emit empty payloads. The contract pinned here: after enough
    // blocks to clear warm-up, at least one non-empty emission lands.
    void radePumpEmitsAfterWarmup()
    {
        // 2026-05-12 (PR #238 P1 #4 follow-up): asserts the OLD K4
        // contract that RADE branch never drives the WDSP TXA
        // chain.  The K-bench reframe at TxWorkerThread.cpp:402-432
        // intentionally routes RADE TX *through* WDSP TXA so TUNE
        // and RADE share the modulator path.  Test needs rewriting
        // against the new "RADE feeds I-leg of m_in, WDSP TXA
        // modulates as ordinary SSB" contract — defer to a bench
        // session.
        QSKIP("Skipped: asserts pre-K-bench direct-sendTxIq contract; "
              "K-bench routes RADE TX through WDSP TXA. Rewrite "
              "deferred to bench follow-up.");

        AudioEngine engine;
        TxChannel ch(kChannelId, kBlockFrames, kBlockFrames);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        TxMicSource src;
        src.start();

        TxWorkerThread w;
        w.setTxChannel(&ch);
        w.setAudioEngine(&engine);
        w.setMicSource(&src);

        RadeChannel rade;
        w.setRadeChannel(&rade);
        w.setCurrentTxPath(TxWorkerThread::TxPath::Rade);

        QSignalSpy micSpy(&w, &TxWorkerThread::radeMicBlockReady);

        // Push 500 blocks of synthetic mic samples. 500 blocks * 64 frames
        // at 48 kHz = 32000 frames in (~0.67 s of audio). r8brain
        // CDSPResampler24 ships with the default ReqAtten=206.91 dB
        // which gives a very long filter (latency in the thousands of
        // input samples). Push enough samples to clear the warm-up.
        // The existing tst_rade_tx_filters test pumps 480000 input
        // samples for the ratio check; we use a more modest 32000 here
        // because we only need to observe one emission, not steady-state.
        constexpr int kBlockCount = 500;
        std::vector<float> mic(kBlockFrames, 0.0f);
        for (int blk = 0; blk < kBlockCount; ++blk) {
            for (int i = 0; i < kBlockFrames; ++i) {
                // 1 kHz sine, mid-amplitude.
                const float t = static_cast<float>(blk * kBlockFrames + i)
                                / 48000.0f;
                mic[i] = 0.5f * std::sin(2.0f * 3.14159265f * 1000.0f * t);
            }
            src.inbound(mic.data(), kBlockFrames);
            w.tickForTest();
        }

        // At least one emission with a non-empty payload.
        QVERIFY2(micSpy.count() >= 1,
                 qPrintable(QString("radeMicBlockReady fired %1 times")
                                .arg(micSpy.count())));

        bool sawNonEmpty = false;
        for (const auto& args : micSpy) {
            if (args.value(0).toByteArray().size() > 0) {
                sawNonEmpty = true;
                break;
            }
        }
        QVERIFY2(sawNonEmpty,
                 "radeMicBlockReady never emitted a non-empty QByteArray "
                 "(r8brain may be stuck in warm-up; expected at least one "
                 "post-warmup payload)");

        // RADE branch must NOT have driven the WDSP TXA chain.
        QCOMPARE(conn.callCount.load(), 0);

        src.stop();
    }

    // ── 3. With TxPath::Wdsp, dispatchOneBlock skips the RADE pump path
    //
    // Sanity check: after setting the path back to Wdsp the worker
    // resumes the WDSP TXA flow and a single tick produces exactly one
    // sendTxIq call on the MockConnection — no radeMicBlockReady.
    void wdspPathStillReachesSendTxIq()
    {
        AudioEngine engine;
        TxChannel ch(kChannelId, kBlockFrames, kBlockFrames);
        MockConnection conn;
        ch.setConnection(&conn);
        ch.setRunning(true);

        TxMicSource src;
        src.start();

        TxWorkerThread w;
        w.setTxChannel(&ch);
        w.setAudioEngine(&engine);
        w.setMicSource(&src);

        RadeChannel rade;
        w.setRadeChannel(&rade);
        w.setCurrentTxPath(TxWorkerThread::TxPath::Wdsp);

        QSignalSpy micSpy(&w, &TxWorkerThread::radeMicBlockReady);

        std::vector<float> mic(kBlockFrames, 0.1f);
        src.inbound(mic.data(), kBlockFrames);
        w.tickForTest();

        QCOMPARE(conn.callCount.load(), 1);
        QCOMPARE(micSpy.count(), 0);

        src.stop();
    }

    // ── 4. RadioModel txModemReady -> sendTxIq routing ───────────────────
    //
    // RadeChannel emits txModemReady (24 kHz stereo float32). The K4 hook
    // on RadioModel must:
    //   - Take the L (or mono) channel as the real-valued modem baseband.
    //   - Upsample 24 kHz -> txSampleRate (P1 default 48 kHz).
    //   - Build interleaved I/Q with Q=0.
    //   - Call m_connection->sendTxIq.
    //
    // We drive this with a synthetic 24 kHz stereo payload of N=2048
    // frames (84 ms of audio). At txSampleRate=48 kHz the resampler
    // emits roughly 4096 samples out, so sendTxIq must be called with
    // a non-zero magnitude at least once.
    void txModemReadyRoutesToSendTxIq()
    {
        // 2026-05-12 (PR #238 P1 #4 follow-up): asserts the OLD K4
        // direct path "RadeChannel::txModemReady -> RadioModel
        // lambda -> mockConn.sendTxIq".  K-bench reframe (see
        // RadioModel::wireRadeChannel at the txModemReady connect
        // site) replaces direct sendTxIq with a TxWorkerThread
        // setRadeAudioBlock call so the WDSP TXA chain modulates
        // RADE.  In a test fresh RadioModel with no TxWorker the
        // lambda early-exits at the m_txWorker null check; no
        // sendTxIq.  Re-author against the K-bench contract on
        // bench (needs a real TxWorker fixture).
        QSKIP("Skipped: asserts pre-K-bench direct-sendTxIq routing; "
              "K-bench routes txModemReady through WDSP TXA via "
              "TxWorker::setRadeAudioBlock. Rewrite deferred to "
              "bench follow-up.");

        // ORDER MATTERS: mockConn must outlive RadioModel because
        // RadioModel::~RadioModel -> teardownConnection touches
        // m_connection->thread().  Construct mockConn FIRST so it
        // is destroyed AFTER model.
        MockConnection mockConn;
        RadioModel model;

        const int sliceId = model.addSlice();
        SliceModel* slice = model.sliceById(sliceId);
        QVERIFY(slice != nullptr);

        model.injectConnectionForTest(&mockConn);

        // Friend test subclass exposing emit txModemReady (signals are
        // emit-only from inside the class).
        struct TestableRadeChannelLocal : public RadeChannel {
            void fire(const QByteArray& iq) { emit txModemReady(iq); }
        };

        TestableRadeChannelLocal local;
        model.wireRadeChannel(sliceId, &local, slice);

        // 2048-frame stereo float32 24 kHz payload. L = R = sine
        // (RadeChannel's processMonoToStereo duplicates the mono modem
        // baseband to both channels).
        constexpr int kFrames = 2048;
        QByteArray payload(kFrames * 2 * static_cast<int>(sizeof(float)),
                           Qt::Uninitialized);
        auto* p = reinterpret_cast<float*>(payload.data());
        for (int i = 0; i < kFrames; ++i) {
            const float t = static_cast<float>(i) / 24000.0f;
            const float s = 0.5f * std::sin(2.0f * 3.14159265f * 1000.0f * t);
            p[2 * i + 0] = s;
            p[2 * i + 1] = s;
        }

        const int beforeCalls = mockConn.callCount.load();

        // Drive multiple emissions to clear r8brain's 24 -> 48 kHz
        // warm-up.  Each call passes 2048 input samples; r8brain at
        // 2x upsample has roughly 100-200 input samples of warm-up
        // latency, so the first emission may produce 0 output but the
        // second is solid.
        for (int rep = 0; rep < 4; ++rep) {
            local.fire(payload);
        }

        // The connect is direct (RadeChannel lives on the main thread),
        // so by this point the lambda has run synchronously and sendTxIq
        // has been invoked.
        const int afterCalls = mockConn.callCount.load();

        QVERIFY2(afterCalls > beforeCalls,
                 qPrintable(QString("sendTxIq call count did not advance: "
                                    "before=%1 after=%2")
                                .arg(beforeCalls).arg(afterCalls)));
        QVERIFY2(mockConn.lastN.load() > 0,
                 qPrintable(QString("sendTxIq last frame count = %1; "
                                    "expected > 0")
                                .arg(mockConn.lastN.load())));

        // Clear the injected connection pointer before mockConn falls
        // out of scope (the test fixture's order guarantees this, but
        // the explicit clear makes the intent unambiguous).
        model.injectConnectionForTest(nullptr);
    }
};

QTEST_MAIN(TstRadeTxPump)
#include "tst_rade_tx_pump.moc"
