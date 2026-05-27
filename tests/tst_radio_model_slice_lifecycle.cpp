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
};

QTEST_MAIN(TestRadioModelSliceLifecycle)
#include "tst_radio_model_slice_lifecycle.moc"
