// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - tst_rade_channel: tests for the Phase 3R RadeChannel wrapper.
//
// I1 skeleton contracts (lifecycle only):
//
//   1. initialState              fresh RadeChannel reports !isActive() && !isSynced()
//   2. startStop                 start(<real path>) returns true and flips isActive()
//                                back to false after stop()
//   3. modelLoadFailureDisables  start(<nonexistent path>) returns false and leaves
//                                isActive() at false
//
// I2 RX-path contracts:
//
//   4. startInitializesRade              start("dummy") opens librade and isActive() flips true
//   5. processIqEmitsSyncFalseOnNoise    feeding random I/Q noise preserves
//                                        one quiet speech chunk per input
//   6. processIqAccumulatesAcrossChunks  small chunks accumulate; rade_rx fires only
//                                        once a rade_nin()-sized buffer is ready
//   7. stopReleasesResources             start("dummy") then stop() tears down cleanly
//
// I3 TX-path contracts:
//
//   8. txEncodeAcceptsAndAccumulates  feed 16 kHz mono int16 speech samples; the
//                                     wrapper accumulates LPCNET_FRAME_SIZE chunks,
//                                     extracts features, accumulates to
//                                     rade_n_features_in_out, then calls rade_tx
//                                     at least once (radeTxCallCountForTest > 0).
//   9. txEncodeEmitsModemSamples      same scenario, verify txModemReady fires
//                                     with a non-zero QByteArray payload.
//  10. resetTxClearsAccumulators      feed a partial frame (less than the
//                                     LPCNET_FRAME_SIZE-byte threshold), call
//                                     resetTx(), verify the TX feature accumulator
//                                     drains via the test seam.
//  11. txWhileInactiveIsNoOp          calling txEncode() without start() must not
//                                     crash and must not emit any signals.
//
// I4 text channel lands in its own task.
//
// See src/core/RadeChannel.h for the upstream license headers and
// modification-history block (verbatim freedv-gui BSD-2-Clause-style
// header + AetherSDR project-level attribution per
// docs/attribution/HOW-TO-PORT.md rule 6).

#include <QtTest/QtTest>
#include <QTemporaryFile>
#include <QSignalSpy>
#include <QByteArray>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <chrono>
#include <atomic>

#include "core/RadeChannel.h"

using namespace NereusSDR;

class TestRadeChannel : public QObject {
    Q_OBJECT

private slots:
    void initialState();
    void startStop();
    void modelLoadFailureDisablesChannel();

    // I2 RX-path tests
    void startInitializesRade();
    void shortInputEmitsSameSizedQuietPadding();
    void processIqEmitsSyncFalseOnNoise();
    void processIqAccumulatesAcrossMultipleChunks();
    void stopReleasesResources();

    // I3 TX-path tests
    void txEncodeAcceptsAndAccumulates();
    void txEncodeEmitsModemSamples();
    void resetTxClearsAccumulators();
    void txWhileInactiveIsNoOp();

    // v0.5.0 RADE U/L sideband-split fix-up.
    void sidebandRoundTripsViaSetter();

    // RADE end-of-over callsigns.
    void endOfOverQueuesFrameAndSilence();
    void noSpeechAfterEndOfOverUntilReset();
    void endOfOverWhileInactiveSendsNothing();
    void endOfOverCallsignReachesAnotherChannel();
    void endOfOverEncodesHeldSpeechFirstWithoutWaiting();
};

void TestRadeChannel::initialState()
{
    // A freshly constructed RadeChannel must not advertise itself as
    // active or in sync. Active flips on start(); sync flips on the
    // RADE decoder's sync indication once I2 wires it up.
    RadeChannel ch;
    QVERIFY(!ch.isActive());
    QVERIFY(!ch.isSynced());
}

void TestRadeChannel::startStop()
{
    // Create a real fixture file so start()'s skeleton path-exists
    // check passes. The byte contents are irrelevant for I1; I2 will
    // teach start() to actually load the .f32 model via rade_open().
    QTemporaryFile fixture;
    QVERIFY(fixture.open());
    fixture.write("rade-model-skeleton-fixture");
    fixture.flush();

    RadeChannel ch;
    QVERIFY(ch.start(fixture.fileName()));
    QVERIFY(ch.isActive());

    ch.stop();
    QVERIFY(!ch.isActive());
}

