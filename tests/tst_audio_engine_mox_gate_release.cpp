// =================================================================
// tests/tst_audio_engine_mox_gate_release.cpp  (NereusSDR)
// =================================================================
//
// Drives the real MoxController -> AudioEngine::setMoxState wire that
// RadioModel's constructor makes, across the sequences an operator can
// produce with MOX and TUNE, and asserts on BOTH halves of the MOX audio
// gate: that m_moxActive returns to false, and that the slice the gate
// withdrew gets back into the mixers' readiness barriers.
//
// The barriers have no timeout by design (MasterMixer.h, divergence 3), so
// membership bookkeeping that is merely nearly right does not self-heal --
// it persists for the life of the process. Two defects found this way:
//
//   twoTrueEdgesWithNoFalseBetween / reKeyStrandsThePreviouslyWithdrawnSlice
//     moxStateChanged does not alternate. MoxController emits it at the END
//     of a timer walk and stopAllTimers() cancels a pending emit, so
//     re-keying inside the 30 ms release window delivers true, true. The
//     `active` branch of setMoxState cleared m_moxWithdrawnSlice before
//     re-reading activeSlice(), stranding the earlier slice out of both
//     barriers permanently.
//
//   midOverActiveSliceMoveKeepsTheMixAlive
//     The gate reads isActiveSlice() per block; the withdrawal sampled
//     activeSlice() once at the transition. Handing TX to another slice
//     mid-over left the newly gated slice enrolled with nothing to deliver,
//     and the drain waits on every member: the mix went from one push per
//     period to none for the rest of the over.
//
// =================================================================

#include <QtTest/QtTest>

#include "core/AudioEngine.h"
#include "core/IAudioBus.h"
#include "core/MoxController.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include "fakes/FakeAudioBus.h"

#include <array>
#include <memory>

using namespace NereusSDR;

namespace {

constexpr int kTestFrames = 64;
constexpr int kTestStereoFloats = kTestFrames * 2;

std::array<float, kTestStereoFloats> makeBlock()
{
    std::array<float, kTestStereoFloats> b{};
    for (int i = 0; i < kTestStereoFloats; ++i) {
        b[static_cast<size_t>(i)] = 0.25f;
    }
    return b;
}

const std::array<float, kTestStereoFloats> kBlock = makeBlock();

} // namespace

class TstAudioEngineMoxGateRelease : public QObject {
    Q_OBJECT

private:
    struct Harness {
        std::unique_ptr<RadioModel> radio;
        AudioEngine*   engine   = nullptr;
        FakeAudioBus*  speakers = nullptr;
        MoxController* mox      = nullptr;

        int addSlice() { return radio->addSlice(); }

        // One DSP period: hand every live slice a block, exactly as
        // RxDspWorker does.
        void feedPeriod()
        {
            const QList<SliceModel*> live = radio->slices();
            for (SliceModel* s : live) {
                engine->rxBlockReady(s->sliceIndex(), kBlock.data(), kTestFrames);
            }
        }

        // Run the MoxController timer walk to completion. The TX->RX walk
        // chains two single-shot timers, so one pass is not enough.
        void settle()
        {
            for (int i = 0; i < 8; ++i) {
                QCoreApplication::processEvents();
            }
        }

        // True when a period of feeding puts something on the speakers.
        bool audioFlows()
        {
            const int before = speakers->pushCount();
            // Several periods: the mixer's readiness barrier can legitimately
            // hold the first one while a re-admitted slice re-enrols.
            for (int i = 0; i < 4; ++i) {
                feedPeriod();
            }
            return speakers->pushCount() > before;
        }
    };

    Harness makeHarness(int sliceCount = 1)
    {
        Harness h;
        h.radio  = std::make_unique<RadioModel>();
        h.engine = h.radio->audioEngine();
        h.mox    = h.radio->moxController();

        auto speakers = std::make_unique<FakeAudioBus>(
            QStringLiteral("FakeSpeakers"));
        AudioFormat fmt;
        fmt.sampleRate = 48000;
        fmt.channels   = 2;
        fmt.sample     = AudioFormat::Sample::Float32;
        speakers->open(fmt);
        h.speakers = speakers.get();
        h.engine->setSpeakersBusForTest(std::move(speakers));

        h.radio->configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                     /*defaultRateHz=*/192000);

        for (int i = 0; i < sliceCount; ++i) {
            h.addSlice();
        }

        // Zero-length timer walk so processEvents() drives it.
        h.mox->setTimerIntervals(0, 0, 0, 0, 0, 0);
        return h;
    }

