// =================================================================
// tests/tst_station_handshake_deadline.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original test infrastructure.
//
// R-R3-16 / R-R3-17: a reconnect that never finishes setting up gives up
// and retries.
//
// The 2026-09-23 incident: after a heartbeat timeout at 08:57:32 the GUI
// scheduled reconnect attempt 1, the redial connected, and then nothing at
// all happened on the GUI side for 8.7 minutes, until Core closed the link
// at 09:06:13. The GUI log shows no heartbeat timeout in that window and
// none of the per-session "No way to apply station value" lines the
// capability exchange produces, so the GUI never received a single frame
// on the new link. StationClient starts its heartbeat on the station's
// FIRST frame (its Hello), deliberately, so a slow dial is not reported as
// a dead station; nothing else bounded the wait before that frame. With
// Core's event loop blocked behind the WDSP channel lock, the TLS and
// WebSocket upgrade and Core's Hello all waited on that loop.
//
// clientWaitingForTheFirstFrameIsBoundedOnlyByTheDeadline reproduces that
// step over the in-process link and pins it: the heartbeat never arms, so
// only the handshake deadline ends the wait. The wss slots then cover the
// three acceptance cases over real sockets: a Core that accepts and never
// answers, a Core blocked past the deadline that then recovers, and a
// completed handshake that the deadline leaves alone. The Core-side slots
// cover a GUI that never sends its hello, and the media context Core
// created for a GUI that gives up before its own snapshot arrived.
//
// Every deadline is shortened through the injectable value; nothing here
// waits the production 30 s.
//
// =================================================================

#include <QtTest/QtTest>

#include <QByteArray>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSignalSpy>
#include <QSslSocket>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QUrl>

#include <memory>

#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/security/CertificateStore.h"
#include "core/session/RemoteStationOptions.h"
#include "core/session/SessionMessages.h"
#include "core/session/SessionTransport.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "gui/RemoteConnectionController.h"
#include "models/RadioModel.h"

#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

constexpr auto kSchemaVersion = "6";

QString tlsSkipMessage()
{
    return QStringLiteral("No Qt TLS backend is available: ")
           + CertificateStore::tlsBackendDiagnostic();
}

std::unique_ptr<RadioModel> makeStationRadioModel()
{
    auto model = std::make_unique<RadioModel>();
    model->setBoardForTest(HPSDRHW::HermesLite);
    RadioInfo info;
    info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
    info.name = QStringLiteral("Bench HL2");
    info.boardType = HPSDRHW::HermesLite;
    model->setLastRadioInfoForTest(info);
    model->setConnectionStateForTest(ConnectionState::Connected);
    model->addSlice(QStringLiteral("pan-0"));
    return model;
}

// ── Log capture with the message type ────────────────────────────────────

struct CapturedLine {
    QtMsgType type;
    QString text;
};
QList<CapturedLine>* g_lines = nullptr;

void capture(QtMsgType type, const QMessageLogContext&, const QString& text)
{
    if (g_lines != nullptr) {
        g_lines->append({type, text});
    }
}

class LogCapture {
public:
    explicit LogCapture(QList<CapturedLine>* sink)
    {
        g_lines = sink;
        m_previous = qInstallMessageHandler(&capture);
    }
    ~LogCapture()
    {
        qInstallMessageHandler(m_previous);
        g_lines = nullptr;
    }
    LogCapture(const LogCapture&) = delete;
    LogCapture& operator=(const LogCapture&) = delete;

private:
    QtMessageHandler m_previous = nullptr;
};

// ── A Core that accepts TCP and never says anything ─────────────────────
//
// What a Core whose event loop is blocked looks like from the GUI: the
// kernel completes the TCP accept, and then the TLS handshake, the
// WebSocket upgrade and Core's Hello all wait on a loop that is not
// running.
class SilentCore : public QObject {
public:
    bool listen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return m_server.serverPort(); }

    SilentCore()
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* socket = m_server.nextPendingConnection()) {
                ++m_accepted;
                m_open.append(socket);
                connect(socket, &QTcpSocket::disconnected, this, [this, socket] {
                    ++m_closedByClient;
                    m_open.removeAll(socket);
                    socket->deleteLater();
                });
            }
        });
    }

    int accepted() const { return m_accepted; }
    int closedByClient() const { return m_closedByClient; }

