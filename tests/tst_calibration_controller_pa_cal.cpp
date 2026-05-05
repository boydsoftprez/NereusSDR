// tests/tst_calibration_controller_pa_cal.cpp  (NereusSDR)
//
// TDD tests for CalibrationController::paCalProfile / setPaCalProfile /
// setPaCalPoint / calibratedFwdPowerWatts (Section 3.2 of the P1 full-
// parity epic — see docs/architecture/2026-05-02-p1-full-parity-plan.md).
//
// no-port-check: test file — Thetis cites in commentary only, behaviour
// is exercised through PaCalProfile (already-cited) and the new
// CalibrationController owners.
//
// Covers:
//   - setPaCalProfile updates state + emits paCalProfileChanged + changed,
//     once each.
//   - setPaCalProfile with an equal profile is a silent no-op (de-dup).
//   - setPaCalPoint(idx, w) updates the watts slot, emits
//     paCalPointChanged(idx, w) + changed(), once each.
//   - setPaCalPoint(0, ...) is a silent no-op (idx 0 is hard-coded 0.0
//     in PaCalProfile, never user-editable; production-safe rather than
//     fatal so a UI bug can't crash the calibration tab).
//   - setPaCalPoint(11, ...) and setPaCalPoint(-1, ...) are silent no-ops
//     (out-of-range guard).
//   - calibratedFwdPowerWatts forwards to PaCalProfile::interpolate for
//     two non-trivial inputs (Thetis console.cs:6691-6724 PowerKernel
//     [v2.10.3.13+501e3f51]).
//   - Round-trip: install Anan100 defaults, mutate cal points 1/5/10,
//     save, reload from a fresh controller, confirm boardClass + every
//     watts slot survives the round trip.

#include "core/CalibrationController.h"
#include "core/PaCalProfile.h"

#include <QtTest/QtTest>
#include <QSignalSpy>

class TstCalibrationControllerPaCal : public QObject {
    Q_OBJECT

private slots:
    void defaults_areNoneClass();
    void setPaCalProfile_emitsBothSignals();
    void setPaCalProfile_idempotentNoSignals();
    void setPaCalPoint_emitsBothSignals();
    void setPaCalPoint_idxZero_isNoOp();
    void setPaCalPoint_outOfRange_isNoOp();
    void calibratedFwdPowerWatts_forwardsToInterpolate();
    void persistence_roundTrip();
};

// ---------------------------------------------------------------------------
// Default-constructed controller has a None-class (empty) PA profile.
// ---------------------------------------------------------------------------
void TstCalibrationControllerPaCal::defaults_areNoneClass()
{
    NereusSDR::CalibrationController ctrl;
    QCOMPARE(ctrl.paCalProfile().boardClass, NereusSDR::PaCalBoardClass::None);
    // None-class watts is value-initialized to all zeros.
    for (float w : ctrl.paCalProfile().watts) {
        QCOMPARE(w, 0.0f);
    }
}

// ---------------------------------------------------------------------------
// setPaCalProfile updates state and fires paCalProfileChanged + changed
// exactly once each.
// ---------------------------------------------------------------------------
void TstCalibrationControllerPaCal::setPaCalProfile_emitsBothSignals()
{
    NereusSDR::CalibrationController ctrl;
    QSignalSpy profileSpy(&ctrl, &NereusSDR::CalibrationController::paCalProfileChanged);
    QSignalSpy changedSpy(&ctrl, &NereusSDR::CalibrationController::changed);

    const auto p = NereusSDR::PaCalProfile::defaults(NereusSDR::PaCalBoardClass::Anan100);
    ctrl.setPaCalProfile(p);

    QCOMPARE(profileSpy.count(), 1);
    QCOMPARE(changedSpy.count(), 1);
    QCOMPARE(ctrl.paCalProfile().boardClass, NereusSDR::PaCalBoardClass::Anan100);
    QCOMPARE(ctrl.paCalProfile().watts[10], 100.0f);
}

// ---------------------------------------------------------------------------
// Re-applying the same profile is a silent no-op — no signals fire.
// Mirrors the project convention used by every other CalibrationController
// setter (see e.g. setFreqCorrectionFactor's idempotency test).
// ---------------------------------------------------------------------------
void TstCalibrationControllerPaCal::setPaCalProfile_idempotentNoSignals()
{
    NereusSDR::CalibrationController ctrl;
    const auto p = NereusSDR::PaCalProfile::defaults(NereusSDR::PaCalBoardClass::Anan100);
    ctrl.setPaCalProfile(p);  // first install — fires signals

    QSignalSpy profileSpy(&ctrl, &NereusSDR::CalibrationController::paCalProfileChanged);
    QSignalSpy changedSpy(&ctrl, &NereusSDR::CalibrationController::changed);
    ctrl.setPaCalProfile(p);  // same value — must not fire
    QCOMPARE(profileSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 0);
}

// ---------------------------------------------------------------------------
// setPaCalPoint(5, 4.7f) writes watts[5], emits paCalPointChanged(5, 4.7f)
// and changed() once each.
// ---------------------------------------------------------------------------
void TstCalibrationControllerPaCal::setPaCalPoint_emitsBothSignals()
{
    NereusSDR::CalibrationController ctrl;
    ctrl.setPaCalProfile(
        NereusSDR::PaCalProfile::defaults(NereusSDR::PaCalBoardClass::Anan10));

    QSignalSpy pointSpy(&ctrl, &NereusSDR::CalibrationController::paCalPointChanged);
    QSignalSpy changedSpy(&ctrl, &NereusSDR::CalibrationController::changed);

    ctrl.setPaCalPoint(5, 4.7f);

    QCOMPARE(pointSpy.count(), 1);
    QCOMPARE(changedSpy.count(), 1);
    const auto args = pointSpy.takeFirst();
    QCOMPARE(args.at(0).toInt(), 5);
    QCOMPARE(args.at(1).toFloat(), 4.7f);
    QCOMPARE(ctrl.paCalProfile().watts[5], 4.7f);
}

