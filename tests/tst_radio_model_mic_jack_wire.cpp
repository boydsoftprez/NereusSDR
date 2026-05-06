// no-port-check: NereusSDR-original unit-test file. The Thetis cite
// comments below document which upstream lines each assertion verifies;
// no upstream logic is ported in this file.
// =================================================================
// tests/tst_radio_model_mic_jack_wire.cpp  (NereusSDR)
// =================================================================
//
// Spec coverage: docs/architecture/radio-mic-input-thetis-parity.md §4.6
//
// Verifies that toggling each of the six mic-jack TransmitModel signals
// reaches the radio over the wire, and that the model state is primed
// onto the connection at connect-time.  Mirrors tst_radio_model_mic_ptt_wire
// (issue #182, PR #193) which established the helper-extraction +
// queued-connection + prime-on-connect pattern.
//
// Signals exercised:
//   1. micBoostChanged(bool)         -> RadioConnection::setMicBoost(bool)
//   2. lineInChanged(bool)           -> RadioConnection::setLineIn(bool)
//   3. micTipRingChanged(bool)       -> RadioConnection::setMicTipRing(bool)
//   4. micBiasChanged(bool)          -> RadioConnection::setMicBias(bool)
//   5. micXlrChanged(bool)           -> RadioConnection::setMicXlr(bool)
//   6. lineInBoostChanged(double)    -> RadioConnection::setLineInBoost(double)
//
// The lineInBoost wire goes through the RadioConnection::setLineInBoost(double)
// virtual default, which forwards to setLineInGain(int) via the Thetis 32-entry
// table.  We capture lineInGain(int) at the FakeRadioConnection so the test
// remains protocol-agnostic; 0.0 dB maps to index 23 per Task 1's table.
//
// Source cites (no Thetis logic translated here, comments only):
//   Thetis console.cs:13237 [v2.10.3.13+501e3f51]   mic_boost = true (default)
//   Thetis console.cs:13213 [v2.10.3.13+501e3f51]   line_in = false (default)
//   Thetis console.cs:13225 [v2.10.3.13+501e3f51]   line_in_boost = 0.0 (default)
//   Thetis setup.designer.cs:8683 [v2.10.3.13+501e3f51]
//                                                   radOrionMicTip.Checked=true
//   Thetis setup.designer.cs:8779 [v2.10.3.13+501e3f51]
//                                                   radOrionBiasOff.Checked=true
//   Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51]
//                                                   MakeLineInList + SetMicGain
// =================================================================

#include <QtTest/QtTest>
#include <QObject>
#include <QCoreApplication>
#include <QScopeGuard>

#include "core/RadioConnection.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;

// ── MockConnection ──────────────────────────────────────────────────────────
// Captures the most-recent value passed to each mic-jack setter.  Mirrors the
// pattern used by tst_radio_model_mic_ptt_wire's MockConnection.  Fields are
// initialised to "impossible" sentinel values so the tests can distinguish
// "never called" from "called with the default".
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    int   micBoostCalls{0};        bool   lastMicBoost{false};
    int   lineInCalls{0};          bool   lastLineIn{false};
    int   micTipRingCalls{0};      bool   lastMicTipRing{false};
    int   micBiasCalls{0};         bool   lastMicBias{false};
    int   micXlrCalls{0};          bool   lastMicXlr{false};
    int   lineInGainCalls{0};      int    lastLineInGain{-1};

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
    void setTxDrive(int) override {}
    void sendTxIq(const float*, int) override {}
    void setWatchdogEnabled(bool) override {}
    void setAntennaRouting(AntennaRouting) override {}
    void setMox(bool) override {}
    void setTrxRelay(bool) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool) override {}

    // Captured setters.
    void setMicBoost(bool on) override   { ++micBoostCalls;   lastMicBoost   = on; }
    void setLineIn(bool on) override     { ++lineInCalls;     lastLineIn     = on; }
    void setMicTipRing(bool on) override { ++micTipRingCalls; lastMicTipRing = on; }
    void setMicBias(bool on) override    { ++micBiasCalls;    lastMicBias    = on; }
    void setMicXlr(bool on) override     { ++micXlrCalls;     lastMicXlr     = on; }
    // setLineInBoost(double) inherits the base default impl, which forwards
    // to setLineInGain(int).  Capturing here lets the test stay protocol-
    // agnostic and exercise the same virtual path the production code uses.
    void setLineInGain(int g) override   { ++lineInGainCalls; lastLineInGain = g; }
};

