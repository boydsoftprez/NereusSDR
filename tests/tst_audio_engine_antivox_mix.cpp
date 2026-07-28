// =================================================================
// tests/tst_audio_engine_antivox_mix.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. Thetis citations
// below are rationale for the topology, not ported code.
//
// Phase 3F Sub-Epic J Task 9. Anti-VOX was fed only from the stream
// hosting slice 0, so the canceller heard receiver A alone. With a second
// receiver audible the reference no longer matches what reaches the
// speakers, and antivox_level then drives
//   asig = avsig - antivox_gain * antivox_level
// (Thetis dexp.c:313-316 [v2.10.3.15]) either too hard or not hard
// enough: a false VOX trigger, meaning unintended transmit, or a failure
// to cancel.
//
// Thetis feeds EVERY sub-receiver to a dedicated per-transmitter mixer
// (cmaster.c:371-372 [v2.10.3.15]) whose membership is explicit, through
// the same SetAAudioMixStates machinery as the RX mixer
// (cmaster.c:584-588 [v2.10.3.15]).
//
// The mixer is exercised directly rather than through rxBlockReady
// because the property under test is the mix itself: what it sums, what
// it refuses to sum, and how often it lets a block out. rxBlockReady's
// own wiring is covered by tst_audio_engine_multi_slice_mix.
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/audio/MasterMixer.h"
#include "models/RadioModel.h"

#include <array>
#include <cmath>

using namespace NereusSDR;

class TstAudioEngineAntiVoxMix : public QObject {
    Q_OBJECT

private slots:

    // ── Every slice reaches the reference, not just slice A. ──────────────

    void theAntiVoxMixSumsEverySlice()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        AudioEngine* engine = radio.audioEngine();
        QVERIFY(engine != nullptr);

        MasterMixer& av = engine->antiVoxMixForTest();
        av.setRampFrames(1);   // assert steady-state gain, not the fade-in

        std::array<float, 2> a = {0.3f, 0.3f};
        std::array<float, 2> b = {0.4f, 0.4f};
        std::array<float, 2> out{};

        av.setSliceGain(0, 1.0f, 0.0f);
        av.setSliceGain(1, 1.0f, 0.0f);
        av.accumulate(0, a.data(), 1);
        av.accumulate(1, b.data(), 1);

