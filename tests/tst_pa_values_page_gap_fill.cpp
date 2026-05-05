// tests/tst_pa_values_page_gap_fill.cpp  (NereusSDR)
//
// PA calibration safety hotfix — Phase 5 Agent 5B of issue #167.
// no-port-check: test fixture — no Thetis attribution required.
//
// Verifies the four PaValuesPage gap-fill labels, peak/min tracking, and
// the reset slot/button:
//
//   - m_fwdRawLabel      — populated from paTelemetryUpdated via
//                          PaTelemetryScaling::scaleFwdPowerWatts
//                          (Phase 1B public function)
//   - m_driveLabel       — populated from TransmitModel::power() via
//                          powerChanged signal (slider position, in W)
//   - m_fwdVoltageLabel  — populated from paTelemetryUpdated via
//                          PaTelemetryScaling::scaleFwdRevVoltage
//   - m_revVoltageLabel  — same path for the REV side
//   - peak/min tracking  — fwdCalibrated / rev / swr / paCurrent /
//                          paTemp / supplyVolts each track running
//                          peak/min and re-render with annotation
//   - reset              — clears peak/min back to current value, both
//                          via in-page button click and via the
//                          resetPaValues() public slot (Phase 5A wiring)
//
// Source attribution (lives in production headers, not the test):
//   Thetis panelPAValues  setup.designer.cs:51155-51177 [v2.10.3.13+501e3f51]
//   Thetis btnResetPAValues_Click  setup.cs:16346-16357 [v2.10.3.13+501e3f51]

#include <QtTest/QtTest>
#include <QPushButton>
#include <QSignalSpy>

#include "core/HpsdrModel.h"
#include "core/PaTelemetryScaling.h"
#include "core/RadioConnection.h"
#include "core/RadioStatus.h"
#include "gui/setup/PaSetupPages.h"
#include "gui/widgets/MetricLabel.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

// ── NullRadioConnection ──────────────────────────────────────────────────────
// Minimal concrete subclass so RadioModel::injectConnectionForTest accepts a
// live connection pointer.  Mirrors tst_pa_values_page.cpp.
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

    using RadioConnection::supplyVoltsChanged;
    using RadioConnection::paTelemetryUpdated;
    using RadioConnection::adcOverflow;
};

// ── ResetSignaller ───────────────────────────────────────────────────────────
// Stand-in for Phase 5A's PaWattMeterPage::resetPaValuesRequested.  Lets the
// peak/min reset wiring be exercised in isolation: we connect this object's
// `resetPaValuesRequested` signal to PaValuesPage::resetPaValues() the same
// way SetupDialog will at integration time.
class ResetSignaller : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
signals:
    void resetPaValuesRequested();
};

class TstPaValuesPageGapFill : public QObject {
    Q_OBJECT

private slots:
    void raw_fwd_label_populates_from_paTelemetryUpdated();
    void fwd_voltage_label_populates();
    void rev_voltage_label_populates();
    void drive_label_populates_from_power_slider();
    void peak_min_tracking_for_fwd_calibrated();
    void reset_clears_peak_min_to_current();
    void reset_signal_subscription_clears_peak_min();
    void labels_show_default_text_with_no_telemetry();
};

// ---------------------------------------------------------------------------
// Test 1: Raw FWD power label populates from paTelemetryUpdated.
// Verifies the label text matches scaleFwdPowerWatts(model, raw) formatting.
// ---------------------------------------------------------------------------
void TstPaValuesPageGapFill::raw_fwd_label_populates_from_paTelemetryUpdated()
{
    RadioModel model;
    model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);

    auto* conn = new TestNullConnection();
    model.injectConnectionForTest(conn);

    PaValuesPage page(&model);

    // ANAN_G2 triplet: bridgeVolt=0.12, refVoltage=5.0, adcCalOffset=32
    // raw=2048: volts = (2048-32)/4095 * 5.0 = 2.4615
    //          watts = 2.4615^2 / 0.12 = 50.49 W
    constexpr quint16 kRaw = 2048;
    const double expectedWatts = scaleFwdPowerWatts(HPSDRModel::ANAN_G2, kRaw);

    emit conn->paTelemetryUpdated(/*fwdRaw=*/kRaw, /*revRaw=*/0,
                                  /*exciterRaw=*/0, /*userAdc0=*/0,
                                  /*userAdc1=*/0, /*supply=*/0);

    const QString expected = QString::number(expectedWatts, 'f', 2)
                             + QStringLiteral(" W");
    QCOMPARE(page.fwdRawTextForTest(), expected);

    model.injectConnectionForTest(nullptr);
    delete conn;
}

