// =================================================================
// tests/tst_new_pan_is_its_own_receiver.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Multi-pan slice-to-DDC placement is
// a Phase 3F concept; Thetis has a fixed RX1/RX2 pair and no allocator to
// port from.
//
// Bench report 2026-07-30 (JJ, KG4VCF): "creating a second pan, then
// retuning the first or second pan causes both to tune... dragging and
// panning up and down the band from the waterfall controls both, they
// should be independent."
//
// Not a wiring fault. Every spectrum handler already resolves its own
// pan's slice through sliceForPan(panId), and that was verified. The
// coupling was one layer down: RadioModel::addSlice seeds a new slice on
// the ACTIVE slice's frequency so the pan opens on the band being worked,
// which put it inside the active stream's window, so the allocator shared
// the DDC. A DDC has one centre and feeds one FFT stream, so the two pans
// were two views of a single receiver. They moved together because they
// were one thing.
//
// Operator decision, 2026-07-30: a new pan is a new receiver, matching
// AetherSDR and Thetis RX1/RX2. A slice added to a pan that already has
// slices still shares that pan's receiver, which is what makes co-hosted
// slices on one panadapter free.
//
// These cases pin the placement, which is where the coupling lived. The
// operator-visible half (drag pan A, pan B stays put) is bench row 26.
// =================================================================

#include <QtTest/QtTest>

#include "core/SliceStreamAllocator.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestNewPanIsItsOwnReceiver : public QObject
{
    Q_OBJECT

private:
    static void seed(RadioModel& model, int ddcs = 5)
    {
        model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
        model.configureStreamPool(/*userDdcCount*/ ddcs, /*maxSlices*/ 5, 192000);
    }

private slots:

    // ── 1. The reported case ─────────────────────────────────────────────
    //
    // Two pans, same band, because the new pan is seeded on the band being
    // worked. They must still be two receivers.
    void a_second_pan_on_the_same_band_gets_its_own_ddc()
    {
        RadioModel model;
        seed(model);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        SliceModel* sliceA = model.sliceById(a);
        QVERIFY(sliceA);
        sliceA->setFrequency(14200000.0);

        const int b = model.addSlice(QStringLiteral("pan-1"));
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceB);

        // Seeded onto A's band, which is the behaviour that caused the
        // sharing and which is deliberately kept.
        QCOMPARE(sliceB->frequency(), sliceA->frequency());

        QVERIFY2(sliceA->streamIndex() >= 0 && sliceB->streamIndex() >= 0,
                 "both slices must be bound");
        QVERIFY2(sliceA->streamIndex() != sliceB->streamIndex(),
            "a new pan is a new receiver: sharing the DDC is what made "
            "panning either pan move both");
    }

    // ── 2. +RX on an existing pan still shares ───────────────────────────
    //
    // The other half of the rule. If this also claimed a DDC, co-hosted
    // slices would stop being free and a 5-DDC radio would run out after
    // five slices however they were arranged.
    void a_second_slice_on_the_same_pan_shares_its_receiver()
    {
        RadioModel model;
        seed(model);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        SliceModel* sliceA = model.sliceById(a);
        QVERIFY(sliceA);
        sliceA->setFrequency(14200000.0);

        const int b = model.addSlice(QStringLiteral("pan-0"));
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceB);

        QCOMPARE(sliceB->streamIndex(), sliceA->streamIndex());
    }

    // ── 3. Four pans, four receivers ─────────────────────────────────────
    //
    // The 2x2 layout, which is where an operator would most expect four
    // independent views.
    void four_pans_claim_four_distinct_ddcs()
    {
        RadioModel model;
        seed(model);

        QSet<int> streams;
        for (int i = 0; i < 4; ++i) {
            const int id = model.addSlice(QStringLiteral("pan-%1").arg(i));
            SliceModel* s = model.sliceById(id);
            QVERIFY(s);
            QVERIFY(s->streamIndex() >= 0);
            streams.insert(s->streamIndex());
        }

        QCOMPARE(streams.size(), 4);
    }

    // ── 4. A full DDC pool shares rather than refusing ───────────────────
    //
    // Dedicated is a preference, not a demand. With every DDC spoken for,
    // a coupled pan beats no pan and a toast: the operator can still see
    // and hear, and the coupling is visible rather than silent.
    void a_new_pan_falls_back_to_sharing_when_no_ddc_is_free()
    {
        RadioModel model;
        seed(model, /*ddcs*/ 2);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        SliceModel* sliceA = model.sliceById(a);
        QVERIFY(sliceA);
        sliceA->setFrequency(14200000.0);

        const int b = model.addSlice(QStringLiteral("pan-1"));
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceB);
        QVERIFY(sliceA->streamIndex() != sliceB->streamIndex());

        // Third pan, no DDC left. It must still open.
        const int c = model.addSlice(QStringLiteral("pan-2"));
        SliceModel* sliceC = model.sliceById(c);
        QVERIFY2(sliceC != nullptr, "the pan must still get a slice");
        QVERIFY2(sliceC->streamIndex() >= 0,
            "and that slice must be bound to something: refusing to bind "
            "would leave a pan rendering nothing");
    }

    // ── 5. The allocator honours the preference directly ─────────────────
    //
    // Below RadioModel, so a future caller reading the flag gets the same
    // answer without standing up a model.
    void the_allocator_prefers_a_free_ddc_when_asked()
    {
        SliceStreamAllocator alloc;
        alloc.configure(/*userDdcCount*/ 4, /*maxSlices*/ 4);
        alloc.setDefaultSampleRateHz(192000);
        alloc.activateStream(0, 14200000.0, 192000);

        using Outcome = SliceStreamAllocator::Outcome;

        // Same frequency, inside stream 0's window.
        const auto shared = alloc.placeSlice(14200000.0, /*preferDedicated*/ false);
        QCOMPARE(shared.outcome, Outcome::JoinedExisting);
        QCOMPARE(shared.streamIndex, 0);

        const auto dedicated = alloc.placeSlice(14200000.0, /*preferDedicated*/ true);
        QCOMPARE(dedicated.outcome, Outcome::NewStream);
        QVERIFY2(dedicated.streamIndex != 0,
                 "a dedicated request must not land on the occupied stream");
        QCOMPARE(dedicated.newStreamCentreHz, 14200000.0);
        QCOMPARE(dedicated.shiftOffsetHz, 0.0);
    }

    // ── 6. The preference does not change a retune ───────────────────────
    //
    // Only the first bind may claim. A retune has co-hosts that depend on
    // its window, and pulling a slice onto a fresh DDC there would strand
    // them or leave a hole in the enable mask.
    void retuning_inside_a_shared_window_still_shares()
    {
        RadioModel model;
        seed(model);

        const int a = model.addSlice(QStringLiteral("pan-0"));
        SliceModel* sliceA = model.sliceById(a);
        QVERIFY(sliceA);
        sliceA->setFrequency(14200000.0);

        const int b = model.addSlice(QStringLiteral("pan-0"));   // co-host
        SliceModel* sliceB = model.sliceById(b);
        QVERIFY(sliceB);
        const int shared = sliceA->streamIndex();
        QCOMPARE(sliceB->streamIndex(), shared);

        // Nudge B a little, still inside the 192 kHz window.
        sliceB->setFrequency(14210000.0);

        QCOMPARE(sliceB->streamIndex(), shared);
    }
};

QTEST_MAIN(TestNewPanIsItsOwnReceiver)
#include "tst_new_pan_is_its_own_receiver.moc"
