// no-port-check: NereusSDR-original test file.  All Thetis source cites
// for the underlying TransmitModel properties live in TransmitModel.h.
// =================================================================
// tests/tst_tx_applet_lev_eq_cfc.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-3a-i Batch 2 (Task F) — TxApplet LEV / EQ / PROC quick toggles.
// Phase 3M-3a-ii Batch 6 (Task F + A) — PROC enabled + CFC button added.
// Phase 3M-3a-ii post-bench cleanup — PROC removed from TxApplet (lives on
// PhoneCwApplet — see tst_phone_applet_proc.cpp).  File renamed from
// tst_tx_applet_lev_eq_proc.cpp.
//
// TxApplet now hosts a row of THREE checkable buttons between the MON volume
// slider and the TUNE/MOX row:
//   LEV  — bidirectional ↔ TransmitModel::txLevelerOn (green-checked style).
//   EQ   — bidirectional ↔ TransmitModel::txEqEnabled (green-checked style).
//          Right-click → TxEqDialog (3M-3a-i Batch 3).
//   CFC  — bidirectional ↔ TransmitModel::cfcEnabled (3M-3a-ii Batch 6 Task A).
//          Right-click → modeless TxCfcDialog.
//
// Tests:
//   1. All three buttons exist (located by objectName).
//   2. LEV.click() flips TransmitModel::txLevelerOn.
//   3. EQ.click() flips TransmitModel::txEqEnabled.
//   4. Model→UI: setTxLevelerOn(false) un-checks LEV button.
//   5. Model→UI: setTxEqEnabled(true) checks EQ button.
//   6. CFC.click() flips TransmitModel::cfcEnabled.
//   7. Model→UI: setCfcEnabled(true) checks CFC button.
//   8. CFC right-click triggers requestOpenCfcDialog (single instance).
//
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QLabel>
#include <QSplitter>
#include <QTemporaryDir>

#include "core/TxChannel.h"
#include "gui/containers/ContainerArrangeController.h"
#include "gui/containers/ContainerContentRegistry.h"
#include "gui/containers/ContainerManager.h"
#include "gui/containers/ContainerWorkspaceStore.h"
#include "gui/containers/FloatingContainer.h"
#include "gui/widgets/ParametricEqWidget.h"

