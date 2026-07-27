#include <QtTest/QtTest>
#include "core/audio/MasterMixer.h"
#include <array>

using namespace NereusSDR;

// Phase 3F reworked MasterMixer from "accumulate then unconditionally
// flush" into per-slice rings behind a readiness barrier, so N slices
// produce ONE mixed block per audio period rather than N pushes.
//
// The gain/pan/mute cases below set the ramp to a single frame so they
// keep asserting steady-state values. The ramp itself is covered
// separately at the bottom; it is 240 frames (5 ms) in production, which
// is what stops a slice joining or leaving from clicking.
class TstMasterMixer : public QObject {
    Q_OBJECT
private slots:
    void emptyMixDrainsNothing() {
        MasterMixer mix;
        std::array<float, 16> out{};
        // Nothing queued, so nothing leaves. Returning 0 (rather than a
        // block of silence) is what keeps the caller from pushing.
        QCOMPARE(mix.tryDrain(out.data(), 8), 0);
    }

    void singleSliceUnityGainMixesThrough() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSliceGain(42, 1.0f, 0.0f);  // unity, center
        std::array<float, 4> in = {0.5f, 0.5f, -0.25f, -0.25f};  // 2 frames stereo
        mix.accumulate(42, in.data(), 2);
        std::array<float, 4> out{};
        QCOMPARE(mix.tryDrain(out.data(), 2), 2);
        QCOMPARE(out[0], 0.5f);
        QCOMPARE(out[1], 0.5f);
        QCOMPARE(out[2], -0.25f);
        QCOMPARE(out[3], -0.25f);
    }

    // The single-slice path must still drain inside the same call that
    // fed it: that is the guarantee that one slice sees no added latency.
    void aLoneSliceDrainsImmediately() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSliceGain(0, 1.0f, 0.0f);
        std::array<float, 2> in = {0.5f, 0.5f};
        mix.accumulate(0, in.data(), 1);
        std::array<float, 2> out{};
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
    }

    void twoSlicesSumLinearly() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSliceGain(1, 1.0f, 0.0f);
        mix.setSliceGain(2, 1.0f, 0.0f);
        std::array<float, 2> a = {0.3f, 0.3f};
        std::array<float, 2> b = {0.4f, 0.4f};
        mix.accumulate(1, a.data(), 1);
        mix.accumulate(2, b.data(), 1);
        std::array<float, 2> out{};
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QCOMPARE(out[0], 0.7f);
        QCOMPARE(out[1], 0.7f);
    }

    // ── The barrier, and the bench defect it fixes ────────────────────
    //
    // Two slices feeding the mix must produce ONE drained block, not two.
    // Before the rework each slice's block was flushed and pushed on its
    // own, handing the sink twice the audio it could consume.
    void oneSliceShortOfTheBarrierDrainsNothing() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSliceGain(1, 1.0f, 0.0f);
        mix.setSliceGain(2, 1.0f, 0.0f);
        std::array<float, 2> blk = {0.5f, 0.5f};
        std::array<float, 2> out{};

        // Both enrol, both deliver, one block out.
        mix.accumulate(1, blk.data(), 1);
        mix.accumulate(2, blk.data(), 1);
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QCOMPARE(mix.producingSliceCount(), 2);

        // Now only slice 1 delivers. Slice 2 is still a member, so the
        // barrier holds and nothing is pushed this period.
        mix.accumulate(1, blk.data(), 1);
        QCOMPARE(mix.tryDrain(out.data(), 1), 0);
    }

    // A member that stops delivering must not wedge the mix forever.
    // This is our stand-in for Thetis's explicit release at aamix.c:486.
    void aStalledMemberIsDemotedAndTheMixResumes() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSliceGain(1, 1.0f, 0.0f);
        mix.setSliceGain(2, 1.0f, 0.0f);
        std::array<float, 2> blk = {0.5f, 0.5f};
        std::array<float, 2> out{};

        mix.accumulate(1, blk.data(), 1);
        mix.accumulate(2, blk.data(), 1);
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);

        // Slice 2 goes quiet. Slice 1 keeps producing.
        mix.accumulate(1, blk.data(), 1);
        QCOMPARE(mix.tryDrain(out.data(), 1), 0);   // stall 1, still waiting
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);   // stall 2 -> demote, drain
        QCOMPARE(mix.producingSliceCount(), 1);
        QCOMPARE(out[0], 0.5f);                     // slice 1 alone

        // Slice 2 comes back and re-enrols on its own.
        mix.accumulate(2, blk.data(), 1);
        QCOMPARE(mix.producingSliceCount(), 2);
    }

    // The TX monitor feeds only during MOX. If it enrolled as a barrier
    // member it would stall the drain for kStallTolerance periods every
    // time TX stopped, so it is marked opportunistic and mixed in only
    // when it happens to have audio.
    void anOpportunisticSlotNeverHoldsUpTheDrain() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSliceGain(1, 1.0f, 0.0f);
        mix.setSliceGain(-2, 1.0f, 0.0f);
        mix.setSliceOpportunistic(-2, true);
        std::array<float, 2> blk = {0.5f, 0.5f};
        std::array<float, 2> out{};

        // Both feed once: the opportunistic slot is summed in.
        mix.accumulate(1, blk.data(), 1);
        mix.accumulate(-2, blk.data(), 1);
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QCOMPARE(out[0], 1.0f);
        QCOMPARE(mix.producingSliceCount(), 1);   // only slice 1 enrolled

        // Now the opportunistic slot goes quiet, as it does the moment TX
        // ends. The drain must not stall even once.
        mix.accumulate(1, blk.data(), 1);
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QCOMPARE(out[0], 0.5f);
    }

    void panFullLeftSuppressesRight() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSliceGain(1, 1.0f, -1.0f);  // full left
        std::array<float, 2> in = {0.5f, 0.5f};
        mix.accumulate(1, in.data(), 1);
        std::array<float, 2> out{};
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QVERIFY(out[0] > 0.4f);
        QVERIFY(std::abs(out[1]) < 0.0001f);
    }

    void muteZerosContribution() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSliceGain(1, 1.0f, 0.0f);
        std::array<float, 2> in = {0.9f, 0.9f};
        // Mute now rides in with the block: the audio thread must not
        // take the slice-map mutex that setSliceMuted() holds.
        mix.accumulate(1, in.data(), 1, /*muted*/ true);
        std::array<float, 2> out{};
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QCOMPARE(out[0], 0.0f);
        QCOMPARE(out[1], 0.0f);
    }

    void unknownSliceIsIgnored() {
        MasterMixer mix;
        std::array<float, 2> in = {0.9f, 0.9f};
        mix.accumulate(999, in.data(), 1);  // never registered
        std::array<float, 2> out{};
        QCOMPARE(mix.tryDrain(out.data(), 1), 0);
    }

    void drainConsumesWhatItTook() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSliceGain(1, 1.0f, 0.0f);
        std::array<float, 2> in = {0.5f, 0.5f};
        mix.accumulate(1, in.data(), 1);
        std::array<float, 2> out{};
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        // The ring is empty now, so the next drain has nothing to give.
        std::array<float, 2> out2{};
        QCOMPARE(mix.tryDrain(out2.data(), 1), 0);
    }

    void removedSliceNoLongerMixes() {
        MasterMixer mix;
        mix.setSliceGain(1, 1.0f, 0.0f);
        mix.removeSlice(1);
        std::array<float, 2> in = {0.9f, 0.9f};
        mix.accumulate(1, in.data(), 1);
        std::array<float, 2> out{};
        QCOMPARE(mix.tryDrain(out.data(), 1), 0);
    }

    // ── Anti-click ────────────────────────────────────────────────────
    //
    // A slice's first block fades in over the ramp instead of stepping
    // to full gain, which is what stops a pan or slice appearing from
    // clicking. Per-slice gain ramp rather than a port of Thetis's
    // upslew/downslew, which gates the mixed output and is built for VAC
    // stream start/stop; see MasterMixer.h.
    void aJoiningSliceFadesInRatherThanStepping() {
        MasterMixer mix;
        mix.setRampFrames(4);
        mix.setSliceGain(1, 1.0f, 0.0f);
        std::array<float, 8> in = {1.0f, 1.0f, 1.0f, 1.0f,
                                   1.0f, 1.0f, 1.0f, 1.0f};  // 4 frames
        mix.accumulate(1, in.data(), 4);
        std::array<float, 8> out{};
        QCOMPARE(mix.tryDrain(out.data(), 4), 4);

        // Strictly rising, starting well below unity and reaching it.
        QVERIFY(out[0] < 0.3f);
        QVERIFY(out[2] > out[0]);
        QVERIFY(out[4] > out[2]);
        QVERIFY(out[6] > out[4]);
        QVERIFY(std::abs(out[6] - 1.0f) < 0.0001f);
    }

    void aMutedSliceFadesOutRatherThanCutting() {
        MasterMixer mix;
        mix.setRampFrames(4);
        mix.setSliceGain(1, 1.0f, 0.0f);
        std::array<float, 8> in = {1.0f, 1.0f, 1.0f, 1.0f,
                                   1.0f, 1.0f, 1.0f, 1.0f};
        std::array<float, 8> out{};

        // Get the slice up to full gain first.
        mix.accumulate(1, in.data(), 4);
        QCOMPARE(mix.tryDrain(out.data(), 4), 4);
        QVERIFY(std::abs(out[6] - 1.0f) < 0.0001f);

        // Now mute it: the block still arrives, and fades down.
        mix.accumulate(1, in.data(), 4, /*muted*/ true);
        QCOMPARE(mix.tryDrain(out.data(), 4), 4);
        QVERIFY(out[0] < 1.0f);
        QVERIFY(out[2] < out[0]);
        QVERIFY(std::abs(out[6]) < 0.0001f);
    }
};

QTEST_APPLESS_MAIN(TstMasterMixer)
#include "tst_master_mixer.moc"
