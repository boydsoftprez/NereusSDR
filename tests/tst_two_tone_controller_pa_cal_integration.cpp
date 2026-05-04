// =================================================================
// tests/tst_two_tone_controller_pa_cal_integration.cpp  (NereusSDR)
// =================================================================
//
// Phase 4 Agent 4B of issue #167 PA-cal safety hotfix — verifies
// TwoToneController routes its activation flow through the Phase 3C
// deep-parity wrapper TransmitModel::setPowerUsingTargetDbm
// (console.cs:46645-46762 [v2.10.3.13]).
//
// Covers:
//   §4B.1 — start() with DriveSlider → wrapper invoked with bTwoTone=true,
//           audioVolumeChanged emitted with the slider-value-driven volume.
//   §4B.2 — start() with TuneSlider → wrapper routes through
//           tunePowerForBand, audioVolumeChanged matches.
//   §4B.3 — start() with Fixed → wrapper routes through twoTonePower,
//           bConstrain=false, audioVolumeChanged matches.
//   §4B.4 — start() sets isTwoToneActive(true); stop() clears it.
//   §4B.5 — stop() does NOT itself call setPowerUsingTargetDbm
//           (RadioModel's powerChanged lambda picks up the restore).
//   §4B.6 — No PaProfileManager injected → graceful no-op,
//           audioVolumeChanged not emitted.
//   §4B.7 — Manager exists but no active profile → graceful no-op.
//
// =================================================================

// no-port-check: NereusSDR-original test file. All Thetis cites are in
// TwoToneController.h/.cpp at the call sites under test.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "core/MoxController.h"
#include "core/PaProfile.h"
#include "core/PaProfileManager.h"
#include "core/TwoToneController.h"
#include "core/TxChannel.h"
#include "core/WdspTypes.h"
#include "models/Band.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

namespace {

constexpr int kTxChannelId = 1;

// Minimal TxChannel subclass that accepts the TXPostGen* setter calls.
// We don't need to record them here (the existing
// tst_two_tone_controller covers TXPostGen ordering); we just need a
// concrete subclass to inject into the controller.
class StubTxChannel : public TxChannel {
public:
    explicit StubTxChannel(int id) : TxChannel(id) {}

    void setTxPostGenMode(int) override {}
    void setTxPostGenTTFreq1(double) override {}
    void setTxPostGenTTFreq2(double) override {}
    void setTxPostGenTTMag1(double) override {}
    void setTxPostGenTTMag2(double) override {}
    void setTxPostGenTTPulseToneFreq1(double) override {}
    void setTxPostGenTTPulseToneFreq2(double) override {}
    void setTxPostGenTTPulseMag1(double) override {}
    void setTxPostGenTTPulseMag2(double) override {}
    void setTxPostGenTTPulseFreq(int) override {}
    void setTxPostGenTTPulseDutyCycle(double) override {}
    void setTxPostGenTTPulseTransition(double) override {}
    void setTxPostGenTTPulseIQOut(bool) override {}
    void setTxPostGenRun(bool) override {}
};

// Build a "Default - ANAN_G2" PaProfile (not all-100 sentinel; gives a
// non-trivial audio_volume so the assertion has bite).
PaProfile makeProfileG2() {
    return PaProfile(QStringLiteral("Default - ANAN_G2"),
                     HPSDRModel::ANAN_G2, true);
}

// Configure a TwoToneController with the standard test plumbing.
void wireControllerForTest(TwoToneController& ctrl,
                           TransmitModel& tx,
                           TxChannel& tc,
                           MoxController& mox,
                           SliceModel& slice)
{
    mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
    ctrl.setTransmitModel(&tx);
    ctrl.setTxChannel(&tc);
    ctrl.setMoxController(&mox);
    ctrl.setSliceModel(&slice);
    ctrl.setSettleDelaysMs(0, 0);

    // Continuous mode + immediate Mag2 + USB (no invert).
    tx.setTwoTonePulsed(false);
    tx.setTwoToneFreq2Delay(0);
    slice.setDspMode(DSPMode::USB);
}

}  // namespace

class TestTwoToneControllerPaCalIntegration : public QObject
{
    Q_OBJECT

private slots:

    void init() {
        // Clear AppSettings before each test so PaProfileManager seeds
        // its factory defaults fresh.
        AppSettings::instance().clear();
    }

