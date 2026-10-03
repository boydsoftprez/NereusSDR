// no-port-check: NereusSDR-original auxiliary watch protocol tests.
#include "core/session/TxWatchServer.h"
#include "core/session/SessionTransport.h"
#include "core/safety/RemoteTxWatchdog.h"
#include "core/session/StationServer.h"
#include "core/session/SessionMessages.h"
#include "core/security/DeviceAuthenticator.h"
#include "core/security/DeviceStore.h"
#include "core/security/StationIdentity.h"
#include "core/AppSettings.h"
#include "models/RadioModel.h"
#include "fakes/UpgradedCoreToken.h"

#include <QtTest>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSslConfiguration>
#include <QSslSocket>
#include <QTemporaryDir>
#include <QWebSocket>

#include <functional>

using namespace NereusSDR;

namespace {
class Link final : public SessionTransport {
public:
    bool open = true;
    QByteArray lastBinary;
    int closes = 0;
    qint64 backlog = 0;
    std::function<void()> onSendBinary;
    std::function<void()> onClose;
    void sendText(const QByteArray&) override {}
    void ping() override {}
    void closeLink(const QString&) override
    {
        if (open) {
            open = false;
            ++closes;
            emit closed(); // deliberately synchronous, to catch reentrant erasure
            const auto callback = onClose;
            if (callback) {
                callback(); // may delete this transport and its watch owner
            }
        }
    }
    bool isOpen() const override { return open; }
    QString peerDescription() const override { return QStringLiteral("test"); }
    bool sendBinary(const QByteArray& message) override
    {
        lastBinary = message;
        const bool accepted = open;
        const auto callback = onSendBinary;
        if (callback) {
            callback(); // may delete this transport and its watch owner
        }
        return accepted;
    }
    qint64 backlogBytes() const override { return backlog; }
    void binary(const QByteArray& message) { emit binaryReceived(message); }
    void text() { emit textReceived(QByteArrayLiteral("wrong")); }
};

QByteArray ticket(char fill)
{
    return QByteArray(TxWatchServer::kTicketBytes, fill);
}

QByteArray attach(const QByteArray& raw)
{
    return QByteArray(1, '\x01') + raw;
}

struct Rig {
    qint64 time = 100000;
    quint64 generation = 1;
    bool permitted = true;
    int delivered = 0;
    Link primary;
    TxWatchServer server;

    Rig() : server(
        [this](SessionTransport* link, quint64 id, const QByteArray& device, quint64 gen) {
            return permitted && link == &primary && primary.open && id == 7
                && device == QByteArrayLiteral("paired") && gen == generation;
        },
        [this](const QByteArray&, quint64, quint32) { ++delivered; }, nullptr,
        [this]() { return time; }) {}

    std::optional<TxWatchServer::Ticket> issue(char fill = 'a')
    {
        return server.issue(&primary, 7, QByteArrayLiteral("paired"), generation, ticket(fill));
    }
    Link* socket(const QString& address = QStringLiteral("shared-nat"))
    {
        auto* link = new Link;
        server.acceptTransport(link, address);
        return link;
    }
};
} // namespace

class TestTxWatchServer : public QObject {
    Q_OBJECT
private slots:
    void oneUseTicketAndStrictFrames()
    {
        Rig rig;
        QVERIFY(rig.issue());
        QVERIFY(!rig.issue('b'));
        Link* text = rig.socket();
        text->text();
        QCOMPARE(text->closes, 1);
        Link* malformed = rig.socket();
        malformed->binary(RemoteTxWatchdog::channelKeepalive(1, 9));
        QCOMPARE(malformed->closes, 1);
        Link* wrong = rig.socket();
        wrong->binary(attach(ticket('x')));
        QCOMPARE(wrong->closes, 1);
        Link* watch = rig.socket();
        watch->binary(attach(ticket('a')));
        QCOMPARE(watch->lastBinary, QByteArray::fromHex("0100"));
        QCOMPARE(rig.server.bindingCount(), 1);
        watch->binary(RemoteTxWatchdog::channelKeepalive(1, 9));
        QCOMPARE(rig.delivered, 1);
        watch->binary(QByteArray(13, '\0'));
        QCOMPARE(watch->closes, 1);
        Link* reused = rig.socket();
        reused->binary(attach(ticket('a')));
        QCOMPARE(reused->closes, 1);
        QCOMPARE(rig.server.bindingCount(), 0);
    }

