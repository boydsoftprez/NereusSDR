// no-port-check: test-only — exercises NereusSDR-native VOX toggle wiring.
// Phase 3M-1b J.2; updated 3M-3a-iii Task 16 (commit dropping the duplicate
// TxApplet VOX surface + VoxSettingsPopup widget).
//
// The wired VOX surface now lives entirely on PhoneCwApplet (Phase tab,
// Controls #10/#11) — see tst_phone_cw_applet_vox_dexp.cpp for the
// applet-level wiring tests.  This test file retains the model-layer
// contracts that BOTH the old TxApplet VOX button AND the current
// PhoneCwApplet VOX button depend on:
//
//   default voxEnabled=false (plan §0 row 8 safety — VOX always loads OFF),
//   UI→Model toggle ON/OFF (setVoxEnabled / voxEnabled getter),
//   Model→UI: voxEnabledChanged fires with correct bool,
//   visual state on toggle (signal fires on change),
//   no feedback loop / idempotency (same-value set → no extra emit).
//
// Test cases:
//  1. Default state  — voxEnabled() default is false (plan §0 row 8 safety rule).
//  2. UI → Model toggle ON  — setVoxEnabled(true) → getter returns true.
//  3. UI → Model toggle OFF — setVoxEnabled(false) → getter returns false.
//  4. Model → UI  — voxEnabledChanged(true) fires with correct value.
//  5. Model → UI  — voxEnabledChanged(false) fires with correct value.
//  6. Visual state on toggle — voxEnabledChanged fires (implying visual update);
//     checked state contract: button should reflect model state via signal.
//  7. No feedback loop — setVoxEnabled(false) twice → only one signal emitted
//     after initial set (idempotent guard).

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestTxAppletVoxToggle : public QObject
{
    Q_OBJECT

private slots:
    void voxEnabled_defaultIsFalse();
    void setVoxEnabled_true_roundtrip();
    void setVoxEnabled_false_roundtrip();
    void voxEnabledChanged_firesTrue();
    void voxEnabledChanged_firesFalse();
    void voxEnabled_visualChangeOnToggle();
    void noFeedbackLoop_idempotentFalse();
};

// ── 1. Default state ─────────────────────────────────────────────────────────
// VOX always loads OFF — plan §0 row 8 safety rule.
// From Thetis audio.cs:167 [v2.10.3.13]: private static bool vox_enabled = false;
void TestTxAppletVoxToggle::voxEnabled_defaultIsFalse()
{
    TransmitModel tx;
    QVERIFY(!tx.voxEnabled());
}

// ── 2. UI → Model toggle ON ──────────────────────────────────────────────────
void TestTxAppletVoxToggle::setVoxEnabled_true_roundtrip()
{
    TransmitModel tx;
    tx.setVoxEnabled(true);
    QVERIFY(tx.voxEnabled());
}

// ── 3. UI → Model toggle OFF ─────────────────────────────────────────────────
void TestTxAppletVoxToggle::setVoxEnabled_false_roundtrip()
{
    TransmitModel tx;
    tx.setVoxEnabled(true);   // first ON
    tx.setVoxEnabled(false);  // back OFF
    QVERIFY(!tx.voxEnabled());
}

// ── 4. Model → UI: voxEnabledChanged(true) fires ────────────────────────────
void TestTxAppletVoxToggle::voxEnabledChanged_firesTrue()
{
    TransmitModel tx;
    QSignalSpy spy(&tx, &TransmitModel::voxEnabledChanged);

    tx.setVoxEnabled(true);

    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.at(0).at(0).toBool());
}

// ── 5. Model → UI: voxEnabledChanged(false) fires ───────────────────────────
void TestTxAppletVoxToggle::voxEnabledChanged_firesFalse()
{
    TransmitModel tx;
    tx.setVoxEnabled(true);   // prime state to ON first

    QSignalSpy spy(&tx, &TransmitModel::voxEnabledChanged);
    tx.setVoxEnabled(false);

    QCOMPARE(spy.count(), 1);
    QVERIFY(!spy.at(0).at(0).toBool());
}

// ── 6. Visual state on toggle ────────────────────────────────────────────────
// The PhoneCwApplet VOX button's visual state (checked/unchecked) is driven by
// voxEnabledChanged. Here we verify the signal fires when toggling, which is
// the contract that causes the UI to update. We also confirm the boolean
// sequence matches (false → true → false).
void TestTxAppletVoxToggle::voxEnabled_visualChangeOnToggle()
{
    TransmitModel tx;
    QSignalSpy spy(&tx, &TransmitModel::voxEnabledChanged);

    // Toggle ON → button becomes checked (green border via greenCheckedStyle).
    tx.setVoxEnabled(true);
    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.at(0).at(0).toBool());

    // Toggle OFF → button unchecked (green border removed).
    tx.setVoxEnabled(false);
    QCOMPARE(spy.count(), 2);
    QVERIFY(!spy.at(1).at(0).toBool());
}

// ── 7. No feedback loop / idempotency ────────────────────────────────────────
// Setting the same value twice must not emit a second signal.
// This verifies the m_updatingFromModel guard (model side: idempotent check).
void TestTxAppletVoxToggle::noFeedbackLoop_idempotentFalse()
{
    TransmitModel tx;
    // tx starts at false (default).

    QSignalSpy spy(&tx, &TransmitModel::voxEnabledChanged);

    // Calling setVoxEnabled(false) on a model that is already false → no-op.
    tx.setVoxEnabled(false);
    QCOMPARE(spy.count(), 0);

    // Calling setVoxEnabled(true) → exactly one emit.
    tx.setVoxEnabled(true);
    QCOMPARE(spy.count(), 1);

    // Calling setVoxEnabled(true) again → no-op (idempotent guard).
    tx.setVoxEnabled(true);
    QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(TestTxAppletVoxToggle)
#include "tst_tx_applet_vox_toggle.moc"
