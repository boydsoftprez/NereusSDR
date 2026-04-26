// no-port-check: test file — verifies NereusSDR::safety::paScalingFor() +
// computeAlexFwdPower() output values; the Thetis source reference is a
// citation in the docstring, not a derivation claim. The ported functions
// themselves are registered in THETIS-PROVENANCE.md.
// =================================================================
// tests/tst_pa_scaling.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for NereusSDR::safety::paScalingFor() and
// NereusSDR::safety::computeAlexFwdPower(), which port the per-board
// PA scaling table from Thetis console.cs:25008-25072 [v2.10.3.13].
//
// Expected values derived directly from the ported formula:
//   volts = (adc - adc_cal_offset) / 4095.0 * refVoltage
//   watts = volts^2 / bridgeVolt
// with both floors clamped to 0.

#include <QtTest>
#include "core/safety/safety_constants.h"
#include "core/HpsdrModel.h"

using namespace NereusSDR;
using namespace NereusSDR::safety;

class TestPaScaling : public QObject
{
    Q_OBJECT
private slots:
    void anan100_known_adc_returns_known_watts();
    void ananG2_known_adc_returns_known_watts();
    void orionMkII_known_adc_returns_known_watts();
    void redPitaya_known_adc_returns_known_watts();
    void default_unknown_board_uses_default_triplet();
};

// ANAN100/100B: bridge_volt=0.095, refvoltage=3.3, offset=6
// ADC=4095 → volts = (4095-6)/4095*3.3 = 3.2952 V
//            watts = 3.2952^2 / 0.095 ≈ 114.30 W
void TestPaScaling::anan100_known_adc_returns_known_watts()
{
    const float watts = computeAlexFwdPower(HPSDRModel::ANAN100, 4095);
    QVERIFY2(watts >= 114.0f && watts <= 114.5f,
             qPrintable(QString("ANAN100 watts=%1, expected ~114.3 W").arg(static_cast<double>(watts))));
}

// ANAN_G2: bridge_volt=0.12, refvoltage=5.0, offset=32
// ADC=4095 → volts = (4095-32)/4095*5.0 = 4.9609 V
//            watts = 4.9609^2 / 0.12 ≈ 205.09 W
void TestPaScaling::ananG2_known_adc_returns_known_watts()
{
    const float watts = computeAlexFwdPower(HPSDRModel::ANAN_G2, 4095);
    QVERIFY2(watts >= 204.0f && watts <= 206.0f,
             qPrintable(QString("ANAN_G2 watts=%1, expected ~205.1 W").arg(static_cast<double>(watts))));
}

// ORIONMKII: bridge_volt=0.08, refvoltage=5.0, offset=18
// ADC=4095 → volts = (4095-18)/4095*5.0 = 4.9780 V
//            watts = 4.9780^2 / 0.08 ≈ 309.76 W
void TestPaScaling::orionMkII_known_adc_returns_known_watts()
{
    const float watts = computeAlexFwdPower(HPSDRModel::ORIONMKII, 4095);
    QVERIFY2(watts >= 308.0f && watts <= 311.0f,
             qPrintable(QString("ORIONMKII watts=%1, expected ~309.8 W").arg(static_cast<double>(watts))));
}

// REDPITAYA: same triplet as ANAN_G2 group (bridge_volt=0.12, refvoltage=5.0, offset=32)
// Expected watts same as ANAN_G2 ≈ 205.09 W
void TestPaScaling::redPitaya_known_adc_returns_known_watts()
{
    const float watts = computeAlexFwdPower(HPSDRModel::REDPITAYA, 4095);
    QVERIFY2(watts >= 204.0f && watts <= 206.0f,
             qPrintable(QString("REDPITAYA watts=%1, expected ~205.1 W").arg(static_cast<double>(watts))));
}

// HERMES (no on-board PA): hits the default branch
// Default: bridge_volt=0.09, refvoltage=3.3, offset=6
// ADC=4095 → volts = (4095-6)/4095*3.3 = 3.2952 V
//            watts = 3.2952^2 / 0.09 ≈ 120.65 W
void TestPaScaling::default_unknown_board_uses_default_triplet()
{
    const float watts = computeAlexFwdPower(HPSDRModel::HERMES, 4095);
    QVERIFY2(watts >= 120.0f && watts <= 121.5f,
             qPrintable(QString("HERMES (default) watts=%1, expected ~120.6 W").arg(static_cast<double>(watts))));
}

QTEST_GUILESS_MAIN(TestPaScaling)
#include "tst_pa_scaling.moc"
