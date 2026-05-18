// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: This test fabricates synthetic Socket.IO Connect-ACK
// payloads to drive FreeDVReporterClient into the connected state
// without a real qso.freedv.org socket. Precedent matches
// tst_freedv_reporter_socketio.cpp (Phase 3J-2 Task B5).
//
// NereusSDR - FreeDVRadeReporterBridge tests
//
// Pins the contract that the bridge mirrors the freedv-gui RADE rx_report
// upload paths from MainFrame::OnTimer:
//
//   - Path A (callsign-decoded via EOO): existing path lives in
//     RadioModel::onRadeTextDecoded and stays there; this file pins
//     Path B only.
//   - Path B (RADE synced, no callsign): every 10th 100 ms tick, emit
//     a Socket.IO "rx_report" with empty callsign, mode "RADEV1", and
//     the current rounded SNR. Drives qso.freedv.org's "we are
//     receiving something" marker even before an EOO frame arrives.
//
// Source: freedv-gui/src/main.cpp:1971-1996 [@77e793a].
//
// Bridge slots are exercised directly via test seam (tickForTest)
// rather than the real 100 ms QTimer so the suite stays deterministic.

#include <QtTest>
#include <QSignalSpy>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "core/FreeDVRadeReporterBridge.h"
#include "core/FreeDVReporterClient.h"

using namespace NereusSDR;

namespace {

// Drive a real FreeDVReporterClient into the connected state so its
// outbound sendRxReport call survives the `m_connected.load()` gate.
// Mirrors the handshake exercised in tst_freedv_reporter_socketio.cpp.
void markClientConnected(FreeDVReporterClient& c) {
    c.handleSocketIOForTest(QStringLiteral("0{\"sid\":\"xyz\"}"));
    QVERIFY(c.isConnected());
}

// Extract the "snr" / "callsign" / "mode" fields from the last
// Socket.IO Event message stored by the client (test seam
// lastSentMessageForTest). Returns a default-constructed object if no
// message was sent.
QJsonObject lastRxReportPayload(const FreeDVReporterClient& c) {
    const QString msg = c.lastSentMessageForTest();
    if (msg.isEmpty()) return {};
    // Wire shape: "42<json-array>" where the JSON array is
    // ["rx_report", {payload}].
    if (!msg.startsWith(QStringLiteral("42"))) return {};
    const QString jsonPart = msg.mid(2);
    const QJsonDocument doc = QJsonDocument::fromJson(jsonPart.toUtf8());
    if (!doc.isArray()) return {};
    const QJsonArray arr = doc.array();
    if (arr.size() < 2) return {};
    if (arr[0].toString() != QStringLiteral("rx_report")) return {};
    return arr[1].toObject();
}

} // namespace

class TestFreeDVRadeReporterBridge : public QObject
{
    Q_OBJECT

private slots:

    // Path B: 30 ticks while synced with a valid SNR -> exactly 3
    // rx_report emits (10 Hz timer modulo 10 == 1 Hz effective rate,
    // matching freedv-gui main.cpp:1982 `m_reportCounter % 10`).
    void pathB_emitsEveryTenTicksWhileSynced()
    {
        FreeDVReporterClient client;
        markClientConnected(client);

        FreeDVRadeReporterBridge bridge(&client, nullptr);
        bridge.setReportingEnabled(true);
        bridge.onRadeSnrChanged(0, 7.0f);
        bridge.onRadeSyncChanged(0, true);

        int emitCount = 0;
        for (int i = 0; i < 30; ++i) {
            client.clearLastSentForTest();
            bridge.tickForTest();
            if (!client.lastSentMessageForTest().isEmpty()) {
                ++emitCount;
            }
        }
        QCOMPARE(emitCount, 3);
    }

    // Path B: no sync -> zero emits regardless of tick count.
    void pathB_skipsWhenUnsynced()
    {
        FreeDVReporterClient client;
        markClientConnected(client);

        FreeDVRadeReporterBridge bridge(&client, nullptr);
        bridge.setReportingEnabled(true);
        bridge.onRadeSnrChanged(0, 7.0f);
        // Never calls onRadeSyncChanged(true).

        int emitCount = 0;
        for (int i = 0; i < 30; ++i) {
            client.clearLastSentForTest();
            bridge.tickForTest();
            if (!client.lastSentMessageForTest().isEmpty()) {
                ++emitCount;
            }
        }
        QCOMPARE(emitCount, 0);
    }

