// tests/tst_tx_profile_setup_page.cpp  (NereusSDR)
//
// Phase 3M-1c chunk J (J.3 + J.4) — Setup → Audio → TX Profile editor page.
//
// no-port-check: NereusSDR-original test file.  Inline Thetis cites for the
// behaviours we mirror live in TxProfileSetupPage.cpp.
//
// J.3 — Editor page coverage:
//   - Constructor doesn't crash with default RadioModel + injected
//     MicProfileManager.
//   - Editing combo populated from MicProfileManager::profileNames().
//   - Save button with new name calls saveProfile and the manifest grows.
//   - Save button with empty name is a no-op (validation).
//   - Save button with existing name overwrites without consulting overwrite-
//     confirmation when the test-hook returns "yes".
//   - Delete button calls deleteProfile and the combo shrinks.
//   - Delete-on-last-remaining shows the verbatim Thetis string and refuses
//     (matches MicProfileManager::deleteProfile last-profile guard from F.3).
//
// J.4 — Focus-gated unsaved-changes prompt coverage:
//   - Combo programmatic change (no focus) does NOT trigger prompt.
//   - Combo user change (with focus) DOES trigger prompt when dirty.
//   - Prompt Yes saves the current profile then loads the new one.
//   - Prompt No discards then loads.
//   - Prompt Cancel reverts the combo to the original selection.
//
// Mirrors Thetis comboTXProfileName_SelectedIndexChanged (setup.cs:9505-9543
// [v2.10.3.13]).  Dialog mocking uses a m_promptHook injection point on
// TxProfileSetupPage so QInputDialog / QMessageBox don't actually pop up.

#include <QtTest/QtTest>
#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/MicProfileManager.h"
#include "gui/setup/TxProfileSetupPage.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {
const QString kMacA = QStringLiteral("aa:bb:cc:11:22:33");
} // namespace

class TestTxProfileSetupPage : public QObject {
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

