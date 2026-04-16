// tst_slice_rit_xit.cpp
//
// Verifies SliceModel RIT/XIT client-offset behavior (S2.8).
//
// RIT (Receive Incremental Tuning): client-side demodulation offset.
// XIT (Transmit Incremental Tuning): stored for 3M-1 (TX phase), no RX effect.
//
// effectiveRxFrequency() = frequency + (ritEnabled ? ritHz : 0)
// This offset feeds the existing RxChannel::setShiftFrequency path via RadioModel.
// It does NOT retune the hardware VFO.
//
// Source: Thetis console.cs — RIT shifts receive demodulation without
// retuning the hardware DDC center frequency.

#include <QtTest/QtTest>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceRitXit : public QObject {
    Q_OBJECT

private slots:
    // ── effectiveRxFrequency ─────────────────────────────────────────────────

    void effectiveRxFrequencyNoRitEqualsFrequency() {
        SliceModel s;
        s.setFrequency(14225000.0);
        s.setRitEnabled(false);
        s.setRitHz(500);
        // RIT disabled → effective = base frequency
        QCOMPARE(s.effectiveRxFrequency(), 14225000.0);
    }

    void effectiveRxFrequencyWithRitAddsOffset() {
        SliceModel s;
        s.setFrequency(14225000.0);
        s.setRitHz(300);
        s.setRitEnabled(true);
        // RIT enabled → effective = base + offset
        QCOMPARE(s.effectiveRxFrequency(), 14225300.0);
    }

    void effectiveRxFrequencyNegativeRitOffset() {
        SliceModel s;
        s.setFrequency(7200000.0);
        s.setRitHz(-150);
        s.setRitEnabled(true);
        QCOMPARE(s.effectiveRxFrequency(), 7199850.0);
    }

    void effectiveRxFrequencyZeroRitOffset() {
        SliceModel s;
        s.setFrequency(3700000.0);
        s.setRitHz(0);
        s.setRitEnabled(true);
        // Zero offset — enabled but no shift
        QCOMPARE(s.effectiveRxFrequency(), 3700000.0);
    }

    // ── setRitHz while disabled doesn't change effective frequency ────────────

    void setRitHzWhileDisabledDoesNotAffectEffective() {
        SliceModel s;
        s.setFrequency(14000000.0);
        s.setRitEnabled(false);
        s.setRitHz(1000);
        QCOMPARE(s.effectiveRxFrequency(), 14000000.0);
        // Changing Hz while disabled still no effect
        s.setRitHz(-500);
        QCOMPARE(s.effectiveRxFrequency(), 14000000.0);
    }

    // ── ritEnabled signal guard ───────────────────────────────────────────────

    void setRitEnabledEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::ritEnabledChanged);
        s.setRitEnabled(true);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
    }

    void setRitEnabledNoSignalOnSameValue() {
        SliceModel s;
        s.setRitEnabled(true);
        QSignalSpy spy(&s, &SliceModel::ritEnabledChanged);
        s.setRitEnabled(true);
        QCOMPARE(spy.count(), 0);
    }

    // ── setRitHz signal guard ─────────────────────────────────────────────────

    void setRitHzEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::ritHzChanged);
        s.setRitHz(200);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 200);
    }

    void setRitHzNoSignalOnSameValue() {
        SliceModel s;
        s.setRitHz(100);
        QSignalSpy spy(&s, &SliceModel::ritHzChanged);
        s.setRitHz(100);
        QCOMPARE(spy.count(), 0);
    }

    // ── XIT stores value, no RX effect ────────────────────────────────────────

    void setXitHzStoresValue() {
        SliceModel s;
        s.setXitHz(400);
        QCOMPARE(s.xitHz(), 400);
    }

    void setXitEnabledStoresValue() {
        SliceModel s;
        s.setXitEnabled(true);
        QVERIFY(s.xitEnabled());
    }

    void xitHzDoesNotAffectEffectiveRxFrequency() {
        // XIT is TX-side only — no RX effect in 3G-10.
        SliceModel s;
        s.setFrequency(14100000.0);
        s.setXitHz(500);
        s.setXitEnabled(true);
        // Effective RX frequency is unchanged (RIT disabled)
        QCOMPARE(s.effectiveRxFrequency(), 14100000.0);
    }

    void setXitHzEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::xitHzChanged);
        s.setXitHz(350);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 350);
    }

    void setXitHzNoSignalOnSameValue() {
        SliceModel s;
        s.setXitHz(200);
        QSignalSpy spy(&s, &SliceModel::xitHzChanged);
        s.setXitHz(200);
        QCOMPARE(spy.count(), 0);
    }

    // ── Default values ────────────────────────────────────────────────────────

    void defaultRitEnabledIsFalse() {
        SliceModel s;
        QVERIFY(!s.ritEnabled());
    }

    void defaultRitHzIsZero() {
        SliceModel s;
        QCOMPARE(s.ritHz(), 0);
    }

    void defaultXitEnabledIsFalse() {
        SliceModel s;
        QVERIFY(!s.xitEnabled());
    }

    void defaultXitHzIsZero() {
        SliceModel s;
        QCOMPARE(s.xitHz(), 0);
    }
};

QTEST_MAIN(TestSliceRitXit)
#include "tst_slice_rit_xit.moc"