    void expiryGenerationAndPermission()
    {
        Rig rig;
        QVERIFY(rig.issue());
        rig.time += TxWatchServer::kTicketLifetimeMs;
        Link* late = rig.socket();
        late->binary(attach(ticket('a')));
        QCOMPARE(late->closes, 1);
        QVERIFY(rig.issue('b'));
        ++rig.generation;
        Link* stale = rig.socket();
        stale->binary(attach(ticket('b')));
        QCOMPARE(stale->closes, 1);
        QVERIFY(!rig.issue('c')); // consumed, but still within the issue interval
        rig.time += TxWatchServer::kIssueIntervalMs;
        QVERIFY(rig.issue('c'));
        Link* watch = rig.socket();
        watch->binary(attach(ticket('c')));
        QCOMPARE(watch->lastBinary, QByteArray::fromHex("0100"));
        rig.permitted = false;
        watch->binary(RemoteTxWatchdog::channelKeepalive(1, 9));
        QCOMPARE(rig.delivered, 0);
        QCOMPARE(watch->closes, 1);
    }

    void liveBindingSurvivesTicketExpiryOnlyAfterAttach()
    {
        Rig rig;
        QVERIFY(!rig.server.hasLiveBinding(&rig.primary, rig.generation));
        QVERIFY(rig.issue());
        QVERIFY(rig.server.hasLiveBinding(&rig.primary, rig.generation));
        QVERIFY(!rig.server.hasLiveBinding(&rig.primary, rig.generation + 1));
        Link* watch = rig.socket();
        watch->binary(attach(ticket('a')));
        QCOMPARE(watch->lastBinary, QByteArray::fromHex("0100"));
        rig.time += TxWatchServer::kTicketLifetimeMs;
        QVERIFY(rig.server.hasLiveBinding(&rig.primary, rig.generation));
        watch->closeLink(QStringLiteral("closed synchronously"));
        QVERIFY(!rig.server.hasLiveBinding(&rig.primary, rig.generation));
        QCOMPARE(rig.server.bindingCount(), 0);

        QVERIFY(rig.issue('b'));
        rig.time += TxWatchServer::kTicketLifetimeMs;
        QVERIFY(!rig.server.hasLiveBinding(&rig.primary, rig.generation));
    }

    void pendingAndRateBounds()
    {
        Rig rig;
        QList<Link*> pending;
        for (int i = 0; i < TxWatchServer::kMaxPendingSockets; ++i) {
            pending.append(rig.socket());
            QVERIFY(pending.last()->open);
        }
        Link* ninth = rig.socket();
        QCOMPARE(ninth->closes, 1);
        QCOMPARE(rig.server.pendingSocketCount(), TxWatchServer::kMaxPendingSockets);
        rig.time += TxWatchServer::kAttachDeadlineMs;
        pending.first()->binary(attach(ticket('x')));
        QCOMPARE(pending.first()->closes, 1);
        for (Link* link : pending) {
            link->closeLink({});
        }
        QVERIFY(rig.issue());
        Link* watch = rig.socket();
        watch->binary(attach(ticket('a')));
        for (int i = 0; i < TxWatchServer::kBurstFrames; ++i) {
            watch->binary(RemoteTxWatchdog::channelKeepalive(i + 1, 9));
        }
        QCOMPARE(rig.delivered, TxWatchServer::kBurstFrames);
        watch->binary(RemoteTxWatchdog::channelKeepalive(21, 9));
        QCOMPARE(watch->closes, 1);
    }

    void retireBeforeLateCallback()
    {
        Rig rig;
        QVERIFY(rig.issue());
        Link* watch = rig.socket();
        watch->binary(attach(ticket('a')));
        rig.server.retire(&rig.primary);
        QCOMPARE(watch->closes, 1);
        watch->binary(RemoteTxWatchdog::channelKeepalive(1, 9));
        QCOMPARE(rig.delivered, 0);
        QCOMPARE(rig.server.bindingCount(), 0);
    }