    // ── J.3.1 ─ Constructor doesn't crash ────────────────────────────────────
    void constructor_doesNotCrash()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);
        QVERIFY(true);  // Survived construction.
    }

    // ── J.3.2 ─ Combo populated from MicProfileManager ───────────────────────
    void combo_populatedFromManager()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("Alpha", &tx);

        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);

        QComboBox* combo = page.profileCombo();
        QVERIFY(combo != nullptr);
        QVERIFY(combo->count() >= 2);
    }

    // ── J.3.3 ─ Save button with new name calls saveProfile ──────────────────
    void saveButton_newName_savesProfile()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);

        // Inject a prompt hook that returns "BrandNew" for the save dialog.
        page.setSavePromptHook([](const QString& /*current*/) {
            return TxProfileSetupPage::SavePromptResult{true, "BrandNew"};
        });

        QSignalSpy listSpy(&mgr, &MicProfileManager::profileListChanged);
        page.saveButton()->click();
        QApplication::processEvents();

        QVERIFY(mgr.profileNames().contains("BrandNew"));
        QCOMPARE(listSpy.count(), 1);
    }

    // ── J.3.4 ─ Empty save name is rejected (no save) ────────────────────────
    void saveButton_emptyName_isNoOp()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);

        page.setSavePromptHook([](const QString& /*current*/) {
            return TxProfileSetupPage::SavePromptResult{true, ""};
        });

        const int beforeCount = mgr.profileNames().count();
        page.saveButton()->click();
        QApplication::processEvents();

        QCOMPARE(mgr.profileNames().count(), beforeCount);
    }

    // ── J.3.5 ─ Whitespace-only save name is rejected ────────────────────────
    void saveButton_whitespaceName_isNoOp()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();

        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);

        page.setSavePromptHook([](const QString& /*current*/) {
            return TxProfileSetupPage::SavePromptResult{true, "   "};
        });

        const int beforeCount = mgr.profileNames().count();
        page.saveButton()->click();
        QApplication::processEvents();

        QCOMPARE(mgr.profileNames().count(), beforeCount);
    }

    // ── J.3.6 ─ Save with existing name + overwrite=Yes overwrites ───────────
    void saveButton_existingName_overwrite_yes()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("Existing", &tx);

        // Mutate model so overwrite is observable.
        tx.setMicGainDb(15);

        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);

        page.setSavePromptHook([](const QString& /*current*/) {
            return TxProfileSetupPage::SavePromptResult{true, "Existing"};
        });
        page.setOverwriteConfirmHook([]() {
            return true;  // user confirmed overwrite
        });

        page.saveButton()->click();
        QApplication::processEvents();

        // Profile count unchanged (still Default + Existing), but the value
        // has been overwritten.
        const QString micGainKey = QStringLiteral(
            "hardware/%1/tx/profile/Existing/MicGain").arg(kMacA);
        QCOMPARE(AppSettings::instance().value(micGainKey).toString(),
                 QStringLiteral("15"));
    }

    // ── J.3.7 ─ Save with existing name + overwrite=No is a no-op ────────────
    void saveButton_existingName_overwrite_no()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("Existing", &tx);
        // Capture current micGain (should be the model default).
        const int beforeMicGain = tx.micGainDb();

        // Mutate model — but if overwrite is denied, AppSettings should
        // continue to show the previous value.
        tx.setMicGainDb(99);

        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);

        page.setSavePromptHook([](const QString& /*current*/) {
            return TxProfileSetupPage::SavePromptResult{true, "Existing"};
        });
        page.setOverwriteConfirmHook([]() {
            return false;  // user declined overwrite
        });

        page.saveButton()->click();
        QApplication::processEvents();

        const QString micGainKey = QStringLiteral(
            "hardware/%1/tx/profile/Existing/MicGain").arg(kMacA);
        // Expect the previous (pre-mutation) value still in storage.
        QCOMPARE(AppSettings::instance().value(micGainKey).toString(),
                 QString::number(beforeMicGain));
    }

    // ── J.3.8 ─ Delete button calls deleteProfile and combo shrinks ──────────
    void deleteButton_callsDeleteProfile()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("ToGo", &tx);

        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);

        // Select "ToGo" in the combo so delete operates on it.
        page.profileCombo()->setCurrentText("ToGo");
        QApplication::processEvents();

        page.setDeleteConfirmHook([]() { return true; });

        const int beforeCount = mgr.profileNames().count();
        page.deleteButton()->click();
        QApplication::processEvents();

        QCOMPARE(mgr.profileNames().count(), beforeCount - 1);
        QVERIFY(!mgr.profileNames().contains("ToGo"));
    }

    // ── J.3.9 ─ Delete-on-last-remaining shows verbatim Thetis string ───────
    void deleteButton_lastRemaining_refuses()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        // Only "Default" exists.

        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);
        page.profileCombo()->setCurrentText("Default");

        // Capture the rejection-message hook output.
        QString shownMessage;
        page.setRejectionMessageHook([&shownMessage](const QString& msg) {
            shownMessage = msg;
        });
        page.setDeleteConfirmHook([]() { return true; });

        page.deleteButton()->click();
        QApplication::processEvents();

        // Verbatim Thetis string per F.3 / chunk-J spec.
        QVERIFY2(shownMessage.contains(
                     QStringLiteral("It is not possible to delete the last remaining TX profile")),
                 qPrintable(QStringLiteral("captured rejection message: ") + shownMessage));
        QCOMPARE(mgr.profileNames().count(), 1);
    }

    // =========================================================================
    // J.4 — Focus-gated unsaved-changes prompt
    // =========================================================================

    // ── J.4.1 ─ Programmatic combo change (no focus) does NOT trigger prompt
    void programmaticChange_noPromptFires()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("ProfileA", &tx);

        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);

        bool promptFired = false;
        page.setUnsavedPromptHook(
            [&promptFired](const QString&) {
                promptFired = true;
                return TxProfileSetupPage::UnsavedPromptResult::No;
            });

        // Mark dirty (so a prompt would fire on user-driven change).
        tx.setMicGainDb(7);
        QApplication::processEvents();

        // Programmatic change — combo does NOT have focus.
        page.profileCombo()->setCurrentText("ProfileA");
        QApplication::processEvents();

        QVERIFY(!promptFired);
    }

    // ── J.4.2 ─ User-driven combo change (focus) DOES trigger prompt ────────
    void userChange_promptFiresWhenDirty()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("ProfileA", &tx);

        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);

        bool promptFired = false;
        page.setUnsavedPromptHook(
            [&promptFired](const QString&) {
                promptFired = true;
                return TxProfileSetupPage::UnsavedPromptResult::No;
            });

        // Mark dirty.
        tx.setMicGainDb(13);
        QApplication::processEvents();

        // Simulate a user-driven change: page exposes a test seam to inject
        // the focus state (so we don't need a real window-show + focus event).
        page.simulateUserComboChangeForTest("ProfileA");
        QApplication::processEvents();

        QVERIFY(promptFired);
    }

    // ── J.4.3 ─ Prompt: Yes saves then loads ─────────────────────────────────
    void userChange_yesSavesAndLoads()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("ProfileA", &tx);

        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);

        page.setUnsavedPromptHook(
            [](const QString&) {
                return TxProfileSetupPage::UnsavedPromptResult::Yes;
            });

        // Mark dirty after the initial Default profile load.
        tx.setMicGainDb(42);

        // Active was "Default".  User picks "ProfileA".
        page.simulateUserComboChangeForTest("ProfileA");
        QApplication::processEvents();

        // Default should now have MicGain=42 saved (Yes path performed save
        // before switching).
        const QString defaultMicKey = QStringLiteral(
            "hardware/%1/tx/profile/Default/MicGain").arg(kMacA);
        QCOMPARE(AppSettings::instance().value(defaultMicKey).toString(),
                 QStringLiteral("42"));

        // Active now ProfileA.
        QCOMPARE(mgr.activeProfileName(), QStringLiteral("ProfileA"));
    }

    // ── J.4.4 ─ Prompt: No discards then loads ───────────────────────────────
    void userChange_noDiscardsAndLoads()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        // Capture pre-save MicGain on Default profile (the seed value).
        const QString defaultMicKey = QStringLiteral(
            "hardware/%1/tx/profile/Default/MicGain").arg(kMacA);
        const QString preMicGain
            = AppSettings::instance().value(defaultMicKey).toString();

        mgr.saveProfile("ProfileA", &tx);

        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);

        page.setUnsavedPromptHook(
            [](const QString&) {
                return TxProfileSetupPage::UnsavedPromptResult::No;
            });

        // Mark dirty.
        tx.setMicGainDb(99);
        QApplication::processEvents();

        page.simulateUserComboChangeForTest("ProfileA");
        QApplication::processEvents();

        // Default's MicGain must NOT have been overwritten with 99.
        QCOMPARE(AppSettings::instance().value(defaultMicKey).toString(),
                 preMicGain);
        QCOMPARE(mgr.activeProfileName(), QStringLiteral("ProfileA"));
    }

    // ── J.4.5 ─ Prompt: Cancel reverts the combo to the original ────────────
    void userChange_cancelRevertsCombo()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        mgr.saveProfile("ProfileA", &tx);

        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);

        page.setUnsavedPromptHook(
            [](const QString&) {
                return TxProfileSetupPage::UnsavedPromptResult::Cancel;
            });

        tx.setMicGainDb(13);
        QApplication::processEvents();

        const QString originalActive = mgr.activeProfileName();
        page.simulateUserComboChangeForTest("ProfileA");
        QApplication::processEvents();

        // Cancel keeps active at original.
        QCOMPARE(mgr.activeProfileName(), originalActive);
        QCOMPARE(page.profileCombo()->currentText(), originalActive);
    }
};

QTEST_MAIN(TestTxProfileSetupPage)
#include "tst_tx_profile_setup_page.moc"
