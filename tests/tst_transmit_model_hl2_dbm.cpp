// no-port-check: NereusSDR-original unit-test file.  The mi0bot-Thetis
// references below are cite comments documenting which upstream lines each
// assertion verifies; no Thetis logic is ported in this test file.
// =================================================================
// tests/tst_transmit_model_hl2_dbm.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel::setPowerUsingTargetDbm — HL2 sub-step DSP
// audio-gain modulation in the txMode 1 TUNE_SLIDER branch.
//
// Spec §5.4.1; Plan Task 4; Issue #175.
//
// Reference table from mi0bot-Thetis console.cs:47660-47673 [v2.10.3.13-beta2]:
//   if (new_pwr <= 51) { mag = (new_pwr + 40) / 100; new_pwr = 0; }
//   else              { mag = 0.9999;                new_pwr = (new_pwr - 54) * 2; }
//
// Six cases: slider 0/25/51 sub-step regime, 52/99 attenuator regime,
// non-HL2 pass-through.
// =================================================================

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "core/PaProfile.h"
#include "models/Band.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

class TestTransmitModelHl2Dbm : public QObject {
    Q_OBJECT

private:
    // Helper: configure model into txMode 1 TUNE_SLIDER on Band40m, set
    // tunePower-by-band slider, and call setPowerUsingTargetDbm with the
    // given HPSDRModel.  Returns the result struct; caller asserts on
    // m.txPostGenToneMag() and result.newPower.
    TransmitModel::TxPowerResult callDbmHl2Tune(TransmitModel& m,
                                                int sliderVal,
                                                HPSDRModel model) {
        m.setTune(true);
        m.setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
        m.setTunePowerForBand(Band::Band40m, sliderVal);
        const PaProfile p;  // default sentinel — getGainForBand returns 1000,
                            // computeAudioVolume short-circuits.  This test
                            // does not care about audioVolume.
        return m.setPowerUsingTargetDbm(
            p, Band::Band40m,
            /*bSetPower=*/true,
            /*bFromTune=*/false,
            /*bTwoTone=*/false,
            model);
    }

private slots:

    void init() {
        AppSettings::instance().clear();
    }

    // Slider 0 -> sub-step: mag = (0 + 40) / 100 = 0.40; new_pwr = 0.
    // mi0bot console.cs:47664-47666 [v2.10.3.13-beta2].
    void hl2_subStep_zero() {
        TransmitModel m;
        const auto r = callDbmHl2Tune(m, 0, HPSDRModel::HERMESLITE);
        QCOMPARE(m.txPostGenToneMag(), 0.40);
        QCOMPARE(r.newPower, 0);
    }

    // Slider 25 -> sub-step: mag = (25 + 40) / 100 = 0.65; new_pwr = 0.
    // mi0bot console.cs:47664-47666 [v2.10.3.13-beta2].
    void hl2_subStep_mid() {
        TransmitModel m;
        const auto r = callDbmHl2Tune(m, 25, HPSDRModel::HERMESLITE);
        QCOMPARE(m.txPostGenToneMag(), 0.65);
        QCOMPARE(r.newPower, 0);
    }

    // Slider 51 -> sub-step boundary: mag = (51 + 40) / 100 = 0.91; new_pwr = 0.
    // mi0bot console.cs:47664-47666 [v2.10.3.13-beta2].
    void hl2_subStep_boundary() {
        TransmitModel m;
        const auto r = callDbmHl2Tune(m, 51, HPSDRModel::HERMESLITE);
        QCOMPARE(m.txPostGenToneMag(), 0.91);
        QCOMPARE(r.newPower, 0);
    }

    // Slider 52 -> attenuator regime entry: mag = 0.9999.
    // Formula: (52 - 54) * 2 = -4, but the post-switch
    // ConstrainAValue clamp at console.cs:47704 [v2.10.3.13-beta2] (mirrored
    // by NereusSDR's outer std::clamp in TransmitModel.cpp) pulls negative
    // values up to 0.  The formula path is exercised; the asserted post-
    // clamp final value matches mi0bot wire behavior on slider 52.
    // mi0bot console.cs:47670-47672 + 47704 [v2.10.3.13-beta2].
    void hl2_attenuator_low() {
        TransmitModel m;
        const auto r = callDbmHl2Tune(m, 52, HPSDRModel::HERMESLITE);
        QCOMPARE(m.txPostGenToneMag(), 0.9999);
        // Formula yields -4; outer ConstrainAValue clamp clips to 0.
        QCOMPARE(r.newPower, 0);
    }

    // Slider 99 -> attenuator regime top: mag = 0.9999; new_pwr = (99 - 54) * 2 = 90.
    // mi0bot console.cs:47670-47672 [v2.10.3.13-beta2].
    void hl2_attenuator_top() {
        TransmitModel m;
        const auto r = callDbmHl2Tune(m, 99, HPSDRModel::HERMESLITE);
        QCOMPARE(m.txPostGenToneMag(), 0.9999);
        QCOMPARE(r.newPower, 90);
    }

    // Non-HL2 pass-through: ANAN100 is NOT HERMESLITE, so the HL2 sub-step
    // branch must NOT fire — txPostGenToneMag stays at default 1.0 and
    // new_pwr equals the slider value (after the slider clamp).
    // mi0bot console.cs:47675-47677 [v2.10.3.13-beta2] — the else-branch
    // of the `if (HardwareSpecific.Model == HPSDRModel.HERMESLITE)` guard.
    void nonHl2_unchanged() {
        TransmitModel m;
        const double magBefore = m.txPostGenToneMag();
        const auto r = callDbmHl2Tune(m, 50, HPSDRModel::ANAN100);
        QCOMPARE(m.txPostGenToneMag(), magBefore);  // untouched (default 1.0)
        QCOMPARE(r.newPower, 50);                   // straight pass-through
    }
};

QTEST_MAIN(TestTransmitModelHl2Dbm)
#include "tst_transmit_model_hl2_dbm.moc"
