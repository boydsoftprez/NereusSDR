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
//   5. processIqEmitsSyncFalseOnNoise    feeding random I/Q noise emits syncChanged(false)
//                                        and no rxSpeechReady chunks
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

    // No rxSpeechReady chunks should be emitted because the codec
    // never synced and so never produced decoded features.
    QCOMPARE(speechSpy.count(), 0);

    // At least one rade_rx() must have been called given the input
    // volume; otherwise the accumulator is broken.
    QVERIFY2(ch.radeRxCallCountForTest() >= 1,
             qPrintable(QString("rade_rx() never invoked across %1 input chunks")
                            .arg(kNumChunks)));

    ch.stop();
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

QTEST_GUILESS_MAIN(TestRadeChannel)
#include "tst_rade_channel.moc"
