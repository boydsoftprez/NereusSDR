// tst_radio_model_3m1b_ownership.cpp
//
// no-port-check: Test file exercises NereusSDR API; no C# is translated here.
// NereusSDR-original integration glue (Phase 3M-1b Task L.1) — strategy-pattern
// mic sources and composite router owned by RadioModel.  Phase 3M-1c TX pump
// architecture redesign extends this with TxWorkerThread ownership cases.
//
// Unit tests for Phase 3M-1b Task L.1 + 3M-1c TX pump architecture redesign:
//   1.  mic source test seams return nullptr before connectToRadio().
//   2.  RadioMicSource (QObject) lives on the main thread.
//   3.  RadioMicSource construction (standalone) doesn't crash with null connection.
//   4.  CompositeTxMicRouter construction (standalone) — zero-fills on pullSamples
//       when both sources are null.
//   5.  TransmitModel::micPreampChanged → AudioEngine (direct connect) wiring shape.
//   6.  TransmitModel::monEnabledChanged → AudioEngine::setTxMonitorEnabled
//       wiring shape verified via state accessor.
//   7.  TransmitModel::monitorVolumeChanged → AudioEngine::setTxMonitorVolume
//       wiring shape verified via state accessor.
//   8.  MoxCheck callback lifecycle — install, invoke, clear.
//   9.  MoxCheck rejection path — moxRejected signal emitted on !ok result.
//  10.  MoxCheck cleared on nullptr-like check — setMox(true) allowed after clear.
//  11.  Mic source objects null before connect: pcMicSourceForTest.
//  12.  Mic source objects null before connect: radioMicSourceForTest.
//  13   Mic source objects null before connect: compositeMicRouterForTest.
//  14.  Construct + destroy RadioModel without crash (lifecycle).
//  15.  RadioMicSource ring starts empty (zero fill level).
//  16.  TxWorkerThread test seam returns nullptr before connectToRadio()
//       (Phase 3M-1c TX pump architecture redesign).
//  17.  Construct + destroy RadioModel — TxWorkerThread null throughout
//       no-connect lifecycle.
//  18.  Double-construct lifecycle — TxWorkerThread stays null across two
//       consecutive RadioModel instances.
//
// L.1 signal connections AND the TxWorkerThread post-connect construction
// require a live radio connection (m_audioEngine running + m_txChannel via
// WdspEngine::createTxChannel(1)) and cannot be tested here without
// significant harness infrastructure (WDSP + full RadioConnection lifecycle).
// Those paths are covered by:
//   - L.1 signal wiring: tst_tx_channel_real_mic_router, tst_radio_mic_source,
//     tst_tx_mic_router_selector.
//   - TxWorkerThread pump cadence + driveOneTxBlock dispatch:
//     tst_tx_worker_thread (constructs + ticks the worker directly).
//
// What this file CAN verify (covered by tests 16-18 below) and what it
// CANNOT (deferred to integration layer):
//   CAN: m_txWorker is a unique_ptr<TxWorkerThread> in the class layout
//        (verified by the txWorkerForTest accessor's existence + null
//         pre-connect return — if the field were absent or wrong type
//         the build would fail).
//   CAN: m_txWorker holds nullptr from construction through destruction
//        for any RadioModel that never reaches connectToRadio()'s
//        WDSP-init lambda.
//   CANNOT: post-connect "is the worker actually running" — that
//           requires WdspEngine::initialize() success + AudioEngine
//           running + WDSP TX channel creation.  Covered by manual bench
//           verification matrix [3M-1c-bench] rows.
//
// Plan: 3M-1b Task L.1 (mic source ownership) + 3M-1c TX pump architecture
// redesign §5.5 (TxWorkerThread construct/destroy on connect/teardown).
// Pre-code review §0.3 + master design §5.2.4.

// NEREUS_BUILD_TESTS is defined in CMakeLists.txt for this target, which
// unlocks the test seams used below (pcMicSourceForTest, etc.).