    // Path B: TX active -> zero emits even while synced. Matches
    // freedv-gui main.cpp:1756 `(!halfDuplexState || !txState)`.
    void pathB_skipsWhenTxActive()
    {
        FreeDVReporterClient client;
        markClientConnected(client);

        FreeDVRadeReporterBridge bridge(&client, nullptr);
        bridge.setReportingEnabled(true);
        bridge.onRadeSnrChanged(0, 7.0f);
        bridge.onRadeSyncChanged(0, true);
        bridge.onMoxStateChanged(true); // TX engaged

        int emitCount = 0;
        for (int i = 0; i < 30; ++i) {
            client.clearLastSentForTest();
            bridge.tickForTest();
            if (!client.lastSentMessageForTest().isEmpty()) {
                ++emitCount;
            }
        }
        QCOMPARE(emitCount, 0);
    }

    // Path B: reporter disconnected -> zero emits. Mirrors freedv-gui
    // FreeDVReporter.cpp:191 isFullyConnected_ check inside
    // addReceiveRecord, hoisted into the bridge for early-out.
    void pathB_skipsWhenReporterDisconnected()
    {
        FreeDVReporterClient client; // never call markClientConnected

        FreeDVRadeReporterBridge bridge(&client, nullptr);
        bridge.setReportingEnabled(true);
        bridge.onRadeSnrChanged(0, 7.0f);
        bridge.onRadeSyncChanged(0, true);

        int emitCount = 0;
        for (int i = 0; i < 30; ++i) {
            client.clearLastSentForTest();
            bridge.tickForTest();
            if (!client.lastSentMessageForTest().isEmpty()) {
                ++emitCount;
            }
        }
        QCOMPARE(emitCount, 0);
    }

    // Path B: no SNR sample arrived yet -> bridge has nothing to
    // upload, zero emits. Matches freedv-gui main.cpp:1737
    // `!(isnan(snrEstimate) || isinf(snrEstimate))` gate.
    void pathB_skipsWhenSnrIsNan()
    {
        FreeDVReporterClient client;
        markClientConnected(client);

        FreeDVRadeReporterBridge bridge(&client, nullptr);
        bridge.setReportingEnabled(true);
        // No onRadeSnrChanged call - bridge's m_snrDb stays NaN.
        bridge.onRadeSyncChanged(0, true);

        int emitCount = 0;
        for (int i = 0; i < 30; ++i) {
            client.clearLastSentForTest();
            bridge.tickForTest();
            if (!client.lastSentMessageForTest().isEmpty()) {
                ++emitCount;
            }
        }
        QCOMPARE(emitCount, 0);
    }

    // Path B wire shape: callsign empty, mode "RADEV1". Matches
    // freedv-gui main.cpp:1988-1993 addReceiveRecord call.
    void pathB_uploadsEmptyCallsignAndRadev1Mode()
    {
        FreeDVReporterClient client;
        markClientConnected(client);

        FreeDVRadeReporterBridge bridge(&client, nullptr);
        bridge.setReportingEnabled(true);
        bridge.onRadeSnrChanged(0, 5.0f);
        bridge.onRadeSyncChanged(0, true);

        // Drive 10 ticks to land on the modulo-10 boundary.
        for (int i = 0; i < 10; ++i) {
            bridge.tickForTest();
        }
        const QJsonObject payload = lastRxReportPayload(client);
        QVERIFY(!payload.isEmpty());
        QCOMPARE(payload.value(QStringLiteral("callsign")).toString(),
                 QString()); // empty string
        QCOMPARE(payload.value(QStringLiteral("mode")).toString(),
                 QStringLiteral("RADEV1"));
        QCOMPARE(payload.value(QStringLiteral("snr")).toInt(), 5);
    }

    // Path B SNR rounding matches upstream (int)(snr + 0.5):
    //   7.4 -> 7, 7.5 -> 8. Matches freedv-gui main.cpp:1884
    //   `auto pendingSnr = (int)(g_snr + 0.5);`
    void pathB_roundsSnrToNearest()
    {
        FreeDVReporterClient client;
        markClientConnected(client);

        FreeDVRadeReporterBridge bridge(&client, nullptr);
        bridge.setReportingEnabled(true);
        bridge.onRadeSyncChanged(0, true);

        // Case 1: 7.4 should round down to 7.
        bridge.onRadeSnrChanged(0, 7.4f);
        for (int i = 0; i < 10; ++i) {
            bridge.tickForTest();
        }
        QCOMPARE(lastRxReportPayload(client).value(QStringLiteral("snr")).toInt(),
                 7);

        // Case 2: 7.5 should round up to 8.
        bridge.onRadeSnrChanged(0, 7.5f);
        for (int i = 0; i < 10; ++i) {
            bridge.tickForTest();
        }
        QCOMPARE(lastRxReportPayload(client).value(QStringLiteral("snr")).toInt(),
                 8);
    }
};

QTEST_MAIN(TestFreeDVRadeReporterBridge)
#include "tst_freedv_rade_reporter_bridge.moc"
