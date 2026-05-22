// no-port-check: test file — verifies NereusSDR::scaleFwdPowerWatts(),
// NereusSDR::scaleFwdRevVoltage(), and NereusSDR::scaleExciterPowerMw()
// output values; the Thetis source references are citations in the
// docstrings, not derivation claims. The ported functions themselves are
// registered in THETIS-PROVENANCE.md.
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

    // ─── ANAN_G2E (D2: joins ANAN7000D/G2/ANVELINAPRO3/REDPITAYA fwd triplet) ─
    void anan_g2e_fwd_uses_anan7000d_triplet();

    // ─── scaleFwdRevVoltage (FWD-side per-board curve) ───────────────────
    void hermes_fwd_rev_voltage_two_raw_values();
    void orionMkII_fwd_rev_voltage_two_raw_values();
    void ananG2_fwd_rev_voltage_two_raw_values();
    void hl2_fwd_rev_voltage_two_raw_values();

    // ─── scaleExciterPowerMw (F1: ANAN_G2E joins OrionMKII family) ──────────
    // From Thetis console.cs:25179-25237 (computeOrionMkIIExciterPower) and
    // console.cs:25120-25178 (computeExciterPower) and dispatch switch
    // at console.cs:26001-26013 [v2.10.3.15]. //N1GP G2E added //DH1KLM
    void anan_g2e_exciter_uses_orion_mkii_curve();
    void orionmkii_exciter_matches_anan_g2e();
    void default_board_exciter_uses_low_power_curve();
    void anan_g2e_exciter_at_zero_is_zero();

    // ─── scaleHermesLiteTempCelsius (port of mi0bot _MKIIHL2Temp formula) ─
    void hl2_temp_at_zero_raw_is_minus_fifty_c();
    void hl2_temp_at_typical_idle_raw_is_room_temperature();
    void hl2_temp_at_mid_scale_is_one_thirteen_c();
    void hl2_temp_at_full_scale_is_about_two_seventy_six_c();
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

// ─── scaleHermesLiteTempCelsius ─────────────────────────────────────────
//
// Port of mi0bot console.cs:25079 [v2.10.3.13-beta2 @c26a8a4]:
//   _MKIIHL2Temp = (3.26f * (tempAverage / 4096.0f) - 0.5f) / 0.01f;
//
// Single-sample inputs (averaging is the caller's job):
//   raw=0    -> (3.26 * 0      - 0.5) / 0.01 = -50.0 °C
//   raw=628  -> (3.26 * 0.1533 - 0.5) / 0.01 ≈   0.0 °C
//   raw=942  -> (3.26 * 0.2300 - 0.5) / 0.01 ≈  25.0 °C   (typical idle)
//   raw=2048 -> (3.26 * 0.5    - 0.5) / 0.01 = 113.0 °C
//   raw=4095 -> (3.26 * 0.99975- 0.5) / 0.01 ≈ 275.92 °C

void TestPaTelemetryScaling::hl2_temp_at_zero_raw_is_minus_fifty_c()
{
    const double t_zero = scaleHermesLiteTempCelsius(0);
    QCOMPARE(t_zero, -50.0);
}

void TestPaTelemetryScaling::hl2_temp_at_typical_idle_raw_is_room_temperature()
{
    // raw=942 hits ~25 °C (room temperature), the value an idle HL2
    // FPGA reads on the bench. Pin it tight to catch sign/scale errors.
    const double t = scaleHermesLiteTempCelsius(942);
    QVERIFY2(t > 24.5 && t < 25.5,
             qPrintable(QString("HL2 raw=942 temp=%1°C, expected ~25").arg(t)));
}

void TestPaTelemetryScaling::hl2_temp_at_mid_scale_is_one_thirteen_c()
{
    // raw=2048 -> exact mid of (raw / 4096) so the formula simplifies
    // to (3.26 * 0.5 - 0.5) / 0.01 = 113.0 °C exactly.
    const double t = scaleHermesLiteTempCelsius(2048);
    QCOMPARE(t, 113.0);
}

void TestPaTelemetryScaling::hl2_temp_at_full_scale_is_about_two_seventy_six_c()
{
    // raw=4095 -> well above any plausible FPGA junction temperature,
    // but pinned here to catch off-by-one /4095-vs-/4096 errors.  The
    // mi0bot port divides by 4096 (not 4095), so raw=4095 is just below
    // 3.26 V, not at it.
    const double t = scaleHermesLiteTempCelsius(4095);
    QVERIFY2(t > 275.5 && t < 276.0,
             qPrintable(QString("HL2 raw=4095 temp=%1°C, expected ~275.92").arg(t)));
}

// ANAN_G2E joins the ANAN7000D / ANAN_G2 / ANVELINAPRO3 / REDPITAYA group.
// Triplet: bridge_volt=0.12, refvoltage=5.0, adc_cal_offset=32.
// Ported in PaTelemetryScaling.cpp; Thetis attribution there.
//
// Spot-check at raw=2048:
//   volts = (2048-32)/4095*5.0 = 2.4615...
//   watts = 2.4615^2 / 0.12   = 50.49 W (approx)
//
// Verify: result matches ANAN_G2 (both in same triplet group).
void TestPaTelemetryScaling::anan_g2e_fwd_uses_anan7000d_triplet()
{
    // G2E and G2 share the {0.12, 5.0, 32} fwd triplet.
    // Thetis cites are in PaTelemetryScaling.cpp (//N1GP G2E added //DH1KLM).
    const double w_g2e  = scaleFwdPowerWatts(HPSDRModel::ANAN_G2E, 2048);
    const double w_g2   = scaleFwdPowerWatts(HPSDRModel::ANAN_G2,  2048);
    QCOMPARE(w_g2e, w_g2);

    // Verify G2E does NOT fall through to the default {0.09, 3.3, 6} branch.
    const double w_def = scaleFwdPowerWatts(HPSDRModel::FIRST, 2048);
    QVERIFY2(qAbs(w_g2e - w_def) > 5.0,
             qPrintable(QString("G2E (%1) should differ from default (%2) by >5 W")
                        .arg(w_g2e).arg(w_def)));

    // Pinned magnitude check: ~50.5 W at raw=2048.
    QVERIFY2(w_g2e > 50.0 && w_g2e < 51.0,
             qPrintable(QString("G2E raw=2048 watts=%1, expected ~50.5").arg(w_g2e)));
}

