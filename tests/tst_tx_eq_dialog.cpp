// no-port-check: NereusSDR-original test file.  All Thetis source cites
// for the underlying TransmitModel properties live in TransmitModel.h
// and the dialog source itself.
// =================================================================
// tests/tst_tx_eq_dialog.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-3a-i Batch 3 (Task A.1) — TxEqDialog scaffold smoke tests.
//
// TxEqDialog is the modeless 10-band TX EQ dialog launched from the
// TxApplet's [EQ] right-click and the Tools → TX Equalizer menu.
// Controls bidirectionally bind to RadioModel::transmitModel() with
// an m_updatingFromModel echo guard.
//
// Tests:
//   1. Dialog constructs without crash (RadioModel default ctor).
//   2. Initial values populate from TransmitModel defaults
//      (preamp=0, band[0]=-12, freq[0]=32, enable=false, Nc=2048).
//   3. Move preamp slider → TransmitModel.txEqPreampChanged emitted.
//   4. Move band 0 slider → TransmitModel.txEqBandChanged emitted with
//      idx=0 + new value.
//   5. Move freq 0 spinbox → TransmitModel.txEqFreqChanged emitted.
//   6. Toggle enable checkbox → TransmitModel.txEqEnabledChanged emitted.
//   7. setTxEqPreamp(N) external setter → dialog preamp slider/spin
//      updates to N (round-trip via syncFromModel).
//   8. Echo guard: setting a TransmitModel value that triggers UI
//      update doesn't cause a re-emit storm (no infinite loop —
//      each setter only fires its own signal once).
//   9. Singleton: TxEqDialog::instance(...) returns the same pointer
//      on repeated calls.
//
// =================================================================

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QSlider>
#include <QSpinBox>

#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "gui/applets/TxEqDialog.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

static const QString kMac = QStringLiteral("aa:bb:cc:dd:ee:ff");

class TestTxEqDialog : public QObject {
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

