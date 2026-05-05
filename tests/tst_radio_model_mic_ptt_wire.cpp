// no-port-check: NereusSDR-original unit-test file. The Thetis cite
// comments below document which upstream lines each assertion verifies;
// no upstream logic is ported in this file.
// =================================================================
// tests/tst_radio_model_mic_ptt_wire.cpp  (NereusSDR)
// =================================================================
//
// Issue #182 regression coverage — verifies that toggling
// TransmitModel::micPttDisabled actually reaches the radio over the wire,
// and that the model state is primed onto the connection at connect-time.
//
// Pre-fix (v0.3.1): no connect() existed between the model property and
// RadioConnection::setMicPTT, so the Setup -> Audio -> TX Input ->
// "Mic PTT Disabled" checkbox was decorative on every protocol. ANAN-8000DLE
// (Protocol 2) shipped with the wire bit asserting "PTT disabled at firmware"
// out of the box, so the user-reported "external PTT button does nothing"
// bug applied to every Protocol 2 OrionMKII / Saturn family board.
//
// Post-fix:
//   - RadioConnection::setMicPTTDisabled(bool disabled) virtual matches the
//     Thetis NetworkIO.SetMicPTT(disabled?1:0) wire convention exactly.
//   - RadioModel::wireConnectionSignals connects
//     TransmitModel::micPttDisabledChanged -> RadioConnection::setMicPTTDisabled
//     and primes the connection with the current model value once at boot.
//
// Source cite (no Thetis logic translated here, comments only):
//   Thetis console.cs:19757-19766 [v2.10.3.13+501e3f51]
//     private bool mic_ptt_disabled = false;        // default: PTT enabled
//     public bool MicPTTDisabled {
//         set {
//             mic_ptt_disabled = value;
//             NetworkIO.SetMicPTT(Convert.ToInt32(value));   // disabled?1:0
//         }
//     }
//   Thetis ChannelMaster/networkproto1.c:597-598 [v2.10.3.13+501e3f51]
//     C1 = ... | ((prn->mic.mic_ptt & 1) << 6);     // direct write to wire
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
// Records setMicPTTDisabled() argument values. Mirrors the pattern used by
// tst_radio_model_drive_path.cpp's MockConnection.
class MockConnection : public RadioConnection {
    Q_OBJECT
public:
    QList<bool> micPttDisabledLog;

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
    void setMicBoost(bool) override {}
    void setLineIn(bool) override {}
    void setMicTipRing(bool) override {}
    void setMicBias(bool) override {}
    void setLineInGain(int) override {}
    void setUserDigOut(quint8) override {}
    void setPuresignalRun(bool) override {}
    void setMicPTTDisabled(bool disabled) override {
        micPttDisabledLog.append(disabled);
    }
    void setMicXlr(bool) override {}
};

// ── Helpers ─────────────────────────────────────────────────────────────────

// pump: drains the Qt event loop for queued signal/slot connections.
// The model->connection wiring uses Qt::QueuedConnection (m_connection lives
// on the worker thread in production); two passes cover any chained queued
// emissions from the prime path.
static void pump()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

// ── Test class ──────────────────────────────────────────────────────────────
class TestRadioModelMicPttWire : public QObject {
    Q_OBJECT
private slots:

    // ── 1. Toggle from default false → true reaches the connection. ──────────
    // Default TransmitModel::micPttDisabled is false (PTT enabled).
    // Setting it to true via the model setter must fire setMicPTTDisabled(true)
    // on the connection. This is the user-facing "uncheck the box" -> radio
    // updates path that issue #182 reported broken.
    void modelToggleTrue_reachesConnectionAsTrue()
    {
        RadioModel model;
        auto* conn = new MockConnection();
        model.injectConnectionForTest(conn);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        // Wire up the mic_ptt_disabled signal path. Test seam mirrors what
        // wireConnectionSignals() does in production, without spinning up
        // the full DSP-thread pipeline.
        model.wireMicPttDisabledForTest();

        // Drain any prime-on-wire emit before we measure the toggle.
        pump();
        conn->micPttDisabledLog.clear();

        // Exercise: flip the model property.
        model.transmitModel().setMicPttDisabled(true);
        pump();

        QCOMPARE(conn->micPttDisabledLog.size(), 1);
        QCOMPARE(conn->micPttDisabledLog.first(), true);
    }

    // ── 2. Toggle from true back to false reaches the connection. ────────────
    // Round-trip: re-enabling PTT (unchecking the Setup checkbox) must
    // also propagate to the wire. Confirms the connect() is bidirectional
    // (not a one-shot).
    void modelToggleFalse_reachesConnectionAsFalse()
    {
        RadioModel model;
        auto* conn = new MockConnection();
        model.injectConnectionForTest(conn);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        model.wireMicPttDisabledForTest();
        model.transmitModel().setMicPttDisabled(true);  // first move
        pump();
        conn->micPttDisabledLog.clear();

        // Exercise: flip back.
        model.transmitModel().setMicPttDisabled(false);
        pump();

        QCOMPARE(conn->micPttDisabledLog.size(), 1);
        QCOMPARE(conn->micPttDisabledLog.first(), false);
    }

    // ── 3. Connect-time prime pushes the current model value to the wire. ────
    // Persisted user preference must reach the firmware on every reconnect.
    // If the user has saved micPttDisabled=true and reconnects, the wire bit
    // has to be asserted before they can transmit. This mirrors the Thetis
    // pattern where every NetworkIO.SetXxx is called once at radio init.
    //
    // Source cite: Thetis console.cs:19757 [v2.10.3.13+501e3f51] —
    //   default mic_ptt_disabled = false. Persistence layer round-trips
    //   this through TransmitModel.
    void connectTimePrime_pushesCurrentModelValueOnce()
    {
        RadioModel model;
        // Pre-set the model BEFORE wiring up the connection.
        model.transmitModel().setMicPttDisabled(true);

        auto* conn = new MockConnection();
        model.injectConnectionForTest(conn);
        std::unique_ptr<MockConnection> connOwner(conn);
        auto detach = qScopeGuard([&]{ model.injectConnectionForTest(nullptr); });

        // Wiring should also prime the connection with the current value.
        model.wireMicPttDisabledForTest();
        pump();

        // Exactly one prime fire with the current model value.
        QCOMPARE(conn->micPttDisabledLog.size(), 1);
        QCOMPARE(conn->micPttDisabledLog.first(), true);
    }
};

QTEST_MAIN(TestRadioModelMicPttWire)
#include "tst_radio_model_mic_ptt_wire.moc"
