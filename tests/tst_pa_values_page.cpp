// tests/tst_pa_values_page.cpp  (NereusSDR)
//
// Setup IA reshape Phase 4 — live PA Values page.
// no-port-check: test fixture — no Thetis attribution required.
//
// Verifies the live wiring of MetricLabel children on PaValuesPage and
// the page's response to RadioStatus + RadioConnection signals:
//
//   - powerChanged → FWD calibrated, REV, SWR labels
//   - PA temperature, PA current and supply volts labels from
//     RadioModel::paReadings() (remote-window parity Task 6), unavailable
//     until the connected radio reports each
//   - paTelemetryUpdated → FWD/REV ADC raw labels
//   - adcOverflow → overload label (and clears after 2s)
//   - null model constructs safely (model-less preview path)
//
// Source attribution (lives in production headers, not the test):
//   Thetis panelPAValues  setup.designer.cs:51155-51177 [v2.10.3.13+501e3f51]

#include <QtTest/QtTest>

#include "core/RadioConnection.h"
#include "core/RadioStatus.h"
#include "gui/setup/PaSetupPages.h"
#include "gui/widgets/MetricLabel.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

// ── NullRadioConnection ──────────────────────────────────────────────────────
// Minimal concrete subclass so RadioModel::injectConnectionForTest accepts a
// live connection pointer.  Mirrors the pattern in
// tst_radio_connection_supply_volts.cpp / tst_radio_model_mox_hardware_flip.cpp.
class TestNullConnection : public RadioConnection {
    Q_OBJECT
public:
    TestNullConnection() : RadioConnection() {}
    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
    void setMox(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void sendTxIq(const float*, int) override {}
    void setTrxRelay(bool) override {}
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}
    void setMicXlr(bool) override {}
    void setWatchdogEnabled(bool) override {}

    // Re-publish protected signals so the test can fire them at the page
    // exactly as P1/P2 connections do.  Calling these from outside the class
    // is identical to a real frame-decoder firing the same signals.
    using RadioConnection::supplyVoltsChanged;
    using RadioConnection::paTelemetryUpdated;
    using RadioConnection::adcOverflow;
};

class TstPaValuesPage : public QObject {
    Q_OBJECT

private slots:
    void page_constructs_with_metric_labels();
    void forward_power_label_updates_on_status_signal();
    void reflected_power_and_swr_update_on_powerChanged();
    void pa_current_updates_on_signal();
    void pa_temperature_updates_on_signal();
    void adc_label_updates_on_paTelemetryUpdated();
    void supply_volts_updates_on_signal();
    void adc_overload_label_shows_yes_on_adcOverflow();
    void null_model_constructs_safely();
};

// ---------------------------------------------------------------------------
// Construction-time invariants: at least 8 MetricLabel children are wired.
// ---------------------------------------------------------------------------
void TstPaValuesPage::page_constructs_with_metric_labels()
{
    RadioModel model;
    PaValuesPage page(&model);

    const auto labels = page.findChildren<MetricLabel*>();
    QVERIFY2(labels.size() >= 8,
             qPrintable(QStringLiteral("expected at least 8 MetricLabel children, got %1")
                            .arg(labels.size())));
}

// ---------------------------------------------------------------------------
// powerChanged(fwd, rev, swr) — RadioStatus aggregates fwd/rev into one signal.
// Setting forward power emits powerChanged with the latest fwd value; the page
// must surface that on the FWD calibrated label.
// ---------------------------------------------------------------------------
void TstPaValuesPage::forward_power_label_updates_on_status_signal()
{
    RadioModel model;
    PaValuesPage page(&model);

    model.radioStatus().setForwardPower(42.5);

    QCOMPARE(page.fwdCalibratedTextForTest(), QStringLiteral("42.50 W"));
}

// ---------------------------------------------------------------------------
// powerChanged(fwd, rev, swr) — setting reflected power also recomputes SWR.
// ARRL formula:  rho = sqrt(refl/fwd);  swr = (1 + rho)/(1 - rho)
// fwd=50, refl=5  →  rho = sqrt(0.1) ≈ 0.3162  →  swr ≈ 1.92
// Phase 5B (#167) adds running peak/min to the rev/swr labels — the
// annotation may follow the formatted value once the tracker has seen
// more than one sample, so pin only the leading current-value substring.
// ---------------------------------------------------------------------------
void TstPaValuesPage::reflected_power_and_swr_update_on_powerChanged()
{
    RadioModel model;
    PaValuesPage page(&model);

    model.radioStatus().setForwardPower(50.0);
    model.radioStatus().setReflectedPower(5.0);

    QVERIFY2(page.revPowerTextForTest().startsWith(QStringLiteral("5.00 W")),
             qPrintable(QStringLiteral("expected leading '5.00 W', got '%1'")
                            .arg(page.revPowerTextForTest())));
    // SWR formula yields ~1.92 for fwd=50/refl=5; assert the label starts with "1." so
    // we don't pin a specific decimal that depends on RadioStatus::computeSwr rounding.
    QVERIFY2(page.swrTextForTest().startsWith(QStringLiteral("1.")),
             qPrintable(QStringLiteral("got SWR text '%1', expected leading '1.'")
                            .arg(page.swrTextForTest())));
}

