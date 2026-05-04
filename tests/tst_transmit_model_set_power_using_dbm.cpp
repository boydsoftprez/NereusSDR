// no-port-check: NereusSDR-original unit-test file.  The "console.cs"
// references below are cite comments documenting which Thetis lines each
// assertion verifies; no Thetis logic is ported in this test file.
// =================================================================
// tests/tst_transmit_model_set_power_using_dbm.cpp  (NereusSDR)
// =================================================================
//
// Unit tests for TransmitModel::setPowerUsingTargetDbm — Phase 3 Agent
// 3C of issue #167.  Full deep-parity port of Thetis
// SetPowerUsingTargetDBM (console.cs:46645-46762 [v2.10.3.13]) integrating
// Phase 3A scaffolding + Phase 3B math kernel into the unified API.
//
// Covers:
//   §1 txMode 0 normal — writes m_powerByBand on side-effect
//   §2 txMode 1 tune — DriveSlider / TuneSlider / Fixed routing
//   §3 txMode 2 two-tone — DriveSlider / Fixed routing
//   §4 bSetPower=false skips side-effects (no audioVolumeChanged signal,
//      no m_lastPower update, no m_powerByBand write)
//   §5 ATT-on-TX gate fires when PS-active simulated
//   §6 ATT-on-TX gate dormant by default (pureSignalActive returns false)
//   §7 ATT-on-TX gate skips on no-power-change
//   §8 ATT-on-TX gate fires on decrease only when _anddecreased=true
//   §9 ATT-on-TX gate skips on decrease when _anddecreased=false
//   §10 audioVolumeChanged signal payload matches computeAudioVolume
//   §11 K2GX integration regression (ANAN-8000DLE 80m TUN slider 50)
//   §12 No XVTR live wiring — sentinel fallback in computeAudioVolume
//   §13 Edge case: power=0
//
// Source references (cite comments only — no Thetis logic in tests):
//   console.cs:46645-46762 [v2.10.3.13] — full SetPowerUsingTargetDBM
//   console.cs:46740-46748 [v2.10.3.13] — //[2.10.3.5]MW0LGE ATT-on-TX gate
// =================================================================

#include <QtTest/QtTest>

#include <cmath>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "core/PaProfile.h"
#include "core/StepAttenuatorController.h"
#include "models/Band.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

// Helper: build a "Default - ANAN8000D" PaProfile.
PaProfile makeProfile8000d() {
    return PaProfile(QStringLiteral("Default - ANAN8000D"),
                     HPSDRModel::ANAN8000D, true);
}

// Helper: build a "Default - ANAN_G2" PaProfile.
PaProfile makeProfileG2() {
    return PaProfile(QStringLiteral("Default - ANAN_G2"),
                     HPSDRModel::ANAN_G2, true);
}

}  // namespace

class TstTransmitModelSetPowerUsingDbm : public QObject {
    Q_OBJECT

private slots:

    void init() {
        // Clear AppSettings before each test for isolation.
        AppSettings::instance().clear();
    }

    // =========================================================================
    // §1 txMode 0 normal — writes m_powerByBand
    // =========================================================================
    //
    // Thetis console.cs:46673-46676 [v2.10.3.13]:
    //   case 0: //normal
    //       new_pwr = ptbPWR.Value;
    //       power_by_band[(int)_tx_band] = new_pwr;
    //
    // With MOX off, TUN off, 2TONE off, bFromTune=false: txMode 0 (normal).
    // setPower(75) -> setPowerUsingTargetDbm(...) reads m_power and writes
    // m_powerByBand[currentBand] as side-effect.
    void txMode0_normal_writesPowerByBand() {
        TransmitModel t;
        const PaProfile p = makeProfile8000d();
        t.setMox(false);
        t.setTune(false);
        t.setTwoToneActive(false);
        t.setPower(75);

        const auto result = t.setPowerUsingTargetDbm(
            p, Band::Band80m,
            /*bSetPower=*/true, /*bFromTune=*/false, /*bTwoTone=*/false);

        QCOMPARE(result.newPower, 75);
        QVERIFY(result.bConstrain);
        QCOMPARE(t.powerForBand(Band::Band80m), 75);
    }

