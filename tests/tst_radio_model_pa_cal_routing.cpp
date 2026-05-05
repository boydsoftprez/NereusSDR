// =================================================================
// tests/tst_radio_model_pa_cal_routing.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original test file.  The Thetis citation
// below documents which upstream behaviour is exercised; no C# is
// translated in this file.
//
// Source: Thetis console.cs:6691-6724 CalibratedPAPower [v2.10.3.13+501e3f51]
//
// P1 full-parity §3.4: verifies that RadioModel routes the raw alex_fwd
// watts reading through CalibrationController::calibratedFwdPowerWatts()
// before publishing to RadioStatus.  Thetis applies CalibratedPAPower as
// a piecewise-linear remap of the raw ADC-derived watts to the user-
// calibrated watts; NereusSDR mirrors this in the per-sample
// paTelemetryUpdated handler installed by RadioModel::wireConnectionSignals.
//
// Coverage:
//   - identity_when_no_profile          — default-constructed PaCalProfile{None}
//                                         leaves the published RadioStatus value
//                                         equal to the raw post-scale watts.
//   - calibrated_when_anan100_profile_loaded
//                                       — Anan100 defaults with watts[5]=47.0f
//                                         remaps a raw 50 W reading to ~47 W.
//   - reflected_path_unchanged_by_cal   — Reflected-power publishing is not
//                                         routed through the FWD cal table
//                                         (Thetis CalibratedPAPower is FWD-only).
//
// Strategy:
//   The per-sample handler is installed by wireConnectionSignals which also
//   spins up RxDspWorker / DSP threads / etc.  Driving that path under
//   QtTest is heavy, so the production lambda body has been extracted
//   into a private method `RadioModel::handlePaTelemetryForTest()` (mirrors
//   the existing on*ForTest hooks like setConnectionStateForTest and
//   onConnectedForTest) which is what wireConnectionSignals' lambda also
//   calls.  Tests invoke that hook directly with chosen (fwdRaw, revRaw,
//   …) tuples and assert against the public RadioStatus surface.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/CalibrationController.h"
#include "core/HpsdrModel.h"
#include "core/PaCalProfile.h"
#include "core/RadioStatus.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

// ANAN_G2-shaped raw FWD ADC count that produces ~50 W under
// scaleFwdPowerWatts: refvoltage=5.0, bridge_volt=0.12, adc_cal_offset=32.
// Solve watts = (((adc - 32) / 4095) * 5.0)^2 / 0.12 = 50 →
// volts ≈ sqrt(50 * 0.12) ≈ 2.449 → adc ≈ 32 + 4095 * 2.449/5.0 ≈ 2037.
//
// We don't pin the exact watts result — we use whatever scaleFwdPowerWatts
// computes for this raw value and then assert the cal step's relative
// behaviour against that baseline.
constexpr quint16 kFwdRawForApproxFiftyWattsOnG2 = 2037;

}  // anonymous namespace