// ---------------------------------------------------------------------------
// PA current label. R-R3-32 (remote-window parity Task 6): the page reads
// RadioModel::paReadings(), where the current is present once a telemetry
// sample from a connected radio with a PA current sensor has reported it;
// before that it reads "Unavailable", never 0.
// ---------------------------------------------------------------------------
void TstPaValuesPage::pa_current_updates_on_signal()
{
    RadioModel model;
    model.setBoardForTest(HPSDRHW::Saturn);
    model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);
    auto* conn = new TestNullConnection();
    PaValuesPage page(&model);
    QCOMPARE(page.paCurrentTextForTest(), QStringLiteral("Unavailable"));
    model.injectConnectionForTest(conn);

    // userAdc1 1181: ((1181 * 5000 / 4095) - 360) / 120 = 9.02 A (Thetis
    // convertToAmps), then RadioStatus holds it.
    model.handlePaTelemetryForTest(0, 0, 0, 0, 1181, 0);
    QCOMPARE(page.paCurrentTextForTest(),
             QString::number(model.radioStatus().paCurrentAmps(), 'f', 2) + QStringLiteral(" A"));
    QVERIFY(model.radioStatus().paCurrentAmps() > 8.9);

    model.injectConnectionForTest(nullptr);
    QCOMPARE(page.paCurrentTextForTest(), QStringLiteral("Unavailable"));
    delete conn;
}

// ---------------------------------------------------------------------------
// PA temperature label: the HL2 reports its PA temperature in the exciter
// power field (mi0bot console.cs); present once reported.
// ---------------------------------------------------------------------------
void TstPaValuesPage::pa_temperature_updates_on_signal()
{
    RadioModel model;
    model.setBoardForTest(HPSDRHW::HermesLite);
    model.setHpsdrModelForTest(HPSDRModel::HERMESLITE);
    auto* conn = new TestNullConnection();
    model.injectConnectionForTest(conn);
    PaValuesPage page(&model);
    QCOMPARE(page.paTempTextForTest(), QStringLiteral("Unavailable"));

    model.handlePaTelemetryForTest(0, 0, /*exciterRaw=*/1200, 0, 0, 0);
    const double celsius = model.radioStatus().paTemperatureCelsius();
    QVERIFY(celsius > 0.0);
    QVERIFY2(page.paTempTextForTest().startsWith(QString::number(celsius, 'f', 1)),
             qPrintable(page.paTempTextForTest()));
    QVERIFY(page.paTempTextForTest().contains(QString::fromUtf8("\xC2\xB0")));

    model.injectConnectionForTest(nullptr);
    delete conn;
}

// ---------------------------------------------------------------------------
// paTelemetryUpdated(fwdRaw, revRaw, ...) → FWD/REV ADC raw labels.
// Inject a TestNullConnection so the page's wiring path picks it up.
// ---------------------------------------------------------------------------
void TstPaValuesPage::adc_label_updates_on_paTelemetryUpdated()
{
    RadioModel model;
    auto* conn = new TestNullConnection();
    model.injectConnectionForTest(conn);

    PaValuesPage page(&model);

    emit conn->paTelemetryUpdated(/*fwdRaw=*/1234, /*revRaw=*/567,
                                  /*exciterRaw=*/0, /*userAdc0=*/0,
                                  /*userAdc1=*/0, /*supply=*/0);

    QCOMPARE(page.fwdAdcTextForTest(), QStringLiteral("1234"));
    QCOMPARE(page.revAdcTextForTest(), QStringLiteral("567"));

    // Detach before destruction — RadioModel destructor doesn't own the
    // injected connection (we're responsible for it).
    model.injectConnectionForTest(nullptr);
    delete conn;
}

// ---------------------------------------------------------------------------
// supplyVoltsChanged → supply voltage label.
// ---------------------------------------------------------------------------
void TstPaValuesPage::supply_volts_updates_on_signal()
{
    RadioModel model;
    auto* conn = new TestNullConnection();
    model.injectConnectionForTest(conn);

    PaValuesPage page(&model);
    QCOMPARE(page.supplyVoltsTextForTest(), QStringLiteral("Unavailable"));

    // R-R3-32 (parity Task 6): the connection's cached supply volts, as the
    // status frame handler leaves them.
    // (RadioModel re-reads it on the connection's supplyVoltsChanged, which
    // the connect path wires; Reset re-reads it here.)
    conn->handleSupplyRaw(2400);
    page.resetPaValues();
    QCOMPARE(page.supplyVoltsTextForTest(),
             QString::number(conn->lastSupplyVolts(), 'f', 1) + QStringLiteral(" V"));

    model.injectConnectionForTest(nullptr);
    delete conn;
}

// ---------------------------------------------------------------------------
// adcOverflow → "Yes (ADC N)" overload label.  We don't spin the auto-clear
// timer here — that path is straightforward and the auto-clear interval
// (2 s) is too long for a tight unit test.  Just pin the "Yes" path.
// ---------------------------------------------------------------------------
void TstPaValuesPage::adc_overload_label_shows_yes_on_adcOverflow()
{
    RadioModel model;
    auto* conn = new TestNullConnection();
    model.injectConnectionForTest(conn);

    PaValuesPage page(&model);

    emit conn->adcOverflow(/*adc=*/0);

    QVERIFY2(page.adcOverloadTextForTest().startsWith(QStringLiteral("Yes")),
             qPrintable(QStringLiteral("expected leading 'Yes', got '%1'")
                            .arg(page.adcOverloadTextForTest())));

    model.injectConnectionForTest(nullptr);
    delete conn;
}

// ---------------------------------------------------------------------------
// Null-model construction must not crash; mirrors the model-less preview
// path used by the Phase 2 placeholder test.
// ---------------------------------------------------------------------------
void TstPaValuesPage::null_model_constructs_safely()
{
    PaValuesPage page(/*model=*/nullptr);
    // No assertion on label count — model-less variant is allowed to render
    // a fallback hint label instead of the live grid.  We only verify it
    // didn't crash and constructed a usable widget with the expected title.
    QCOMPARE(page.pageTitle(), QStringLiteral("PA Values"));
}

QTEST_MAIN(TstPaValuesPage)
#include "tst_pa_values_page.moc"