    // ── §4B.1: DriveSlider → audioVolume = computeAudioVolume(power) ─────
    void start_driveSliderSource_emitsAudioVolumeForPowerSlider()
    {
        TransmitModel tx;
        StubTxChannel tc(kTxChannelId);
        MoxController mox;
        SliceModel slice;
        TwoToneController ctrl;
        wireControllerForTest(ctrl, tx, tc, mox, slice);

        PaProfileManager mgr;
        mgr.setMacAddress(QStringLiteral("AA:BB:CC:DD:EE:01"));
        mgr.load(HPSDRModel::ANAN_G2);
        ctrl.setPaProfileManager(&mgr);

        tx.setTwoToneDrivePowerSource(DrivePowerSource::DriveSlider);
        tx.setPower(60);

        QSignalSpy audioSpy(&tx, &TransmitModel::audioVolumeChanged);

        ctrl.setActive(true);
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        QVERIFY(ctrl.isActive());
        QVERIFY(audioSpy.count() >= 1);

        // Compute expected via the same kernel.
        const PaProfile* profile = mgr.activeProfile();
        QVERIFY(profile != nullptr);
        const double expected = tx.computeAudioVolume(*profile,
                                                      Band::Band20m, 60);
        // The LAST emitted volume should equal expected (drive-source
        // routing landed on m_power=60).
        const double last = audioSpy.last().at(0).toDouble();
        QVERIFY(qFuzzyCompare(last, expected));
    }

    // ── §4B.2: TuneSlider → audioVolume = computeAudioVolume(tunePerBand) ─
    void start_tuneSliderSource_emitsAudioVolumeForTunePowerForBand()
    {
        TransmitModel tx;
        StubTxChannel tc(kTxChannelId);
        MoxController mox;
        SliceModel slice;
        TwoToneController ctrl;
        wireControllerForTest(ctrl, tx, tc, mox, slice);

        PaProfileManager mgr;
        mgr.setMacAddress(QStringLiteral("AA:BB:CC:DD:EE:02"));
        mgr.load(HPSDRModel::ANAN_G2);
        ctrl.setPaProfileManager(&mgr);

        tx.setTwoToneDrivePowerSource(DrivePowerSource::TuneSlider);
        tx.setTunePowerForBand(Band::Band20m, 25);

        QSignalSpy audioSpy(&tx, &TransmitModel::audioVolumeChanged);

        ctrl.setActive(true);
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        QVERIFY(ctrl.isActive());
        QVERIFY(audioSpy.count() >= 1);

        const PaProfile* profile = mgr.activeProfile();
        QVERIFY(profile != nullptr);
        const double expected = tx.computeAudioVolume(*profile,
                                                      Band::Band20m, 25);
        const double last = audioSpy.last().at(0).toDouble();
        QVERIFY(qFuzzyCompare(last, expected));
    }

    // ── §4B.3: Fixed → audioVolume = computeAudioVolume(twoTonePower) ────
    void start_fixedSource_emitsAudioVolumeForTwoTonePower()
    {
        TransmitModel tx;
        StubTxChannel tc(kTxChannelId);
        MoxController mox;
        SliceModel slice;
        TwoToneController ctrl;
        wireControllerForTest(ctrl, tx, tc, mox, slice);

        PaProfileManager mgr;
        mgr.setMacAddress(QStringLiteral("AA:BB:CC:DD:EE:03"));
        mgr.load(HPSDRModel::ANAN_G2);
        ctrl.setPaProfileManager(&mgr);

        tx.setTwoToneDrivePowerSource(DrivePowerSource::Fixed);
        tx.setTwoTonePower(17);
        tx.setPower(80);  // ignored by Fixed source

        QSignalSpy audioSpy(&tx, &TransmitModel::audioVolumeChanged);

        ctrl.setActive(true);
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        QVERIFY(ctrl.isActive());
        QVERIFY(audioSpy.count() >= 1);

        const PaProfile* profile = mgr.activeProfile();
        QVERIFY(profile != nullptr);
        const double expected = tx.computeAudioVolume(*profile,
                                                      Band::Band20m, 17);
        const double last = audioSpy.last().at(0).toDouble();
        QVERIFY(qFuzzyCompare(last, expected));
    }

