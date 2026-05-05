// no-port-check: NereusSDR-original unit-test file.  The Thetis cite
// comments below document which upstream lines each assertion verifies;
// no upstream logic is ported in this file.
// =================================================================
// tests/tst_radio_model_drive_path.cpp  (NereusSDR)
// =================================================================
//
// Phase 4 Agent 4A of issue #167 — wire-byte / IQ-scalar topology +
// K2GX safety regression.
//
// Verifies that RadioModel routes the drive-slider lambda + TUNE-engage
// path through TransmitModel::setPowerUsingTargetDbm (Phase 3C deep-
// parity wrapper), composing:
//   wire_byte = clamp(int(audio_volume * 1.02 * 255), 0, 255)
//               From Thetis audio.cs:262-271 [v2.10.3.13]. NO SWR factor.
//   iq_gain   = audio_volume * swrProtect
//               From Thetis cmaster.cs:1115-1119 [v2.10.3.13]. SWR HERE.
//
// Coverage:
//   §1 K2GX regression: ANAN-8000DLE 80m TUN slider=50 -> wire byte ~= 49
//      (vs. pre-hotfix 127).  Hand-computed expectation pinned in test.
//   §2 Drive-slider lambda: ANAN-8000DLE 80m slider=100 -> wire byte ~= 68.
//   §3 HL2 sentinel preserves linear behaviour (gbb >= 99.5 short-circuit).
//   §4 HL2 6m uses dBm math (gbb=38.8 entry, hits the rail).
//   §5 SWR foldback applies to IQ ONLY, not wire byte.
//   §6 TUNE engagement triggers full math chain.
//   §7 No PaProfileManager / no active profile -> graceful no-op.
//   §8 PaTelemetryScaling::scaleFwdPowerWatts public lift parity.
//
// Source references (cite comments only — no Thetis logic translated):
//   audio.cs:262-271 [v2.10.3.13] — wire byte composition (NO SWR).
//   cmaster.cs:1115-1119 [v2.10.3.13] — IQ scalar composition (SWR HERE).
//   console.cs:46645-46762 [v2.10.3.13] — SetPowerUsingTargetDBM wrapper.
//   console.cs:46720-46751 [v2.10.3.13] — dBm math kernel.
//   clsHardwareSpecific.cs:655-683 [v2.10.3.13] — ANAN-8000D row (80m=50.5).
//   clsHardwareSpecific.cs:769-797 [v2.10.3.13-beta2] — HL2 row.
// =================================================================

#include <QtTest/QtTest>
#include <QObject>
#include <QSignalSpy>
#include <QCoreApplication>
#include <QThread>

#include <cmath>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "core/MoxController.h"
#include "core/PaProfile.h"
#include "core/PaProfileManager.h"
#include "core/PaTelemetryScaling.h"
#include "core/RadioConnection.h"
#include "core/StepAttenuatorController.h"
#include "core/TxChannel.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

// ── MockConnection ──────────────────────────────────────────────────────────
// Records setTxDrive() argument values.  Mirrors the pattern used by
// tst_radio_model_set_tune.cpp's MockConnection.
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    QList<int> txDriveLog;

    explicit MockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    // Pure-virtual stubs.
    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int level) override { txDriveLog.append(level); }
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
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
};

// ── Helpers ─────────────────────────────────────────────────────────────────

// processEvents helper: drains the Qt event loop for queued connections.
// QMetaObject::invokeMethod from the lambda routes setTxDrive through a
// queued connection on the connection thread; two passes cover any chained
// queued emissions.
static void pump()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

