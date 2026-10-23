// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_happy_eyeballs.cpp  (NereusSDR)
// =================================================================
//
// Happy Eyeballs (RFC 8305) for WebSockets to a host name
// (src/core/session/HappyEyeballs.h). The case seen on 2026-10-22: the
// name's IPv6 address leaves the connection unanswered (SYN_SENT), its IPv4
// address answers. A plain socket waits on the silent address; the race
// connects to the next one within its 250 ms attempt delay. The silent
// address here is tests/StalledListener.h's (::1 with a full accept queue
// on Linux, the unconfigured 127.0.0.2 on macOS). Also: the family order, the
// WebSocket through the in-process tunnel (Host header, bytes both ways,
// the far end's real address), the cases left as before (an address
// literal, a proxy the computer's settings name), and a tunnel that
// carries only its own CONNECT. Also: no Nagle on either tunnel socket
// (and the link diagnostic reads the network side), and a failed race
// reaching the WebSocket as its real cause, never as a proxy fault.
//
// Which tests need the silent address (StalledListener.h, confirmed for
// 3.5 s): Linux runs them over ::1, or over 127.0.0.2 when it has no IPv6;
// macOS over 127.0.0.2; Windows has no silent loopback path and skips them,
// saying why. Every other test runs everywhere.
//
// Loopback only; nothing leaves this computer.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-22: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-10-22: tunnelSocketsSendWithoutNagle,
//               failureStatusesNameTheCause, aFailedRaceReachesTheWebSocket.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QElapsedTimer>
#include <QNetworkProxy>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QWebSocket>
#include <QWebSocketServer>

#include "StalledListener.h"
#include "core/session/HappyEyeballs.h"
#include "core/session/SessionTransport.h"
#include "core/session/SystemProxy.h"

using namespace NereusSDR;

namespace {

const QHostAddress kV6(QStringLiteral("::1"));
const QHostAddress kV4(QStringLiteral("127.0.0.1"));

// Well inside RendezvousClient's 10 s hello timer, and far beyond the 250 ms
// attempt delay plus a loopback connect, even on a loaded machine.
constexpr int kWellInsideMs = 3000;

} // namespace

