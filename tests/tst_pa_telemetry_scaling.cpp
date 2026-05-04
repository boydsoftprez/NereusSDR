// no-port-check: test file — verifies NereusSDR::scaleFwdPowerWatts() and
// NereusSDR::scaleFwdRevVoltage() output values; the Thetis source reference
// is a citation in the docstring, not a derivation claim. The ported
// functions themselves are registered in THETIS-PROVENANCE.md.
// =================================================================
// tests/tst_pa_telemetry_scaling.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for NereusSDR::scaleFwdPowerWatts() and
// NereusSDR::scaleFwdRevVoltage(), which lift the per-board PA telemetry
// scaling helpers from RadioModel.cpp into a public free-function API
// (Phase 1 Agent 1B of issue #167 PA calibration safety hotfix).
//
// scaleFwdPowerWatts ports computeAlexFwdPower (console.cs:25008-25072
// [v2.10.3.13]). scaleFwdRevVoltage ports the `volts` value computed
// inside computeAlexFwdPower / computeAlexRevPower (the same value Thetis
// surfaces via SetupForm.textFwdVoltage / textRevVoltage labels).
//
// Expected values derived directly from the ported formula:
//   volts = (adc - adc_cal_offset) / 4095.0 * refvoltage    (clamped >= 0)
//   watts = volts^2 / bridge_volt                           (clamped >= 0)

#include <QtTest>

#include "core/HpsdrModel.h"
#include "core/PaTelemetryScaling.h"

using namespace NereusSDR;

class TestPaTelemetryScaling : public QObject
{
    Q_OBJECT
private slots:
    // ─── scaleFwdPowerWatts (port of computeAlexFwdPower) ────────────────
    void anan8000d_fwd_watts_at_three_raw_values();
    void hl2_fwd_watts_uses_mi0bot_hl2_triplet();
    void unknown_model_fwd_watts_uses_default_branch();
    void zero_raw_returns_zero_watts();

    // ─── scaleFwdRevVoltage (FWD-side per-board curve) ───────────────────
    void hermes_fwd_rev_voltage_two_raw_values();
    void orionMkII_fwd_rev_voltage_two_raw_values();
    void ananG2_fwd_rev_voltage_two_raw_values();
    void hl2_fwd_rev_voltage_two_raw_values();
};

// ─── scaleFwdPowerWatts ─────────────────────────────────────────────────
//
// ANAN8000D: bridge_volt=0.08, refvoltage=5.0, adc_cal_offset=18
// Computed from existing private helper in RadioModel.cpp before lift —
// these triplets pin the byte-for-byte parity of the lift.
//
//   raw=2048 -> volts=(2048-18)/4095*5.0 = 2.4786325...
//               watts = 2.4786325^2 / 0.08      ≈ 76.794 W
//   raw=4095 -> volts=(4095-18)/4095*5.0 = 4.9779...
//               watts = 4.9779^2 / 0.08         ≈ 309.755 W
//   raw=18   -> volts=0 -> watts=0
void TestPaTelemetryScaling::anan8000d_fwd_watts_at_three_raw_values()
{
    const double w_2048 = scaleFwdPowerWatts(HPSDRModel::ANAN8000D, 2048);
    QVERIFY2(w_2048 > 76.5 && w_2048 < 77.0,
             qPrintable(QString("ANAN8000D raw=2048 watts=%1, expected ~76.8").arg(w_2048)));

    const double w_4095 = scaleFwdPowerWatts(HPSDRModel::ANAN8000D, 4095);
    QVERIFY2(w_4095 > 309.0 && w_4095 < 310.5,
             qPrintable(QString("ANAN8000D raw=4095 watts=%1, expected ~309.8").arg(w_4095)));

    const double w_18 = scaleFwdPowerWatts(HPSDRModel::ANAN8000D, 18);
    QCOMPARE(w_18, 0.0);
}

// HL2 has its own HERMESLITE entry per mi0bot console.cs:25269-25273
// [v2.10.3.13-beta2]:  bridge_volt=1.5, refvoltage=3.3, offset=6.
// Bench-reported #167 follow-up: previously HL2 fell through to the
// default {0.09, 3.3, 6} branch which is meant for the ANAN-100 coupler
// and over-read HL2 power by ~16.7×.  HL2 is a 5 W QRP radio.
//   raw=2048 -> volts=(2048-6)/4095*3.3 = 1.6452
//               watts = 1.6452² / 1.5      ≈ 1.805 W
//   raw=4095 -> volts ≈ 3.2952 V; watts = 3.2952² / 1.5 ≈ 7.24 W
//   raw=512  -> volts=(512-6)/4095*3.3 = 0.4078;
//               watts = 0.4078² / 1.5      ≈ 0.111 W
void TestPaTelemetryScaling::hl2_fwd_watts_uses_mi0bot_hl2_triplet()
{
    const double w_2048 = scaleFwdPowerWatts(HPSDRModel::HERMESLITE, 2048);
    QVERIFY2(w_2048 > 1.79 && w_2048 < 1.82,
             qPrintable(QString("HL2 raw=2048 watts=%1, expected ~1.805").arg(w_2048)));

    const double w_4095 = scaleFwdPowerWatts(HPSDRModel::HERMESLITE, 4095);
    QVERIFY2(w_4095 > 7.20 && w_4095 < 7.28,
             qPrintable(QString("HL2 raw=4095 watts=%1, expected ~7.24").arg(w_4095)));

    const double w_512 = scaleFwdPowerWatts(HPSDRModel::HERMESLITE, 512);
    QVERIFY2(w_512 > 0.105 && w_512 < 0.115,
             qPrintable(QString("HL2 raw=512 watts=%1, expected ~0.111").arg(w_512)));
}

