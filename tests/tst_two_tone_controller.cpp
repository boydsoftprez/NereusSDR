// =================================================================
// tests/tst_two_tone_controller.cpp  (NereusSDR)
// =================================================================
//
// Phase 3M-1c chunk I (I.1-I.5) — TwoToneController activation handler.
//
// Verifies:
//   I.1.A — setActive(true) when not powered → no MOX engaged, no TXPostGen
//           setters fire, twoToneActiveChanged(false) emitted (revert UI).
//   I.1.B — setActive(true) when MOX is on → MOX cycles off, 200 ms settle,
//           then activation continues.
//   I.1.C — setActive(true) → all required TXPostGen setters fire in the
//           expected order; setTxPostGenRun(true) is the last call before
//           setMox(true).
//   I.1.D — setActive(false) → setTxPostGenRun(false) fires after the
//           200 ms deactivation settle; twoToneActiveChanged(false) emitted.
//   I.1.E — Continuous mode: setTxPostGenMode(1), continuous setters fire,
//           pulsed-mode setters do NOT fire.
//   I.1.F — Pulsed mode: setTxPostGenMode(7), pulsed setters fire,
//           continuous setters do NOT fire.  Pulse profile (window/duty/ramp/IQout)
//           also applied.
//   I.1.G — 0.49999 magnitude scaling: ttmag1/ttmag2 = 0.49999 * 10^(level/20).
//   I.1.H — Mode-aware invert: LSB + invert=true → frequencies sign-flipped.
//           USB + invert=true → no flip.
//   I.2   — Freq2Delay > 0 → Mag2 starts at 0, transitions to ttmag2 after delay.
//   I.3   — TUN auto-stop: currently a TODO (no isTuneToneActive() getter).
//           Test verifies the path is ready for the wiring.
//   I.4.A — DrivePowerSource::Fixed → PWR is snapshotted on activate
//           (current power becomes m_savedPwr) and restored on deactivate.
//   I.4.B — DrivePowerSource::DriveSlider → no override; PWR unchanged.
//   I.5   — BandPlanGuard rejects CW mode → setMox(true) emits moxRejected,
//           TwoToneController cleans up state and emits twoToneActiveChanged(false).
//
// =================================================================

// no-port-check: NereusSDR-original test file. All Thetis source cites are
// in TwoToneController.h/cpp.

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QSignalSpy>

#include "core/TwoToneController.h"
#include "core/MoxController.h"
#include "core/TxChannel.h"
#include "core/TxInterlockPolicy.h"
#include "core/safety/BandPlanGuard.h"
#include "core/WdspTypes.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;
using namespace NereusSDR::safety;

// ---------------------------------------------------------------------------
// RecordingTxChannel — overrides the 13 TXPostGen setters to record the
// call sequence. Production methods early-return when WDSP is not initialised
// (the txa[] gen1.p pointer is null), so subclassing is purely for test
// observability.
// ---------------------------------------------------------------------------
class RecordingTxChannel : public TxChannel {
public:
    explicit RecordingTxChannel(int channelId) : TxChannel(channelId) {}

    struct Call {
        QString method;
        double  arg1{0.0};
        double  arg2{0.0};
    };

    QVector<Call> calls;

    void setTxPostGenMode(int mode) override {
        calls.append({QStringLiteral("setTxPostGenMode"), double(mode), 0.0});
    }
    void setTxPostGenTTFreq1(double hz) override {
        calls.append({QStringLiteral("setTxPostGenTTFreq1"), hz, 0.0});
    }
    void setTxPostGenTTFreq2(double hz) override {
        calls.append({QStringLiteral("setTxPostGenTTFreq2"), hz, 0.0});
    }
    void setTxPostGenTTMag1(double linear) override {
        calls.append({QStringLiteral("setTxPostGenTTMag1"), linear, 0.0});
    }
    void setTxPostGenTTMag2(double linear) override {
        calls.append({QStringLiteral("setTxPostGenTTMag2"), linear, 0.0});
    }
    void setTxPostGenTTPulseToneFreq1(double hz) override {
        calls.append({QStringLiteral("setTxPostGenTTPulseToneFreq1"), hz, 0.0});
    }
    void setTxPostGenTTPulseToneFreq2(double hz) override {
        calls.append({QStringLiteral("setTxPostGenTTPulseToneFreq2"), hz, 0.0});
    }
    void setTxPostGenTTPulseMag1(double linear) override {
        calls.append({QStringLiteral("setTxPostGenTTPulseMag1"), linear, 0.0});
    }
    void setTxPostGenTTPulseMag2(double linear) override {
        calls.append({QStringLiteral("setTxPostGenTTPulseMag2"), linear, 0.0});
    }
    void setTxPostGenTTPulseFreq(int hz) override {
        calls.append({QStringLiteral("setTxPostGenTTPulseFreq"), double(hz), 0.0});
    }
    void setTxPostGenTTPulseDutyCycle(double pct) override {
        calls.append({QStringLiteral("setTxPostGenTTPulseDutyCycle"), pct, 0.0});
    }
    void setTxPostGenTTPulseTransition(double sec) override {
        calls.append({QStringLiteral("setTxPostGenTTPulseTransition"), sec, 0.0});
    }
    void setTxPostGenTTPulseIQOut(bool on) override {
        calls.append({QStringLiteral("setTxPostGenTTPulseIQOut"), on ? 1.0 : 0.0, 0.0});
    }
    void setTxPostGenRun(bool on) override {
        calls.append({QStringLiteral("setTxPostGenRun"), on ? 1.0 : 0.0, 0.0});
    }

