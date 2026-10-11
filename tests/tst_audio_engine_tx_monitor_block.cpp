// =================================================================
// tests/tst_audio_engine_tx_monitor_block.cpp  (NereusSDR)
// =================================================================
//
// Exercises AudioEngine::txMonitorBlockReady — the audio-thread consumer
// of TxChannel::sip1OutputReady added in Phase 3M-1b E.3.
//
// Coverage:
//   - Disabled monitor: accumulate NOT called -> tryDrain returns silence.
//   - Enabled monitor: accumulate called -> tryDrain returns non-zero audio.
//   - Volume applied: gain stored via setSliceGain is respected by accumulate.
//   - Null samples guard: null pointer → no-op (no crash, no output).
//   - Zero frames guard: frames=0 → no-op.
//   - Volume=0 still calls accumulate (gain-zero is valid; mixer returns silence).
//   - Stereo expansion: mono input L=sample, R=sample in output.
//   - Hand-off: a block reaches the mixer only when the DSP thread pumps it
//     (the transmit thread never touches the mixer).
//   - Rate: 192 kHz blocks of 256 frames (a Protocol 2 radio's transmit
//     channel output) come out as 64 frames of the same tone at 48 kHz.
//     Before 2026-10-10 they were mixed unconverted: a 1 kHz tone played
//     at 250 Hz and the ring dropped three quarters of it.
//   - Rate change: the resampler follows setTxMonitorSampleRate.
//   - A full hand-off drops whole blocks, counts them, and keeps order.
//   - Two threads: the transmit thread feeds while the DSP thread drains.
//
// Test seam: masterMixForTest() (NEREUS_BUILD_TESTS) exposes m_masterMix so
// we can call tryDrain() to observe accumulated audio without needing a full
// IAudioBus pipeline. pumpTxMonitorForTest() runs the DSP thread's half of
// the hand-off, which drainMixes runs before every drain.
//
// Plan: 3M-1b E.3. Pre-code review §4.3 + §4.4.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/audio/MasterMixer.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <thread>
#include <vector>

using namespace NereusSDR;

class TstAudioEngineTxMonitorBlock : public QObject {
    Q_OBJECT

private:

    // Helper: drain the engine's MasterMixer and return what came out.
    //
    // Phase 3F replaced mixInto() with tryDrain(), which returns the frame
    // count it wrote and writes nothing at all when the readiness barrier
    // is unsatisfied. That does not change what this test observes: the TX
    // monitor occupies kTxMonitorSlotId, which is marked opportunistic and
    // so is never a barrier member, meaning a queued monitor block always
    // drains here regardless of what any RX slice is doing.
    //
    // out is zero-initialised and tryDrain only writes the frames it
    // produced, so a short or empty drain leaves the remainder silent and
    // hasAudio() still reports correctly.
    static std::vector<float> drainMix(AudioEngine& engine, int frames)
    {
        std::vector<float> out(static_cast<size_t>(frames * 2), 0.0f);
        // The DSP thread's half of the hand-off, as drainMixes runs it.
        engine.pumpTxMonitorForTest();
        engine.masterMixForTest().tryDrain(out.data(), frames);
        return out;
    }

    static constexpr double kPi = 3.14159265358979323846;
    static constexpr int kMixRate = 48000;
    static constexpr double kToneHz = 1000.0;
    static constexpr float kToneAmp = 0.5f;

    // One block of a continuous tone, `frames` long, starting at sample
    // `startFrame` of a stream at `rate`.
    static std::vector<float> toneBlock(int rate, qint64 startFrame, int frames)
    {
        std::vector<float> block(static_cast<size_t>(frames));
        for (int i = 0; i < frames; ++i) {
            const double t = static_cast<double>(startFrame + i) / rate;
            block[static_cast<size_t>(i)] =
                kToneAmp * static_cast<float>(std::sin(2.0 * kPi * kToneHz * t));
        }
        return block;
    }

