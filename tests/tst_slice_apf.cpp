// tst_slice_apf.cpp
//
// Verifies that SliceModel APF (Audio Peak Filter) setters store + emit
// signals correctly. SliceModel is the single source of truth for VFO/DSP
// state. Setters must:
//   1. Guard against no-op updates (unchanged value → no signal)
//   2. Store the new value
//   3. Emit the corresponding changed signal
//
// Source citations:
//   From Thetis Project Files/Source/Console/radio.cs:1910-2008 — APF properties
//   WDSP: third_party/wdsp/src/apfshadow.c:45,93,117,141,165

#include <QtTest/QtTest>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceApf : public QObject {
    Q_OBJECT

private slots:
    // ── apfEnabled ───────────────────────────────────────────────────────────

    void apfEnabledDefaultIsFalse() {
        // From Thetis radio.cs:1910 — rx_apf_run default = false
        SliceModel s;
        QCOMPARE(s.apfEnabled(), false);
    }

    void setApfEnabledStoresValue() {
        SliceModel s;
        s.setApfEnabled(true);
        QCOMPARE(s.apfEnabled(), true);
    }

    void setApfEnabledEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::apfEnabledChanged);
        s.setApfEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setApfEnabledNoSignalOnSameValue() {
        SliceModel s;
        s.setApfEnabled(true);
        QSignalSpy spy(&s, &SliceModel::apfEnabledChanged);
        s.setApfEnabled(true);  // same value — no signal
        QCOMPARE(spy.count(), 0);
    }

    void setApfEnabledToggleRoundTrip() {
        SliceModel s;
        s.setApfEnabled(true);
        s.setApfEnabled(false);
        QCOMPARE(s.apfEnabled(), false);
    }

    // ── apfTuneHz ────────────────────────────────────────────────────────────

    void apfTuneHzDefaultIsZero() {
        // Neutral default — zero tune offset (no CW pitch shift)
        SliceModel s;
        QCOMPARE(s.apfTuneHz(), 0);
    }

    void setApfTuneHzStoresValue() {
        SliceModel s;
        s.setApfTuneHz(100);
        QCOMPARE(s.apfTuneHz(), 100);
    }

    void setApfTuneHzEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::apfTuneHzChanged);
        s.setApfTuneHz(50);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 50);
    }

    void setApfTuneHzNoSignalOnSameValue() {
        SliceModel s;
        s.setApfTuneHz(100);
        QSignalSpy spy(&s, &SliceModel::apfTuneHzChanged);
        s.setApfTuneHz(100);  // same value — no signal
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestSliceApf)
#include "tst_slice_apf.moc"
