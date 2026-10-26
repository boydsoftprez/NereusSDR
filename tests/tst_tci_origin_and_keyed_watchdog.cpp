// =================================================================
// tests/tst_tci_origin_and_keyed_watchdog.cpp  (NereusSDR)
// =================================================================
//
// Ported from Longpath source:
//   tests/tst_tci_mox_release_on_disconnect.cpp and
//   tests/tst_tci_origin_and_tx_time_cap.cpp [@383f058], original headers
//   from Longpath source are included below.
//
// Longpath (Martin Fischer, OE5SOS, https://github.com/oe5sos/Longpath)
// is a fork of NereusSDR distributed under the GNU General Public
// License version 3 (its root LICENSE). Of the two upstream files,
// tst_tci_mox_release_on_disconnect.cpp opens with an
// SPDX-License-Identifier: GPL-2.0-or-later line (kept below) and no GPL
// text; tst_tci_origin_and_tx_time_cap.cpp has no licence line at all.
// Neither has a full GPL header; Longpath's project-level LICENSE applies.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22 - Ported from Longpath by J.J. Boyd (KG4VCF), with
//                AI-assisted transformation via Claude Code: the browser
//                origin checks, run on the first listener and on an extra
//                one, and the keyed-client watchdog checked against a real
//                RadioModel (MOX itself, not a bookkeeping signal). The
//                transmit time cap tests are not ported.
//   2026-10-22 - Review fix wave by J.J. Boyd (KG4VCF), with AI-assisted
//                transformation via Claude Code: Origin: null, a key
//                dropped another way, the operator's own key, the desktop
//                program key, a pong from another app; the live control
//                at 200 ms.
// =================================================================
//
// --- From tst_tci_mox_release_on_disconnect.cpp ---
// SPDX-License-Identifier: GPL-2.0-or-later
// tests/tst_tci_mox_release_on_disconnect.cpp (NereusSDR)
//
// A TCI client that keys the radio (trx:N,true) and then goes away — WSJT-X
// crashing mid-over, a remote client losing its link — must not leave the
// transmitter keyed. Thetis has exactly that gap; other station servers
// close it with a transmit lease and a heartbeat. TciServer's version
// (2026-09-17): remember who keyed, and unkey when that socket vanishes.
//
// The first three tests run without a RadioModel (server(nullptr), the same
// injection path tst_tci_tx_mutex uses) and check the bookkeeping through
// the moxReleasedOnClientLoss signal. The last two put a real RadioModel
// behind the server and check MOX itself — including the case that must
// NOT unkey: the operator keyed after the client let go.
// no-port-check: NereusSDR-original integration test.
//
// --- From tst_tci_origin_and_tx_time_cap.cpp ---
// no-port-check: Longpath-eigener Prüfstand. Kein Thetis-Vorbild — weder
// Thetis noch deskHPSDR noch AetherSDR prüfen die Herkunft einer
// WebSocket-Verbindung oder decken die Sendezeit; beides ist hier neu.
//
// Zwei Sicherungen, die vor jedem Fernbedienungsbetrieb nötig sind:
//
// 1. HERKUNFTSPRÜFUNG. WebSocket-Verbindungen unterliegen NICHT der
//    Gleiche-Herkunft-Regel. Ohne Prüfung konnte jede Webseite, die der
//    Bediener irgendwo im Browser offen hatte, `new WebSocket(
//    "ws://127.0.0.1:50001")` aufmachen und `trx:0,true;` senden — also die
//    Station tasten. Die Bindung an 127.0.0.1 half dagegen NICHT: sie hält
//    andere Rechner fern, nicht einen Browser auf demselben Rechner.
//    Die Regel muss dabei native Clients (WSJT-X, JTDX, N1MM+, Logger)
//    unangetastet lassen — die senden gar keinen Origin-Kopf.
//
// 2. SENDEZEIT-DECKEL. Der vorhandene Wachhund (2026-09-17) fängt einen
//    Client, dessen Socket stirbt, und einen, der auf Pings nicht mehr
//    antwortet. Er fängt NICHT den Client, der munter weiterantwortet und
//    trotzdem sendet, weil das Telefon in der Tasche liegt. Dagegen hilft
//    nur eine harte Obergrenze je Sendevorgang.

