// no-port-check: NereusSDR-original watch relay ownership tests.
#include <QtTest>
#include <QDateTime>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QWebSocket>
#include <QWebSocketServer>

#include "core/session/DataChannelTransport.h"
#include "core/session/RelayLeg.h"
#include "fakes/DataChannelPair.h"

using namespace NereusSDR;

class TstWatchRelayContext : public QObject {
    Q_OBJECT
private slots:
    void cleanup()
    {
        RelayLeg::setRelayUrlForTest({});
        DataChannelTransport::setSelectedPathOverrideForTest({});
    }

    void belongsToLivePrimaryAndExpiryOnlyPreventsNewAdmissions()
    {
        QWebSocketServer relay(QStringLiteral("test relay"), QWebSocketServer::NonSecureMode);
        QVERIFY(relay.listen(QHostAddress::LocalHost, 0));
        QPointer<QWebSocket> socket;
        connect(&relay, &QWebSocketServer::newConnection, &relay, [&]() {
            socket = relay.nextPendingConnection();
            connect(socket, &QWebSocket::binaryMessageReceived, &relay,
                    [&](const QByteArray& frame) {
                if (!frame.isEmpty() && quint8(frame.at(0)) == RelayLeg::kTagJoin) {
                    socket->sendBinaryMessage(QByteArray::fromHex("810101"));
                }
            });
        });
        RelayLeg::setRelayUrlForTest(QUrl(QStringLiteral("ws://127.0.0.1:%1")
                                            .arg(relay.serverPort())));
        auto leg = RelayLeg::create();
        QVERIFY(leg);
        leg->open(QUrl(QStringLiteral("wss://relay.example/ws")), QStringLiteral("primary"));
        QTRY_COMPARE(leg->state(), RelayLeg::State::Joined);
        QVERIFY(leg->peerPresent());

        DataChannelTransport primary;
        DataChannelTransport other;
        QVERIFY(Test::startDataChannelPair(&primary, &other, 65536, 65536, {}, {}));
        QTRY_VERIFY_WITH_TIMEOUT(primary.isOpen() && other.isOpen(), 15000);
        bool relayPath = false;
        DataChannelTransport::setSelectedPathOverrideForTest(
            [&](const DataChannelTransport*) -> std::optional<MediaIcePath> {
                MediaIcePath path;
                path.remoteAddress = relayPath ? QStringLiteral("127.0.0.1")
                                               : QStringLiteral("192.0.2.1");
                path.ownedLoopbackShim = relayPath;
                return path;
            });
        DataChannelTransport::WatchRelayGrant grant{
            QUrl(QStringLiteral("wss://relay.example/ws")), QStringLiteral("watch"),
            QDateTime::currentSecsSinceEpoch() + 3, leg};
        QVERIFY(primary.setWatchRelayGrant(grant));
        QVERIFY(!primary.setWatchRelayGrant(grant)); // immutable introduction context
        QVERIFY(!primary.hasWatchRelayRoute()); // a direct selected path is not relay
        relayPath = true;
        QVERIFY(primary.hasWatchRelayRoute());
        QVERIFY(primary.canOpenWatchRelay());
        socket->sendBinaryMessage(QByteArray::fromHex("8200"));
        QTRY_VERIFY(!leg->peerPresent());
        QVERIFY(!primary.hasWatchRelayRoute());
        socket->sendBinaryMessage(QByteArray::fromHex("8201"));
        QTRY_VERIFY(leg->peerPresent());
        QVERIFY(primary.hasWatchRelayRoute());
        QTRY_VERIFY_WITH_TIMEOUT(!primary.canOpenWatchRelay(), 5000);
        QVERIFY(primary.hasWatchRelayRoute()); // existing admitted connection remains valid
        primary.closeLink(QStringLiteral("test complete"));
        QVERIFY(!primary.hasWatchRelayRoute());
        QVERIFY(!primary.watchRelayGrant().has_value());
        QVERIFY(!other.watchRelayGrant().has_value());
        leg->close();
    }

    void refusesExpiredAndMissingPrimaryGrant()
    {
        DataChannelTransport primary;
        DataChannelTransport other;
        QVERIFY(Test::startDataChannelPair(&primary, &other, 65536, 65536, {}, {}));
        auto leg = RelayLeg::create();
        QVERIFY(leg);
        DataChannelTransport::WatchRelayGrant grant{
            QUrl(QStringLiteral("wss://relay.example/ws")), QStringLiteral("watch"),
            QDateTime::currentSecsSinceEpoch() - 1, leg};
        QVERIFY(!primary.setWatchRelayGrant(grant));
        grant.expires = QDateTime::currentSecsSinceEpoch() + 120;
        grant.url = QUrl(QStringLiteral("wss://user@relay.example/ws"));
        QVERIFY(!primary.setWatchRelayGrant(grant));
        grant.url = QUrl(QStringLiteral("wss://relay.example/ws"));
        leg.reset();
        QVERIFY(!primary.setWatchRelayGrant(grant));
        QVERIFY(!primary.watchRelayGrant().has_value());
    }
};

QTEST_GUILESS_MAIN(TstWatchRelayContext)
#include "tst_watch_relay_context.moc"