// ── Helpers ─────────────────────────────────────────────────────────────────

// pump: drains the Qt event loop for queued signal/slot connections.
// Mirrors the pump() helper in tst_radio_model_mic_ptt_wire.cpp.  The model
// to connection wiring uses Qt::QueuedConnection (m_connection lives on the
// worker thread in production); two passes cover any chained queued
// emissions from the prime path.
static void pump()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

// ── Test class ──────────────────────────────────────────────────────────────
class TestRadioModelMicJackWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. micBoost: model toggle reaches the connection. ────────────────────
    // Default TransmitModel::micBoost is true (Thetis console.cs:13237).
    // Flipping it to false via the model setter must fire setMicBoost(false)
    // on the connection.
    void micBoostReachesConnection()
    {
        RadioModel model;
        auto* conn = new MockConnection();
        model.injectConnectionForTest(conn);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.wireMicJackForTest();
        pump();
        conn->micBoostCalls = 0;
        conn->lastMicBoost  = true;  // sentinel — different from value we'll set.

        model.transmitModel().setMicBoost(false);
        pump();

        QCOMPARE(conn->micBoostCalls, 1);
        QCOMPARE(conn->lastMicBoost,  false);
    }

    // ── 2. lineIn: model toggle reaches the connection. ──────────────────────
    void lineInReachesConnection()
    {
        RadioModel model;
        auto* conn = new MockConnection();
        model.injectConnectionForTest(conn);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.wireMicJackForTest();
        pump();
        conn->lineInCalls = 0;
        conn->lastLineIn  = false;

        model.transmitModel().setLineIn(true);
        pump();

        QCOMPARE(conn->lineInCalls, 1);
        QCOMPARE(conn->lastLineIn,  true);
    }

    // ── 3. micTipRing: model toggle reaches the connection. ──────────────────
    // Default true; flip to false.  RadioConnection inverts polarity on the
    // wire, but the SET-and-PROPAGATE contract is direct — RadioConnection
    // receives the same bool the model emits.
    void micTipRingReachesConnection()
    {
        RadioModel model;
        auto* conn = new MockConnection();
        model.injectConnectionForTest(conn);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.wireMicJackForTest();
        pump();
        conn->micTipRingCalls = 0;
        conn->lastMicTipRing  = true;

        model.transmitModel().setMicTipRing(false);
        pump();

        QCOMPARE(conn->micTipRingCalls, 1);
        QCOMPARE(conn->lastMicTipRing,  false);
    }

    // ── 4. micBias: model toggle reaches the connection. ─────────────────────
    void micBiasReachesConnection()
    {
        RadioModel model;
        auto* conn = new MockConnection();
        model.injectConnectionForTest(conn);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.wireMicJackForTest();
        pump();
        conn->micBiasCalls = 0;
        conn->lastMicBias  = false;

        model.transmitModel().setMicBias(true);
        pump();

        QCOMPARE(conn->micBiasCalls, 1);
        QCOMPARE(conn->lastMicBias,  true);
    }

    // ── 5. micXlr: model toggle reaches the connection. ──────────────────────
    // Default false (3.5 mm; flipped from Thetis self-contradicting source
    // to user-visible default per spec §5).  Flip to true.
    void micXlrReachesConnection()
    {
        RadioModel model;
        auto* conn = new MockConnection();
        model.injectConnectionForTest(conn);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.wireMicJackForTest();
        pump();
        conn->micXlrCalls = 0;
        conn->lastMicXlr  = false;

        model.transmitModel().setMicXlr(true);
        pump();

        QCOMPARE(conn->micXlrCalls, 1);
        QCOMPARE(conn->lastMicXlr,  true);
    }

    // ── 6. lineInBoost: model dB change reaches the connection. ──────────────
    // setLineInBoost(double) is the base virtual default that maps dB to
    // a 5-bit gain index per the Thetis 32-entry table.  The MockConnection
    // captures setLineInGain(int) instead of setLineInBoost(double), so the
    // assertion checks the table index rather than the raw dB value.  This
    // exercises the SAME virtual chain production uses.
    //
    // TransmitModel::setLineInBoost has an idempotent guard, so the test
    // moves the model OFF its 0.0 dB default to +12.0 dB (max table index 31).
    //
    // Source: Thetis console.cs:40827-40859 [v2.10.3.13+501e3f51]
    //   MakeLineInList + SetMicGain (32-entry -34.5..+12.0 in 1.5 dB steps).
    void lineInBoostReachesConnection()
    {
        RadioModel model;
        auto* conn = new MockConnection();
        model.injectConnectionForTest(conn);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.wireMicJackForTest();
        pump();
        conn->lineInGainCalls = 0;
        conn->lastLineInGain  = -1;

        // +12.0 dB → index 31 (max).  Differs from default 0.0 dB so the
        // TransmitModel idempotent guard fires the change signal.
        model.transmitModel().setLineInBoost(12.0);
        pump();

        QCOMPARE(conn->lineInGainCalls, 1);
        QCOMPARE(conn->lastLineInGain,  31);
    }

    // ── 7. Connect-time prime pushes all six values to the wire. ─────────────
    // Persisted user preferences must reach the firmware on every reconnect.
    // Construct the model with non-default values BEFORE wiring up the
    // connection; the prime path must capture the model's current state.
    //
    // Choice of non-default values:
    //   micBoost   = false  (default true,  flip)
    //   lineIn     = true   (default false, flip)
    //   micTipRing = false  (default true,  flip)
    //   micBias    = true   (default false, flip)
    //   micXlr     = true   (default false, flip)
    //   lineInBoost = -3.0  (default 0.0;   index 21 = (−3.0+34.5)/1.5)
    void primeOnConnectPushesAllSix()
    {
        RadioModel model;
        // Pre-set the model BEFORE wiring up the connection.
        model.transmitModel().setMicBoost(false);
        model.transmitModel().setLineIn(true);
        model.transmitModel().setMicTipRing(false);
        model.transmitModel().setMicBias(true);
        model.transmitModel().setMicXlr(true);
        model.transmitModel().setLineInBoost(-3.0);

        auto* conn = new MockConnection();
        model.injectConnectionForTest(conn);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.wireMicJackForTest();
        pump();

        QCOMPARE(conn->micBoostCalls,   1);
        QCOMPARE(conn->lastMicBoost,    false);
        QCOMPARE(conn->lineInCalls,     1);
        QCOMPARE(conn->lastLineIn,      true);
        QCOMPARE(conn->micTipRingCalls, 1);
        QCOMPARE(conn->lastMicTipRing,  false);
        QCOMPARE(conn->micBiasCalls,    1);
        QCOMPARE(conn->lastMicBias,     true);
        QCOMPARE(conn->micXlrCalls,     1);
        QCOMPARE(conn->lastMicXlr,      true);
        QCOMPARE(conn->lineInGainCalls, 1);
        QCOMPARE(conn->lastLineInGain,  21);  // (-3.0 - (-34.5)) / 1.5 = 21
    }
};

QTEST_MAIN(TestRadioModelMicJackWire)
#include "tst_radio_model_mic_jack_wire.moc"