    // ── 1. Construct ────────────────────────────────────────────────
    void constructsWithoutCrash()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        QVERIFY(dlg.findChild<QCheckBox*>(QStringLiteral("TxEqEnableChk")));
        QVERIFY(dlg.findChild<QSlider*>(QStringLiteral("TxEqPreampSlider")));
    }

    // ── 2. Initial values populate from TransmitModel defaults ─────
    void initialValues_matchTransmitModelDefaults()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);

        TransmitModel& tx = rm.transmitModel();

        // Enable default off.
        auto* en = dlg.findChild<QCheckBox*>(QStringLiteral("TxEqEnableChk"));
        QVERIFY(en);
        QCOMPARE(en->isChecked(), tx.txEqEnabled());
        QCOMPARE(en->isChecked(), false);

        // Preamp default 0.
        auto* pre = dlg.findChild<QSlider*>(QStringLiteral("TxEqPreampSlider"));
        auto* preSpin = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqPreampSpin"));
        QVERIFY(pre);
        QVERIFY(preSpin);
        QCOMPARE(pre->value(), tx.txEqPreamp());
        QCOMPARE(preSpin->value(), tx.txEqPreamp());
        QCOMPARE(pre->value(), 0);

        // Band 0 default -12 (matches TransmitModel m_txEqBand init).
        auto* b0 = dlg.findChild<QSlider*>(QStringLiteral("TxEqBandSlider0"));
        auto* b0s = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqBandSpin0"));
        QVERIFY(b0);
        QVERIFY(b0s);
        QCOMPARE(b0->value(), tx.txEqBand(0));
        QCOMPARE(b0s->value(), tx.txEqBand(0));
        QCOMPARE(b0->value(), -12);

        // Freq 0 default 32 Hz.
        auto* f0 = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqFreqSpin0"));
        QVERIFY(f0);
        QCOMPARE(f0->value(), tx.txEqFreq(0));
        QCOMPARE(f0->value(), 32);

        // Nc default 2048.
        auto* nc = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqNcSpin"));
        QVERIFY(nc);
        QCOMPARE(nc->value(), tx.txEqNc());
        QCOMPARE(nc->value(), 2048);

        // Mp default off.
        auto* mp = dlg.findChild<QCheckBox*>(QStringLiteral("TxEqMpChk"));
        QVERIFY(mp);
        QCOMPARE(mp->isChecked(), tx.txEqMp());
        QCOMPARE(mp->isChecked(), false);

        // Ctfmode default 0, Wintype default 0.
        auto* ctf = dlg.findChild<QComboBox*>(QStringLiteral("TxEqCtfmodeCombo"));
        auto* win = dlg.findChild<QComboBox*>(QStringLiteral("TxEqWintypeCombo"));
        QVERIFY(ctf);
        QVERIFY(win);
        QCOMPARE(ctf->currentIndex(), tx.txEqCtfmode());
        QCOMPARE(win->currentIndex(), tx.txEqWintype());
        QCOMPARE(ctf->currentIndex(), 0);
        QCOMPARE(win->currentIndex(), 0);
    }

    // ── 3. Preamp slider → txEqPreampChanged ────────────────────────
    void preampSlider_emitsTxEqPreampChanged()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        QSignalSpy spy(&tx, &TransmitModel::txEqPreampChanged);

        auto* pre = dlg.findChild<QSlider*>(QStringLiteral("TxEqPreampSlider"));
        QVERIFY(pre);
        pre->setValue(7);

        // Slider emits valueChanged → onPreampChanged → setTxEqPreamp → signal.
        // Note: the model→UI sync handler also fires syncFromModel which
        // re-sets the spinbox; but setTxEqPreamp itself only fires once.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toInt(), 7);
        QCOMPARE(tx.txEqPreamp(), 7);
    }

    // ── 4. Band 0 slider → txEqBandChanged with idx=0 ───────────────
    void band0Slider_emitsTxEqBandChanged()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        QSignalSpy spy(&tx, &TransmitModel::txEqBandChanged);

        auto* b0 = dlg.findChild<QSlider*>(QStringLiteral("TxEqBandSlider0"));
        QVERIFY(b0);
        b0->setValue(5);

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toInt(), 0);
        QCOMPARE(args.at(1).toInt(), 5);
        QCOMPARE(tx.txEqBand(0), 5);
    }

    // ── 5. Freq 0 spinbox → txEqFreqChanged with idx=0 ──────────────
    void freq0Spin_emitsTxEqFreqChanged()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        QSignalSpy spy(&tx, &TransmitModel::txEqFreqChanged);

        auto* f0 = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqFreqSpin0"));
        QVERIFY(f0);
        f0->setValue(75);

        QCOMPARE(spy.count(), 1);
        const QList<QVariant> args = spy.takeFirst();
        QCOMPARE(args.at(0).toInt(), 0);
        QCOMPARE(args.at(1).toInt(), 75);
        QCOMPARE(tx.txEqFreq(0), 75);
    }

    // ── 6. Enable checkbox → txEqEnabledChanged ─────────────────────
    void enableCheckbox_emitsTxEqEnabledChanged()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        QSignalSpy spy(&tx, &TransmitModel::txEqEnabledChanged);

        auto* en = dlg.findChild<QCheckBox*>(QStringLiteral("TxEqEnableChk"));
        QVERIFY(en);
        en->setChecked(true);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), true);
        QCOMPARE(tx.txEqEnabled(), true);
    }

    // ── 7. External setTxEqPreamp(N) → dialog UI updates ────────────
    void externalSetTxEqPreamp_updatesDialogPreamp()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();

        auto* pre     = dlg.findChild<QSlider*>(QStringLiteral("TxEqPreampSlider"));
        auto* preSpin = dlg.findChild<QSpinBox*>(QStringLiteral("TxEqPreampSpin"));
        QVERIFY(pre && preSpin);
        QCOMPARE(pre->value(), 0);

        tx.setTxEqPreamp(11);
        QCOMPARE(pre->value(), 11);
        QCOMPARE(preSpin->value(), 11);
    }

    // ── 8. Echo guard — model setter fires signal exactly once ──────
    // If the echo guard were missing, the slider valueChanged from
    // syncFromModel would call back into setTxEqPreamp, which would
    // emit again, etc.  Verify the signal count stays at 1.
    void echoGuard_externalSetterDoesNotReEmit()
    {
        RadioModel rm;
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();
        QSignalSpy spy(&tx, &TransmitModel::txEqPreampChanged);

        tx.setTxEqPreamp(4);   // single emit expected
        QCOMPARE(spy.count(), 1);

        tx.setTxEqPreamp(4);   // no-op — value unchanged, no re-emit
        QCOMPARE(spy.count(), 1);
    }

    // ── 9. Singleton — instance() returns same pointer ──────────────
    void singleton_returnsSameInstance()
    {
        RadioModel rm;
        TxEqDialog* a = TxEqDialog::instance(&rm);
        TxEqDialog* b = TxEqDialog::instance(&rm);
        QVERIFY(a != nullptr);
        QCOMPARE(a, b);
        // Cleanup — the singleton survives across tests, so delete it
        // explicitly to avoid leaks across test-method boundaries.
        delete a;
    }

    // =====================================================================
    // Phase 3M-3a-i Batch 4 (A.2) — TX profile combo + Save/Save-As/Delete
    // =====================================================================

    // Helper: prime a RadioModel's MicProfileManager with the per-MAC scope
    // and seed Default + 20 factory profiles.  Mirrors the wiring done in
    // RadioModel::setupConnection (cpp:1108-1117).
    static void primeProfileManager(RadioModel* rm) {
        auto* mgr = rm->micProfileManager();
        QVERIFY(mgr);
        mgr->setMacAddress(kMac);
        mgr->load();
    }

    // ── A.2.1 ─ Profile combo populates from MicProfileManager ──────
    void profileCombo_populatesFromMicProfileManager()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TxEqDialog dlg(&rm);

        auto* combo = dlg.profileCombo();
        QVERIFY(combo);
        // Default + 20 factory profiles = 21 entries.
        QCOMPARE(combo->count(), 21);
        // Active profile "Default" should be the current selection.
        QCOMPARE(combo->currentText(), QStringLiteral("Default"));
        // Factory profiles must be present.
        QVERIFY(combo->findText(QStringLiteral("D-104+EQ"))     >= 0);
        QVERIFY(combo->findText(QStringLiteral("PR781+EQ"))     >= 0);
        QVERIFY(combo->findText(QStringLiteral("SSB 2.8k CFC")) >= 0);
    }

    // ── A.2.2 ─ Selecting a profile calls setActiveProfile ──────────
    void profileCombo_selectionCallsSetActiveProfile()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TxEqDialog dlg(&rm);

        auto* combo = dlg.profileCombo();
        QVERIFY(combo);
        QSignalSpy spy(rm.micProfileManager(),
                       &MicProfileManager::activeProfileChanged);

        combo->setCurrentText(QStringLiteral("D-104+EQ"));
        QApplication::processEvents();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toString(),
                 QStringLiteral("D-104+EQ"));
        QCOMPARE(rm.micProfileManager()->activeProfileName(),
                 QStringLiteral("D-104+EQ"));
    }

    // ── A.2.3 ─ Selecting a factory profile updates the EQ controls ─
    void profileCombo_selectingFactoryProfileUpdatesEqControls()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();

        auto* combo  = dlg.profileCombo();
        auto* enable = dlg.findChild<QCheckBox*>(QStringLiteral("TxEqEnableChk"));
        auto* preamp = dlg.findChild<QSlider*>(QStringLiteral("TxEqPreampSlider"));
        auto* b0     = dlg.findChild<QSlider*>(QStringLiteral("TxEqBandSlider0"));
        QVERIFY(combo && enable && preamp && b0);

        // Switch to D-104+EQ — TXEQEnabled=true, TXEQPreamp=-6, TXEQ1=7.
        combo->setCurrentText(QStringLiteral("D-104+EQ"));
        QApplication::processEvents();

        QCOMPARE(tx.txEqEnabled(), true);
        QCOMPARE(tx.txEqPreamp(), -6);
        QCOMPARE(tx.txEqBand(0), 7);
        QCOMPARE(enable->isChecked(), true);
        QCOMPARE(preamp->value(), -6);
        QCOMPARE(b0->value(), 7);
    }

    // ── A.2.4 ─ Save button triggers overwrite confirmation ─────────
    void saveButton_overwritesActiveProfileAfterConfirm()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();

        // Capture overwrite confirmation prompt; accept.
        QString overwriteName;
        dlg.setOverwriteConfirmHook([&overwriteName](const QString& n) {
            overwriteName = n;
            return true;
        });

        // Mutate model and click Save.
        tx.setTxEqPreamp(11);
        dlg.saveBtn()->click();
        QApplication::processEvents();

        QCOMPARE(overwriteName, QStringLiteral("Default"));
        // Verify the saved value.
        QCOMPARE(AppSettings::instance().value(
                     QStringLiteral("hardware/%1/tx/profile/Default/TXEQPreamp")
                         .arg(kMac)).toString(),
                 QStringLiteral("11"));
    }

    // ── A.2.5 ─ Save button: declining overwrite is a no-op ─────────
    void saveButton_declinedOverwriteIsNoop()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();

        // Decline overwrite.
        dlg.setOverwriteConfirmHook([](const QString&) { return false; });

        tx.setTxEqPreamp(7);
        dlg.saveBtn()->click();
        QApplication::processEvents();

        // Default's TXEQPreamp must remain at the seeded value (0).
        QCOMPARE(AppSettings::instance().value(
                     QStringLiteral("hardware/%1/tx/profile/Default/TXEQPreamp")
                         .arg(kMac)).toString(),
                 QStringLiteral("0"));
    }

    // ── A.2.6 ─ Save As button creates new profile + sets active ────
    void saveAsButton_createsNewProfileAndActivates()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TxEqDialog dlg(&rm);
        TransmitModel& tx = rm.transmitModel();

        const int initialCount = dlg.profileCombo()->count();

        // Hook the Save As prompt.
        dlg.setSaveAsPromptHook([](const QString& /*seed*/) {
            return std::make_pair(true, QStringLiteral("My Custom EQ"));
        });

        tx.setTxEqPreamp(8);
        tx.setTxEqEnabled(true);
        dlg.saveAsBtn()->click();
        QApplication::processEvents();

        // New profile present.
        QCOMPARE(dlg.profileCombo()->count(), initialCount + 1);
        QVERIFY(dlg.profileCombo()->findText(QStringLiteral("My Custom EQ")) >= 0);
        // Becomes active.
        QCOMPARE(rm.micProfileManager()->activeProfileName(),
                 QStringLiteral("My Custom EQ"));
        QCOMPARE(dlg.profileCombo()->currentText(),
                 QStringLiteral("My Custom EQ"));
        // Saved values present.
        QCOMPARE(AppSettings::instance().value(
                     QStringLiteral("hardware/%1/tx/profile/My Custom EQ/TXEQPreamp")
                         .arg(kMac)).toString(),
                 QStringLiteral("8"));
    }

    // ── A.2.7 ─ Save As: comma in name is sanitized ─────────────────
    void saveAsButton_stripsCommas()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TxEqDialog dlg(&rm);

        dlg.setSaveAsPromptHook([](const QString&) {
            return std::make_pair(true, QStringLiteral("Bad,Name,Profile"));
        });

        dlg.saveAsBtn()->click();
        QApplication::processEvents();

        QVERIFY(dlg.profileCombo()->findText(QStringLiteral("Bad_Name_Profile")) >= 0);
        QVERIFY(dlg.profileCombo()->findText(QStringLiteral("Bad,Name,Profile")) < 0);
    }

    // ── A.2.8 ─ Delete button removes active profile ────────────────
    void deleteButton_removesActiveProfile()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TxEqDialog dlg(&rm);

        // Switch to D-104+EQ then delete.
        dlg.profileCombo()->setCurrentText(QStringLiteral("D-104+EQ"));
        QApplication::processEvents();
        const int countBefore = dlg.profileCombo()->count();

        dlg.setDeleteConfirmHook([](const QString&) { return true; });
        dlg.deleteBtn()->click();
        QApplication::processEvents();

        QCOMPARE(dlg.profileCombo()->count(), countBefore - 1);
        QVERIFY(dlg.profileCombo()->findText(QStringLiteral("D-104+EQ")) < 0);
        // Active falls back to the next remaining (sorted lexicographically).
        QVERIFY(rm.micProfileManager()->activeProfileName()
                != QStringLiteral("D-104+EQ"));
    }

    // ── A.2.9 ─ Delete: declined confirmation is a no-op ────────────
    void deleteButton_declinedIsNoop()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TxEqDialog dlg(&rm);

        const int countBefore = dlg.profileCombo()->count();

        dlg.setDeleteConfirmHook([](const QString&) { return false; });
        dlg.deleteBtn()->click();
        QApplication::processEvents();

        QCOMPARE(dlg.profileCombo()->count(), countBefore);
    }

    // ── A.2.10 ─ Delete: last-remaining surfaces verbatim Thetis msg ─
    void deleteButton_lastRemainingShowsVerbatimThetisString()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        // Reduce to last remaining.
        for (const QString& n : rm.micProfileManager()->profileNames()) {
            if (n == QStringLiteral("Default")) { continue; }
            rm.micProfileManager()->deleteProfile(n);
        }
        QCOMPARE(rm.micProfileManager()->profileNames().size(), 1);

        TxEqDialog dlg(&rm);

        QString rejectionMsg;
        dlg.setRejectionMessageHook([&rejectionMsg](const QString& msg) {
            rejectionMsg = msg;
        });
        dlg.setDeleteConfirmHook([](const QString&) { return true; });

        dlg.deleteBtn()->click();
        QApplication::processEvents();

        QVERIFY2(rejectionMsg.contains(
                     QStringLiteral("It is not possible to delete the last remaining TX profile")),
                 qPrintable(QStringLiteral("captured rejection message: ") + rejectionMsg));
    }

    // ── A.2.11 ─ External activeProfileChanged updates combo (no echo) ─
    void activeProfileChanged_updatesComboNoEcho()
    {
        RadioModel rm;
        primeProfileManager(&rm);
        TxEqDialog dlg(&rm);

        QSignalSpy spy(rm.micProfileManager(),
                       &MicProfileManager::activeProfileChanged);

        // External setActiveProfile → manager emits activeProfileChanged
        // → dialog's onActiveProfileChanged updates combo.  Because the
        // combo update uses a QSignalBlocker, the combo's currentText-
        // Changed slot does NOT re-fire setActiveProfile (no echo).
        rm.micProfileManager()->setActiveProfile(
            QStringLiteral("AM"), &rm.transmitModel());
        QApplication::processEvents();

        // Single emission — echo guard works.
        QCOMPARE(spy.count(), 1);
        QCOMPARE(dlg.profileCombo()->currentText(), QStringLiteral("AM"));
    }
};

QTEST_MAIN(TestTxEqDialog)
#include "tst_tx_eq_dialog.moc"
