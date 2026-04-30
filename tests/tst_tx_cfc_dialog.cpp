// no-port-check: NereusSDR-original test file.  All Thetis source cites
// for the underlying TransmitModel CFC properties live in TransmitModel.h
// and the dialog source itself.
// =================================================================
// tests/tst_tx_cfc_dialog.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-3a-ii Batch 6 (Task A) — TxCfcDialog smoke + binding tests.
//
// TxCfcDialog is the modeless 10-band CFC editor launched from the
// TxApplet's [CFC] right-click and from CfcSetupPage's [Configure CFC
// bands…] button.  Controls bidirectionally bind to TransmitModel
// with an m_updatingFromModel echo guard.
//
// Tests:
//   1. Dialog constructs without crash and exposes the documented
//      32-control surface (profile combo + 5 buttons + 2 globals + 30
//      band spins).
//   2. Initial values match TransmitModel defaults (cfcPrecompDb=0,
//      cfcPostEqGainDb=0, cfcEqFreq[0]=0 / [9]=10000 per the database.cs
//      defaults; cfcCompressionDb[i]=5; cfcPostEqBandGainDb[i]=0).
//   3. Spinboxes drive TM (UI → model): precomp, post-EQ gain, freq[i],
//      comp[i], post-EQ band gain[i].
//   4. TM updates UI (model → UI): set precomp / post-EQ gain / per-band
//      values and verify dialog spinboxes reflect.
//   5. Profile combo populates from MicProfileManager and switches
//      active profile on selection.
//   6. Save / Save As / Delete buttons exist and route to the manager
//      (with hooks for the modal prompts).
//   7. Reset to factory defaults restores values from the "Default"
//      profile (round-trip via setActiveProfile).
//   8. Echo guard: external setCfcPrecompDb does not cause a re-emit
//      storm (signal fires exactly once).
//
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QSpinBox>

#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "gui/applets/TxCfcDialog.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

static const QString kMac = QStringLiteral("aa:bb:cc:dd:ee:ff");

class TestTxCfcDialog : public QObject {
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

    // Helper: prime a RadioModel's MicProfileManager with the per-MAC scope
    // and seed Default + 20 factory profiles (mirrors RadioModel::setupConnection).
    static void primeProfileManager(RadioModel* rm) {
        auto* mgr = rm->micProfileManager();
        QVERIFY(mgr);
        mgr->setMacAddress(kMac);
        mgr->load();
    }

    // ── 1. Dialog constructs and exposes 32 controls ────────────────
    void constructsWithExpectedControls()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TxCfcDialog dlg(&rm.transmitModel(), rm.micProfileManager());

        QVERIFY(dlg.profileCombo());
        QVERIFY(dlg.saveBtn());
        QVERIFY(dlg.saveAsBtn());
        QVERIFY(dlg.deleteBtn());
        QVERIFY(dlg.resetBtn());
        QVERIFY(dlg.closeBtn());
        QVERIFY(dlg.precompSpin());
        QVERIFY(dlg.postEqGainSpin());

        for (int i = 0; i < 10; ++i) {
            QVERIFY2(dlg.freqSpin(i),       qPrintable(QStringLiteral("freq spin %1").arg(i)));
            QVERIFY2(dlg.compSpin(i),       qPrintable(QStringLiteral("comp spin %1").arg(i)));
            QVERIFY2(dlg.postEqBandSpin(i), qPrintable(QStringLiteral("post-eq band %1").arg(i)));
        }