private:
    QTcpServer m_server;
    QList<QPointer<QTcpSocket>> m_open;
    int m_accepted = 0;
    int m_closedByClient = 0;
};

// ── A Core that is blocked for a while and then responsive ──────────────
//
// A TCP relay in front of a real StationServer. While stalled it accepts
// and holds each connection without forwarding a byte either way (Core's
// loop is blocked); once released, new connections pass straight through.
class StallingRelay : public QObject {
public:
    explicit StallingRelay(quint16 upstreamPort) : m_upstreamPort(upstreamPort)
    {
        connect(&m_server, &QTcpServer::newConnection, this, [this] {
            while (QTcpSocket* client = m_server.nextPendingConnection()) {
                client->setParent(this);
                connect(client, &QTcpSocket::disconnected, client, &QObject::deleteLater);
                if (m_stalled) {
                    ++m_heldConnections;
                    continue;
                }
                bridge(client);
            }
        });
    }

    bool listen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return m_server.serverPort(); }
    void release() { m_stalled = false; }
    int heldConnections() const { return m_heldConnections; }

private:
    void bridge(QTcpSocket* client)
    {
        auto* upstream = new QTcpSocket(this);
        const QPointer<QTcpSocket> guardedClient(client);
        const QPointer<QTcpSocket> guardedUpstream(upstream);
        connect(upstream, &QTcpSocket::connected, this, [guardedClient, guardedUpstream] {
            if (!guardedClient || !guardedUpstream) { return; }
            guardedUpstream->write(guardedClient->readAll());
            connect(guardedClient, &QTcpSocket::readyRead, guardedUpstream,
                    [guardedClient, guardedUpstream] {
                        if (guardedClient && guardedUpstream) {
                            guardedUpstream->write(guardedClient->readAll());
                        }
                    });
        });
        connect(upstream, &QTcpSocket::readyRead, client, [guardedClient, guardedUpstream] {
            if (guardedClient && guardedUpstream) {
                guardedClient->write(guardedUpstream->readAll());
            }
        });
        connect(client, &QTcpSocket::disconnected, upstream, [guardedUpstream] {
            if (guardedUpstream) { guardedUpstream->disconnectFromHost(); }
        });
        connect(upstream, &QTcpSocket::disconnected, client, [guardedClient] {
            if (guardedClient) { guardedClient->disconnectFromHost(); }
        });
        connect(upstream, &QTcpSocket::disconnected, upstream, &QObject::deleteLater);
        upstream->connectToHost(QHostAddress::LocalHost, m_upstreamPort);
    }

    QTcpServer m_server;
    quint16 m_upstreamPort = 0;
    bool m_stalled = true;
    int m_heldConnections = 0;
};

// ── An in-process link whose GUI end stops delivering after the Hello ────
//
// Core completes its side of the handshake and creates the media context,
// but the GUI never sees the capability exchange or the snapshot: the shape
// of the incident's Core-side "consumed=0" audio context.
class GatedTransport final : public SessionTransport {
public:
    explicit GatedTransport(const QString& description, QObject* parent = nullptr)
        : SessionTransport(parent), m_description(description)
    {
    }

    void linkTo(GatedTransport* peer)
    {
        m_peer = peer;
        if (peer != nullptr) { peer->m_peer = this; }
    }

    /// Deliver the peer's Hello and hold everything after it.
    void setHoldAfterHello(bool hold) { m_holdAfterHello = hold; }
    int heldFrames() const { return m_heldFrames; }

    void sendText(const QByteArray& wire) override
    {
        if (!m_open || m_peer.isNull()) { return; }
        const QPointer<GatedTransport> peer(m_peer);
        QMetaObject::invokeMethod(peer, [peer, wire] {
            if (!peer.isNull()) { peer->deliver(wire); }
        }, Qt::QueuedConnection);
    }

    void ping() override
    {
        if (!m_open || m_peer.isNull()) { return; }
        const QPointer<GatedTransport> self(this);
        QMetaObject::invokeMethod(this, [self] {
            if (!self.isNull() && self->m_open) { emit self->pongReceived(); }
        }, Qt::QueuedConnection);
    }

