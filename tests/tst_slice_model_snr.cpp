// SPDX-License-Identifier: GPL-2.0-or-later
//
// NereusSDR - tst_slice_model_snr: SliceModel snrDb Q_PROPERTY contract.
//
// NEW NereusSDR-native extension to SliceModel. No upstream equivalent.
// RadeChannel will populate snrDb when slice mode is RADE; future
// digital modes wire the same setSnrDb slot. VfoWidget (Phase L1)
// binds snrDbChanged to paint the SNR row in the flag.

#include <QtTest>
#include <QSignalSpy>

#include <cmath>
#include <limits>

#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceModelSnr : public QObject {
    Q_OBJECT
private slots:
    void initialSnrIsNaN();
    void setSnrDbEmitsOnChange();
    void setSnrDbNoEmitOnSameValue();
    void setSnrDbNoEmitOnNaNToNaN();
    void setSnrDbEmitOnNaNToValue();
    void setSnrDbEmitOnValueToNaN();
};

void TestSliceModelSnr::initialSnrIsNaN() {
    SliceModel s;
    QVERIFY(qIsNaN(s.snrDb()));
}

void TestSliceModelSnr::setSnrDbEmitsOnChange() {
    SliceModel s;
    QSignalSpy spy(&s, &SliceModel::snrDbChanged);
    s.setSnrDb(5.5);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(s.snrDb(), 5.5);
}

void TestSliceModelSnr::setSnrDbNoEmitOnSameValue() {
    SliceModel s;
    QSignalSpy spy(&s, &SliceModel::snrDbChanged);
    s.setSnrDb(7.0);
    s.setSnrDb(7.0);  // same value, should not re-emit
    QCOMPARE(spy.count(), 1);
}

void TestSliceModelSnr::setSnrDbNoEmitOnNaNToNaN() {
    SliceModel s;
    QSignalSpy spy(&s, &SliceModel::snrDbChanged);
    // Initial value is NaN. Setting NaN again must not emit.
    s.setSnrDb(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(spy.count(), 0);
}

void TestSliceModelSnr::setSnrDbEmitOnNaNToValue() {
    SliceModel s;
    QSignalSpy spy(&s, &SliceModel::snrDbChanged);
    // NaN -> 5.0 should emit.
    s.setSnrDb(5.0);
    QCOMPARE(spy.count(), 1);
    QCOMPARE(s.snrDb(), 5.0);
}

void TestSliceModelSnr::setSnrDbEmitOnValueToNaN() {
    SliceModel s;
    s.setSnrDb(5.0);  // seed with numeric value
    QSignalSpy spy(&s, &SliceModel::snrDbChanged);
    // 5.0 -> NaN should emit (signal-loss event).
    s.setSnrDb(std::numeric_limits<double>::quiet_NaN());
    QCOMPARE(spy.count(), 1);
    QVERIFY(qIsNaN(s.snrDb()));
}

QTEST_GUILESS_MAIN(TestSliceModelSnr)
#include "tst_slice_model_snr.moc"
