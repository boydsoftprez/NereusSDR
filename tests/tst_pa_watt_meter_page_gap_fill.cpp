// tests/tst_pa_watt_meter_page_gap_fill.cpp  (NereusSDR)
//
// Phase 5 Agent 5A of issue #167 — PaWattMeterPage gap fill.
// no-port-check: test fixture — no Thetis attribution required.
//
// Verifies the two missing Thetis controls now wired into PaWattMeterPage:
//   1. chkPAValues  — "Show PA Values page" checkbox; toggles a visibility
//                      hint persisted under AppSettings key
//                      "display/showPaValuesPage" (default "True").
//   2. btnResetPAValues — "Reset PA Values" button; emits
//                      resetPaValuesRequested() so PaValuesPage (Phase 5B)
//                      can clear its peak/min tracking state.
//
// Source attribution (lives in production headers, not the test):
//   Thetis chkPAValues       setup.designer.cs:51155-51177 [v2.10.3.13]
//   Thetis chkPAValues_CheckedChanged  setup.cs:16340-16343 [v2.10.3.13]
//   Thetis btnResetPAValues  setup.designer.cs:51155-51177 [v2.10.3.13]
//   Thetis btnResetPAValues_Click      setup.cs:16346-16357 [v2.10.3.13]

#include <QtTest/QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QPushButton>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "gui/setup/PaSetupPages.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

class TstPaWattMeterPageGapFill : public QObject {
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

    void showPaValuesCheckbox_defaultsToTrue_onFirstRun();
    void showPaValuesCheckbox_togglePersistsToAppSettings();
    void showPaValuesCheckbox_appSettingsValueDrivesDefault();
    void resetPaValuesButton_emitsResetSignalOnClick();
    void resetPaValuesButton_repeatedClicksFireOneSignalEach();
};

// ---------------------------------------------------------------------------
// 1. With a fresh AppSettings (cleared in cleanup()), the page must default
//    "Show PA Values page" to checked. This matches the Thetis startup
//    behaviour where panelPAValues is gated on chkPAValues whose default
//    state in NereusSDR is "always show the readout page" so users don't
//    have to dig through Setup to find their PA telemetry.
// ---------------------------------------------------------------------------
void TstPaWattMeterPageGapFill::showPaValuesCheckbox_defaultsToTrue_onFirstRun()
{
    RadioModel model;
    PaWattMeterPage page(&model);

    QVERIFY2(page.showPaValuesCheckedForTest(),
             "Fresh AppSettings should leave 'Show PA Values page' checked");
}

// ---------------------------------------------------------------------------
// 2. Toggling the checkbox writes the new state to AppSettings under
//    "display/showPaValuesPage" using the True/False string convention
//    (per CLAUDE.md AppSettings policy).
// ---------------------------------------------------------------------------
void TstPaWattMeterPageGapFill::showPaValuesCheckbox_togglePersistsToAppSettings()
{
    RadioModel model;
    PaWattMeterPage page(&model);

    auto* check = page.findChild<QCheckBox*>(QStringLiteral("chkPAValues"));
    QVERIFY2(check != nullptr, "PaWattMeterPage must own a QCheckBox named 'chkPAValues'");

    // Uncheck: AppSettings should now read "False".
    check->setChecked(false);
    QCOMPARE(AppSettings::instance()
                 .value(QStringLiteral("display/showPaValuesPage")).toString(),
             QStringLiteral("False"));

    // Re-check: back to "True".
    check->setChecked(true);
    QCOMPARE(AppSettings::instance()
                 .value(QStringLiteral("display/showPaValuesPage")).toString(),
             QStringLiteral("True"));
}

// ---------------------------------------------------------------------------
// 3. When AppSettings already holds "False", a freshly constructed page
//    must reflect that state in the checkbox (page reads from settings on
//    construct, the default of "True" only applies when the key is absent).
// ---------------------------------------------------------------------------
void TstPaWattMeterPageGapFill::showPaValuesCheckbox_appSettingsValueDrivesDefault()
{
    AppSettings::instance().setValue(QStringLiteral("display/showPaValuesPage"),
                                     QStringLiteral("False"));

    RadioModel model;
    PaWattMeterPage page(&model);

    QVERIFY2(!page.showPaValuesCheckedForTest(),
             "AppSettings 'False' should leave the checkbox unchecked at construct time");
}

// ---------------------------------------------------------------------------
// 4. Clicking the Reset button must emit resetPaValuesRequested() so that
//    PaValuesPage (Phase 5B) can subscribe and clear its peak/min state.
//    Mirrors Thetis btnResetPAValues_Click at setup.cs:16346-16357 which
//    blanks every readout text field on the panel.
// ---------------------------------------------------------------------------
void TstPaWattMeterPageGapFill::resetPaValuesButton_emitsResetSignalOnClick()
{
    RadioModel model;
    PaWattMeterPage page(&model);

    auto* btn = page.findChild<QPushButton*>(QStringLiteral("btnResetPAValues"));
    QVERIFY2(btn != nullptr,
             "PaWattMeterPage must own a QPushButton named 'btnResetPAValues'");

    QSignalSpy spy(&page, &PaWattMeterPage::resetPaValuesRequested);
    QVERIFY(spy.isValid());

    page.clickResetPaValuesForTest();
    QCOMPARE(spy.count(), 1);
}

// ---------------------------------------------------------------------------
// 5. Each click is independent — repeated clicks fire one signal per click.
//    No debounce or single-shot semantics. Matches Thetis (button click is
//    fire-and-forget; user can hammer it as often as they like).
// ---------------------------------------------------------------------------
void TstPaWattMeterPageGapFill::resetPaValuesButton_repeatedClicksFireOneSignalEach()
{
    RadioModel model;
    PaWattMeterPage page(&model);

    QSignalSpy spy(&page, &PaWattMeterPage::resetPaValuesRequested);
    QVERIFY(spy.isValid());

    page.clickResetPaValuesForTest();
    page.clickResetPaValuesForTest();
    page.clickResetPaValuesForTest();

    QCOMPARE(spy.count(), 3);
}

QTEST_MAIN(TstPaWattMeterPageGapFill)
#include "tst_pa_watt_meter_page_gap_fill.moc"
