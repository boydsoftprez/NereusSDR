// tst_slice_emnr.cpp
//
// Verifies that SliceModel EMNR (NR2) setter stores + emits signal correctly.
// SliceModel is the single source of truth for VFO/DSP state. Setters must:
//   1. Guard against no-op updates (unchanged value → no signal)
//   2. Store the new value
//   3. Emit the corresponding changed signal
//
// Source citations:
//   From Thetis Project Files/Source/Console/radio.cs:2216-2232 — EMNR run flag
//   WDSP: third_party/wdsp/src/emnr.c:1283

#include <QtTest/QtTest>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceEmnr : public QObject {
    Q_OBJECT

private slots:
    // ── emnrEnabled ──────────────────────────────────────────────────────────

    void emnrEnabledDefaultIsFalse() {
        // From Thetis radio.cs:2216 — rx_nr2_run default = 0
        SliceModel s;
        QCOMPARE(s.emnrEnabled(), false);
    }

    void setEmnrEnabledStoresValue() {
        SliceModel s;
        s.setEmnrEnabled(true);
        QCOMPARE(s.emnrEnabled(), true);
    }

    void setEmnrEnabledEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::emnrEnabledChanged);
        s.setEmnrEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setEmnrEnabledNoSignalOnSameValue() {
        SliceModel s;
        s.setEmnrEnabled(true);
        QSignalSpy spy(&s, &SliceModel::emnrEnabledChanged);
        s.setEmnrEnabled(true);  // same value — no signal
        QCOMPARE(spy.count(), 0);
    }

    void setEmnrEnabledToggleRoundTrip() {
        SliceModel s;
        s.setEmnrEnabled(true);
        s.setEmnrEnabled(false);
        QCOMPARE(s.emnrEnabled(), false);
    }
};

QTEST_MAIN(TestSliceEmnr)
#include "tst_slice_emnr.moc"