// ─── scaleExciterPowerMw ─────────────────────────────────────────────────
//
// F1 ANAN-G2E port: ANAN_G2E joins OrionMKII / ANAN7000D / ANAN8000D /
// ANAN_G2 / ANAN_G2_1K / ANVELINAPRO3 / REDPITAYA in computeOrionMkIIExciterPower().
// From Thetis console.cs:26001-26013 [v2.10.3.15] — dispatch switch.
// //N1GP G2E added (console.cs:26004)  //DH1KLM (console.cs:26007)
//
// computeOrionMkIIExciterPower breakpoints (from console.cs:25179-25237
// [v2.10.3.15]):
//   adcCounts <= 60:   result = 0.0
//   60 < adc <= 580:   result = (adc - 60) * 0.097656
//   ...  (see PaTelemetryScaling.cpp for full table)
//
// Spot-check at adc=905 (boundary of first two inner segments):
//   result = 50.0 + ((905-580) * 0.153846) = 50.0 + 50.0 = 100.0 mW exactly
// Spot-check at adc=1340 (boundary of outer segments):
//   result = 200.0 + ((1340-1340) * 0.294118) = 200.0 mW exactly

void TestPaTelemetryScaling::anan_g2e_exciter_uses_orion_mkii_curve()
{
    // G2E should use the OrionMkII piecewise curve.
    // At adc=905, result must be exactly 100 mW (segment boundary).
    const float mw_g2e_905 = scaleExciterPowerMw(HPSDRModel::ANAN_G2E, 905);
    QVERIFY2(qAbs(static_cast<double>(mw_g2e_905) - 100.0) < 0.1,
             qPrintable(QString("G2E adc=905 mW=%1, expected ~100.0").arg(mw_g2e_905)));

    // G2E must NOT equal the low-power (default) curve at the same point.
    // computeExciterPower at adc=905: adc=905 is in the 874..1380 segment
    //   result = 50.0 + ((905-874) * 0.098814) = 50.0 + 3.063 = 53.063 mW
    const float mw_default_905 = scaleExciterPowerMw(HPSDRModel::FIRST, 905);
    QVERIFY2(qAbs(static_cast<double>(mw_g2e_905 - mw_default_905)) > 40.0,
             qPrintable(QString("G2E (%1) and default (%2) should differ by >40 mW at adc=905")
                        .arg(mw_g2e_905).arg(mw_default_905)));
}

void TestPaTelemetryScaling::orionmkii_exciter_matches_anan_g2e()
{
    // OrionMKII and ANAN_G2E use the same curve; their outputs must be identical.
    for (quint16 raw : {quint16(0), quint16(60), quint16(580), quint16(905),
                        quint16(1340), quint16(1680), quint16(1950), quint16(4095)}) {
        const float mw_orion = scaleExciterPowerMw(HPSDRModel::ORIONMKII, raw);
        const float mw_g2e   = scaleExciterPowerMw(HPSDRModel::ANAN_G2E, raw);
        QVERIFY2(qAbs(static_cast<double>(mw_orion - mw_g2e)) < 0.001,
                 qPrintable(QString("OrionMKII (%1) != G2E (%2) at raw=%3")
                            .arg(mw_orion).arg(mw_g2e).arg(raw)));
    }
}

void TestPaTelemetryScaling::default_board_exciter_uses_low_power_curve()
{
    // HERMES / ANAN100 etc. fall to computeExciterPower (default branch).
    // At adc=98, result=0 (dead-zone).
    // At adc=874, boundary: result = (874-98)*0.065703 = 50.98... mW ≈ 51.0 mW.
    const float mw_at_98 = scaleExciterPowerMw(HPSDRModel::HERMES, 98);
    QCOMPARE(mw_at_98, 0.0f);

    const float mw_at_874 = scaleExciterPowerMw(HPSDRModel::HERMES, 874);
    // (874-98)*0.065703 = 776 * 0.065703 = 50.985... mW
    QVERIFY2(mw_at_874 > 50.0f && mw_at_874 < 52.0f,
             qPrintable(QString("HERMES adc=874 mW=%1, expected ~51.0").arg(mw_at_874)));
}

void TestPaTelemetryScaling::anan_g2e_exciter_at_zero_is_zero()
{
    // adc <= 60 → result=0 for OrionMkII curve.
    QCOMPARE(scaleExciterPowerMw(HPSDRModel::ANAN_G2E, 0),   0.0f);
    QCOMPARE(scaleExciterPowerMw(HPSDRModel::ANAN_G2E, 60),  0.0f);
    QCOMPARE(scaleExciterPowerMw(HPSDRModel::ORIONMKII, 0),  0.0f);
    QCOMPARE(scaleExciterPowerMw(HPSDRModel::ANAN_G2E, 61),
             scaleExciterPowerMw(HPSDRModel::ORIONMKII, 61));
}

QTEST_GUILESS_MAIN(TestPaTelemetryScaling)
#include "tst_pa_telemetry_scaling.moc"