    void closeLink(const QString& reason) override
    {
        if (!m_open) { return; }
        m_open = false;
        emit closed();
        if (!m_peer.isNull()) {
            const QPointer<GatedTransport> peer(m_peer);
            QMetaObject::invokeMethod(peer, [peer, reason] {
                if (!peer.isNull()) { peer->closeLink(reason); }
            }, Qt::QueuedConnection);
        }
    }

    bool isOpen() const override { return m_open; }
    QString peerDescription() const override { return m_description; }

private:
    void deliver(const QByteArray& wire)
    {
        if (!m_open) { return; }
        const QByteArray kind = QJsonDocument::fromJson(wire)
                                    .object().value(QStringLiteral("type")).toString().toUtf8();
        if (m_holdAfterHello && kind != QByteArrayLiteral("hello")) {
            ++m_heldFrames;
            return;
        }
        emit textReceived(wire);
    }

    QString m_description;
    QPointer<GatedTransport> m_peer;
    bool m_open = true;
    bool m_holdAfterHello = false;
    int m_heldFrames = 0;
};

} // namespace

class TstStationHandshakeDeadline : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();

    void productionDeadlineIsSharedAndThirtySeconds();

    // GUI side.
    void clientWaitingForTheFirstFrameIsBoundedOnlyByTheDeadline();
    void coreThatAcceptsAndNeverAnswersTimesOutAndRetriesWithBackoff();
    void persistentStatusShowsTheReasonAndCancelStopsTheWait();
    void coreBlockedPastTheDeadlineThenResponsiveSucceedsNextAttempt();
    void handshakeThatCompletesInTimeIsUnaffected();

    // Core side.
    void coreDetachesAGuiThatNeverSendsItsHelloWithOneLogLine();
    void coreRetiresTheMediaContextWhenTheGuiGivesUp();

private:
    QTemporaryDir m_securityDir;
};

void TstStationHandshakeDeadline::initTestCase()
{
    QVERIFY(m_securityDir.isValid());
    const QString profile = QStringLiteral("station-handshake-deadline-%1")
                                .arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
    AppSettings::instance().clear();
    AppSettings::instance().setValue(QStringLiteral("SettingsSchemaVersion"),
                                     QString::fromLatin1(kSchemaVersion));
}

void TstStationHandshakeDeadline::cleanupTestCase()
{
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
}

void TstStationHandshakeDeadline::productionDeadlineIsSharedAndThirtySeconds()
{
    QCOMPARE(kStationHandshakeDeadlineMs, 30000);
    QCOMPARE(StationServer::kDefaultAuthDeadlineMs, kStationHandshakeDeadlineMs);

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    QCOMPARE(client.handshakeDeadlineMs(), kStationHandshakeDeadlineMs);
    QCOMPARE(StationClient::handshakeDeadlineReason(),
             QStringLiteral("The Core did not finish connecting."));
}

void TstStationHandshakeDeadline::clientWaitingForTheFirstFrameIsBoundedOnlyByTheDeadline()
{
    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    client.setHeartbeatIntervalMs(40);
    client.setHandshakeDeadlineMs(600);
    QSignalSpy ended(&client, &StationClient::sessionEnded);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);

    // Core end that is linked (so it would answer pings) but never greets.
    auto* coreEnd = new LoopbackTransport(QStringLiteral("blocked-core"), this);
    auto* guiEnd = new LoopbackTransport(QStringLiteral("gui"), this);
    coreEnd->linkTo(guiEnd);
    client.startSession(guiEnd, QStringLiteral("token"));

    // THE STUCK STEP. Seven heartbeat intervals in, far past the two missed
    // pongs that declare a station dead, the GUI has not sent one ping:
    // the heartbeat arms on the station's first frame, and none came.
    QTest::qWait(280);
    QCOMPARE(coreEnd->pingsSeen(), 0);
    QVERIFY(client.isConnectionActive());
    QVERIFY(!client.isHandshakeComplete());
    QCOMPARE(ended.count(), 0);

    // Only the deadline ends it.
    QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 5000);
    QCOMPARE(ended.first().first().toString(), StationClient::handshakeDeadlineReason());
    QCOMPARE(client.lastError(), StationClient::handshakeDeadlineReason());
    QCOMPARE(completed.count(), 0);
    QVERIFY(!client.isConnectionActive());
    QTRY_COMPARE(coreEnd->closeReason(), StationClient::handshakeDeadlineReason());
}