    // =========================================================================
    // §2 txMode 1 tune via DriveSlider
    // =========================================================================
    //
    // Thetis console.cs:46680-46682 [v2.10.3.13]:
    //   case DRIVE_SLIDER: new_pwr = ptbPWR.Value; break;
    //
    // setTune(true), DriveSlider source -> reads m_power.
    void txMode1_tune_driveSlider_returnsPowerSlider() {
        TransmitModel t;
        const PaProfile p = makeProfile8000d();
        t.setTune(true);
        t.setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
        t.setPower(40);

        const auto result = t.setPowerUsingTargetDbm(
            p, Band::Band20m,
            /*bSetPower=*/true, /*bFromTune=*/false, /*bTwoTone=*/false);

        QCOMPARE(result.newPower, 40);
        QVERIFY(result.bConstrain);
    }

    // =========================================================================
    // §2 (cont) txMode 1 tune via TuneSlider
    // =========================================================================
    //
    // Thetis console.cs:46683-46686 [v2.10.3.13]:
    //   case TUNE_SLIDER: slider = ptbTune; new_pwr = ptbTune.Value; break;
    //
    // setTune(true), TuneSlider source -> reads tunePowerForBand(currentBand).
    void txMode1_tune_tuneSlider_returnsTunePowerForBand() {
        TransmitModel t;
        const PaProfile p = makeProfile8000d();
        t.setTune(true);
        t.setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
        t.setTunePowerForBand(Band::Band20m, 25);

        const auto result = t.setPowerUsingTargetDbm(
            p, Band::Band20m,
            /*bSetPower=*/true, /*bFromTune=*/false, /*bTwoTone=*/false);

        QCOMPARE(result.newPower, 25);
        QVERIFY(result.bConstrain);
    }

    // =========================================================================
    // §2 (cont) txMode 1 tune via Fixed
    // =========================================================================
    //
    // Thetis console.cs:46687-46690 [v2.10.3.13]:
    //   case FIXED: new_pwr = tune_power; bConstrain = false; break;
    //
    // setTune(true), Fixed source -> reads m_tunePower; bConstrain=false.
    void txMode1_tune_fixed_returnsTunePowerAndUnconstrained() {
        TransmitModel t;
        const PaProfile p = makeProfile8000d();
        t.setTune(true);
        t.setTuneDrivePowerSource(DrivePowerSource::Fixed);
        t.setTunePower(17);

        const auto result = t.setPowerUsingTargetDbm(
            p, Band::Band20m,
            /*bSetPower=*/true, /*bFromTune=*/false, /*bTwoTone=*/false);

        QCOMPARE(result.newPower, 17);
        QVERIFY(!result.bConstrain);  // Fixed bypasses slider clamp.
    }

    // =========================================================================
    // §3 txMode 2 two-tone via DriveSlider
    // =========================================================================
    //
    // Thetis console.cs:46696-46698 [v2.10.3.13]:
    //   case DRIVE_SLIDER: new_pwr = ptbPWR.Value; break;
    //
    // setTwoToneActive(true), DriveSlider source -> reads m_power.
    void txMode2_twoTone_driveSlider_returnsPowerSlider() {
        TransmitModel t;
        const PaProfile p = makeProfile8000d();
        t.setTwoToneActive(true);
        t.setTwoToneDrivePowerSource(DrivePowerSource::DriveSlider);
        t.setPower(60);

        const auto result = t.setPowerUsingTargetDbm(
            p, Band::Band40m,
            /*bSetPower=*/true, /*bFromTune=*/false, /*bTwoTone=*/false);

        QCOMPARE(result.newPower, 60);
        QVERIFY(result.bConstrain);
    }