// Set up the model with a connected slice + injected mock connection so
// drive-slider / TUNE callsites have something to push wire bytes to.
static void setupModel(RadioModel& model, MockConnection*& mockConn,
                       HPSDRModel hwModel)
{
    AppSettings::instance().clear();
    model.setCapsForTest(/*hasAlex=*/false);
    model.setHpsdrModelForTest(hwModel);

    mockConn = new MockConnection();
    model.injectConnectionForTest(mockConn);

    // Make MoxController walk synchronous (relevant for §6 TUNE engage).
    model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);

    // Add an active slice on a known frequency (3.7 MHz = 80m band).
    // bandFromFrequency(3.7 MHz) -> Band80m, which matches the K2GX repro.
    model.addSlice();
    SliceModel* slice = model.activeSlice();
    Q_ASSERT(slice != nullptr);
    slice->setFrequency(3700000.0);
    slice->setDspMode(DSPMode::USB);
    model.setLastBandForTest(Band::Band80m);

    // Seed PaProfileManager with the right factory profile so the call
    // sites can resolve activeProfile() during setPowerUsingTargetDbm.
    if (PaProfileManager* pm = model.paProfileManager()) {
        pm->setMacAddress(QStringLiteral("AABBCCDDEEFF"));
        pm->load(hwModel);
    }
}