#include <QtTest/QtTest>
#include <QObject>
#include <QSignalSpy>
#include <QCoreApplication>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/MoxController.h"
#include "core/RadioConnection.h"
#include "core/TxWorkerThread.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/audio/PcMicSource.h"
#include "core/audio/RadioMicSource.h"
#include "core/safety/BandPlanGuard.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

// ---------------------------------------------------------------------------
// MockRadioConnection — minimal concrete RadioConnection for tests that need
// a non-null connection pointer (mirrors the pattern in tst_radio_model_g1_wiring).
// ---------------------------------------------------------------------------
class MockRadioConnection : public RadioConnection {
    Q_OBJECT
public:
    explicit MockRadioConnection(QObject* parent = nullptr) : RadioConnection(parent)
    {
        setState(ConnectionState::Connected);
    }
    void init() override {}
    void connectToRadio(const NereusSDR::RadioInfo&) override {}
    void disconnect() override {}
    void setReceiverFrequency(int, quint64) override {}
    void setTxFrequency(quint64) override {}
    void setActiveReceiverCount(int) override {}
    void setSampleRate(int) override {}
    void setAttenuator(int) override {}
    void setPreamp(bool) override {}
    void setTxDrive(int) override {}
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

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------
class TestRadioModel3m1bOwnership : public QObject {
    Q_OBJECT

    void clearAppSettings() {
        AppSettings::instance().clear();
    }

private slots:
    void initTestCase() { clearAppSettings(); }
    void init()          { clearAppSettings(); }
    void cleanup()       { clearAppSettings(); }

    // ── 1. pcMicSourceForTest() returns nullptr before connectToRadio() ────────
    // L.1 constructs mic sources inside connectToRadio() after WDSP initializes.
    // In unit-test builds WDSP never initializes, so the sources stay null.
    // This mirrors the G.1 "txChannelNullBeforeWdspInit" pattern.
    void pcMicSourceNullBeforeConnect()
    {
        RadioModel model;
        QVERIFY(model.pcMicSourceForTest() == nullptr);
    }

    // ── 2. radioMicSourceForTest() returns nullptr before connectToRadio() ─────
    void radioMicSourceNullBeforeConnect()
    {
        RadioModel model;
        QVERIFY(model.radioMicSourceForTest() == nullptr);
    }

    // ── 3. compositeMicRouterForTest() returns nullptr before connectToRadio() ──
    void compositeMicRouterNullBeforeConnect()
    {
        RadioModel model;
        QVERIFY(model.compositeMicRouterForTest() == nullptr);
    }

    // ── 4. RadioMicSource (QObject) lives on the main thread after construction ──
    // RadioMicSource is a QObject with no parent; it inherits the thread affinity
    // of the constructing thread (main thread in tests). The producer (connection
    // thread) pushes into the lock-free ring; pullSamples is called from the
    // WDSP audio thread. Neither producer nor consumer needs thread affinity
    // on RadioMicSource itself, but this test documents the invariant.
    void radioMicSourceOnMainThread()
    {
        // Construct standalone (not through RadioModel — WDSP not available).
        RadioMicSource src(nullptr, nullptr);
        QCOMPARE(src.thread(), QThread::currentThread());
    }

    // ── 5. RadioMicSource construction with null connection doesn't crash ───────
    // L.1 passes m_connection to the RadioMicSource constructor. The RadioMicSource
    // header documents that null is safe (no slot is connected; pullSamples
    // always zero-fills). This test verifies that contract.
    void radioMicSourceNullConnectionNocrash()
    {
        {
            RadioMicSource src(nullptr, nullptr);
            // Pull samples with a zero connection — should zero-fill without crash.
            std::array<float, 8> buf{};
            const int got = src.pullSamples(buf.data(), 8);
            // Always returns n (8) even on underrun (zero-fill path).
            QCOMPARE(got, 8);
            // All samples must be zero (ring was never fed).
            for (float s : buf) {
                QCOMPARE(s, 0.0f);
            }
        }
        QVERIFY(true);  // reach here = no crash on destruction
    }

