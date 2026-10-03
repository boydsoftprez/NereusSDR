// no-port-check: NereusSDR-original unit-test file.  Thetis cite comments
// document upstream sources; no Thetis logic ported in this test file.
// =================================================================
// tests/tst_setup_deltas.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for the Phase 3M-4 Task 11 Setup-page deltas:
//
//   (1) AntennaAlexAlex1Tab "HPF Bypass on PureSignal feedback" toggle
//       gains the IMD warning dialog when un-checked.  Multi-paragraph
//       warning text ported verbatim from Thetis setup.cs:29274-29292
//       [v2.10.3.13].  Cancel default-focused (Button2) reverts the
//       toggle via QSignalBlocker.  Emits new hpfBypassOnPsChanged signal.
//
//   (2) GeneralOptionsPage "Options" group gains "Hide feedback level"
//       checkbox — chkHideFeebackLevel from Thetis setup.designer.cs:10571
//       [v2.10.3.13] (Thetis typo "Feeback" preserved in source-cite,
//       NereusSDR uses corrected spelling).  Drives PureSignal::
//       setHideFeedback when wired by SetupDialog.
//
//   (3) GeneralOptionsPage "Options" group gains "Swap red and blue
//       PS-A feedback colours" checkbox (chkSwapREDBluePSAColours from
//       setup.designer.cs:10572).  Drives PureSignal::setInvertRedBlue.
//
// Both new checkboxes persist via AppSettings.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 — New test file for Phase 3M-4 Task 11: Setup deltas.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation
//                 via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QApplication>
#include <QCheckBox>
#include <QGroupBox>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "gui/setup/GeneralOptionsPage.h"
#include "gui/setup/hardware/AntennaAlexAlex1Tab.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TstSetupDeltas : public QObject {
    Q_OBJECT