    // Pumps and drains everything queued; appends the left leg to `left`
    // and returns the frames drained. Fails the caller's test through
    // `stereoEqual` when L and R ever differ.
    static int pumpAndDrainAll(AudioEngine& engine, std::vector<float>& left,
                               bool& stereoEqual)
    {
        constexpr int kChunk = 256;
        std::array<float, kChunk * 2> out{};
        int total = 0;
        engine.pumpTxMonitorForTest();
        for (;;) {
            const int n = engine.masterMixForTest().tryDrain(out.data(), kChunk);
            if (n <= 0) {
                return total;
            }
            for (int i = 0; i < n; ++i) {
                const float l = out[static_cast<size_t>(i) * 2 + 0];
                const float r = out[static_cast<size_t>(i) * 2 + 1];
                if (l != r) {
                    stereoEqual = false;
                }
                left.push_back(l);
            }
            total += n;
        }
    }

    // Rising zero crossings per second over `x`, sampled at kMixRate.
    static double toneFrequency(const std::vector<float>& x, size_t from)
    {
        int crossings = 0;
        for (size_t i = from + 1; i < x.size(); ++i) {
            if (x[i - 1] < 0.0f && x[i] >= 0.0f) {
                ++crossings;
            }
        }
        const double seconds = static_cast<double>(x.size() - from) / kMixRate;
        return crossings / seconds;
    }

    static float peak(const std::vector<float>& x, size_t from)
    {
        float p = 0.0f;
        for (size_t i = from; i < x.size(); ++i) {
            p = std::max(p, std::fabs(x[i]));
        }
        return p;
    }

    static float largestStep(const std::vector<float>& x, size_t from)
    {
        float step = 0.0f;
        for (size_t i = from + 1; i < x.size(); ++i) {
            step = std::max(step, std::fabs(x[i] - x[i - 1]));
        }
        return step;
    }

    // The largest step between neighbouring samples of the tone at the mix
    // rate: amp * 2 * sin(pi * f / rate). A block lost or played twice
    // shifts the tone by 64 frames, a third of a cycle, which is a step of
    // about 0.87 times the amplitude, more than ten times this bound.
    static float toneStepBound()
    {
        return kToneAmp * 2.0f
            * static_cast<float>(std::sin(kPi * kToneHz / kMixRate)) * 1.05f;
    }

    // Helper: true when any sample in the buffer is non-zero.
    static bool hasAudio(const std::vector<float>& buf)
    {
        for (float s : buf) {
            if (s != 0.0f) { return true; }
        }
        return false;
    }

private slots:

    // ── 1. Disabled → no mixer output ─────────────────────────────────────

    void txMonitorBlockReady_disabled_noOutput()
    {
        AudioEngine engine;
        // Monitor disabled by default.
        QCOMPARE(engine.txMonitorEnabled(), false);

        const std::vector<float> input = {0.5f, 0.8f, -0.3f, 0.1f};
        engine.txMonitorBlockReady(input.data(), static_cast<int>(input.size()));

        // Nothing should have been accumulated.
        const auto out = drainMix(engine, static_cast<int>(input.size()));
        QVERIFY(!hasAudio(out));
    }

    // ── 2. Enabled → mixer receives audio ─────────────────────────────────

    void txMonitorBlockReady_enabled_hasOutput()
    {
        AudioEngine engine;
        engine.setTxMonitorEnabled(true);

        const std::vector<float> input = {0.5f, 0.8f, -0.3f, 0.1f};
        engine.txMonitorBlockReady(input.data(), static_cast<int>(input.size()));

        const auto out = drainMix(engine, static_cast<int>(input.size()));
        QVERIFY(hasAudio(out));
    }

    // ── 3. Volume applied ─────────────────────────────────────────────────
    //
    // With volume = 1.0, the gain stored in MasterMixer equals 1.0, so
    // accumulate() passes samples through unscaled. With volume = 0.5 the
    // output should be half as large. We compare the two cases numerically.

