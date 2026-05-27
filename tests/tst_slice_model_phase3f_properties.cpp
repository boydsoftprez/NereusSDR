// =================================================================
// tests/tst_slice_model_phase3f_properties.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// Phase 3F Sub-Epic A Tasks 4-11: verify SliceModel gains 7 new
// Q_PROPERTYs per
// docs/architecture/2026-05-26-phase3f-multi-pan-multi-slice-design.md §3.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceModelPhase3FProperties : public QObject {
    Q_OBJECT

private slots:
    // ── Task 4: sliceLetter ──────────────────────────────────────────────
    void slice_letter_default_is_A()
    {
        SliceModel slice;
        QCOMPARE(slice.sliceLetter(), QChar('A'));
    }

    void slice_letter_setter_round_trips()
    {
        SliceModel slice;
        slice.setSliceLetter(QChar('C'));
        QCOMPARE(slice.sliceLetter(), QChar('C'));
    }

    void slice_letter_setter_emits_signal()
    {
        SliceModel slice;
        QSignalSpy spy(&slice, &SliceModel::sliceLetterChanged);
        slice.setSliceLetter(QChar('B'));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toChar(), QChar('B'));
    }

    void slice_letter_setter_idempotent()
    {
        SliceModel slice;
        slice.setSliceLetter(QChar('B'));
        QSignalSpy spy(&slice, &SliceModel::sliceLetterChanged);
        slice.setSliceLetter(QChar('B'));  // same value
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestSliceModelPhase3FProperties)
#include "tst_slice_model_phase3f_properties.moc"