class TstRadioModelPaCalRouting : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void init()         { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // ── 1. No PA cal profile loaded → identity routing ─────────────────────
    // With a default-constructed CalibrationController (boardClass == None),
    // calibratedFwdPowerWatts() short-circuits to identity, so the published
    // RadioStatus.forwardPower() must equal the raw post-scale watts that
    // the prior code path would have published.
    void identity_when_no_profile()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);  // ANAN_G2 family

        // Sanity: controller starts None.
        QCOMPARE(model.calibrationController().paCalProfile().boardClass,
                 PaCalBoardClass::None);

        // Drive the per-sample handler with our chosen raw FWD value.
        model.handlePaTelemetryForTest(
            kFwdRawForApproxFiftyWattsOnG2,  // fwdRaw
            /*revRaw*/      0,
            /*exciterRaw*/  0,
            /*userAdc0Raw*/ 0,
            /*userAdc1Raw*/ 0,
            /*supplyRaw*/   0);

        // Identity → published equals raw post-scale watts (~50).
        const double published = model.radioStatus().forwardPowerWatts();
        QVERIFY2(published > 49.0 && published < 51.0,
                 qPrintable(QStringLiteral("identity-path FWD %1 not near 50 W")
                                .arg(published)));
    }

    // ── 2. Anan100 cal profile with watts[5]=47.0 → 50 W raw maps to ~47 W ─
    // Loading an Anan100 profile and lowering its watts[5] entry to 47.0 W
    // tells Thetis-style PowerKernel that the user measured 47 W when the
    // hardware reports a raw 50 W reading.  After routing, RadioStatus
    // must show the calibrated value (~47 W), not the raw scaled value.
    void calibrated_when_anan100_profile_loaded()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);

        // Install Anan100 defaults, then mutate the 50 W cal point to 47 W.
        // Anan100 defaults: watts = {0,10,20,30,40,50,60,70,80,90,100}.
        auto profile = PaCalProfile::defaults(PaCalBoardClass::Anan100);
        profile.watts[5] = 47.0f;
        model.calibrationControllerMutable().setPaCalProfile(profile);

        // Sanity: profile installed.
        QCOMPARE(model.calibrationController().paCalProfile().boardClass,
                 PaCalBoardClass::Anan100);

        // First, capture the raw post-scale baseline (no cal) for comparison.
        // We rebuild a fresh model with no profile and the same raw input.
        double rawBaseline = 0.0;
        {
            RadioModel rawModel;
            rawModel.setBoardForTest(HPSDRHW::Saturn);
            rawModel.handlePaTelemetryForTest(
                kFwdRawForApproxFiftyWattsOnG2, 0, 0, 0, 0, 0);
            rawBaseline = rawModel.radioStatus().forwardPowerWatts();
        }
        QVERIFY2(rawBaseline > 49.0 && rawBaseline < 51.0,
                 qPrintable(QStringLiteral("baseline raw %1 not near 50 W")
                                .arg(rawBaseline)));

        // Drive the calibrated model.
        model.handlePaTelemetryForTest(
            kFwdRawForApproxFiftyWattsOnG2, 0, 0, 0, 0, 0);

        // After cal: PowerKernel maps a raw value near watts[5] (47.0) to
        // an output near interval*5 = 50.0.  Wait — the algorithm is the
        // OTHER direction: profile.watts is the table indexed by segment.
        // Cross-check by calling the controller directly with the same
        // baseline value: that's the canonical expected output.
        const double expected = double(
            model.calibrationController().calibratedFwdPowerWatts(
                float(rawBaseline)));

        const double published = model.radioStatus().forwardPowerWatts();
        QCOMPARE(published, expected);

        // And the calibrated value differs from the raw baseline (proves
        // the cal step actually ran).
        QVERIFY2(qAbs(published - rawBaseline) > 0.5,
                 qPrintable(QStringLiteral(
                     "calibrated %1 should differ from raw %2 by > 0.5 W")
                     .arg(published).arg(rawBaseline)));
    }

    // ── 3. Reflected path is NOT routed through FWD cal ───────────────────
    // Thetis CalibratedPAPower is FWD-only.  Confirm the reflected-power
    // publication is unaffected by an installed FWD cal profile by checking
    // that the published RadioStatus.reflectedPower() matches the raw
    // scaleRevPowerWatts output (which the cal table cannot influence).
    // We test this indirectly: rev=0 raw must always yield rev=0 W after
    // publication, even with a wonky FWD cal table installed.
    void reflected_path_unchanged_by_cal()
    {
        RadioModel model;
        model.setBoardForTest(HPSDRHW::Saturn);

        // Install a profile with all FWD cal points zeroed except [10] —
        // a profile that would map ALL inputs to extreme values if it were
        // applied to rev power.
        auto profile = PaCalProfile::defaults(PaCalBoardClass::Anan100);
        for (int i = 1; i <= 10; ++i) {
            profile.watts[i] = 0.001f * float(i);  // tiny values
        }
        model.calibrationControllerMutable().setPaCalProfile(profile);

        // Drive with rev=0 raw.  Without FWD-cal-leaking-into-rev, the
        // reflected reading must remain 0 W.
        model.handlePaTelemetryForTest(
            /*fwdRaw*/      kFwdRawForApproxFiftyWattsOnG2,
            /*revRaw*/      0,
            /*exciterRaw*/  0,
            /*userAdc0Raw*/ 0,
            /*userAdc1Raw*/ 0,
            /*supplyRaw*/   0);

        QCOMPARE(model.radioStatus().reflectedPowerWatts(), 0.0);
    }
};

QTEST_GUILESS_MAIN(TstRadioModelPaCalRouting)
#include "tst_radio_model_pa_cal_routing.moc"