// ---------------------------------------------------------------------------
// Test 2: FWD voltage label populates from paTelemetryUpdated.
// ---------------------------------------------------------------------------
void TstPaValuesPageGapFill::fwd_voltage_label_populates()
{
    RadioModel model;
    model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);

    auto* conn = new TestNullConnection();
    model.injectConnectionForTest(conn);

    PaValuesPage page(&model);

    constexpr quint16 kRaw = 1024;
    const double expectedVolts = scaleFwdRevVoltage(HPSDRModel::ANAN_G2, kRaw);

    emit conn->paTelemetryUpdated(/*fwdRaw=*/kRaw, /*revRaw=*/0,
                                  /*exciterRaw=*/0, /*userAdc0=*/0,
                                  /*userAdc1=*/0, /*supply=*/0);

    const QString expected = QString::number(expectedVolts, 'f', 2)
                             + QStringLiteral(" V");
    QCOMPARE(page.fwdVoltageTextForTest(), expected);

    model.injectConnectionForTest(nullptr);
    delete conn;
}

// ---------------------------------------------------------------------------
// Test 3: REV voltage label populates from paTelemetryUpdated.
// Per Phase 1B's combined-API design, REV uses the same FWD-side curve.
// ---------------------------------------------------------------------------
void TstPaValuesPageGapFill::rev_voltage_label_populates()
{
    RadioModel model;
    model.setHpsdrModelForTest(HPSDRModel::ANAN_G2);

    auto* conn = new TestNullConnection();
    model.injectConnectionForTest(conn);

    PaValuesPage page(&model);

    constexpr quint16 kRaw = 256;
    const double expectedVolts = scaleFwdRevVoltage(HPSDRModel::ANAN_G2, kRaw);

    emit conn->paTelemetryUpdated(/*fwdRaw=*/0, /*revRaw=*/kRaw,
                                  /*exciterRaw=*/0, /*userAdc0=*/0,
                                  /*userAdc1=*/0, /*supply=*/0);

    const QString expected = QString::number(expectedVolts, 'f', 2)
                             + QStringLiteral(" V");
    QCOMPARE(page.revVoltageTextForTest(), expected);

    model.injectConnectionForTest(nullptr);
    delete conn;
}

// ---------------------------------------------------------------------------
// Test 4: Drive label populates from TransmitModel::powerChanged.
// Setting power() to 50 should surface "50 W" on the drive label.
// ---------------------------------------------------------------------------
void TstPaValuesPageGapFill::drive_label_populates_from_power_slider()
{
    RadioModel model;
    PaValuesPage page(&model);

    model.transmitModel().setPower(50);

    QCOMPARE(page.driveTextForTest(), QStringLiteral("50 W"));
}

// ---------------------------------------------------------------------------
// Test 5: Peak/min tracking on FWD calibrated label.
// Fire 3 powerChanged signals (10W, 50W, 30W).  Verify FWD calibrated
// label shows current=30 with peak=50 / min=10 in the annotation.
// ---------------------------------------------------------------------------
void TstPaValuesPageGapFill::peak_min_tracking_for_fwd_calibrated()
{
    RadioModel model;
    PaValuesPage page(&model);

    model.radioStatus().setForwardPower(10.0);
    model.radioStatus().setForwardPower(50.0);
    model.radioStatus().setForwardPower(30.0);

    QCOMPARE(page.fwdCalibratedPeakForTest(), QStringLiteral("50.00 W"));
    QCOMPARE(page.fwdCalibratedMinForTest(),  QStringLiteral("10.00 W"));

    // The full label should also include the annotation.  Pin the prefix
    // so we don't break on whitespace tweaks; both peak and min text must
    // appear in the rendered string.
    const QString full = page.fwdCalibratedTextForTest();
    QVERIFY2(full.startsWith(QStringLiteral("30.00 W")),
             qPrintable(QStringLiteral("expected leading '30.00 W', got '%1'")
                            .arg(full)));
    QVERIFY2(full.contains(QStringLiteral("50.00")),
             qPrintable(QStringLiteral("missing peak '50.00' in '%1'").arg(full)));
    QVERIFY2(full.contains(QStringLiteral("10.00")),
             qPrintable(QStringLiteral("missing min '10.00' in '%1'").arg(full)));
}