#include "core/AppSettings.h"
#include "gui/applets/TxApplet.h"
#include "gui/applets/TxCfcDialog.h"
#include "gui/applets/TxEqDialog.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestTxAppletLevEqCfc : public QObject {
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

    // ── 1. All three buttons exist (located by objectName) ─────────────────
    //     PROC moved to PhoneCwApplet — verify it is NOT in TxApplet.
    void allThreeButtons_existByObjectName()
    {
        RadioModel rm;
        TxApplet applet(&rm);

        auto* lev  = applet.findChild<QPushButton*>(QStringLiteral("TxLevButton"));
        auto* eq   = applet.findChild<QPushButton*>(QStringLiteral("TxEqButton"));
        auto* cfc  = applet.findChild<QPushButton*>(QStringLiteral("TxCfcButton"));

        QVERIFY(lev  != nullptr);
        QVERIFY(eq   != nullptr);
        QVERIFY(cfc  != nullptr);

        // Post-bench cleanup: PROC button must NOT exist on TxApplet anymore.
        auto* proc = applet.findChild<QPushButton*>(QStringLiteral("TxProcButton"));
        QVERIFY(proc == nullptr);
    }

    // ── 2. LEV.click() flips TransmitModel::txLevelerOn ─────────────────────
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

    // ── 3. EQ.click() flips TransmitModel::txEqEnabled ──────────────────────
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

    // ── 4. Model→UI: setTxLevelerOn(false) un-checks LEV button ─────────────
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

    // ── 5. Model→UI: setTxEqEnabled(true) checks EQ button ──────────────────
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

    // ── 6. CFC.click() flips TransmitModel::cfcEnabled ─────────────────────
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

    // ── 7. Model→UI: setCfcEnabled(true) checks CFC button ─────────────────
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

    // ── 8. requestOpenCfcDialog creates dialog (lazy) and reuses single
    //      instance on subsequent calls.  Dialog is modeless and parented
    //      to the retained applet.
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

    void cfcDialogSurvivesPopOutReturnWithHistoryAndLiveUpdates_data()
    {
        QTest::addColumn<bool>("openBeforePopOut");
        QTest::addColumn<QString>("retirement");
        for (bool before : {false, true}) {
            for (const QString& action : {QString("return"), QString("close"), QString("remove")}) {
                QTest::newRow(qPrintable((before ? "open-before-" : "open-floated-") + action))
                    << before << action;
            }
        }
    }

    void cfcDialogSurvivesPopOutReturnWithHistoryAndLiveUpdates()
    {
        QFETCH(bool, openBeforePopOut);
        QFETCH(QString, retirement);
        QPointer<TxCfcDialog> dialog;
        QPointer<TxEqDialog> eqDialog;
        {
            QTemporaryDir dir;
            QVERIFY(dir.isValid());
            AppSettings settings(dir.filePath("settings.xml"));
            ContainerWorkspaceStore store(settings);
            QWidget owner;
            RadioModel rm;
            TxApplet applet(&rm, &owner);
            ContainerContentRegistry registry;
            registry.attachSingleton("applet:TX", &applet);
            QWidget dock;
            QSplitter splitter;
            ContainerManager manager(&dock, &splitter);
            manager.setWorkspaceAdapter(&store, &registry);
            WorkspaceDocument document;
            document.mainContainerId = "main";
            ContainerDocument container;
            container.id = "main";
            container.layout = ContentLayout::VerticalStack;
            const auto entry = registry.makeEntry("applet:TX");
            container.contents = {entry};
            document.containers = {container};
            QCOMPARE(manager.commitWorkspace(document, 0).status, CommitStatus::Saved);
            ContainerArrangeController arrange(store, &manager);
            if (openBeforePopOut) {
                applet.requestOpenCfcDialog();
                dialog = applet.cfcDialog();
            }
            QVERIFY(arrange.popOut(entry.id).ok);
            QPointer<FloatingContainer> shell = qobject_cast<FloatingContainer*>(applet.window());
            QVERIFY(shell);
            applet.setStationCfcBarChart([](bool) {});
            applet.requestOpenCfcDialog();
            if (dialog) {
                QCOMPARE(applet.cfcDialog(), dialog.data());
            }
            dialog = applet.cfcDialog();
            QVERIFY(dialog);
            const auto original = rm.transmitModel().effectiveCfcProfile();
            dialog->precompSpin()->setValue(original.compression.globalGainDb + 1.5);
            const auto edited = rm.transmitModel().effectiveCfcProfile();
            QVERIFY(edited != original);
            auto* eqButton = applet.findChild<QPushButton*>("TxEqButton");
            QVERIFY(eqButton);
            eqButton->customContextMenuRequested(QPoint());
            eqDialog = applet.findChild<TxEqDialog*>();
            QVERIFY(eqDialog);
            const int originalNc = rm.transmitModel().txEqNc();
            auto* nc = eqDialog->findChild<QSpinBox*>("TxEqNcSpin");
            QVERIFY(nc);
            nc->setValue(originalNc == 512 ? 1024 : 512);
            QVERIFY(rm.transmitModel().txEqNc() != originalNc);
            const QString shellId = shell->id();
            const auto returned = retirement == "return" ? arrange.returnEntry(entry.id)
                : retirement == "close" ? arrange.closeContainer(shellId)
                : arrange.removeContainer(shellId);
            QVERIFY2(returned.ok, qPrintable(returned.error));
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            QVERIFY(shell.isNull());
            // Return retains the applet, its open editor, and its undo history.
            QVERIFY2(dialog, "Returning TX must not destroy its CFC dialog with the floating shell");
            QCOMPARE(registry.singletonView("applet:TX"), &applet);
            QCOMPARE(applet.cfcDialog(), dialog.data());
            QVERIFY(eqDialog);
            eqButton->customContextMenuRequested(QPoint());
            QCOMPARE(applet.findChild<TxEqDialog*>(), eqDialog.data());
            auto* eqUndo = eqDialog->findChild<QPushButton*>("TxEqUndoBtn");
            QVERIFY(eqUndo && eqUndo->isEnabled());
            eqUndo->click();
            QCOMPARE(rm.transmitModel().txEqNc(), originalNc);
            applet.requestOpenCfcDialog();
            QCOMPARE(applet.cfcDialog(), dialog.data());
            QVERIFY(dialog->isVisible());
            auto* undo = dialog->findChild<QPushButton*>("TxCfcUndo");
            QVERIFY(undo && undo->isEnabled());
            undo->click();
            QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original);
            applet.setTxProcessingPermitted(false, "CFC permission withdrawn");
            QVERIFY(!dialog->precompSpin()->isEnabled());
            QCOMPARE(dialog->settingsReasonLabel()->text(), QString("CFC permission withdrawn"));
            applet.setTxProcessingPermitted(true);
            QVERIFY(dialog->precompSpin()->isEnabled());
            applet.applyStationCfcCompression(QList<double>(TxChannel::kCfcDisplayBinCount, 3.25));
            const auto bars = dialog->compWidget()->barChartData();
            QVERIFY(!bars.isEmpty());
            for (double value : bars) {
                QCOMPARE(value, 3.25);
            }
            applet.setStationCfcBarChartUnavailable("Display unavailable");
            QCOMPARE(dialog->barChartReasonLabel()->text(), QString("Display unavailable"));
            QCOMPARE(rm.transmitModel().effectiveCfcProfile(), original);
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(dialog.isNull());
        QVERIFY(eqDialog.isNull());
    }

    void cfcDialogDestructionClearsCacheAndAllowsReopen()
    {
        RadioModel rm;
        TxApplet applet(&rm);
        applet.requestOpenCfcDialog();
        QPointer<TxCfcDialog> first = applet.cfcDialog();
        QVERIFY(first);
        first->deleteLater();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(first.isNull());
        QVERIFY(applet.cfcDialog() == nullptr);
        // These delivery paths must safely tolerate a retired dialog.
        applet.setTxProcessingPermitted(false, "CFC permission withdrawn");
        applet.applyStationCfcCompression(QList<double>(TxChannel::kCfcDisplayBinCount, 3.25));
        applet.setStationCfcBarChartUnavailable("Display unavailable");
        applet.requestOpenCfcDialog();
        QVERIFY(applet.cfcDialog());
        QVERIFY(!applet.cfcDialog()->precompSpin()->isEnabled());
        QCOMPARE(applet.cfcDialog()->settingsReasonLabel()->text(), QString("CFC permission withdrawn"));
    }

};

QTEST_MAIN(TestTxAppletLevEqCfc)
#include "tst_tx_applet_lev_eq_cfc.moc"