// HPSDRModel::FIRST is the -1 sentinel. Should hit default branch
// {0.09, 3.3, 6} — distinct from HL2's {1.5, 3.3, 6} now that HL2 has
// its own case. raw=4095 default → ~120.65 W, HL2 → ~7.24 W.
void TestPaTelemetryScaling::unknown_model_fwd_watts_uses_default_branch()
{
    const double w_unknown = scaleFwdPowerWatts(HPSDRModel::FIRST, 4095);
    QVERIFY2(w_unknown > 120.0 && w_unknown < 121.5,
             qPrintable(QString("FIRST raw=4095 watts=%1, expected ~120.65").arg(w_unknown)));
    // HL2 explicitly NOT equal to default any more (regression check).
    const double w_hl2 = scaleFwdPowerWatts(HPSDRModel::HERMESLITE, 4095);
    QVERIFY2(qAbs(w_unknown - w_hl2) > 100.0,
             qPrintable(QString("HL2 (%1) should differ from default (%2) by >100 W")
                        .arg(w_hl2).arg(w_unknown)));
}

// At raw=0 (or anything <= adc_cal_offset), volts clamps to 0 -> watts=0.
void TestPaTelemetryScaling::zero_raw_returns_zero_watts()
{
    QCOMPARE(scaleFwdPowerWatts(HPSDRModel::ANAN8000D, 0), 0.0);
    QCOMPARE(scaleFwdPowerWatts(HPSDRModel::ANAN_G2, 0), 0.0);
    QCOMPARE(scaleFwdPowerWatts(HPSDRModel::HERMES, 0), 0.0);
}

// ─── scaleFwdRevVoltage ─────────────────────────────────────────────────
//
// Voltage formula (FWD-side curve, per scaleFwdRevVoltage docstring):
//   volts = (raw - adc_cal_offset) / 4095.0 * refvoltage     clamp >= 0
//
// Per-board values match the FWD switch table in scaleFwdPowerWatts.

// HERMES: no on-board PA -> default triplet {0.09, 3.3, offset=6}
//   raw=1024 -> volts = (1024-6)/4095*3.3 = 0.8204
//   raw=4095 -> volts = (4095-6)/4095*3.3 = 3.2952
void TestPaTelemetryScaling::hermes_fwd_rev_voltage_two_raw_values()
{
    const double v_1024 = scaleFwdRevVoltage(HPSDRModel::HERMES, 1024);
    QVERIFY2(v_1024 > 0.81 && v_1024 < 0.83,
             qPrintable(QString("HERMES raw=1024 volts=%1").arg(v_1024)));

    const double v_4095 = scaleFwdRevVoltage(HPSDRModel::HERMES, 4095);
    QVERIFY2(v_4095 > 3.29 && v_4095 < 3.30,
             qPrintable(QString("HERMES raw=4095 volts=%1").arg(v_4095)));
}

// ORIONMKII / ANAN8000D: {0.08, 5.0, offset=18}
//   raw=2048 -> volts = (2048-18)/4095*5.0 = 2.4786
//   raw=4095 -> volts = (4095-18)/4095*5.0 = 4.9779
void TestPaTelemetryScaling::orionMkII_fwd_rev_voltage_two_raw_values()
{
    const double v_2048 = scaleFwdRevVoltage(HPSDRModel::ORIONMKII, 2048);
    QVERIFY2(v_2048 > 2.47 && v_2048 < 2.49,
             qPrintable(QString("ORIONMKII raw=2048 volts=%1").arg(v_2048)));

    const double v_4095 = scaleFwdRevVoltage(HPSDRModel::ORIONMKII, 4095);
    QVERIFY2(v_4095 > 4.97 && v_4095 < 4.98,
             qPrintable(QString("ORIONMKII raw=4095 volts=%1").arg(v_4095)));
}

// ANAN_G2 (and 7000D / G2_1K / REDPITAYA / ANVELINAPRO3): {0.12, 5.0, offset=32}
//   raw=2048 -> volts = (2048-32)/4095*5.0 = 2.4615
//   raw=4095 -> volts = (4095-32)/4095*5.0 = 4.9609
void TestPaTelemetryScaling::ananG2_fwd_rev_voltage_two_raw_values()
{
    const double v_2048 = scaleFwdRevVoltage(HPSDRModel::ANAN_G2, 2048);
    QVERIFY2(v_2048 > 2.45 && v_2048 < 2.47,
             qPrintable(QString("ANAN_G2 raw=2048 volts=%1").arg(v_2048)));

    const double v_4095 = scaleFwdRevVoltage(HPSDRModel::ANAN_G2, 4095);
    QVERIFY2(v_4095 > 4.95 && v_4095 < 4.97,
             qPrintable(QString("ANAN_G2 raw=4095 volts=%1").arg(v_4095)));
}

// HL2: falls to default {0.09, 3.3, offset=6} (same as HERMES).
//   raw=1024 -> volts ≈ 0.8204
//   raw=2048 -> volts ≈ 1.6452
void TestPaTelemetryScaling::hl2_fwd_rev_voltage_two_raw_values()
{
    const double v_1024 = scaleFwdRevVoltage(HPSDRModel::HERMESLITE, 1024);
    QVERIFY2(v_1024 > 0.81 && v_1024 < 0.83,
             qPrintable(QString("HL2 raw=1024 volts=%1").arg(v_1024)));

    const double v_2048 = scaleFwdRevVoltage(HPSDRModel::HERMESLITE, 2048);
    QVERIFY2(v_2048 > 1.64 && v_2048 < 1.66,
             qPrintable(QString("HL2 raw=2048 volts=%1").arg(v_2048)));
}

QTEST_GUILESS_MAIN(TestPaTelemetryScaling)
#include "tst_pa_telemetry_scaling.moc"
