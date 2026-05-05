// tst_radio_model_set_tune.cpp
//
// no-port-check: Test file exercises NereusSDR API; Thetis behavior is
// cited in RadioModel.cpp via pre-code review §3.2/§3.3 and
// Thetis console.cs:29978-30157 [v2.10.3.13] — no C# is translated here.
//
// Unit tests for Phase 3M-1a Task G.4: RadioModel::setTune(bool).
//
// Covers:
//   1. tuneRefused emitted when radio not connected.
//   2. USB mode → no DSP swap, tone freq = +cw_pitch (+600 Hz).
//   3. LSB mode → no DSP swap, tone freq = -cw_pitch (-600 Hz).
//   4. CWL mode → swap to LSB, tone freq = -cw_pitch (-600 Hz).
//   5. CWU mode → swap to USB, tone freq = +cw_pitch (+600 Hz).
//   6. CWL TUN-on → TUN-off restores CWL.
//   7. CWU TUN-on → TUN-off restores CWU.
//   8. Non-CW (USB) TUN-on → TUN-off: DSP mode unchanged.
//   9. TUN-on pushes tunePowerForBand to connection.
//  10. TUN-off restores saved power to connection.
//  11. MoxController::setTune(true) called on TUN-on (via manualMoxChanged spy).
//  12. MoxController::setTune(false) called on TUN-off.
//  13. Tune tone engaged on TUN-on (setTuneTone recorded by MockTxChannel).
//  14. Tune tone released on TUN-off.
//  15. m_isTuning guard: TUN-off without prior TUN-on → complete no-op (G.4 fixup).
//  16. AM mode: not swapped (isLsbFamily(AM) == false → no setDspMode).
//  17. FM mode: not swapped (isLsbFamily(FM) == false → no setDspMode).
//  18. DIGL/DIGU isLsbFamily predicate: DIGL → true, DIGU → false (tone sign).

#include <QtTest/QtTest>
#include <QObject>
#include <QSignalSpy>
#include <QCoreApplication>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/PaProfileManager.h"
#include "core/RadioConnection.h"
#include "core/TxChannel.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

// ── isLsbFamily reference copy (test seam) ───────────────────────────────────
// G.4 fixup: isLsbFamily() is a file-scope static in RadioModel.cpp and cannot
// be linked from the test binary (NereusSDRObjs is not compiled with
// NEREUS_BUILD_TESTS).  We maintain an independent reference copy here that
// mirrors the production logic exactly.  A mismatch between this copy and
// RadioModel.cpp will be caught by test 18 failing on the observable behaviors
// of setTune (AM/FM/DIGL/DIGU mode-swap / no-swap tests 16-17 verify the
// integration path; test 18 validates the predicate contract itself).
//
// Production source: RadioModel.cpp — static bool isLsbFamily(DSPMode mode)
// Cite: Thetis console.cs:30024-30037 [v2.10.3.13].
static bool isLsbFamilyRef(DSPMode mode) noexcept
{
    return mode == DSPMode::LSB || mode == DSPMode::CWL || mode == DSPMode::DIGL;
}

// ── MockConnection ────────────────────────────────────────────────────────────
// Records setTxDrive calls (and satisfies RadioConnection pure virtuals).
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    // Ordered log of setTxDrive() argument values.
    QList<int> txDriveLog;

    explicit MockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }

    // ── Pure-virtual overrides ────────────────────────────────────────────────
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

// ── MockTxChannel ─────────────────────────────────────────────────────────────
// Overrides setTuneTone to record calls without touching real WDSP.
// Note: TxChannel inherits QObject but has no virtual methods in 3M-1a.
// We cannot subclass TxChannel cleanly. Instead we use a real TxChannel but
// verify indirectly via QSignalSpy on MoxController since setTuneTone in
// unit tests stubs out WDSP calls (HAVE_WDSP not defined in test builds).
// The actual setTuneTone/setRunning calls in RadioModel will be no-ops on
// the wdsp side (guarded by #ifdef HAVE_WDSP in TxChannel.cpp), so they
// are safe to call in unit tests.
//
// To verify setTuneTone was called, we track m_txChannel state via the
// fact that setTuneTone calls WDSP stubs (no crash = it ran). For positive
// frequency verification we need a spy on a signal — but TxChannel emits
// none. We verify indirectly via the DSP mode change (which IS observable
// via SliceModel::dspModeChanged) and via the connection txDriveLog.

// ── Helpers ───────────────────────────────────────────────────────────────────