    // ── 6. RadioMicSource ring starts empty ────────────────────────────────────
    // No frames have been pushed via onMicFrame. pullSamples zero-fills.
    void radioMicSourceRingStartsEmpty()
    {
        RadioMicSource src(nullptr, nullptr);
        QCOMPARE(src.ringFillForTest(), 0);
    }

    // ── 7. CompositeTxMicRouter zero-fills when both sources pass null ──────────
    // This is the edge-case branch in CompositeTxMicRouter::pullSamples when
    // the active source is null (shouldn't happen in production, but the
    // zero-fill fallback must be safe). Tested by passing nullptr to both
    // source arguments and hasMicJack=false (force-Pc mode with null pc source).
    // The CompositeTxMicRouter collapses to Pc-only when hasMicJack=false;
    // if pcSource is also null, it zero-fills.
    void compositeMicRouterZeroFillsWithNullSources()
    {
        // Both sources null: triggers the "fallback: zero-fill" branch.
        CompositeTxMicRouter router(nullptr, nullptr, /*hasMicJack=*/false);
        std::array<float, 8> buf{};
        buf.fill(99.0f);  // poison so we can confirm zero-fill overwrites
        const int got = router.pullSamples(buf.data(), 8);
        QCOMPARE(got, 8);
        for (float s : buf) {
            QCOMPARE(s, 0.0f);
        }
    }

    // ── 8. TransmitModel::micPreampChanged → TxChannel::setMicPreamp shape ──────
    // L.1 connection 3 wires micPreampChanged → setMicPreamp. We can't test
    // the RadioModel-wired path without a live connection, so we verify the
    // signal shape by spy: micPreampChanged carries a double (linear gain)
    // derived from setMicGainDb (the public setter that triggers it).
    // The actual wiring is covered by tst_tx_channel_real_mic_router.
    void micPreampChangedSignalCarriesLinearGain()
    {
        TransmitModel tm;
        QSignalSpy spy(&tm, &TransmitModel::micPreampChanged);
        QVERIFY(spy.isValid());

        // setMicGainDb(0) → 0 dB → 1.0 linear → emits micPreampChanged(1.0).
        tm.setMicGainDb(0);
        QCoreApplication::processEvents();

        // At least one emission.
        QVERIFY(!spy.isEmpty());

        // Payload must be a double (linear gain).
        const QVariant payload = spy.last().first();
        QCOMPARE(static_cast<QMetaType::Type>(payload.typeId()), QMetaType::Double);
        // 0 dB → 1.0 linear (within float precision).
        const double linear = payload.toDouble();
        QVERIFY(std::abs(linear - 1.0) < 1e-6);
    }

    // ── 9. TransmitModel::monEnabledChanged → AudioEngine::setTxMonitorEnabled ──
    // L.1 connection 4. Both objects are standalone-constructible. Verify that
    // connecting and emitting changes AudioEngine state.
    void monEnabledChangedUpdatesAudioEngineState()
    {
        TransmitModel tm;
        AudioEngine ae;

        // Manually wire the same connection L.1 makes.
        QObject::connect(&tm, &TransmitModel::monEnabledChanged,
                         &ae, &AudioEngine::setTxMonitorEnabled);

        // Default: TX monitor disabled.
        QCOMPARE(ae.txMonitorEnabled(), false);

        // Emit via TransmitModel setter.
        tm.setMonEnabled(true);
        QCoreApplication::processEvents();
        QCOMPARE(ae.txMonitorEnabled(), true);

        tm.setMonEnabled(false);
        QCoreApplication::processEvents();
        QCOMPARE(ae.txMonitorEnabled(), false);
    }

