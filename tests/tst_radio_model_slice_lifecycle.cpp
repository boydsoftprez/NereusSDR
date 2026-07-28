// =================================================================
// tests/tst_radio_model_slice_lifecycle.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic C Task 7: RadioModel slice lifecycle.
// See docs/architecture/2026-05-26-phase3f-sub-epic-c-tx-arbiter-lifecycle-plan.md
// Task 7.
// =================================================================
#include <QtTest/QtTest>
#include <QSet>
#include <QSignalSpy>
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestRadioModelSliceLifecycle : public QObject {
    Q_OBJECT
private slots:
    // Bench report, ANAN-G2E 2026-07-28: a second receiver added on the same
    // pan is "stuck to the exact dial freq as the first", and the B flag
    // "just follows the A flag" instead of tuning independently.
    //
    // Opening on A's frequency is intentional (addSlice seeds a new slice
    // from the active slice so it lands on the band being worked). What must
    // then work is moving it: two slices sharing one DDC window are supposed
    // to differ only by their shift oscillators, which is
    // SliceStreamAllocator::retuneSlice returning JoinedExisting with
    // shiftOffsetHz = frequencyHz - centreHz.
    //
    // This test is deliberately at the model layer, to establish whether the
    // defect is here or above it in the flag wiring.
    void secondSliceOnAPanTunesIndependently()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);

        // Order matters, and it mirrors what the operator does: tune the
        // first receiver, THEN add a second one on the same pan. Seeding
        // reads the active slice at add time, so adding both before tuning
        // would seed B from the 14.225 MHz ctor default instead.
        const int a = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sa = radio.sliceById(a);
        QVERIFY(sa != nullptr);
        sa->setFrequency(7'240'000.0);

        const int b = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sb = radio.sliceById(b);
        QVERIFY(sb != nullptr);

        // B is seeded from A, so it starts co-channel. That part is by design.
        QCOMPARE(sb->frequency(), sa->frequency());

        // Move B 5 kHz up, comfortably inside a 192 kHz window.
        sb->setFrequency(7'245'000.0);

        // B must hold its new frequency, not snap back onto A.
        QCOMPARE(sb->frequency(), 7'245'000.0);
        QCOMPARE(sa->frequency(), 7'240'000.0);

        // And they must differ by their shift oscillators, which is what
        // actually makes them demodulate different signals.
        QVERIFY2(!qFuzzyCompare(sa->shiftOffsetHz(), sb->shiftOffsetHz()),
                 "both slices share a shift offset, so they demodulate the "
                 "same frequency however the flags read");
    }

    void addSliceOnPan_exists_and_is_callable()
    {
        // Disconnected RadioModel: maxSlices() == 1 (safe default) and
        // m_slices starts empty.  First addSliceOnPan should SUCCEED
        // (creates slice 0, total = 1 = cap).  A second call should be
        // REJECTED (cap exceeded).  The test exists primarily to verify
        // the API compiles and the cap-enforcement contract holds.
        RadioModel radio;
        QSignalSpy added(&radio, &RadioModel::sliceAdded);
        QSignalSpy rejected(&radio, &RadioModel::sliceAddRejected);

        radio.addSliceOnPan(QStringLiteral("pan-0"));
        QCOMPARE(added.count(), 1);
        QCOMPARE(rejected.count(), 0);

        // Cap is 1 when disconnected; a second add must be rejected.
        radio.addSliceOnPan(QStringLiteral("pan-1"));
        QCOMPARE(added.count(), 1);
        QCOMPARE(rejected.count(), 1);
    }

    void removeSlice_refuses_to_remove_last_slice()
    {
        RadioModel radio;
        QSignalSpy removed(&radio, &RadioModel::sliceRemoved);

        // Create one slice (succeeds, cap = 1 when disconnected).
        radio.addSliceOnPan(QStringLiteral("pan-0"));

        // Attempting to remove the only remaining slice must be a silent
        // no-op (never remove the last slice).  No sliceRemoved signal.
        radio.removeSlice(0);
        QCOMPARE(removed.count(), 0);
    }

    // ── Phase 3F Sub-Epic I closeout, defect C3 ───────────────────────────
    //
    // addSlice stamped the id from m_slices.size() and removeSlice never
    // renumbered survivors, so ids and list positions diverged after any
    // mid-list removal and ids could be handed out twice.
    //
    // addSlice now takes the lowest free id and sliceById resolves by id
    // rather than by position. addSliceOnPan is capped at maxSlices() (1
    // while disconnected), so these use addSlice directly.

    void survivorsKeepTheirIdsAcrossAMiddleRemoval()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);

        const int a = radio.addSlice();
        const int b = radio.addSlice();
        const int c = radio.addSlice();
        QCOMPARE(a, 0);
        QCOMPARE(b, 1);
        QCOMPARE(c, 2);

        SliceModel* sliceA = radio.sliceById(a);
        SliceModel* sliceC = radio.sliceById(c);
        QVERIFY(sliceA);
        QVERIFY(sliceC);

        QSignalSpy removed(&radio, &RadioModel::sliceRemoved);
        radio.removeSlice(b);

        // sliceRemoved carries the id, which is what the MainWindow
        // handler keys its per-slice widget map by.
        QCOMPARE(removed.count(), 1);
        QCOMPARE(removed.first().at(0).toInt(), b);

        QCOMPARE(radio.slices().size(), 2);

        // C is now at list POSITION 1 but keeps id 2, and resolves to
        // itself. Positionally it would have been out of range (silent
        // slice); removing A instead would have handed B's audio block to
        // C's SliceModel.
        QCOMPARE(sliceC->sliceIndex(), c);
        QCOMPARE(radio.sliceById(c), sliceC);
        QCOMPARE(radio.sliceById(a), sliceA);
        QCOMPARE(radio.slices().at(1), sliceC);

        // The removed id resolves to nothing rather than to a survivor.
        QCOMPARE(radio.sliceById(b), nullptr);
    }

    void reAddedSliceDoesNotCollideWithASurvivor()
    {
        RadioModel radio;
        radio.configureStreamPool(5, 5, 192000);

        const int a = radio.addSlice();
        const int b = radio.addSlice();
        QCOMPARE(a, 0);
        QCOMPARE(b, 1);

        SliceModel* sliceB = radio.sliceById(b);
        QVERIFY(sliceB);

        radio.removeSlice(a);
        QCOMPARE(radio.slices().size(), 1);

        // Old behaviour: index = m_slices.size() = 1, colliding with the
        // surviving B. Two SliceModels then shared WDSP channel 1 —
        // slicesOnStream returned {1, 1}, so the fan-out ran that channel
        // twice per chunk and both slices wrote shift offsets into it.
        const int reAdded = radio.addSlice();
        QCOMPARE(reAdded, a);              // the freed id, not B's
        QVERIFY(reAdded != b);
        QCOMPARE(sliceB->sliceIndex(), b); // B untouched

        // Every live slice carries a distinct id.
        QSet<int> ids;
        for (SliceModel* s : radio.slices()) {
            QVERIFY(s);
            QVERIFY(!ids.contains(s->sliceIndex()));
            ids.insert(s->sliceIndex());
        }
        QCOMPARE(ids.size(), radio.slices().size());
    }

    // The design doc's slice-letter contract (A..E in creation order, letter
    // freed on destroy, re-creation takes the lowest available) rides on the
    // id: every display site derives the letter as QChar('A' + sliceIndex()).
    void freedSliceLetterIsReused()
    {
        RadioModel radio;
        radio.configureStreamPool(5, 5, 192000);

        radio.addSlice();                       // A
        const int b = radio.addSlice();         // B
        radio.addSlice();                       // C

        radio.removeSlice(b);                   // B's letter is freed
        const int reAdded = radio.addSlice();

        QCOMPARE(QChar('A' + reAdded), QChar('B'));
    }

    // A shared DDC window has ONE centre. When CTUN moves it, every slice on
    // that stream needs its shift recomputed as (frequency - newCentre), not
    // just the slice that dragged. Otherwise the co-host keeps a stale shift
    // and demodulates the wrong signal, which is the same hazard family as
    // the 2026-07-27 CTUN stranding fix.
    void movingTheDdcCentreReshiftsEveryCoHostedSlice()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);

        const int a = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sa = radio.sliceById(a);
        QVERIFY(sa != nullptr);
        sa->setFrequency(7'240'000.0);

        const int b = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sb = radio.sliceById(b);
        QVERIFY(sb != nullptr);
        sb->setFrequency(7'245'000.0);

        // Both must be on the same stream for this test to mean anything.
        QCOMPARE(sa->streamIndex(), sb->streamIndex());
        const int stream = sa->streamIndex();

        // A third slice, far enough out to have claimed its own DDC. Its shift
        // is the control: reshifting one stream must not reach across into a
        // window it does not own.
        const int c = radio.addSlice(QStringLiteral("pan-1"));
        SliceModel* sc = radio.sliceById(c);
        QVERIFY(sc != nullptr);
        sc->setFrequency(14'200'000.0);
        QVERIFY(sc->streamIndex() != stream);
        QCOMPARE(sc->shiftOffsetHz(), 0.0);

        const double newCentre = 7'250'000.0;
        radio.reshiftSlicesOnStream(stream, newCentre);

        QVERIFY(qFuzzyCompare(sa->shiftOffsetHz(), 7'240'000.0 - newCentre));
        QVERIFY(qFuzzyCompare(sb->shiftOffsetHz(), 7'245'000.0 - newCentre));
        QCOMPARE(sc->shiftOffsetHz(), 0.0);
    }

    // The noise blanker belongs to the DDC, not the slice: ANB panb / NOB
    // pnob live in struct _rcvr beside double* audio[cmMAXSubRcvr]
    // (Thetis cmaster.h:74-82 [v2.10.3.15]). Sub-Epic I Task 4b's rule is
    // that the first slice to reach processIq blanks the chunk WITH ITS OWN
    // SETTINGS and the co-hosts are bypassed, so linking only the buttons
    // would leave the blanker reading whichever slice happened to own the
    // pass. The state itself has to be mirrored.
    void coHostedSlicesShareNbState()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);

        const int a = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sa = radio.sliceById(a);
        sa->setFrequency(7'240'000.0);
        const int b = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sb = radio.sliceById(b);
        sb->setFrequency(7'245'000.0);
        QCOMPARE(sa->streamIndex(), sb->streamIndex());

        sa->setNbMode(NereusSDR::NbMode::NB);
        QCOMPARE(sb->nbMode(), NereusSDR::NbMode::NB);

        // And the other direction.
        sb->setNbMode(NereusSDR::NbMode::Off);
        QCOMPARE(sa->nbMode(), NereusSDR::NbMode::Off);
    }

    // A slice joining an occupied stream adopts that stream's NB state; a
    // slice claiming an empty one keeps its own.
    void aSliceJoiningAStreamAdoptsItsNbState()
    {
        RadioModel radio;
        radio.configureStreamPool(/*userDdcCount*/ 5, /*maxSlices*/ 5, 192000);

        const int a = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sa = radio.sliceById(a);
        sa->setFrequency(7'240'000.0);
        sa->setNbMode(NereusSDR::NbMode::NB2);

        const int b = radio.addSlice(QStringLiteral("pan-0"));
        SliceModel* sb = radio.sliceById(b);
        QCOMPARE(sa->streamIndex(), sb->streamIndex());
        QCOMPARE(sb->nbMode(), NereusSDR::NbMode::NB2);
    }

    // Single-slice operation is untouched: slice A is still id 0 and still
    // resolves.
    void sliceAIsStillIdZero()
    {
        RadioModel radio;
        const int a = radio.addSlice();
        QCOMPARE(a, 0);
        QVERIFY(radio.sliceById(0) != nullptr);
        QCOMPARE(radio.sliceById(0), radio.activeSlice());
    }
};

QTEST_MAIN(TestRadioModelSliceLifecycle)
#include "tst_radio_model_slice_lifecycle.moc"