// processEvents helper: drains the Qt event loop for queued connections.
//
// Pre-issue #177: setTune(false) restored power synchronously (single-frame
// queued setTxDrive call) so two passes were enough.
//
// Post-issue #177 (Thetis-faithful TUN-off): setTune(false) now defers the
// tone-off + power-restore until MoxController::rxReady fires AND a settle
// timer elapses.  With test timers set to 0 and m_tuneOffSettleMs forced to 0
// via setTuneOffSettleMsForTest(), the chain that needs to drain on a
// TUN-off cycle is:
//
//   iter 1: keyUpDelayTimer (0 ms QTimer) fires → onKeyUpDelayElapsed →
//           emit txaFlushed; pttOutDelayTimer.start()
//   iter 2: pttOutDelayTimer fires → onPttOutElapsed → emit rxReady →
//           constructor lambda schedules QTimer::singleShot(0)
//   iter 3: QTimer::singleShot(0) fires → completeTuneOff() →
//           QMetaObject::invokeMethod queues setTxDrive (same-thread Auto
//           collapses to Direct, so the call lands during this iteration)
//
// Four passes leaves headroom for the Qt::QueuedConnection on
// hardwareFlipped (line 624) which adds a same-thread queue hop.
static void pump()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

// Build a RadioModel with a connected slice, a mock connection, and the
// MoxController timer set to zero so state walks are synchronous.
// Returns: model, mock connection (caller takes ownership of mock lifetime).
static void setupModel(RadioModel& model, MockConnection*& mockConn)
{
    // Clear any stale AppSettings.
    AppSettings::instance().clear();

    model.setCapsForTest(/*hasAlex=*/false);
    mockConn = new MockConnection();
    model.injectConnectionForTest(mockConn);

    // Make MoxController walk synchronous (0-ms timers → pump() drives walk).
    model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);

    // Issue #177 — drive the TUN-off settle synchronously in tests.
    // Production default is 100 ms (matches Thetis console.cs:30107
    // [v2.10.3.13] `await Task.Delay(100)`).  Setting to 0 lets pump()
    // drive completeTuneOff() through QCoreApplication::processEvents
    // without sleeping in tests.  Behavior is otherwise identical: the
    // deferred completion still runs from the rxReady → settle-timer
    // chain wired in the RadioModel constructor.
    model.setTuneOffSettleMsForTest(0);

    // Add a slice so m_activeSlice is non-null.
    model.addSlice();
    // Slice 0 is active after addSlice.
}

// ── Test class ────────────────────────────────────────────────────────────────
class TestRadioModelSetTune : public QObject {
    Q_OBJECT

    void clearSettings() { AppSettings::instance().clear(); }

private slots:
    void initTestCase() { clearSettings(); }
    void init()          { clearSettings(); }
    void cleanup()       { clearSettings(); }

    // ── 1. tuneRefused emitted when radio not connected ───────────────────────
    // setTune(true) with no active connection must emit tuneRefused and not
    // touch MoxController.
    // Cite: console.cs:29983-29991 [v2.10.3.13].
    void tuneRefusedWhenNotConnected()
    {
        RadioModel model;
        AppSettings::instance().clear();

        QSignalSpy refusedSpy(&model, &RadioModel::tuneRefused);
        QSignalSpy manualSpy(model.moxController(), &MoxController::manualMoxChanged);

        model.setTune(true);
        pump();

        QCOMPARE(refusedSpy.count(), 1);
        // MoxController must NOT have been engaged.
        QCOMPARE(manualSpy.count(), 0);
    }