private slots:

    // ── Sanity: the wire exists at all ────────────────────────────────────
    void wire_moxStateChanged_reaches_audioEngine()
    {
        Harness h = makeHarness();
        QCOMPARE(h.engine->moxState(), false);

        h.mox->setMox(true);
        h.settle();
        QCOMPARE(h.engine->moxState(), true);

        h.mox->setMox(false);
        h.settle();
        QCOMPARE(h.engine->moxState(), false);
    }

    // ── A. Plain key-down / key-up ────────────────────────────────────────
    void cycle_mox_plain()
    {
        Harness h = makeHarness();
        QVERIFY(h.audioFlows());

        h.mox->setMox(true);
        h.settle();

        h.mox->setMox(false);
        h.settle();

        QCOMPARE(h.mox->isMox(), false);
        QCOMPARE(h.engine->moxState(), false);
        QVERIFY2(h.audioFlows(), "audio dead after a plain MOX cycle");
    }

    // ── B. TUNE ───────────────────────────────────────────────────────────
    void cycle_tune_plain()
    {
        Harness h = makeHarness();
        QVERIFY(h.audioFlows());

        h.mox->setTune(true);
        h.settle();

        h.mox->setTune(false);
        h.settle();

        QCOMPARE(h.mox->isMox(), false);
        QCOMPARE(h.engine->moxState(), false);
        QVERIFY2(h.audioFlows(), "audio dead after a plain TUNE cycle");
    }

    // ── C. Released before the RX->TX walk finishes ───────────────────────
    void cycle_mox_released_mid_rx_to_tx_walk()
    {
        Harness h = makeHarness();
        QVERIFY(h.audioFlows());

        h.mox->setMox(true);   // rfDelay walk armed, NOT settled
        h.mox->setMox(false);
        h.settle();

        QCOMPARE(h.mox->isMox(), false);
        QCOMPARE(h.engine->moxState(), false);
        QVERIFY2(h.audioFlows(), "audio dead after a tapped MOX");
    }

    // ── D. Re-keyed before the TX->RX walk finishes ───────────────────────
    void cycle_mox_rekeyed_mid_tx_to_rx_walk()
    {
        Harness h = makeHarness();
        QVERIFY(h.audioFlows());

        h.mox->setMox(true);
        h.settle();
        h.mox->setMox(false);  // keyUp walk armed, NOT settled
        h.mox->setMox(true);
        h.settle();
        h.mox->setMox(false);
        h.settle();

        QCOMPARE(h.mox->isMox(), false);
        QCOMPARE(h.engine->moxState(), false);
        QVERIFY2(h.audioFlows(), "audio dead after a re-keyed MOX");
    }

    // ── E. TUNE engaged on top of MOX ─────────────────────────────────────
    void cycle_tune_on_top_of_mox()
    {
        Harness h = makeHarness();
        QVERIFY(h.audioFlows());

        h.mox->setMox(true);
        h.settle();
        h.mox->setTune(true);   // setMox(true) again -> idempotent guard
        h.settle();
        h.mox->setTune(false);
        h.settle();

        QCOMPARE(h.mox->isMox(), false);
        QCOMPARE(h.engine->moxState(), false);
        QVERIFY2(h.audioFlows(), "audio dead after TUNE on top of MOX");
    }

    // ── F. MOX released while TUNE is still latched ───────────────────────
    void cycle_mox_released_under_tune()
    {
        Harness h = makeHarness();
        QVERIFY(h.audioFlows());

        h.mox->setTune(true);
        h.settle();
        h.mox->setMox(false);   // e.g. TxSliceArbiter / TwoTone force-unkey
        h.settle();
        h.mox->setTune(false);  // setMox(false) -> idempotent guard
        h.settle();

        QCOMPARE(h.mox->isMox(), false);
        QCOMPARE(h.engine->moxState(), false);
        QVERIFY2(h.audioFlows(), "audio dead after a force-unkey under TUNE");
    }

    // ── G. Many cycles: does anything drift? ──────────────────────────────
    void cycle_mox_repeated()
    {
        Harness h = makeHarness();
        QVERIFY(h.audioFlows());

        for (int i = 0; i < 25; ++i) {
            h.mox->setMox(true);
            h.settle();
            h.feedPeriod();
            h.mox->setMox(false);
            h.settle();
            h.feedPeriod();
        }

        QCOMPARE(h.engine->moxState(), false);
        QVERIFY2(h.audioFlows(), "audio dead after 25 MOX cycles");
    }

    // ── H. Two slices, active slice changes DURING the transmission ───────
    //
    // The gate is keyed on SliceModel::isActiveSlice(); the barrier
    // withdrawal is keyed on the id captured at key-down. Moving the
    // active slice mid-over makes the two name different slices.
    void activeSliceMovesDuringTx()
    {
        Harness h = makeHarness(/*sliceCount=*/2);
        QVERIFY(h.audioFlows());

        h.mox->setMox(true);
        h.settle();

        h.radio->setActiveSlice(1);   // operator clicks pan B mid-over
        h.feedPeriod();

        h.mox->setMox(false);
        h.settle();

        QCOMPARE(h.engine->moxState(), false);
        QVERIFY2(h.audioFlows(),
                 "audio dead after the active slice moved during TX");
    }

    // ── I. Two slices, active slice changes BETWEEN two key-downs ─────────
    void activeSliceMovesBetweenTransmissions()
    {
        Harness h = makeHarness(/*sliceCount=*/2);
        QVERIFY(h.audioFlows());

        h.mox->setMox(true);
        h.settle();
        h.radio->setActiveSlice(1);
        h.mox->setMox(true);          // idempotent, no second walk
        h.settle();
        h.mox->setMox(false);
        h.settle();

        QCOMPARE(h.engine->moxState(), false);
        QVERIFY2(h.audioFlows(),
                 "audio dead after the active slice moved between overs");
    }

    // ── J. TX monitor feeding across a transmission ───────────────────────
    //
    // The monitor is an opportunistic slot, so it must never enrol as a
    // barrier member. If it ever did, it would stop delivering on unkey and
    // hold the drain forever: permanent silence, restart-only.
    void txMonitorNeverEnrols()
    {
        Harness h = makeHarness();
        QVERIFY(h.audioFlows());
        const int before = h.engine->masterMixForTest().producingSliceCount();

        h.engine->setTxMonitorEnabled(true);
        h.mox->setMox(true);
        h.settle();

        for (int i = 0; i < 4; ++i) {
            h.feedPeriod();
            h.engine->txMonitorBlockReady(kBlock.data(), kTestFrames);
        }

        h.mox->setMox(false);
        h.settle();
        h.engine->setTxMonitorEnabled(false);

        QVERIFY2(h.audioFlows(), "audio dead after the TX monitor fed");
        QCOMPARE(h.engine->masterMixForTest().producingSliceCount(), before);
    }

    // ── K. A slice removed while it is the gated TX slice ─────────────────
    void sliceRemovedDuringTx()
    {
        Harness h = makeHarness(/*sliceCount=*/2);
        QVERIFY(h.audioFlows());

        h.mox->setMox(true);
        h.settle();
        h.radio->removeSlice(0);      // close pan A mid-over
        h.feedPeriod();
        h.mox->setMox(false);
        h.settle();

        QCOMPARE(h.engine->moxState(), false);
        QVERIFY2(h.audioFlows(),
                 "audio dead after a slice was removed during TX");
    }

    // ── L. A slice added while the gate is on ─────────────────────────────
    void sliceAddedDuringTx()
    {
        Harness h = makeHarness();
        QVERIFY(h.audioFlows());

        h.mox->setMox(true);
        h.settle();
        h.addSlice();                 // +RX mid-over
        h.feedPeriod();
        h.mox->setMox(false);
        h.settle();

        QCOMPARE(h.engine->moxState(), false);
        QVERIFY2(h.audioFlows(),
                 "audio dead after a slice was added during TX");
    }

    // ── N. Two `true` edges with no `false` between them ──────────────────
    //
    // moxStateChanged(false) is emitted at the END of the TX->RX walk, 30 ms
    // after the release. Re-keying inside that window calls stopAllTimers(),
    // which cancels the pending emit, and the next walk ends in `true`. So
    // AudioEngine can see true, true with no false in between -- and the
    // `active` branch of setMoxState clears m_moxWithdrawnSlice before
    // re-reading activeSlice().
    void twoTrueEdgesWithNoFalseBetween()
    {
        Harness h = makeHarness(/*sliceCount=*/2);
        QSignalSpy spy(h.mox, &MoxController::moxStateChanged);

        h.mox->setMox(true);
        h.settle();                    // true #1

        h.mox->setMox(false);          // release walk armed, NOT settled
        h.mox->setMox(true);           // re-key inside the window
        h.settle();                    // true #2

        int trues = 0;
        int falses = 0;
        for (const QList<QVariant>& args : spy) {
            args.at(0).toBool() ? ++trues : ++falses;
        }
        QCOMPARE(trues, 2);
        QCOMPARE(falses, 0);
    }

    // ── O. …and the active slice moves between those two `true` edges ─────
    void reKeyStrandsThePreviouslyWithdrawnSlice()
    {
        Harness h = makeHarness(/*sliceCount=*/2);
        QVERIFY(h.audioFlows());

        h.mox->setMox(true);
        h.settle();                    // true #1 -> withdraws slice 0

        h.mox->setMox(false);          // release walk armed, NOT settled
        h.radio->setActiveSlice(1);    // TX hands off to slice B
        h.mox->setMox(true);           // re-key cancels the pending false
        h.settle();                    // true #2 -> withdraws slice 1,
                                       //            forgetting slice 0

        h.mox->setMox(false);
        h.settle();                    // re-admits slice 1 only

        QCOMPARE(h.engine->moxState(), false);
        QVERIFY(h.audioFlows());

        // Both slices are feeding and MOX is off, so both must be back in
        // both readiness barriers. A slice left out is not waited on: the
        // mix stops being paced by it, which is the clumped-delivery
        // artefact the barrier exists to prevent.
        QCOMPARE(h.engine->masterMixForTest().producingSliceCount(), 2);
        QCOMPARE(h.engine->antiVoxMixForTest().producingSliceCount(), 2);
    }


    // ── P. The mix survives an active-slice move mid-transmission ─────────
    //
    // The gate silences whichever slice is active per block; the withdrawal
    // samples activeSlice() once at the transition. A mid-over move separates
    // them: the newly gated slice stops feeding while still enrolled, and the
    // barrier waits on every member with no timeout.
    void midOverActiveSliceMoveKeepsTheMixAlive()
    {
        Harness h = makeHarness(/*sliceCount=*/2);
        QVERIFY(h.audioFlows());

        h.mox->setMox(true);
        h.settle();

        // Slice B is un-gated, so it carries the mix for the whole over.
        int before = h.speakers->pushCount();
        for (int i = 0; i < 4; ++i) { h.feedPeriod(); }
        const int beforeMove = h.speakers->pushCount() - before;
        QVERIFY2(beforeMove > 0, "no audio during TX even before the move");

        h.radio->setActiveSlice(1);   // TX handed to slice B mid-over
        h.settle();

        before = h.speakers->pushCount();
        for (int i = 0; i < 4; ++i) { h.feedPeriod(); }
        const int afterMove = h.speakers->pushCount() - before;

        QVERIFY2(afterMove >= beforeMove,
                 qPrintable(QStringLiteral(
                     "mix collapsed mid-over: %1 pushes before the move, "
                     "%2 after").arg(beforeMove).arg(afterMove)));

        h.mox->setMox(false);
        h.settle();
        QVERIFY(h.audioFlows());
        QCOMPARE(h.engine->masterMixForTest().producingSliceCount(), 2);
    }

    // ── M. Long mixed session: everything an operator can do, in sequence ──
    void longMixedSession()
    {
        Harness h = makeHarness(/*sliceCount=*/2);
        QVERIFY(h.audioFlows());
        h.engine->setTxMonitorEnabled(true);

        for (int cycle = 0; cycle < 12; ++cycle) {
            // TUNE, then MOX, then a tapped MOX.
            h.mox->setTune(true);
            h.settle();
            h.feedPeriod();
            h.engine->txMonitorBlockReady(kBlock.data(), kTestFrames);
            h.mox->setTune(false);
            h.settle();

            QVERIFY2(h.audioFlows(),
                     qPrintable(QStringLiteral("audio dead after TUNE, cycle %1")
                                    .arg(cycle)));

            h.radio->setActiveSlice(cycle % 2);

            h.mox->setMox(true);
            h.settle();
            h.feedPeriod();
            h.radio->setActiveSlice((cycle + 1) % 2);   // handoff mid-over
            h.feedPeriod();
            h.mox->setMox(false);
            h.settle();

            QVERIFY2(h.audioFlows(),
                     qPrintable(QStringLiteral("audio dead after MOX, cycle %1")
                                    .arg(cycle)));

            h.mox->setMox(true);
            h.mox->setMox(false);     // tapped: walk interrupted
            h.settle();

            QVERIFY2(h.audioFlows(),
                     qPrintable(QStringLiteral("audio dead after tap, cycle %1")
                                    .arg(cycle)));
        }

        QCOMPARE(h.engine->moxState(), false);
    }
};

QTEST_MAIN(TstAudioEngineMoxGateRelease)
#include "tst_audio_engine_mox_gate_release.moc"