    void txMonitorBlockReady_volumeScalesOutput()
    {
        // Ramp collapsed to a single frame so this keeps asserting the
        // steady-state gain. Phase 3F gave MasterMixer a 240-frame (5 ms)
        // anti-click ramp, and these blocks are 4 frames long, so both
        // volumes would still be climbing and the ratio would read 1.0
        // rather than 0.5. The ramp itself is covered in tst_master_mixer.
        AudioEngine full;
        full.masterMixForTest().setRampFrames(1);
        full.setTxMonitorEnabled(true);
        full.setTxMonitorVolume(1.0f);

        const std::vector<float> input = {1.0f, 1.0f, 1.0f, 1.0f};
        full.txMonitorBlockReady(input.data(), static_cast<int>(input.size()));
        const auto outFull = drainMix(full, static_cast<int>(input.size()));

        // Half volume
        AudioEngine half;
        half.masterMixForTest().setRampFrames(1);
        half.setTxMonitorEnabled(true);
        half.setTxMonitorVolume(0.5f);

        half.txMonitorBlockReady(input.data(), static_cast<int>(input.size()));
        const auto outHalf = drainMix(half, static_cast<int>(input.size()));

        // outHalf[i] should be ~0.5 × outFull[i] for all i.
        QVERIFY(!outFull.empty());
        for (int i = 0; i < static_cast<int>(outFull.size()); ++i) {
            const float ratio = (outFull[i] != 0.0f)
                ? outHalf[i] / outFull[i]
                : 0.0f;
            QVERIFY2(std::fabs(ratio - 0.5f) < 1e-5f,
                     qPrintable(QString("sample %1: ratio %2, expected 0.5")
                                    .arg(i).arg(static_cast<double>(ratio))));
        }
    }

    // ── 4. Null samples guard ─────────────────────────────────────────────

    void txMonitorBlockReady_nullSamples_noOp()
    {
        AudioEngine engine;
        engine.setTxMonitorEnabled(true);

        // Must not crash; mixer must stay empty.
        engine.txMonitorBlockReady(nullptr, 4);

        const auto out = drainMix(engine, 4);
        QVERIFY(!hasAudio(out));
    }

    // ── 5. Zero frames guard ──────────────────────────────────────────────

    void txMonitorBlockReady_zeroFrames_noOp()
    {
        AudioEngine engine;
        engine.setTxMonitorEnabled(true);

        const std::vector<float> input = {0.5f, 0.5f};
        engine.txMonitorBlockReady(input.data(), 0);

        const auto out = drainMix(engine, 2);
        QVERIFY(!hasAudio(out));
    }

    // ── 6. Volume = 0 still calls accumulate (produces silence, not no-op) ─
    //
    // A user can set monitor volume to 0 (slider at minimum) and still
    // have MON enabled. The accumulate() call must still go through — the
    // zero-gain slot produces silence, which is correct. The test verifies
    // that a subsequent enable=true + volume=1.0 call does produce audio,
    // distinguishing the "volume=0 disabled" case from "slot never reached".

    void txMonitorBlockReady_volumeZero_thenNonZero_hasOutput()
    {
        AudioEngine engine;
        engine.setTxMonitorEnabled(true);
        engine.setTxMonitorVolume(0.0f);

        const std::vector<float> input = {1.0f, 1.0f};
        engine.txMonitorBlockReady(input.data(), static_cast<int>(input.size()));
        {
            const auto out = drainMix(engine, static_cast<int>(input.size()));
            QVERIFY(!hasAudio(out));  // silence at gain=0
        }

        // Now set volume to 1.0 and verify we get audio.
        engine.setTxMonitorVolume(1.0f);
        engine.txMonitorBlockReady(input.data(), static_cast<int>(input.size()));
        {
            const auto out = drainMix(engine, static_cast<int>(input.size()));
            QVERIFY(hasAudio(out));
        }
    }

    // ── 7. Stereo expansion: L == R == input sample ───────────────────────

