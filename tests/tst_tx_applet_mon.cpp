// no-port-check: test-only — exercises NereusSDR-native TxApplet MON toggle +
// monitor volume slider + mic-source badge wiring (Phase 3M-1b J.3).
//
// TxApplet inserts:
//   - A MON toggle button below VOX. Bidirectional with TransmitModel::monEnabled
//     (default false; does NOT persist across restarts — safety: MON always loads
//     OFF, plan §0 row 9).
//   - A monitor volume slider (0..100) bidirectional with TransmitModel::monitorVolume
//     (default 0.5f → slider 50). Mapping: float = int / 100.0f.
//   - A mic-source badge (read-only label) above the gauges. Text "PC mic" when
//     micSource == Pc, "Radio mic" when Radio. Updates on micSourceChanged.
//
// Test cases:
// MON toggle:
//  1. Default state  — monEnabled() default is false (plan §0 row 9 safety rule).
//  2. UI → Model ON  — setMonEnabled(true) → getter returns true.
//  3. UI → Model OFF — setMonEnabled(false) → getter returns false.
//  4. Model → UI ON  — monEnabledChanged(true) fires with correct value.
//  5. No feedback loop / idempotency — setMonEnabled(false) twice → no extra emit.
//
// Volume slider:
//  6. Default volume — monitorVolume() default is 0.5f.
//  7. UI → Model — setMonitorVolume(0.75f) → getter returns 0.75f.
//  8. Model → UI — monitorVolumeChanged(0.25f) fires with correct float.
//  9. Boundary — setMonitorVolume(0.0f) → 0.0f; setMonitorVolume(1.0f) → 1.0f.
// 10. Integer↔float mapping — slider 75 → 0.75f; float 0.25f → slider 25.
//
// Mic-source badge:
// 11. Default state — micSource() default is MicSource::Pc.
// 12. Set Radio → micSourceChanged(Radio) fires; badge text should be "Radio mic".
// 13. Set back to Pc → micSourceChanged(Pc) fires; badge text should be "PC mic".
//
// These tests exercise the model layer contracts that TxApplet::wireControls()
// depends on. TxApplet itself requires a display server; widget construction is
// not performed here (same headless pattern as the other tst_tx_applet_*.cpp tests).

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include "models/TransmitModel.h"
#include "core/audio/CompositeTxMicRouter.h"

using namespace NereusSDR;

class TestTxAppletMon : public QObject
{
    Q_OBJECT

private slots:
    // MON toggle
    void monEnabled_defaultIsFalse();
    void setMonEnabled_true_roundtrip();
    void setMonEnabled_false_roundtrip();
    void monEnabledChanged_firesTrue();
    void noFeedbackLoop_idempotentFalse();

    // Volume slider
    void monitorVolume_defaultIsHalf();
    void setMonitorVolume_roundtrip();
    void monitorVolumeChanged_fires();
    void monitorVolume_boundaries();
    void monitorVolume_intFloatMapping();

    // Mic-source badge
    void micSource_defaultIsPc();
    void setMicSource_radio_fires();
    void setMicSource_backToPc_fires();
};

// ── MON toggle ──────────────────────────────────────────────────────────────

// 1. Default state — MON always loads OFF (plan §0 row 9 safety rule).
void TestTxAppletMon::monEnabled_defaultIsFalse()
{
    TransmitModel tx;
    QVERIFY(!tx.monEnabled());
}

// 2. UI → Model ON — setMonEnabled(true) → getter returns true.
void TestTxAppletMon::setMonEnabled_true_roundtrip()
{
    TransmitModel tx;
    tx.setMonEnabled(true);
    QVERIFY(tx.monEnabled());
}

// 3. UI → Model OFF — setMonEnabled(true) then setMonEnabled(false).
void TestTxAppletMon::setMonEnabled_false_roundtrip()
{
    TransmitModel tx;
    tx.setMonEnabled(true);
    tx.setMonEnabled(false);
    QVERIFY(!tx.monEnabled());
}

// 4. Model → UI — monEnabledChanged(true) fires with the correct value.
void TestTxAppletMon::monEnabledChanged_firesTrue()
{
    TransmitModel tx;
    QSignalSpy spy(&tx, &TransmitModel::monEnabledChanged);

    tx.setMonEnabled(true);

    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.at(0).at(0).toBool());
}

