// no-port-check: NereusSDR-original test file.  All Thetis source cites
// for the underlying TransmitModel properties live in TransmitModel.h.
// =================================================================
// tests/tst_tx_applet_lev_eq_proc.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-3a-i Batch 2 (Task F) — TxApplet LEV / EQ / PROC quick toggles.
// Phase 3M-3a-ii Batch 6 (Task F + A) — PROC enabled + CFC button added.
//
// TxApplet now hosts a row of FOUR checkable buttons between the MON volume
// slider and the TUNE/MOX row:
//   LEV  — bidirectional ↔ TransmitModel::txLevelerOn (green-checked style).
//   EQ   — bidirectional ↔ TransmitModel::txEqEnabled (green-checked style).
//          Right-click → TxEqDialog (3M-3a-i Batch 3).
//   PROC — bidirectional ↔ TransmitModel::cpdrOn (3M-3a-ii Batch 6 Task F).
//          Drives WDSP TXA compressor.run via SetTXACompressorRun.
//   CFC  — bidirectional ↔ TransmitModel::cfcEnabled (3M-3a-ii Batch 6 Task A).
//          Right-click → modeless TxCfcDialog.
//
// File name retained for git history continuity even though it now also
// covers PROC + CFC (extended in Batch 6).
//
// Tests:
//   1. All four buttons exist (located by objectName).
//   2. PROC button is enabled (was disabled placeholder pre-Batch 6).
//   3. LEV.click() flips TransmitModel::txLevelerOn.
//   4. EQ.click() flips TransmitModel::txEqEnabled.
//   5. Model→UI: setTxLevelerOn(false) un-checks LEV button.
//   6. Model→UI: setTxEqEnabled(true) checks EQ button.
//   7. PROC tooltip mentions WDSP/CPDR (no more "coming in 3M-3a-ii").
//   8. PROC.click() flips TransmitModel::cpdrOn.
//   9. Model→UI: setCpdrOn(true) checks PROC button.
//   10. CFC.click() flips TransmitModel::cfcEnabled.
//   11. Model→UI: setCfcEnabled(true) checks CFC button.
//   12. CFC right-click triggers requestOpenCfcDialog (single instance).
//
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QPushButton>

