// no-port-check: NereusSDR-original direct watch transport regression.
// Modification history (NereusSDR):
//   2026-10-04: Qt 6.4 WebSocket error-signal compatibility. J.J. Boyd
//               (KG4VCF), AI-assisted via OpenAI Codex.
#include <QtTest/QtTest>

#include <QAbstractSocket>
#include <QCryptographicHash>
#include <QDeadlineTimer>
#include <QFile>
#include <QHash>
#include <QHostAddress>
#include <QPointer>
#include <QSignalSpy>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTemporaryDir>
#include <QTcpServer>
#include <QWebSocket>
#include <QWebSocketServer>

#include "core/safety/RemoteTxWatchdog.h"
#include "core/security/CertificateStore.h"
#include "core/session/DataChannelTransport.h"
#include "core/session/IceConfiguration.h"
#include "core/session/RelayLeg.h"
#include "core/session/TxWatchClient.h"

using namespace NereusSDR;

namespace {
class TlsWatchPeer {
public:
    TlsWatchPeer()
        : identity(directory.path())
        , listener(QStringLiteral("watch-test"), QWebSocketServer::SecureMode)
    {
        QFile key(directory.filePath(QStringLiteral("tls-key.pem")));
        if (identity.isValid() && key.open(QIODevice::ReadOnly)) {
            QSslConfiguration tls = QSslConfiguration::defaultConfiguration();
            tls.setLocalCertificate(identity.certificate());
            tls.setPrivateKey(QSslKey(key.readAll(), QSsl::Rsa));
            listener.setSslConfiguration(tls);
            listening = listener.listen(QHostAddress::LocalHost, 0);
            QObject::connect(&listener, &QWebSocketServer::newConnection, &listener,
                             [this]() {
                QWebSocket* socket = listener.nextPendingConnection();
                sockets.append(socket);
                QObject::connect(socket, &QWebSocket::binaryMessageReceived,
                                 &listener, [this, socket](const QByteArray& bytes) {
                    messages[socket].append(bytes);
                });
            });
        }
    }

    QUrl url() const
    {
        return QUrl(QStringLiteral("wss://127.0.0.1:%1/primary?ignored=1")
                        .arg(listener.serverPort()));
    }

    QByteArray pin() const
    {
        return identity.certificate().digest(QCryptographicHash::Sha256);
    }

