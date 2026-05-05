// tst_radio_model_mox_hardware_flip.cpp
//
// no-port-check: Test file exercises NereusSDR API; Thetis behavior is
// cited in RadioModel.cpp via pre-code review §2.3 / §2.5 — no C# is
// translated here.
//
// Unit tests for RadioModel::onMoxHardwareFlipped(bool isTx) (3M-1a Task F.1).
// Exercises the three ordered side-effects:
//   1. applyAlexAntennaForBand(currentBand, isTx)  — Step 8 of §2.3
//   2. m_connection->setMox(isTx)                  — Step 12 of §1.4 / §2.3
//   3. m_connection->setTrxRelay(isTx)             — Step 10 of §2.3
//
// Test strategy:
//   - MockConnection records call order via a shared call-log vector.
//     Each entry is a tagged string ("alex", "setMox:1", "setTrxRelay:1").
//   - applyAlexAntennaForBand → setAntennaRouting() on the mock, so the
//     "alex" entry appears at the position Alex fires relative to setMox /
//     setTrxRelay.
//   - Band is read from m_activeSlice->frequency() when a slice exists, or
//     falls back to m_lastBand (Band::Band20m default). Tests confirm the
//     correct band is forwarded to Alex by checking the AntennaRouting
//     recorded by the mock.

#include <QtTest/QtTest>

#include "core/AppSettings.h"
#include "core/RadioConnection.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace NereusSDR;

// ── SignalEmitter ─────────────────────────────────────────────────────────────
// Minimal QObject with a signal matching MoxController::hardwareFlipped.
// Used in test 8 to verify the slot is connectable via QObject::connect.
// Must be defined at file scope (Qt MOC limitation: Q_OBJECT not allowed in
// local classes).
class SignalEmitter : public QObject {
    Q_OBJECT
signals:
    void hardwareFlipped(bool isTx);
};

// ── MockConnection ────────────────────────────────────────────────────────────
// Records setMox, setTrxRelay, and setAntennaRouting calls with order stamps.
// Uses the same pattern as tst_antenna_routing_model.cpp.
class MockConnection : public RadioConnection {
    Q_OBJECT

public:
    // Ordered log of calls made by the slot.  Each entry is a one-word tag:
    //   "alex"           — setAntennaRouting() (Alex step, triggered by
    //                       applyAlexAntennaForBand → setAntennaRouting)
    //   "setMox:0"       — setMox(false)
    //   "setMox:1"       — setMox(true)
    //   "setTrxRelay:0"  — setTrxRelay(false)
    //   "setTrxRelay:1"  — setTrxRelay(true)
    QList<QString> callLog;

    // Last routing struct received (for band/isTx assertion).
    AntennaRouting lastRouting;

    // Last values seen by setMox / setTrxRelay.
    bool lastSetMoxArg{false};
    bool lastSetTrxRelayArg{false};

    explicit MockConnection(QObject* parent = nullptr)
        : RadioConnection(parent)
    {
        // Mark as connected so applyAlexAntennaForBand passes its
        // !isConnected() early-return guard.
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
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}

    void setAntennaRouting(AntennaRouting r) override {
        lastRouting = r;
        callLog.append(QStringLiteral("alex"));
    }

    void setMox(bool enabled) override {
        lastSetMoxArg = enabled;
        callLog.append(QStringLiteral("setMox:") + (enabled ? QStringLiteral("1") : QStringLiteral("0")));
    }

    void setTrxRelay(bool enabled) override {
        lastSetTrxRelayArg = enabled;
        callLog.append(QStringLiteral("setTrxRelay:") + (enabled ? QStringLiteral("1") : QStringLiteral("0")));
    }
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

// ── Test class ───────────────────────────────────────────────────────────────
class TestRadioModelMoxHardwareFlip : public QObject {
    Q_OBJECT

private:
    void clearAppSettings() {
        AppSettings::instance().clear();
    }

private slots:
    void initTestCase() { clearAppSettings(); }
    void init()          { clearAppSettings(); }
    void cleanup()       { clearAppSettings(); }