        QCOMPARE(av.tryDrain(out.data(), 1), 1);
        QCOMPARE(out[0], 0.7f);
    }

    // ── Zero slew. ────────────────────────────────────────────────────────
    //
    // The DEXP reference must be amplitude-faithful from the first sample.
    // Thetis creates this mixer with 0.000 on all four slew parameters
    // (cmaster.c:159-175 [v2.10.3.15]), unlike the RX mixer's 0.010
    // (cmaster.c:297-313 [v2.10.3.15]). A fade on the reference would
    // under-report the audio the operator is actually hearing for the
    // length of the fade, which is exactly the window a transition opens.

    void theAntiVoxMixDoesNotSlew()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        MasterMixer& av = radio.audioEngine()->antiVoxMixForTest();
        av.setRampFrames(1);
        av.setSliceGain(0, 1.0f, 0.0f);

        std::array<float, 2> in = {1.0f, 1.0f};
        std::array<float, 2> out{};

        // A membership change is what arms the up-slew on the speakers
        // mixer, so drive one here and then assert full amplitude on the
        // very first frame after it.
        av.setSliceStreaming(0, false);
        av.setSliceStreaming(0, true);
        av.accumulate(0, in.data(), 1);
        QCOMPARE(av.tryDrain(out.data(), 1), 1);
        QVERIFY(std::abs(out[0] - 1.0f) < 0.0001f);
    }

    // ── The TX monitor never feeds anti-VOX. ──────────────────────────────
    //
    // Upstream's masks are drawn from RX1 + RX1S + RX2 and never include
    // MON, while the speakers mixer alongside them gets RX1 + RX1S + RX2 +
    // MON on the same line (console.cs:27650-27771 [v2.10.3.15]). Monitor
    // audio suppressing the operator's own VOX would be feedback by
    // definition, so kTxMonitorSlotId is simply never registered with this
    // instance and accumulate() drops it.

    void theTxMonitorNeverFeedsAntiVox()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        MasterMixer& av = radio.audioEngine()->antiVoxMixForTest();

        std::array<float, 2> mon = {0.9f, 0.9f};
        std::array<float, 2> out{};
        // kTxMonitorSlotId is never registered with this instance, so an
        // accumulate for it is dropped.
        av.accumulate(/*kTxMonitorSlotId*/ -2, mon.data(), 1);
        QCOMPARE(av.tryDrain(out.data(), 1), 0);
    }

    // ── The cadence constraint. ───────────────────────────────────────────
    //
    // The slice-0 gate this replaces guaranteed exactly one block per
    // outSize/outRate seconds, which is what DEXP was configured for, and
    // it held because inSize = 64 * rate / 48000 makes
    // inSize/inputRate == outSize/outRate per stream. Barrier pacing now
    // supplies it: one drained block per period, whatever the stream width.
    //
    // Get this wrong in either direction and the failure this task exists
    // to fix returns by another route. Too many blocks and DEXP integrates
    // its single-pole IIR faster than antivox_rate says it should; too few
    // and antivox_level goes stale.

    void theAntiVoxMixDrainsOneBlockPerPeriod()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        MasterMixer& av = radio.audioEngine()->antiVoxMixForTest();
        av.setRampFrames(1);
        av.setSliceGain(0, 1.0f, 0.0f);
        av.setSliceGain(1, 1.0f, 0.0f);

        std::array<float, 2> blk = {0.5f, 0.5f};
        std::array<float, 2> out{};

        // Both members deliver once: exactly one block leaves.
        av.accumulate(0, blk.data(), 1);
        av.accumulate(1, blk.data(), 1);
        QCOMPARE(av.tryDrain(out.data(), 1), 1);
        QCOMPARE(av.tryDrain(out.data(), 1), 0);

        // One member alone delivers: nothing leaves until the other does.
        // This is the half that an opportunistic mix would fail, and an
        // opportunistic anti-VOX mix has no pacing at all.
        av.accumulate(0, blk.data(), 1);
        QCOMPARE(av.tryDrain(out.data(), 1), 0);
        av.accumulate(1, blk.data(), 1);
        QCOMPARE(av.tryDrain(out.data(), 1), 1);
    }

    // ── The feed is actually wired to the RX audio entry point. ───────────
    //
    // rxBlockReady is the single place slice audio arrives, and it must
    // hand the same block to both mixes, then release one anti-VOX block
    // per period. Connected DirectConnection because the emitted pointer is
    // thread_local scratch that the next block overwrites, which is the
    // contract the signal declares.

    void rxBlockReadyFeedsAndDrainsTheAntiVoxMix()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);
        AudioEngine* engine = radio.audioEngine();
        QVERIFY(engine != nullptr);
        engine->antiVoxMixForTest().setRampFrames(1);

        const int a = radio.addSlice();
        QCOMPARE(a, 0);

        int   emits      = 0;
        int   gotFrames  = 0;
        float gotFirst   = 0.0f;
        QObject::connect(engine, &AudioEngine::antiVoxBlockReady, engine,
                         [&](const float* s, int n) {
                             ++emits;
                             gotFrames = n;
                             gotFirst  = s[0];
                         },
                         Qt::DirectConnection);

        // 2 frames of interleaved stereo, all non-zero so a dropped feed
        // shows up as silence rather than as a passing zero comparison.
        const std::array<float, 4> block = {0.10f, 0.20f, -0.30f, -0.40f};
        engine->rxBlockReady(a, block.data(), 2);

        QCOMPARE(emits, 1);
        QCOMPARE(gotFrames, 2);
        QCOMPARE(gotFirst, 0.10f);
    }
};

QTEST_MAIN(TstAudioEngineAntiVoxMix)
#include "tst_audio_engine_antivox_mix.moc"