    QTemporaryDir directory;
    CertificateStore identity;
    QWebSocketServer listener;
    bool listening = false;
    QList<QWebSocket*> sockets;
    QHash<QWebSocket*, QList<QByteArray>> messages;
};

QByteArray ticket()
{
    return QByteArray(32, '\x5a');
}

class WatchSource final : public IceConfiguration::CandidateSource {
public:
    void start(std::function<void(const QString&)> add) override { m_add = std::move(add); }
    void stop() override { m_add = {}; }
    bool ready() const { return static_cast<bool>(m_add); }
    void inject(const QString& candidate) { if (m_add) { m_add(candidate); } }

private:
    std::function<void(const QString&)> m_add;
};

IceConfiguration watchIce(const std::shared_ptr<WatchSource>& source)
{
    IceConfiguration ice = IceConfiguration::throughRendezvous({}, true, {}, {});
    ice.setRelay(std::nullopt, 1);
    ice.setCandidateSourceFactory(
        [source](int lane, const QString& id, bool routed)
            -> std::shared_ptr<IceConfiguration::CandidateSource> {
            return lane == IceConfiguration::kControlLane && id.isEmpty() && !routed
                ? source : nullptr;
        }, true);
    return ice;
}

QString loopbackFor(const QStringList& gathered)
{
    for (const QString& candidate : gathered) {
        const QStringList fields = candidate.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (fields.size() >= 8 && fields.at(4).contains(QLatin1Char('.'))
            && fields.at(7) == QLatin1String("host")) {
            bool ok = false;
            const int port = fields.at(5).toInt(&ok);
            if (ok && port > 0 && port <= 65535) {
                return RelayLeg::candidateLine(IceConfiguration::kControlLane,
                                               static_cast<quint16>(port));
            }
        }
    }
    return {};
}

template <typename Done>
bool until(Done done, int ms)
{
    const QDeadlineTimer deadline(ms);
    while (!done() && !deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return done();
}

struct DtlsWatchPair {
    QTemporaryDir directory;
    CertificateStore identity{directory.path()};
    std::unique_ptr<DataChannelTransport> offerer = std::make_unique<DataChannelTransport>();
    std::unique_ptr<DataChannelTransport> answerer = std::make_unique<DataChannelTransport>();
    std::shared_ptr<WatchSource> offerSource = std::make_shared<WatchSource>();
    std::shared_ptr<WatchSource> answerSource = std::make_shared<WatchSource>();
    bool descriptionsAccepted = true;

    QByteArray pin() const { return identity.certificate().digest(QCryptographicHash::Sha256); }

    bool open(bool awaitChannel = true)
    {
        if (!directory.isValid() || !identity.isValid()) { return false; }
        QObject::connect(offerer.get(), &DataChannelTransport::localDescription,
                         answerer.get(), [this](const QString& sdp, const QString& type) {
            descriptionsAccepted &= answerer->acceptDescription(sdp, type);
        });
        QObject::connect(answerer.get(), &DataChannelTransport::localDescription,
                         offerer.get(), [this](const QString& sdp, const QString& type) {
            descriptionsAccepted &= offerer->acceptDescription(sdp, type);
        });
        DataChannelTransport::Options answer;
        answer.role = DataChannelTransport::Role::Answerer;
        answer.purpose = DataChannelTransport::Purpose::TxWatch;
        answer.maxIncomingBytes = DataChannelTransport::kMaxWatchFrameBytes;
        answer.ice = watchIce(answerSource);
        answer.certificatePemPath = identity.certificatePath();
        answer.privateKeyPemPath = identity.privateKeyPath();
        DataChannelTransport::Options offer = answer;
        offer.role = DataChannelTransport::Role::Offerer;
        offer.ice = watchIce(offerSource);
        offer.certificatePemPath.clear();
        offer.privateKeyPemPath.clear();
        if (!answerer->start(answer) || !offerer->start(offer)) { return false; }
        if (!until([this] {
                return descriptionsAccepted && offerSource->ready() && answerSource->ready()
                    && !loopbackFor(offerer->localCandidatesForTest()).isEmpty()
                    && !loopbackFor(answerer->localCandidatesForTest()).isEmpty();
            }, 10000)) { return false; }
        offerSource->inject(loopbackFor(answerer->localCandidatesForTest()));
        answerSource->inject(loopbackFor(offerer->localCandidatesForTest()));
        return !awaitChannel || until([this] {
            return offerer->isOpen() && answerer->isOpen();
        }, 15000);
    }

    DataChannelTransport* takeOfferer() { return offerer.release(); }
};
}

class TestTxWatchClient : public QObject {
    Q_OBJECT
private slots:
    void relayDtlsChecksPresentedPinThenAttachesAndSendsFrame()
    {
        DtlsWatchPair pair;
        QVERIFY(pair.open(false));
        QSignalSpy atCore(pair.answerer.get(), &SessionTransport::binaryReceived);
        TxWatchClient client;
        QSignalSpy ready(&client, &TxWatchClient::ready);
        QCOMPARE(client.telemetry().receivedPayloadBytes, quint64(0));
        QCOMPARE(client.telemetry().submittedPayloadBytes, quint64(0));
        QVERIFY(client.openRelay(pair.takeOfferer(), pair.pin(), ticket(), 101));
        QTRY_COMPARE_WITH_TIMEOUT(atCore.size(), 1, 15000);
        QCOMPARE(atCore.at(0).at(0).toByteArray(), QByteArray(1, char(1)) + ticket());
        QCOMPARE(client.telemetry().submittedPayloadBytes, quint64(33));
        QVERIFY(!client.isReady());
        QVERIFY(pair.answerer->sendBinary(QByteArray::fromHex("0100")));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 5000);
        QCOMPARE(ready.at(0).at(0).toULongLong(), quint64(101));
        QCOMPARE(client.telemetry().receivedPayloadBytes, quint64(2));
        QVERIFY(client.sendKeepalive(123, 45));
        QTRY_COMPARE_WITH_TIMEOUT(atCore.size(), 2, 5000);
        QCOMPARE(atCore.at(1).at(0).toByteArray(),
                 RemoteTxWatchdog::channelKeepalive(123, 45));
        QCOMPARE(client.telemetry().submittedPayloadBytes,
                 quint64(33 + RemoteTxWatchdog::kChannelKeepaliveBytes));
    }

