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
//
// Test seam: masterMixForTest() (NEREUS_BUILD_TESTS) exposes m_masterMix so
// we can call tryDrain() to observe accumulated audio without needing a full
// IAudioBus pipeline.
//
// Plan: 3M-1b E.3. Pre-code review §4.3 + §4.4.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/audio/MasterMixer.h"

#include <vector>
#include <cmath>

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
        engine.masterMixForTest().tryDrain(out.data(), frames);
        return out;
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
};

QTEST_MAIN(TstAudioEngineTxMonitorBlock)
#include "tst_audio_engine_tx_monitor_block.moc"