    void fourBindingsAndOneActivePerPrimary()
    {
        Link primaries[5];
        qint64 time = 100000;
        TxWatchServer server(
            [&primaries](SessionTransport* link, quint64 id, const QByteArray& device,
                         quint64 generation) {
                for (int i = 0; i < 5; ++i) {
                    if (link == &primaries[i]) {
                        return primaries[i].open && id == static_cast<quint64>(i + 1)
                            && device == QByteArray::number(i + 1) && generation == 1;
                    }
                }
                return false;
            }, [](const QByteArray&, quint64, quint32) {}, nullptr,
            [&time]() { return time; });
        for (int i = 0; i < 4; ++i) {
            QVERIFY(server.issue(&primaries[i], i + 1, QByteArray::number(i + 1), 1,
                                 ticket(static_cast<char>('a' + i))));
        }
        QVERIFY(!server.issue(&primaries[4], 5, QByteArrayLiteral("5"), 1, ticket('e')));
        for (int i = 0; i < 4; ++i) {
            auto* auxiliary = new Link;
            QVERIFY(server.acceptTransport(auxiliary, QStringLiteral("one-nat")));
            auxiliary->binary(attach(ticket(static_cast<char>('a' + i))));
            QCOMPARE(auxiliary->lastBinary, QByteArray::fromHex("0100"));
            QVERIFY(!server.issue(&primaries[i], i + 1, QByteArray::number(i + 1), 1,
                                  ticket('z')));
        }
        QCOMPARE(server.bindingCount(), 4);
        server.retire(&primaries[0]);
        ++time;
        QVERIFY(server.issue(&primaries[4], 5, QByteArrayLiteral("5"), 1, ticket('e')));
    }

    void synchronousAuxiliaryCallbacksMayDestroyOwner()
    {
        Link primary;
        const auto current = [&primary](SessionTransport* link, quint64 id,
                                        const QByteArray& device, quint64 generation) {
            return link == &primary && id == 7 && device == QByteArrayLiteral("paired")
                && generation == 1;
        };
        TxWatchServer* server = new TxWatchServer(current,
            [](const QByteArray&, quint64, quint32) {});
        QVERIFY(server->issue(&primary, 7, QByteArrayLiteral("paired"), 1, ticket('a')));
        Link* onAck = new Link;
        QVERIFY(server->acceptTransport(onAck, QStringLiteral("one-nat")));
        const QPointer<Link> ackGuard(onAck);
        onAck->onSendBinary = [&server]() { delete server; server = nullptr; };
        onAck->binary(attach(ticket('a')));
        QVERIFY(server == nullptr);
        QVERIFY(ackGuard.isNull());

        server = new TxWatchServer(current,
            [](const QByteArray&, quint64, quint32) {});
        QVERIFY(server->issue(&primary, 7, QByteArrayLiteral("paired"), 1, ticket('b')));
        Link* onRefusal = new Link;
        QVERIFY(server->acceptTransport(onRefusal, QStringLiteral("one-nat")));
        const QPointer<Link> refusalGuard(onRefusal);
        onRefusal->onClose = [&server]() { delete server; server = nullptr; };
        onRefusal->binary(attach(ticket('x')));
        QVERIFY(server == nullptr);
        QVERIFY(refusalGuard.isNull());
    }

    void eligibilityDeliveryAndRetirementCallbacksMayDestroyOwner()
    {
        Link primary;
        TxWatchServer* server = nullptr;
        server = new TxWatchServer(
            [&server](SessionTransport*, quint64, const QByteArray&, quint64) {
                delete server;
                server = nullptr;
                return true;
            }, [](const QByteArray&, quint64, quint32) {});
        QVERIFY(!server->issue(&primary, 7, QByteArrayLiteral("paired"), 1, ticket('a')));
        QVERIFY(server == nullptr);

        server = new TxWatchServer(
            [&primary](SessionTransport* link, quint64 id, const QByteArray& device,
                       quint64 generation) {
                return link == &primary && id == 7 && device == QByteArrayLiteral("paired")
                    && generation == 1;
            }, [&server](const QByteArray&, quint64, quint32) {
                delete server;
                server = nullptr;
            });
        QVERIFY(server->issue(&primary, 7, QByteArrayLiteral("paired"), 1, ticket('b')));
        Link* onDelivery = new Link;
        QVERIFY(server->acceptTransport(onDelivery, QStringLiteral("one-nat")));
        const QPointer<Link> deliveryGuard(onDelivery);
        onDelivery->binary(attach(ticket('b')));
        onDelivery->binary(RemoteTxWatchdog::channelKeepalive(1, 9));
        QVERIFY(server == nullptr);
        QVERIFY(deliveryGuard.isNull());

        Link secondPrimary;
        server = new TxWatchServer(
            [&primary, &secondPrimary](SessionTransport* link, quint64, const QByteArray&,
                                       quint64) {
                return link == &primary || link == &secondPrimary;
            }, [](const QByteArray&, quint64, quint32) {});
        QVERIFY(server->issue(&primary, 7, QByteArrayLiteral("paired"), 1, ticket('c')));
        QVERIFY(server->issue(&secondPrimary, 8, QByteArrayLiteral("other"), 1, ticket('d')));
        Link* onRetirement = new Link;
        QVERIFY(server->acceptTransport(onRetirement, QStringLiteral("one-nat")));
        const QPointer<Link> retirementGuard(onRetirement);
        onRetirement->binary(attach(ticket('c')));
        onRetirement->onClose = [&server]() { delete server; server = nullptr; };
        server->retireAll();
        QVERIFY(server == nullptr);
        QVERIFY(retirementGuard.isNull());
    }