void TestRadeChannel::modelLoadFailureDisablesChannel()
{
    // Nonexistent path must not flip isActive(). I2 will further
    // require the file's contents to be a valid RADE model and will
    // close + reset on rade_open() failure as well; I1 just guards
    // the path-exists precondition.
    RadeChannel ch;
    QVERIFY(!ch.start("/nonexistent/path.f32"));
    QVERIFY(!ch.isActive());
}

// =========================================================================
// I2 RX-path tests
// =========================================================================

namespace {

// Build a buffer of N interleaved I/Q float32 samples filled with
// deterministic low-amplitude noise. RADE will not sync on this input,
// so the pipeline runs through resample + rade_rx + FARGAN without
// emitting a decoded speech chunk. Seed is fixed so the test is
// deterministic. See tests/fixtures/rade/README.md for the rationale.
QByteArray makeSyntheticIq(int nSamples)
{
    QByteArray buf(nSamples * 2 * static_cast<int>(sizeof(float)), Qt::Uninitialized);
    auto* p = reinterpret_cast<float*>(buf.data());
    std::mt19937 rng(0xC0DEC0DEu);
    std::uniform_real_distribution<float> noise(-0.1f, 0.1f);
    for (int i = 0; i < nSamples; ++i) {
        p[2 * i]     = noise(rng);  // I
        p[2 * i + 1] = noise(rng);  // Q
    }
    return buf;
}

// Build a buffer of N mono int16 samples at 16 kHz containing a Hanning-
// windowed 300 Hz sine. The amplitude (peak ~24000) sits well inside the
// int16 range; the Hanning envelope keeps the signal speech-like (avoids
// LPCNet feature-extractor blowup on a hard square edge). Used by the
// I3 TX tests to drive txEncode() with deterministic input that is not
// pure noise; the LPCNet encoder + rade_tx still get exercised through
// their full code path regardless of whether the synthesised signal is
// recognisable speech. See tests/fixtures/rade/README.md for rationale.
QByteArray makeSyntheticSpeech16k(int nSamples)
{
    QByteArray buf(nSamples * static_cast<int>(sizeof(int16_t)), Qt::Uninitialized);
    auto* p = reinterpret_cast<int16_t*>(buf.data());
    constexpr double kSampleRate = 16000.0;
    constexpr double kToneHz     = 300.0;
    constexpr double kPeak       = 24000.0;
    constexpr double kTwoPi      = 6.283185307179586;
    for (int i = 0; i < nSamples; ++i) {
        const double t = static_cast<double>(i) / kSampleRate;
        const double envelope = 0.5 * (1.0 - std::cos(kTwoPi * i /
                                                       static_cast<double>(nSamples - 1)));
        const double sample = kPeak * envelope * std::sin(kTwoPi * kToneHz * t);
        p[i] = static_cast<int16_t>(std::clamp(sample, -32768.0, 32767.0));
    }
    return buf;
}

}  // namespace

void TestRadeChannel::startInitializesRade()
{
    // "dummy" is the radae_nopy convention: the model_file argument
    // is ignored, the built-in weights compiled into librade are
    // used. RadeChannel::start treats "dummy" as a sentinel and
    // bypasses the path-exists check.
    RadeChannel ch;
    QVERIFY(ch.start("dummy"));
    QVERIFY(ch.isActive());

    // Calling start() a second time on the same wrapper is idempotent.
    QVERIFY(ch.start("dummy"));
    QVERIFY(ch.isActive());

    ch.stop();
    QVERIFY(!ch.isActive());

    // Non-existent path still rejected.
    QVERIFY(!ch.start("/nonexistent/path.f32"));
    QVERIFY(!ch.isActive());
}