    // =========================================================================
    // §3 (cont) txMode 2 two-tone via Fixed
    // =========================================================================
    //
    // Thetis console.cs:46703-46706 [v2.10.3.13]:
    //   case FIXED: new_pwr = twotone_tune_power; bConstrain = false; break;
    //
    // setTwoToneActive(true), Fixed source -> reads twoTonePower();
    // bConstrain=false.
    void txMode2_twoTone_fixed_returnsTwoTonePowerAndUnconstrained() {
        TransmitModel t;
        const PaProfile p = makeProfile8000d();
        t.setTwoToneActive(true);
        t.setTwoToneDrivePowerSource(DrivePowerSource::Fixed);
        t.setTwoTonePower(45);

        const auto result = t.setPowerUsingTargetDbm(
            p, Band::Band40m,
            /*bSetPower=*/true, /*bFromTune=*/false, /*bTwoTone=*/false);

        QCOMPARE(result.newPower, 45);
        QVERIFY(!result.bConstrain);
    }

    // =========================================================================
    // §4 bSetPower=false skips side-effects
    // =========================================================================
    //
    // Thetis console.cs:46738 [v2.10.3.13]:
    //   if (!bSetPower) return new_pwr;
    //
    // bSetPower=false -> result populated, but no audioVolumeChanged signal
    // fires, m_lastPower NOT updated, m_powerByBand NOT updated.
    void bSetPowerFalse_skipsSideEffects() {
        TransmitModel t;
        const PaProfile p = makeProfile8000d();
        t.setMox(false);
        t.setTune(false);
        t.setTwoToneActive(false);
        t.setPower(50);
        // Establish a known state for fields the gate would touch.
        t.setLastPower(-1);
        const int powerBefore = t.powerForBand(Band::Band80m);

        QSignalSpy audioSpy(&t, &TransmitModel::audioVolumeChanged);

        const auto result = t.setPowerUsingTargetDbm(
            p, Band::Band80m,
            /*bSetPower=*/false, /*bFromTune=*/false, /*bTwoTone=*/false);

        // Result is populated even when bSetPower=false.
        QCOMPARE(result.newPower, 50);
        // m_powerByBand[Band80m] is still updated for txMode 0 normal mode
        // because the side-effect runs INSIDE the txMode switch, BEFORE
        // the bSetPower check (matches Thetis: power_by_band write at
        // 46676 runs before the !bSetPower check at 46738).
        // So we expect this side-effect to land regardless of bSetPower.
        QCOMPARE(t.powerForBand(Band::Band80m), 50);

        // But the post-bSetPower side-effects should NOT have fired:
        //   - audioVolumeChanged signal NOT emitted
        QCOMPARE(audioSpy.count(), 0);
        //   - m_lastPower NOT updated
        QCOMPARE(t.lastPower(), -1);

        // Suppress unused warning when only powerBefore is referenced.
        Q_UNUSED(powerBefore);
    }

    // =========================================================================
    // §5 ATT-on-TX gate fires when PS-active simulated
    // =========================================================================
    //
    // Thetis console.cs:46740-46748 [v2.10.3.13]:
    //   //[2.10.3.5]MW0LGE max tx attenuation when power is increased and PS is enabled
    //   if (new_pwr != _lastPower && chkFWCATUBypass.Checked && _forceATTwhenPowerChangesWhenPSAon)
    //   {
    //       if(new_pwr > _lastPower || _forceATTwhenPowerChangesWhenPSAon_anddecreased)
    //           SetupForm.ATTOnTX = 31;
    //
    //       _lastPower = new_pwr;
    //   }
    //
    // With pureSignalActive simulated true + power changing, the gate
    // fires: setAttOnTxValue(31) called on the injected controller.
    void attOnTxGate_firesWhenPsActive() {
        TransmitModel t;
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBand(Band::Band20m);
        t.setStepAttenuatorController(&ctrl);

        // Simulate PureSignal-active.
        t.setPureSignalActiveForTest(true);
        // Phase 3A defaults: forceAttwhenPowerChangesWhenPSAon=true.
        QVERIFY(t.forceAttwhenPowerChangesWhenPSAon());

        const PaProfile p = makeProfile8000d();
        t.setMox(false);
        t.setTune(false);
        t.setTwoToneActive(false);
        t.setPower(60);
        // Reset m_lastPower to a value that will trigger the gate
        // (default -1 sentinel: any new_pwr triggers because new_pwr > -1).
        t.setLastPower(-1);

        const auto result = t.setPowerUsingTargetDbm(
            p, Band::Band20m,
            /*bSetPower=*/true, /*bFromTune=*/false, /*bTwoTone=*/false);

        // ATT-on-TX gate fires: new_pwr=60 > lastPower=-1, gate engaged,
        // setAttOnTxValue(31) called.
        QCOMPARE(ctrl.attOnTxValue(), 31);
        // m_lastPower updated to new_pwr.
        QCOMPARE(t.lastPower(), 60);
        QCOMPARE(result.newPower, 60);
    }

