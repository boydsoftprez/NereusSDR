// tst_slice_agc_advanced.cpp
//
// Verifies that SliceModel AGC advanced setters store + emit signals correctly.
// SliceModel is the single source of truth for VFO/DSP state. Setters must:
//   1. Guard against no-op updates (unchanged value → no signal)
//   2. Store the new value
//   3. Emit the corresponding changed signal
//
// Source citations:
//   From Thetis Project Files/Source/Console/radio.cs:1037-1124 — AGC advanced
//   From Thetis Project Files/Source/Console/console.cs:45977  — AGCThresh

#include <QtTest/QtTest>
#include "models/SliceModel.h"

using namespace NereusSDR;

class TestSliceAgcAdvanced : public QObject {
    Q_OBJECT

private slots:
    // ── agcThreshold ─────────────────────────────────────────────────────────

    void agcThresholdDefaultIsMinusTwenty() {
        SliceModel s;
        QCOMPARE(s.agcThreshold(), -20);
    }

    void setAgcThresholdStoresValue() {
        SliceModel s;
        s.setAgcThreshold(-80);
        QCOMPARE(s.agcThreshold(), -80);
    }

    void setAgcThresholdEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::agcThresholdChanged);
        s.setAgcThreshold(-100);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), -100);
    }

    void setAgcThresholdNoSignalOnSameValue() {
        SliceModel s;
        s.setAgcThreshold(-50);
        QSignalSpy spy(&s, &SliceModel::agcThresholdChanged);
        s.setAgcThreshold(-50);  // same value — no signal
        QCOMPARE(spy.count(), 0);
    }

    // ── agcHang ──────────────────────────────────────────────────────────────

    void agcHangDefaultIs250() {
        // From Thetis radio.cs:1056-1057 — rx_agc_hang = 250 ms
        SliceModel s;
        QCOMPARE(s.agcHang(), 250);
    }

    void setAgcHangStoresValue() {
        SliceModel s;
        s.setAgcHang(500);
        QCOMPARE(s.agcHang(), 500);
    }

    void setAgcHangEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::agcHangChanged);
        s.setAgcHang(750);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 750);
    }

    void setAgcHangNoSignalOnSameValue() {
        SliceModel s;
        s.setAgcHang(100);
        QSignalSpy spy(&s, &SliceModel::agcHangChanged);
        s.setAgcHang(100);
        QCOMPARE(spy.count(), 0);
    }

    // ── agcSlope ─────────────────────────────────────────────────────────────

    void agcSlopeDefaultIsZero() {
        // From Thetis radio.cs:1107-1108 — rx_agc_slope = 0
        SliceModel s;
        QCOMPARE(s.agcSlope(), 0);
    }

    void setAgcSlopeStoresValue() {
        SliceModel s;
        s.setAgcSlope(5);
        QCOMPARE(s.agcSlope(), 5);
    }

    void setAgcSlopeEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::agcSlopeChanged);
        s.setAgcSlope(10);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 10);
    }

    void setAgcSlopeNoSignalOnSameValue() {
        SliceModel s;
        s.setAgcSlope(3);
        QSignalSpy spy(&s, &SliceModel::agcSlopeChanged);
        s.setAgcSlope(3);
        QCOMPARE(spy.count(), 0);
    }

    // ── agcAttack ────────────────────────────────────────────────────────────

    void agcAttackDefaultIsTwo() {
        // From WDSP wcpAGC.c create_wcpagc — tau_attack default 2 ms
        SliceModel s;
        QCOMPARE(s.agcAttack(), 2);
    }

    void setAgcAttackStoresValue() {
        SliceModel s;
        s.setAgcAttack(8);
        QCOMPARE(s.agcAttack(), 8);
    }

    void setAgcAttackEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::agcAttackChanged);
        s.setAgcAttack(5);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 5);
    }

    void setAgcAttackNoSignalOnSameValue() {
        SliceModel s;
        s.setAgcAttack(2);
        QSignalSpy spy(&s, &SliceModel::agcAttackChanged);
        s.setAgcAttack(2);
        QCOMPARE(spy.count(), 0);
    }

    // ── agcDecay ─────────────────────────────────────────────────────────────

    void agcDecayDefaultIs250() {
        // From Thetis radio.cs:1037-1038 — rx_agc_decay = 250 ms
        SliceModel s;
        QCOMPARE(s.agcDecay(), 250);
    }

    void setAgcDecayStoresValue() {
        SliceModel s;
        s.setAgcDecay(1000);
        QCOMPARE(s.agcDecay(), 1000);
    }

    void setAgcDecayEmitsSignal() {
        SliceModel s;
        QSignalSpy spy(&s, &SliceModel::agcDecayChanged);
        s.setAgcDecay(300);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 300);
    }

    void setAgcDecayNoSignalOnSameValue() {
        SliceModel s;
        s.setAgcDecay(250);
        QSignalSpy spy(&s, &SliceModel::agcDecayChanged);
        s.setAgcDecay(250);
        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestSliceAgcAdvanced)
#include "tst_slice_agc_advanced.moc"
