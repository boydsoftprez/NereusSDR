// tests/tst_setup_dialog_pa_reset_wiring.cpp  (NereusSDR)
//
// Phase 9 of issue #167 — Final integration: cross-page wiring between
// PaWattMeterPage::resetPaValuesRequested (Phase 5A) and
// PaValuesPage::resetPaValues() (Phase 5B). The connect() lives in
// SetupDialog::buildTree() and was deferred from Phase 5 so that
// Agents 5A and 5B could land in parallel without touching the same
// SetupDialog source.
//
// no-port-check: test fixture exercising the NereusSDR-original
// SetupDialog cross-wire. Cite to Thetis btnResetPAValues_Click
// (setup.cs:16346-16357 [v2.10.3.13]) is documentary only — the
// production connect() carries the inline citation.
//
// Test strategy:
//   1. Construct SetupDialog with a RadioModel.
//   2. Realize the WattMeter and Values leaves, then pull the pages via
//      NEREUS_BUILD_TESTS seams.
//   3. Drive RadioStatus::powerChanged so the Values page's running
//      peak/min trackers diverge from current.
//   4. Click the WattMeter Reset button via the Phase 5A test seam.
//   5. Verify the Values page's peak/min trackers collapse to current —
//      that's only possible if SetupDialog actually wired the cross-page
//      connect() correctly.
//
// Issues #272 + #301 (2026-07-27): SetupDialog now builds pages lazily, so the
// two pages no longer exist at construction time. Both cases realize their
// leaves explicitly via realizePageForTest() before pulling the pointers. The
// property under test is unchanged: the cross-page reset fan-out fires.
// Coverage for the reset when PA Values has *not* been visited yet lives in
// tst_setup_dialog_lazy_pages.cpp.

#include <QtTest/QtTest>
#include <QApplication>

#include "core/AppSettings.h"
#include "gui/SetupDialog.h"
#include "gui/setup/PaSetupPages.h"
#include "models/RadioModel.h"
#include "core/RadioStatus.h"

using namespace NereusSDR;

class TstSetupDialogPaResetWiring : public QObject {
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

    // Core acceptance: clicking PaWattMeterPage's Reset PA Values button
    // collapses PaValuesPage's running peak/min trackers to the current
    // reading. Only passes if SetupDialog::buildTree() actually wires
    // the resetPaValuesRequested -> resetPaValues() cross-page connect().
    void wattMeter_reset_button_clears_values_page_peak_min();

    // Sanity: realizing both PA leaves yields non-null pointers for
    // the WattMeter and Values pages. If either is null, the
    // Phase 9 connect() is a no-op and downstream tests would silently
    // pass for the wrong reason.
    void both_pa_pages_are_constructed();
};

// ---------------------------------------------------------------------------
// Test 1: end-to-end cross-page wiring drives peak/min reset.
// ---------------------------------------------------------------------------
void TstSetupDialogPaResetWiring::wattMeter_reset_button_clears_values_page_peak_min()
{
    RadioModel model;
    SetupDialog dialog(&model);

    // #272 / #301: pages are built on first visit, so realize both PA leaves
    // before reaching for their pointers.
    QVERIFY(dialog.realizePageForTest(QStringLiteral("Watt Meter")) != nullptr);
    QVERIFY(dialog.realizePageForTest(QStringLiteral("PA Values"))  != nullptr);

    PaWattMeterPage* wattMeter = dialog.paWattMeterPageForTest();
    PaValuesPage*    values    = dialog.paValuesPageForTest();
    QVERIFY2(wattMeter != nullptr, "SetupDialog must construct PaWattMeterPage");
    QVERIFY2(values    != nullptr, "SetupDialog must construct PaValuesPage");

    // Drive 3 forward-power readings so peak/min diverge from current.
    model.radioStatus().setForwardPower(10.0);
    model.radioStatus().setForwardPower(50.0);
    model.radioStatus().setForwardPower(30.0);

    // Sanity: peak/min capture the extrema, not the most recent reading.
    QCOMPARE(values->fwdCalibratedPeakForTest(), QStringLiteral("50.00 W"));
    QCOMPARE(values->fwdCalibratedMinForTest(),  QStringLiteral("10.00 W"));

    // Click the WattMeter page's Reset button via the Phase 5A test seam.
    // This is the cross-page event the Phase 9 connect() routes through.
    wattMeter->clickResetPaValuesForTest();

    // After the cross-wire fires, the Values page's trackers must collapse
    // to the current reading.
    QCOMPARE(values->fwdCalibratedPeakForTest(), QStringLiteral("30.00 W"));
    QCOMPARE(values->fwdCalibratedMinForTest(),  QStringLiteral("30.00 W"));
}

// ---------------------------------------------------------------------------
// Test 2: both pages exist once their leaves are realized.
//
// #272 / #301: pre-refactor this held straight after construction. Pages are
// now built on first visit, so realize both leaves and then assert the
// pointers land. tst_setup_dialog_lazy_pages.cpp asserts the complementary
// property: that they are null before the visit.
// ---------------------------------------------------------------------------
void TstSetupDialogPaResetWiring::both_pa_pages_are_constructed()
{
    RadioModel model;
    SetupDialog dialog(&model);

    QVERIFY(dialog.realizePageForTest(QStringLiteral("Watt Meter")) != nullptr);
    QVERIFY(dialog.realizePageForTest(QStringLiteral("PA Values"))  != nullptr);

    QVERIFY(dialog.paWattMeterPageForTest() != nullptr);
    QVERIFY(dialog.paValuesPageForTest()    != nullptr);
}

QTEST_MAIN(TstSetupDialogPaResetWiring)
#include "tst_setup_dialog_pa_reset_wiring.moc"