    // ── 10. TransmitModel::monitorVolumeChanged → AudioEngine::setTxMonitorVolume ─
    // L.1 connection 5. Same standalone wiring verification as test 9.
    void monitorVolumeChangedUpdatesAudioEngineState()
    {
        TransmitModel tm;
        AudioEngine ae;

        QObject::connect(&tm, &TransmitModel::monitorVolumeChanged,
                         &ae, &AudioEngine::setTxMonitorVolume);

        // setMonitorVolume(0.75f) should propagate.
        tm.setMonitorVolume(0.75f);
        QCoreApplication::processEvents();
        QCOMPARE(ae.txMonitorVolume(), 0.75f);
    }

    // ── 11. MoxCheck callback lifecycle — install, invoke, clear ───────────────
    // K.2 carry-forward. MoxController::setMoxCheck installs a callback.
    // This test verifies:
    //   (a) Callback is invoked on setMox(true).
    //   (b) Clearing with {} disables the callback (setMox(true) proceeds).
    void moxCheckCallbackLifecycle()
    {
        MoxController mc;
        mc.setTimerIntervals(0, 0, 0, 0, 0, 0);

        int callCount = 0;

        // Install a callback that always approves (returns ok=true).
        mc.setMoxCheck([&callCount]() -> safety::BandPlanGuard::MoxCheckResult {
            ++callCount;
            return {true, QString()};
        });

        // setMox(true) should consult the callback.
        mc.setMox(true);
        QCoreApplication::processEvents();
        QCOMPARE(callCount, 1);

        // Clear the callback.
        mc.setMoxCheck({});

        // Reset MOX to Rx for the next setMox(true).
        mc.setMox(false);
        QCoreApplication::processEvents();

        // setMox(true) with no callback — should not call old callback.
        mc.setMox(true);
        QCoreApplication::processEvents();
        // callCount must still be 1 (callback was cleared).
        QCOMPARE(callCount, 1);
    }

    // ── 12. MoxCheck rejection path — moxRejected signal emitted ──────────────
    // K.2 carry-forward. When the MoxCheckFn returns ok=false, setMox(true)
    // emits moxRejected(reason) and returns without advancing state.
    void moxCheckRejectionEmitsMoxRejected()
    {
        MoxController mc;
        mc.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy rejectedSpy(&mc, &MoxController::moxRejected);
        QVERIFY(rejectedSpy.isValid());

        const QString kReason = QStringLiteral("Band plan: TX not allowed");

        mc.setMoxCheck([&kReason]() -> safety::BandPlanGuard::MoxCheckResult {
            return {false, kReason};
        });

        // Attempt TX-on: should be rejected.
        mc.setMox(true);
        QCoreApplication::processEvents();

        // moxRejected must have been emitted once with the correct reason.
        QCOMPARE(rejectedSpy.count(), 1);
        QCOMPARE(rejectedSpy.first().first().toString(), kReason);

        // MOX must NOT have advanced to TX state.
        QCOMPARE(mc.isMox(), false);
    }

    // ── 13. MoxCheck approved — no moxRejected emission ───────────────────────
    // Symmetric to test 12: when the check returns ok=true, moxRejected does
    // NOT fire and MOX proceeds to TX state.
    void moxCheckApprovedNoRejectedEmission()
    {
        MoxController mc;
        mc.setTimerIntervals(0, 0, 0, 0, 0, 0);

        QSignalSpy rejectedSpy(&mc, &MoxController::moxRejected);

        mc.setMoxCheck([]() -> safety::BandPlanGuard::MoxCheckResult {
            return {true, QString()};
        });

        mc.setMox(true);
        QCoreApplication::processEvents();

        // No rejection.
        QCOMPARE(rejectedSpy.count(), 0);
        // MOX must have advanced.
        QCOMPARE(mc.isMox(), true);
    }