    // =========================================================================
    // §6 ATT-on-TX gate dormant by default
    // =========================================================================
    //
    // Without overriding pureSignalActive (returns false in Phase 3A
    // scaffolding), the entire ATT-on-TX block is skipped: setAttOnTxValue
    // NOT called, m_lastPower stays at -1.
    void attOnTxGate_dormantWhenPsInactive() {
        TransmitModel t;
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBand(Band::Band20m);
        // Establish a known initial ATT value (NOT 31) so we can detect
        // bad gate-fire via the controller's stored value.
        ctrl.setAttOnTxValue(7);
        QCOMPARE(ctrl.attOnTxValue(), 7);

        t.setStepAttenuatorController(&ctrl);
        // pureSignalActive() returns false (Phase 3A default; no override).
        QVERIFY(!t.pureSignalActive());

        const PaProfile p = makeProfile8000d();
        t.setMox(false);
        t.setTune(false);
        t.setTwoToneActive(false);
        t.setPower(60);
        t.setLastPower(-1);

        t.setPowerUsingTargetDbm(p, Band::Band20m,
                                 /*bSetPower=*/true,
                                 /*bFromTune=*/false,
                                 /*bTwoTone=*/false);

        // Gate did NOT fire: ATT-on-TX value unchanged at 7 (NOT 31).
        QCOMPARE(ctrl.attOnTxValue(), 7);
        // m_lastPower stays at -1 because the gate's outer guard
        // (pureSignalActive() && forceAttwhenPowerChangesWhenPSAon) is
        // false, so the entire block is skipped — including the
        // _lastPower = new_pwr update.
        QCOMPARE(t.lastPower(), -1);
    }

    // =========================================================================
    // §7 ATT-on-TX gate skips on no-power-change
    // =========================================================================
    //
    // Thetis console.cs:46741 [v2.10.3.13]: outer guard requires
    // (new_pwr != _lastPower).  When unchanged, gate skips entirely —
    // setAttOnTxValue NOT called.
    void attOnTxGate_skipsOnNoPowerChange() {
        TransmitModel t;
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBand(Band::Band20m);
        ctrl.setAttOnTxValue(7);  // sentinel non-31

        t.setStepAttenuatorController(&ctrl);
        t.setPureSignalActiveForTest(true);

        const PaProfile p = makeProfile8000d();
        t.setMox(false);
        t.setTune(false);
        t.setTwoToneActive(false);
        t.setPower(50);
        // Pre-set m_lastPower to match the new power.  Same value -> guard
        // false -> gate doesn't fire.
        t.setLastPower(50);

        t.setPowerUsingTargetDbm(p, Band::Band20m,
                                 /*bSetPower=*/true,
                                 /*bFromTune=*/false,
                                 /*bTwoTone=*/false);

        // Gate did NOT fire: ATT-on-TX value still 7.
        QCOMPARE(ctrl.attOnTxValue(), 7);
    }