        QVERIFY(!dlg.isModal());
    }

    // ── 2. Initial values match TransmitModel CFC defaults ─────────
    void initialValues_matchTmDefaults()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, rm.micProfileManager());

        // Globals.
        QCOMPARE(dlg.precompSpin()->value(),    tx.cfcPrecompDb());
        QCOMPARE(dlg.postEqGainSpin()->value(), tx.cfcPostEqGainDb());
        QCOMPARE(tx.cfcPrecompDb(),    0);  // database.cs:4731 [v2.10.3.13]
        QCOMPARE(tx.cfcPostEqGainDb(), 0);  // database.cs:4732 [v2.10.3.13]

        // Per-band defaults — TransmitModel.h:1310 defines:
        // m_cfcEqFreqHz       = {0, 125, 250, 500, 1000, 2000, 3000, 4000, 5000, 10000};
        // m_cfcCompressionDb  = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
        // m_cfcPostEqBandGainDb = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
        for (int i = 0; i < 10; ++i) {
            QCOMPARE(dlg.freqSpin(i)->value(),       tx.cfcEqFreq(i));
            QCOMPARE(dlg.compSpin(i)->value(),       tx.cfcCompression(i));
            QCOMPARE(dlg.postEqBandSpin(i)->value(), tx.cfcPostEqBandGain(i));
        }
        QCOMPARE(dlg.freqSpin(0)->value(),    0);
        QCOMPARE(dlg.freqSpin(9)->value(), 10000);
        QCOMPARE(dlg.compSpin(0)->value(),    5);
        QCOMPARE(dlg.postEqBandSpin(0)->value(), 0);
    }

    // ── 3a. Precomp spin → setCfcPrecompDb ─────────────────────────
    void precompSpin_drivesTm()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, rm.micProfileManager());
        QSignalSpy spy(&tx, &TransmitModel::cfcPrecompDbChanged);

        dlg.precompSpin()->setValue(10);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toInt(), 10);
        QCOMPARE(tx.cfcPrecompDb(), 10);
    }

    // ── 3b. Post-EQ gain spin → setCfcPostEqGainDb ─────────────────
    void postEqGainSpin_drivesTm()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, rm.micProfileManager());
        QSignalSpy spy(&tx, &TransmitModel::cfcPostEqGainDbChanged);

        dlg.postEqGainSpin()->setValue(-12);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toInt(), -12);
        QCOMPARE(tx.cfcPostEqGainDb(), -12);
    }

    // ── 3c. Band 3 freq spin → setCfcEqFreq(3, ...) ────────────────
    void freqSpin_drivesTm()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, rm.micProfileManager());
        QSignalSpy spy(&tx, &TransmitModel::cfcEqFreqChanged);

        dlg.freqSpin(3)->setValue(750);

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toInt(), 3);
        QCOMPARE(args.at(1).toInt(), 750);
        QCOMPARE(tx.cfcEqFreq(3), 750);
    }

    // ── 3d. Band 5 comp spin → setCfcCompression(5, ...) ───────────
    void compSpin_drivesTm()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, rm.micProfileManager());
        QSignalSpy spy(&tx, &TransmitModel::cfcCompressionChanged);

        dlg.compSpin(5)->setValue(12);

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toInt(), 5);
        QCOMPARE(args.at(1).toInt(), 12);
        QCOMPARE(tx.cfcCompression(5), 12);
    }

    // ── 3e. Band 7 post-EQ spin → setCfcPostEqBandGain(7, ...) ─────
    void postEqBandSpin_drivesTm()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, rm.micProfileManager());
        QSignalSpy spy(&tx, &TransmitModel::cfcPostEqBandGainChanged);

        dlg.postEqBandSpin(7)->setValue(-6);

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toInt(), 7);
        QCOMPARE(args.at(1).toInt(), -6);
        QCOMPARE(tx.cfcPostEqBandGain(7), -6);
    }

    // ── 4. Model → UI sync (external setter updates dialog) ────────
    void externalSetters_updateDialog()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, rm.micProfileManager());

        tx.setCfcPrecompDb(8);
        QCOMPARE(dlg.precompSpin()->value(), 8);

        tx.setCfcPostEqGainDb(-3);
        QCOMPARE(dlg.postEqGainSpin()->value(), -3);

        tx.setCfcEqFreq(2, 1500);
        QCOMPARE(dlg.freqSpin(2)->value(), 1500);

        tx.setCfcCompression(4, 9);
        QCOMPARE(dlg.compSpin(4)->value(), 9);

        tx.setCfcPostEqBandGain(8, 7);
        QCOMPARE(dlg.postEqBandSpin(8)->value(), 7);
    }

    // ── 5. Profile combo populates and switches active profile ─────
    void profileCombo_populatesAndSwitches()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, rm.micProfileManager());

        auto* combo = dlg.profileCombo();
        QVERIFY(combo);
        // Default + 20 factory profiles = 21 entries (same bank as TxEqDialog).
        QCOMPARE(combo->count(), 21);
        QCOMPARE(combo->currentText(), QStringLiteral("Default"));

        QSignalSpy spy(rm.micProfileManager(),
                       &MicProfileManager::activeProfileChanged);

        combo->setCurrentText(QStringLiteral("D-104+EQ"));
        QApplication::processEvents();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toString(), QStringLiteral("D-104+EQ"));
        QCOMPARE(rm.micProfileManager()->activeProfileName(),
                 QStringLiteral("D-104+EQ"));
    }

    // ── 6. Save button routes through the overwrite hook ───────────
    // Note: MicProfileManager::captureLiveValues currently captures the
    // 50 EQ/Lev/ALC/mic/VOX/2-tone keys but not the 41 CFC/CPDR/CESSB/
    // PhRot keys (those are persisted to AppSettings via TransmitModel's
    // own per-property auto-persist path, separate from the profile bank).
    // So the test verifies routing (hook fires with the active profile
    // name) but does NOT assert CFC values reach the profile snapshot —
    // that's a follow-up enhancement to MicProfileManager outside Batch 6.
    void saveButton_routesThroughOverwriteHook()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, rm.micProfileManager());

        QString overwriteName;
        dlg.setOverwriteConfirmHook([&overwriteName](const QString& n) {
            overwriteName = n;
            return true;
        });

        tx.setCfcPrecompDb(11);
        dlg.saveBtn()->click();
        QApplication::processEvents();

        QCOMPARE(overwriteName, QStringLiteral("Default"));
    }

    // ── 6b. Save button: declined overwrite is a no-op ─────────────
    void saveButton_declinedIsNoop()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, rm.micProfileManager());

        bool hookFired = false;
        dlg.setOverwriteConfirmHook([&hookFired](const QString&) {
            hookFired = true;
            return false;  // decline
        });

        dlg.saveBtn()->click();
        QApplication::processEvents();

        QVERIFY(hookFired);
        // No exception, no crash, no-op.  Active profile unchanged.
        QCOMPARE(rm.micProfileManager()->activeProfileName(),
                 QStringLiteral("Default"));
    }

    // ── 7. Reset to defaults restores via setActiveProfile("Default") ─
    void resetButton_restoresDefaultProfile()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, rm.micProfileManager());

        // Switch to a non-Default profile then back via Reset.
        rm.micProfileManager()->setActiveProfile(
            QStringLiteral("D-104+EQ"), &tx);
        QApplication::processEvents();
        QCOMPARE(rm.micProfileManager()->activeProfileName(),
                 QStringLiteral("D-104+EQ"));

        dlg.setResetConfirmHook([]() { return true; });
        dlg.resetBtn()->click();
        QApplication::processEvents();

        QCOMPARE(rm.micProfileManager()->activeProfileName(),
                 QStringLiteral("Default"));
    }

    // ── 8. Echo guard — external setter fires signal exactly once ──
    void echoGuard_externalSetterFiresOnce()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TransmitModel& tx = rm.transmitModel();
        TxCfcDialog dlg(&tx, rm.micProfileManager());
        QSignalSpy spy(&tx, &TransmitModel::cfcPrecompDbChanged);

        tx.setCfcPrecompDb(7);
        QCOMPARE(spy.count(), 1);

        tx.setCfcPrecompDb(7);  // no-op: value unchanged
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestTxCfcDialog)
#include "tst_tx_cfc_dialog.moc"