#include "core/AppSettings.h"
#include "gui/applets/TxApplet.h"
#include "gui/applets/TxCfcDialog.h"
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

    // ── 1. All four buttons exist (located by objectName) ──────────────────
    void allFourButtons_existByObjectName()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* lev  = applet.findChild<QPushButton*>(QStringLiteral("TxLevButton"));
        auto* eq   = applet.findChild<QPushButton*>(QStringLiteral("TxEqButton"));
        auto* proc = applet.findChild<QPushButton*>(QStringLiteral("TxProcButton"));
        auto* cfc  = applet.findChild<QPushButton*>(QStringLiteral("TxCfcButton"));

        QVERIFY(lev  != nullptr);
        QVERIFY(eq   != nullptr);
        QVERIFY(proc != nullptr);
        QVERIFY(cfc  != nullptr);
    }

    // ── 2. PROC button is enabled (3M-3a-ii Batch 6 — was disabled) ────────
    void procButton_isEnabled()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* proc = applet.findChild<QPushButton*>(QStringLiteral("TxProcButton"));
        QVERIFY(proc != nullptr);
        QVERIFY(proc->isEnabled());
        QVERIFY(proc->isCheckable());
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

    // ── 7. PROC tooltip mentions CPDR / WDSP (no more "coming in 3M-3a-ii") ─
    void procButton_tooltipMentionsCpdr()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* proc = applet.findChild<QPushButton*>(QStringLiteral("TxProcButton"));
        QVERIFY(proc != nullptr);
        QVERIFY(proc->toolTip().contains(QStringLiteral("CPDR")));
        QVERIFY(!proc->toolTip().contains(QStringLiteral("coming in")));
    }

    // ── 8. PROC.click() flips TransmitModel::cpdrOn ─────────────────────────
    void procButton_clickFlipsCpdrOn()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* proc = applet.findChild<QPushButton*>(QStringLiteral("TxProcButton"));
        QVERIFY(proc != nullptr);

        // Default cpdrOn is false (TransmitModel.h:1328).
        QCOMPARE(rm.transmitModel().cpdrOn(), false);

        proc->click();
        QCOMPARE(rm.transmitModel().cpdrOn(), true);

        proc->click();
        QCOMPARE(rm.transmitModel().cpdrOn(), false);
    }

    // ── 9. Model→UI: setCpdrOn(true) checks PROC button ─────────────────────
    void setCpdrOn_updatesProcButton()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* proc = applet.findChild<QPushButton*>(QStringLiteral("TxProcButton"));
        QVERIFY(proc != nullptr);

        // Sync from the default (false).
        applet.syncFromModel();
        QCOMPARE(proc->isChecked(), false);

        rm.transmitModel().setCpdrOn(true);
        QCOMPARE(proc->isChecked(), true);

        rm.transmitModel().setCpdrOn(false);
        QCOMPARE(proc->isChecked(), false);
    }

    // ── 10. CFC.click() flips TransmitModel::cfcEnabled ─────────────────────
    void cfcButton_clickFlipsCfcEnabled()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* cfc = applet.findChild<QPushButton*>(QStringLiteral("TxCfcButton"));
        QVERIFY(cfc != nullptr);
        QVERIFY(cfc->isCheckable());
        QVERIFY(cfc->isEnabled());

        // Default cfcEnabled is false (TransmitModel.h:1303).
        QCOMPARE(rm.transmitModel().cfcEnabled(), false);

        cfc->click();
        QCOMPARE(rm.transmitModel().cfcEnabled(), true);

        cfc->click();
        QCOMPARE(rm.transmitModel().cfcEnabled(), false);
    }

    // ── 11. Model→UI: setCfcEnabled(true) checks CFC button ─────────────────
    void setCfcEnabled_updatesCfcButton()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* cfc = applet.findChild<QPushButton*>(QStringLiteral("TxCfcButton"));
        QVERIFY(cfc != nullptr);

        applet.syncFromModel();
        QCOMPARE(cfc->isChecked(), false);

        rm.transmitModel().setCfcEnabled(true);
        QCOMPARE(cfc->isChecked(), true);

        rm.transmitModel().setCfcEnabled(false);
        QCOMPARE(cfc->isChecked(), false);
    }

    // ── 12. requestOpenCfcDialog creates dialog (lazy) and reuses single
    //       instance on subsequent calls.  Dialog is modeless and parented
    //       to the applet's window.
    void requestOpenCfcDialog_lazyCreatesSingleInstance()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        // No dialog before first call.
        QPointer<TxCfcDialog> initial = applet.findChild<TxCfcDialog*>();
        QVERIFY(initial.isNull());

        applet.requestOpenCfcDialog();
        QApplication::processEvents();

        // Dialog now exists.  Could be a child of applet OR a top-level
        // window — search via the application widget list to be parent-agnostic.
        TxCfcDialog* first = nullptr;
        for (QWidget* w : QApplication::allWidgets()) {
            if (auto* dlg = qobject_cast<TxCfcDialog*>(w)) {
                first = dlg;
                break;
            }
        }
        QVERIFY(first != nullptr);
        QVERIFY(!first->isModal());
        QVERIFY(first->isVisible());

        // Second call returns the same instance — single-instance lazy
        // singleton.  Compare by counting TxCfcDialog widgets in the
        // application: still exactly one.
        applet.requestOpenCfcDialog();
        QApplication::processEvents();

        int dialogCount = 0;
        for (QWidget* w : QApplication::allWidgets()) {
            if (qobject_cast<TxCfcDialog*>(w)) {
                ++dialogCount;
            }
        }
        QCOMPARE(dialogCount, 1);
    }
};

QTEST_MAIN(TestTxAppletLevEqProc)
#include "tst_tx_applet_lev_eq_proc.moc"