private slots:

    void initTestCase()
    {
        if (!qApp) {
            static int   argc = 0;
            static char* argv = nullptr;
            new QApplication(argc, &argv);
        }
    }

    void cleanupTestCase() {}

    // ── (1) AntennaAlexAlex1Tab — HPF Bypass IMD warning ────────────────

    // Cancel reverts the un-check: when the user un-checks
    // m_hpfBypassOnPs and clicks "Cancel" on the IMD dialog, the
    // checkbox snaps back to checked and no hpfBypassOnPsChanged
    // signal is emitted.
    void cancelOnImdWarning_revertsCheckbox()
    {
        RadioModel model;
        AntennaAlexAlex1Tab tab(&model);
        tab.setImdWarningResultForTest(
            AntennaAlexAlex1Tab::TestImdResult::ConfirmCancel);

        QCheckBox* chk = tab.hpfBypassOnPsCheckboxForTest();
        QVERIFY2(chk, "m_hpfBypassOnPs must be present");
        QVERIFY2(chk->isChecked(),
                 "m_hpfBypassOnPs default-checked per Thetis setup.designer.cs:23676");

        QSignalSpy spy(&tab, &AntennaAlexAlex1Tab::hpfBypassOnPsChanged);

        // User un-checks → IMD dialog (auto-Cancel) → revert.
        chk->setChecked(false);

        QVERIFY2(chk->isChecked(),
                 "Cancel must revert the toggle to checked");
        QCOMPARE(spy.count(), 0);
    }

    // OK on the IMD warning lets the un-check land: the checkbox
    // stays unchecked and hpfBypassOnPsChanged(false) is emitted.
    void okOnImdWarning_keepsCheckboxUnchecked()
    {
        RadioModel model;
        AntennaAlexAlex1Tab tab(&model);
        tab.setImdWarningResultForTest(
            AntennaAlexAlex1Tab::TestImdResult::ConfirmOk);

        QCheckBox* chk = tab.hpfBypassOnPsCheckboxForTest();
        QSignalSpy spy(&tab, &AntennaAlexAlex1Tab::hpfBypassOnPsChanged);

        chk->setChecked(false);

        QVERIFY2(!chk->isChecked(),
                 "OK must allow the unchecked state to persist");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), false);
    }

    // Re-checking from unchecked-state does NOT show the dialog.
    // The dialog only fires on the un-check transition (user is
    // about to INCLUDE the BPFs during a PureSignal feedback path).
    // Re-checking restores the safety baseline and is silent.
    void recheckingFromUnchecked_doesNotShowDialog()
    {
        RadioModel model;
        AntennaAlexAlex1Tab tab(&model);
        tab.setImdWarningResultForTest(
            AntennaAlexAlex1Tab::TestImdResult::ConfirmOk);

        QCheckBox* chk = tab.hpfBypassOnPsCheckboxForTest();
        chk->setChecked(false);  // first transition with auto-OK

        // Now flip the auto-result to Cancel — if the dialog fires
        // on re-check it would revert us back to unchecked.
        tab.setImdWarningResultForTest(
            AntennaAlexAlex1Tab::TestImdResult::ConfirmCancel);

        QSignalSpy spy(&tab, &AntennaAlexAlex1Tab::hpfBypassOnPsChanged);
        chk->setChecked(true);

        QVERIFY2(chk->isChecked(),
                 "Re-checking must succeed without showing the warning");
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.takeFirst().at(0).toBool(), true);
    }

    // hpfBypassOnPsChanged emits with the new state.  This is the
    // contract SetupDialog/RadioModel relies on to drive the live
    // PureSignal feedback path.
    void hpfBypassOnPsChanged_emitsWithNewState()
    {
        RadioModel model;
        AntennaAlexAlex1Tab tab(&model);
        tab.setImdWarningResultForTest(
            AntennaAlexAlex1Tab::TestImdResult::ConfirmOk);

        QCheckBox* chk = tab.hpfBypassOnPsCheckboxForTest();
        QSignalSpy spy(&tab, &AntennaAlexAlex1Tab::hpfBypassOnPsChanged);

        chk->setChecked(false);
        chk->setChecked(true);
        chk->setChecked(false);

        QCOMPARE(spy.count(), 3);
        QCOMPARE(spy.takeFirst().at(0).toBool(), false);
        QCOMPARE(spy.takeFirst().at(0).toBool(), true);
        QCOMPARE(spy.takeFirst().at(0).toBool(), false);
    }

    // ── (2) GeneralOptionsPage — "Hide feedback level" checkbox ────────

    void hideFeedbackCheckbox_isPresentInOptionsGroup()
    {
        GeneralOptionsPage page(/*model=*/nullptr);
        auto* group = page.findChild<QGroupBox*>(
            QStringLiteral("grpGeneralOptions"));
        QVERIFY2(group, "grpGeneralOptions must exist");

        auto* chk = group->findChild<QCheckBox*>(
            QStringLiteral("chkHideFeedbackLevel"));
        QVERIFY2(chk, "chkHideFeedbackLevel must exist on the Options group");
        // User-visible text uses corrected spelling — Thetis typo "Feeback"
        // is preserved only in the source-cite comment per Task 11 spec.
        QCOMPARE(chk->text(), QStringLiteral("Hide feedback level"));
        QVERIFY2(!chk->toolTip().isEmpty(),
                 "chkHideFeedbackLevel must carry a tooltip");
    }

    void hideFeedbackCheckbox_persistsToAppSettings()
    {
        GeneralOptionsPage page(/*model=*/nullptr);
        auto* chk = page.findChild<QCheckBox*>(
            QStringLiteral("chkHideFeedbackLevel"));
        QVERIFY(chk);

        chk->setChecked(true);
        QCOMPARE(AppSettings::instance().value(
                     QStringLiteral("HideFeedbackLevel"),
                     QStringLiteral("False")).toString(),
                 QStringLiteral("True"));

        chk->setChecked(false);
        QCOMPARE(AppSettings::instance().value(
                     QStringLiteral("HideFeedbackLevel"),
                     QStringLiteral("False")).toString(),
                 QStringLiteral("False"));
    }

    void hideFeedbackCheckbox_emitsSignalOnToggle()
    {
        GeneralOptionsPage page(/*model=*/nullptr);
        auto* chk = page.findChild<QCheckBox*>(
            QStringLiteral("chkHideFeedbackLevel"));
        QVERIFY(chk);

        QSignalSpy spy(&page, &GeneralOptionsPage::hideFeedbackLevelChanged);
        chk->setChecked(true);
        chk->setChecked(false);

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.takeFirst().at(0).toBool(), true);
        QCOMPARE(spy.takeFirst().at(0).toBool(), false);
    }

    // ── (3) GeneralOptionsPage — "Swap red and blue PS-A feedback colours"

    void swapRedBlueCheckbox_isPresentInOptionsGroup()
    {
        GeneralOptionsPage page(/*model=*/nullptr);
        auto* group = page.findChild<QGroupBox*>(
            QStringLiteral("grpGeneralOptions"));
        QVERIFY(group);

        auto* chk = group->findChild<QCheckBox*>(
            QStringLiteral("chkSwapREDBluePSAColours"));
        QVERIFY2(chk, "chkSwapREDBluePSAColours must exist on the Options group");
        QCOMPARE(chk->text(),
                 QStringLiteral("Swap red and blue PS-A feedback colors"));
        QVERIFY2(!chk->toolTip().isEmpty(),
                 "chkSwapREDBluePSAColours must carry a tooltip");
    }

    void swapRedBlueCheckbox_persistsToAppSettings()
    {
        GeneralOptionsPage page(/*model=*/nullptr);
        auto* chk = page.findChild<QCheckBox*>(
            QStringLiteral("chkSwapREDBluePSAColours"));
        QVERIFY(chk);

        chk->setChecked(true);
        QCOMPARE(AppSettings::instance().value(
                     QStringLiteral("InvertRedBluePsa"),
                     QStringLiteral("False")).toString(),
                 QStringLiteral("True"));

        chk->setChecked(false);
        QCOMPARE(AppSettings::instance().value(
                     QStringLiteral("InvertRedBluePsa"),
                     QStringLiteral("False")).toString(),
                 QStringLiteral("False"));
    }

    void swapRedBlueCheckbox_emitsSignalOnToggle()
    {
        GeneralOptionsPage page(/*model=*/nullptr);
        auto* chk = page.findChild<QCheckBox*>(
            QStringLiteral("chkSwapREDBluePSAColours"));
        QVERIFY(chk);

        QSignalSpy spy(&page, &GeneralOptionsPage::invertRedBluePsaChanged);
        chk->setChecked(true);
        chk->setChecked(false);

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.takeFirst().at(0).toBool(), true);
        QCOMPARE(spy.takeFirst().at(0).toBool(), false);
    }
};

QTEST_MAIN(TstSetupDeltas)
#include "tst_setup_deltas.moc"