void TstStationHandshakeDeadline::coreThatAcceptsAndNeverAnswersTimesOutAndRetriesWithBackoff()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }
    SilentCore core;
    QVERIFY(core.listen());

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    client.setHandshakeDeadlineMs(200);
    client.setReconnectBackoffUnitMs(25);
    QSignalSpy ended(&client, &StationClient::sessionEnded);
    QSignalSpy scheduled(&client, &StationClient::reconnectScheduled);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);

    const QString anyFingerprint =
        QStringLiteral("00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:"
                       "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF");
    client.connectToStation(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(core.port())),
                            QStringLiteral("token"), anyFingerprint);

    // Two expiries: the first retry is scheduled at the first backoff
    // step, and the second attempt, stuck the same way, at the next one.
    QTRY_VERIFY_WITH_TIMEOUT(scheduled.count() >= 2, 10000);
    QCOMPARE(scheduled.at(0).at(0).toInt(), 1);
    QCOMPARE(scheduled.at(0).at(1).toInt(), 25);
    QCOMPARE(scheduled.at(1).at(0).toInt(), 2);
    QCOMPARE(scheduled.at(1).at(1).toInt(), 50);
    QVERIFY(ended.count() >= 2);
    for (const QList<QVariant>& args : ended) {
        QCOMPARE(args.first().toString(), StationClient::handshakeDeadlineReason());
    }
    QCOMPARE(completed.count(), 0);
    QVERIFY(core.accepted() >= 2);
    // The GUI closed each link it gave up on rather than leaving it open.
    QTRY_VERIFY_WITH_TIMEOUT(core.closedByClient() >= 2, 5000);

    client.disconnectFromStation(QStringLiteral("operator disconnect"));
    QVERIFY(!client.isConnectionActive());
}

void TstStationHandshakeDeadline::persistentStatusShowsTheReasonAndCancelStopsTheWait()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }
    SilentCore core;
    QVERIFY(core.listen());

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    client.setHandshakeDeadlineMs(200);
    // Long enough that the retry is still pending while the status is read.
    client.setReconnectBackoffUnitMs(60000);

    RemoteStationOptions options;
    options.url = QStringLiteral("wss://127.0.0.1:%1").arg(core.port());
    options.token = QStringLiteral("token");
    options.fingerprint =
        QStringLiteral("00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF:"
                       "00:11:22:33:44:55:66:77:88:99:AA:BB:CC:DD:EE:FF");
    RemoteConnectionController controller(&client, &clientModel, options);
    QSignalSpy ended(&client, &StationClient::sessionEnded);
    QSignalSpy scheduled(&client, &StationClient::reconnectScheduled);

    controller.connectToStation();
    QCOMPARE(controller.state(), ConnectionState::Connecting);

    QTRY_COMPARE_WITH_TIMEOUT(controller.state(), ConnectionState::LinkLost, 5000);
    QVERIFY2(controller.detailText().contains(
                 QStringLiteral("Last failure: The Core did not finish connecting.")),
             qPrintable(controller.detailText()));
    QCOMPARE(scheduled.count(), 1);

    // Cancel during the retry wait stops it.
    controller.disconnectFromStation();
    QCOMPARE(controller.state(), ConnectionState::Disconnected);
    QVERIFY(!client.isReconnectPending());
    QVERIFY(!client.isConnectionActive());

    // Cancel during the handshake wait stops the deadline too: no expiry,
    // no retry, however long the stalled Core stays silent.
    QVERIFY(controller.canConnect());
    controller.connectToStation();
    QCOMPARE(controller.state(), ConnectionState::Connecting);
    QTRY_VERIFY_WITH_TIMEOUT(core.accepted() >= 2, 5000);
    const int endedBeforeCancel = ended.count();
    controller.disconnectFromStation();
    QCOMPARE(ended.count(), endedBeforeCancel + 1);
    QCOMPARE(ended.last().first().toString(), QStringLiteral("operator disconnect"));
    QTest::qWait(600);  // three deadlines
    QCOMPARE(ended.count(), endedBeforeCancel + 1);
    QCOMPARE(scheduled.count(), 1);
    QCOMPARE(controller.state(), ConnectionState::Disconnected);
    QVERIFY(!client.isConnectionActive());
}