    void realStationDirectRouteDoesNotAdoptAnAuxiliary()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt TLS backend unavailable");
        }
        QTemporaryDir settingsDir;
        QTemporaryDir securityDir;
        QVERIFY(settingsDir.isValid());
        QVERIFY(securityDir.isValid());
        AppSettings settings(settingsDir.filePath(QStringLiteral("station.settings")));
        RadioModel model;
        StationServer station(&model, settings,
            NereusSDR::Test::seedUpgradedCoreToken(securityDir.path()));
        QVERIFY2(station.listen(QHostAddress::LocalHost, 0), qPrintable(station.lastError()));

        QWebSocket auxiliary;
        QSslConfiguration tls = auxiliary.sslConfiguration();
        tls.setPeerVerifyMode(QSslSocket::VerifyNone);
        auxiliary.setSslConfiguration(tls);
        const QString authority = QStringLiteral("wss://127.0.0.1:%1").arg(station.serverPort());
        auxiliary.open(QUrl(authority + QLatin1String(TxWatchServer::kPath)));
        QVERIFY(QTest::qWaitFor([&]() { return auxiliary.state() == QAbstractSocket::ConnectedState; }, 5000));
        QCOMPARE(station.peerCount(), 0);
        auxiliary.sendBinaryMessage(attach(ticket('x')));
        QVERIFY(QTest::qWaitFor([&]() { return auxiliary.state() == QAbstractSocket::UnconnectedState; }, 5000));
        QCOMPARE(station.peerCount(), 0);

        QWebSocket query;
        query.setSslConfiguration(tls);
        QSignalSpy queryClosed(&query, &QWebSocket::disconnected);
        query.open(QUrl(authority + QLatin1String(TxWatchServer::kPath) + QStringLiteral("?ticket=x")));
        QVERIFY(QTest::qWaitFor([&]() { return !queryClosed.isEmpty(); }, 5000));
        QCOMPARE(station.peerCount(), 0);
        station.close();
    }

    void realPairedPrimaryIssuesAndAttachesIndependentWssWatch()
    {
        if (!QSslSocket::supportsSsl()) {
            QSKIP("Qt TLS backend unavailable");
        }
        QTemporaryDir settingsDir;
        QTemporaryDir securityDir;
        QTemporaryDir deviceDir;
        QVERIFY(settingsDir.isValid());
        QVERIFY(securityDir.isValid());
        QVERIFY(deviceDir.isValid());
        AppSettings settings(settingsDir.filePath(QStringLiteral("station.settings")));
        RadioModel model;
        model.setBoardForTest(HPSDRHW::HermesLite);
        model.addSlice(QStringLiteral("pan-0"));
        StationServer station(&model, settings,
            NereusSDR::Test::withSharedTlsIdentity(
                NereusSDR::Test::seedCoreIdentity(securityDir.path())));
        station.setRemoteTransmitAllowed(true);
        StationIdentity device = StationIdentity::loadOrCreate(deviceDir.path());
        const QByteArray deviceId = device.fingerprint();
        PairedDevice record;
        record.id = deviceId;
        record.publicKeySpki = device.publicKeySpki();
        record.name = QStringLiteral("Watch test device");
        record.kind = QStringLiteral("computer");
        QVERIFY(station.deviceStore()->add(record));
        QVERIFY2(station.listen(QHostAddress::LocalHost, 0), qPrintable(station.lastError()));

        const QString authority = QStringLiteral("wss://127.0.0.1:%1").arg(station.serverPort());
        QWebSocket primary;
        QSslConfiguration tls = primary.sslConfiguration();
        tls.setPeerVerifyMode(QSslSocket::VerifyNone);
        primary.setSslConfiguration(tls);
        QList<QByteArray> messages;
        connect(&primary, &QWebSocket::textMessageReceived, &primary,
                [&messages](const QString& message) { messages.append(message.toUtf8()); });
        const auto findType = [&messages](const QString& type) {
            for (const QByteArray& wire : messages) {
                const QJsonObject object = QJsonDocument::fromJson(wire).object();
                if (object.value(QStringLiteral("type")).toString() == type) {
                    return object;
                }
            }
            return QJsonObject{};
        };
        const auto resultFor = [&messages](quint32 id) {
            for (const QByteArray& wire : messages) {
                const QJsonObject object = QJsonDocument::fromJson(wire).object();
                if (object.value(QStringLiteral("type")).toString()
                        == QLatin1String("command.result")
                    && object.value(QStringLiteral("id")).toInteger() == id) {
                    return object;
                }
            }
            return QJsonObject{};
        };
        primary.open(QUrl(authority + QStringLiteral("/")));
        QVERIFY(QTest::qWaitFor([&]() { return !findType(QStringLiteral("hello")).isEmpty(); }, 5000));
        const QJsonObject hello = findType(QStringLiteral("hello"));
        const QByteArray challenge = StationIdentity::fromBase64Url(
            hello.value(QStringLiteral("challenge")).toString());
        QCOMPARE(challenge.size(), DeviceAuthenticator::kChallengeBytes);
        QString fingerprint = station.certificateFingerprint();
        fingerprint.remove(QLatin1Char(':'));
        const QByteArray certHash = QByteArray::fromHex(fingerprint.toLatin1());
        const QByteArray stationSpki = station.stationIdentity().publicKeySpki();
        const SessionDeviceBlock proof{
            StationIdentity::toBase64Url(deviceId),
            StationIdentity::toBase64Url(device.publicKeySpki()),
            record.name, record.kind,
            StationIdentity::toBase64Url(device.sign(DeviceAuthenticator::transcript(
                challenge, certHash, stationSpki, device.publicKeySpki())))};
        const auto sendPrimary = [&primary](const SessionMessage& message) {
            primary.sendTextMessage(QString::fromUtf8(SessionMessages::encode(message)));
        };
        sendPrimary(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 0,
            QStringLiteral("Watch test device"), {kSessionProtocolMajor},
            {{"deviceAuth", 1}, {"remoteTx", 1}, {"txWatchPath", 1}}));
        sendPrimary(SessionMessages::authRequest(QString(), proof));
        QVERIFY(QTest::qWaitFor([&]() {
            return !findType(QStringLiteral("auth.result")).isEmpty()
                && !findType(QStringLiteral("snapshot.complete")).isEmpty();
        }, 5000));
        QVERIFY2(findType(QStringLiteral("auth.result")).value(QStringLiteral("accepted")).toBool(),
                 qPrintable(QString::fromUtf8(QJsonDocument(findType(QStringLiteral("auth.result"))).toJson())));
        QCOMPARE(station.peerCount(), 1);
        QCOMPARE(station.authenticatedSessionCount(), 1);
        bool advertised = false;
        for (const QByteArray& wire : messages) {
            const QJsonObject object = QJsonDocument::fromJson(wire).object();
            if (object.value(QStringLiteral("type")).toString() != QLatin1String("capabilities")) {
                continue;
            }
            for (const QJsonValue& entry : object.value(QStringLiteral("properties")).toArray()) {
                const QJsonObject value = entry.toObject();
                if (value.value(QStringLiteral("name")).toString()
                        == QLatin1String("txWatchPathVersion")
                    && value.value(QStringLiteral("value")).toInteger() == 1) {
                    advertised = true;
                }
            }
        }
        QVERIFY(advertised);

        // A direct primary cannot obtain relay watch construction, even when
        // it sends a syntactically valid relay command.
        sendPrimary(SessionMessages::commandInvoke(
            QByteArrayLiteral("tx.watchRelay"), 700,
            {{0, "offer", MirrorWireKind::Utf8, QStringLiteral("offer")}}));
        QVERIFY(QTest::qWaitFor([&]() { return !resultFor(700).isEmpty(); }, 5000));
        QCOMPARE(resultFor(700).value(QStringLiteral("reason")).toString(),
                 QStringLiteral("The Core cannot relay the transmit watch connection for this device."));

        sendPrimary(SessionMessages::commandInvoke(QByteArrayLiteral("tx.watchTicket"), 701, {}));
        QVERIFY(QTest::qWaitFor([&]() { return !resultFor(701).isEmpty(); }, 5000));
        const QJsonObject result = resultFor(701);
        QVERIFY2(result.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(QString::fromUtf8(QJsonDocument(result).toJson())));
        QString ticketText;
        QString path;
        qint64 expiresInMs = 0;
        for (const QJsonValue& entry : result.value(QStringLiteral("values")).toArray()) {
            const QJsonObject value = entry.toObject();
            if (value.value(QStringLiteral("name")).toString() == QLatin1String("ticket")) {
                ticketText = value.value(QStringLiteral("value")).toString();
            } else if (value.value(QStringLiteral("name")).toString() == QLatin1String("path")) {
                path = value.value(QStringLiteral("value")).toString();
            } else if (value.value(QStringLiteral("name")).toString() == QLatin1String("expiresInMs")) {
                expiresInMs = value.value(QStringLiteral("value")).toInteger();
            }
        }
        QCOMPARE(path, QString::fromLatin1(TxWatchServer::kPath));
        QCOMPARE(expiresInMs, qint64(TxWatchServer::kTicketLifetimeMs));
        bool canonical = false;
        const QByteArray rawTicket = StationIdentity::fromBase64Url(ticketText, &canonical);
        QVERIFY(canonical);
        QCOMPARE(rawTicket.size(), TxWatchServer::kTicketBytes);

        QWebSocket auxiliary;
        auxiliary.setSslConfiguration(tls);
        QSignalSpy ack(&auxiliary, &QWebSocket::binaryMessageReceived);
        auxiliary.open(QUrl(authority + path));
        QVERIFY(QTest::qWaitFor([&]() { return auxiliary.state() == QAbstractSocket::ConnectedState; }, 5000));
        auxiliary.sendBinaryMessage(attach(rawTicket));
        QVERIFY(QTest::qWaitFor([&]() { return !ack.isEmpty(); }, 5000));
        QCOMPARE(ack.first().first().toByteArray(), QByteArray::fromHex("0100"));
        QCOMPARE(station.peerCount(), 1);

        RemoteTxWatchdog* watchdog = station.txWatchdog();
        QVERIFY(watchdog != nullptr);
        QSignalSpy heard(watchdog, &RemoteTxWatchdog::keepaliveHeard);
        watchdog->setKeyed(deviceId, true, 9); // logical watch only; no radio key action
        auxiliary.sendBinaryMessage(RemoteTxWatchdog::channelKeepalive(1, 9));
        QVERIFY(QTest::qWaitFor([&]() { return heard.size() == 1; }, 5000));
        QCOMPARE(heard.first().first().toByteArray(), deviceId);

        QWebSocket replay;
        replay.setSslConfiguration(tls);
        QSignalSpy replayClosed(&replay, &QWebSocket::disconnected);
        replay.open(QUrl(authority + path));
        QVERIFY(QTest::qWaitFor([&]() { return replay.state() == QAbstractSocket::ConnectedState; }, 5000));
        replay.sendBinaryMessage(attach(rawTicket));
        QVERIFY(QTest::qWaitFor([&]() { return !replayClosed.isEmpty(); }, 5000));
        QCOMPARE(station.peerCount(), 1);
        QCOMPARE(station.authenticatedSessionCount(), 1);
        QVERIFY(primary.state() == QAbstractSocket::ConnectedState);
        sendPrimary(SessionMessages::commandInvoke(QByteArrayLiteral("tx.watchTicket"), 702, {}));
        QVERIFY(QTest::qWaitFor([&]() { return !resultFor(702).isEmpty(); }, 5000));
        QVERIFY(!resultFor(702).value(QStringLiteral("accepted")).toBool());
        auxiliary.sendBinaryMessage(RemoteTxWatchdog::channelKeepalive(2, 9));
        QVERIFY(QTest::qWaitFor([&]() { return heard.size() == 2; }, 5000));
        watchdog->setKeyed(deviceId, false);
        station.close();
    }
};

QTEST_MAIN(TestTxWatchServer)
#include "tst_tx_watch_server.moc"