    void txMonitorBlockReady_monoExpandedToStereo_LequalsR()
    {
        // Steady-state gain assertion, so collapse the anti-click ramp to a
        // single frame (see txMonitorBlockReady_volumeScalesOutput).
        AudioEngine engine;
        engine.masterMixForTest().setRampFrames(1);
        engine.setTxMonitorEnabled(true);
        engine.setTxMonitorVolume(1.0f);

        // Single mono sample: 0.7f.
        const std::vector<float> input = {0.7f};
        engine.txMonitorBlockReady(input.data(), static_cast<int>(input.size()));

        const auto out = drainMix(engine, static_cast<int>(input.size()));
        // out[0] = L, out[1] = R. Both should equal 0.7f (at gain 1.0).
        QCOMPARE(out.size(), static_cast<size_t>(2));
        QVERIFY2(std::fabs(out[0] - 0.7f) < 1e-5f,
                 qPrintable(QString("L = %1, expected 0.7")
                                .arg(static_cast<double>(out[0]))));
        QVERIFY2(std::fabs(out[1] - 0.7f) < 1e-5f,
                 qPrintable(QString("R = %1, expected 0.7")
                                .arg(static_cast<double>(out[1]))));
    }

    // ── 8. Enable-then-disable silences subsequent blocks ─────────────────

    void txMonitorBlockReady_disableAfterEnable_silencesNextBlock()
    {
        AudioEngine engine;
        engine.setTxMonitorEnabled(true);

        const std::vector<float> input = {0.5f, 0.5f};

        // First call — enabled.
        engine.txMonitorBlockReady(input.data(), static_cast<int>(input.size()));
        drainMix(engine, static_cast<int>(input.size()));  // flush

        // Disable.
        engine.setTxMonitorEnabled(false);

        // Second call — disabled.
        engine.txMonitorBlockReady(input.data(), static_cast<int>(input.size()));
        const auto out = drainMix(engine, static_cast<int>(input.size()));
        QVERIFY(!hasAudio(out));
    }

    // ── 9. The transmit thread never touches the mixer ────────────────────
    //
    // MasterMixer's rings belong to the thread that drains them. The
    // monitor block arrives on the transmit thread, so it must wait in the
    // hand-off until the DSP thread pumps it.

    void txMonitorBlockReady_reachesMixerOnlyAtDspPump()
    {
        AudioEngine engine;
        engine.masterMixForTest().setRampFrames(1);
        engine.setTxMonitorEnabled(true);
        engine.setTxMonitorVolume(1.0f);

        const std::vector<float> input(64, 0.5f);
        engine.txMonitorBlockReady(input.data(), 64);

        std::vector<float> out(128, 0.0f);
        QCOMPARE(engine.masterMixForTest().tryDrain(out.data(), 64), 0);
        QVERIFY(!hasAudio(out));

        engine.pumpTxMonitorForTest();
        QCOMPARE(engine.masterMixForTest().tryDrain(out.data(), 64), 64);
        QVERIFY(hasAudio(out));
        QCOMPARE(out[0], 0.5f);
        QCOMPARE(out[127], 0.5f);
    }

    // ── 10. 192 kHz blocks play at 48 kHz ─────────────────────────────────
    //
    // A Protocol 2 radio's transmit channel puts out 256 frames at 192 kHz
    // for every 64-frame mic block (WdspEngine::createTxChannel). The mix
    // is 48 kHz, so each must come out as 64 frames of the same tone.

