// =================================================================
// tests/tst_master_mixer.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure. The Thetis
// aamix.c citations below are rationale, not ported code: they record
// WHY the barrier waits without a timeout and why a slice leaves the mix
// only when told to, so that a future reader does not reintroduce the
// stall-demotion heuristic these tests exist to prevent. The ported
// logic itself lives in src/core/audio/MasterMixer.{h,cpp}, which carry
// the verbatim Warren Pratt NR0V header and the PROVENANCE rows.
// =================================================================

#include <QtTest/QtTest>
#include <QSemaphore>
#include "core/audio/MasterMixer.h"
#include <array>
#include <atomic>
#include <thread>
#include <vector>
#include <cmath>

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

    // Two DDCs deliver in clumps, not alternating. A burst from one slice
    // must never evict the other: the slice is late, not gone.
    //
    // This is the 2026-07-27 ANAN-G2E bench defect ("scratchy and robo
    // digitized as soon as the second pan is up, clears when it is
    // removed"). The old 2-block stall demotion mistook a clumped-but-live
    // slice for a dead one, emitted blocks carrying one receiver instead of
    // the sum, and pushed more blocks than periods had elapsed.
    //
    // Upstream has no such rule to port: mix_main blocks on
    // WaitForMultipleObjects(nactive, Aready, TRUE, INFINITE) with no
    // timeout at all, and a stream leaves the mix only through an explicit
    // SetAAudioMixState. From Thetis aamix.c:43 and aamix.c:522
    // [v2.10.3.15].
    void aBurstFromOneSliceNeverEvictsTheOther() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSliceGain(1, 1.0f, 0.0f);
        mix.setSliceGain(2, 1.0f, 0.0f);
        std::array<float, 2> a = {0.3f, 0.3f};
        std::array<float, 2> b = {0.4f, 0.4f};
        std::array<float, 2> out{};

        // Both deliver, so both are members.
        mix.accumulate(1, a.data(), 1);
        mix.accumulate(2, b.data(), 1);
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QCOMPARE(mix.producingSliceCount(), 2);

        // Slice 1 bursts three blocks with nothing from slice 2 in flight.
        // The barrier must hold every time: no block may leave without 2.
        for (int i = 0; i < 3; ++i) {
            mix.accumulate(1, a.data(), 1);
            QCOMPARE(mix.tryDrain(out.data(), 1), 0);
        }
        QCOMPARE(mix.producingSliceCount(), 2);  // nobody evicted

        // Slice 2's clump arrives. Every block that leaves carries BOTH,
        // and the backlog drains one block per period, not all at once.
        for (int i = 0; i < 3; ++i) {
            mix.accumulate(2, b.data(), 1);
            QCOMPARE(mix.tryDrain(out.data(), 1), 1);
            QCOMPARE(out[0], 0.7f);
        }
    }

    // What replaces stall demotion. A member that stops delivering holds
    // the barrier for as long as it stays a member: the mixer cannot tell
    // "late" from "gone", so the slice lifecycle tells it instead.
    //
    // From Thetis aamix.c:522 [v2.10.3.15], SetAAudioMixState is the only
    // way a stream leaves the mix: it stops the mixer, clears the stream's
    // bit in a->active, drops it from the Aready wait set, resets its
    // accept[] gate, and restarts. Nothing times out.
    void aSliceHoldsTheBarrierUntilExplicitlyWithdrawn() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSliceGain(1, 1.0f, 0.0f);
        mix.setSliceGain(2, 1.0f, 0.0f);
        std::array<float, 2> blk = {0.5f, 0.5f};
        std::array<float, 2> out{};

        mix.accumulate(1, blk.data(), 1);
        mix.accumulate(2, blk.data(), 1);
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QCOMPARE(mix.producingSliceCount(), 2);

        // Slice 2 goes quiet. No elapsed number of periods rescues this:
        // while slice 2 is still a member the barrier holds, every time.
        for (int i = 0; i < 50; ++i) {
            mix.accumulate(1, blk.data(), 1);
            QCOMPARE(mix.tryDrain(out.data(), 1), 0);
        }
        QCOMPARE(mix.producingSliceCount(), 2);

        // The slice lifecycle withdraws it, and the mix flows again with
        // slice 1 alone.
        mix.setSliceStreaming(2, false);
        QCOMPARE(mix.producingSliceCount(), 1);
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QCOMPARE(out[0], 0.5f);

        // It rejoins once the lifecycle re-admits it and it delivers.
        mix.setSliceStreaming(2, true);
        mix.accumulate(2, blk.data(), 1);
        QCOMPARE(mix.producingSliceCount(), 2);
    }

    // Withdrawing a slice must not strand what it already queued, and must
    // not let a withdrawn slice keep contributing audio.
    void aWithdrawnSliceStopsContributing() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSliceGain(1, 1.0f, 0.0f);
        mix.setSliceGain(2, 1.0f, 0.0f);
        std::array<float, 2> a = {0.3f, 0.3f};
        std::array<float, 2> b = {0.4f, 0.4f};
        std::array<float, 2> out{};

        mix.accumulate(1, a.data(), 1);
        mix.accumulate(2, b.data(), 1);
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QCOMPARE(out[0], 0.7f);

        // Slice 2 is withdrawn while slice 1 keeps running. Output is
        // slice 1 alone from here on.
        mix.setSliceStreaming(2, false);
        mix.accumulate(1, a.data(), 1);
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QCOMPARE(out[0], 0.3f);
    }

    void withdrawal_invalidates_queued_audio_before_readmission() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSlewUpFrames(0);
        mix.setSliceGain(1, 1.0f, 0.0f);
        mix.setSliceGain(2, 1.0f, 0.0f);

        const std::array<float, 2> old = {0.4f, 0.4f};
        const std::array<float, 2> solo = {0.3f, 0.3f};
        const std::array<float, 2> fresh = {0.2f, 0.2f};
        std::array<float, 2> out{};

        // Slice 2 gets one block ahead. Draining the synchronized first
        // pair leaves one old 0.4 block queued only on slice 2.
        mix.accumulate(2, old.data(), 1);
        mix.accumulate(2, old.data(), 1);
        mix.accumulate(1, old.data(), 1);
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QCOMPARE(out[0], 0.8f);

        // Withdrawal removes slice 2 from the barrier and invalidates its
        // queued generation. Slice 1 must drain alone: the stale 0.4 must
        // not turn this into 0.7.
        mix.setSliceStreaming(2, false);
        mix.accumulate(1, solo.data(), 1);
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QCOMPARE(out[0], 0.3f);
        QCOMPARE(out[1], 0.3f);

        // Re-admission does not make the old generation valid again. Slice
        // 2 contributes only after its fresh 0.2 block arrives.
        mix.setSliceStreaming(2, true);
        mix.accumulate(2, fresh.data(), 1);
        mix.accumulate(1, fresh.data(), 1);
        QCOMPARE(mix.tryDrain(out.data(), 1), 1);
        QCOMPARE(out[0], 0.4f);
        QCOMPARE(out[1], 0.4f);
    }

    void withdrawalDuringAdmittedDrainInvalidatesTheResult() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSlewUpFrames(0);
        mix.setSliceGain(1, 1.0f, 0.0f);
        const std::array<float, 2> old = {0.7f, 0.7f};
        std::array<float, 2> out{};
        mix.accumulate(1, old.data(), 1);

        QSemaphore admitted;
        QSemaphore resume;
        mix.setDrainAdmissionHookForTest([&] {
            admitted.release();
            resume.acquire();
        });

        std::atomic<int> drained{-1};
        std::thread audio([&] {
            drained.store(mix.tryDrain(out.data(), 1),
                          std::memory_order_release);
        });

        const bool admissionObserved = admitted.tryAcquire(1, 1000);
        if (admissionObserved) {
            mix.setSliceStreaming(1, false);
        }
        resume.release();
        audio.join();

        QVERIFY2(admissionObserved, "drain never reached the admission seam");
        QCOMPARE(drained.load(std::memory_order_acquire), 0);
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

    // ── Master slew across a membership change ────────────────────────
    //
    // Re-admitting a slice must fade the MIXED output in, not resume it at
    // full amplitude in one sample. That abrupt resume is the "kerplunk at
    // the end of the unkey" reported on the 2026-07-27 G2E bench: MOX
    // withdraws the TX slice, and on unkey its audio came back instantly.
    //
    // Thetis fades. create_aaslew builds a raised-cosine window
    //   cup[i] = 0.5 * (1.0 - cos (theta)),  theta over [0, PI]
    // (aamix.c:86-92 [v2.10.3.15]), and open_mixer sets the upslew flag and
    // blocks until it completes (aamix.c:494-505) on every membership
    // change made through SetAAudioMixState (aamix.c:522).
    //
    // The RX mixer asks for 10 ms of it: create_aamix is called with
    // tdelayup 0.000 / tslewup 0.010 / tdelaydown 0.000 / tslewdown 0.010
    // (cmaster.c:297-313 [v2.10.3.15]). Note the VAC mixer passes 0.0 for
    // all four (ivac.c:106), so only the RX path slews.
    //
    // Ramp collapsed to one frame so this isolates the master slew from the
    // per-slice anti-click ramp.
    void readmittingASliceFadesTheMixIn() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSliceGain(1, 1.0f, 0.0f);

        std::array<float, 8> in = {1.0f, 1.0f, 1.0f, 1.0f,
                                   1.0f, 1.0f, 1.0f, 1.0f};  // 4 frames
        std::array<float, 8> out{};

        // Withdraw and re-admit, exactly as a MOX cycle does.
        mix.setSliceStreaming(1, false);
        mix.setSliceStreaming(1, true);

        mix.accumulate(1, in.data(), 4);
        QCOMPARE(mix.tryDrain(out.data(), 4), 4);

        // First frame must be far below unity, and the output rising.
        QVERIFY2(out[0] < 0.05f,
                 qPrintable(QString("first frame %1 should be near silence")
                                .arg(static_cast<double>(out[0]))));
        QVERIFY(out[2] > out[0]);
        QVERIFY(out[4] > out[2]);
        QVERIFY(out[6] > out[4]);
    }

    // Once the slew has run its course the mix sits at unity, so a steady
    // stream is not permanently attenuated.
    void theMixReachesUnityAfterTheSlew() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSliceGain(1, 1.0f, 0.0f);
        mix.setSliceStreaming(1, true);

        // 10 ms at 48 kHz is 480 frames; run well past it.
        std::vector<float> in(2048 * 2, 1.0f);
        std::vector<float> out(2048 * 2, 0.0f);
        for (int rep = 0; rep < 4; ++rep) {
            mix.accumulate(1, in.data(), 512);
            mix.tryDrain(out.data(), 512);
        }
        // Tail of the last block is past the slew and must be unity.
        QVERIFY(std::abs(out[1022] - 1.0f) < 0.0001f);
    }

    // The anti-VOX mixer must NOT slew. Thetis creates the RX mixer with
    // tslewup 0.010 but the anti-VOX mixer with 0.000 on all four slew
    // parameters (cmaster.c:297-313 vs cmaster.c:159-175 [v2.10.3.15]).
    // The DEXP reference has to be amplitude-faithful from the first sample
    // after a transition, so the slew length is per instance.
    void slewCanBeDisabledPerInstance() {
        MasterMixer mix;
        mix.setRampFrames(1);
        mix.setSlewUpFrames(0);
        mix.setSliceGain(1, 1.0f, 0.0f);

        std::array<float, 8> in = {1.0f, 1.0f, 1.0f, 1.0f,
                                   1.0f, 1.0f, 1.0f, 1.0f};
        std::array<float, 8> out{};

        // Arming would normally fade the mix in over kSlewUpFrames.
        mix.setSliceStreaming(1, false);
        mix.setSliceStreaming(1, true);

        mix.accumulate(1, in.data(), 4);
        QCOMPARE(mix.tryDrain(out.data(), 4), 4);

        // With slew disabled the first frame is already at full amplitude.
        QVERIFY(std::abs(out[0] - 1.0f) < 0.0001f);
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