void TestRadeChannel::processIqEmitsSyncFalseOnNoise()
{
    RadeChannel ch;
    QVERIFY(ch.start("dummy"));

    QSignalSpy syncSpy(&ch, &RadeChannel::syncChanged);
    QSignalSpy speechSpy(&ch, &RadeChannel::rxSpeechReady);

    // Feed roughly 1 second of synthetic I/Q noise at 24 kHz in
    // several smaller chunks. rade_nin() at the RADE-v1 default
    // is a few thousand RADE_COMP samples at 8 kHz, so a 24kHz
    // input of ~24000 samples is more than enough to trigger
    // multiple rade_rx() invocations. The codec will not sync
    // on noise, so syncChanged is only ever emitted as false
    // (or stays unemitted if it was false at construction).
    constexpr int kChunkSamples = 4096;
    constexpr int kNumChunks    = 8;
    for (int chunk = 0; chunk < kNumChunks; ++chunk) {
        ch.processIq(makeSyntheticIq(kChunkSamples));
    }

    // We made at least one rade_rx() call (otherwise sync state
    // never refreshed). Per the AetherSDR pattern, syncChanged is
    // only emitted on a state transition. Since the wrapper starts
    // with m_synced = false and noise input keeps it false, the
    // signal should not fire at all - but if for any reason
    // librade flips sync briefly and back, we accept that too.
    // What we cannot accept is a sync=true that persists.
    QVERIFY2(!ch.isSynced(),
             "noise input must not produce a sustained RADE sync indication");

    // syncChanged may fire zero or more times, but every emitted
    // value should be false (no spurious sync on noise).
    for (const auto& args : syncSpy) {
        QVERIFY2(args.value(0).toBool() == false,
                 "syncChanged emitted true on pure-noise input");
    }

    // AetherSDR's source contract emits one same-sized quiet block whenever
    // decoder output is short. NereusSDR's MasterMixer has no timeout, so
    // preserving this input cadence is what lets ordinary co-hosted slices
    // continue while RADE is unsynchronised.
    QCOMPARE(speechSpy.count(), kNumChunks);
    for (const auto& args : speechSpy) {
        const QByteArray pcm = args.value(0).toByteArray();
        QCOMPARE(pcm.size(),
                 kChunkSamples * 2 * static_cast<int>(sizeof(float)));
    }

    // At least one rade_rx() must have been called given the input
    // volume; otherwise the accumulator is broken.
    QVERIFY2(ch.radeRxCallCountForTest() >= 1,
             qPrintable(QString("rade_rx() never invoked across %1 input chunks")
                            .arg(kNumChunks)));

    ch.stop();
}

void TestRadeChannel::shortInputEmitsSameSizedQuietPadding()
{
    RadeChannel ch;
    QVERIFY(ch.start("dummy"));
    QSignalSpy speechSpy(&ch, &RadeChannel::rxSpeechReady);
    constexpr int kFrames = 64;
    ch.processIq(makeSyntheticIq(kFrames));
    QCOMPARE(speechSpy.count(), 1);
    const QByteArray pcm = speechSpy.first().first().toByteArray();
    QCOMPARE(pcm.size(), kFrames * 2 * static_cast<int>(sizeof(float)));
    for (char byte : pcm) {
        QCOMPARE(byte, '\0');
    }
}

void TestRadeChannel::processIqAccumulatesAcrossMultipleChunks()
{
    RadeChannel ch;
    QVERIFY(ch.start("dummy"));

    // Feed five chunks of 64 samples each (320 I/Q samples total,
    // = 640 floats). At 24 kHz -> 8 kHz this is ~107 RADE_COMP
    // samples after downsampling, well under rade_nin()'s typical
    // few-thousand-sample threshold. rade_rx() should NOT be
    // called yet.
    for (int i = 0; i < 5; ++i) {
        ch.processIq(makeSyntheticIq(64));
    }
    QCOMPARE(ch.radeRxCallCountForTest(), 0);

    // Now feed a large chunk that pushes the accumulator past
    // rade_nin(). 24000 samples at 24 kHz -> ~8000 samples at
    // 8 kHz, comfortably above rade_nin()'s threshold.
    ch.processIq(makeSyntheticIq(24000));
    QVERIFY2(ch.radeRxCallCountForTest() >= 1,
             qPrintable(QString("rade_rx() count was %1 after pushing 24000-sample chunk")
                            .arg(ch.radeRxCallCountForTest())));

    ch.stop();
}