// ---------------------------------------------------------------------------
// Test 6: Reset (in-page button) clears peak/min back to current.
// After three powerChanged signals, click the reset button — peak == min ==
// current value.
// ---------------------------------------------------------------------------
void TstPaValuesPageGapFill::reset_clears_peak_min_to_current()
{
    RadioModel model;
    PaValuesPage page(&model);

    model.radioStatus().setForwardPower(10.0);
    model.radioStatus().setForwardPower(50.0);
    model.radioStatus().setForwardPower(30.0);

    // Sanity: peak/min diverged from current.
    QCOMPARE(page.fwdCalibratedPeakForTest(), QStringLiteral("50.00 W"));
    QCOMPARE(page.fwdCalibratedMinForTest(),  QStringLiteral("10.00 W"));

    // Click reset via the test seam (mirrors a real button click).
    page.clickResetForTest();

    // Now peak == min == current = 30 W.
    QCOMPARE(page.fwdCalibratedPeakForTest(), QStringLiteral("30.00 W"));
    QCOMPARE(page.fwdCalibratedMinForTest(),  QStringLiteral("30.00 W"));
}

// ---------------------------------------------------------------------------
// Test 7: Reset signal subscription — connects an external signaller
// (stand-in for Phase 5A PaWattMeterPage::resetPaValuesRequested) to
// PaValuesPage::resetPaValues() and verifies the slot fires.
// ---------------------------------------------------------------------------
void TstPaValuesPageGapFill::reset_signal_subscription_clears_peak_min()
{
    RadioModel model;
    PaValuesPage page(&model);
    ResetSignaller signaller;

    QObject::connect(&signaller, &ResetSignaller::resetPaValuesRequested,
                     &page, &PaValuesPage::resetPaValues);

    model.radioStatus().setForwardPower(10.0);
    model.radioStatus().setForwardPower(50.0);
    model.radioStatus().setForwardPower(30.0);

    QCOMPARE(page.fwdCalibratedPeakForTest(), QStringLiteral("50.00 W"));

    emit signaller.resetPaValuesRequested();

    QCOMPARE(page.fwdCalibratedPeakForTest(), QStringLiteral("30.00 W"));
    QCOMPARE(page.fwdCalibratedMinForTest(),  QStringLiteral("30.00 W"));
}

// ---------------------------------------------------------------------------
// Test 8: With no telemetry signal fired, labels show their default text.
// The four new labels (raw FWD, drive, FWD voltage, REV voltage) start at
// "0.00 W" / "0 W" / "0.00 V" defaults — must not crash, must not be empty.
// ---------------------------------------------------------------------------
void TstPaValuesPageGapFill::labels_show_default_text_with_no_telemetry()
{
    RadioModel model;
    PaValuesPage page(&model);

    QVERIFY(!page.fwdRawTextForTest().isEmpty());
    QVERIFY(!page.driveTextForTest().isEmpty());
    QVERIFY(!page.fwdVoltageTextForTest().isEmpty());
    QVERIFY(!page.revVoltageTextForTest().isEmpty());

    QCOMPARE(page.fwdRawTextForTest(),     QStringLiteral("0.00 W"));
    QCOMPARE(page.fwdVoltageTextForTest(), QStringLiteral("0.00 V"));
    QCOMPARE(page.revVoltageTextForTest(), QStringLiteral("0.00 V"));
}

QTEST_MAIN(TstPaValuesPageGapFill)
#include "tst_pa_values_page_gap_fill.moc"