void TstStationHandshakeDeadline::coreBlockedPastTheDeadlineThenResponsiveSucceedsNextAttempt()
{
    if (!QSslSocket::supportsSsl()) {
        QSKIP(qPrintable(tlsSkipMessage()));
    }
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    stationSettings.setValue(QStringLiteral("SettingsSchemaVersion"),
                             QString::fromLatin1(kSchemaVersion));
    auto stationModel = makeStationRadioModel();
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    QVERIFY2(server.listen(QHostAddress::LocalHost, 0), qPrintable(server.lastError()));

    StallingRelay relay(server.serverPort());
    QVERIFY(relay.listen());

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    client.setHandshakeDeadlineMs(1500);
    client.setReconnectBackoffUnitMs(50);
    QSignalSpy ended(&client, &StationClient::sessionEnded);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    // Core's loop comes back once the GUI has given up on the first link.
    connect(&client, &StationClient::sessionEnded, &relay, [&relay] { relay.release(); });

    client.connectToStation(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(relay.port())),
                            server.token(), server.certificateFingerprint());

    QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 10000);
    QCOMPARE(ended.first().first().toString(), StationClient::handshakeDeadlineReason());
    QCOMPARE(relay.heldConnections(), 1);

    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 15000);
    QVERIFY(client.isHandshakeComplete());
    QVERIFY(clientModel.isConnected());
    QVERIFY(server.hasAuthenticatedSession());
    QCOMPARE(ended.count(), 1);

    client.disconnectFromStation(QStringLiteral("operator disconnect"));
    server.close();
}

void TstStationHandshakeDeadline::handshakeThatCompletesInTimeIsUnaffected()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    stationSettings.setValue(QStringLiteral("SettingsSchemaVersion"),
                             QString::fromLatin1(kSchemaVersion));
    auto stationModel = makeStationRadioModel();
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setAuthDeadlineMs(300);

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    client.setHandshakeDeadlineMs(300);
    QSignalSpy ended(&client, &StationClient::sessionEnded);
    QSignalSpy completed(&client, &StationClient::handshakeComplete);
    QSignalSpy dropped(&server, &StationServer::peerDisconnected);

    auto* coreEnd = new LoopbackTransport(QStringLiteral("core"), this);
    auto* guiEnd = new LoopbackTransport(QStringLiteral("gui"), this);
    coreEnd->linkTo(guiEnd);
    server.acceptTransport(coreEnd);
    client.startSession(guiEnd, server.token());

    QTRY_COMPARE_WITH_TIMEOUT(completed.count(), 1, 5000);
    QTest::qWait(900);  // three deadlines past completion, on both ends
    QCOMPARE(ended.count(), 0);
    QCOMPARE(dropped.count(), 0);
    QVERIFY(client.isHandshakeComplete());
    QVERIFY(server.hasAuthenticatedSession());
    QVERIFY(client.lastError().isEmpty());
}