#ifdef HAVE_WEBSOCKETS

#include <QtTest>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QUrl>
#include <QWebSocket>

#include "core/AppSettings.h"
#include "core/MoxController.h"
#include "core/SliceOwnership.h"
#include "core/TciServer.h"
#include "core/TxSliceArbiter.h"
#include "core/safety/BandPlanGuard.h"
#include "core/safety/TransmitHolder.h"
#include "models/RadioModel.h"

using namespace NereusSDR;

namespace {

// From Longpath tests/tst_tci_origin_and_tx_time_cap.cpp:38-43 [@383f058]
// Ein Client, der sich wie ein Browser meldet: mit Origin-Kopf.
void openWithOrigin(QWebSocket& ws, const QString& host, quint16 port,
                    const QByteArray& origin)
{
    QNetworkRequest req{QUrl(QStringLiteral("ws://%1:%2").arg(host).arg(port))};
    req.setRawHeader(QByteArrayLiteral("Origin"), origin);
    ws.open(req);
}

void allowEveryKey(MoxController* mox)
{
    mox->setMoxCheck([]() {
        return safety::BandPlanGuard::MoxCheckResult{true, QString()};
    });
}

// From Longpath tests/tst_tci_mox_release_on_disconnect.cpp:44-108 [@383f058]
// A hung client. QWebSocket answers pings by itself, deep in the
// protocol layer, so it cannot play a frozen program. This speaks just
// enough WebSocket over a raw TCP socket to shake hands and key the radio
// (one masked text frame, RFC 6455 §5.3), and then answers nothing — the
// socket stays open, the pings pile up unread.
class HungClient {
public:
    // Server and client share this thread, so nothing here may block:
    // the server needs the event loop to accept the socket and answer the
    // handshake. Every wait is a qWait loop.
    bool connectAndKey(quint16 port,
                       const QByteArray& trx = QByteArrayLiteral("trx:0,true;"))
    {
        sock.connectToHost(QHostAddress::LocalHost, port);
        if (!waitUntil([&] { return sock.state() == QAbstractSocket::ConnectedState; })) {
            return false;
        }
        sock.write(QByteArrayLiteral(
            "GET / HTTP/1.1\r\n"
            "Host: 127.0.0.1\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n\r\n"));
        sock.flush();
        QByteArray reply;
        if (!waitUntil([&] {
                reply += sock.readAll();
                return reply.contains("\r\n\r\n");
            })) {
            return false;
        }
        if (!reply.startsWith("HTTP/1.1 101")) { return false; }
        sock.write(maskedTextFrame(trx));
        sock.flush();
        return true;
    }

    template <typename Pred>
    static bool waitUntil(Pred pred, int timeoutMs = 2000)
    {
        QElapsedTimer t;
        t.start();
        while (t.elapsed() < timeoutMs) {
            if (pred()) { return true; }
            QTest::qWait(10);
        }
        return pred();
    }

    static QByteArray maskedTextFrame(const QByteArray& payload)
    {
        Q_ASSERT(payload.size() < 126);
        QByteArray f;
        f.append(char(0x81));                          // FIN + text
        f.append(char(0x80 | payload.size()));         // masked, short length
        const char mask[4] = {0x12, 0x34, 0x56, 0x78};
        f.append(mask, 4);
        for (int i = 0; i < payload.size(); ++i) {
            f.append(char(payload.at(i) ^ mask[i % 4]));
        }
        return f;
    }

    QTcpSocket sock;
};

} // namespace