// ── Test class ──────────────────────────────────────────────────────────────
class TestRadioModelDrivePath : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { AppSettings::instance().clear(); }
    void init()         { AppSettings::instance().clear(); }
    void cleanup()      { AppSettings::instance().clear(); }

    // ── §1 K2GX regression (CRITICAL, ship-blocking) ─────────────────────────
    // ANAN-8000DLE, TUNE engaged at slider=50W on 80m (PA gain = 50.5 dB).
    // Pre-hotfix: wire_byte = clamp(int(255 * 0.5 * 1.0), 0, 255) = 127
    //              (=> radio drive ~50% of max => ~300+ W on a 200 W radio).
    // Post-hotfix: wire_byte = clamp(int(audio_volume * 1.02 * 255), 0, 255)
    //              where audio_volume comes from the dBm math kernel:
    //                target_dbm = 10*log10(50000) - 50.5 = 46.99 - 50.5 = -3.51
    //                volts = sqrt(10^-0.351 * 0.05) = sqrt(0.0223) = 0.1493
    //                volume = 0.1493/0.8 = 0.1866
    //                wire = int(0.1866 * 1.02 * 255) = int(48.55) = 48
    //              ~= 49 (tolerance ±1 wire byte for IEEE 754 rounding).
    //
    // Test path: route through the TUNE engagement at RadioModel.cpp:4280-4296
    // (Phase 4A rewrite) with TuneSlider drive source so tunePowerForBand
    // resolves to 50W.
    void k2gxRegression_anan8000d_80m_tune50()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        // Detach from RadioModel before the unique_ptr destroys conn so the
        // RadioModel dtor's teardownConnection() doesn't dereference a freed
        // pointer (mirrors tst_radio_model_set_tune.cpp pattern).
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        // TuneSlider source so tunePowerForBand(80m) = 50W is consulted.
        model.transmitModel().setTuneDrivePowerSource(
            DrivePowerSource::TuneSlider);
        model.transmitModel().setTunePowerForBand(Band::Band80m, 50);

        // Engage TUNE.  This walks the TUN-on path which now routes
        // through setPowerUsingTargetDbm.
        model.setTune(true);
        pump();

        // Find the wire-byte that the TUN-on path emitted.  setMox(true)
        // / hardwareFlipped fanout doesn't touch setTxDrive, so the first
        // (and only) TUN-on entry should be the dBm-target wire byte.
        QVERIFY2(!conn->txDriveLog.isEmpty(),
                 "TUN-on path did not push any wire byte");
        const int wireByte = conn->txDriveLog.last();

        // Pre-hotfix expectation (broken): 127.
        QVERIFY2(wireByte != 127,
                 qPrintable(QStringLiteral("K2GX regression: wire byte %1 "
                            "matches pre-hotfix linear formula (127). "
                            "Drive-byte path NOT routed through Phase 3C math.")
                            .arg(wireByte)));

        // Post-hotfix expectation: 48-49 (hand-computed).
        // ±1 tolerance for IEEE 754 rounding noise.
        QVERIFY2(wireByte >= 47 && wireByte <= 50,
                 qPrintable(QStringLiteral("K2GX regression: wire byte %1 "
                            "outside expected dBm-math range [47..50]")
                            .arg(wireByte)));
    }

    // ── §2 Drive-slider lambda at slider=100 on ANAN-8000D 80m ───────────────
    // PA gain = 50.5 dB, slider = 100W.
    //   target_dbm = 10*log10(100000) - 50.5 = 50 - 50.5 = -0.5
    //   volts = sqrt(10^-0.05 * 0.05) = sqrt(0.04457) = 0.2111
    //   volume = 0.2111/0.8 = 0.2639
    //   wire = int(0.2639 * 1.02 * 255) = int(68.65) = 68
    void driveSlider_anan8000d_80m_slider100()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        // Detach from RadioModel before the unique_ptr destroys conn so the
        // RadioModel dtor's teardownConnection() doesn't dereference a freed
        // pointer (mirrors tst_radio_model_set_tune.cpp pattern).
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        // Drive-slider lambda fires on setPower() emit. setPower(100)
        // routes through setPowerUsingTargetDbm.  Set a non-100 value
        // first so the setter actually emits powerChanged.
        model.transmitModel().setPower(50);
        pump();
        conn->txDriveLog.clear();

        model.transmitModel().setPower(100);
        pump();

        QVERIFY2(!conn->txDriveLog.isEmpty(),
                 "drive-slider lambda did not push wire byte");
        const int wireByte = conn->txDriveLog.last();

        // Hand-computed expectation: 68 (±1 for rounding).
        QVERIFY2(wireByte >= 67 && wireByte <= 69,
                 qPrintable(QStringLiteral("Drive-slider wire byte %1 outside "
                            "expected range [67..69] for ANAN-8000D 80m@100W")
                            .arg(wireByte)));
    }

    // ── §3 HL2 sentinel preserves linear behaviour ───────────────────────────
    // HL2 80m PA gain row contains 100.0 sentinel (mi0bot:484 "100 is no
    // output power").  computeAudioVolume short-circuits on gbb >= 99.5
    // and returns clamp(slider/100, 0, 1) linear fallback.
    //   audio_volume = 50/100 = 0.5
    //   wire = int(0.5 * 1.02 * 255) = int(130.05) = 130
    void hl2_sentinel_preservesLinearBehaviour()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::HERMESLITE);
        std::unique_ptr<MockConnection> connOwner(conn);
        // Detach from RadioModel before the unique_ptr destroys conn so the
        // RadioModel dtor's teardownConnection() doesn't dereference a freed
        // pointer (mirrors tst_radio_model_set_tune.cpp pattern).
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.transmitModel().setPower(20);  // seed
        pump();
        conn->txDriveLog.clear();

        model.transmitModel().setPower(50);
        pump();

        QVERIFY(!conn->txDriveLog.isEmpty());
        const int wireByte = conn->txDriveLog.last();

        // Linear fallback: 50/100 = 0.5 -> wire = int(0.5 * 1.02 * 255) = 130
        QVERIFY2(wireByte >= 129 && wireByte <= 131,
                 qPrintable(QStringLiteral("HL2 sentinel-band wire byte %1 "
                            "not at linear-fallback ~130 (50%% slider)")
                            .arg(wireByte)));
    }

    // ── §4 HL2 6m uses dBm math (gbb=38.8 row entry) ─────────────────────────
    // HL2 6m PA gain = 38.8 dB (real entry, NOT sentinel).
    //   target_dbm = 10*log10(100000) - 38.8 = 50 - 38.8 = 11.2
    //   volts = sqrt(10^1.12 * 0.05) = sqrt(0.6592) = 0.8119
    //   volume = min(0.8119/0.8, 1.0) = min(1.015, 1.0) = 1.0
    //   wire = clamp(int(1.0 * 1.02 * 255), 0, 255) = clamp(260, 0, 255) = 255
    // HL2 6m hits the rail at 100W slider per Phase 3B's audit.
    void hl2_6m_usesDbmMath_hitsRail()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::HERMESLITE);
        std::unique_ptr<MockConnection> connOwner(conn);
        // Detach from RadioModel before the unique_ptr destroys conn so the
        // RadioModel dtor's teardownConnection() doesn't dereference a freed
        // pointer (mirrors tst_radio_model_set_tune.cpp pattern).
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        // Move slice to 6m (50.5 MHz = Band6m).
        SliceModel* slice = model.activeSlice();
        slice->setFrequency(50500000.0);

        model.transmitModel().setPower(20);
        pump();
        conn->txDriveLog.clear();

        model.transmitModel().setPower(100);
        pump();

        QVERIFY(!conn->txDriveLog.isEmpty());
        const int wireByte = conn->txDriveLog.last();

        // 6m at 100W on HL2 hits the rail (audio_volume clamped to 1.0).
        QCOMPARE(wireByte, 255);
    }

    // ── §5 SWR foldback applies to IQ ONLY, not wire byte ────────────────────
    // ANAN-8000D 80m at slider=100 with swrProtectFactor=0.5.
    //   audio_volume from Phase 3C: ~0.2639 (matches §2 path).
    //   wire_byte = int(0.2639 * 1.02 * 255) = 68 (UNCHANGED from §2).
    //   iq_gain   = 0.2639 * 0.5 = 0.1319 (HALVED).
    //
    // Verifies the MW0LGE-canonical topology: SWR foldback hits the IQ
    // scalar, never the wire byte.
    void swrFoldback_appliesToIqNotWireByte()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        // Detach from RadioModel before the unique_ptr destroys conn so the
        // RadioModel dtor's teardownConnection() doesn't dereference a freed
        // pointer (mirrors tst_radio_model_set_tune.cpp pattern).
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        // Inject a real TxChannel so we can spy on setTxFixedGain via
        // lastFixedGainForTest.  TxChannel(1) constructs cleanly without
        // WDSP (HAVE_WDSP guards the actual fexchange2/setTXA calls);
        // setTxFixedGain still updates m_lastFixedGain in test builds.
        TxChannel txCh(/*channelId=*/1);
        model.injectTxChannelForTest(&txCh);

        // Drive SWR factor to 0.5.
        model.transmitModel().setSwrProtectFactor(0.5f);

        model.transmitModel().setPower(20);
        pump();
        conn->txDriveLog.clear();

        model.transmitModel().setPower(100);
        pump();

        // Wire byte: same as §2 (~68; SWR factor does NOT affect it).
        QVERIFY(!conn->txDriveLog.isEmpty());
        const int wireByte = conn->txDriveLog.last();
        QVERIFY2(wireByte >= 67 && wireByte <= 69,
                 qPrintable(QStringLiteral("SWR foldback bled into wire byte: "
                            "got %1, expected ~68. Topology violation — "
                            "audio.cs:268 does NOT see SWR factor.")
                            .arg(wireByte)));

        // IQ gain: should be ~0.5 * audio_volume (~0.2639 * 0.5 = 0.1319).
        const double iqGain = txCh.lastFixedGainForTest();
        QVERIFY2(iqGain > 0.12 && iqGain < 0.145,
                 qPrintable(QStringLiteral("IQ gain %1 outside expected "
                            "[~0.13] for ANAN-8000D 80m@100W * SWR=0.5")
                            .arg(iqGain)));

        // Detach before scope-exit destroys the TxChannel.
        model.injectTxChannelForTest(nullptr);
    }

    // ── §6 TUNE engagement triggers full math chain ──────────────────────────
    // setTune(true) routes through Phase 3C wrapper with bFromTune=true,
    // bTwoTone=false.  Result: wire byte computed from per-band tune power
    // through the dBm-target math kernel.
    //
    // ANAN-8000D 80m, TuneSlider source, tunePowerForBand=25.
    //   target_dbm = 10*log10(25000) - 50.5 = 43.98 - 50.5 = -6.52
    //   volts = sqrt(10^-0.652 * 0.05) = sqrt(0.01115) = 0.1056
    //   volume = 0.1056/0.8 = 0.1320
    //   wire = int(0.1320 * 1.02 * 255) = int(34.34) = 34
    void tuneEngage_triggersFullMathChain()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        // Detach from RadioModel before the unique_ptr destroys conn so the
        // RadioModel dtor's teardownConnection() doesn't dereference a freed
        // pointer (mirrors tst_radio_model_set_tune.cpp pattern).
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.transmitModel().setTuneDrivePowerSource(
            DrivePowerSource::TuneSlider);
        model.transmitModel().setTunePowerForBand(Band::Band80m, 25);

        // Spy on audioVolumeChanged to verify the wrapper was called.
        QSignalSpy avSpy(&model.transmitModel(),
                         &TransmitModel::audioVolumeChanged);

        model.setTune(true);
        pump();

        // setPowerUsingTargetDbm always emits audioVolumeChanged when
        // bSetPower=true.  The TUN-on path passes bSetPower=true.
        QVERIFY2(avSpy.count() >= 1,
                 "TUN-on path did not invoke setPowerUsingTargetDbm");

        QVERIFY(!conn->txDriveLog.isEmpty());
        const int wireByte = conn->txDriveLog.last();
        QVERIFY2(wireByte >= 33 && wireByte <= 35,
                 qPrintable(QStringLiteral("TUN-on wire byte %1 outside "
                            "expected range [33..35] for "
                            "ANAN-8000D 80m TuneSlider=25W")
                            .arg(wireByte)));
    }

    // ── §7 No PaProfileManager / no active profile -> graceful no-op ─────────
    // Without a loaded profile, the lambda + TUN handler must early-return
    // without crashing or pushing wire bytes.
    void noActiveProfile_gracefulNoOp()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        AppSettings::instance().clear();
        model.setCapsForTest(/*hasAlex=*/false);
        model.setHpsdrModelForTest(HPSDRModel::ANAN8000D);

        conn = new MockConnection();
        model.injectConnectionForTest(conn);
        std::unique_ptr<MockConnection> connOwner(conn);
        // Detach from RadioModel before the unique_ptr destroys conn so the
        // RadioModel dtor's teardownConnection() doesn't dereference a freed
        // pointer (mirrors tst_radio_model_set_tune.cpp pattern).
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        slice->setFrequency(3700000.0);

        // Deliberately do NOT seed paProfileManager so activeProfile() == nullptr.
        Q_ASSERT(model.paProfileManager()->activeProfile() == nullptr);

        model.transmitModel().setPower(50);
        pump();

        // No wire bytes pushed (the lambda early-returned on null profile).
        QCOMPARE(conn->txDriveLog.size(), 0);
    }

    // ── §8 PaTelemetryScaling::scaleFwdPowerWatts public lift parity ─────────
    // The private scaleFwdPowerWatts helper in RadioModel.cpp is lifted to
    // PaTelemetryScaling.cpp (Phase 1B).  Verify that the public function
    // produces the same outputs for known inputs.
    void scaleFwdPowerWatts_lift_parity()
    {
        // Known input/output pairs computed by hand from the per-board
        // bridge_volt / refvoltage / adc_cal_offset triplet:
        //
        // ANAN8000D: bridge=0.08, refV=5.0, cal=18.
        //   raw=2048: volts = (2048 - 18)/4095 * 5.0 = 2.4787 V
        //             watts = 2.4787^2 / 0.08 = 76.79 W
        const double w8000_2048 =
            scaleFwdPowerWatts(HPSDRModel::ANAN8000D, 2048);
        QVERIFY2(w8000_2048 > 76.0 && w8000_2048 < 78.0,
                 qPrintable(QStringLiteral("ANAN8000D 2048 -> %1 W (expected ~77)")
                            .arg(w8000_2048)));

        // ANAN_G2: bridge=0.12, refV=5.0, cal=32.
        //   raw=2037: volts = (2037 - 32)/4095 * 5.0 = 2.4481 V
        //             watts = 2.4481^2 / 0.12 = 49.94 W
        const double wG2_2037 =
            scaleFwdPowerWatts(HPSDRModel::ANAN_G2, 2037);
        QVERIFY2(wG2_2037 > 49.0 && wG2_2037 < 51.0,
                 qPrintable(QStringLiteral("ANAN_G2 2037 -> %1 W (expected ~50)")
                            .arg(wG2_2037)));

        // Below cal_offset clamps to zero.
        QCOMPARE(scaleFwdPowerWatts(HPSDRModel::ANAN8000D, 0), 0.0);
        QCOMPARE(scaleFwdPowerWatts(HPSDRModel::ANAN_G2, 5), 0.0);
    }
};

QTEST_MAIN(TestRadioModelDrivePath)
#include "tst_radio_model_drive_path.moc"
