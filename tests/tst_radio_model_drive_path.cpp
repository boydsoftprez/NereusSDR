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
// Upstream tags preserved: //MW0LGE (from cited cmaster.cs:1114) [v2.10.3.15]
//
// Coverage:
//   §1 K2GX regression: ANAN-8000DLE 80m TUN slider=50 -> wire byte ~= 49
//      (vs. pre-hotfix 127).  Hand-computed expectation pinned in test.
//   §2 Drive-slider lambda: ANAN-8000DLE 80m slider=100 -> wire byte ~= 68.
//   §3 HL2 HF band uses mi0bot audioVolume formula (gbb=100, audio_volume~0.533).
//   §4 HL2 6m uses mi0bot audioVolume formula (gbb=38.8, below rail ~107).
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
#include <limits>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "core/MoxController.h"
#include "core/PaProfile.h"
#include "core/PaProfileManager.h"
#include "core/PaTelemetryScaling.h"
#include "core/RadioConnection.h"
#include "core/StepAttenuatorController.h"
#include "core/TxChannel.h"
#include "core/TwoToneController.h"
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

    // ── §3 HL2 HF band uses mi0bot audio-volume formula ─────────────────────
    // Task 5 of #175 ported mi0bot's HL2 formula BEFORE the gbb >= 99.5
    // sentinel, so HL2 HF bands now use:
    //   audio_volume = clamp((sliderWatts * gbb/100) / 93.75, 0, 1)
    // From mi0bot-Thetis console.cs:47775-47778 [v2.10.3.13-beta2].
    //
    // HL2 80m gbb = 100.0f (sentinel value from the mi0bot gain table, but
    // mi0bot's own formula consumes it directly rather than short-circuiting).
    //
    //   audio_volume = (50 * 100.0/100.0) / 93.75 = 50.0 / 93.75 = 0.5333
    //   wire = int(0.5333 * 1.02 * 255) = int(138.82) = 138
    void hl2_hf_uses_mi0bot_audioVolume()
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

        // mi0bot formula: (50 * 100.0/100.0) / 93.75 = 0.5333
        // wire = int(0.5333 * 1.02 * 255) = 138  (±1 for IEEE 754 rounding)
        // From mi0bot-Thetis console.cs:47775-47778 [v2.10.3.13-beta2].
        QVERIFY2(wireByte >= 137 && wireByte <= 139,
                 qPrintable(QStringLiteral("HL2 HF-band wire byte %1 "
                            "not at mi0bot-formula ~138 (50W, gbb=100)")
                            .arg(wireByte)));
    }

    // ── §4 HL2 6m uses mi0bot audio-volume formula (below rail) ─────────────
    // Task 5 of #175 ported mi0bot's HL2 formula BEFORE the gbb >= 99.5
    // sentinel.  HL2 6m gbb = 38.8f (real entry, not sentinel), and with
    // mi0bot's formula the 6m band no longer rails at 100W — it produces a
    // well-below-1.0 audio_volume:
    //
    //   audio_volume = clamp((sliderWatts * gbb/100) / 93.75, 0, 1)
    //                = (100 * 38.8/100) / 93.75 = 38.8 / 93.75 = 0.4139
    //   wire = int(0.4139 * 1.02 * 255) = int(107.58) = 107
    //
    // From mi0bot-Thetis console.cs:47775-47778 [v2.10.3.13-beta2].
    // The old dBm/target_volts path (which would have railed at 255) is no
    // longer reached for HL2 — mi0bot's branch intercepts first.
    void hl2_6m_uses_mi0bot_audioVolume_belowRail()
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

        // mi0bot formula: (100 * 38.8/100) / 93.75 = 0.4139
        // wire = int(0.4139 * 1.02 * 255) = 107  (±1 for IEEE 754 rounding)
        // From mi0bot-Thetis console.cs:47775-47778 [v2.10.3.13-beta2].
        QVERIFY2(wireByte >= 106 && wireByte <= 108,
                 qPrintable(QStringLiteral("HL2 6m wire byte %1 outside "
                            "expected mi0bot-formula range [106..108] "
                            "(100W, gbb=38.8, audio_volume~0.414)")
                            .arg(wireByte)));
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

        // Issue #202 deep-fix — SWR topology corrected to match Thetis.
        //
        // From Thetis NetworkIO.cs:201-211 [v2.10.3.13]:
        //   int i = (int)(255 * f * _swr_protect);   // WIRE BYTE sees SWR.
        // From Thetis cmaster.cs:1115-1119 [v2.10.3.13]:
        // Upstream tags preserved: //MW0LGE (from cited cmaster.cs:1114) [v2.10.3.15]
        //   double level = Audio.RadioVolume * Audio.HighSWRScale;
        // where HighSWRScale is set to 1.0 once at console.cs:29194 and
        // never reassigned in baseline Thetis — IQ side is no-op.  So
        // the SWR factor multiplies the wire byte, NOT the IQ gain.
        //
        // For ANAN-8000D 80m @ 100W with swrProtect=0.5:
        //   audio_volume ≈ 0.2639 (gbb=50.5; computeAudioVolume).
        //   wire = int(0.2639 * 1.02 * 0.5 * 255) ≈ 34 (HALVED by SWR).
        //   iqGain = audio_volume = 0.2639 (no SWR factor).
        QVERIFY(!conn->txDriveLog.isEmpty());
        const int wireByte = conn->txDriveLog.last();
        QVERIFY2(wireByte >= 33 && wireByte <= 35,
                 qPrintable(QStringLiteral("Wire byte %1 outside expected "
                            "[~34] for ANAN-8000D 80m@100W with SWR=0.5. "
                            "Thetis NetworkIO.cs:210 puts _swr_protect on "
                            "the wire byte (audio.cs:268 -> SetOutputPower "
                            "with the 1.02-scaled value).")
                            .arg(wireByte)));

        // IQ gain: pure audio_volume per Thetis cmaster.cs:1117 with
        // HighSWRScale=1.0 baseline (~0.2639).
        const double iqGain = txCh.lastFixedGainForTest();
        QVERIFY2(iqGain > 0.25 && iqGain < 0.28,
                 qPrintable(QStringLiteral("IQ gain %1 outside expected "
                            "[~0.264] for ANAN-8000D 80m@100W (pure "
                            "audio_volume; no SWR factor on IQ side).")
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
    // ── Job B item 4: an on-the-air gain edit during TUNE or two-tone ────────
    // Thetis nudPAProfileGain_ValueChanged runs `console.PWR = console.PWR`
    // (setup.cs:23351-23352 [v2.10.3.15]); the PWR setter runs ptbPWR_Scroll
    // (console.cs:18437-18448), whose SetPowerUsingTargetDBM takes txMode 1
    // while chkTUN is checked and 2 while chk2TONE is (console.cs:46724-46747).
    // So the tune or two-tone drive is recomputed with the new gain.
    static void setGain80m(RadioModel& model, float gain)
    {
        PaProfileManager* pm = model.paProfileManager();
        PaProfile edited = *pm->activeProfile();
        edited.setGainForBand(Band::Band80m, gain);
        QVERIFY(pm->saveProfile(pm->activeProfileName(), edited));
    }

    void onAirGainEditDuringTune_recomputesTuneDrive()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.transmitModel().setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
        model.transmitModel().setTunePowerForBand(Band::Band80m, 50);
        model.setTune(true);
        pump();
        QVERIFY(!conn->txDriveLog.isEmpty());
        const int before = conn->txDriveLog.last();
        QVERIFY2(before >= 47 && before <= 50, qPrintable(QString::number(before)));

        // 80m gain 50.5 -> 40.5 dB at 50 W tune:
        //   target_dbm = 46.99 - 40.5 = 6.49; volts = sqrt(10^0.649 * 0.05) = 0.4721
        //   volume = 0.590; wire = int(0.590 * 1.02 * 255) = 153
        setGain80m(model, 40.5f);
        conn->txDriveLog.clear();
        model.applyPaEditOnAir(RadioModel::PaProfileAction::SetGain, -1);
        pump();
        QVERIFY2(!conn->txDriveLog.isEmpty(), "gain edit during TUNE pushed no drive");
        const int after = conn->txDriveLog.last();
        QVERIFY2(after >= 151 && after <= 155, qPrintable(QString::number(after)));
        QVERIFY(model.transmitModel().isTune());

        model.setTune(false);
        pump();
    }

    void onAirGainEditDuringTwoTone_recomputesTwoToneDrive()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.transmitModel().setTwoToneDrivePowerSource(DrivePowerSource::DriveSlider);
        model.transmitModel().setPower(50);
        pump();
        model.transmitModel().setPower(100);
        pump();
        model.transmitModel().setTwoToneActive(true);
        QVERIFY(model.paOnAirNow());

        // 80m gain 50.5 -> 40.5 dB at the 100 W drive slider:
        //   target_dbm = 50 - 40.5 = 9.5; volts = sqrt(10^0.95 * 0.05) = 0.6676
        //   volume = 0.8345; wire = int(0.8345 * 1.02 * 255) = 217
        setGain80m(model, 40.5f);
        conn->txDriveLog.clear();
        model.applyPaEditOnAir(RadioModel::PaProfileAction::SetGain, -1);
        pump();
        QVERIFY2(!conn->txDriveLog.isEmpty(), "gain edit during two-tone pushed no drive");
        const int after = conn->txDriveLog.last();
        QVERIFY2(after >= 215 && after <= 219, qPrintable(QString::number(after)));

        model.transmitModel().setTwoToneActive(false);
        pump();
    }

    // ── Job B item 3: the live apply follows MOX, not the TX to RX flush ─────
    // Thetis nudAdjustGain_ValueChanged gates on `if (console.MOX)`
    // (setup.cs:24210-24222 [v2.10.3.15]); OnMoxChangeHandler unlocks the
    // page as MOX drops (setup.cs:23826-23834). An edit made after MOX drops
    // but before the controller is back in Rx is an off-air edit.
    void paEditInTheUnkeyFlush_isAnOffAirEdit()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        MoxController* mox = model.moxController();
        mox->setMoxCheck({});
        mox->setMox(true);
        pump();
        QVERIFY(model.paOnAirNow());
        const int band40 = static_cast<int>(Band::Band40m);
        QCOMPARE(model.paOnAirEditRefusal(false, band40, true),
                 RadioModel::paOnAirLockedReason());

        mox->setMox(false); // no pump: the TX to RX handover has not finished
        QVERIFY(!model.mox());
        QVERIFY(mox->state() != MoxState::Rx);
        QVERIFY(model.stationOnAirRefusal(nullptr));
        QVERIFY(!model.paOnAirNow());
        QVERIFY(model.paOnAirEditRefusal(false, band40, false).isEmpty());
        QVERIFY(model.paOnAirEditRefusal(true, band40, false).isEmpty());

        setGain80m(model, 40.5f);
        conn->txDriveLog.clear();
        model.applyPaEditOnAir(RadioModel::PaProfileAction::SetGain, -1);
        QVERIFY2(conn->txDriveLog.isEmpty(), "an edit in the unkey flush moved the drive");
        pump();
        QCOMPARE(mox->state(), MoxState::Rx);
    }

    // ── The PWR slider during TUNE and two-tone ──────────────────────────────
    // Thetis PWR setter (console.cs:18437-18448 [v2.10.3.15]) runs
    // ptbPWR_Scroll (:28682-28693), whose setPowerFromDriveSlider
    // (:46710-46716) is SetPowerUsingTargetDBM(..., true, false, false). That
    // takes txMode 1 while chkTUN is checked and 2 while chk2TONE is
    // (:46724-46747), so moving PWR with the drive-slider source recomputes
    // the tune or two-tone drive.
    //   100 W at the 80m gain 50.5 dB: target_dbm = 50 - 50.5 = -0.5;
    //   volts = sqrt(10^-0.05 * 0.05) = 0.2111; volume = 0.2639;
    //   wire = int(0.2639 * 1.02 * 255) = 68
    void pwrSliderDuringTune_recomputesTuneDrive()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.transmitModel().setTuneDrivePowerSource(DrivePowerSource::DriveSlider);
        model.transmitModel().setPower(50);
        pump();
        model.setTune(true);
        pump();
        QVERIFY(model.transmitModel().isTune());

        conn->txDriveLog.clear();
        model.transmitModel().setPower(100);
        pump();
        QVERIFY2(!conn->txDriveLog.isEmpty(), "PWR during TUNE pushed no drive");
        const int after = conn->txDriveLog.last();
        QVERIFY2(after >= 66 && after <= 70, qPrintable(QString::number(after)));

        model.setTune(false);
        pump();
    }

    void pwrSliderDuringTwoTone_recomputesTwoToneDrive()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.transmitModel().setTwoToneDrivePowerSource(DrivePowerSource::DriveSlider);
        model.transmitModel().setPower(50);
        pump();
        model.transmitModel().setTwoToneActive(true);

        conn->txDriveLog.clear();
        model.transmitModel().setPower(100);
        pump();
        QVERIFY2(!conn->txDriveLog.isEmpty(), "PWR during two-tone pushed no drive");
        const int after = conn->txDriveLog.last();
        QVERIFY2(after >= 66 && after <= 70, qPrintable(QString::number(after)));

        model.transmitModel().setTwoToneActive(false);
        pump();
    }

    // ── The PWR slider saves the per-band power in every mode ────────────────
    // Thetis ptbPWR_Scroll (console.cs:28692-28693 [v2.10.3.15]) writes
    //   power_by_band[(int)_tx_band] = ptbPWR.Value;
    // after the drive is set, whatever the transmit mode, and with or without
    // a radio. The saved key is unchanged (hardware/<mac>/powerByBand/<band>).
    void pwrSliderSavesPowerByBandDuringTuneAndTwoTone()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });
        const QString mac = QStringLiteral("AABBCCDDEEFF");
        model.transmitModel().loadFromSettings(mac);
        const QString key80 = QStringLiteral("hardware/%1/powerByBand/%2")
                                  .arg(mac, bandKeyName(Band::Band80m));

        model.transmitModel().setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
        model.transmitModel().setTunePowerForBand(Band::Band80m, 30);
        model.setTune(true);
        pump();
        model.transmitModel().setPower(70);
        pump();
        QCOMPARE(model.transmitModel().powerForBand(Band::Band80m), 70);
        QCOMPARE(AppSettings::instance().value(key80).toString(), QStringLiteral("70"));
        model.setTune(false);
        pump();

        model.transmitModel().setTwoToneDrivePowerSource(DrivePowerSource::Fixed);
        model.transmitModel().setTwoToneActive(true);
        model.transmitModel().setPower(40);
        pump();
        QCOMPARE(model.transmitModel().powerForBand(Band::Band80m), 40);
        QCOMPARE(AppSettings::instance().value(key80).toString(), QStringLiteral("40"));
        model.transmitModel().setTwoToneActive(false);
        pump();
    }

    void pwrSliderSavesPowerByBandWithoutARadio()
    {
        AppSettings::instance().clear();
        RadioModel model;
        model.addSlice();
        model.activeSlice()->setFrequency(7100000.0);
        const QString mac = QStringLiteral("AABBCCDDEEFF");
        model.transmitModel().loadFromSettings(mac);

        model.transmitModel().setPower(65);
        pump();
        QCOMPARE(model.transmitModel().powerForBand(Band::Band40m), 65);
        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("hardware/%1/powerByBand/%2")
                                .arg(mac, bandKeyName(Band::Band40m)))
                     .toString(),
                 QStringLiteral("65"));
    }

    // A Core with no window loads the transmit band's stored power on a
    // band change (Thetis TXBand setter, console.cs:17511-17545
    // [v2.10.3.15]: power_by_band[old] = PWR; PWR = power_by_band[new]).
    void headlessBandChange_loadsStoredPower()
    {
        AppSettings::instance().clear();
        RadioModel model;
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setFrequency(7100000.0);
        pump();
        TransmitModel& tx = model.transmitModel();
        tx.setPowerForBand(Band::Band20m, 33);
        tx.setPower(60);
        pump();
        QCOMPARE(tx.powerForBand(Band::Band40m), 60);

        slice->setFrequency(14200000.0);
        pump();
        QCOMPARE(tx.power(), 33);
        QCOMPARE(tx.powerForBand(Band::Band40m), 60);
        QCOMPARE(tx.powerForBand(Band::Band20m), 33);

        slice->setFrequency(7150000.0);
        pump();
        QCOMPARE(tx.power(), 60);

        // A retune inside the band leaves PWR where it is.
        tx.setPower(61);
        slice->setFrequency(7200000.0);
        pump();
        QCOMPARE(tx.power(), 61);
        QCOMPARE(tx.powerForBand(Band::Band40m), 61);
    }

    // The start-of-transmit restore saves PWR into the transmit band's slot
    // (mi0bot console.cs:30272 [v2.10.3.13-beta2] calls ptbPWR_Scroll) and
    // no other band's; a retune while keyed does not change the band.
    void moxEdgeSave_doesNotOverwriteAnotherBand()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });
        TransmitModel& tx = model.transmitModel();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);

        // setupModel leaves the slice on 80 m.
        tx.setPowerForBand(Band::Band40m, 70);
        tx.setPower(40);
        pump();
        QCOMPARE(tx.powerForBand(Band::Band80m), 40);

        slice->setFrequency(7100000.0);
        pump();
        QCOMPARE(tx.power(), 70);

        // Something wrote the 40 m slot behind PWR's back; the start of
        // transmit puts PWR back into it.
        tx.setPowerForBand(Band::Band40m, 20);
        MoxController* mox = model.moxController();
        mox->setMoxCheck({});
        mox->setMox(true);
        pump();
        QCOMPARE(tx.powerForBand(Band::Band40m), 70);
        QCOMPARE(tx.powerForBand(Band::Band80m), 40);
        QCOMPARE(tx.power(), 70);

        // MW0LGE no band change on TX: a retune while keyed keeps PWR and
        // leaves the new band's slot alone.
        const int band20Before = tx.powerForBand(Band::Band20m);
        slice->setFrequency(14200000.0);
        pump();
        QCOMPARE(tx.power(), 70);
        QCOMPARE(tx.powerForBand(Band::Band20m), band20Before);
        QCOMPARE(tx.powerForBand(Band::Band40m), 70);

        mox->setMox(false);
        pump();
    }

    // A Core whose first transmit band has a stored power loads it (the
    // TXBand setter's initializing pass, console.cs:17511-17545
    // [v2.10.3.15]); the start-of-transmit save then writes that value back,
    // never a PWR that was not loaded for the band.
    void firstTransmitBand_loadsStoredPower_keyUnkeyKeepsIt()
    {
        AppSettings::instance().clear();
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/false);
        model.setHpsdrModelForTest(HPSDRModel::ANAN8000D);
        auto* conn = new MockConnection();
        std::unique_ptr<MockConnection> connOwner(conn);
        model.injectConnectionForTest(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        if (PaProfileManager* pm = model.paProfileManager()) {
            pm->setMacAddress(QStringLiteral("AABBCCDDEEFF"));
            pm->load(HPSDRModel::ANAN8000D);
        }
        TransmitModel& tx = model.transmitModel();
        tx.setPowerForBand(Band::Band20m, 30);

        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        QCOMPARE(bandFromFrequency(slice->frequency()), Band::Band20m);
        pump();
        QCOMPARE(tx.power(), 30);

        MoxController* mox = model.moxController();
        mox->setMoxCheck({});
        mox->setMox(true);
        pump();
        QCOMPARE(tx.power(), 30);
        QCOMPARE(tx.powerForBand(Band::Band20m), 30);
        mox->setMox(false);
        pump();
        QCOMPARE(tx.power(), 30);
        QCOMPARE(tx.powerForBand(Band::Band20m), 30);
    }

    // The TXBand setter returns while MOX (//[2.10.3.6]MW0LGE no band change
    // on TX fix), so TunePWR = tunePower_by_band[value] //MW0LGE_22b does not
    // run either: a retune while keyed keeps the transmit band's tune power.
    void keyedRetune_holdsTunePower()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });
        TransmitModel& tx = model.transmitModel();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        tx.setTunePowerForBand(Band::Band80m, 20);
        tx.setTunePowerForBand(Band::Band40m, 60);
        pump();
        QCOMPARE(tx.tunePowerForTxBand(), 20);

        MoxController* mox = model.moxController();
        mox->setMoxCheck({});
        mox->setMox(true);
        pump();
        slice->setFrequency(7100000.0);
        pump();
        QCOMPARE(tx.tunePowerForTxBand(), 20);
        mox->setMox(false);
        pump();
        QCOMPARE(tx.tunePowerForTxBand(), 20);

        // The next band change after unkey loads the new band's tune power.
        slice->setFrequency(7150000.0);
        pump();
        QCOMPARE(tx.tunePowerForTxBand(), 60);
    }

    // Setup's _adjustingBand moves only in OnTXBandChanged
    // (setup.cs:23836-23840 [v2.10.3.15]), which the TXBand setter raises;
    // the setter returns while MOX (//[2.10.3.6]MW0LGE no band change on TX
    // fix). nudPAProfileGain_ValueChanged runs console.PWR = console.PWR,
    // which drives with GainByBand(TXBand, ...). So the editable row and the
    // driving row stay the band keyed on, through a retune while keyed.
    void keyedRetune_holdsTheOnAirPaRow()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setFrequency(14200000.0);
        pump();
        QSignalSpy bandSpy(&model, &RadioModel::transmitBandChanged);
        model.transmitModel().setPower(100);
        pump();

        MoxController* mox = model.moxController();
        mox->setMoxCheck({});
        mox->setMox(true);
        pump();
        QVERIFY(model.paOnAirNow());
        slice->setFrequency(18100000.0);
        pump();
        QCOMPARE(bandSpy.count(), 0);

        const int band20 = static_cast<int>(Band::Band20m);
        const int band17 = static_cast<int>(Band::Band17m);
        QCOMPARE(model.paOnAirBandIndex(), band20);
        QVERIFY(model.paOnAirEditRefusal(false, band20, true).isEmpty());
        QCOMPARE(model.paOnAirEditRefusal(false, band17, true),
                 RadioModel::paOnAirLockedReason());

        // The 20 m gain edit moves the live drive.
        conn->txDriveLog.clear();
        PaProfileManager* pm = model.paProfileManager();
        PaProfile edited = *pm->activeProfile();
        edited.setGainForBand(Band::Band20m, edited.getGainForBand(Band::Band20m) - 10.0f);
        QVERIFY(pm->saveProfile(pm->activeProfileName(), edited));
        model.applyPaEditOnAir(RadioModel::PaProfileAction::SetGain, -1);
        pump();
        QVERIFY2(!conn->txDriveLog.isEmpty(), "the 20 m gain edit moved no drive");

        // After unkey the next band change moves the row.
        mox->setMox(false);
        pump();
        slice->setFrequency(18110000.0);
        pump();
        QCOMPARE(bandSpy.count(), 1);
        QCOMPARE(model.paOnAirBandIndex(), band17);
    }

    // Keys on 80 m, retunes the slice to 40 m while keyed and unkeys. The
    // TXBand setter returned while MOX (//[2.10.3.6]MW0LGE no band change on
    // TX fix), so _tx_band stays 80 m until the next band change. The 40 m
    // row gets a gain of its own so a drive on the wrong band shows.
    static void holdEightyWithSliceOnForty(RadioModel& model)
    {
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        PaProfileManager* pm = model.paProfileManager();
        PaProfile edited = *pm->activeProfile();
        edited.setGainForBand(Band::Band40m, 40.5f);
        QVERIFY(pm->saveProfile(pm->activeProfileName(), edited));

        MoxController* mox = model.moxController();
        mox->setMoxCheck({});
        mox->setMox(true);
        pump();
        slice->setFrequency(7100000.0);
        pump();
        mox->setMox(false);
        pump();
        QCOMPARE(bandFromFrequency(slice->frequency()), Band::Band40m);
        QCOMPARE(model.paOnAirBandIndex(), static_cast<int>(Band::Band80m));
    }

    // TUNE-on drive reads _tx_band. From Thetis console.cs:30098
    // [v2.10.3.15] chkTUN_CheckedChanged:
    //   // remember old power //MW0LGE_22b
    //   ...
    //   int new_pwr = SetPowerUsingTargetDBM(out bool bUseConstrain, out double targetdBm, true, true, false);
    // SetPowerUsingTargetDBM (console.cs:46762-46808 [v2.10.3.15]) takes the
    // tune slider's value (ptbTune, set to the TXBand's tune power) and
    //   gbb = GainByBand(TXBand, new_pwr);
    //   25 W at the 80m gain 50.5 dB: wire 33..35 (tuneEngage above).
    //   60 W at the 40m gain 40.5 dB would be about 168.
    void tuneOn_afterKeyedRetune_drivesTheHeldTxBand()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });
        TransmitModel& tx = model.transmitModel();
        tx.setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
        tx.setTunePowerForBand(Band::Band80m, 25);
        tx.setTunePowerForBand(Band::Band40m, 60);
        pump();
        holdEightyWithSliceOnForty(model);
        QCOMPARE(tx.tunePowerForTxBand(), 25);

        conn->txDriveLog.clear();
        model.setTune(true);
        pump();
        QVERIFY2(!conn->txDriveLog.isEmpty(), "TUN-on pushed no drive");
        const int wireByte = conn->txDriveLog.last();
        QVERIFY2(wireByte >= 33 && wireByte <= 35, qPrintable(QString::number(wireByte)));

        model.setTune(false);
        pump();
    }

    // The first-MOX seed is ptbPWR_Scroll's drive (setPowerFromDriveSlider,
    // console.cs:46710-46716 [v2.10.3.15]), SetPowerUsingTargetDBM with
    // (console.cs:46750-46752 [v2.10.3.15])
    //   case 0: //normal
    //       new_pwr = ptbPWR.Value;
    //       power_by_band[(int)_tx_band] = new_pwr;
    // and GainByBand(TXBand, new_pwr), so it drives the held transmit band.
    //   100 W at the 80m gain 50.5 dB: wire 66..70 (pwrSlider tests above).
    //   100 W at the 40m gain 40.5 dB would be about 217.
    void firstMoxSeed_afterKeyedRetune_drivesTheHeldTxBand()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });
        TransmitModel& tx = model.transmitModel();
        tx.setPower(50);
        pump();
        tx.setPower(100);
        pump();
        holdEightyWithSliceOnForty(model);
        QCOMPARE(tx.power(), 100);

        conn->txDriveLog.clear();
        model.seedInitialAudioVolumeForTest();
        pump();
        QVERIFY2(!conn->txDriveLog.isEmpty(), "the seed pushed no drive");
        const int wireByte = conn->txDriveLog.last();
        QVERIFY2(wireByte >= 66 && wireByte <= 70, qPrintable(QString::number(wireByte)));
    }

    // ── The two-tone start drives through the PA gain ────────────────────────
    // From Thetis setup.cs:11153 [v2.10.3.15] (chkTestIMD_CheckedChanged):
    //   //MW0LGE_22b
    //   // remember old power //MW0LGE_22b
    //   if (console.TwoToneDrivePowerOrigin == DrivePowerSource.FIXED)
    //       console.PreviousPWR = console.PWR;
    //   // set power
    //   int new_pwr = console.SetPowerUsingTargetDBM(out bool bUseConstrain, out double targetdBm, true, true, true);
    // before console.TwoTone = true; // MW0LGE_21a and console.MOX = true
    // (setup.cs:11162-11165). The drive is GainByBand(TXBand, new_pwr)
    // (console.cs:46808 [v2.10.3.15]), the held transmit band.
    //   25 W tune power at the 80m gain 50.5 dB: wire 33..35.
    //   The 100 W PWR drive the MOX-edge restore would push: 66..70.
    // Runs RadioModel's own controller, wired as in production.
    static void startTwoTone(RadioModel& model)
    {
        model.setTwoTone(true);
        for (int i = 0; i < 10; ++i) { pump(); }
    }
    static void stopTwoTone(RadioModel& model)
    {
        model.setTwoTone(false);
        for (int i = 0; i < 10; ++i) { pump(); }
    }
    static void attachTwoTone(RadioModel& model, TxChannel& tx)
    {
        TwoToneController* twoTone = model.twoToneController();
        QVERIFY(twoTone != nullptr);
        twoTone->setTxChannel(&tx);
        twoTone->setPowerOn(true);
        twoTone->setSettleDelaysMs(/*moxReleaseMs=*/0, /*tuneReleaseMs=*/0);
        model.moxController()->setMoxCheck({});
    }

    void twoToneStart_drivesThePaGainForTheHeldTxBand()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        TxChannel txChannel{/*channelId=*/1};
        auto detach = qScopeGuard([&]{
            if (model.twoToneController()) {
                model.twoToneController()->setTxChannel(nullptr);
            }
            model.injectConnectionForTest(nullptr);
        });
        attachTwoTone(model, txChannel);
        TransmitModel& tx = model.transmitModel();
        tx.setTwoToneDrivePowerSource(DrivePowerSource::TuneSlider);
        tx.setTunePowerForBand(Band::Band80m, 25);
        tx.setTunePowerForBand(Band::Band40m, 60);
        tx.setPower(50);
        pump();
        tx.setPower(100);
        pump();
        holdEightyWithSliceOnForty(model);

        // console.TwoTone = true before console.MOX = true.
        bool activeAtKey = false;
        QMetaObject::Connection c = connect(
            model.moxController(), &MoxController::moxChanging, this,
            [&activeAtKey, &tx](int, bool, bool on) { if (on) { activeAtKey = tx.isTwoToneActive(); } });
        conn->txDriveLog.clear();
        startTwoTone(model);
        disconnect(c);
        QVERIFY(model.twoToneController()->isActive());
        QVERIFY(model.moxController()->isMox());
        QVERIFY(tx.isTwoToneActive());
        QVERIFY(activeAtKey);
        QVERIFY2(!conn->txDriveLog.isEmpty(), "two-tone start pushed no drive");
        const int wireByte = conn->txDriveLog.last();
        QVERIFY2(wireByte >= 33 && wireByte <= 35, qPrintable(QString::number(wireByte)));

        stopTwoTone(model);
        QVERIFY(!model.twoToneController()->isActive());
        QVERIFY(!tx.isTwoToneActive());
        QVERIFY(!model.moxController()->isMox());
    }

    // FIXED source: PWRSliderLimitEnabled = false around the start, so the
    // band's PWR limit does not cut the two-tone power (setup.cs:11155-11159
    // [v2.10.3.15]); the stop turns it back on and restores PWR:
    //   //MW0LGE_22b
    //   if (console.TwoToneDrivePowerOrigin == DrivePowerSource.FIXED)
    //   {
    //       console.PWRSliderLimitEnabled = true;
    //       console.PWR = console.PreviousPWR;
    //   }
    // (setup.cs:11196-11201 [v2.10.3.15]).
    //   60 W at the 80m gain 50.5 dB: wire 51..55; the 30 W limit: about 37.
    void twoToneStart_fixedSource_ignoresThePwrLimit_stopRestores()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        TxChannel txChannel{/*channelId=*/1};
        auto detach = qScopeGuard([&]{
            if (model.twoToneController()) {
                model.twoToneController()->setTxChannel(nullptr);
            }
            model.injectConnectionForTest(nullptr);
        });
        attachTwoTone(model, txChannel);
        TransmitModel& tx = model.transmitModel();
        tx.setLimitPowerForBand(Band::Band80m, 30);
        tx.setPowerLimit(30);   // the 80 m band's limit, as the TXBand setter assigns
        tx.setTwoToneDrivePowerSource(DrivePowerSource::Fixed);
        tx.setTwoTonePower(60);
        tx.setPower(20);
        pump();
        QCOMPARE(tx.powerLimit(), 30);

        conn->txDriveLog.clear();
        startTwoTone(model);
        QVERIFY(model.twoToneController()->isActive());
        QVERIFY(tx.isTwoToneActive());
        QVERIFY(!tx.powerSliderLimitEnabled());
        QCOMPARE(tx.power(), 60);
        QVERIFY2(!conn->txDriveLog.isEmpty(), "two-tone start pushed no drive");
        const int wireByte = conn->txDriveLog.last();
        QVERIFY2(wireByte >= 51 && wireByte <= 55, qPrintable(QString::number(wireByte)));

        stopTwoTone(model);
        QVERIFY(!tx.isTwoToneActive());
        QVERIFY(tx.powerSliderLimitEnabled());
        QCOMPARE(tx.power(), 20);
        QCOMPARE(tx.powerForBand(Band::Band80m), 20);
    }

    // Power off stops two-tone at once. From Thetis console.cs:27473 and
    // 27488-27492 [v2.10.3.15] (chkPower_CheckedChanged, power going off):
    //   SetupForm.TestIMD = false;
    //   ...
    //   chk2TONE.Checked = false;  // MW0LGE_21a
    // The FIXED source's stop (setup.cs:11196-11201 [v2.10.3.15]) puts the
    // PWR limit back on and PWR back to its saved value, and _tx_band is
    // never cleared, so the saved value lands in the held band. None of
    // these tests pumps the event loop between the start and the teardown's
    // checks: a quit never runs it again.
    static void armFixedTwoToneAtTwenty(RadioModel& model)
    {
        TransmitModel& tx = model.transmitModel();
        tx.loadFromSettings(QStringLiteral("AABBCCDDEEFF"));
        tx.setTwoToneDrivePowerSource(DrivePowerSource::Fixed);
        tx.setTwoTonePower(60);
        tx.setPower(20);
        pump();
        QCOMPARE(tx.powerForBand(Band::Band80m), 20);
    }
    static QString powerKey(Band band)
    {
        return QStringLiteral("hardware/AABBCCDDEEFF/powerByBand/%1")
            .arg(bandKeyName(band));
    }
    static void verifyTwentyRestored(TransmitModel& tx, Band band)
    {
        QVERIFY(!tx.isTwoToneActive());
        QVERIFY(tx.powerSliderLimitEnabled());
        QCOMPARE(tx.power(), 20);
        QCOMPARE(tx.powerForBand(band), 20);
        QCOMPARE(AppSettings::instance().value(powerKey(band)).toString(),
                 QStringLiteral("20"));
    }

    void twoToneFixed_disconnectRestoresThePowerAtOnce()
    {
        TxChannel txChannel{/*channelId=*/1};
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{
            if (model.twoToneController()) {
                model.twoToneController()->setTxChannel(nullptr);
            }
            model.injectConnectionForTest(nullptr);
        });
        attachTwoTone(model, txChannel);
        armFixedTwoToneAtTwenty(model);
        TransmitModel& tx = model.transmitModel();

        startTwoTone(model);
        QVERIFY(model.twoToneController()->isActive());
        QCOMPARE(tx.power(), 60);
        QCOMPARE(tx.powerForBand(Band::Band80m), 60);

        model.disconnectFromRadio();
        QVERIFY(!model.twoToneController()->isActive());
        QVERIFY(!model.twoToneController()->isActivationInFlight());
        verifyTwentyRestored(tx, Band::Band80m);
    }

    // Keyed retune: the slice moves to 40 m while the test runs, so 80 m
    // stays _tx_band (//[2.10.3.6]MW0LGE no band change on TX fix). The
    // restore lands in 80 m and leaves 40 m as it was.
    void twoToneFixed_disconnectAfterAKeyedRetune_restoresTheHeldBand()
    {
        TxChannel txChannel{/*channelId=*/1};
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{
            if (model.twoToneController()) {
                model.twoToneController()->setTxChannel(nullptr);
            }
            model.injectConnectionForTest(nullptr);
        });
        attachTwoTone(model, txChannel);
        armFixedTwoToneAtTwenty(model);
        TransmitModel& tx = model.transmitModel();
        tx.setPowerForBand(Band::Band40m, 45);

        startTwoTone(model);
        QVERIFY(model.twoToneController()->isActive());
        model.activeSlice()->setFrequency(7100000.0);
        pump();
        QCOMPARE(model.paOnAirBandIndex(), static_cast<int>(Band::Band80m));

        model.disconnectFromRadio();
        verifyTwentyRestored(tx, Band::Band80m);
        QCOMPARE(tx.powerForBand(Band::Band40m), 45);
        QCOMPARE(AppSettings::instance().value(powerKey(Band::Band40m)).toString(),
                 QStringLiteral("45"));
    }

    // Quit: ~RadioModel tears the connection down and nothing runs after.
    void twoToneFixed_quitRestoresThePersistedPower()
    {
        TxChannel txChannel{/*channelId=*/1};
        MockConnection* conn = nullptr;
        std::unique_ptr<MockConnection> connOwner;
        {
            auto model = std::make_unique<RadioModel>();
            setupModel(*model, conn, HPSDRModel::ANAN8000D);
            connOwner.reset(conn);
            attachTwoTone(*model, txChannel);
            armFixedTwoToneAtTwenty(*model);
            startTwoTone(*model);
            QVERIFY(model->twoToneController()->isActive());
            QCOMPARE(AppSettings::instance().value(powerKey(Band::Band80m)).toString(),
                     QStringLiteral("60"));
            model.reset();
        }
        QCOMPARE(AppSettings::instance().value(powerKey(Band::Band80m)).toString(),
                 QStringLiteral("20"));
        TransmitModel reloaded;
        reloaded.loadFromSettings(QStringLiteral("AABBCCDDEEFF"));
        QCOMPARE(reloaded.powerForBand(Band::Band80m), 20);
    }

    // A stop already waiting out its 200 ms settle (setup.cs:11190-11191
    // [v2.10.3.15]: console.MOX = false; await Task.Delay(200);) when the
    // connection goes.
    void twoToneFixed_disconnectDuringTheStopSettle_restoresAtOnce()
    {
        TxChannel txChannel{/*channelId=*/1};
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{
            if (model.twoToneController()) {
                model.twoToneController()->setTxChannel(nullptr);
            }
            model.injectConnectionForTest(nullptr);
        });
        attachTwoTone(model, txChannel);
        armFixedTwoToneAtTwenty(model);
        TransmitModel& tx = model.transmitModel();

        startTwoTone(model);
        model.setTwoTone(false);
        QVERIFY(tx.isTwoToneActive());   // the stop's settle has not run

        model.disconnectFromRadio();
        QVERIFY(!model.twoToneController()->isActive());
        verifyTwentyRestored(tx, Band::Band80m);
    }

    // A start still waiting out the MOX release (setup.cs:11172-11177
    // [v2.10.3.15]) when the connection goes ends there: nothing keys or
    // starts the tones later.
    void twoTone_disconnectDuringTheStartSettle_endsTheStart()
    {
        TxChannel txChannel{/*channelId=*/1};
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{
            if (model.twoToneController()) {
                model.twoToneController()->setTxChannel(nullptr);
            }
            model.injectConnectionForTest(nullptr);
        });
        attachTwoTone(model, txChannel);
        armFixedTwoToneAtTwenty(model);
        TransmitModel& tx = model.transmitModel();

        model.moxController()->setMox(true);
        pump();
        QVERIFY(model.moxController()->isMox());
        model.setTwoTone(true);
        QVERIFY(model.twoToneController()->isActivationInFlight());

        model.disconnectFromRadio();
        QVERIFY(!model.twoToneController()->isActivationInFlight());
        for (int i = 0; i < 10; ++i) { pump(); }
        QVERIFY(!model.twoToneController()->isActive());
        QVERIFY(!model.moxController()->isMox());
        QVERIFY(!tx.isTwoToneActive());
        QCOMPARE(tx.power(), 20);
        QCOMPARE(tx.powerForBand(Band::Band80m), 20);
    }

    // TUNE under the FIXED source. From Thetis console.cs:30094-30104
    // [v2.10.3.15] (chkTUN_CheckedChanged, TUN on):
    //   // remember old power //MW0LGE_22b
    //   if (_tuneDrivePowerSource == DrivePowerSource.FIXED)
    //       PreviousPWR = ptbPWR.Value;
    //   // set power
    //   int new_pwr = SetPowerUsingTargetDBM(out bool bUseConstrain, out double targetdBm, true, true, false);
    //   //
    //   if (_tuneDrivePowerSource == DrivePowerSource.FIXED)
    //   {
    //       PWRSliderLimitEnabled = false;
    //       PWR = new_pwr;
    //   }
    // and console.cs:30180-30185 [v2.10.3.15] (TUN off):
    //   //MW0LGE_22b
    //   if (_tuneDrivePowerSource == DrivePowerSource.FIXED)
    //   {
    //       PWRSliderLimitEnabled = true;
    //       PWR = PreviousPWR;
    //   }
    // The drive does not change: TUN on drives the fixed tune power, and
    // TUN off drives PWR again.
    void tuneFixed_setsPwrToTheTunePower_offRestoresIt()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });
        TransmitModel& tx = model.transmitModel();
        tx.loadFromSettings(QStringLiteral("AABBCCDDEEFF"));
        tx.setTuneDrivePowerSource(DrivePowerSource::Fixed);
        tx.setTunePower(35);
        tx.setPowerLimit(30);
        tx.setPower(20);
        pump();
        QVERIFY(!conn->txDriveLog.isEmpty());
        const int pwrByte = conn->txDriveLog.last();

        conn->txDriveLog.clear();
        model.setTune(true);
        pump();
        QVERIFY(model.isTune());
        QVERIFY(!tx.powerSliderLimitEnabled());
        QCOMPARE(tx.power(), 35);
        QVERIFY2(!conn->txDriveLog.isEmpty(), "TUN on pushed no drive");
        const int tuneByte = conn->txDriveLog.first();
        for (int b : std::as_const(conn->txDriveLog)) {
            QCOMPARE(b, tuneByte);
        }

        model.setTune(false);
        QTRY_VERIFY(!model.tuneOffPendingForTest());
        QVERIFY(tx.powerSliderLimitEnabled());
        QCOMPARE(tx.power(), 20);
        QCOMPARE(tx.powerForBand(Band::Band80m), 20);
        QCOMPARE(conn->txDriveLog.last(), pwrByte);
    }

    // A disconnect mid-TUNE runs TUN-off at once (console.cs:27490
    // [v2.10.3.15], chkTUN.Checked = false), so the FIXED restore lands
    // before the saves.
    void tuneFixed_disconnectRestoresThePowerAtOnce()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });
        TransmitModel& tx = model.transmitModel();
        tx.loadFromSettings(QStringLiteral("AABBCCDDEEFF"));
        tx.setTuneDrivePowerSource(DrivePowerSource::Fixed);
        tx.setTunePower(35);
        tx.setPower(20);
        pump();

        model.setTune(true);
        pump();
        QCOMPARE(tx.power(), 35);

        model.disconnectFromRadio();
        QVERIFY(!model.isTune());
        QVERIFY(tx.powerSliderLimitEnabled());
        QCOMPARE(tx.power(), 20);
        QCOMPARE(tx.powerForBand(Band::Band80m), 20);
        QCOMPARE(AppSettings::instance().value(powerKey(Band::Band80m)).toString(),
                 QStringLiteral("20"));
    }

    // Not FIXED: TUN neither sets nor restores PWR, so a PWR change made
    // during TUNE stays.
    void tuneNotFixed_leavesPwrAlone()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });
        TransmitModel& tx = model.transmitModel();
        tx.setTuneDrivePowerSource(DrivePowerSource::TuneSlider);
        tx.setTunePowerForBand(Band::Band80m, 35);
        tx.setPower(20);
        pump();

        model.setTune(true);
        pump();
        QVERIFY(model.isTune());
        QVERIFY(tx.powerSliderLimitEnabled());
        QCOMPARE(tx.power(), 20);
        tx.setPower(25);
        pump();

        model.setTune(false);
        QTRY_VERIFY(!model.tuneOffPendingForTest());
        QVERIFY(tx.powerSliderLimitEnabled());
        QCOMPARE(tx.power(), 25);
        QCOMPARE(tx.powerForBand(Band::Band80m), 25);
    }

    // The stop by unkey (chkMOX_Click unchecks chk2TONE) and by an error
    // (the emergency stop, and a refused key: if (!console.MOX) {
    // chkTestIMD.Checked = false; return; }, setup.cs:11166-11170
    // [v2.10.3.15]) each end with console.TwoTone = false.
    void twoToneStop_byUnkeyStopAllAndRefusal_clearsTwoToneActive()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        TxChannel txChannel{/*channelId=*/1};
        auto detach = qScopeGuard([&]{
            if (model.twoToneController()) {
                model.twoToneController()->setTxChannel(nullptr);
            }
            model.injectConnectionForTest(nullptr);
        });
        attachTwoTone(model, txChannel);
        TransmitModel& tx = model.transmitModel();
        tx.setTwoToneDrivePowerSource(DrivePowerSource::Fixed);
        tx.setTwoTonePower(40);
        tx.setPower(20);
        pump();

        startTwoTone(model);
        QVERIFY(tx.isTwoToneActive());
        model.setMoxFromButton(false);
        for (int i = 0; i < 10; ++i) { pump(); }
        QVERIFY(!model.twoToneController()->isActive());
        QVERIFY(!tx.isTwoToneActive());
        QVERIFY(tx.powerSliderLimitEnabled());
        QCOMPARE(tx.power(), 20);

        startTwoTone(model);
        QVERIFY(tx.isTwoToneActive());
        model.stopAllTx(QStringLiteral("test stop"));
        for (int i = 0; i < 10; ++i) { pump(); }
        QVERIFY(!model.twoToneController()->isActive());
        QVERIFY(!tx.isTwoToneActive());
        QVERIFY(tx.powerSliderLimitEnabled());
        QCOMPARE(tx.power(), 20);

        model.moxController()->setMoxCheck([]() {
            return safety::BandPlanGuard::MoxCheckResult{
                false, QStringLiteral("test refusal")};
        });
        QSignalSpy activeSpy(&tx, &TransmitModel::twoToneActiveChanged);
        startTwoTone(model);
        QVERIFY(!model.moxController()->isMox());
        QVERIFY(!model.twoToneController()->isActive());
        QVERIFY(!tx.isTwoToneActive());
        QVERIFY(tx.powerSliderLimitEnabled());
        QCOMPARE(tx.power(), 20);
        // It went true for the key, as console.TwoTone does, then false.
        QCOMPARE(activeSpy.count(), 2);
    }

    // The TXBand setter assigns the band's slider limits
    // (console.cs:17539-17540 [v2.10.3.15]:
    //   ptbPWR.LimitValue = limitPower_by_band[(int)value];
    //   ptbTune.LimitValue = limitTunePower_by_band[(int)value]; //MW0LGE_22b)
    // and SetPowerUsingTargetDBM runs the drive through the slider's limit
    // (console.cs:46798 [v2.10.3.15]:
    //   if(bConstrain) new_pwr = slider.ConstrainAValue(new_pwr);).
    void bandChange_assignsPowerLimits_constrainsDrive()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });
        TransmitModel& tx = model.transmitModel();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        const PaProfile* profile = model.paProfileManager()->activeProfile();
        QVERIFY(profile != nullptr);
        pump();

        // Defaults: every band's limit is 100 (console.cs:1824-1831).
        QCOMPARE(tx.limitPowerForBand(Band::Band40m), 100);
        QCOMPARE(tx.limitTunePowerForBand(Band::Band40m), 100);
        QCOMPARE(tx.powerLimit(), 100);
        QCOMPARE(tx.tunePowerLimit(), 100);

        tx.setLimitPowerForBand(Band::Band40m, 30);
        tx.setLimitTunePowerForBand(Band::Band40m, 20);
        tx.setPowerForBand(Band::Band40m, 70);
        tx.setTunePowerForBand(Band::Band40m, 60);
        tx.setTuneDrivePowerSource(DrivePowerSource::TuneSlider);

        slice->setFrequency(7100000.0);
        pump();
        QCOMPARE(tx.power(), 70);
        QCOMPARE(tx.powerLimit(), 30);
        QCOMPARE(tx.tunePowerLimit(), 20);
        QCOMPARE(tx.setPowerUsingTargetDbm(*profile, Band::Band40m, false,
                                           false, false, HPSDRModel::ANAN8000D)
                     .newPower, 30);
        QCOMPARE(tx.setPowerUsingTargetDbm(*profile, Band::Band40m, false,
                                           true, false, HPSDRModel::ANAN8000D)
                     .newPower, 20);
        // The slider keeps its value; only the drive is constrained.
        QCOMPARE(tx.powerForBand(Band::Band40m), 70);

        // The drive the band change pushed was the constrained one.
        const double atLimit = tx.setPowerUsingTargetDbm(
            *profile, Band::Band40m, false, false, false,
            HPSDRModel::ANAN8000D).audioVolume;
        tx.setLimitPowerForBand(Band::Band40m, 100);
        tx.setPowerLimit(100);
        const double unlimited = tx.setPowerUsingTargetDbm(
            *profile, Band::Band40m, false, false, false,
            HPSDRModel::ANAN8000D).audioVolume;
        QVERIFY(atLimit < unlimited);
        tx.setLimitPowerForBand(Band::Band40m, 30);

        // Back on 80 m the 80 m limits (100) apply again.
        slice->setFrequency(3700000.0);
        pump();
        QCOMPARE(tx.powerLimit(), 100);
        QCOMPARE(tx.tunePowerLimit(), 100);

        // FIXED is never constrained (bConstrain = false).
        slice->setFrequency(7100000.0);
        pump();
        tx.setTuneDrivePowerSource(DrivePowerSource::Fixed);
        tx.setTunePower(80);
        QCOMPARE(tx.setPowerUsingTargetDbm(*profile, Band::Band40m, false,
                                           true, false, HPSDRModel::ANAN8000D)
                     .newPower, 80);
    }

    // A band change the limit does not allow still recomputes the drive:
    // Thetis's PWR setter always runs ptbPWR_Scroll (console.cs:18437-18447
    // [v2.10.3.15]), even when the value is unchanged.
    void bandChange_samePower_newLimit_recomputesDrive()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });
        TransmitModel& tx = model.transmitModel();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        pump();
        tx.setPowerForBand(Band::Band80m, 50);
        tx.setPower(50);
        tx.setPowerForBand(Band::Band40m, 50);
        tx.setLimitPowerForBand(Band::Band40m, 10);
        pump();
        conn->txDriveLog.clear();
        slice->setFrequency(7100000.0);
        pump();
        QCOMPARE(tx.power(), 50);
        QVERIFY(!conn->txDriveLog.isEmpty());
        const int limitedByte = conn->txDriveLog.last();
        slice->setFrequency(3700000.0);
        pump();
        QVERIFY(!conn->txDriveLog.isEmpty());
        QVERIFY(conn->txDriveLog.last() > limitedByte);
    }

    // The per-band limits persist per radio like power_by_band
    // (console.cs:3101-3109 save, 4921-4935 load [v2.10.3.15]).
    void powerLimits_persistPerRadio()
    {
        AppSettings::instance().clear();
        {
            TransmitModel tx;
            tx.loadFromSettings(QStringLiteral("AABBCCDDEEFF"));
            tx.setLimitPowerForBand(Band::Band20m, 35);
            tx.setLimitTunePowerForBand(Band::Band20m, 25);
            tx.setLimitPowerForBand(Band::Band6m, 150);  // clamps to 100
            QCOMPARE(tx.limitPowerForBand(Band::Band6m), 100);
        }
        TransmitModel tx;
        tx.loadFromSettings(QStringLiteral("AABBCCDDEEFF"));
        QCOMPARE(tx.limitPowerForBand(Band::Band20m), 35);
        QCOMPARE(tx.limitTunePowerForBand(Band::Band20m), 25);
        QCOMPARE(tx.limitPowerForBand(Band::Band40m), 100);
    }

    // The TXBand setter saves the FM TX offset into the old band and loads
    // the new band's (console.cs:17546-17550 [v2.10.3.15]:
    //   if (!initializing) fm_tx_offset_by_band_mhz[(int)old_band] = fm_tx_offset_mhz;
    //   FMTXOffsetMHz = fm_tx_offset_by_band_mhz[(int)value]; //MW0LGE_21k9),
    // defaults 1 MHz on 6 m and 0.1 MHz elsewhere (console.cs:1833-1841).
    void bandChange_savesAndLoadsFmTxOffset()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn, HPSDRModel::ANAN8000D);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });
        TransmitModel& tx = model.transmitModel();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        pump();
        QCOMPARE(tx.fmTxOffsetForBandMhz(Band::Band6m), 1.0);
        QCOMPARE(tx.fmTxOffsetForBandMhz(Band::Band10m), 0.1);
        QCOMPARE(tx.fmTxOffsetForBandMhz(Band::Band2m), 0.1);

        slice->setFrequency(50100000.0);
        pump();
        QCOMPARE(tx.fmTxOffsetMhz(), 1.0);
        tx.setFmTxOffsetMhz(0.6);
        // Out of udFMOffset's 0..50 MHz range: ignored.
        tx.setFmTxOffsetMhz(60.0);
        QCOMPARE(tx.fmTxOffsetMhz(), 0.6);

        slice->setFrequency(28500000.0);
        pump();
        QCOMPARE(tx.fmTxOffsetForBandMhz(Band::Band6m), 0.6);
        QCOMPARE(tx.fmTxOffsetMhz(), 0.1);

        slice->setFrequency(50200000.0);
        pump();
        QCOMPARE(tx.fmTxOffsetMhz(), 0.6);
    }

    // The per-band FM TX offset store takes only what udFMOffset can hold
    // (0..50 MHz, the FMTXOffsetMHz setter's check at console.cs:20891-20902
    // [v2.10.3.15], //MW0LGE_21k9). A set outside that keeps the value it
    // had, as the setter's return does; NaN and infinities are outside it.
    // A hand-edited settings file's bad value loads as the band's default
    // (console.cs:1833-1841).
    void fmTxOffsetStore_keepsOnlyValidOffsets()
    {
        const double inf = std::numeric_limits<double>::infinity();
        const double nan = std::numeric_limits<double>::quiet_NaN();
        TransmitModel tx;
        tx.setFmTxOffsetForBandMhz(Band::Band20m, 25.0);
        QCOMPARE(tx.fmTxOffsetForBandMhz(Band::Band20m), 25.0);
        tx.setFmTxOffsetForBandMhz(Band::Band20m, nan);
        QCOMPARE(tx.fmTxOffsetForBandMhz(Band::Band20m), 25.0);
        tx.setFmTxOffsetForBandMhz(Band::Band6m, 3.0);
        tx.setFmTxOffsetForBandMhz(Band::Band6m, inf);
        QCOMPARE(tx.fmTxOffsetForBandMhz(Band::Band6m), 3.0);
        tx.setFmTxOffsetForBandMhz(Band::Band40m, 7.0);
        tx.setFmTxOffsetForBandMhz(Band::Band40m, -1e300);
        QCOMPARE(tx.fmTxOffsetForBandMhz(Band::Band40m), 7.0);
        tx.setFmTxOffsetForBandMhz(Band::Band40m, 50.5);
        QCOMPARE(tx.fmTxOffsetForBandMhz(Band::Band40m), 7.0);
        tx.setFmTxOffsetForBandMhz(Band::Band40m, 50.0);
        QCOMPARE(tx.fmTxOffsetForBandMhz(Band::Band40m), 50.0);

        const QString mac = QStringLiteral("AABBCCDDEEFF");
        const QString pfx = QStringLiteral("hardware/%1/fmTxOffsetByBandMhz/").arg(mac);
        AppSettings& settings = AppSettings::instance();
        settings.setValue(pfx + bandKeyName(Band::Band6m), QStringLiteral("nan"));
        settings.setValue(pfx + bandKeyName(Band::Band10m), QStringLiteral("inf"));
        settings.setValue(pfx + bandKeyName(Band::Band20m), QStringLiteral("-1e300"));
        settings.setValue(pfx + bandKeyName(Band::Band40m), QStringLiteral("51"));
        settings.setValue(pfx + bandKeyName(Band::Band80m), QStringLiteral("7.5"));
        TransmitModel loaded;
        loaded.loadFromSettings(mac);
        QCOMPARE(loaded.fmTxOffsetForBandMhz(Band::Band6m), 1.0);
        QCOMPARE(loaded.fmTxOffsetForBandMhz(Band::Band10m), 0.1);
        QCOMPARE(loaded.fmTxOffsetForBandMhz(Band::Band20m), 0.1);
        QCOMPARE(loaded.fmTxOffsetForBandMhz(Band::Band40m), 0.1);
        QCOMPARE(loaded.fmTxOffsetForBandMhz(Band::Band80m), 7.5);
    }
};

QTEST_MAIN(TestRadioModelDrivePath)
#include "tst_radio_model_drive_path.moc"