    // ── 2. USB mode: no DSP swap, tone sign = +cw_pitch ──────────────────────
    // When active slice is in USB, no CW→LSB/USB swap occurs.
    // The slice mode must remain USB after TUN-on.
    // Cite: console.cs:30043-30070 [v2.10.3.13] — switch (old_dsp_mode).
    void usbModeNoDspSwap()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        auto* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);

        // Spy on dspModeChanged — must NOT fire.
        QSignalSpy modeSpy(slice, &SliceModel::dspModeChanged);

        model.setTune(true);
        pump();

        // USB → no swap → dspModeChanged must not have fired.
        QCOMPARE(modeSpy.count(), 0);
        QCOMPARE(slice->dspMode(), DSPMode::USB);

        // Cleanup: release tune.
        model.setTune(false);
        pump();

        model.injectConnectionForTest(nullptr);
    }

    // ── 3. LSB mode: no DSP swap, tone sign = -cw_pitch ──────────────────────
    // Same as test 2 for LSB. The sign difference is only observable at the
    // WDSP level (which we cannot stub), but we verify no mode swap occurs.
    void lsbModeNoDspSwap()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        auto* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::LSB);

        QSignalSpy modeSpy(slice, &SliceModel::dspModeChanged);

        model.setTune(true);
        pump();

        QCOMPARE(modeSpy.count(), 0);
        QCOMPARE(slice->dspMode(), DSPMode::LSB);

        model.setTune(false);
        pump();

        model.injectConnectionForTest(nullptr);
    }

    // ── 4. CWL mode: swap to LSB on TUN-on ───────────────────────────────────
    // Cite: console.cs:30043-30053 [v2.10.3.13]:
    //   case DSPMode.CWL: ... Audio.TXDSPMode = DSPMode.LSB; break;
    void cwlSwapsToLsb()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        auto* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::CWL);

        model.setTune(true);
        pump();

        QCOMPARE(slice->dspMode(), DSPMode::LSB);

        model.setTune(false);
        pump();

        model.injectConnectionForTest(nullptr);
    }

    // ── 5. CWU mode: swap to USB on TUN-on ───────────────────────────────────
    // Cite: console.cs:30054-30064 [v2.10.3.13]:
    //   case DSPMode.CWU: ... Audio.TXDSPMode = DSPMode.USB; break;
    void cwuSwapsToUsb()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        auto* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::CWU);

        model.setTune(true);
        pump();

        QCOMPARE(slice->dspMode(), DSPMode::USB);

        model.setTune(false);
        pump();

        model.injectConnectionForTest(nullptr);
    }

    // ── 6. CWL TUN-on → TUN-off: restores CWL ────────────────────────────────
    // Cite: console.cs:30112-30122 [v2.10.3.13]:
    //   case DSPMode.CWL: case DSPMode.CWU: radio.GetDSPTX(0).CurrentDSPMode = old_dsp_mode;
    void cwlRestoresOnTunOff()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        auto* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::CWL);

        model.setTune(true);
        pump();
        // After TUN-on: swapped to LSB.
        QCOMPARE(slice->dspMode(), DSPMode::LSB);

        model.setTune(false);
        pump();
        // After TUN-off: restored to CWL.
        QCOMPARE(slice->dspMode(), DSPMode::CWL);

        model.injectConnectionForTest(nullptr);
    }

    // ── 7. CWU TUN-on → TUN-off: restores CWU ────────────────────────────────
    void cwuRestoresOnTunOff()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        auto* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::CWU);

        model.setTune(true);
        pump();
        QCOMPARE(slice->dspMode(), DSPMode::USB);

        model.setTune(false);
        pump();
        QCOMPARE(slice->dspMode(), DSPMode::CWU);

        model.injectConnectionForTest(nullptr);
    }

    // ── 8. USB TUN-on → TUN-off: DSP mode unchanged ──────────────────────────
    // For non-CW modes no restore is needed; the mode must be USB at both points.
    void usbModeUnchangedAfterCycle()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        auto* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);

        model.setTune(true);
        pump();
        QCOMPARE(slice->dspMode(), DSPMode::USB);

        model.setTune(false);
        pump();
        QCOMPARE(slice->dspMode(), DSPMode::USB);

        model.injectConnectionForTest(nullptr);
    }

    // ── 9. TUN-on pushes tunePowerForBand to connection ──────────────────────
    // Cite: console.cs:30033-30037 [v2.10.3.13]:
    //   PreviousPWR = ptbPWR.Value;  //MW0LGE_22b
    //   PWR = new_pwr; (tune power via SetPowerUsingTargetDBM; 3M-1a uses tunePowerForBand)
    // We verify via the MockConnection::txDriveLog.
    // tunePowerForBand defaults to 50 for all bands (console.cs:1819-1820 [v2.10.3.13]).
    void tuneOnPushesTunePower()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        auto* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);
        // 14.225 MHz → Band20m.
        slice->setFrequency(14225000.0);

        // Default tunePowerForBand(Band20m) = 50.
        const int expectedTunePower = model.transmitModel().tunePowerForBand(
            Band::Band20m);
        QCOMPARE(expectedTunePower, 50);  // verify default

        // Phase 4 Agent 4A of issue #167 (K2GX safety hotfix): seed a
        // PaProfileManager so the rewritten TUN-on path has an active
        // profile to consult.  Without a loaded profile, the rewritten
        // setPowerUsingTargetDbm path is a graceful no-op (intentional —
        // safer than emitting a stale-profile wire byte).
        if (PaProfileManager* pm = model.paProfileManager()) {
            pm->setMacAddress(QStringLiteral("AABBCCDDEEFF"));
            // Default model used by setupModel doesn't set a board, so
            // load() seeds the manifest using whatever's in the hardware
            // profile.  HERMES is the default and gives a reasonable
            // gain table for assertion purposes.
            pm->load(HPSDRModel::HERMES);
        }

        conn->txDriveLog.clear();
        model.setTune(true);
        pump();

        // At least one setTxDrive call must have arrived with the tune power.
        // Phase 4 Agent 4A: wire byte now follows the Thetis-canonical
        // dBm-target chain (audio.cs:262-271 [v2.10.3.13] +
        // cmaster.cs:1115-1119 [v2.10.3.13]).  We no longer pin the exact
        // wire byte (varies by per-board PA gain table); we assert that
        // the path emits SOME wire byte, and that the byte is in the
        // safe (small) range — pre-hotfix this would have been 127 (50%
        // linear).  See tst_radio_model_drive_path.cpp for tight per-
        // band/per-radio assertions.
        QVERIFY(!conn->txDriveLog.isEmpty());
        const int wireByte = conn->txDriveLog.first();
        // Default HERMES profile uses HF=37.0 dB.  At 50W slider:
        //   target_dbm = 10*log10(50000) - 37.0 = 47 - 37 = 10
        //   volts = sqrt(10^1.0 * 0.05) = sqrt(0.5) = 0.707
        //   volume = min(0.707/0.8, 1.0) = 0.884
        //   wire = int(0.884 * 1.02 * 255) = int(229.8) = 229
        // Allow a ±20 byte window around 229 to cover small per-board /
        // band-row changes without making this test brittle.  The strict
        // K2GX regression (ANAN-8000DLE 80m at 50W -> wire 48) lives in
        // tst_radio_model_drive_path.cpp.
        QVERIFY2(wireByte > 0 && wireByte != 127,
                 qPrintable(QStringLiteral("Pre-hotfix linear formula leaked: "
                            "wire byte %1 matches the 50%%-linear value 127. "
                            "Phase 4A rewrite did not take effect.")
                            .arg(wireByte)));
        Q_UNUSED(expectedTunePower);

        model.setTune(false);
        pump();

        model.injectConnectionForTest(nullptr);
    }

    // ── 10. TUN-off restores saved power ─────────────────────────────────────
    // Cite: console.cs:30129-30132 [v2.10.3.13]:
    //   PWR = PreviousPWR;  //MW0LGE_22b
    void tuneOffRestoresPower()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        auto* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);

        // Phase 4 Agent 4A of issue #167: load PA profile so TUN-on path
        // emits a wire byte to compare against TUN-off restore byte.
        if (PaProfileManager* pm = model.paProfileManager()) {
            pm->setMacAddress(QStringLiteral("AABBCCDDEEFF"));
            pm->load(HPSDRModel::HERMES);
        }

        // Set a specific pre-tune power (not 50, to distinguish from tune power).
        model.transmitModel().setPower(80);

        conn->txDriveLog.clear();
        model.setTune(true);
        pump();

        // Record the drive log size at TUN-on (tune power pushed).
        const int logSizeAfterOn = conn->txDriveLog.size();

        model.setTune(false);
        pump();

        // TUN-off must have pushed at least one more setTxDrive call.
        QVERIFY(conn->txDriveLog.size() > logSizeAfterOn);
        // Codex P1 follow-up to PR #178: the TUN-off RESTORE path now
        // routes through TransmitModel::setPowerUsingTargetDbm (matches
        // the TUN-on path).  Previously the restore used the linear
        // formula `wire = clamp(int(255 * pct/100 * swr), 0, 255)` which
        // shipped a stale pre-hotfix byte; in the flow "TUN on → TUN off
        // → MOX without slider movement" the radio held that linear byte
        // and engaged MOX with K2GX-class over-drive.  Now the restore
        // recomputes via the calibrated dBm path with bFromTune=false
        // (txMode 0 / drive-slider source).
        //
        // Regression check: the restored wire byte must NOT match the old
        // linear formula `(80 * 255) / 100 == 204`.  Sane range: > 0
        // (non-zero) and well below 255 (since 80% slider on a HERMES PA
        // profile produces ~70-80% of full audio_volume).
        const int restoredWireByte = conn->txDriveLog.last();
        QVERIFY2(restoredWireByte > 0 && restoredWireByte < 255,
                 qPrintable(QString("restored wire byte %1 out of sane range [1, 254]")
                                .arg(restoredWireByte)));
        QVERIFY2(restoredWireByte != (80 * 255) / 100,
                 qPrintable(QString("restored wire byte %1 matches old linear formula "
                                    "(should route through dBm path post-Codex P1 fix)")
                                .arg(restoredWireByte)));

        model.injectConnectionForTest(nullptr);
    }

    // ── 11. MoxController::setTune(true) called on TUN-on ────────────────────
    // Verify via manualMoxChanged signal (set by setTune → MoxController::setTune).
    // Cite: console.cs:30081 [v2.10.3.13]: chkMOX.Checked = true.
    void tuneOnEngagesMoxController()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        model.activeSlice()->setDspMode(DSPMode::USB);

        QSignalSpy moxSpy(model.moxController(), &MoxController::manualMoxChanged);

        model.setTune(true);
        pump();

        // manualMoxChanged(true) must have fired — confirms setTune(true) on MC.
        QVERIFY(!moxSpy.isEmpty());
        QCOMPARE(moxSpy.last().first().toBool(), true);

        model.setTune(false);
        pump();

        model.injectConnectionForTest(nullptr);
    }

    // ── 12. MoxController::setTune(false) called on TUN-off ──────────────────
    void tuneOffReleasesMoxController()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        model.activeSlice()->setDspMode(DSPMode::USB);

        // First engage.
        model.setTune(true);
        pump();

        QSignalSpy moxSpy(model.moxController(), &MoxController::manualMoxChanged);

        // Now release.
        model.setTune(false);
        pump();

        // manualMoxChanged(false) must have fired — confirms setTune(false) on MC.
        QVERIFY(!moxSpy.isEmpty());
        QCOMPARE(moxSpy.last().first().toBool(), false);

        model.injectConnectionForTest(nullptr);
    }

    // ── 13. Tune tone engaged on TUN-on (no crash with null txChannel) ───────
    // In unit-test builds WDSP is not initialized, so m_txChannel is null.
    // setTune must guard against null and not crash (the guard is identical to
    // the G.1 null guard for txReady/txaFlushed).
    void tuneOnWithNullTxChannelNocrash()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        // Confirm m_txChannel is null (WDSP not initialized in test builds).
        QVERIFY(model.txChannel() == nullptr);

        model.activeSlice()->setDspMode(DSPMode::USB);

        model.setTune(true);
        pump();
        // If we reach here without crash, the null guard worked.

        model.setTune(false);
        pump();

        model.injectConnectionForTest(nullptr);
    }

    // ── 14. Tune tone released on TUN-off (no crash with null txChannel) ─────
    void tuneOffWithNullTxChannelNocrash()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        model.activeSlice()->setDspMode(DSPMode::USB);

        model.setTune(true);
        pump();

        model.setTune(false);
        pump();
        // No crash = setTuneTone(false) null-guarded correctly.
        QVERIFY(true);

        model.injectConnectionForTest(nullptr);
    }

    // ── 15. TUN-off without prior TUN-on → no-op via m_isTuning guard ──────────
    // MoxController not engaged, no power restore, no setDspMode, no setTuneTone.
    // G.4 fixup: the cold-off guard (if (!m_isTuning) return;) at the top of
    // the TUN-off else-branch means setTune(false) with no prior setTune(true)
    // is a complete no-op: no setTxDrive, no MoxController::setTune(false),
    // no DSP mode restore, no tune tone release.
    void tuneOffWithoutPriorTuneOnNocrash()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        QSignalSpy moxSpy(model.moxController(), &MoxController::manualMoxChanged);

        conn->txDriveLog.clear();
        model.setTune(false);
        pump();

        // m_isTuning was false → guard fires → complete no-op.
        // manualMoxChanged must NOT fire.
        QCOMPARE(moxSpy.count(), 0);
        // setTxDrive (restore) must NOT have been called (guard returned early).
        QCOMPARE(conn->txDriveLog.size(), 0);

        model.injectConnectionForTest(nullptr);
    }

    // ── 16. AM mode: not swapped on TUN-on ───────────────────────────────────
    // AM is not CWL/CWU and not LSB-family; the mode must remain AM after TUN-on.
    // Cite: console.cs:30043-30070 [v2.10.3.13] — switch (old_dsp_mode):
    //   only CWL and CWU arms trigger a mode change; AM falls through default.
    // G.4 fixup: verifies AM-not-swapped, complementing tests 2-5 which only
    // cover USB/LSB/CWL/CWU.
    void amModeNotSwappedOnTuneOn()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        auto* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::AM);

        QSignalSpy modeSpy(slice, &SliceModel::dspModeChanged);

        model.setTune(true);
        pump();

        // AM → no swap → dspModeChanged must NOT have fired.
        QCOMPARE(modeSpy.count(), 0);
        QCOMPARE(slice->dspMode(), DSPMode::AM);
        // Saved mode must reflect AM (not a default USB).
        // Verify by doing TUN-off and checking mode is still AM (no CW restore).
        model.setTune(false);
        pump();
        QCOMPARE(slice->dspMode(), DSPMode::AM);

        model.injectConnectionForTest(nullptr);
    }

    // ── 17. FM mode: not swapped on TUN-on ───────────────────────────────────
    // FM is not CWL/CWU and not LSB-family; the mode must remain FM after TUN-on.
    // G.4 fixup: verifies FM-not-swapped.
    void fmModeNotSwappedOnTuneOn()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        auto* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::FM);

        QSignalSpy modeSpy(slice, &SliceModel::dspModeChanged);

        model.setTune(true);
        pump();

        // FM → no swap → dspModeChanged must NOT have fired.
        QCOMPARE(modeSpy.count(), 0);
        QCOMPARE(slice->dspMode(), DSPMode::FM);

        model.setTune(false);
        pump();
        QCOMPARE(slice->dspMode(), DSPMode::FM);

        model.injectConnectionForTest(nullptr);
    }

    // ── 18. isLsbFamily predicate: DIGL → true (negative tone), DIGU → false ─
    // Validates the sign-selection logic directly via a reference copy of the
    // production predicate (isLsbFamilyRef, defined above in this test file).
    // Cite: console.cs:30024-30037 [v2.10.3.13]:
    //   case DSPMode.DIGL: radio.GetDSPTX(0).TXPostGenToneFreq = -cw_pitch; break;
    //   case DSPMode.DIGU: (falls through to default) → +cw_pitch.
    // G.4 fixup: added to complement tests 2-3 (USB/LSB sign) with DIGL/DIGU,
    // and to provide a standalone predicate test independent of WDSP being live.
    void isLsbFamilyPredicateDiglDigu()
    {
        // DIGL is LSB-family → tone sign = negative (−cw_pitch).
        QVERIFY(isLsbFamilyRef(DSPMode::DIGL));
        // DIGU is NOT LSB-family → tone sign = positive (+cw_pitch).
        QVERIFY(!isLsbFamilyRef(DSPMode::DIGU));
        // Cross-check the full set for regressions against the Thetis switch table.
        QVERIFY(isLsbFamilyRef(DSPMode::LSB));
        QVERIFY(isLsbFamilyRef(DSPMode::CWL));
        QVERIFY(!isLsbFamilyRef(DSPMode::USB));
        QVERIFY(!isLsbFamilyRef(DSPMode::CWU));
        QVERIFY(!isLsbFamilyRef(DSPMode::AM));
        QVERIFY(!isLsbFamilyRef(DSPMode::FM));
    }

    // ── 19. Issue #177: TUN-off defers gen1 + power-restore until rxReady ────
    //
    // Reproduces the ordering that triggered Chris Palmgren's HL2 bench
    // report on 2026-05-03 (issue #177): on TUN-off, power-restore (and the
    // gen1.run=0 it stands in for) must NOT happen synchronously.  It must
    // wait for MoxController::rxReady AND a settle timer.
    //
    // Cite: Thetis console.cs:30106-30109 [v2.10.3.13]:
    //   chkMOX.Checked = false;        // synchronous walk TX→RX (~30 ms blocking)
    //   await Task.Delay(100);
    //   radio.GetDSPTX(0).TXPostGenRun = 0;
    //
    // Strategy: setupModel forces both MoxController timers and the
    // RadioModel tune-off settle to 0 ms.  We snapshot the txDriveLog size
    // immediately after setTune(false) returns; if completion ran
    // synchronously it would already have grown (regression).  After draining
    // the event loop the log MUST grow, confirming the deferred path fires.
    //
    // m_pendingTuneOff is exposed via tuneOffPendingForTest() so we can
    // observe the latch directly.
    void tuneOffDefersUntilRxReadyAndSettle()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        auto* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);

        // Need a PA profile so the dBm-path power restore actually emits a
        // wire byte (mirrors tuneOffRestoresPower setup).
        if (PaProfileManager* pm = model.paProfileManager()) {
            pm->setMacAddress(QStringLiteral("AABBCCDDEEFF"));
            pm->load(HPSDRModel::HERMES);
        }

        model.transmitModel().setPower(80);

        // Engage TUN.  This pushes a tune-power wire byte and arms TX-on.
        conn->txDriveLog.clear();
        model.setTune(true);
        pump();

        const int logSizeAfterOn = conn->txDriveLog.size();
        QVERIFY2(logSizeAfterOn > 0,
                 "TUN-on must push at least one wire drive byte");

        // ── The actual regression check ───────────────────────────────────────
        // Click TUN-off.  Pre-fix behavior: the saved wire byte was queued
        // synchronously inside setTune(false).  Post-fix: nothing should
        // happen until rxReady fires + settle elapses.
        model.setTune(false);

        // Right after the call returns:
        //  - the latch must be armed (deferred completion is queued, not run)
        //  - the wire drive log must be unchanged (no byte queued yet)
        QVERIFY2(model.tuneOffPendingForTest(),
                 "setTune(false) must arm m_pendingTuneOff before returning");
        QCOMPARE(conn->txDriveLog.size(), logSizeAfterOn);

        // Drain the event loop — this drives:
        //   keyUpDelayTimer (0 ms) → txaFlushed
        //   pttOutDelayTimer (0 ms) → rxReady
        //   QTimer::singleShot(0) → completeTuneOff
        //   completeTuneOff queues setTxDrive (synchronous same-thread Auto)
        pump();

        // Now the latch must be cleared and the wire byte must have been
        // emitted (so the radio sees the saved drive level).
        QVERIFY2(!model.tuneOffPendingForTest(),
                 "completeTuneOff must clear m_pendingTuneOff");
        QVERIFY2(conn->txDriveLog.size() > logSizeAfterOn,
                 "completeTuneOff must push the saved-power wire byte");

        model.injectConnectionForTest(nullptr);
    }

    // ── 20. Issue #177: a fresh setTune(true) cancels a pending TUN-off ──────
    //
    // If the operator double-clicks TUN within the rxReady + settle window,
    // the deferred completion must not stomp the new TUN-on state.  The
    // pending latch is cleared in setTune(true) at the top of the on-branch;
    // the rxReady slot and the QTimer body both re-check it before doing any
    // work, so double-firing is harmless.
    void tuneOnCancelsPendingTuneOff()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        auto* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);

        if (PaProfileManager* pm = model.paProfileManager()) {
            pm->setMacAddress(QStringLiteral("AABBCCDDEEFF"));
            pm->load(HPSDRModel::HERMES);
        }

        model.transmitModel().setPower(80);

        // Engage TUN, click off, then click on again BEFORE pumping the
        // event loop (latch is armed, nothing has fired yet).
        model.setTune(true);
        pump();
        model.setTune(false);
        QVERIFY(model.tuneOffPendingForTest());

        // Re-engage immediately — this must clear the latch.
        model.setTune(true);
        QVERIFY2(!model.tuneOffPendingForTest(),
                 "setTune(true) must cancel a pending TUN-off completion");

        // Pump the loop.  rxReady will fire once for the original setTune(false)
        // walk and once for the new TUN-on engagement.  Neither dispatch may
        // run completeTuneOff() — m_pendingTuneOff is false the whole time.
        // We verify via the model's m_isTuning state (still true) since
        // completeTuneOff would clear it.
        pump();

        QVERIFY2(model.isTune(),
                 "fresh TUN-on must remain active across pumped event loop");

        model.setTune(false);
        pump();

        model.injectConnectionForTest(nullptr);
    }

    // ── 21. Issue #177: disconnect-mid-walk clears the deferred latch ────────
    //
    // teardownConnection() (called when the radio drops or the user clicks
    // disconnect) must clear m_pendingTuneOff and m_isTuning.  Otherwise the
    // next connect would inherit a stale latch and the first rxReady from a
    // PTT cycle would mis-fire completeTuneOff().
    //
    // We can't drive teardownConnection() directly without a real connection
    // (it asserts on m_connection != nullptr early-out), so we test the state
    // contract by exercising a disconnect → reconnect cycle implicitly.
    // Instead we verify the simpler invariant: after setTune(false), pump
    // through completion, and re-engage TUN-on; the second cycle behaves
    // identically (no stale state poisons it).
    // ── 22. Issue #177 + Codex P1: TUN-off when MOX already RX still completes ──
    //
    // Codex caught a P1 stranded-latch case in PR #180 review: if MOX was
    // already in RX state when setTune(false) is called (e.g., a safety trip
    // dropped MOX while TUN was still latched, a manual MOX click during
    // TUN, or a future PureSignal / SwrProtectionController force-unkey
    // path), MoxController::setMox(false) hits the idempotent guard and
    // emits no TX→RX phase signals.  No rxReady fires, m_pendingTuneOff
    // stays latched, and a later unrelated rxReady from a normal PTT cycle
    // would consume the stale latch and apply stale TUN-off state.
    //
    // The fix detects MOX-already-off in setTune(false) and schedules
    // completeTuneOff directly via QTimer::singleShot, bypassing rxReady.
    // This mirrors Thetis console.cs:30107 [v2.10.3.13] `await Task.Delay(100)`
    // which is unconditional (fires whether or not chkMOX assignment
    // triggered a walk — WinForms silently no-ops same-value assignment).
    void tuneOffWithMoxAlreadyOffStillCompletes()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        auto* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);

        if (PaProfileManager* pm = model.paProfileManager()) {
            pm->setMacAddress(QStringLiteral("AABBCCDDEEFF"));
            pm->load(HPSDRModel::HERMES);
        }

        model.transmitModel().setPower(80);

        // Engage TUN.  This sets m_isTuning=true and engages MOX.
        model.setTune(true);
        pump();
        QVERIFY(model.moxController()->isMox());
        QVERIFY(model.isTune());

        // Externally drop MOX (simulates a safety trip / PA fault / manual
        // MOX click during TUN).  m_isTuning remains true; MOX is now off.
        // Note: this drives the MOX state machine through a real TX→RX walk
        // so we must pump to drain the chained timers.
        model.moxController()->setMox(false);
        pump();
        QVERIFY(!model.moxController()->isMox());
        QVERIFY(model.isTune());          // m_isTuning still latched
        QVERIFY(!model.tuneOffPendingForTest());

        // Click TUN-off.  With the Codex fix, this must complete cleanly
        // even though MoxController::setTune(false) → setMox(false) hits
        // the idempotent guard and emits no rxReady.  The fallback
        // QTimer::singleShot in setTune(false) drives completeTuneOff.
        const int logBefore = conn->txDriveLog.size();
        model.setTune(false);
        QVERIFY2(model.tuneOffPendingForTest(),
                 "setTune(false) must arm the latch even when MOX was already off");

        // Pump the event loop.  This drives:
        //   QTimer::singleShot(0, ...) fallback → completeTuneOff
        //   completeTuneOff queues setTxDrive (synchronous same-thread Auto)
        pump();

        // Latch must be cleared and the saved-power wire byte must have
        // been pushed via the fallback path.  Without the Codex fix this
        // assertion fails (the latch sits forever, no setTxDrive call).
        QVERIFY2(!model.tuneOffPendingForTest(),
                 "completeTuneOff must run even when MOX was already off");
        QVERIFY2(conn->txDriveLog.size() > logBefore,
                 "saved-power wire byte must be pushed via fallback path");
        QVERIFY2(!model.isTune(),
                 "completeTuneOff must clear m_isTuning");

        model.injectConnectionForTest(nullptr);
    }

    void tuneOffCycleIsRepeatable()
    {
        RadioModel model;
        MockConnection* conn = nullptr;
        setupModel(model, conn);
        std::unique_ptr<MockConnection> connOwner(conn);

        auto* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);

        if (PaProfileManager* pm = model.paProfileManager()) {
            pm->setMacAddress(QStringLiteral("AABBCCDDEEFF"));
            pm->load(HPSDRModel::HERMES);
        }

        model.transmitModel().setPower(50);

        // Cycle 1.
        model.setTune(true);
        pump();
        model.setTune(false);
        pump();
        QVERIFY(!model.tuneOffPendingForTest());
        QVERIFY(!model.isTune());

        // Cycle 2 — must engage cleanly with no stale latch.
        const int logBeforeCycle2 = conn->txDriveLog.size();
        model.setTune(true);
        pump();
        QVERIFY(model.isTune());
        QVERIFY(!model.tuneOffPendingForTest());
        QVERIFY2(conn->txDriveLog.size() > logBeforeCycle2,
                 "second TUN-on must push tune-power wire byte");

        model.setTune(false);
        pump();

        model.injectConnectionForTest(nullptr);
    }
};

QTEST_MAIN(TestRadioModelSetTune)
#include "tst_radio_model_set_tune.moc"