    // ── 14. Construct + destroy RadioModel without crash (lifecycle) ────────────
    // Basic lifecycle: RadioModel can be constructed and destroyed without any
    // connection having been established. The mic source unique_ptrs hold nullptr
    // throughout; the destructor must not crash.
    void radioModelLifecycleNoConnect()
    {
        {
            RadioModel model;
            // Verify test seams return null (pre-connect state).
            QVERIFY(model.pcMicSourceForTest()         == nullptr);
            QVERIFY(model.radioMicSourceForTest()       == nullptr);
            QVERIFY(model.compositeMicRouterForTest()  == nullptr);
        }
        // Destructor ran — no crash.
        QVERIFY(true);
    }

    // ── 15. Construct + destroy RadioModel twice (reconstruct lifecycle) ────────
    // Verifies the unique_ptr reset/reconstruct pattern survives two consecutive
    // RadioModel instances (simulating reconnect without a live radio).
    void radioModelDoubleLifecycleNoConnect()
    {
        for (int i = 0; i < 2; ++i) {
            RadioModel model;
            QVERIFY(model.pcMicSourceForTest()        == nullptr);
            QVERIFY(model.radioMicSourceForTest()      == nullptr);
            QVERIFY(model.compositeMicRouterForTest() == nullptr);
        }
        QVERIFY(true);
    }

    // ── 16. txWorkerForTest() returns nullptr before connectToRadio() ──────────
    // Phase 3M-1c TX pump architecture redesign (§5.5): the TxWorkerThread is
    // constructed inside connectToRadio()'s WDSP-init lambda once m_audioEngine
    // and m_txChannel are both live.  Before connect, m_txWorker holds nullptr
    // (default-constructed unique_ptr).  Mirrors the mic-source pre-connect
    // pattern (tests 1-3 / 11-13).
    //
    // This test verifies the class-layout invariant — that m_txWorker is a
    // unique_ptr<TxWorkerThread> field on RadioModel — without requiring the
    // heavy WDSP + live-connection machinery the post-connect path needs.
    // If the field were absent, removed, or renamed, this test would fail to
    // compile (or fail at runtime if the accessor stub were broken).
    void txWorkerNullBeforeConnect()
    {
        RadioModel model;
        QVERIFY(model.txWorkerForTest() == nullptr);
    }

    // ── 17. RadioModel lifecycle — TxWorker null throughout no-connect path ────
    // Phase 3M-1c TX pump architecture redesign: mirrors test 14 with the
    // additional TxWorkerThread null check.  No connect() = no worker.
    // Destructor must not crash even though m_txWorker is null (the
    // teardownConnection() path's `if (m_txWorker)` guard handles the case).
    void txWorkerLifecycleNoConnect()
    {
        {
            RadioModel model;
            QVERIFY(model.txWorkerForTest() == nullptr);
        }
        // Destructor ran — no crash.
        QVERIFY(true);
    }

    // ── 18. Double-construct lifecycle — TxWorker null across consecutive ──────
    // RadioModel instances.  Phase 3M-1c TX pump architecture redesign: mirrors
    // test 15 with the additional TxWorkerThread null check.  The unique_ptr
    // reset/reconstruct pattern must survive two consecutive RadioModel
    // instances cleanly.
    void txWorkerDoubleLifecycleNoConnect()
    {
        for (int i = 0; i < 2; ++i) {
            RadioModel model;
            QVERIFY(model.txWorkerForTest() == nullptr);
        }
        QVERIFY(true);
    }

    // ── 19. txMicSourceForTest() returns nullptr before connectToRadio() ──────
    // Phase 3M-1c TX pump architecture redesign v3 (§5.6): the TxMicSource
    // (Thetis Inbound/cm_main port) is constructed alongside TxWorkerThread
    // inside the WDSP-init lambda once a connection + TxChannel are both
    // live.  Before connect, m_txMicSource holds nullptr.  Mirrors the
    // m_txWorker pre-connect pattern (tests 16-18).
    void txMicSourceNullBeforeConnect()
    {
        RadioModel model;
        QVERIFY(model.txMicSourceForTest() == nullptr);
    }
};

QTEST_MAIN(TestRadioModel3m1bOwnership)
#include "tst_radio_model_3m1b_ownership.moc"