    void txMonitorBlockReady_192kBlocks_playAt48k()
    {
#ifndef HAVE_WDSP
        QSKIP("needs WDSP's resampler");
#else
        constexpr int kInRate = 192000;
        constexpr int kInFrames = 256;
        constexpr int kBlocks = 300;

        AudioEngine engine;
        engine.masterMixForTest().setRampFrames(1);
        engine.setTxMonitorSampleRate(kInRate);
        QCOMPARE(engine.txMonitorSampleRate(), kInRate);
        engine.setTxMonitorEnabled(true);
        engine.setTxMonitorVolume(1.0f);

        std::vector<float> left;
        bool stereoEqual = true;
        for (int b = 0; b < kBlocks; ++b) {
            const std::vector<float> block =
                toneBlock(kInRate, static_cast<qint64>(b) * kInFrames, kInFrames);
            engine.txMonitorBlockReady(block.data(), kInFrames);
            // One mixer period per transmit block: exactly 64 frames.
            QCOMPARE(pumpAndDrainAll(engine, left, stereoEqual), 64);
        }
        QCOMPARE(left.size(), static_cast<size_t>(kBlocks) * 64);
        QVERIFY(stereoEqual);
        QCOMPARE(engine.txMonitorDroppedFramesForTest(), quint64(0));

        // Past the resampler's filter filling (about 1.4 ms) with margin.
        const size_t settled = 2400;
        const double hz = toneFrequency(left, settled);
        QVERIFY2(std::fabs(hz - kToneHz) < 5.0,
                 qPrintable(QString("tone at %1 Hz, expected 1000").arg(hz)));
        const float p = peak(left, settled);
        QVERIFY2(std::fabs(p - kToneAmp) < 0.015f,
                 qPrintable(QString("peak %1, expected 0.5").arg(static_cast<double>(p))));
        const float step = largestStep(left, settled);
        QVERIFY2(step <= toneStepBound(),
                 qPrintable(QString("step %1 over the tone's own %2")
                                .arg(static_cast<double>(step))
                                .arg(static_cast<double>(toneStepBound()))));
#endif
    }

    // ── 11. The resampler follows the rate ────────────────────────────────

    void txMonitorBlockReady_rateChange_followsNewRate()
    {
#ifndef HAVE_WDSP
        QSKIP("needs WDSP's resampler");
#else
        AudioEngine engine;
        engine.masterMixForTest().setRampFrames(1);
        engine.setTxMonitorEnabled(true);
        engine.setTxMonitorVolume(1.0f);

        std::vector<float> left;
        bool stereoEqual = true;

        engine.setTxMonitorSampleRate(192000);
        for (int b = 0; b < 8; ++b) {
            const std::vector<float> block = toneBlock(192000, b * 256, 256);
            engine.txMonitorBlockReady(block.data(), 256);
            QCOMPARE(pumpAndDrainAll(engine, left, stereoEqual), 64);
        }
        engine.setTxMonitorSampleRate(96000);
        for (int b = 0; b < 8; ++b) {
            const std::vector<float> block = toneBlock(96000, b * 128, 128);
            engine.txMonitorBlockReady(block.data(), 128);
            QCOMPARE(pumpAndDrainAll(engine, left, stereoEqual), 64);
        }
        // Zero and negative rates are ignored.
        engine.setTxMonitorSampleRate(0);
        engine.setTxMonitorSampleRate(-48000);
        QCOMPARE(engine.txMonitorSampleRate(), 96000);

        // At the mix rate nothing is resampled: sample for sample.
        engine.setTxMonitorSampleRate(kMixRate);
        left.clear();
        const std::vector<float> block = toneBlock(kMixRate, 0, 64);
        engine.txMonitorBlockReady(block.data(), 64);
        QCOMPARE(pumpAndDrainAll(engine, left, stereoEqual), 64);
        QCOMPARE(left, block);
        QVERIFY(stereoEqual);
#endif
    }

    // ── 12. A full hand-off drops whole blocks and keeps order ────────────
    //
    // With no drain at all the hand-off fills: 4095 frames hold 63 blocks
    // of 64. The 64th is dropped whole and counted, and what was queued
    // comes out complete and in order.