    int countCalls(const QString& method) const {
        int n = 0;
        for (const auto& c : calls) {
            if (c.method == method) ++n;
        }
        return n;
    }
    int firstIndexOf(const QString& method) const {
        for (int i = 0; i < calls.size(); ++i) {
            if (calls[i].method == method) return i;
        }
        return -1;
    }
    Call findCall(const QString& method) const {
        for (const auto& c : calls) {
            if (c.method == method) return c;
        }
        return {};
    }
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static constexpr int kTxChannelId = 1; // matches TxChannel test convention

// Build a MoxCheckFn that always rejects calls (simulates BandPlanGuard
// rejecting CW mode for SSB-only TX).
static MoxController::MoxCheckFn makeRejectingCheckFn(DSPMode mode)
{
    return [mode]() -> BandPlanGuard::MoxCheckResult {
        BandPlanGuard guard;
        return guard.checkMoxAllowed(
            Region::UnitedStates,
            14'200'000,
            mode,
            Band::Band20m, Band::Band20m,
            /*preventDifferentBand=*/false,
            /*extended=*/false);
    };
}

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------
class TestTwoToneController : public QObject
{
    Q_OBJECT

private slots:
    void supersededPendingStartNeverProgramsOrRekeys()
    {
        TransmitModel tx; RecordingTxChannel tc(kTxChannelId); MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0); mox.setMox(true);
        TwoToneController ctrl; ctrl.setTransmitModel(&tx); ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox); ctrl.setSettleDelaysMs(1000, 0);
        KeyerIdentity cat = KeyerIdentity::station(PttMode::Manual); cat.program = true; cat.requestTag = 73;
        ctrl.setActive(true, cat); QVERIFY(ctrl.isActivationInFlight());
        mox.setMox(true); const int calls = tc.calls.size();
        QVERIFY(QMetaObject::invokeMethod(&ctrl, "onMoxReleaseSettleElapsed", Qt::DirectConnection));
        QCOMPARE(tc.calls.size(), calls); QVERIFY(mox.isMox());
        QVERIFY(!ctrl.isActivationInFlight());
    }

    void rejectedRepeatRetainsTaggedCycle()
    {
        TransmitModel tx; RecordingTxChannel tc(kTxChannelId); MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        TwoToneController ctrl; ctrl.setTransmitModel(&tx); ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox); ctrl.setSettleDelaysMs(0, 0);
        KeyerIdentity cat = KeyerIdentity::station(PttMode::Manual); cat.program = true; cat.requestTag = 65;
        ctrl.setActive(true, cat); QVERIFY(ctrl.isActive());
        const quint64 stamp = mox.acceptedRequestGeneration(); const int calls = tc.calls.size();
        mox.setMoxCheck([] { return BandPlanGuard::MoxCheckResult{false, QStringLiteral("repeat refused")}; });
        ctrl.setActive(true);
        QCOMPARE(mox.acceptedRequestGeneration(), stamp);
        QCOMPARE(ctrl.keyer().requestTag, quint64(65)); QCOMPARE(tc.calls.size(), calls);
        QVERIFY(ctrl.endIfRequest(65, stamp));
    }

    void supersededDelayedMag2DoesNotTouchGenerator()
    {
        TransmitModel tx; RecordingTxChannel tc(kTxChannelId); MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        TwoToneController ctrl; ctrl.setTransmitModel(&tx); ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox); ctrl.setSettleDelaysMs(0, 0);
        tx.setTwoToneFreq2Delay(1000);
        KeyerIdentity cat = KeyerIdentity::station(PttMode::Manual); cat.program = true; cat.requestTag = 63;
        ctrl.setActive(true, cat); QVERIFY(ctrl.isActive());
        mox.setMox(true);
        const int calls = tc.calls.size();
        QVERIFY(QMetaObject::invokeMethod(&ctrl, "onFreq2DelayElapsed", Qt::DirectConnection));
        QCOMPARE(tc.calls.size(), calls);
    }

    void staleDeactivationCannotRestoreNewCycle()
    {
        TransmitModel tx; RecordingTxChannel tc(kTxChannelId); MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        TwoToneController ctrl; ctrl.setTransmitModel(&tx); ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox); ctrl.setSettleDelaysMs(0, 0);
        KeyerIdentity cat = KeyerIdentity::station(PttMode::Manual); cat.program = true; cat.requestTag = 64;
        ctrl.setActive(true, cat); QVERIFY(ctrl.isActive());
        QVERIFY(ctrl.endIfRequest(64, mox.acceptedRequestGeneration()));
        ctrl.setActive(true);
        const int calls = tc.calls.size();
        QVERIFY(QMetaObject::invokeMethod(&ctrl, "onDeactivationSettleElapsed", Qt::DirectConnection));
        QCOMPARE(tc.calls.size(), calls);
    }


    void taggedRepeatAdoptsCycleAndStaleEndDoesNothing()
    {
        TransmitModel tx;
        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx); ctrl.setTxChannel(&tc); ctrl.setMoxController(&mox);
        ctrl.setSettleDelaysMs(0, 0);
        KeyerIdentity cat = KeyerIdentity::station(PttMode::Manual);
        cat.program = true; cat.requestTag = 51;
        ctrl.setActive(true, cat);
        QVERIFY(ctrl.isActive());
        const quint64 stamp = mox.acceptedRequestGeneration();
        const int calls = tc.calls.size();
        ctrl.setActive(true);
        QCOMPARE(tc.calls.size(), calls);
        QCOMPARE(ctrl.keyer().requestTag, quint64(0));
        QVERIFY(!ctrl.endIfRequest(51, stamp));
        QCOMPARE(tc.calls.size(), calls);
        QVERIFY(mox.isMox());
    }


    // ── I.1.A: power-off precondition ─────────────────────────────────────
    void setActive_powerOff_doesNotEngage()
    {
        TransmitModel tx;
        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);
        ctrl.setPowerOn(false);

        QSignalSpy activeSpy(&ctrl, &TwoToneController::twoToneActiveChanged);

        ctrl.setActive(true);
        QCoreApplication::processEvents();

        QVERIFY(!ctrl.isActive());
        QVERIFY(!mox.isMox());
        QCOMPARE(tc.calls.size(), 0);
        // No state transition (was already inactive), so signal should not fire.
        QCOMPARE(activeSpy.count(), 0);
    }

    // ── I.1.B: MOX-on first → cycle off + settle + continue ───────────────
    void setActive_moxOnFirst_cyclesOffThenContinues()
    {
        TransmitModel tx;
        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        // Engage MOX first.
        mox.setMox(true);
        QCoreApplication::processEvents();
        QVERIFY(mox.isMox());

        QSignalSpy activeSpy(&ctrl, &TwoToneController::twoToneActiveChanged);

        ctrl.setActive(true);
        // Drain any pending settle timer + MOX walk + activation sequence.
        for (int i = 0; i < 10; ++i) {
            QCoreApplication::processEvents();
        }

        QVERIFY(ctrl.isActive());
        QVERIFY(mox.isMox()); // re-engaged after settle
        QCOMPARE(activeSpy.count(), 1);
        QCOMPARE(activeSpy[0][0].toBool(), true);
        QVERIFY(tc.countCalls(QStringLiteral("setTxPostGenRun")) >= 1);
    }

    // ── I.1.C: setter ordering — Run is the last call ─────────────────────
    void setActive_setterOrder_runIsLastBeforeMox()
    {
        TransmitModel tx;
        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        ctrl.setActive(true);
        QCoreApplication::processEvents();

        QVERIFY(ctrl.isActive());
        // setTxPostGenRun must appear in the calls list.
        const int runIdx = tc.firstIndexOf(QStringLiteral("setTxPostGenRun"));
        QVERIFY(runIdx >= 0);
        // It should be the LAST TXPostGen call before MOX engages.
        // Verify it's after the mode setter and the freq/mag setters.
        const int modeIdx = tc.firstIndexOf(QStringLiteral("setTxPostGenMode"));
        QVERIFY(modeIdx >= 0);
        QVERIFY(modeIdx < runIdx);
    }

    // ── I.1.D: deactivation — setTxPostGenRun(false) after settle ─────────
    void setActive_deactivation_stopsRunAndEmitsSignal()
    {
        TransmitModel tx;
        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        ctrl.setActive(true);
        QCoreApplication::processEvents();
        QVERIFY(ctrl.isActive());
        // Task 7: two-tone holds the manual key (console.ManualMox,
        // setup.cs:11162 [v2.10.3.15]), so a mic release cannot unkey it.
        QVERIFY(mox.isManualKey());
        mox.onMicPttFromRadio(false);
        QCoreApplication::processEvents();
        QVERIFY(mox.isMox());
        tc.calls.clear();

        QSignalSpy activeSpy(&ctrl, &TwoToneController::twoToneActiveChanged);

        ctrl.setActive(false);
        // Drain settle timer + the deactivation continuation.
        for (int i = 0; i < 10; ++i) {
            QCoreApplication::processEvents();
        }

        QVERIFY(!ctrl.isActive());
        QVERIFY(!mox.isMox());
        // console.ManualMox = false after the settle (setup.cs:11193).
        QVERIFY(!mox.isManualKey());
        QCOMPARE(activeSpy.count(), 1);
        QCOMPARE(activeSpy[0][0].toBool(), false);
        // setTxPostGenRun(false) should have fired.
        bool sawRunOff = false;
        for (const auto& c : tc.calls) {
            if (c.method == QStringLiteral("setTxPostGenRun") && c.arg1 == 0.0) {
                sawRunOff = true;
                break;
            }
        }
        QVERIFY(sawRunOff);
    }

    // ── I.1.E: continuous mode → mode=1, continuous setters fire ──────────
    void setActive_continuousMode_emitsContinuousSetters()
    {
        TransmitModel tx;
        tx.setTwoTonePulsed(false);
        tx.setTwoToneFreq1(700);
        tx.setTwoToneFreq2(1900);
        tx.setTwoToneFreq2Delay(0); // immediate Mag2

        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        SliceModel slice; // default USB → no invert

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        ctrl.setActive(true);
        QCoreApplication::processEvents();

        // Mode = 1 (continuous).
        const auto mc = tc.findCall(QStringLiteral("setTxPostGenMode"));
        QCOMPARE(mc.arg1, 1.0);

        // Continuous setters fired.
        QCOMPARE(tc.countCalls(QStringLiteral("setTxPostGenTTFreq1")), 1);
        QCOMPARE(tc.countCalls(QStringLiteral("setTxPostGenTTFreq2")), 1);
        QCOMPARE(tc.countCalls(QStringLiteral("setTxPostGenTTMag1")), 1);
        QCOMPARE(tc.countCalls(QStringLiteral("setTxPostGenTTMag2")), 1);

        // Pulsed-mode setters did NOT fire.
        QCOMPARE(tc.countCalls(QStringLiteral("setTxPostGenTTPulseToneFreq1")), 0);
        QCOMPARE(tc.countCalls(QStringLiteral("setTxPostGenTTPulseToneFreq2")), 0);
        QCOMPARE(tc.countCalls(QStringLiteral("setTxPostGenTTPulseMag1")), 0);
        QCOMPARE(tc.countCalls(QStringLiteral("setTxPostGenTTPulseFreq")), 0);

        // Frequency arguments match TransmitModel.
        QCOMPARE(tc.findCall(QStringLiteral("setTxPostGenTTFreq1")).arg1, 700.0);
        QCOMPARE(tc.findCall(QStringLiteral("setTxPostGenTTFreq2")).arg1, 1900.0);
    }

    // ── I.1.F: pulsed mode → mode=7, pulse profile + pulsed setters ───────
    void setActive_pulsedMode_emitsPulsedSetters()
    {
        TransmitModel tx;
        tx.setTwoTonePulsed(true);
        tx.setTwoToneFreq1(700);
        tx.setTwoToneFreq2(1900);
        tx.setTwoToneFreq2Delay(0);

        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        ctrl.setActive(true);
        QCoreApplication::processEvents();

        // Mode = 7 (pulsed).
        const auto mc = tc.findCall(QStringLiteral("setTxPostGenMode"));
        QCOMPARE(mc.arg1, 7.0);

        // Pulse profile setters fired with Designer defaults.
        QCOMPARE(tc.findCall(QStringLiteral("setTxPostGenTTPulseFreq")).arg1, 10.0); // window=10pps
        QCOMPARE(tc.findCall(QStringLiteral("setTxPostGenTTPulseDutyCycle")).arg1, 0.25); // 25%
        QCOMPARE(tc.findCall(QStringLiteral("setTxPostGenTTPulseTransition")).arg1, 0.009); // 9ms
        QCOMPARE(tc.findCall(QStringLiteral("setTxPostGenTTPulseIQOut")).arg1, 1.0); // true

        // Pulsed-tone setters fired.
        QCOMPARE(tc.countCalls(QStringLiteral("setTxPostGenTTPulseToneFreq1")), 1);
        QCOMPARE(tc.countCalls(QStringLiteral("setTxPostGenTTPulseToneFreq2")), 1);
        QCOMPARE(tc.countCalls(QStringLiteral("setTxPostGenTTPulseMag1")), 1);
        QCOMPARE(tc.countCalls(QStringLiteral("setTxPostGenTTPulseMag2")), 1);

        // Continuous setters did NOT fire.
        QCOMPARE(tc.countCalls(QStringLiteral("setTxPostGenTTFreq1")), 0);
        QCOMPARE(tc.countCalls(QStringLiteral("setTxPostGenTTFreq2")), 0);
    }

    // ── I.1.G: 0.49999 magnitude scaling formula ─────────────────────────
    void setActive_magnitudeScaling_appliesFormula()
    {
        TransmitModel tx;
        tx.setTwoTonePulsed(false);
        tx.setTwoToneLevel(0.0);   // 10^0 = 1.0  → ttmag = 0.49999 * 1.0
        tx.setTwoToneFreq2Delay(0);

        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        ctrl.setActive(true);
        QCoreApplication::processEvents();

        const double mag1 = tc.findCall(QStringLiteral("setTxPostGenTTMag1")).arg1;
        QVERIFY(qFuzzyCompare(mag1, 0.49999));

        // -6 dB: 10^(-6/20) ≈ 0.5012 → ttmag ≈ 0.49999 * 0.5012 ≈ 0.25058...
        tc.calls.clear();
        ctrl.setActive(false);
        for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

        tx.setTwoToneLevel(-6.0);
        ctrl.setActive(true);
        QCoreApplication::processEvents();

        const double mag1_minus6 = tc.findCall(QStringLiteral("setTxPostGenTTMag1")).arg1;
        const double expected = 0.49999 * std::pow(10.0, -6.0 / 20.0);
        QVERIFY(qFuzzyCompare(mag1_minus6, expected));
    }

    // ── I.1.H: mode-aware invert ─────────────────────────────────────────
    void setActive_invertTones_LSB_flipsFreq()
    {
        TransmitModel tx;
        tx.setTwoTonePulsed(false);
        tx.setTwoToneFreq1(700);
        tx.setTwoToneFreq2(1900);
        tx.setTwoToneInvert(true);
        tx.setTwoToneFreq2Delay(0);

        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        SliceModel slice;
        slice.setDspMode(DSPMode::LSB);

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        ctrl.setActive(true);
        QCoreApplication::processEvents();

        QCOMPARE(tc.findCall(QStringLiteral("setTxPostGenTTFreq1")).arg1, -700.0);
        QCOMPARE(tc.findCall(QStringLiteral("setTxPostGenTTFreq2")).arg1, -1900.0);
    }

    void setActive_invertTones_USB_doesNotFlip()
    {
        TransmitModel tx;
        tx.setTwoTonePulsed(false);
        tx.setTwoToneFreq1(700);
        tx.setTwoToneFreq2(1900);
        tx.setTwoToneInvert(true);
        tx.setTwoToneFreq2Delay(0);

        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        SliceModel slice;
        slice.setDspMode(DSPMode::USB);

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        ctrl.setActive(true);
        QCoreApplication::processEvents();

        QCOMPARE(tc.findCall(QStringLiteral("setTxPostGenTTFreq1")).arg1, 700.0);
        QCOMPARE(tc.findCall(QStringLiteral("setTxPostGenTTFreq2")).arg1, 1900.0);
    }

    // ── I.2: Freq2Delay > 0 → Mag2 starts at 0, applied after delay ──────
    void setActive_freq2Delay_defersMag2()
    {
        TransmitModel tx;
        tx.setTwoTonePulsed(false);
        tx.setTwoToneLevel(0.0);  // ttmag = 0.49999
        tx.setTwoToneFreq2Delay(50); // 50ms delay

        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        ctrl.setActive(true);
        QCoreApplication::processEvents();

        // Initial Mag2 should be 0.0 because of the delay.
        QCOMPARE(tc.findCall(QStringLiteral("setTxPostGenTTMag2")).arg1, 0.0);

        // Wait for the freq2 delay to elapse; track the second Mag2 call.
        QTRY_COMPARE_WITH_TIMEOUT(tc.countCalls(QStringLiteral("setTxPostGenTTMag2")), 2, 500);

        // Find the LAST setTxPostGenTTMag2 call — it should equal 0.49999.
        double lastMag2 = -1.0;
        for (const auto& c : tc.calls) {
            if (c.method == QStringLiteral("setTxPostGenTTMag2")) {
                lastMag2 = c.arg1;
            }
        }
        QVERIFY(qFuzzyCompare(lastMag2, 0.49999));
    }

    // ── I.4.A: Fixed power source → snapshots and restores PWR ───────────
    void setActive_fixedPowerSource_snapshotsAndRestoresPwr()
    {
        TransmitModel tx;
        tx.setTwoTonePulsed(false);
        tx.setTwoToneFreq2Delay(0);
        tx.setPower(75);                                          // pre-test PWR
        tx.setTwoTonePower(40);                                   // override
        tx.setTwoToneDrivePowerSource(DrivePowerSource::Fixed);

        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        ctrl.setActive(true);
        QCoreApplication::processEvents();

        QVERIFY(ctrl.isActive());
        QCOMPARE(tx.power(), 40);   // overridden to twoTonePower

        ctrl.setActive(false);
        for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

        QCOMPARE(tx.power(), 75);   // restored
    }

    // ── I.4.B: DriveSlider mode → no PWR override ────────────────────────
    void setActive_driveSliderPowerSource_doesNotOverridePwr()
    {
        TransmitModel tx;
        tx.setTwoTonePulsed(false);
        tx.setTwoToneFreq2Delay(0);
        tx.setPower(75);
        tx.setTwoTonePower(40);
        tx.setTwoToneDrivePowerSource(DrivePowerSource::DriveSlider);

        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        ctrl.setActive(true);
        QCoreApplication::processEvents();

        QCOMPARE(tx.power(), 75); // unchanged

        ctrl.setActive(false);
        for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

        QCOMPARE(tx.power(), 75); // still unchanged
    }

    // ── I.5: BandPlanGuard rejects CW → cleanup state ────────────────────
    void setActive_bandPlanGuardRejectsCw_cleansUpState()
    {
        TransmitModel tx;
        tx.setTwoTonePulsed(false);
        tx.setTwoToneFreq2Delay(0);

        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        // BandPlanGuard rejects CW.
        mox.setMoxCheck(makeRejectingCheckFn(DSPMode::CWL));

        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        QSignalSpy moxRejectedSpy(&mox, &MoxController::moxRejected);
        QSignalSpy activeSpy(&ctrl, &TwoToneController::twoToneActiveChanged);

        ctrl.setActive(true);
        QCoreApplication::processEvents();

        // BandPlanGuard rejected the setMox(true) call.
        QCOMPARE(moxRejectedSpy.count(), 1);
        QVERIFY(!mox.isMox());

        // TwoToneController cleaned up its state.
        QVERIFY(!ctrl.isActive());
        // Task 7: the refused key leaves no manual key behind.
        QVERIFY(!mox.isManualKey());

        // twoToneActiveChanged(false) was emitted (so UI can revert highlight).
        QVERIFY(activeSpy.count() >= 1);
        // The LAST emit should be false.
        QCOMPARE(activeSpy.last().at(0).toBool(), false);
    }

    // ── R-R3-36: a rejection of someone else's press is not ours ─────────
    // Live two-tone: a later press refused by the check leaves the
    // generator, PWR and MOX as they were.
    void rejectedOtherPressWhileActive_keepsTwoTone()
    {
        TransmitModel tx;
        tx.setTwoTonePulsed(false);
        tx.setTwoToneFreq2Delay(0);
        tx.setTwoToneDrivePowerSource(DrivePowerSource::Fixed);
        tx.setTwoTonePower(10);
        tx.setPower(75);

        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        bool allow = true;
        mox.setMoxCheck([&allow]() -> BandPlanGuard::MoxCheckResult {
            return {allow, allow ? QString() : QStringLiteral("refused")};
        });
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        ctrl.setActive(true);
        QCoreApplication::processEvents();
        QVERIFY(ctrl.isActive());
        QVERIFY(mox.isMox());
        QCOMPARE(tx.power(), 10);
        QSignalSpy activeSpy(&ctrl, &TwoToneController::twoToneActiveChanged);
        const auto runOffCount = [&tc] {
            int n = 0;
            for (const auto& c : tc.calls) {
                if (c.method == QLatin1String("setTxPostGenRun") && c.arg1 == 0.0) {
                    ++n;
                }
            }
            return n;
        };
        const int runOff0 = runOffCount();

        allow = false;
        mox.setMox(true);
        QCoreApplication::processEvents();

        QVERIFY(ctrl.isActive());
        QVERIFY(mox.isMox());
        QCOMPARE(activeSpy.count(), 0);
        QCOMPARE(tx.power(), 10);
        QCOMPARE(runOffCount(), runOff0);

        allow = true;
        ctrl.setActive(false);
        for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();
        QVERIFY(!ctrl.isActive());
        QCOMPARE(tx.power(), 75);
    }

    // Inside the MOX-release settle: a refused unrelated press does not
    // abandon the start; two-tone's own key at the end of the walk still
    // lands.
    void rejectedOtherPressDuringSettle_startContinues()
    {
        TransmitModel tx;
        tx.setTwoTonePulsed(false);
        tx.setTwoToneFreq2Delay(0);

        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        bool allow = true;
        mox.setMoxCheck([&allow]() -> BandPlanGuard::MoxCheckResult {
            return {allow, allow ? QString() : QStringLiteral("refused")};
        });
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(100, 0);

        mox.setMox(true);
        QCoreApplication::processEvents();
        QVERIFY(mox.isMox());

        ctrl.setActive(true);
        QVERIFY(ctrl.isActivationInFlight());
        QVERIFY(!mox.isMox());

        allow = false;
        mox.setMox(true);
        QVERIFY(ctrl.isActivationInFlight());
        allow = true;

        QTRY_VERIFY_WITH_TIMEOUT(ctrl.isActive(), 2000);
        QVERIFY(mox.isMox());

        ctrl.setActive(false);
        QTRY_VERIFY_WITH_TIMEOUT(!ctrl.isActive(), 2000);
    }

    // ── Task 7 fix wave, I2: TX inhibit and the PA trip refuse two-tone ──
    // Thetis disables chk2TONE while inhibited (console.cs:15354
    // [v2.10.3.15]) and aborts any key while the PA is tripped
    // (console.cs:29364-29371). Two-tone must not key, and must clean up
    // (generator off, not active) as a refused key does.
    void setActive_blockedByInhibitOrPaTrip_doesNotKey_data()
    {
        QTest::addColumn<bool>("paTrip");
        QTest::newRow("tx inhibit") << false;
        QTest::newRow("pa trip") << true;
    }

    void setActive_blockedByInhibitOrPaTrip_doesNotKey()
    {
        QFETCH(bool, paTrip);
        TransmitModel tx;
        tx.setTwoTonePulsed(false);
        tx.setTwoToneFreq2Delay(0);

        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        if (paTrip) { mox.setPaTripped(true); } else { mox.setTxInhibited(true); }
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);
        QSignalSpy activeSpy(&ctrl, &TwoToneController::twoToneActiveChanged);

        ctrl.setActive(true);
        for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

        QVERIFY2(!mox.isMox(), "two-tone keyed while blocked");
        QVERIFY(!ctrl.isActive());
        QVERIFY(!ctrl.isActivationInFlight());
        QVERIFY(!activeSpy.isEmpty());
        QCOMPARE(activeSpy.last().at(0).toBool(), false);
        QVERIFY(!tc.calls.isEmpty());
        QCOMPARE(tc.calls.last().method, QStringLiteral("setTxPostGenRun"));
        QCOMPARE(tc.calls.last().arg1, 0.0);
    }

    // ── Task 7 fix wave, M9: two-tone waits out a TUN-off still running ──
    // From Thetis chk2TONE_CheckedChanged, console.cs:44805-44813
    // [v2.10.3.15]: with TUN on, TUN is turned off and two-tone waits
    // 300 ms (await Task.Delay(300)) before starting, so the tune tone is
    // down before two-tone keys. Pressed while a TUN-off is still
    // completing, two-tone must not key until it has.
    void setActive_duringTuneOff_waitsForItToComplete()
    {
        TransmitModel tx;
        tx.setTwoTonePulsed(false);
        tx.setTwoToneFreq2Delay(0);

        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        SliceModel slice;
        bool tuneOffPending = true;
        mox.setManualKey(true);   // TUN holds it until its TUN-off completes

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 20);
        ctrl.setTuneOffPendingFn([&tuneOffPending] { return tuneOffPending; });

        ctrl.setActive(true);
        for (int i = 0; i < 5; ++i) QCoreApplication::processEvents();
        QVERIFY2(!mox.isMox(), "two-tone keyed while the TUN-off was still running");
        QVERIFY(ctrl.isActivationInFlight());
        QTest::qWait(60);
        QVERIFY2(!mox.isMox(), "two-tone keyed while the TUN-off was still running");

        // TUN-off completes (tone down, manual key cleared).
        tuneOffPending = false;
        mox.setManualKey(false);
        QTRY_VERIFY_WITH_TIMEOUT(ctrl.isActive(), 2000);
        QVERIFY(mox.isMox());

        ctrl.setActive(false);
        QTRY_VERIFY_WITH_TIMEOUT(!ctrl.isActive(), 2000);
    }

    // ── Task 7 fix wave, M2: two-tone and the TX interlock ─────────────────
    // A TX-interlock refusal must let two-tone clean up, as a band-plan
    // refusal does: never active while unkeyed, generator off.
    void setActive_interlockRefusal_cleansUpState()
    {
        TransmitModel tx;
        tx.setTwoTonePulsed(false);
        tx.setTwoToneFreq2Delay(0);
        RecordingTxChannel tc(kTxChannelId);
        TxInterlockPolicy policy;
        policy.setMode(TxInterlockPolicy::Block);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        mox.setInterlockPolicy(&policy);
        mox.onAmpStateChanged(/*hasAmp=*/true, /*inOperate=*/false);
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);
        QSignalSpy activeSpy(&ctrl, &TwoToneController::twoToneActiveChanged);

        ctrl.setActive(true);
        for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

        QVERIFY(!mox.isMox());
        QVERIFY2(!ctrl.isActive(), "two-tone committed as active while unkeyed");
        QVERIFY(!ctrl.isActivationInFlight());
        QVERIFY(!mox.isManualKey());
        QVERIFY(!activeSpy.isEmpty());
        QCOMPARE(activeSpy.last().at(0).toBool(), false);
        QCOMPARE(tc.calls.last().method, QStringLiteral("setTxPostGenRun"));
        QCOMPARE(tc.calls.last().arg1, 0.0);
        mox.setInterlockPolicy(nullptr);
        policy.setMode(TxInterlockPolicy::Disabled);
    }

    // A band-plan refusal keeps the manual key through the 200 ms settle,
    // as Thetis's refused start runs the stop branch (setup.cs:11190-11193
    // [v2.10.3.15]: console.MOX = false; await Task.Delay(200); ...
    // console.ManualMox = false;). A held mic is not tried again inside
    // two-tone's own refusal.
    void setActive_bandPlanRefusal_keepsManualKeyThroughSettle()
    {
        TransmitModel tx;
        tx.setTwoTonePulsed(false);
        tx.setTwoToneFreq2Delay(0);
        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        mox.setMoxCheck(makeRejectingCheckFn(DSPMode::CWL));
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(50, 0);
        QSignalSpy rejected(&mox, &MoxController::moxRejected);

        // The mic is pressed while a manual key holds it off, so it has not
        // been tried (or refused) before two-tone starts.
        mox.setManualKey(true);
        mox.onMicPttFromRadio(true);
        const int rejectedBefore = rejected.count();
        QCOMPARE(rejectedBefore, 0);

        ctrl.setActive(true);
        QVERIFY(!mox.isMox());
        QVERIFY(!ctrl.isActive());
        QCOMPARE(rejected.count(), rejectedBefore + 1);   // two-tone's own only
        QVERIFY2(mox.isManualKey(), "the refusal cleared the manual key at once");
        mox.onMicPttFromRadio(true);
        QCOMPARE(rejected.count(), rejectedBefore + 1);

        QTRY_VERIFY_WITH_TIMEOUT(!mox.isManualKey(), 2000);
    }

    // ── Fix wave RD-I4: never committed while unkeyed ──────────────────────
    // The holder gate admits the start, then takes transmit at the key
    // itself (a take answers without moxRejected). Thetis setup.cs:11165-
    // 11170 [v2.10.3.15]: if (!console.MOX) { chkTestIMD.Checked = false;
    // return; }
    void setActive_keyTakenWithoutRefusal_doesNotCommit()
    {
        TransmitModel tx;
        tx.setTwoTonePulsed(false);
        tx.setTwoToneFreq2Delay(0);
        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        int asked = 0;
        mox.setKeyingGate([&asked](PttMode, const KeyerIdentity&) {
            KeyingAnswer answer;
            // admitKey asks first; the key itself is taken.
            answer.verdict = (asked++ == 0) ? KeyingVerdict::Admit : KeyingVerdict::Take;
            return answer;
        });
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);
        QSignalSpy activeSpy(&ctrl, &TwoToneController::twoToneActiveChanged);

        ctrl.setActive(true);
        for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();

        QCOMPARE(asked, 2);
        QVERIFY(!mox.isMox());
        QVERIFY2(!ctrl.isActive(), "two-tone committed as active while unkeyed");
        QVERIFY(!ctrl.isActivationInFlight());
        QVERIFY(!tx.isTwoToneActive());
        QVERIFY(!activeSpy.isEmpty());
        QCOMPARE(activeSpy.last().at(0).toBool(), false);
        QCOMPARE(tc.calls.last().method, QStringLiteral("setTxPostGenRun"));
        QCOMPARE(tc.calls.last().arg1, 0.0);
        QTRY_VERIFY_WITH_TIMEOUT(!mox.isManualKey(), 2000);
    }

    // ── Fix round 1 (minor 3): an abandoned start under another device's
    // key clears its manual key. That key is not the station's, as
    // MoxController::onMoxButton clears _manual_mox for a key refused while
    // another device's key is on; left set, it holds off every PTT source.
    void abandonedStartUnderAnotherDevicesKey_clearsTheManualKey()
    {
        TransmitModel tx;
        tx.setTwoTonePulsed(false);
        tx.setTwoToneFreq2Delay(0);
        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        int asked = 0;
        mox.setKeyingGate([&asked](PttMode, const KeyerIdentity&) {
            KeyingAnswer answer;
            // admitKey asks first; the key itself is taken; then admitted.
            answer.verdict = (asked++ == 1) ? KeyingVerdict::Take : KeyingVerdict::Admit;
            return answer;
        });
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        ctrl.setActive(true);
        QVERIFY(!ctrl.isActive());
        QVERIFY(mox.isManualKey());
        // Within the settle, the device that took transmit keys.
        KeyerIdentity phone;
        phone.deviceId = QByteArray("phone");
        mox.setMox(true, phone);
        QVERIFY(mox.isMox());
        QCOMPARE(mox.currentKeyer().deviceId, QByteArray("phone"));

        QTRY_VERIFY_WITH_TIMEOUT(!mox.isManualKey(), 2000);
        // The phone's key is left alone.
        QVERIFY(mox.isMox());
        QCOMPARE(mox.currentKeyer().deviceId, QByteArray("phone"));
        mox.setMox(false, phone);
        QVERIFY(!mox.isMox());
    }

    // ── Fix wave RD-I4: the stop releases only its own device's key ────────
    // After a take another device may hold the key when the station's
    // two-tone stops; console.MOX = false there must not unkey it.
    void setActive_stop_leavesAnotherDevicesKey_data()
    {
        QTest::addColumn<bool>("powerOff");
        QTest::newRow("stop") << false;
        QTest::newRow("power off") << true;
    }

    void setActive_stop_leavesAnotherDevicesKey()
    {
        QFETCH(bool, powerOff);
        TransmitModel tx;
        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        ctrl.setActive(true);
        QCoreApplication::processEvents();
        QVERIFY(ctrl.isActive());
        QVERIFY(mox.isMox());

        // The station's key ends (another device took transmit) and that
        // device keys.
        mox.setMox(false);
        KeyerIdentity phone;
        phone.deviceId = QByteArray("phone");
        mox.setMox(true, phone);
        QVERIFY(mox.isMox());
        QCOMPARE(mox.currentKeyer().deviceId, QByteArray("phone"));

        if (powerOff) {
            ctrl.stopNow();
        } else {
            ctrl.setActive(false);
        }
        for (int i = 0; i < 10; ++i) QCoreApplication::processEvents();
        QTRY_VERIFY_WITH_TIMEOUT(!ctrl.isActive(), 2000);
        QVERIFY2(mox.isMox(), "the two-tone stop unkeyed another device's key");
        QCOMPARE(mox.currentKeyer().deviceId, QByteArray("phone"));
        mox.setMox(false, phone);
        QVERIFY(!mox.isMox());
    }

    // ── Idempotent: setActive(true) twice is safe ────────────────────────
    void setActive_idempotent_doesNotRepeat()
    {
        TransmitModel tx;
        RecordingTxChannel tc(kTxChannelId);
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        SliceModel slice;

        TwoToneController ctrl;
        ctrl.setTransmitModel(&tx);
        ctrl.setTxChannel(&tc);
        ctrl.setMoxController(&mox);
        ctrl.setSliceModel(&slice);
        ctrl.setSettleDelaysMs(0, 0);

        QSignalSpy activeSpy(&ctrl, &TwoToneController::twoToneActiveChanged);

        ctrl.setActive(true);
        QCoreApplication::processEvents();
        const int callCount1 = tc.calls.size();
        QVERIFY(ctrl.isActive());
        QCOMPARE(activeSpy.count(), 1);

        ctrl.setActive(true);  // duplicate
        QCoreApplication::processEvents();

        QCOMPARE(tc.calls.size(), callCount1);  // no new calls
        QCOMPARE(activeSpy.count(), 1);          // no re-emit
    }
};

QTEST_MAIN(TestTwoToneController)
#include "tst_two_tone_controller.moc"