void TestRadeChannel::stopReleasesResources()
{
    // Start -> stop -> destructor must all run cleanly without
    // tripping ASan, leaks, or hangs. Multiple start/stop cycles
    // exercise the cleanup-and-reallocation paths.
    for (int cycle = 0; cycle < 3; ++cycle) {
        RadeChannel ch;
        QVERIFY(ch.start("dummy"));
        QVERIFY(ch.isActive());

        // Feed a small chunk so the accumulators have content
        // before stop() tears them down.
        ch.processIq(makeSyntheticIq(256));

        ch.stop();
        QVERIFY(!ch.isActive());
        QVERIFY(!ch.isSynced());
    }

    // One more: rely on the destructor to call stop() implicitly.
    {
        RadeChannel ch;
        QVERIFY(ch.start("dummy"));
        ch.processIq(makeSyntheticIq(256));
        // Implicit destructor call here.
    }
}

// =========================================================================
// I3 TX-path tests
// =========================================================================

void TestRadeChannel::txEncodeAcceptsAndAccumulates()
{
    // Drive the TX pipeline with enough 16 kHz mono int16 speech samples
    // to fill rade_n_features_in_out features (LPCNET_FRAME_SIZE samples
    // per feature frame, NB_TOTAL_FEATURES floats out per feature frame;
    // rade_n_features_in_out is typically 12 * NB_TOTAL_FEATURES = 432 at
    // the RADE-v1 default which means we need 12 * 160 = 1920 input
    // samples). We push 16000 samples (one second of audio) in eight
    // 2000-sample chunks to also exercise the cross-chunk accumulator.
    RadeChannel ch;
    QVERIFY(ch.start("dummy"));

    constexpr int kChunkSamples = 2000;
    constexpr int kNumChunks    = 8;
    for (int chunk = 0; chunk < kNumChunks; ++chunk) {
        ch.txEncode(makeSyntheticSpeech16k(kChunkSamples));
    }

    QVERIFY2(ch.radeTxCallCountForTest() >= 1,
             qPrintable(QString("rade_tx() never invoked across %1 x %2 samples")
                            .arg(kNumChunks).arg(kChunkSamples)));

    ch.stop();
}

void TestRadeChannel::txEncodeEmitsModemSamples()
{
    // Same setup as txEncodeAcceptsAndAccumulates but with a QSignalSpy
    // on txModemReady. At least one chunk must be emitted; per
    // AetherSDR's upstream behaviour (RADEEngine.cpp:189-192 [@0cd4559])
    // the wrapper emits the upsampler's output unconditionally, which
    // means the very first emit MAY be empty (r8brain CDSPResampler24
    // has nontrivial startup latency before its FIR delay line fills).
    // What we pin: at least one emitted chunk must be non-empty
    // confirming the encode->upsample->emit pipeline produces real
    // samples beyond the warm-up window.
    RadeChannel ch;
    QVERIFY(ch.start("dummy"));

    QSignalSpy modemSpy(&ch, &RadeChannel::txModemReady);

    constexpr int kChunkSamples = 2000;
    constexpr int kNumChunks    = 8;
    for (int chunk = 0; chunk < kNumChunks; ++chunk) {
        ch.txEncode(makeSyntheticSpeech16k(kChunkSamples));
    }

    QVERIFY2(modemSpy.count() >= 1,
             qPrintable(QString("txModemReady fired %1 times").arg(modemSpy.count())));
    bool sawNonEmpty = false;
    for (const auto& args : modemSpy) {
        if (args.value(0).toByteArray().size() > 0) {
            sawNonEmpty = true;
            break;
        }
    }
    QVERIFY2(sawNonEmpty,
             "txModemReady never emitted a non-empty QByteArray "
             "(upsampler may be stuck in warm-up; expected at least one "
             "post-warmup chunk)");

    ch.stop();
}