class TstHappyEyeballs final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void cleanup()
    {
        HappyEyeballs::setResolverForTest({});
        SystemProxy::setProxyForTest(std::nullopt);
    }

    void familiesAreInterleavedFromTheFirst()
    {
        const QHostAddress a6(QStringLiteral("2001:db8::1"));
        const QHostAddress b6(QStringLiteral("2001:db8::2"));
        const QHostAddress c6(QStringLiteral("2001:db8::3"));
        const QHostAddress a4(QStringLiteral("192.0.2.1"));
        const QHostAddress b4(QStringLiteral("192.0.2.2"));
        QCOMPARE(HappyEyeballs::interleaveFamilies({a6, b6, c6, a4, b4}),
                 QList<QHostAddress>({a6, a4, b6, b4, c6}));
        QCOMPARE(HappyEyeballs::interleaveFamilies({a4, b4, a6}),
                 QList<QHostAddress>({a4, a6, b4}));
        QCOMPARE(HappyEyeballs::interleaveFamilies({a6, b6}), QList<QHostAddress>({a6, b6}));
        QCOMPARE(HappyEyeballs::interleaveFamilies({}), QList<QHostAddress>());
    }

    // The failure seen live, on this computer: a plain socket to the
    // name's first (silent) address is still opening long after the race
    // below has connected. This is what every attempt did before the fix.
    void aPlainSocketWaitsOnTheSilentAddress()
    {
        QTcpServer answering;
        QVERIFY(answering.listen(kV4, 0));
        Test::StalledListener silent;
        const QHostAddress silentAddress = silent.start(answering.serverPort());
        if (silentAddress.isNull()) {
            QSKIP(qPrintable(silent.whyNot));
        }
        QTcpSocket plain;
        plain.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        plain.connectToHost(silentAddress, answering.serverPort());
        QTest::qWait(kWellInsideMs);
        QCOMPARE(plain.state(), QAbstractSocket::ConnectingState);
        plain.abort();
    }

    void theRaceConnectsPastASilentFirstAddress()
    {
        QTcpServer answering;
        QVERIFY(answering.listen(kV4, 0));
        Test::StalledListener silent;
        const QHostAddress silentAddress = silent.start(answering.serverPort());
        if (silentAddress.isNull()) {
            QSKIP(qPrintable(silent.whyNot));
        }
        QStringList asked;
        HappyEyeballs::setResolverForTest([&asked, silentAddress](const QString& host) {
            asked.append(host);
            return QList<QHostAddress>{silentAddress, kV4};
        });
        HappyEyeballsConnector connector;
        QSignalSpy connected(&connector, &HappyEyeballsConnector::connected);
        QSignalSpy failed(&connector, &HappyEyeballsConnector::failed);
        QElapsedTimer clock;
        clock.start();
        connector.start(QStringLiteral("rv.example.test"), answering.serverPort());
        QTRY_COMPARE_WITH_TIMEOUT(connected.size(), 1, kWellInsideMs);
        const qint64 elapsed = clock.elapsed();
        QVERIFY2(elapsed >= HappyEyeballs::kConnectionAttemptDelayMs - 20,
                 qPrintable(QString::number(elapsed)));
        QCOMPARE(failed.size(), 0);
        QCOMPARE(asked, QStringList({QStringLiteral("rv.example.test")}));
        auto* socket = connected.at(0).at(0).value<QTcpSocket*>();
        QVERIFY(socket != nullptr);
        QCOMPARE(socket->state(), QAbstractSocket::ConnectedState);
        QCOMPARE(socket->peerAddress(), kV4);
        // The losing attempt is gone.
        QTRY_COMPARE(connector.findChildren<QTcpSocket*>().size(), 1);
    }

    // A refused first address moves on at once, not after the delay.
    void aRefusedAddressMovesOnAtOnce()
    {
        QTcpServer answering;
        QVERIFY(answering.listen(kV4, 0));
        QTcpServer closedV6;
        if (!closedV6.listen(kV6, answering.serverPort())) {
            QSKIP("no IPv6 loopback on this computer");
        }
        closedV6.close();  // ::1 at that port now refuses.
        HappyEyeballs::setResolverForTest(
            [](const QString&) { return QList<QHostAddress>{kV6, kV4}; });
        HappyEyeballsConnector connector;
        QSignalSpy connected(&connector, &HappyEyeballsConnector::connected);
        connector.start(QStringLiteral("rv.example.test"), answering.serverPort());
        QTRY_COMPARE_WITH_TIMEOUT(connected.size(), 1, kWellInsideMs);
        QCOMPARE(connected.at(0).at(0).value<QTcpSocket*>()->peerAddress(), kV4);
    }

    void aNameWithNoAnswerFails()
    {
        QTcpServer closed;
        QVERIFY(closed.listen(kV4, 0));
        const quint16 port = closed.serverPort();
        closed.close();
        HappyEyeballs::setResolverForTest(
            [](const QString& host) {
                return host == QLatin1String("empty.example.test") ? QList<QHostAddress>{}
                                                                    : QList<QHostAddress>{kV4};
            });
        HappyEyeballsConnector unresolved;
        QSignalSpy unresolvedFailed(&unresolved, &HappyEyeballsConnector::failed);
        unresolved.start(QStringLiteral("empty.example.test"), port);
        QTRY_COMPARE(unresolvedFailed.size(), 1);
        QCOMPARE(unresolvedFailed.at(0).at(0).value<QAbstractSocket::SocketError>(),
                 QAbstractSocket::HostNotFoundError);
        HappyEyeballsConnector refused;
        QSignalSpy refusedFailed(&refused, &HappyEyeballsConnector::failed);
        refused.start(QStringLiteral("closed.example.test"), port);
        QTRY_COMPARE_WITH_TIMEOUT(refusedFailed.size(), 1, kWellInsideMs);
        QCOMPARE(refusedFailed.at(0).at(0).value<QAbstractSocket::SocketError>(),
                 QAbstractSocket::ConnectionRefusedError);
    }

    // The WebSocket keeps the name (its Host header; TLS would verify the
    // same name) while the tunnel races the addresses, carries large
    // messages both ways, and reports the far end's real address.
    void aWebSocketReachesANamePastASilentAddress()
    {
        QWebSocketServer server(QStringLiteral("service"), QWebSocketServer::NonSecureMode);
        QVERIFY(server.listen(kV4, 0));
        const quint16 port = server.serverPort();
        Test::StalledListener silent;
        const QHostAddress silentAddress = silent.start(port);
        if (silentAddress.isNull()) {
            QSKIP(qPrintable(silent.whyNot));
        }
        HappyEyeballs::setResolverForTest(
            [silentAddress](const QString&) { return QList<QHostAddress>{silentAddress, kV4}; });
        QList<QWebSocket*> accepted;
        QStringList hostHeaders;
        QByteArray serverReceived;
        connect(&server, &QWebSocketServer::newConnection, this, [&] {
            while (QWebSocket* peer = server.nextPendingConnection()) {
                hostHeaders.append(QString::fromLatin1(peer->request().rawHeader("Host")));
                connect(peer, &QWebSocket::binaryMessageReceived, peer,
                        [&serverReceived](const QByteArray& message) { serverReceived = message; });
                accepted.append(peer);
            }
        });

        const QUrl url(QStringLiteral("ws://localhost:%1/").arg(port));
        QWebSocket socket;
        socket.setMaxAllowedIncomingMessageSize(8 * 1024 * 1024);
        const QNetworkProxy proxy = HappyEyeballs::proxyForWebSocket(url, &socket);
        QCOMPARE(proxy.type(), QNetworkProxy::HttpProxy);
        QCOMPARE(QHostAddress(proxy.hostName()), kV4);
        socket.setProxy(proxy);
        QSignalSpy connected(&socket, &QWebSocket::connected);
        QByteArray clientReceived;
        connect(&socket, &QWebSocket::binaryMessageReceived, this,
                [&clientReceived](const QByteArray& message) { clientReceived = message; });
        QElapsedTimer clock;
        clock.start();
        socket.open(url);
        QTRY_COMPARE_WITH_TIMEOUT(connected.size(), 1, kWellInsideMs);
        QVERIFY2(clock.elapsed() < kWellInsideMs, qPrintable(QString::number(clock.elapsed())));
        QCOMPARE(hostHeaders, QStringList({QStringLiteral("localhost:%1").arg(port)}));

        const QAbstractSocket* upstream = HappyEyeballs::upstreamOf(&socket);
        QVERIFY(upstream != nullptr);
        QCOMPARE(upstream->peerAddress(), kV4);
        QCOMPARE(upstream->peerPort(), port);
        // WebSocketTransport::backlogBytes() reads the WebSocket's own
        // socket by findChild: still the one it finds first.
        const auto* inner = socket.findChild<QAbstractSocket*>();
        QVERIFY(inner != nullptr);
        QVERIFY(inner->parent() == &socket);

        // Bigger than the tunnel's bounded buffer, each way.
        QByteArray up(3 * 1024 * 1024 + 7, '\0');
        for (qsizetype i = 0; i < up.size(); ++i) {
            up[i] = static_cast<char>(i * 31);
        }
        const QByteArray down = QByteArray(up).replace('\0', '\x5a');
        socket.sendBinaryMessage(up);
        QTRY_COMPARE_WITH_TIMEOUT(serverReceived.size(), up.size(), 15000);
        QCOMPARE(serverReceived, up);
        QCOMPARE(accepted.size(), 1);
        accepted.first()->sendBinaryMessage(down);
        QTRY_COMPARE_WITH_TIMEOUT(clientReceived.size(), down.size(), 15000);
        QCOMPARE(clientReceived, down);

        // A close from the far end reaches the WebSocket.
        QSignalSpy disconnected(&socket, &QWebSocket::disconnected);
        accepted.first()->close();
        QTRY_COMPARE_WITH_TIMEOUT(disconnected.size(), 1, kWellInsideMs);
    }

    // No Nagle anywhere on a tunnelled WebSocket: both tunnel sockets (the
    // accepted loopback connection and the connection to the far end) send
    // small messages at once, whatever leg uses the tunnel, and the link
    // diagnostic reports the network side's TCP_NODELAY.
    void tunnelSocketsSendWithoutNagle()
    {
        QWebSocketServer server(QStringLiteral("service"), QWebSocketServer::NonSecureMode);
        QVERIFY(server.listen(kV4, 0));
        SystemProxy::setProxyForTest(QNetworkProxy(QNetworkProxy::NoProxy));
        HappyEyeballs::setResolverForTest(
            [](const QString&) { return QList<QHostAddress>{kV4}; });
        const QUrl url(QStringLiteral("ws://nagle.example.test:%1/").arg(server.serverPort()));
        auto* socket = new QWebSocket;
        WebSocketTransport transport(socket, 1024 * 1024);
        const QNetworkProxy proxy = HappyEyeballs::proxyForWebSocket(url, socket);
        QCOMPARE(proxy.type(), QNetworkProxy::HttpProxy);
        socket->setProxy(proxy);
        QSignalSpy connected(socket, &QWebSocket::connected);
        socket->open(url);
        QTRY_COMPARE_WITH_TIMEOUT(connected.size(), 1, kWellInsideMs);

        const auto* tunnel =
            socket->findChild<HappyEyeballsTunnel*>(QString(), Qt::FindDirectChildrenOnly);
        QVERIFY(tunnel != nullptr);
        const QList<QTcpSocket*> sides =
            tunnel->findChildren<QTcpSocket*>(QString(), Qt::FindDirectChildrenOnly);
        QCOMPARE(sides.size(), 2);
        QVERIFY(sides.contains(HappyEyeballs::upstreamOf(socket)));
        for (QTcpSocket* side : sides) {
            QCOMPARE(side->state(), QAbstractSocket::ConnectedState);
            // Non-zero when set (macOS reads back the flag's bit, 4).
            QVERIFY(side->socketOption(QAbstractSocket::LowDelayOption).toInt() != 0);
        }

        // The WebSocket's own (loopback) socket with Nagle on: the
        // diagnostic still reports the network side, which has it off.
        auto* inner = socket->findChild<QAbstractSocket*>();
        QVERIFY(inner != nullptr && inner->parent() == socket);
        inner->setSocketOption(QAbstractSocket::LowDelayOption, 0);
        const SessionLinkDiagnostics link = transport.linkDiagnostics();
        QVERIFY(link.noDelay.has_value());
        QVERIFY(*link.noDelay);
    }

    // Qt's HTTP proxy client turns a CONNECT reply into the socket's error
    // (qhttpsocketengine.cpp, Qt 6.11.0 lines 640-653): 404 is
    // HostNotFoundError, 503 ConnectionRefusedError, any other status
    // ProxyProtocolError ("Error communicating with HTTP proxy").
    void failureStatusesNameTheCause_data()
    {
        QTest::addColumn<QAbstractSocket::SocketError>("error");
        QTest::addColumn<QByteArray>("statusLine");
        QTest::newRow("no address") << QAbstractSocket::HostNotFoundError
                                    << QByteArray("HTTP/1.1 404 Not Found");
        QTest::newRow("refused") << QAbstractSocket::ConnectionRefusedError
                                 << QByteArray("HTTP/1.1 503 Service Unavailable");
        QTest::newRow("unreachable") << QAbstractSocket::NetworkError
                                     << QByteArray("HTTP/1.1 503 Service Unavailable");
        QTest::newRow("timed out") << QAbstractSocket::SocketTimeoutError
                                   << QByteArray("HTTP/1.1 503 Service Unavailable");
    }

    void failureStatusesNameTheCause()
    {
        QFETCH(QAbstractSocket::SocketError, error);
        QFETCH(QByteArray, statusLine);
        QCOMPARE(HappyEyeballs::failureStatusLine(error), statusLine);
    }

    // A race that fails reaches the WebSocket as its real cause: a name
    // with no address as HostNotFoundError, a refused port as
    // ConnectionRefusedError, never as a fault of a proxy the operator
    // never set up.
    void aFailedRaceReachesTheWebSocket_data()
    {
        QTest::addColumn<bool>("resolves");
        QTest::addColumn<QAbstractSocket::SocketError>("expected");
        QTest::newRow("unresolvable name") << false << QAbstractSocket::HostNotFoundError;
        QTest::newRow("refused port") << true << QAbstractSocket::ConnectionRefusedError;
    }

    void aFailedRaceReachesTheWebSocket()
    {
        QFETCH(bool, resolves);
        QFETCH(QAbstractSocket::SocketError, expected);
        QTcpServer closed;
        QVERIFY(closed.listen(kV4, 0));
        const quint16 port = closed.serverPort();
        closed.close();
        SystemProxy::setProxyForTest(QNetworkProxy(QNetworkProxy::NoProxy));
        HappyEyeballs::setResolverForTest([resolves](const QString&) {
            return resolves ? QList<QHostAddress>{kV4} : QList<QHostAddress>{};
        });
        const QUrl url(QStringLiteral("ws://fails.example.test:%1/").arg(port));
        QWebSocket socket;
        const QNetworkProxy proxy = HappyEyeballs::proxyForWebSocket(url, &socket);
        QCOMPARE(proxy.type(), QNetworkProxy::HttpProxy);
        socket.setProxy(proxy);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        QSignalSpy errors(&socket, &QWebSocket::errorOccurred);
#else
        QSignalSpy errors(&socket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error));
