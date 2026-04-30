// no-port-check: NereusSDR-original test file.  All Thetis source cites
// for the underlying TransmitModel properties live in TransmitModel.h.
// =================================================================
// tests/tst_tx_applet_lev_eq_proc.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-3a-i Batch 2 (Task F) — TxApplet LEV / EQ / PROC quick toggles.
//
// TxApplet inserts a row of three checkable buttons between the MON volume
// slider and the TUNE/MOX row:
//   LEV  — bidirectional ↔ TransmitModel::txLevelerOn (green-checked style).
//   EQ   — bidirectional ↔ TransmitModel::txEqEnabled (green-checked style).
//          Right-click reserved for TxEqDialog launch (3M-3a-i Batch 3).
//   PROC — disabled placeholder per master design §7.1 (3M-3a-ii).
//
// Tests:
//   1. All three buttons exist (located by objectName).
//   2. PROC is disabled.
//   3. LEV.click() flips TransmitModel::txLevelerOn.
//   4. EQ.click() flips TransmitModel::txEqEnabled.
//   5. Model→UI: setTxLevelerOn(false) un-checks LEV button.
//   6. Model→UI: setTxEqEnabled(true) checks EQ button.
//   7. PROC has the documented "coming in 3M-3a-ii" tooltip.
//
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QPushButton>

#include "core/AppSettings.h"
#include "gui/applets/TxApplet.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestTxAppletLevEqProc : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        if (!qApp) {
            static int argc = 0;
            new QApplication(argc, nullptr);
        }
        AppSettings::instance().clear();
    }

    void cleanup()
    {
        AppSettings::instance().clear();
    }

    // ── 1. All three buttons exist (located by objectName) ──────────────────
    void allThreeButtons_existByObjectName()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* lev  = applet.findChild<QPushButton*>(QStringLiteral("TxLevButton"));
        auto* eq   = applet.findChild<QPushButton*>(QStringLiteral("TxEqButton"));
        auto* proc = applet.findChild<QPushButton*>(QStringLiteral("TxProcButton"));

        QVERIFY(lev  != nullptr);
        QVERIFY(eq   != nullptr);
        QVERIFY(proc != nullptr);
    }

    // ── 2. PROC button is disabled (3M-3a-ii placeholder) ───────────────────
    void procButton_isDisabled()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* proc = applet.findChild<QPushButton*>(QStringLiteral("TxProcButton"));
        QVERIFY(proc != nullptr);
        QVERIFY(!proc->isEnabled());
    }

    // ── 3. LEV.click() flips TransmitModel::txLevelerOn ─────────────────────
    void levButton_clickFlipsTxLevelerOn()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* lev = applet.findChild<QPushButton*>(QStringLiteral("TxLevButton"));
        QVERIFY(lev != nullptr);
        QVERIFY(lev->isCheckable());

        // Default Lev_On is true (Thetis database.cs:4588 [v2.10.3.13]).
        QCOMPARE(rm.transmitModel().txLevelerOn(), true);

        lev->click();
        QCOMPARE(rm.transmitModel().txLevelerOn(), false);

        lev->click();
        QCOMPARE(rm.transmitModel().txLevelerOn(), true);
    }

    // ── 4. EQ.click() flips TransmitModel::txEqEnabled ──────────────────────
    void eqButton_clickFlipsTxEqEnabled()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* eq = applet.findChild<QPushButton*>(QStringLiteral("TxEqButton"));
        QVERIFY(eq != nullptr);
        QVERIFY(eq->isCheckable());

        // Default TXEQEnabled is false (Thetis database.cs:4566 [v2.10.3.13]).
        QCOMPARE(rm.transmitModel().txEqEnabled(), false);

        eq->click();
        QCOMPARE(rm.transmitModel().txEqEnabled(), true);

        eq->click();
        QCOMPARE(rm.transmitModel().txEqEnabled(), false);
    }

    // ── 5. Model→UI: setTxLevelerOn(false) un-checks LEV button ─────────────
    void setTxLevelerOn_updatesLevButton()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* lev = applet.findChild<QPushButton*>(QStringLiteral("TxLevButton"));
        QVERIFY(lev != nullptr);

        // Sync from the default (true).
        applet.syncFromModel();
        QCOMPARE(lev->isChecked(), true);

        rm.transmitModel().setTxLevelerOn(false);
        QCOMPARE(lev->isChecked(), false);

        rm.transmitModel().setTxLevelerOn(true);
        QCOMPARE(lev->isChecked(), true);
    }

    // ── 6. Model→UI: setTxEqEnabled(true) checks EQ button ──────────────────
    void setTxEqEnabled_updatesEqButton()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* eq = applet.findChild<QPushButton*>(QStringLiteral("TxEqButton"));
        QVERIFY(eq != nullptr);

        // Sync from the default (false).
        applet.syncFromModel();
        QCOMPARE(eq->isChecked(), false);

        rm.transmitModel().setTxEqEnabled(true);
        QCOMPARE(eq->isChecked(), true);

        rm.transmitModel().setTxEqEnabled(false);
        QCOMPARE(eq->isChecked(), false);
    }

    // ── 7. PROC has the documented "coming in 3M-3a-ii" tooltip ─────────────
    void procButton_hasComingTooltip()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* proc = applet.findChild<QPushButton*>(QStringLiteral("TxProcButton"));
        QVERIFY(proc != nullptr);
        QVERIFY(proc->toolTip().contains(QStringLiteral("3M-3a-ii")));
    }
};

QTEST_MAIN(TestTxAppletLevEqProc)
#include "tst_tx_applet_lev_eq_proc.moc"