// ---------------------------------------------------------------------------
// idx == 0 is reserved (PaCalProfile pins watts[0] to 0.0 — see PaCalProfile.h
// header comment "Index 0 is always 0 W"). The project convention is silent
// no-op rather than assertion: a flaky UI shouldn't be able to crash the
// calibration tab at runtime.
// ---------------------------------------------------------------------------
void TstCalibrationControllerPaCal::setPaCalPoint_idxZero_isNoOp()
{
    NereusSDR::CalibrationController ctrl;
    ctrl.setPaCalProfile(
        NereusSDR::PaCalProfile::defaults(NereusSDR::PaCalBoardClass::Anan10));

    QSignalSpy pointSpy(&ctrl, &NereusSDR::CalibrationController::paCalPointChanged);
    QSignalSpy changedSpy(&ctrl, &NereusSDR::CalibrationController::changed);

    ctrl.setPaCalPoint(0, 99.0f);

    QCOMPARE(pointSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 0);
    // watts[0] still hard-coded 0.0 from defaults().
    QCOMPARE(ctrl.paCalProfile().watts[0], 0.0f);
}

// ---------------------------------------------------------------------------
// idx outside [1..10] is silently rejected. Negative and >10 both no-op.
// ---------------------------------------------------------------------------
void TstCalibrationControllerPaCal::setPaCalPoint_outOfRange_isNoOp()
{
    NereusSDR::CalibrationController ctrl;
    ctrl.setPaCalProfile(
        NereusSDR::PaCalProfile::defaults(NereusSDR::PaCalBoardClass::Anan10));
    const auto before = ctrl.paCalProfile().watts;

    QSignalSpy pointSpy(&ctrl, &NereusSDR::CalibrationController::paCalPointChanged);
    QSignalSpy changedSpy(&ctrl, &NereusSDR::CalibrationController::changed);

    ctrl.setPaCalPoint(-1, 1.0f);
    ctrl.setPaCalPoint(11, 1.0f);
    ctrl.setPaCalPoint(99, 1.0f);

    QCOMPARE(pointSpy.count(), 0);
    QCOMPARE(changedSpy.count(), 0);
    QCOMPARE(ctrl.paCalProfile().watts, before);
}

// ---------------------------------------------------------------------------
// calibratedFwdPowerWatts is a thin forwarder to PaCalProfile::interpolate.
// Verify it matches for two non-trivial inputs against the same Anan100
// table that Thetis console.cs:6691-6724 PowerKernel [v2.10.3.13+501e3f51]
// would produce.
// ---------------------------------------------------------------------------
void TstCalibrationControllerPaCal::calibratedFwdPowerWatts_forwardsToInterpolate()
{
    NereusSDR::CalibrationController ctrl;
    auto p = NereusSDR::PaCalProfile::defaults(NereusSDR::PaCalBoardClass::Anan100);
    // Skew the table: at raw=15W report calibrated 25W (between 10W and 30W cal points).
    p.watts[1] = 5.0f;
    p.watts[2] = 25.0f;
    ctrl.setPaCalProfile(p);

    // Two probe inputs through the controller and the underlying profile;
    // both must agree.
    const float r1 = 15.0f;
    const float r2 = 75.0f;
    QCOMPARE(ctrl.calibratedFwdPowerWatts(r1), p.interpolate(r1));
    QCOMPARE(ctrl.calibratedFwdPowerWatts(r2), p.interpolate(r2));
}

// ---------------------------------------------------------------------------
// Persistence round-trip: install Anan100 defaults, mutate three cal points,
// save under a test MAC, then construct a fresh controller, set the same
// MAC, load, and assert every field round-tripped.
// ---------------------------------------------------------------------------
void TstCalibrationControllerPaCal::persistence_roundTrip()
{
    const QString testMac = QStringLiteral("AA:BB:CC:DD:EE:01");

    {
        NereusSDR::CalibrationController ctrl;
        ctrl.setMacAddress(testMac);
        auto p = NereusSDR::PaCalProfile::defaults(NereusSDR::PaCalBoardClass::Anan100);
        ctrl.setPaCalProfile(p);
        ctrl.setPaCalPoint(1, 9.5f);
        ctrl.setPaCalPoint(5, 47.5f);
        ctrl.setPaCalPoint(10, 99.25f);
        ctrl.save();
    }

    {
        NereusSDR::CalibrationController ctrl;
        ctrl.setMacAddress(testMac);
        ctrl.load();

        const auto p = ctrl.paCalProfile();
        QCOMPARE(p.boardClass, NereusSDR::PaCalBoardClass::Anan100);
        QCOMPARE(p.watts[0], 0.0f);
        QCOMPARE(p.watts[1], 9.5f);
        QCOMPARE(p.watts[2], 20.0f);  // unchanged factory default
        QCOMPARE(p.watts[3], 30.0f);
        QCOMPARE(p.watts[4], 40.0f);
        QCOMPARE(p.watts[5], 47.5f);
        QCOMPARE(p.watts[6], 60.0f);
        QCOMPARE(p.watts[7], 70.0f);
        QCOMPARE(p.watts[8], 80.0f);
        QCOMPARE(p.watts[9], 90.0f);
        QCOMPARE(p.watts[10], 99.25f);
    }
}

QTEST_MAIN(TstCalibrationControllerPaCal)
#include "tst_calibration_controller_pa_cal.moc"