// 5. No feedback loop / idempotency — calling setMonEnabled with the same value
//    must not emit an additional signal.
void TestTxAppletMon::noFeedbackLoop_idempotentFalse()
{
    TransmitModel tx;
    // tx starts at false (default).

    QSignalSpy spy(&tx, &TransmitModel::monEnabledChanged);

    // Calling setMonEnabled(false) on a model already at false → no-op.
    tx.setMonEnabled(false);
    QCOMPARE(spy.count(), 0);

    // Calling setMonEnabled(true) → exactly one emit.
    tx.setMonEnabled(true);
    QCOMPARE(spy.count(), 1);

    // Calling setMonEnabled(true) again → no-op (idempotent guard).
    tx.setMonEnabled(true);
    QCOMPARE(spy.count(), 1);
}

// ── Volume slider ───────────────────────────────────────────────────────────

// 6. Default volume — model default is 0.5f (Thetis audio.cs:417 coefficient).
//    Maps to slider position 50 in TxApplet (value / 100.0f round-trip).
void TestTxAppletMon::monitorVolume_defaultIsHalf()
{
    TransmitModel tx;
    QCOMPARE(tx.monitorVolume(), 0.5f);
}

// 7. UI → Model — setMonitorVolume(0.75f) → getter returns 0.75f.
void TestTxAppletMon::setMonitorVolume_roundtrip()
{
    TransmitModel tx;
    tx.setMonitorVolume(0.75f);
    QCOMPARE(tx.monitorVolume(), 0.75f);
}

// 8. Model → UI — monitorVolumeChanged fires with correct float value.
void TestTxAppletMon::monitorVolumeChanged_fires()
{
    TransmitModel tx;
    QSignalSpy spy(&tx, &TransmitModel::monitorVolumeChanged);

    tx.setMonitorVolume(0.25f);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).toFloat(), 0.25f);
}

// 9. Boundary — 0.0f and 1.0f are the clamped extremes.
void TestTxAppletMon::monitorVolume_boundaries()
{
    TransmitModel tx;

    tx.setMonitorVolume(0.0f);
    QCOMPARE(tx.monitorVolume(), 0.0f);

    tx.setMonitorVolume(1.0f);
    QCOMPARE(tx.monitorVolume(), 1.0f);
}

// 10. Integer ↔ float mapping — the TxApplet slider maps int [0..100] to
//     float [0.0..1.0] as float = int / 100.0f and int = qRound(float * 100.0f).
//     We verify these contracts at the model level.
void TestTxAppletMon::monitorVolume_intFloatMapping()
{
    TransmitModel tx;

    // Slider 75 → model 0.75f
    const float fromSlider75 = 75 / 100.0f;
    tx.setMonitorVolume(fromSlider75);
    QCOMPARE(tx.monitorVolume(), 0.75f);

    // Model 0.25f → slider 25
    tx.setMonitorVolume(0.25f);
    const int toSlider = qRound(tx.monitorVolume() * 100.0f);
    QCOMPARE(toSlider, 25);

    // Slider 0 → 0.0f
    tx.setMonitorVolume(0 / 100.0f);
    QCOMPARE(tx.monitorVolume(), 0.0f);

    // Slider 100 → 1.0f
    tx.setMonitorVolume(100 / 100.0f);
    QCOMPARE(tx.monitorVolume(), 1.0f);
}

// ── Mic-source badge ─────────────────────────────────────────────────────────

// 11. Default micSource is Pc (PC microphone is always available and safe).
void TestTxAppletMon::micSource_defaultIsPc()
{
    TransmitModel tx;
    QCOMPARE(tx.micSource(), MicSource::Pc);
}

// 12. Set Radio → micSourceChanged(Radio) fires; badge text contract: "Radio mic".
void TestTxAppletMon::setMicSource_radio_fires()
{
    TransmitModel tx;
    QSignalSpy spy(&tx, &TransmitModel::micSourceChanged);

    tx.setMicSource(MicSource::Radio);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<MicSource>(), MicSource::Radio);
    QCOMPARE(tx.micSource(), MicSource::Radio);
}

// 13. Set back to Pc → micSourceChanged(Pc) fires; badge text contract: "PC mic".
void TestTxAppletMon::setMicSource_backToPc_fires()
{
    TransmitModel tx;
    tx.setMicSource(MicSource::Radio);  // prime to Radio first

    QSignalSpy spy(&tx, &TransmitModel::micSourceChanged);
    tx.setMicSource(MicSource::Pc);

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<MicSource>(), MicSource::Pc);
    QCOMPARE(tx.micSource(), MicSource::Pc);
}

QTEST_MAIN(TestTxAppletMon)
#include "tst_tx_applet_mon.moc"