void TstStationHandshakeDeadline::coreDetachesAGuiThatNeverSendsItsHelloWithOneLogLine()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    stationSettings.setValue(QStringLiteral("SettingsSchemaVersion"),
                             QString::fromLatin1(kSchemaVersion));
    auto stationModel = makeStationRadioModel();
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setAuthDeadlineMs(100);
    QSignalSpy dropped(&server, &StationServer::peerDisconnected);
    QSignalSpy mediaStarted(&server, &StationServer::mediaSessionStarted);

    auto* coreEnd = new LoopbackTransport(QStringLiteral("silent-gui"), this);
    auto* guiEnd = new LoopbackTransport(QStringLiteral("silent-gui-end"), this);
    coreEnd->linkTo(guiEnd);

    QList<CapturedLine> lines;
    {
        LogCapture capture(&lines);
        server.acceptTransport(coreEnd);
        QCOMPARE(server.peerCount(), 1);
        QTRY_COMPARE_WITH_TIMEOUT(server.peerCount(), 0, 3000);
    }

    QCOMPARE(dropped.count(), 1);
    QCOMPARE(dropped.first().at(1).toString(),
             QStringLiteral("This app did not finish connecting to the Core in time."));
    QCOMPARE(mediaStarted.count(), 0);
    QVERIFY(!server.hasAuthenticatedSession());
    // Told why, and retryable: a GUI that does come back gets a new chance.
    // dropPeer() forgets the peer at once, but the loopback delivers the
    // session.end on a queued call, so wait for it rather than read the
    // kinds the moment the peer count reaches zero.
    QTRY_VERIFY2_WITH_TIMEOUT(guiEnd->receivedKinds().contains(QByteArrayLiteral("session.end")),
                              qPrintable(QString::fromUtf8(guiEnd->receivedKinds().join(','))),
                              3000);

    QStringList aboutThePeer;
    for (const CapturedLine& line : std::as_const(lines)) {
        if (line.type != QtDebugMsg && line.text.contains(QStringLiteral("silent-gui"))) {
            aboutThePeer.append(line.text);
        }
    }
    QVERIFY2(aboutThePeer.size() == 1, qPrintable(aboutThePeer.join(QStringLiteral(" | "))));
    QVERIFY2(aboutThePeer.first().contains(
                 QStringLiteral("This app did not finish connecting to the Core in time.")),
             qPrintable(aboutThePeer.first()));
}

void TstStationHandshakeDeadline::coreRetiresTheMediaContextWhenTheGuiGivesUp()
{
    QTemporaryDir settingsDir;
    QVERIFY(settingsDir.isValid());
    AppSettings stationSettings(settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
    stationSettings.setValue(QStringLiteral("SettingsSchemaVersion"),
                             QString::fromLatin1(kSchemaVersion));
    auto stationModel = makeStationRadioModel();
    StationServer server(stationModel.get(), stationSettings, NereusSDR::Test::seedUpgradedCoreToken(m_securityDir.path()));
    server.setMediaEnabled(true);
    QSignalSpy mediaStarted(&server, &StationServer::mediaSessionStarted);
    QSignalSpy mediaEnded(&server, &StationServer::mediaSessionEnded);
    QSignalSpy dropped(&server, &StationServer::peerDisconnected);

    RadioModel clientModel(RadioModel::Role::Remote);
    SettingsProxy proxy;
    StationClient client(&clientModel, &proxy);
    client.setHandshakeDeadlineMs(300);
    QSignalSpy ended(&client, &StationClient::sessionEnded);

    auto* coreEnd = new GatedTransport(QStringLiteral("stalled-gui"), this);
    auto* guiEnd = new GatedTransport(QStringLiteral("stalled-gui-end"), this);
    coreEnd->linkTo(guiEnd);
    guiEnd->setHoldAfterHello(true);
    server.acceptTransport(coreEnd);
    client.startSession(guiEnd, server.token());

    // Core finishes its side and creates the media context...
    QTRY_COMPARE_WITH_TIMEOUT(mediaStarted.count(), 1, 3000);
    QVERIFY(server.hasAuthenticatedSession());
    QCOMPARE(mediaEnded.count(), 0);

    // ...the GUI never sees its snapshot and gives up at the deadline...
    QTRY_COMPARE_WITH_TIMEOUT(ended.count(), 1, 3000);
    QCOMPARE(ended.first().first().toString(), StationClient::handshakeDeadlineReason());
    QVERIFY(guiEnd->heldFrames() > 0);

    // ...and Core retires that media context with the peer.
    QTRY_COMPARE_WITH_TIMEOUT(mediaEnded.count(), 1, 3000);
    QCOMPARE(mediaEnded.first().first(), mediaStarted.first().first());
    QCOMPARE(dropped.count(), 1);
    QCOMPARE(server.peerCount(), 0);
    QVERIFY(!server.hasAuthenticatedSession());
}

QTEST_MAIN(TstStationHandshakeDeadline)
#include "tst_station_handshake_deadline.moc"