    // ── §4B.4: start() sets isTwoToneActive(true); stop() clears it ──────
    void start_setsTwoToneActive_stopClearsIt()
    {
        TransmitModel tx;
        StubTxChannel tc(kTxChannelId);
        MoxController mox;
        SliceModel slice;
        TwoToneController ctrl;
        wireControllerForTest(ctrl, tx, tc, mox, slice);

        PaProfileManager mgr;
        mgr.setMacAddress(QStringLiteral("AA:BB:CC:DD:EE:04"));
        mgr.load(HPSDRModel::ANAN_G2);
        ctrl.setPaProfileManager(&mgr);

        QVERIFY(!tx.isTwoToneActive());

        ctrl.setActive(true);
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        QVERIFY(tx.isTwoToneActive());

        ctrl.setActive(false);
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        QVERIFY(!tx.isTwoToneActive());
    }

    // ── §4B.5: stop() does NOT call setPowerUsingTargetDbm itself ────────
    //
    // Per Phase 4B design: deactivation only flips m_twoToneActive=false.
    // RadioModel's powerChanged lambda is responsible for the next
    // setPowerUsingTargetDbm call once the user lets MOX drop. The
    // controller intentionally does NOT push its own audio_volume on
    // stop — it would emit a "ghost" volume against the now-stale
    // twoTone state.
    //
    // This test counts audioVolumeChanged emissions during the stop
    // path and confirms no emission attributable to TwoToneController.
    void stop_doesNotCallSetPowerUsingTargetDbm()
    {
        TransmitModel tx;
        StubTxChannel tc(kTxChannelId);
        MoxController mox;
        SliceModel slice;
        TwoToneController ctrl;
        wireControllerForTest(ctrl, tx, tc, mox, slice);

        PaProfileManager mgr;
        mgr.setMacAddress(QStringLiteral("AA:BB:CC:DD:EE:05"));
        mgr.load(HPSDRModel::ANAN_G2);
        ctrl.setPaProfileManager(&mgr);

        tx.setTwoToneDrivePowerSource(DrivePowerSource::DriveSlider);
        tx.setPower(50);

        ctrl.setActive(true);
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }
        QVERIFY(ctrl.isActive());

        // Capture only the stop-path emissions.
        QSignalSpy audioSpy(&tx, &TransmitModel::audioVolumeChanged);

        ctrl.setActive(false);
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        QVERIFY(!ctrl.isActive());
        // Phase 4B contract: stop() flips m_twoToneActive but does NOT
        // call setPowerUsingTargetDbm, so no audioVolumeChanged emission
        // attributable to the controller's stop path.
        QCOMPARE(audioSpy.count(), 0);
    }

    // ── §4B.6: no PaProfileManager → graceful no-op (no crash, no emit) ──
    void start_withoutPaProfileManager_doesNotEmitAudioVolume()
    {
        TransmitModel tx;
        StubTxChannel tc(kTxChannelId);
        MoxController mox;
        SliceModel slice;
        TwoToneController ctrl;
        wireControllerForTest(ctrl, tx, tc, mox, slice);

        // Intentionally do NOT call setPaProfileManager — controller
        // has nullptr.
        tx.setTwoToneDrivePowerSource(DrivePowerSource::DriveSlider);
        tx.setPower(40);

        QSignalSpy audioSpy(&tx, &TransmitModel::audioVolumeChanged);

        ctrl.setActive(true);
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        // Activation still proceeds (TXPostGen / MOX path is unchanged
        // by Phase 4B); the only difference is no audio_volume emission.
        QVERIFY(ctrl.isActive());
        QCOMPARE(audioSpy.count(), 0);
    }

    // ── §4B.7: manager exists but no active profile → graceful no-op ────
    void start_withManagerButNoActiveProfile_doesNotEmitAudioVolume()
    {
        TransmitModel tx;
        StubTxChannel tc(kTxChannelId);
        MoxController mox;
        SliceModel slice;
        TwoToneController ctrl;
        wireControllerForTest(ctrl, tx, tc, mox, slice);

        // PaProfileManager constructed but never load()ed — no
        // active profile yet.  activeProfile() returns nullptr.
        PaProfileManager mgr;
        ctrl.setPaProfileManager(&mgr);
        QVERIFY(mgr.activeProfile() == nullptr);

        tx.setTwoToneDrivePowerSource(DrivePowerSource::DriveSlider);
        tx.setPower(35);

        QSignalSpy audioSpy(&tx, &TransmitModel::audioVolumeChanged);

        ctrl.setActive(true);
        for (int i = 0; i < 10; ++i) { QCoreApplication::processEvents(); }

        QVERIFY(ctrl.isActive());
        QCOMPARE(audioSpy.count(), 0);
    }
};

QTEST_MAIN(TestTwoToneControllerPaCalIntegration)
#include "tst_two_tone_controller_pa_cal_integration.moc"