    // =========================================================================
    // §8 ATT-on-TX gate fires on decrease only when _anddecreased=true
    // =========================================================================
    //
    // Thetis console.cs:46743 [v2.10.3.13]:
    //   if(new_pwr > _lastPower || _forceATTwhenPowerChangesWhenPSAon_anddecreased)
    //       SetupForm.ATTOnTX = 31;
    //
    // With _anddecreased=true, the inner guard is OR'd: power decrease
    // ALSO fires the gate.
    void attOnTxGate_firesOnDecreaseWhenAndDecreasedTrue() {
        TransmitModel t;
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBand(Band::Band20m);

        t.setStepAttenuatorController(&ctrl);
        t.setPureSignalActiveForTest(true);
        t.setForceAttwhenPowerChangesWhenPSAonAndDecreased(true);

        const PaProfile p = makeProfile8000d();
        t.setMox(false);
        t.setTune(false);
        t.setTwoToneActive(false);
        t.setPower(40);
        // Pre-set m_lastPower=80 so new_pwr=40 is a DECREASE.
        t.setLastPower(80);

        t.setPowerUsingTargetDbm(p, Band::Band20m,
                                 /*bSetPower=*/true,
                                 /*bFromTune=*/false,
                                 /*bTwoTone=*/false);

        // Gate fires: new_pwr (40) != lastPower (80), and _anddecreased=true.
        QCOMPARE(ctrl.attOnTxValue(), 31);
        QCOMPARE(t.lastPower(), 40);
    }

    // =========================================================================
    // §9 ATT-on-TX gate skips on decrease when _anddecreased=false
    // =========================================================================
    //
    // Thetis console.cs:46743 [v2.10.3.13] inner guard with _anddecreased=
    // false: only fires when new_pwr > _lastPower.  Decrease is ignored.
    // BUT m_lastPower IS still updated (outer block ran; we just skipped
    // the SetAttOnTx call).
    void attOnTxGate_skipsOnDecreaseWhenAndDecreasedFalse() {
        TransmitModel t;
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBand(Band::Band20m);
        ctrl.setAttOnTxValue(7);  // sentinel non-31

        t.setStepAttenuatorController(&ctrl);
        t.setPureSignalActiveForTest(true);
        // Default: forceAttwhenPowerChangesWhenPSAonAndDecreased=false
        QVERIFY(!t.forceAttwhenPowerChangesWhenPSAonAndDecreased());

        const PaProfile p = makeProfile8000d();
        t.setMox(false);
        t.setTune(false);
        t.setTwoToneActive(false);
        t.setPower(40);
        // Pre-set m_lastPower=80 so new_pwr=40 is a DECREASE.
        t.setLastPower(80);

        t.setPowerUsingTargetDbm(p, Band::Band20m,
                                 /*bSetPower=*/true,
                                 /*bFromTune=*/false,
                                 /*bTwoTone=*/false);

        // Inner guard fails: new_pwr (40) NOT > lastPower (80), and
        // _anddecreased=false.  setAttOnTxValue NOT called -> still 7.
        QCOMPARE(ctrl.attOnTxValue(), 7);
        // BUT outer block ran: m_lastPower updated to new_pwr (Thetis
        // console.cs:46746 _lastPower = new_pwr is OUTSIDE the inner if).
        QCOMPARE(t.lastPower(), 40);
    }

    // =========================================================================
    // §10 audioVolumeChanged signal payload matches computeAudioVolume
    // =========================================================================
    //
    // The signal payload should equal what computeAudioVolume returns
    // for the same (profile, band, sliderWatts).  Pure parity check.
    void audioVolumeChangedSignal_matchesComputeAudioVolume() {
        TransmitModel t;
        const PaProfile p = makeProfile8000d();
        t.setMox(false);
        t.setTune(false);
        t.setTwoToneActive(false);
        t.setPower(75);

        QSignalSpy spy(&t, &TransmitModel::audioVolumeChanged);

        const auto result = t.setPowerUsingTargetDbm(
            p, Band::Band80m,
            /*bSetPower=*/true, /*bFromTune=*/false, /*bTwoTone=*/false);

        QCOMPARE(spy.count(), 1);
        const double signalValue = spy.takeFirst().at(0).toDouble();
        // Direct call to computeAudioVolume for parity.
        const double directValue = t.computeAudioVolume(p, Band::Band80m, 75);
        QCOMPARE(signalValue, directValue);
        // And matches the result struct.
        QCOMPARE(result.audioVolume, directValue);
    }

