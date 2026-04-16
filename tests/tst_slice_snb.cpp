// tst_slice_snb.cpp
//
// Verifies that SliceModel SNB setter stores + emits signal correctly.
// SliceModel is the single source of truth for VFO/DSP state. Setters must:
//   1. Guard against no-op updates (unchanged value → no signal)
//   2. Store the new value
//   3. Emit the corresponding changed signal
//
// Source citations:
//   From Thetis Project Files/Source/Console/console.cs:36347 — SNB run flag
//   From Thetis Project Files/Source/Console/dsp.cs:692-693 — P/Invoke decl
//   WDSP: third_party/wdsp/src/snb.c:579

#include <QtTest/QtTest>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceSnb : public QObject {
    Q_OBJECT

private slots:
    // ── snbEnabled ───────────────────────────────────────────────────────────

    void snbEnabledDefaultIsFalse() {
        // SNB off at startup — Thetis chkDSPNB2.Checked default = false
        SliceModel s;
        QCOMPARE(s.snbEnabled(), false);
    }

    void setSnbEnabledStoresValue() {
        SliceModel s;
        s.setSnbEnabled(true);
        QCOMPARE(s.snbEnabled(), true);
    }

    void setSnbEnabledEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::snbEnabledChanged);
        s.setSnbEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setSnbEnabledNoSignalOnSameValue() {
        SliceModel s;
        s.setSnbEnabled(true);
        QSignalSpy spy(&s, &SliceModel::snbEnabledChanged);
        s.setSnbEnabled(true);  // same value — no signal
        QCOMPARE(spy.count(), 0);
    }

    void setSnbEnabledToggleRoundTrip() {
        SliceModel s;
        s.setSnbEnabled(true);
        s.setSnbEnabled(false);
        QCOMPARE(s.snbEnabled(), false);
    }
};

QTEST_MAIN(TestSliceSnb)
#include "tst_slice_snb.moc"