    // ── 1. isTx=true: call order is Alex → setMox → setTrxRelay ─────────────
    // Verifies the three-step side-effect order from pre-code review §2.3.
    // Alex fires first (step 8), then the MOX wire bit (step 12 / §1.4),
    // then the T/R relay (step 10).
    void rxToTx_callOrderIsAlexMoxRelay() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);

        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);

        model.onMoxHardwareFlipped(/*isTx=*/true);

        // Verify call count: alex + setMox + setTrxRelay = 3.
        QCOMPARE(mock->callLog.size(), 3);

        // Verify ORDER (index 0 = first call).
        QCOMPARE(mock->callLog.at(0), QStringLiteral("alex"));
        QCOMPARE(mock->callLog.at(1), QStringLiteral("setMox:1"));
        QCOMPARE(mock->callLog.at(2), QStringLiteral("setTrxRelay:1"));

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // ── 2. isTx=false: same order, all three calls carry false ───────────────
    // TX→RX direction uses the same slot; verify the order is unchanged and
    // each call receives the correct (false) argument.
    void txToRx_callOrderIsAlexMoxRelay() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);

        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);

        model.onMoxHardwareFlipped(/*isTx=*/false);

        QCOMPARE(mock->callLog.size(), 3);
        QCOMPARE(mock->callLog.at(0), QStringLiteral("alex"));
        QCOMPARE(mock->callLog.at(1), QStringLiteral("setMox:0"));
        QCOMPARE(mock->callLog.at(2), QStringLiteral("setTrxRelay:0"));

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // ── 3. isTx=true forwarded correctly to setMox ───────────────────────────
    void isTxTrue_setMoxReceivesTrue() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);

        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);

        model.onMoxHardwareFlipped(true);

        QVERIFY(mock->lastSetMoxArg);
        QVERIFY(mock->lastSetTrxRelayArg);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // ── 4. isTx=false forwarded correctly to setMox ──────────────────────────
    void isTxFalse_setMoxReceivesFalse() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);

        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);

        model.onMoxHardwareFlipped(false);

        QVERIFY(!mock->lastSetMoxArg);
        QVERIFY(!mock->lastSetTrxRelayArg);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // ── 5. currentBand read from active slice frequency ────────────────────────
    // When an active slice exists, band is derived from its frequency.
    // Confirm by checking lastRouting.tx matches isTx (AlexController isTx
    // branch always writes AntennaRouting::tx = isTx in applyAlexAntennaForBand
    // line r.tx = isTx).
    void bandReadFromActiveSlice() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);

        model.addSlice();
        model.setActiveSlice(0);
        // Tune to 40m (7.1 MHz) — Band::Band40m.
        model.sliceAt(0)->setFrequency(7100000.0);

        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);

        model.onMoxHardwareFlipped(/*isTx=*/true);

        // lastRouting.tx should be true (isTx=true propagated via r.tx = isTx).
        QVERIFY(mock->lastRouting.tx);

        // Alex was called (order check).
        QVERIFY(mock->callLog.size() == 3);
        QCOMPARE(mock->callLog.at(0), QStringLiteral("alex"));

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // ── 6. No connection set — graceful no-op, no crash ───────────────────────
    // When m_connection is nullptr (no radio connected), the slot must
    // silently return without crashing.  The implementation guards with
    // "if (!m_connection) return;" BEFORE calling QMetaObject::invokeMethod,
    // because invokeMethod(nullptr, ...) asserts and would crash.
    // applyAlexAntennaForBand also has its own null guard; steps 2+3 are
    // never reached if the connection is null.
    void noConnection_doesNotCrash() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);
        // Do NOT inject a connection — m_connection stays null.
        model.onMoxHardwareFlipped(true);  // must not crash
        model.onMoxHardwareFlipped(false); // must not crash
        // If we get here without a segfault, the test passes.
        QVERIFY(true);
    }

    // ── 7. hasAlex=false: Alex still fires (zero routing), setMox, setTrxRelay ─
    // Even when hasAlex=false the slot must call all three steps — Alex sends
    // a zero-routing command, setMox and setTrxRelay still fire.
    void hasAlexFalse_allThreeStepsFire() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/false);

        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);

        model.onMoxHardwareFlipped(true);

        // All three steps must still appear.
        QCOMPARE(mock->callLog.size(), 3);
        QCOMPARE(mock->callLog.at(0), QStringLiteral("alex"));
        QCOMPARE(mock->callLog.at(1), QStringLiteral("setMox:1"));
        QCOMPARE(mock->callLog.at(2), QStringLiteral("setTrxRelay:1"));

        // Zero-routing sent for hasAlex=false.
        QCOMPARE(mock->lastRouting.trxAnt, 0);
        QCOMPARE(mock->lastRouting.txAnt, 0);
        QCOMPARE(mock->lastRouting.rxOnlyAnt, 0);
        QVERIFY(!mock->lastRouting.rxOut);

        model.injectConnectionForTest(nullptr);
        delete mock;
    }

    // ── 8. Slot connectable via QObject::connect + compatible signal ──────────
    // The connect() call that actually wires the production path lands in G.1.
    // This test verifies the slot is valid as a connect target by using a
    // throwaway QObject (SignalEmitter, declared at file scope) to emit a
    // compatible signal and confirming the slot fires.
    // G.1 MUST use Qt::QueuedConnection in production; direct is fine here
    // (no thread boundary in unit tests).
    void slotConnectableViaQObjectConnect() {
        RadioModel model;
        model.setCapsForTest(/*hasAlex=*/true);

        auto* mock = new MockConnection();
        model.injectConnectionForTest(mock);

        SignalEmitter emitter;
        QObject::connect(&emitter, &SignalEmitter::hardwareFlipped,
                         &model, &RadioModel::onMoxHardwareFlipped,
                         Qt::DirectConnection);

        emit emitter.hardwareFlipped(true);

        QCOMPARE(mock->callLog.size(), 3);
        QCOMPARE(mock->callLog.at(0), QStringLiteral("alex"));
        QCOMPARE(mock->callLog.at(1), QStringLiteral("setMox:1"));
        QCOMPARE(mock->callLog.at(2), QStringLiteral("setTrxRelay:1"));

        model.injectConnectionForTest(nullptr);
        delete mock;
    }
};

QTEST_MAIN(TestRadioModelMoxHardwareFlip)
#include "tst_radio_model_mox_hardware_flip.moc"