    // =========================================================================
    // §11 K2GX integration regression
    // =========================================================================
    //
    // ANAN-8000DLE on Band 80m, txMode 1 (TUN), TuneSlider source, slider=50W.
    //   target_dbm = 10 * log10(50 * 1000) = 46.99
    //   gbb = 50.5 (8000DLE 80m gain)
    //   net = 46.99 - 50.5 = -3.51
    //   tv = sqrt(10^-0.351 * 0.05) = sqrt(0.0223) = 0.14930
    //   audio_volume = 0.14930 / 0.8 = 0.18663
    //
    // The tightest-cell K2GX-style scenario.  Tolerance 0.001.
    void k2gx_8000dle_80m_tune_slider_50w() {
        TransmitModel t;
        const PaProfile p = makeProfile8000d();
        t.setTune(true);
        t.setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
        t.setTunePowerForBand(Band::Band80m, 50);

        const auto result = t.setPowerUsingTargetDbm(
            p, Band::Band80m,
            /*bSetPower=*/true, /*bFromTune=*/true, /*bTwoTone=*/false);

        QCOMPARE(result.newPower, 50);
        QVERIFY(result.bConstrain);
        // audioVolume must match computeAudioVolume(...) for the same
        // inputs.  Pin the expected value.
        const double expected = t.computeAudioVolume(p, Band::Band80m, 50);
        QCOMPARE(result.audioVolume, expected);
        QVERIFY2(std::abs(result.audioVolume - 0.18663) < 0.001,
                 qPrintable(QStringLiteral("expected ~0.18663, got %1")
                                .arg(result.audioVolume, 0, 'f', 6)));
    }

    // =========================================================================
    // §12 No XVTR live wiring — sentinel fallback in computeAudioVolume
    // =========================================================================
    //
    // Band::XVTR has no factory PA-gain entry (PaProfile::getGainForBand
    // returns 100.0f sentinel for the XVTR slot in factory profiles).
    // computeAudioVolume catches gbb >= 99.5 and short-circuits to linear
    // fallback clamp(sliderWatts/100, 0, 1).  At slider=50, that's 0.5.
    // No crash, no NaN, no infinite output.
    void xvtr_band_uses_sentinel_fallback_no_crash() {
        TransmitModel t;
        const PaProfile p = makeProfile8000d();
        t.setMox(false);
        t.setTune(false);
        t.setTwoToneActive(false);
        t.setPower(50);

        const auto result = t.setPowerUsingTargetDbm(
            p, Band::XVTR,
            /*bSetPower=*/true, /*bFromTune=*/false, /*bTwoTone=*/false);

        QCOMPARE(result.newPower, 50);
        // XVTR factory gbb=100 (sentinel), so 50/100 = 0.5 linear fallback.
        QCOMPARE(result.audioVolume, 0.5);
        QVERIFY(std::isfinite(result.audioVolume));
    }

    // =========================================================================
    // §13 Edge case: power=0
    // =========================================================================
    //
    // Thetis console.cs:46749-46751 [v2.10.3.13]:
    //   if (new_pwr == 0) { Audio.RadioVolume = 0.0; ... }
    //
    // With m_power=0 and txMode 0, new_pwr=0 -> audio_volume=0.0 exactly
    // via computeAudioVolume's <=0 short-circuit.
    void powerZero_returnsZeroAudioVolume() {
        TransmitModel t;
        const PaProfile p = makeProfile8000d();
        t.setMox(false);
        t.setTune(false);
        t.setTwoToneActive(false);
        t.setPower(0);

        const auto result = t.setPowerUsingTargetDbm(
            p, Band::Band80m,
            /*bSetPower=*/true, /*bFromTune=*/false, /*bTwoTone=*/false);

        QCOMPARE(result.newPower, 0);
        QCOMPARE(result.audioVolume, 0.0);
    }