void TestRadeChannel::resetTxClearsAccumulators()
{
    // Feed a small chunk so the TX speech accumulator has content but
    // not enough to drain an LPCNET_FRAME_SIZE-byte chunk. The TX
    // feature accumulator should remain non-zero after the input pass
    // (depending on the chunk size, m_txAccum is non-empty while
    // m_txFeatAccum may be either empty or partially-filled). Call
    // resetTx() and verify both accumulators read zero via the test
    // seam.
    RadeChannel ch;
    QVERIFY(ch.start("dummy"));

    // 80 samples is half an LPCNET_FRAME_SIZE (160) at 16 kHz, so the
    // TX accumulator will have content but rade_tx() will NOT have run.
    ch.txEncode(makeSyntheticSpeech16k(80));
    QCOMPARE(ch.radeTxCallCountForTest(), 0);

    // After resetTx(), the test seam reports zero feature-accumulator
    // bytes. The wrapper's behaviour on the speech accumulator is
    // covered by the (post-reset) re-encode below: a follow-up partial
    // chunk that brings the *accumulated* count to >= LPCNET_FRAME_SIZE
    // should NOT trigger rade_tx() if resetTx() flushed properly,
    // because the pre-reset partial frame must have been discarded.
    ch.resetTx();
    QCOMPARE(ch.txFeatureAccumSizeForTest(), 0);

    // Push another 80 samples. The pre-reset 80 samples should be gone
    // so we still won't have a full LPCNET_FRAME_SIZE frame and rade_tx
    // must remain uncalled.
    ch.txEncode(makeSyntheticSpeech16k(80));
    QCOMPARE(ch.radeTxCallCountForTest(), 0);

    ch.stop();
}

void TestRadeChannel::txWhileInactiveIsNoOp()
{
    // Calling txEncode() before start() must be safe: no crash, no
    // signal emissions, no accumulator state. Also exercise the
    // already-stopped path to pin the same contract on the exit side.
    RadeChannel ch;
    QSignalSpy modemSpy(&ch, &RadeChannel::txModemReady);

    ch.txEncode(makeSyntheticSpeech16k(2000));
    QCOMPARE(modemSpy.count(), 0);
    QCOMPARE(ch.radeTxCallCountForTest(), 0);
    QCOMPARE(ch.txFeatureAccumSizeForTest(), 0);

    // start -> stop, then verify the post-stop path is also a no-op.
    QVERIFY(ch.start("dummy"));
    ch.stop();
    ch.txEncode(makeSyntheticSpeech16k(2000));
    QCOMPARE(modemSpy.count(), 0);
    QCOMPARE(ch.radeTxCallCountForTest(), 0);
    QCOMPARE(ch.txFeatureAccumSizeForTest(), 0);
}

// v0.5.0 RADE U/L sideband-split fix-up: setSideband stores the flag
// and sidebandUpper() returns it.  The default value is upper (true),
// matching the RADE_U enum value 12 (= the lower-numbered, default
// sideband, mirroring the USB-default convention in this codebase).
// The flag is not yet consumed by the I/Q routing layer at v0.5.0;
// this test pins the storage contract that K-bench follow-up will
// consume.
void TestRadeChannel::sidebandRoundTripsViaSetter()
{
    RadeChannel ch;
    // Default: upper.
    QVERIFY(ch.sidebandUpper());

    ch.setSideband(false);
    QVERIFY(!ch.sidebandUpper());

    ch.setSideband(true);
    QVERIFY(ch.sidebandUpper());
}

// RADE end-of-over callsigns: queueEndOfOver emits the EOO frame and FreeDV's
// 200 ms of silence once, as txModemReady, and reports its length.
void TestRadeChannel::endOfOverQueuesFrameAndSilence()
{
    RadeChannel ch;
    QVERIFY(ch.start("dummy"));
    // rade_n_tx_eoo_out (1152 for RADE V1) + NUM_SAMPLES_SILENCE (1600) +
    // the 8 -> 24 kHz resampler's latency in zeros.
    const int samples8k = ch.endOfOverSamples8k();
    QVERIFY2(samples8k > 1152 + 1600, qPrintable(QString::number(samples8k)));

    QSignalSpy modemSpy(&ch, &RadeChannel::txModemReady);
    QVERIFY(ch.queueEndOfOver(QStringLiteral("KG4VCF")));
    QVERIFY(ch.endOfOverQueued());
    QCOMPARE(modemSpy.count(), 1);
    // From a fresh resampler the output at 24 kHz is exactly the EOO and
    // the 200 ms of silence: (1152 + 1600) x 3 stereo float frames.
    const int bytes = modemSpy.first().value(0).toByteArray().size();
    const int frames = bytes / (2 * static_cast<int>(sizeof(float)));
    QVERIFY2(std::abs(frames - (1152 + 1600) * 3) <= 3,
             qPrintable(QStringLiteral("%1 frames").arg(frames)));
    ch.stop();
    QCOMPARE(ch.endOfOverSamples8k(), 0);
}