class TestTciOriginAndKeyedWatchdog : public QObject {
    Q_OBJECT

private slots:
    // From Longpath tests/tst_tci_origin_and_tx_time_cap.cpp:46-53 [@383f058]
    void init()
    {
        // Jeder Prüfpunkt startet mit leerer Erlaubnisliste. (Die
        // Einstellungen liegen im Testmodus-Sandkasten, siehe
        // TestSandboxInit.cpp — die echte Datei des Bedieners wird nie
        // angefasst.)
        AppSettings::instance().setValue(QStringLiteral("TciAllowedOrigins"),
                                         QString());
    }

    // From Longpath tests/tst_tci_origin_and_tx_time_cap.cpp:57-68 [@383f058]
    void native_client_without_origin_is_accepted()
    {
        TciServer server(nullptr);
        QVERIFY(server.start(0));

        // QWebSocket sendet von sich aus keinen Origin — genau wie WSJT-X,
        // N1MM+ oder TCI Remote. Diese Clients dürfen sich nichts ändern.
        QWebSocket client;
        QSignalSpy connectedSpy(&client, &QWebSocket::connected);
        client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        QVERIFY2(connectedSpy.wait(2000),
                 "a client with no Origin header must still be accepted");
        QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 1, 2000);
        client.close();
        server.stop();
    }

    // From Longpath tests/tst_tci_origin_and_tx_time_cap.cpp:70-84 [@383f058]
    void browser_origin_is_refused_by_default()
    {
        TciServer server(nullptr);
        QVERIFY(server.start(0));

        QWebSocket client;
        QSignalSpy connectedSpy(&client, &QWebSocket::connected);
        openWithOrigin(client, QStringLiteral("127.0.0.1"), server.port(),
                       QByteArrayLiteral("https://example.com"));

        // Der Handschlag muss scheitern. Wir warten die gleiche Zeit wie oben
        // und verlangen, dass NICHTS verbunden wurde.
        QTest::qWait(1500);
        QCOMPARE(connectedSpy.count(), 0);
        QVERIFY2(server.clientCount() == 0,
                 "a refused origin must leave no session behind");
        server.stop();
    }

    // NereusSDR: the Core's station server listens on more than one
    // address (start(QList) and addListener); each of them refuses.
    void browser_origin_is_refused_on_an_extra_listener()
    {
        TciServer server(nullptr);
        QVERIFY(server.start(0));
        if (!server.addListener(QHostAddress(QHostAddress::LocalHostIPv6))) {
            QSKIP("no IPv6 loopback on this machine for a second listener");
        }

        QWebSocket browser;
        QSignalSpy browserConnected(&browser, &QWebSocket::connected);
        openWithOrigin(browser, QStringLiteral("[::1]"), server.port(),
                       QByteArrayLiteral("https://example.com"));
        QTest::qWait(1500);
        QCOMPARE(browserConnected.count(), 0);
        QCOMPARE(server.clientCount(), 0);

        QWebSocket native;
        QSignalSpy nativeConnected(&native, &QWebSocket::connected);
        native.open(QUrl(QStringLiteral("ws://[::1]:%1").arg(server.port())));
        QVERIFY(nativeConnected.wait(2000));
        QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 1, 2000);
        native.close();
        server.stop();
    }

    // NereusSDR: a sandboxed page (or a file: URL) sends Origin: null; it
    // is a browser all the same, and refused.
    void null_origin_is_refused()
    {
        TciServer server(nullptr);
        QVERIFY(server.start(0));

        QWebSocket client;
        QSignalSpy connectedSpy(&client, &QWebSocket::connected);
        openWithOrigin(client, QStringLiteral("127.0.0.1"), server.port(),
                       QByteArrayLiteral("null"));
        QTest::qWait(1500);
        QCOMPARE(connectedSpy.count(), 0);
        QCOMPARE(server.clientCount(), 0);
        server.stop();
    }

    // From Longpath tests/tst_tci_origin_and_tx_time_cap.cpp:86-99 [@383f058]
    void listed_origin_is_accepted()
    {
        AppSettings::instance().setValue(
            QStringLiteral("TciAllowedOrigins"),
            QStringLiteral("http://longpath.local, http://192.168.1.5:8080"));

        TciServer server(nullptr);
        QVERIFY(server.start(0));

        QWebSocket client;
        QSignalSpy connectedSpy(&client, &QWebSocket::connected);
        openWithOrigin(client, QStringLiteral("127.0.0.1"), server.port(),
                       QByteArrayLiteral("http://192.168.1.5:8080"));
        QVERIFY2(connectedSpy.wait(2000),
                 "a listed origin must be let through");
        client.close();
        server.stop();
    }

    // From Longpath tests/tst_tci_origin_and_tx_time_cap.cpp:101-112 [@383f058]
    void origin_match_ignores_case_and_spaces()
    {
        AppSettings::instance().setValue(QStringLiteral("TciAllowedOrigins"),
                                         QStringLiteral("  HTTP://Longpath.Local  "));

        TciServer server(nullptr);
        QVERIFY(server.start(0));

        QWebSocket client;
        QSignalSpy connectedSpy(&client, &QWebSocket::connected);
        openWithOrigin(client, QStringLiteral("127.0.0.1"), server.port(),
                       QByteArrayLiteral("http://longpath.local"));
        QVERIFY(connectedSpy.wait(2000));
        client.close();
        server.stop();
    }

    // From Longpath tests/tst_tci_mox_release_on_disconnect.cpp:260-287 [@383f058]
    void a_hung_client_that_stops_answering_pings_is_released()
    {
        // The other way a keying client goes missing: not a closed socket but
        // a frozen program. The watchdog pings the owner and lets go after
        // three unanswered pings — the socket stays open.
        //
        // NereusSDR: against a real RadioModel, so the check is MOX itself.
        RadioModel core;
        MoxController* mox = core.moxController();
        QVERIFY(mox != nullptr);
        allowEveryKey(mox);
        TciServer server(&core);
        server.setKeyedWatchdog(100, 3);      // 1000 ms / 3 in the app; fast here
        QVERIFY(server.start(0));

        HungClient hung;
        QVERIFY2(hung.connectAndKey(server.port()), "raw WebSocket handshake failed");
        QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 1, 2000);
        QTRY_VERIFY_WITH_TIMEOUT(core.mox(), 2000);

        // ≥ 3 unanswered pings at 100 ms → released well inside 2 s.
        QTRY_VERIFY_WITH_TIMEOUT(!core.mox(), 2000);
        QVERIFY(!mox->isTciPttHeld());
        // And the socket is still there: the watchdog releases the key, it
        // does not throw the client out.
        QCOMPARE(hung.sock.state(), QAbstractSocket::ConnectedState);
        QCOMPARE(server.clientCount(), 1);

        hung.sock.abort();
        QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 0, 2000);
        QVERIFY(!core.mox());
        server.stop();
    }

    // From Longpath tests/tst_tci_mox_release_on_disconnect.cpp:289-310 [@383f058]
    void a_live_client_that_answers_pings_keeps_the_key()
    {
        // The control: a client whose event loop runs answers every ping, and
        // the watchdog must never fire on it, however long it stays keyed.
        RadioModel core;
        MoxController* mox = core.moxController();
        QVERIFY(mox != nullptr);
        allowEveryKey(mox);
        TciServer server(&core);
        // NereusSDR: 200 ms x 3 and a 2 s wait, so a loaded machine that
        // runs the event loop late cannot miss three answers in a row.
        server.setKeyedWatchdog(200, 3);
        QVERIFY(server.start(0));

        QWebSocket client;
        QSignalSpy connected(&client, &QWebSocket::connected);
        client.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        QVERIFY(connected.wait(2000));
        QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 1, 2000);
        client.sendTextMessage(QStringLiteral("trx:0,true;"));
        QTRY_VERIFY_WITH_TIMEOUT(core.mox(), 2000);

        // 2 s at 200 ms = ~10 pings, each answered by QWebSocket itself.
        QTest::qWait(2000);
        QVERIFY(core.mox());
        QCOMPARE(server.clientCount(), 1);

        client.sendTextMessage(QStringLiteral("trx:0,false;"));
        QTRY_VERIFY_WITH_TIMEOUT(!core.mox(), 3000);
        client.close();
        server.stop();
    }

    // NereusSDR: the operator unkeys while a hung app holds the key. The
    // unkey drops the TCI level (MoxController::dropPttOnUnkey) without
    // the server hearing of it; the watchdog then has nothing to guard and
    // must stop, with no release and no warning.
    void a_key_dropped_another_way_stops_the_watchdog()
    {
        RadioModel core;
        MoxController* mox = core.moxController();
        QVERIFY(mox != nullptr);
        allowEveryKey(mox);
        TciServer server(&core);
        server.setKeyedWatchdog(100, 3);
        QVERIFY(server.start(0));

        HungClient hung;
        QVERIFY2(hung.connectAndKey(server.port()), "raw WebSocket handshake failed");
        QTRY_VERIFY_WITH_TIMEOUT(core.mox(), 2000);
        core.setMoxFromButton(false);
        QTRY_VERIFY_WITH_TIMEOUT(!core.mox(), 3000);
        QVERIFY(!mox->isTciPttHeld());

        QTest::failOnWarning(QRegularExpression(QStringLiteral("stopped answering pings")));
        // Well past three unanswered pings at 100 ms.
        QTest::qWait(1000);
        QVERIFY(!core.mox());
        QCOMPARE(server.clientCount(), 1);
        hung.sock.abort();
        QTRY_COMPARE_WITH_TIMEOUT(server.clientCount(), 0, 2000);
        server.stop();
    }

    // NereusSDR: the operator holds his own key while a hung app's key and
    // TX audio are on. The watchdog releases the app's key; the operator's
    // key keeps the radio on, and the app loses the TX audio lock (as a
    // disconnect takes it), so its audio does not stay on the air.
    void operator_key_stays_on_and_the_hung_app_loses_tx_audio()
    {
        RadioModel core;
        MoxController* mox = core.moxController();
        QVERIFY(mox != nullptr);
        allowEveryKey(mox);
        TciServer server(&core);
        server.setKeyedWatchdog(100, 3);
        QVERIFY(server.start(0));

        HungClient hung;
        QVERIFY2(hung.connectAndKey(server.port(), QByteArrayLiteral("trx:0,true,tci;")),
                 "raw WebSocket handshake failed");
        QTRY_VERIFY_WITH_TIMEOUT(core.mox(), 2000);
        QTRY_COMPARE_WITH_TIMEOUT(server.activeTxClientCount(), 1, 2000);
        mox->setManualKey(true);

        QTRY_VERIFY_WITH_TIMEOUT(!mox->isTciPttHeld(), 2000);
        QVERIFY(core.mox());
        QTRY_COMPARE_WITH_TIMEOUT(server.activeTxClientCount(), 0, 2000);
        QCOMPARE(hung.sock.state(), QAbstractSocket::ConnectedState);

        mox->setManualKey(false);
        core.setMoxFromButton(false);
        QTRY_VERIFY_WITH_TIMEOUT(!core.mox(), 3000);
        hung.sock.abort();
        server.stop();
    }

    // NereusSDR: on a desktop host the app's trx is the station's program
    // key (releaseDesktopProgramKey); a hung app loses it the same way.
    void a_hung_app_loses_the_desktop_program_key()
    {
        RadioModel radio;
        const int owned = radio.addSlice(QStringLiteral("pan-0"));
        radio.sliceOwnership()->setOwner(owned, SliceOwnership::stationDevice());
        TransmitHolder holder;
        MoxController* mox = radio.moxController();
        mox->setKeyingGate([&holder](PttMode source, const KeyerIdentity& keyer) {
            return holder.askKey({keyer.deviceId, TransmitHolder::Source::Device,
                                  keyer.program, source == PttMode::Vox});
        });
        holder.transferTo(TransmitHolder::Holder{SliceOwnership::stationDevice()},
                          QStringLiteral("test"), [](bool) {});
        radio.setTransmitHolder(SliceOwnership::stationDevice());
        QVERIFY(radio.txSliceArbiter()->bindForHolder(SliceOwnership::stationDevice(), owned));
        TciServer server(&radio);
        server.setDesktopHostMode(true);
        server.setKeyedWatchdog(100, 3);
        QVERIFY(server.start(0));

        HungClient hung;
        QVERIFY2(hung.connectAndKey(server.port(), QByteArrayLiteral("trx:0,true,tci;")),
                 "raw WebSocket handshake failed");
        QTRY_VERIFY_WITH_TIMEOUT(radio.mox(), 2000);
        QCOMPARE(mox->currentKeyer(), KeyerIdentity::station(PttMode::Tci));
        QTRY_COMPARE_WITH_TIMEOUT(server.activeTxClientCount(), 1, 2000);

        QTRY_VERIFY_WITH_TIMEOUT(!radio.mox(), 2000);
        QVERIFY(!mox->isTciPttHeld());
        QTRY_COMPARE_WITH_TIMEOUT(server.activeTxClientCount(), 0, 2000);
        QCOMPARE(hung.sock.state(), QAbstractSocket::ConnectedState);
        hung.sock.abort();
        server.stop();
    }

    // NereusSDR: only the key holder's pongs count. Another app that
    // answers the keepalive pings often must not keep a hung holder keyed.
    void a_pong_from_another_app_does_not_count()
    {
        RadioModel core;
        MoxController* mox = core.moxController();
        QVERIFY(mox != nullptr);
        allowEveryKey(mox);
        TciServer server(&core);
        server.setKeyedWatchdog(100, 3);
        server.setPingIntervalMs(50);
        QVERIFY(server.start(0));

        qRegisterMetaType<QWebSocket*>("QWebSocket*");
        QSignalSpy serverSide(&server, &TciServer::clientConnected);
        QWebSocket other;
        QSignalSpy connected(&other, &QWebSocket::connected);
        other.open(QUrl(QStringLiteral("ws://127.0.0.1:%1").arg(server.port())));
        QVERIFY(connected.wait(2000));
        QTRY_COMPARE_WITH_TIMEOUT(serverSide.count(), 1, 2000);
        auto* otherOnServer = serverSide.at(0).at(0).value<QWebSocket*>();
        QVERIFY(otherOnServer != nullptr);
        QSignalSpy otherPongs(otherOnServer, &QWebSocket::pong);

        HungClient hung;
        QVERIFY2(hung.connectAndKey(server.port()), "raw WebSocket handshake failed");
        QTRY_VERIFY_WITH_TIMEOUT(core.mox(), 2000);
        QTRY_VERIFY_WITH_TIMEOUT(!core.mox(), 2000);
        // The other app was answering all along.
        QVERIFY2(otherPongs.count() >= 3, qPrintable(QString::number(otherPongs.count())));
        QCOMPARE(server.clientCount(), 2);
        other.close();
        hung.sock.abort();
        server.stop();
    }
};

QTEST_GUILESS_MAIN(TestTciOriginAndKeyedWatchdog)
#include "tst_tci_origin_and_keyed_watchdog.moc"

#else
// WebSockets not available — test file must still compile and produce a
// no-op binary so CTest doesn't report a missing executable.
int main() { return 0; }
#endif // HAVE_WEBSOCKETS
