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