    void relayDtlsWrongPresentedPinDisclosesNoTicket()
    {
        DtlsWatchPair pair;
        QVERIFY(pair.open());
        QSignalSpy atCore(pair.answerer.get(), &SessionTransport::binaryReceived);
        TxWatchClient client;
        QSignalSpy closed(&client, &TxWatchClient::closed);
        QByteArray wrong = pair.pin();
        wrong[0] = char(wrong.at(0) ^ 1);
        QVERIFY(!client.openRelay(pair.takeOfferer(), wrong, ticket(), 102));
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 5000);
        QCOMPARE(atCore.size(), 0);
        QVERIFY(!client.isReady());
        QCOMPARE(client.telemetry().submittedPayloadBytes, quint64(0));
        QCOMPARE(client.telemetry().receivedPayloadBytes, quint64(0));
    }

    void relayRefusesControlTransportWithoutClosingIt()
    {
        TxWatchClient client;
        auto control = std::make_unique<DataChannelTransport>();
        QVERIFY(!client.openRelay(control.get(), QByteArray(32, 'p'), ticket(), 103));
        QVERIFY(control != nullptr);
        QVERIFY(!control->isOpen());
    }

    void relayInvalidInputsAreRefused_data()
    {
        QTest::addColumn<int>("badField");
        QTest::newRow("pin") << 1;
        QTest::newRow("ticket") << 2;
        QTest::newRow("generation") << 3;
    }

    void relayInvalidInputsAreRefused()
    {
        QFETCH(int, badField);
        DtlsWatchPair pair;
        QVERIFY(pair.open());
        QSignalSpy atCore(pair.answerer.get(), &SessionTransport::binaryReceived);
        TxWatchClient client;
        QByteArray pin = pair.pin();
        QByteArray rawTicket = ticket();
        quint64 generation = 113;
        if (badField == 1) { pin.chop(1); }
        if (badField == 2) { rawTicket.chop(1); }
        if (badField == 3) { generation = 0; }
        QPointer<DataChannelTransport> offered = pair.offerer.get();
        QVERIFY(!client.openRelay(pair.takeOfferer(), pin, rawTicket, generation));
        QTRY_VERIFY(offered.isNull());
        QCOMPARE(atCore.size(), 0);
    }

    void relayReadyHandlerMayDeleteClient()
    {
        DtlsWatchPair pair;
        QVERIFY(pair.open());
        QSignalSpy atCore(pair.answerer.get(), &SessionTransport::binaryReceived);
        QPointer<TxWatchClient> client = new TxWatchClient;
        QObject::connect(client, &TxWatchClient::ready, client,
                         [&client](quint64) { delete client.data(); });
        QVERIFY(client->openRelay(pair.takeOfferer(), pair.pin(), ticket(), 104));
        QTRY_COMPARE_WITH_TIMEOUT(atCore.size(), 1, 5000);
        QVERIFY(pair.answerer->sendBinary(QByteArray::fromHex("0100")));
        QTRY_VERIFY_WITH_TIMEOUT(client.isNull(), 5000);
    }

    void relayClosedHandlerMayReplaceGenerationWithoutOldCallbacks()
    {
        DtlsWatchPair first;
        DtlsWatchPair second;
        QVERIFY(first.open());
        QVERIFY(second.open());
        QSignalSpy atFirst(first.answerer.get(), &SessionTransport::binaryReceived);
        QSignalSpy atSecond(second.answerer.get(), &SessionTransport::binaryReceived);
        TxWatchClient client;
        QSignalSpy ready(&client, &TxWatchClient::ready);
        QPointer<DataChannelTransport> old = first.offerer.get();
        AuxiliaryWatchTelemetry firstAtClose;
        QObject::connect(&client, &TxWatchClient::closed, &client,
                         [&](quint64 generation, const QString&) {
            if (generation != 105) { return; }
            firstAtClose = client.telemetry();
            QVERIFY(client.openRelay(second.takeOfferer(), second.pin(), ticket(), 106));
            if (old) { old->binaryReceived(QByteArray::fromHex("0100")); }
        });
        QVERIFY(client.openRelay(first.takeOfferer(), first.pin(), ticket(), 105));
        QTRY_COMPARE_WITH_TIMEOUT(atFirst.size(), 1, 5000);
        QVERIFY(first.answerer->sendBinary(QByteArray::fromHex("0101")));
        QTRY_COMPARE_WITH_TIMEOUT(atSecond.size(), 1, 5000);
        QCOMPARE(ready.size(), 0);
        QVERIFY(second.answerer->sendBinary(QByteArray::fromHex("0100")));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 5000);
        QCOMPARE(ready.at(0).at(0).toULongLong(), quint64(106));
        QVERIFY(client.isReady());
        QCOMPARE(firstAtClose.submittedPayloadBytes, quint64(33));
        QCOMPARE(firstAtClose.receivedPayloadBytes, quint64(0));
        QCOMPARE(client.telemetry().submittedPayloadBytes, quint64(33));
        QCOMPARE(client.telemetry().receivedPayloadBytes, quint64(2));
    }

    void relaySynchronousTicketSendFailureMayDeleteClient()
    {
        DtlsWatchPair pair;
        QVERIFY(pair.open());
        QSignalSpy atCore(pair.answerer.get(), &SessionTransport::binaryReceived);
        QPointer<TxWatchClient> client = new TxWatchClient;
        QObject::connect(client, &TxWatchClient::closed, client,
                         [&client](quint64, const QString&) { delete client.data(); });
        client->setRelayWriterForTesting(
            [](DataChannelTransport* transport, const QByteArray&) {
                transport->closeLink(QStringLiteral("test send failure"));
                return false;
            });
        QPointer<DataChannelTransport> offerer = pair.offerer.get();
        client->openRelay(pair.takeOfferer(), pair.pin(), ticket(), 107);
        QTRY_VERIFY_WITH_TIMEOUT(client.isNull(), 5000);
        QCOMPARE(atCore.size(), 0);
        QTRY_VERIFY(offerer.isNull());
    }

    void relaySynchronousKeepaliveFailureCannotCloseReplacement()
    {
        DtlsWatchPair first;
        DtlsWatchPair second;
        QVERIFY(first.open());
        QVERIFY(second.open());
        QSignalSpy atFirst(first.answerer.get(), &SessionTransport::binaryReceived);
        QSignalSpy atSecond(second.answerer.get(), &SessionTransport::binaryReceived);
        TxWatchClient client;
        QSignalSpy ready(&client, &TxWatchClient::ready);
        AuxiliaryWatchTelemetry firstAtClose;
        QObject::connect(&client, &TxWatchClient::closed, &client,
                         [&](quint64 generation, const QString&) {
            if (generation == 108) {
                firstAtClose = client.telemetry();
                QVERIFY(client.openRelay(second.takeOfferer(), second.pin(), ticket(), 109));
            }
        });
        QVERIFY(client.openRelay(first.takeOfferer(), first.pin(), ticket(), 108));
        QTRY_COMPARE_WITH_TIMEOUT(atFirst.size(), 1, 5000);
        QVERIFY(first.answerer->sendBinary(QByteArray::fromHex("0100")));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 5000);
        client.setRelayWriterForTesting([&client](DataChannelTransport* transport,
                                                   const QByteArray&) {
            client.setRelayWriterForTesting({});
            transport->closeLink(QStringLiteral("test send failure"));
            return false;
        });
        QVERIFY(!client.sendKeepalive(1, 2));
        QTRY_COMPARE_WITH_TIMEOUT(atSecond.size(), 1, 5000);
        QCOMPARE(atFirst.size(), 1);
        QVERIFY(second.answerer->sendBinary(QByteArray::fromHex("0100")));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 2, 5000);
        QCOMPARE(ready.at(1).at(0).toULongLong(), quint64(109));
        QVERIFY(client.isReady());
        QCOMPARE(firstAtClose.submittedPayloadBytes,
                 quint64(33 + RemoteTxWatchdog::kChannelKeepaliveBytes));
        QCOMPARE(firstAtClose.receivedPayloadBytes, quint64(2));
        QCOMPARE(client.telemetry().submittedPayloadBytes, quint64(33));
        QCOMPARE(client.telemetry().receivedPayloadBytes, quint64(2));
    }

    void relayMissingAckExpires()
    {
        DtlsWatchPair pair;
        QVERIFY(pair.open());
        QSignalSpy atCore(pair.answerer.get(), &SessionTransport::binaryReceived);
        TxWatchClient client;
        client.setDeadlinesForTesting(1000, 100);
        QSignalSpy closed(&client, &TxWatchClient::closed);
        QVERIFY(client.openRelay(pair.takeOfferer(), pair.pin(), ticket(), 110));
        QTRY_COMPARE_WITH_TIMEOUT(atCore.size(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 2000);
        QVERIFY(closed.first().at(1).toString().contains(QStringLiteral("timed out")));
        QVERIFY(!client.isReady());
    }

    void relayBacklogClosesBeforeKeepaliveWrite()
    {
        DtlsWatchPair pair;
        QVERIFY(pair.open());
        QSignalSpy atCore(pair.answerer.get(), &SessionTransport::binaryReceived);
        TxWatchClient client;
        QSignalSpy closed(&client, &TxWatchClient::closed);
        QVERIFY(client.openRelay(pair.takeOfferer(), pair.pin(), ticket(), 111));
        QTRY_COMPARE_WITH_TIMEOUT(atCore.size(), 1, 5000);
        QVERIFY(pair.answerer->sendBinary(QByteArray::fromHex("0100")));
        QTRY_VERIFY_WITH_TIMEOUT(client.isReady(), 5000);
        client.setBacklogBytesForTesting(4084);
        QVERIFY(!client.sendKeepalive(3, 4));
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 5000);
        QCOMPARE(atCore.size(), 1);
        QCOMPARE(client.telemetry().submittedPayloadBytes, quint64(33));
        QCOMPARE(client.telemetry().receivedPayloadBytes, quint64(2));
    }

    void correctFreshTlsPinAttachesAndSendsExistingFrame()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("TLS backend unavailable"); }
        TlsWatchPeer peer;
        QVERIFY(peer.listening);
        TxWatchClient client;
        QSignalSpy ready(&client, &TxWatchClient::ready);
        QCOMPARE(client.telemetry().receivedPayloadBytes, quint64(0));
        QCOMPARE(client.telemetry().submittedPayloadBytes, quint64(0));
        QVERIFY(client.openDirect(peer.url(), peer.pin(), ticket(), 71));
        QTRY_COMPARE_WITH_TIMEOUT(peer.sockets.size(), 1, 5000);
        QWebSocket* socket = peer.sockets.at(0);
        QCOMPARE(socket->requestUrl().path(), QStringLiteral("/tx-watch/v1"));
        QVERIFY(!socket->requestUrl().hasQuery());
        QTRY_COMPARE_WITH_TIMEOUT(peer.messages[socket].size(), 1, 5000);
        QCOMPARE(peer.messages[socket].at(0), QByteArray(1, '\x01') + ticket());
        QCOMPARE(client.telemetry().submittedPayloadBytes, quint64(33));
        QVERIFY(!client.isReady());
        socket->sendBinaryMessage(QByteArray::fromHex("0100"));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 5000);
        QCOMPARE(ready.at(0).at(0).toULongLong(), quint64(71));
        QCOMPARE(client.telemetry().receivedPayloadBytes, quint64(2));
        QVERIFY(client.isReady());
        QVERIFY(client.sendKeepalive(123, 45));
        QTRY_COMPARE_WITH_TIMEOUT(peer.messages[socket].size(), 2, 5000);
        QCOMPARE(peer.messages[socket].at(1),
                 RemoteTxWatchdog::channelKeepalive(123, 45));
        QCOMPARE(client.telemetry().submittedPayloadBytes,
                 quint64(33 + RemoteTxWatchdog::kChannelKeepaliveBytes));
    }

    void wrongActualPinNeverSendsTicket()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("TLS backend unavailable"); }
        TlsWatchPeer peer;
        QVERIFY(peer.listening);
        TxWatchClient client;
        QSignalSpy closed(&client, &TxWatchClient::closed);
        QByteArray wrong = peer.pin();
        wrong[0] = char(wrong.at(0) ^ 1);
        QVERIFY(client.openDirect(peer.url(), wrong, ticket(), 72));
        // A TLS handshake may be rejected before the listener upgrades the socket.
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 5000);
        if (!peer.sockets.isEmpty()) {
            QTest::qWait(100);
            QCOMPARE(peer.messages[peer.sockets.at(0)].size(), 0);
        }
        QVERIFY(!client.isReady());
        QCOMPARE(client.telemetry().submittedPayloadBytes, quint64(0));
        QCOMPARE(client.telemetry().receivedPayloadBytes, quint64(0));
    }

    void badInputsAreRefusedBeforeDial()
    {
        TxWatchClient client;
        const QByteArray pin(32, 'p');
        QVERIFY(!client.openDirect(QUrl(QStringLiteral("ws://127.0.0.1:5")), pin, ticket(), 1));
        QVERIFY(!client.openDirect(QUrl(QStringLiteral("wss://u:p@127.0.0.1:5")), pin, ticket(), 2));
        QVERIFY(!client.openDirect(QUrl(QStringLiteral("wss://@127.0.0.1:5")), pin, ticket(), 6));
        QVERIFY(!client.openDirect(QUrl(QStringLiteral("wss:///nohost")), pin, ticket(), 3));
        QVERIFY(!client.openDirect(QUrl(QStringLiteral("wss://127.0.0.1:5")), pin.left(31), ticket(), 4));
        QVERIFY(!client.openDirect(QUrl(QStringLiteral("wss://127.0.0.1:5")), pin, ticket().left(31), 5));
        QVERIFY(client.findChildren<QWebSocket*>().isEmpty());
    }

    void earlyDataAndDuplicateAckCloseOnlyWatch()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("TLS backend unavailable"); }
        TlsWatchPeer peer;
        QVERIFY(peer.listening);
        TxWatchClient client;
        QSignalSpy closed(&client, &TxWatchClient::closed);
        QVERIFY(client.openDirect(peer.url(), peer.pin(), ticket(), 10));
        QTRY_COMPARE_WITH_TIMEOUT(peer.sockets.size(), 1, 5000);
        QWebSocket* first = peer.sockets.at(0);
        first->sendBinaryMessage(QByteArray(13, 'x'));
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 5000);
        QVERIFY(!client.isReady());
        QCOMPARE(client.telemetry().receivedPayloadBytes, quint64(0));

        QVERIFY(client.openDirect(peer.url(), peer.pin(), ticket(), 11));
        QTRY_COMPARE_WITH_TIMEOUT(peer.sockets.size(), 2, 5000);
        QWebSocket* second = peer.sockets.at(1);
        second->sendBinaryMessage(QByteArray::fromHex("0100"));
        QTRY_VERIFY_WITH_TIMEOUT(client.isReady(), 5000);
        second->sendBinaryMessage(QByteArray::fromHex("0100"));
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 2, 5000);
        QVERIFY(!client.isReady());
        QCOMPARE(client.telemetry().receivedPayloadBytes, quint64(2));
    }

    void missingAckExpiresAndTextIsNeverAccepted()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("TLS backend unavailable"); }
        TlsWatchPeer peer;
        QVERIFY(peer.listening);
        TxWatchClient client;
        client.setDeadlinesForTesting(1000, 100);
        QSignalSpy closed(&client, &TxWatchClient::closed);
        QVERIFY(client.openDirect(peer.url(), peer.pin(), ticket(), 30));
        QTRY_COMPARE_WITH_TIMEOUT(peer.sockets.size(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(peer.messages[peer.sockets.at(0)].size(), 1, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 2000);
        QVERIFY(closed.at(0).at(1).toString().contains(QStringLiteral("timed out")));
        QVERIFY(!client.isReady());

        QVERIFY(client.openDirect(peer.url(), peer.pin(), ticket(), 31));
        QTRY_COMPARE_WITH_TIMEOUT(peer.sockets.size(), 2, 5000);
        peer.sockets.at(1)->sendTextMessage(QStringLiteral("ok"));
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 2, 2000);
        QVERIFY(!client.isReady());
    }

    void oversizedAndMalformedAcknowledgementsAreRefused()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("TLS backend unavailable"); }
        TlsWatchPeer peer;
        QVERIFY(peer.listening);
        TxWatchClient client;
        QSignalSpy closed(&client, &TxWatchClient::closed);
        QVERIFY(client.openDirect(peer.url(), peer.pin(), ticket(), 41));
        QTRY_COMPARE_WITH_TIMEOUT(peer.sockets.size(), 1, 5000);
        peer.sockets.at(0)->sendBinaryMessage(QByteArray::fromHex("0101"));
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 5000);
        QCOMPARE(client.telemetry().receivedPayloadBytes, quint64(0));
        QVERIFY(client.openDirect(peer.url(), peer.pin(), ticket(), 42));
        QTRY_COMPARE_WITH_TIMEOUT(peer.sockets.size(), 2, 5000);
        peer.sockets.at(1)->sendBinaryMessage(QByteArray(34, 'x'));
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 2, 5000);
        QVERIFY(!client.isReady());
        QCOMPARE(client.telemetry().receivedPayloadBytes, quint64(0));
    }

    void closedHandlerMayReplaceGenerationWithoutOldCallbacks()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("TLS backend unavailable"); }
        TlsWatchPeer peer;
        QVERIFY(peer.listening);
        TxWatchClient client;
        QSignalSpy ready(&client, &TxWatchClient::ready);
        QObject::connect(&client, &TxWatchClient::closed, &client,
                         [&](quint64 generation, const QString&) {
            if (generation == 51) {
                QVERIFY(client.openDirect(peer.url(), peer.pin(), ticket(), 52));
            }
        });
        QVERIFY(client.openDirect(peer.url(), peer.pin(), ticket(), 51));
        QTRY_COMPARE_WITH_TIMEOUT(peer.sockets.size(), 1, 5000);
        peer.sockets.at(0)->sendTextMessage(QStringLiteral("bad"));
        QTRY_COMPARE_WITH_TIMEOUT(peer.sockets.size(), 2, 5000);
        peer.sockets.at(1)->sendBinaryMessage(QByteArray::fromHex("0100"));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 5000);
        QCOMPARE(ready.at(0).at(0).toULongLong(), quint64(52));
        QCOMPARE(client.generation(), quint64(52));
        QVERIFY(client.isReady());
    }

    void readyHandlerMayDeleteClient()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("TLS backend unavailable"); }
        TlsWatchPeer peer;
        QVERIFY(peer.listening);
        QPointer<TxWatchClient> client = new TxWatchClient;
        QObject::connect(client, &TxWatchClient::ready, client,
                         [&client](quint64) { delete client.data(); });
        QVERIFY(client->openDirect(peer.url(), peer.pin(), ticket(), 61));
        QTRY_COMPARE_WITH_TIMEOUT(peer.sockets.size(), 1, 5000);
        peer.sockets.at(0)->sendBinaryMessage(QByteArray::fromHex("0100"));
        QTRY_VERIFY_WITH_TIMEOUT(client.isNull(), 5000);
    }

    void fullOutboundBacklogClosesWithoutSendingFrame()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("TLS backend unavailable"); }
        TlsWatchPeer peer;
        QVERIFY(peer.listening);
        TxWatchClient client;
        QSignalSpy closed(&client, &TxWatchClient::closed);
        QVERIFY(client.openDirect(peer.url(), peer.pin(), ticket(), 70));
        QTRY_COMPARE_WITH_TIMEOUT(peer.sockets.size(), 1, 5000);
        QWebSocket* socket = peer.sockets.at(0);
        QTRY_COMPARE_WITH_TIMEOUT(peer.messages[socket].size(), 1, 5000);
        socket->sendBinaryMessage(QByteArray::fromHex("0100"));
        QTRY_VERIFY_WITH_TIMEOUT(client.isReady(), 5000);
        client.setBacklogBytesForTesting(4084);
        QVERIFY(!client.sendKeepalive(9, 2));
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 5000);
        QCOMPARE(peer.messages[socket].size(), 1);
        QCOMPARE(client.telemetry().submittedPayloadBytes, quint64(33));
        QCOMPARE(client.telemetry().receivedPayloadBytes, quint64(2));
    }

    void stalledLocalTlsOpeningExpires()
    {
        QTcpServer silent;
        QVERIFY(silent.listen(QHostAddress::LocalHost, 0));
        TxWatchClient client;
        client.setDeadlinesForTesting(100, 100);
        QSignalSpy closed(&client, &TxWatchClient::closed);
        const QUrl url(QStringLiteral("wss://127.0.0.1:%1/primary")
                           .arg(silent.serverPort()));
        QVERIFY(client.openDirect(url, QByteArray(32, 'p'), ticket(), 80));
        QTRY_COMPARE_WITH_TIMEOUT(closed.size(), 1, 2000);
        QVERIFY(closed.at(0).at(1).toString().contains(QStringLiteral("timed out")));
        QVERIFY(!client.isReady());
    }

    void sendingBeforeAckClosesTheWatch()
    {
        QTcpServer silent;
        QVERIFY(silent.listen(QHostAddress::LocalHost, 0));
        TxWatchClient client;
        QSignalSpy closed(&client, &TxWatchClient::closed);
        const QUrl url(QStringLiteral("wss://127.0.0.1:%1/").arg(silent.serverPort()));
        QVERIFY(client.openDirect(url, QByteArray(32, 'p'), ticket(), 81));
        QVERIFY(!client.sendKeepalive(1, 1));
        QCOMPARE(closed.size(), 1);
        QCOMPARE(closed.at(0).at(0).toULongLong(), quint64(81));
        QVERIFY(!client.isReady());
    }

    void synchronousInitialSendFailureMayDeleteClient()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("TLS backend unavailable"); }
        TlsWatchPeer peer;
        QVERIFY(peer.listening);
        QPointer<TxWatchClient> client = new TxWatchClient;
        QObject::connect(client, &TxWatchClient::closed, client,
                         [&client](quint64, const QString&) { delete client.data(); });
        client->setBinaryWriterForTesting(
            [](QWebSocket* socket, const QByteArray&) -> qint64 {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
                socket->errorOccurred(QAbstractSocket::NetworkError);
#else
                socket->error(QAbstractSocket::NetworkError);
#endif
                return -1;
            });
        QVERIFY(client->openDirect(peer.url(), peer.pin(), ticket(), 90));
        QTRY_VERIFY_WITH_TIMEOUT(client.isNull(), 5000);
        if (!peer.sockets.isEmpty()) {
            QCOMPARE(peer.messages[peer.sockets.at(0)].size(), 0);
        }
    }

    void synchronousInitialSendFailureCannotCloseReplacement()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("TLS backend unavailable"); }
        TlsWatchPeer peer;
        QVERIFY(peer.listening);
        TxWatchClient client;
        QSignalSpy ready(&client, &TxWatchClient::ready);
        AuxiliaryWatchTelemetry firstAtClose;
        QObject::connect(&client, &TxWatchClient::closed, &client,
                         [&](quint64 generation, const QString&) {
            if (generation == 94) {
                firstAtClose = client.telemetry();
                QVERIFY(client.openDirect(peer.url(), peer.pin(), ticket(), 95));
            }
        });
        client.setBinaryWriterForTesting(
            [&client](QWebSocket* socket, const QByteArray&) -> qint64 {
                client.setBinaryWriterForTesting({});
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
                socket->errorOccurred(QAbstractSocket::NetworkError);
#else
                socket->error(QAbstractSocket::NetworkError);
#endif
                return -1;
            });
        QVERIFY(client.openDirect(peer.url(), peer.pin(), ticket(), 94));
        QTRY_COMPARE_WITH_TIMEOUT(peer.sockets.size(), 2, 5000);
        QTRY_COMPARE_WITH_TIMEOUT(peer.messages[peer.sockets.at(1)].size(), 1, 5000);
        peer.sockets.at(1)->sendBinaryMessage(QByteArray::fromHex("0100"));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 5000);
        QCOMPARE(ready.at(0).at(0).toULongLong(), quint64(95));
        QVERIFY(client.isReady());
        QCOMPARE(firstAtClose.submittedPayloadBytes, quint64(33));
        QCOMPARE(firstAtClose.receivedPayloadBytes, quint64(0));
        QCOMPARE(client.telemetry().submittedPayloadBytes, quint64(33));
        QCOMPARE(client.telemetry().receivedPayloadBytes, quint64(2));
    }

    void synchronousKeepaliveSendFailureCannotCloseReplacement()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("TLS backend unavailable"); }
        TlsWatchPeer peer;
        QVERIFY(peer.listening);
        TxWatchClient client;
        QSignalSpy ready(&client, &TxWatchClient::ready);
        AuxiliaryWatchTelemetry firstAtClose;
        QObject::connect(&client, &TxWatchClient::closed, &client,
                         [&](quint64 generation, const QString&) {
            if (generation == 91) {
                firstAtClose = client.telemetry();
                QVERIFY(client.openDirect(peer.url(), peer.pin(), ticket(), 92));
            }
        });
        QVERIFY(client.openDirect(peer.url(), peer.pin(), ticket(), 91));
        QTRY_COMPARE_WITH_TIMEOUT(peer.sockets.size(), 1, 5000);
        peer.sockets.at(0)->sendBinaryMessage(QByteArray::fromHex("0100"));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 1, 5000);
        client.setBinaryWriterForTesting(
            [&client](QWebSocket* socket, const QByteArray&) -> qint64 {
                client.setBinaryWriterForTesting({});
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
                socket->errorOccurred(QAbstractSocket::NetworkError);
#else
                socket->error(QAbstractSocket::NetworkError);
#endif
                return -1;
            });
        QVERIFY(!client.sendKeepalive(2, 3));
        QTRY_COMPARE_WITH_TIMEOUT(peer.sockets.size(), 2, 5000);
        peer.sockets.at(1)->sendBinaryMessage(QByteArray::fromHex("0100"));
        QTRY_COMPARE_WITH_TIMEOUT(ready.size(), 2, 5000);
        QCOMPARE(ready.at(1).at(0).toULongLong(), quint64(92));
        QCOMPARE(client.generation(), quint64(92));
        QVERIFY(client.isReady());
        QCOMPARE(firstAtClose.submittedPayloadBytes,
                 quint64(33 + RemoteTxWatchdog::kChannelKeepaliveBytes));
        QCOMPARE(firstAtClose.receivedPayloadBytes, quint64(2));
        QCOMPARE(client.telemetry().submittedPayloadBytes, quint64(33));
        QCOMPARE(client.telemetry().receivedPayloadBytes, quint64(2));
    }

    void synchronousKeepaliveSendFailureMayDeleteClient()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("TLS backend unavailable"); }
        TlsWatchPeer peer;
        QVERIFY(peer.listening);
        QPointer<TxWatchClient> client = new TxWatchClient;
        QVERIFY(client->openDirect(peer.url(), peer.pin(), ticket(), 93));
        QTRY_COMPARE_WITH_TIMEOUT(peer.sockets.size(), 1, 5000);
        peer.sockets.at(0)->sendBinaryMessage(QByteArray::fromHex("0100"));
        QTRY_VERIFY_WITH_TIMEOUT(client->isReady(), 5000);
        QObject::connect(client, &TxWatchClient::closed, client,
                         [&client](quint64, const QString&) { delete client.data(); });
        client->setBinaryWriterForTesting(
            [](QWebSocket* socket, const QByteArray&) -> qint64 {
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
                socket->errorOccurred(QAbstractSocket::NetworkError);
#else
                socket->error(QAbstractSocket::NetworkError);
#endif
                return -1;
            });
        QVERIFY(!client->sendKeepalive(4, 5));
        QVERIFY(client.isNull());
    }
};

QTEST_GUILESS_MAIN(TestTxWatchClient)
#include "tst_tx_watch_client.moc"