#endif
        socket.open(url);
        QTRY_VERIFY_WITH_TIMEOUT(!errors.isEmpty(), kWellInsideMs);
        QCOMPARE(errors.first().first().value<QAbstractSocket::SocketError>(), expected);
        QCOMPARE(socket.error(), expected);
        QVERIFY2(!socket.errorString().contains(QLatin1String("proxy"), Qt::CaseInsensitive),
                 qPrintable(socket.errorString()));
    }

    // The tunnel ends with its WebSocket.
    void theTunnelEndsWithItsSocket()
    {
        SystemProxy::setProxyForTest(QNetworkProxy(QNetworkProxy::NoProxy));
        QPointer<HappyEyeballsTunnel> tunnel;
        {
            QWebSocket socket;
            HappyEyeballs::proxyForWebSocket(QUrl(QStringLiteral("wss://rv.example.test/")),
                                             &socket);
            tunnel = socket.findChild<HappyEyeballsTunnel*>();
            QVERIFY(tunnel);
        }
        QVERIFY(tunnel.isNull());
    }

    void addressesAndNamedProxiesAreLeftAsBefore()
    {
        // This computer's own proxy settings play no part.
        SystemProxy::setProxyForTest(QNetworkProxy(QNetworkProxy::NoProxy));
        QWebSocket socket;
        for (const char* url : {"wss://127.0.0.1:8443/", "wss://[::1]:8443/",
                                "wss://192.0.2.10/", "wss://[2001:db8::10]/"}) {
            QCOMPARE(HappyEyeballs::proxyForWebSocket(QUrl(QString::fromLatin1(url)), &socket)
                         .type(),
                     QNetworkProxy::NoProxy);
        }
        QCOMPARE(socket.findChildren<HappyEyeballsTunnel*>().size(), 0);
        QVERIFY(HappyEyeballs::upstreamOf(&socket) == nullptr);
        const QNetworkProxy named(QNetworkProxy::HttpProxy, QStringLiteral("proxy.example.test"),
                                  3128);
        SystemProxy::setProxyForTest(named);
        QCOMPARE(HappyEyeballs::proxyForWebSocket(QUrl(QStringLiteral("wss://rv.example.test/")),
                                                  &socket),
                 named);
        QCOMPARE(socket.findChildren<HappyEyeballsTunnel*>().size(), 0);
    }

    // The loopback listener carries one CONNECT, to its own host and port.
    void theTunnelCarriesOnlyItsOwnConnect_data()
    {
        QTest::addColumn<QByteArray>("request");
        QTest::newRow("another host")
            << QByteArray("CONNECT other.example.test:443 HTTP/1.1\r\n\r\n");
        QTest::newRow("another port")
            << QByteArray("CONNECT rv.example.test:444 HTTP/1.1\r\n\r\n");
        QTest::newRow("not a CONNECT") << QByteArray("GET / HTTP/1.1\r\nHost: x\r\n\r\n");
    }

    void theTunnelCarriesOnlyItsOwnConnect()
    {
        QFETCH(QByteArray, request);
        HappyEyeballs::setResolverForTest(
            [](const QString&) { return QList<QHostAddress>{kV4}; });
        HappyEyeballsTunnel tunnel(QStringLiteral("rv.example.test"), 443);
        QVERIFY(tunnel.isListening());
        QTcpSocket client;
        client.setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        client.connectToHost(kV4, tunnel.proxy().port());
        QVERIFY(client.waitForConnected(kWellInsideMs));
        QTest::ignoreMessage(QtWarningMsg, "Tunnel refused a request that was not its CONNECT");
        client.write(request);
        QTRY_VERIFY_WITH_TIMEOUT(client.bytesAvailable() > 0, kWellInsideMs);
        QVERIFY2(client.readAll().startsWith("HTTP/1.1 403"), request.constData());
        QTRY_COMPARE_WITH_TIMEOUT(client.state(), QAbstractSocket::UnconnectedState,
                                  kWellInsideMs);
        // One connection only: a second one finds nothing listening.
        QVERIFY(!tunnel.isListening());
    }
};

QTEST_MAIN(TstHappyEyeballs)
#include "tst_happy_eyeballs.moc"