// After the end-of-over frame nothing more is encoded (FreeDV stops taking
// microphone audio once the over is ending) until resetTx starts a new over.
void TestRadeChannel::noSpeechAfterEndOfOverUntilReset()
{
    RadeChannel ch;
    QVERIFY(ch.start("dummy"));
    QVERIFY(ch.queueEndOfOver(QString()));

    QSignalSpy modemSpy(&ch, &RadeChannel::txModemReady);
    ch.txEncode(makeSyntheticSpeech16k(16000));
    QCOMPARE(ch.radeTxCallCountForTest(), 0);
    QCOMPARE(modemSpy.count(), 0);

    ch.resetTx();
    QVERIFY(!ch.endOfOverQueued());
    ch.txEncode(makeSyntheticSpeech16k(16000));
    QVERIFY(ch.radeTxCallCountForTest() > 0);
    ch.stop();
}

void TestRadeChannel::endOfOverWhileInactiveSendsNothing()
{
    RadeChannel ch;
    QSignalSpy modemSpy(&ch, &RadeChannel::txModemReady);
    QVERIFY(!ch.queueEndOfOver(QStringLiteral("KG4VCF")));
    QVERIFY(!ch.endOfOverQueued());
    QCOMPARE(modemSpy.count(), 0);
}

// One channel transmits an over (speech, then the end-of-over frame with
// KG4VCF); its modem audio, the real leg as a radio's SSB receiver hands it
// back (I = audio, Q = 0), goes into a second channel, which reports the
// callsign once with no grid.
void TestRadeChannel::endOfOverCallsignReachesAnotherChannel()
{
    RadeChannel tx;
    RadeChannel rx;
    QVERIFY(tx.start("dummy"));
    QVERIFY(rx.start("dummy"));

    QByteArray air;  // 24 kHz stereo float
    connect(&tx, &RadeChannel::txModemReady, this,
            [&air](const QByteArray& pcm) { air.append(pcm); });

    // 1.5 s of speech-like audio, then the end-of-over frame.
    for (int i = 0; i < 12; ++i) {
        tx.txEncode(makeSyntheticSpeech16k(2000));
    }
    QVERIFY(tx.queueEndOfOver(QStringLiteral("KG4VCF")));
    // Silence behind it: the receiver's 24 -> 8 kHz resampler holds back
    // about 300 ms.
    air.append(QByteArray(24000 * 2 * 2 * static_cast<int>(sizeof(float)), '\0'));

    QSignalSpy textSpy(&rx, &RadeChannel::rxTextDecoded);
    const auto* stereo = reinterpret_cast<const float*>(air.constData());
    const int frames = air.size() / (2 * static_cast<int>(sizeof(float)));
    constexpr int kChunk = 2048;
    for (int off = 0; off < frames; off += kChunk) {
        const int n = std::min(kChunk, frames - off);
        QByteArray iq(n * 2 * static_cast<int>(sizeof(float)), Qt::Uninitialized);
        auto* out = reinterpret_cast<float*>(iq.data());
        for (int i = 0; i < n; ++i) {
            out[2 * i] = stereo[2 * (off + i)];
            out[2 * i + 1] = 0.0f;
        }
        rx.processIq(iq);
    }

    QCOMPARE(textSpy.count(), 1);
    QCOMPARE(textSpy.first().value(0).toString(), QStringLiteral("KG4VCF"));
    QVERIFY(textSpy.first().value(1).toString().isEmpty());
    tx.stop();
    rx.stop();
}

