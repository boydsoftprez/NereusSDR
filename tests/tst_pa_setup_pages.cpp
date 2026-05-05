// tests/tst_pa_setup_pages.cpp  (NereusSDR)
//
// Setup IA reshape Phase 2 — top-level "PA" category pages.
// no-port-check: test fixture — no Thetis attribution required.
//
// Verifies the three placeholder pages added under the new top-level
// PA category construct cleanly with their expected page titles and
// expose a placeholder QLabel hinting at the future-phase tag.
//
// Source attribution (lives in production headers, not the test):
//   Thetis tpPowerAmplifier   setup.designer.cs:47366-47371 [v2.10.3.13+501e3f51]
//   Thetis tpGainByBand       setup.designer.cs:47386-47525 [v2.10.3.13+501e3f51]
//   Thetis tpWattMeter        setup.designer.cs:49304-49309 [v2.10.3.13+501e3f51]
//   Thetis panelPAValues      setup.designer.cs:51155-51177 [v2.10.3.13+501e3f51]

#include <QtTest/QtTest>
#include <QLabel>

#include "gui/setup/PaSetupPages.h"

using namespace NereusSDR;

class TstPaSetupPages : public QObject {
    Q_OBJECT

private slots:
    void pa_gain_page_constructs_with_placeholder_label();
    void pa_watt_meter_page_constructs_with_placeholder_label();
    void pa_values_page_constructs_with_placeholder_label();
    void all_three_pages_have_setupPage_title();
};

namespace {

// Helper: walk every QLabel child and return true if any contains `needle`.
// Avoids tying the test to the exact placeholder copy — phrasing can be
// tweaked without breaking these checks as long as the future-phase tag
// remains in the body.
bool labelContains(const QWidget& page, const QString& needle)
{
    const auto labels = page.findChildren<QLabel*>();
    for (const QLabel* lbl : labels) {
        if (lbl->text().contains(needle)) {
            return true;
        }
    }
    return false;
}

} // namespace

void TstPaSetupPages::pa_gain_page_constructs_with_placeholder_label()
{
    // Phase 6 of #167 (2026-05-03): the placeholder body was replaced
    // wholesale with the live PA Gain editor backed by PaProfileManager.
    // When constructed without a model, the page falls back to a brief
    // "requires a connected RadioModel with PaProfileManager" hint label
    // so model-less previews still render coherently.  Live-editor
    // integration coverage lives in tst_pa_gain_by_band_page_editor.
    PaGainByBandPage page(/*model=*/nullptr);
    QVERIFY2(labelContains(page, QStringLiteral("requires a connected RadioModel")),
             "PaGainByBandPage model-less fallback must hint at the model requirement");
}

void TstPaSetupPages::pa_watt_meter_page_constructs_with_placeholder_label()
{
    // Setup IA reshape Phase 3A (2026-05-02): the placeholder body was
    // replaced wholesale with the live PaCalibrationGroup migrated from
    // Hardware → Calibration.  When constructed without a model, the
    // page falls back to a brief "requires a connected radio model"
    // hint label so model-less previews still render coherently.
    // Live-group integration coverage lives in tst_pa_watt_meter_page.
    PaWattMeterPage page(/*model=*/nullptr);
    QVERIFY2(labelContains(page, QStringLiteral("requires a connected radio model")),
             "PaWattMeterPage model-less fallback must hint at the model requirement");
}

void TstPaSetupPages::pa_values_page_constructs_with_placeholder_label()
{
    // Setup IA reshape Phase 4 (2026-05-02): the placeholder body was
    // replaced wholesale with the live MetricLabel grid bound to RadioStatus
    // + RadioConnection signals.  When constructed without a model, the
    // page falls back to a brief hint label so model-less previews still
    // render coherently.  Live-binding integration coverage lives in
    // tst_pa_values_page.
    PaValuesPage page(/*model=*/nullptr);
    QCOMPARE(page.pageTitle(), QStringLiteral("PA Values"));
}

void TstPaSetupPages::all_three_pages_have_setupPage_title()
{
    PaGainByBandPage gainPage(/*model=*/nullptr);
    QCOMPARE(gainPage.pageTitle(), QStringLiteral("PA Gain"));

    PaWattMeterPage wattPage(/*model=*/nullptr);
    QCOMPARE(wattPage.pageTitle(), QStringLiteral("Watt Meter"));

    PaValuesPage valuesPage(/*model=*/nullptr);
    QCOMPARE(valuesPage.pageTitle(), QStringLiteral("PA Values"));
}

QTEST_MAIN(TstPaSetupPages)
#include "tst_pa_setup_pages.moc"
