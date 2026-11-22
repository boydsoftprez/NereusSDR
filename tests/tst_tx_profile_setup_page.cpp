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
//
// Modification history (NereusSDR):
//   2026-11-17 - Export... and Import...: the file written, the unsaved
//                 question, import and a declined overwrite, and the
//                 Diagnostics page's button. Brian Sutton (W4KZO),
//                 AI-assisted via xAI Grok.
//   2026-11-22 - connectRadio for the export tests; export Yes with unsaved
//                 edits, import Cancel on the unsaved question, import over
//                 the active profile, refused names and values, the wiring
//                 keys left out, the newer-build note, the suggested file
//                 name, and the Diagnostics button really opening the TX
//                 Profile page; export says why an old name cannot be
//                 saved and keeps the edits, a cancelled file keeps them
//                 too, and the file name avoids Windows' reserved names.
//                 J.J. Boyd (KG4VCF), AI-assisted via Claude Code.

#include <QtTest/QtTest>
#include <QApplication>
#include <QComboBox>
#include <QPushButton>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/MicProfileManager.h"
#include "core/TxProfileFile.h"
#include "gui/SetupDialog.h"
#include "gui/diagnostics/DiagnosticsPhaseHPages.h"
#include "gui/setup/TxProfileSetupPage.h"
#include "models/RadioModel.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QStackedWidget>
#include <QTreeWidget>
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {
const QString kMacA = QStringLiteral("aa:bb:cc:11:22:33");

void connectRadio(RadioModel& model)
{
    RadioInfo info;
    info.macAddress = kMacA;
    model.setLastRadioInfoForTest(info);
    model.setConnectionStateForTest(ConnectionState::Connected);
}

QString storedValue(const QString& profile, const QString& key)
{
    return AppSettings::instance()
        .value(QStringLiteral("hardware/%1/tx/profile/%2/%3").arg(kMacA, profile, key))
        .toString();
}

// A profile file named `name` holding `keys`, written in `dir`.
QString writeProfileFile(const QTemporaryDir& dir, const QString& name,
                         const QHash<QString, QString>& keys)
{
    QJsonObject object;
    for (auto it = keys.constBegin(); it != keys.constEnd(); ++it) {
        object.insert(it.key(), it.value());
    }
    const QJsonObject root{{QStringLiteral("nereusTxProfileVersion"), TxProfileFile::kVersion},
                           {QStringLiteral("name"), name},
                           {QStringLiteral("keys"), object}};
    const QString path = dir.filePath(QStringLiteral("in.nereus-tx-profile"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write(QJsonDocument(root).toJson());
    return path;
}

TxProfileFile::Document readProfileFile(const QString& path)
{
    TxProfileFile::Document document;
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        TxProfileFile::decode(file.readAll(), &document, nullptr);
    }
    return document;
}
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
        // After 3M-3a-i Batch 4 (A.2): first launch seeds Default + 20
        // factory profiles.  Delete every non-Default profile so the
        // delete-Default click hits the F.3 last-remaining guard.
        for (const QString& name : mgr.profileNames()) {
            if (name == QStringLiteral("Default")) {
                continue;
            }
            QVERIFY(mgr.deleteProfile(name));
        }
        QCOMPARE(mgr.profileNames().count(), 1);

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

    void exportWritesTheSelectedProfile()
    {
        TransmitModel tx;
        tx.setMicGainDb(-11);
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("Desk"), &tx));
        QVERIFY(mgr.setActiveProfile(QStringLiteral("Desk"), &tx));

        RadioModel rm;
        connectRadio(rm);
        TxProfileSetupPage page(&rm, &mgr, &tx);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("Desk.nereus-tx-profile"));
        page.setExportPathHook([path] { return path; });
        QString shown;
        bool ok = false;
        page.setFileResultHook([&](bool success, const QString& text) {
            ok = success;
            shown = text;
        });
        page.exportButton()->click();
        QVERIFY(ok);
        QVERIFY(shown.contains(path));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        TxProfileFile::Document document;
        QString error;
        QVERIFY(TxProfileFile::decode(file.readAll(), &document, &error));
        QCOMPARE(document.name, QStringLiteral("Desk"));
        QCOMPARE(document.keys.value(QStringLiteral("MicGain")), QStringLiteral("-11"));
    }

    void exportOfUnsavedEditsAsksFirst()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.setActiveProfile(QStringLiteral("Default"), &tx));
        const QString stored = AppSettings::instance()
            .value(QStringLiteral("hardware/%1/tx/profile/Default/MicGain").arg(kMacA))
            .toString();

        RadioModel rm;
        connectRadio(rm);
        TxProfileSetupPage page(&rm, &mgr, &tx);
        page.setUnsavedPromptHook([](const QString&) {
            return TxProfileSetupPage::UnsavedPromptResult::No;
        });
        tx.setMicGainDb(-4);
        QApplication::processEvents();

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("Default.nereus-tx-profile"));
        page.setExportPathHook([path] { return path; });
        page.setFileResultHook([](bool, const QString&) {});
        page.exportButton()->click();

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        TxProfileFile::Document document;
        QVERIFY(TxProfileFile::decode(file.readAll(), &document, nullptr));
        QCOMPARE(document.keys.value(QStringLiteral("MicGain")), stored);
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("hardware/%1/tx/profile/Default/MicGain").arg(kMacA))
                     .toString(), stored);
    }

    void importAddsTheProfileAndSelectsIt()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);

        TxProfileFile::Document document;
        document.name = QStringLiteral("Brought,In");
        document.keys.insert(QStringLiteral("MicGain"), QStringLiteral("-21"));
        QByteArray bytes;
        QVERIFY(TxProfileFile::encode(document, &bytes, nullptr));
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("in.nereus-tx-profile"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), qint64(bytes.size()));
        file.close();

        page.setImportPathHook([path] { return path; });
        QString shown;
        page.setFileResultHook([&](bool success, const QString& text) {
            QVERIFY(success);
            shown = text;
        });
        page.importButton()->click();
        QCOMPARE(mgr.activeProfileName(), QStringLiteral("Brought_In"));
        QCOMPARE(tx.micGainDb(), -21);
        QVERIFY(shown.contains(QStringLiteral("Brought_In")));
    }

    void importOfAnExistingNameCanBeDeclined()
    {
        TransmitModel tx;
        tx.setMicGainDb(-7);
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("Desk"), &tx));
        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);
        page.profileCombo()->setCurrentText(QStringLiteral("Default"));

        TxProfileFile::Document document;
        document.name = QStringLiteral("Desk");
        document.keys.insert(QStringLiteral("MicGain"), QStringLiteral("-21"));
        QByteArray bytes;
        QVERIFY(TxProfileFile::encode(document, &bytes, nullptr));
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("in.nereus-tx-profile"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), qint64(bytes.size()));
        file.close();

        page.setImportPathHook([path] { return path; });
        page.setOverwriteConfirmHook([] { return false; });
        bool shown = false;
        page.setFileResultHook([&](bool, const QString&) { shown = true; });
        page.importButton()->click();
        QVERIFY(!shown);
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("hardware/%1/tx/profile/Desk/MicGain").arg(kMacA))
                     .toString(), QStringLiteral("-7"));
    }

    // Export's Yes saves the edits first and exports what was saved.
    void exportYesSavesTheEditsAndExportsThem()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.setActiveProfile(QStringLiteral("Default"), &tx));
        RadioModel rm;
        connectRadio(rm);
        TxProfileSetupPage page(&rm, &mgr, &tx);
        int asked = 0;
        page.setUnsavedPromptHook([&asked](const QString&) {
            ++asked;
            return TxProfileSetupPage::UnsavedPromptResult::Yes;
        });
        tx.setMicGainDb(-4);
        QApplication::processEvents();

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("Default.nereus-tx-profile"));
        page.setExportPathHook([path] { return path; });
        bool ok = false;
        page.setFileResultHook([&ok](bool success, const QString&) { ok = success; });
        page.exportButton()->click();
        QCOMPARE(asked, 1);
        QVERIFY(ok);
        QCOMPARE(storedValue(QStringLiteral("Default"), QStringLiteral("MicGain")),
                 QStringLiteral("-4"));
        QCOMPARE(readProfileFile(path).keys.value(QStringLiteral("MicGain")), QStringLiteral("-4"));
    }

    // A profile named before the name rules cannot be saved again under
    // that name: export's Yes says why, writes nothing and keeps the edits.
    void exportYesOnAnOldNameSaysWhyAndKeepsTheEdits()
    {
        const QString oldName(65, QLatin1Char('x'));
        auto& settings = AppSettings::instance();
        const QString manifest = QStringLiteral("hardware/%1/tx/profile/_names").arg(kMacA);
        {
            MicProfileManager seed;
            seed.setMacAddress(kMacA);
            seed.load();
        }
        settings.setValue(manifest, settings.value(manifest).toString()
                                        + QLatin1Char(',') + oldName);
        settings.setValue(QStringLiteral("hardware/%1/tx/profile/%2/MicGain").arg(kMacA, oldName),
                          QStringLiteral("-6"));
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.profileNames().contains(oldName));
        QVERIFY(mgr.setActiveProfile(oldName, &tx));
        RadioModel rm;
        connectRadio(rm);
        TxProfileSetupPage page(&rm, &mgr, &tx);
        int asked = 0;
        page.setUnsavedPromptHook([&asked](const QString&) {
            ++asked;
            return TxProfileSetupPage::UnsavedPromptResult::Yes;
        });
        tx.setMicGainDb(-4);
        QApplication::processEvents();
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("Old.nereus-tx-profile"));
        int pathAsked = 0;
        page.setExportPathHook([&pathAsked, path] {
            ++pathAsked;
            return path;
        });
        bool ok = true;
        QString shown;
        page.setFileResultHook([&](bool success, const QString& text) {
            ok = success;
            shown = text;
        });
        page.exportButton()->click();
        QCOMPARE(asked, 1);
        QVERIFY(!ok);
        QCOMPARE(shown, QStringLiteral("Use a transmit profile name of 64 characters or fewer."));
        QCOMPARE(pathAsked, 0);
        QVERIFY(!QFile::exists(path));
        QCOMPARE(storedValue(oldName, QStringLiteral("MicGain")), QStringLiteral("-6"));
        // Still unsaved: the next export asks again.
        page.exportButton()->click();
        QCOMPARE(asked, 2);
    }

    // Export's Yes saves only once the file is chosen: a cancelled file
    // saves nothing and keeps the edits unsaved.
    void exportCancelOnTheFileKeepsTheEditsUnsaved()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.setActiveProfile(QStringLiteral("Default"), &tx));
        const QString before = storedValue(QStringLiteral("Default"), QStringLiteral("MicGain"));
        RadioModel rm;
        connectRadio(rm);
        TxProfileSetupPage page(&rm, &mgr, &tx);
        int asked = 0;
        page.setUnsavedPromptHook([&asked](const QString&) {
            ++asked;
            return TxProfileSetupPage::UnsavedPromptResult::Yes;
        });
        tx.setMicGainDb(-4);
        QApplication::processEvents();
        page.setExportPathHook([] { return QString(); });
        int results = 0;
        page.setFileResultHook([&results](bool, const QString&) { ++results; });
        page.exportButton()->click();
        QCOMPARE(asked, 1);
        QCOMPARE(results, 0);
        QCOMPARE(storedValue(QStringLiteral("Default"), QStringLiteral("MicGain")), before);
        page.exportButton()->click();
        QCOMPARE(asked, 2);
    }

    // The file carries no wiring key.
    void exportLeavesOutTheRadiosWiring()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        RadioModel rm;
        connectRadio(rm);
        TxProfileSetupPage page(&rm, &mgr, &tx);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("Default.nereus-tx-profile"));
        page.setExportPathHook([path] { return path; });
        page.setFileResultHook([](bool, const QString&) {});
        page.exportButton()->click();
        const TxProfileFile::Document document = readProfileFile(path);
        QVERIFY(document.keys.contains(QStringLiteral("MicGain")));
        for (const QString& key : MicProfileManager::radioWiringKeys()) {
            QVERIFY2(!document.keys.contains(key), qPrintable(key));
        }
    }

    // Cancel on the unsaved question imports nothing and keeps the edits.
    void importCancelOnTheUnsavedQuestionImportsNothing()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.setActiveProfile(QStringLiteral("Default"), &tx));
        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);
        int asked = 0;
        page.setUnsavedPromptHook([&asked](const QString&) {
            ++asked;
            return TxProfileSetupPage::UnsavedPromptResult::Cancel;
        });
        tx.setMicGainDb(-4);
        QApplication::processEvents();
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = writeProfileFile(dir, QStringLiteral("Brought In"),
                                              {{QStringLiteral("MicGain"), QStringLiteral("-21")}});
        page.setImportPathHook([path] { return path; });
        bool shown = false;
        page.setFileResultHook([&shown](bool, const QString&) { shown = true; });
        page.importButton()->click();
        QCOMPARE(asked, 1);
        QVERIFY(!shown);
        QVERIFY(!mgr.profileNames().contains(QStringLiteral("Brought In")));
        QCOMPARE(mgr.activeProfileName(), QStringLiteral("Default"));
        QCOMPARE(tx.micGainDb(), -4);
    }

    // Importing over the active profile, under its own name, replaces it
    // and applies the file's values.
    void importOverTheActiveProfileReplacesIt()
    {
        TransmitModel tx;
        tx.setMicGainDb(-7);
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        QVERIFY(mgr.saveProfile(QStringLiteral("Desk"), &tx));
        QVERIFY(mgr.setActiveProfile(QStringLiteral("Desk"), &tx));
        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = writeProfileFile(dir, QStringLiteral("Desk"),
                                              {{QStringLiteral("MicGain"), QStringLiteral("-21")}});
        page.setImportPathHook([path] { return path; });
        int confirmed = 0;
        page.setOverwriteConfirmHook([&confirmed] { ++confirmed; return true; });
        QString shown;
        page.setFileResultHook([&shown](bool success, const QString& text) {
            QVERIFY(success);
            shown = text;
        });
        const int count = static_cast<int>(mgr.profileNames().size());
        page.importButton()->click();
        QCOMPARE(confirmed, 1);
        QCOMPARE(static_cast<int>(mgr.profileNames().size()), count);
        QCOMPARE(mgr.activeProfileName(), QStringLiteral("Desk"));
        QCOMPARE(tx.micGainDb(), -21);
        QCOMPARE(storedValue(QStringLiteral("Desk"), QStringLiteral("MicGain")), QStringLiteral("-21"));
        QCOMPARE(page.profileCombo()->currentText(), QStringLiteral("Desk"));
        QCOMPARE(shown, QStringLiteral("Imported \"Desk\" and selected it."));
    }

    // A file this build cannot use says why, before any question.
    void importRefusesABadFileWithItsReason()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);
        page.setOverwriteConfirmHook([] {
            [] { QFAIL("asked to overwrite"); }();
            return false;
        });
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = writeProfileFile(dir, QStringLiteral("Bad"),
                                        {{QStringLiteral("MicGain"), QStringLiteral("loud")}});
        page.setImportPathHook([&path] { return path; });
        bool ok = true;
        QString shown;
        page.setFileResultHook([&](bool success, const QString& text) {
            ok = success;
            shown = text;
        });
        page.importButton()->click();
        QVERIFY(!ok);
        QCOMPARE(shown, QStringLiteral("Choose a mic level from -50 to 70 dB."));
        QVERIFY(!mgr.profileNames().contains(QStringLiteral("Bad")));

        path = writeProfileFile(dir, QStringLiteral("Tab\tName"),
                                {{QStringLiteral("MicGain"), QStringLiteral("-9")}});
        page.importButton()->click();
        QVERIFY(!ok);
        QCOMPARE(shown, QStringLiteral(
            "Use a transmit profile name without tabs, line breaks or other control characters."));
    }

    // Keys from a newer build are left out, and the confirmation says so.
    void importNotesKeysFromANewerBuild()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = writeProfileFile(dir, QStringLiteral("Newer"),
            {{QStringLiteral("MicGain"), QStringLiteral("-9")},
             {QStringLiteral("FutureKnob"), QStringLiteral("3")},
             {QStringLiteral("FutureSwitch"), QStringLiteral("True")}});
        page.setImportPathHook([path] { return path; });
        QString shown;
        page.setFileResultHook([&shown](bool, const QString& text) { shown = text; });
        page.importButton()->click();
        QCOMPARE(shown, QStringLiteral("Imported \"Newer\" and selected it.\n\n"
                                       "2 settings this version of NereusSDR does not know "
                                       "were left out."));
        QVERIFY(storedValue(QStringLiteral("Newer"), QStringLiteral("FutureKnob")).isEmpty());
    }

    // Save refuses a name with a control character, and says why.
    void saveRefusesABadNameWithItsReason()
    {
        TransmitModel tx;
        MicProfileManager mgr;
        mgr.setMacAddress(kMacA);
        mgr.load();
        RadioModel rm;
        TxProfileSetupPage page(&rm, &mgr, &tx);
        page.setSavePromptHook([](const QString&) {
            return TxProfileSetupPage::SavePromptResult{true, QStringLiteral("Desk\tNight")};
        });
        QString shown;
        page.setRejectionMessageHook([&shown](const QString& text) { shown = text; });
        const QStringList before = mgr.profileNames();
        page.saveButton()->click();
        QCOMPARE(shown, QStringLiteral(
            "Use a transmit profile name without tabs, line breaks or other control characters."));
        QCOMPARE(mgr.profileNames(), before);
    }

    void suggestedFileNameAvoidsWhatWindowsForbids()
    {
        QCOMPARE(TxProfileSetupPage::suggestedExportFileName(
                     QStringLiteral("a*b?c\"d<e>f|g/h\\i:j") + QChar(0x01) + QStringLiteral("k")),
                 QStringLiteral("a_b_c_d_e_f_g_h_i_j_k.nereus-tx-profile"));
        QCOMPARE(TxProfileSetupPage::suggestedExportFileName(QStringLiteral("D-104+CPDR")),
                 QStringLiteral("D-104+CPDR.nereus-tx-profile"));
        // A trailing dot or space is dropped.
        QCOMPARE(TxProfileSetupPage::suggestedExportFileName(QStringLiteral("Desk. .")),
                 QStringLiteral("Desk.nereus-tx-profile"));
        QCOMPARE(TxProfileSetupPage::suggestedExportFileName(QStringLiteral("...")),
                 QStringLiteral("_.nereus-tx-profile"));
        // A device name, in any case, with or without an extension.
        const struct {
            const char* name;
            const char* file;
        } reserved[] = {
            {"CON", "_CON.nereus-tx-profile"},
            {"prn", "_prn.nereus-tx-profile"},
            {"Aux.old", "_Aux.old.nereus-tx-profile"},
            {"nul ", "_nul.nereus-tx-profile"},
            {"NUL .txt", "_NUL .txt.nereus-tx-profile"},
            {"com1", "_com1.nereus-tx-profile"},
            {"COM9", "_COM9.nereus-tx-profile"},
            {"lpt1.x", "_lpt1.x.nereus-tx-profile"},
            {"LPT9", "_LPT9.nereus-tx-profile"},
        };
        for (const auto& r : reserved) {
            QCOMPARE(TxProfileSetupPage::suggestedExportFileName(QString::fromLatin1(r.name)),
                     QString::fromLatin1(r.file));
        }
        // Near misses stay as they are.
        for (const char* name : {"COM0", "COM10", "LPT", "CONSOLE", "Nul2", "CON-1"}) {
            QCOMPARE(TxProfileSetupPage::suggestedExportFileName(QString::fromLatin1(name)),
                     QString::fromLatin1(name) + QStringLiteral(".nereus-tx-profile"));
        }
    }

    // The Diagnostics page's button switches Setup to the TX Profile page.
    void diagnosticsButtonSwitchesSetupToTheTxProfilePage()
    {
        RadioModel rm;
        SetupDialog dialog(&rm);
        dialog.selectPage(QStringLiteral("Export / Import"));
        auto* tree = dialog.findChild<QTreeWidget*>();
        QVERIFY(tree != nullptr);
        QVERIFY(tree->currentItem() != nullptr);
        QCOMPARE(tree->currentItem()->text(0), QStringLiteral("Export / Import"));
        auto* button = dialog.findChild<QPushButton*>(QStringLiteral("openTxProfileButton"));
        QVERIFY(button != nullptr);
        button->click();
        QCOMPARE(tree->currentItem()->text(0), QStringLiteral("TX Profile"));
        auto* stack = dialog.findChild<QStackedWidget*>();
        QVERIFY(stack != nullptr);
        QVERIFY(stack->currentWidget() != nullptr);
        QVERIFY(stack->currentWidget()->findChild<QPushButton*>(
                    QStringLiteral("importTxProfileButton")) != nullptr
                || stack->currentWidget()->objectName() == QStringLiteral("importTxProfileButton"));
    }

    void settingsPageOpensTheTxProfilePage()
    {
        ExportImportConfigPage page;
        QSignalSpy opened(&page, &ExportImportConfigPage::txProfilePageRequested);
        auto* button = page.findChild<QPushButton*>(QStringLiteral("openTxProfileButton"));
        QVERIFY(button != nullptr);
        button->click();
        QCOMPARE(opened.count(), 1);
    }
};

QTEST_MAIN(TestTxProfileSetupPage)
#include "tst_tx_profile_setup_page.moc"