    // =========================================================================
    // §14 Null StepAttenuatorController is no-op (default state)
    // =========================================================================
    //
    // Without setStepAttenuatorController() being called, m_stepAttCtrl is
    // nullptr.  Even when the gate fires (PS-active + power change), no
    // hardware setter call is dispatched.  Verifies the no-op guard.
    void nullStepAttCtrl_isNoOpWhenGateFires() {
        TransmitModel t;
        // No setStepAttenuatorController call -> m_stepAttCtrl == nullptr.
        t.setPureSignalActiveForTest(true);

        const PaProfile p = makeProfile8000d();
        t.setMox(false);
        t.setTune(false);
        t.setTwoToneActive(false);
        t.setPower(60);
        t.setLastPower(-1);

        // Should not crash even though m_stepAttCtrl is nullptr.
        const auto result = t.setPowerUsingTargetDbm(
            p, Band::Band20m,
            /*bSetPower=*/true, /*bFromTune=*/false, /*bTwoTone=*/false);

        QCOMPARE(result.newPower, 60);
        // m_lastPower still updates (it's outside the controller call).
        QCOMPARE(t.lastPower(), 60);
    }

    // =========================================================================
    // §15 forceAttwhenPowerChangesWhenPSAon=false disables gate entirely
    // =========================================================================
    //
    // The Phase 3A property gate.  When the user disables this safety
    // toggle, the entire gate skips even when PS is active.
    void attOnTxGate_disabledByPSAonFlag() {
        TransmitModel t;
        StepAttenuatorController ctrl;
        ctrl.setTickTimerEnabled(false);
        ctrl.setBand(Band::Band20m);
        ctrl.setAttOnTxValue(7);  // sentinel non-31

        t.setStepAttenuatorController(&ctrl);
        t.setPureSignalActiveForTest(true);
        // Disable the property -> outer guard fails -> gate skipped.
        t.setForceAttwhenPowerChangesWhenPSAon(false);

        const PaProfile p = makeProfile8000d();
        t.setMox(false);
        t.setTune(false);
        t.setTwoToneActive(false);
        t.setPower(60);
        // Note: setForceAttwhenPowerChangesWhenPSAon(false) above already
        // reset lastPower to -1 (Thetis console.cs:29298 semantic).
        QCOMPARE(t.lastPower(), -1);

        t.setPowerUsingTargetDbm(p, Band::Band20m,
                                 /*bSetPower=*/true,
                                 /*bFromTune=*/false,
                                 /*bTwoTone=*/false);

        // Gate skipped entirely: ATT unchanged, lastPower unchanged.
        QCOMPARE(ctrl.attOnTxValue(), 7);
        QCOMPARE(t.lastPower(), -1);
    }

    // =========================================================================
    // §16 Constraint clamp on FIXED uses raw value (>100 OK)
    // =========================================================================
    //
    // Thetis console.cs:46689 [v2.10.3.13]: bConstrain = false on FIXED
    // bypasses ConstrainAValue.  Setup-page-fixed values can exceed the
    // PWR/TUN slider Max=100, but setTunePower clamps to [0, 100] at the
    // setter.  So this test verifies that bConstrain=false flag is set,
    // not that the raw value bypasses clamp (the clamp lives at the
    // setter, which is one layer up).
    void txMode1_fixed_bConstrainFalseFlagSet() {
        TransmitModel t;
        const PaProfile p = makeProfile8000d();
        t.setTune(true);
        t.setTuneDrivePowerSource(DrivePowerSource::Fixed);
        t.setTunePower(95);  // High but in [0,100].

        const auto result = t.setPowerUsingTargetDbm(
            p, Band::Band20m,
            /*bSetPower=*/true, /*bFromTune=*/false, /*bTwoTone=*/false);

        QCOMPARE(result.newPower, 95);
        QVERIFY(!result.bConstrain);
    }
};

QTEST_MAIN(TstTransmitModelSetPowerUsingDbm)
#include "tst_transmit_model_set_power_using_dbm.moc"