// Fix wave (RADE EOO): FreeDV keeps encoding the microphone audio recorded
// before the release, then sends the end-of-over frame (freedv-gui
// src/pipeline/TxRxThread.cpp:808-847 [@a4ae053]). Speech held while the
// decoder had the codec goes out ahead of the EOO, and queueing the EOO
// never waits for the decoder on the main thread.
void TestRadeChannel::endOfOverEncodesHeldSpeechFirstWithoutWaiting()
{
    RadeChannel ch;
    QVERIFY(ch.start("dummy"));

    // A decode on another thread holds the codec until released.
    std::mutex gateMutex;
    std::condition_variable gateCv;
    bool released = false;
    std::atomic<int> entered{0};
    auto release = [&] {
        {
            std::lock_guard<std::mutex> l(gateMutex);
            released = true;
        }
        gateCv.notify_all();
    };
    ch.setRxDecodeLockedHookForTest([&] {
        entered.fetch_add(1);
        std::unique_lock<std::mutex> l(gateMutex);
        gateCv.wait(l, [&] { return released; });
    });
    const QByteArray iq(2048 * 2 * static_cast<int>(sizeof(float)), '\0');
    std::thread decoder([&] { ch.processIq(iq); });

    // A watchdog frees the decoder after 5 s, so a call that waited for it
    // returns only then.
    std::atomic<bool> watchdogFired{false};
    std::atomic<bool> done{false};
    std::thread watchdog([&] {
        for (int i = 0; i < 500 && !done.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!done.load()) {
            watchdogFired.store(true);
            release();
        }
    });
    struct JoinOnExit {
        std::function<void()> release;
        std::atomic<bool>& done;
        std::thread& a;
        std::thread& b;
        ~JoinOnExit()
        {
            done.store(true);
            release();
            if (a.joinable()) {
                a.join();
            }
            if (b.joinable()) {
                b.join();
            }
        }
    } joinOnExit{release, done, watchdog, decoder};

    QTRY_VERIFY_WITH_TIMEOUT(entered.load() == 1, 5000);

    QSignalSpy modemSpy(&ch, &RadeChannel::txModemReady);
    ch.txEncode(makeSyntheticSpeech16k(16000));
    QVERIFY(ch.txHeldBytesForTest() > 0);
    QCOMPARE(ch.radeTxCallCountForTest(), 0);

    bool sent = false;
    QVERIFY(ch.queueEndOfOver(QStringLiteral("KG4VCF"), [&sent] { sent = true; }));
    const bool waited = watchdogFired.load();
    QVERIFY(ch.endOfOverQueued());
    QVERIFY2(!waited, "queueEndOfOver waited for the decoder");
    QCOMPARE(modemSpy.count(), 0);
    QVERIFY(!sent);

    // The decode ends; the held speech, then the EOO, go out.
    ch.setRxDecodeLockedHookForTest({});
    release();
    decoder.join();
    QTRY_VERIFY_WITH_TIMEOUT(sent, 5000);
    QCOMPARE(ch.txHeldBytesForTest(), 0);
    QVERIFY2(ch.radeTxCallCountForTest() > 0,
             qPrintable(QStringLiteral("radeTx=%1").arg(ch.radeTxCallCountForTest())));
    QVERIFY2(modemSpy.count() == ch.radeTxCallCountForTest() + 1,
             qPrintable(QStringLiteral("chunks=%1 radeTx=%2")
                            .arg(modemSpy.count()).arg(ch.radeTxCallCountForTest())));
    // The last chunk carries the EOO and the silence: at least
    // (1152 + 1600) x 3 stereo float frames at 24 kHz.
    const int lastFrames = modemSpy.last().value(0).toByteArray().size()
        / (2 * static_cast<int>(sizeof(float)));
    QVERIFY2(lastFrames >= (1152 + 1600) * 3 - 3,
             qPrintable(QStringLiteral("%1 frames").arg(lastFrames)));

    // Sent once; a dropped over sends no second one.
    sent = false;
    ch.dropTxAudio();
    QCoreApplication::processEvents();
    QVERIFY(!sent);
    ch.stop();
}

QTEST_GUILESS_MAIN(TestRadeChannel)
#include "tst_rade_channel.moc"