    void txMonitorBlockReady_handoffFull_dropsWholeBlock()
    {
        AudioEngine engine;
        engine.masterMixForTest().setRampFrames(1);
        engine.setTxMonitorEnabled(true);
        engine.setTxMonitorVolume(1.0f);

        constexpr int kFrames = 64;
        constexpr int kHeld = 63;
        std::vector<float> block(kFrames);
        for (int b = 0; b < kHeld + 1; ++b) {
            for (int i = 0; i < kFrames; ++i) {
                block[static_cast<size_t>(i)] =
                    static_cast<float>(b * kFrames + i + 1) / 8192.0f;
            }
            engine.txMonitorBlockReady(block.data(), kFrames);
            QCOMPARE(engine.txMonitorDroppedFramesForTest(),
                     quint64(b < kHeld ? 0 : kFrames));
        }

        std::vector<float> left;
        bool stereoEqual = true;
        QCOMPARE(pumpAndDrainAll(engine, left, stereoEqual), kHeld * kFrames);
        QVERIFY(stereoEqual);
        for (size_t i = 0; i < left.size(); ++i) {
            QCOMPARE(left[i], static_cast<float>(i + 1) / 8192.0f);
        }
    }

    // ── 13. The transmit thread feeds while the DSP thread drains ─────────
    //
    // The shape in the running Core: TxWorkerThread calls
    // txMonitorBlockReady while the DSP thread drains. Nothing may be
    // lost, repeated or reordered. No clock is involved: the feeder sends a
    // batch that fits the hand-off, then waits until the drainer has made
    // two full passes that found it empty.

    void txMonitorBlockReady_feedAndDrainOnTwoThreads()
    {
#ifndef HAVE_WDSP
        QSKIP("needs WDSP's resampler");
#else
        constexpr int kInRate = 192000;
        constexpr int kInFrames = 256;
        constexpr int kBatches = 100;
        constexpr int kBlocksPerBatch = 32;  // 2048 frames at 48 kHz, half the hand-off

        AudioEngine engine;
        engine.masterMixForTest().setRampFrames(1);
        engine.setTxMonitorSampleRate(kInRate);
        engine.setTxMonitorEnabled(true);
        engine.setTxMonitorVolume(1.0f);

        std::atomic<quint64> emptyPasses{0};
        std::atomic<bool> feederDone{false};

        std::thread feeder([&]() {
            qint64 frame = 0;
            for (int batch = 0; batch < kBatches; ++batch) {
                for (int b = 0; b < kBlocksPerBatch; ++b) {
                    const std::vector<float> block = toneBlock(kInRate, frame, kInFrames);
                    engine.txMonitorBlockReady(block.data(), kInFrames);
                    frame += kInFrames;
                }
                const quint64 seen = emptyPasses.load(std::memory_order_acquire);
                while (emptyPasses.load(std::memory_order_acquire) < seen + 2) {
                    std::this_thread::yield();
                }
            }
            feederDone.store(true, std::memory_order_release);
        });

        std::vector<float> left;
        left.reserve(static_cast<size_t>(kBatches) * kBlocksPerBatch * 64);
        bool stereoEqual = true;
        for (;;) {
            const bool done = feederDone.load(std::memory_order_acquire);
            const int n = pumpAndDrainAll(engine, left, stereoEqual);
            emptyPasses.fetch_add(1, std::memory_order_release);
            if (done && n == 0) {
                break;
            }
        }
        feeder.join();

        QCOMPARE(engine.txMonitorDroppedFramesForTest(), quint64(0));
        QCOMPARE(left.size(), static_cast<size_t>(kBatches) * kBlocksPerBatch * 64);
        QVERIFY(stereoEqual);
        const size_t settled = 2400;
        const double hz = toneFrequency(left, settled);
        QVERIFY2(std::fabs(hz - kToneHz) < 5.0,
                 qPrintable(QString("tone at %1 Hz, expected 1000").arg(hz)));
        const float step = largestStep(left, settled);
        QVERIFY2(step <= toneStepBound(),
                 qPrintable(QString("step %1 over the tone's own %2")
                                .arg(static_cast<double>(step))
                                .arg(static_cast<double>(toneStepBound()))));
#endif
    }
};

QTEST_MAIN(TstAudioEngineTxMonitorBlock)
#include "tst_audio_engine_tx_monitor_block.moc"
