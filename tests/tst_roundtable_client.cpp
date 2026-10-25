// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_roundtable_client.cpp  (NereusSDR)
// =================================================================
//
// Roundtable plan Task 10 (R-RT-6, R-RT-12, R-RT-13, R-RT-14 Core side):
// the Core's RoundtableClient against a fake Roundtable server on
// 127.0.0.1 (a QWebSocketServer in this test), with fake clocks:
//   - the sign-in transcript is signed over the host the client connected
//     to, and the server's check of it passes;
//   - before the first-run answer only /v1/live opens and nothing about
//     the station is sent (D25); after it, presence and then readings once
//     a second, none while keyed and none when the meter has no new
//     reading;
//   - going hidden sends `hidden` last before the close, then reads live;
//   - an archive request while unanswered opens a brief station
//     connection that sends only `hello` and `weblink`;
//   - the retry ladder 1, 2, 5, 10, 30, 30 s, a connection cap's
//     retryAfterMs, and the close codes that stop retrying;
//   - at radio false with AwayChoice "Stop" sends nothing about the
//     station;
//   - the resync-after-gap session fixture's exchange;
//   - the desktop's key and Remote Access's key are one key;
//   - an archive request just before going live never makes two signed-in
//     station connections at once (3.1, the fake server closes the older
//     with 4003);
//   - a hidden client's live reader watches its slice's room once the
//     band list names it (7.8), and unwatches it when the slice leaves;
//   - a write of Roundtable/Callsign or Roundtable/GridSquare while the
//     Core runs Roundtable starts the station connection again with the
//     new values; a callsign the server would refuse stops it with the
//     reason; while unanswered nothing about the station opens (D25).
//
// Keys and nonces are made at run time; nothing leaves this computer.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 10).
//   2026-10-24: Task 11 fix round: the fake server's 3.1 rule and the
//               archive-then-live and slice-watch cases. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-24: Roundtable plan Task 15: the live station-info write case
//               through SpotSourceHost. J.J. Boyd (KG4VCF), AI-assisted
//               via Claude Code.
// =================================================================

#include "core/AppSettings.h"
#include "core/DxClusterClient.h"
#include "core/PotaClient.h"
#include "core/PskReporterClient.h"
#include "core/SpotCollectorClient.h"
#include "core/SpotSourceHost.h"
#include "core/WsjtxClient.h"
#include "models/SpotModel.h"
#include "core/roundtable/RoundtableClient.h"
#include "core/roundtable/RoundtableWire.h"
#include "core/security/StationIdentity.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QWebSocket>
#include <QWebSocketServer>
#include <QtTest/QtTest>

#include <algorithm>
#include <memory>
#include <vector>

using namespace NereusSDR;
namespace Wire = NereusSDR::RoundtableWire;

namespace {

const QString kSuite = QStringLiteral(NEREUS_ROUNDTABLE_CONFORMANCE_DIR);
constexpr qint64 kWallStart = 1800000000000LL;
constexpr qint64 kMonoStart = 5000000;
constexpr int kWaitMs = 5000;
const QString kRoom = QStringLiteral("usb-14212000");

QJsonObject fixtureWire(const QString& name)
{
    QFile file(kSuite + QStringLiteral("/control/") + name + QStringLiteral(".json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("wire")).toObject();
}

QString typeOf(const QJsonObject& message)
{
    return message.value(QStringLiteral("type")).toString();
}

// One connection the fake server accepted.
struct FakeConnection {
    QWebSocket* socket = nullptr;
    QString path;
    QList<QJsonObject> frames;
    QByteArray nonce;
    QByteArray spki;
    bool signatureVerified = false;
    bool signatureVerifiesWithPort = false;
    bool closed = false;
    bool welcomed = false;
    bool allFramesValid = true;

    QStringList types() const
    {
        QStringList out;
        for (const QJsonObject& frame : frames) {
            out.append(typeOf(frame));
        }
        return out;
    }

    void send(const QJsonObject& message) const
    {
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
    }
};

// A Roundtable server as far as the Core sees it: a station connection
// gets a challenge, a valid hello a welcome, and a weblink a link.
class FakeServer {
public:
    FakeServer()
        : m_server(QStringLiteral("fake-roundtable"), QWebSocketServer::NonSecureMode)
    {
        QObject::connect(&m_server, &QWebSocketServer::newConnection, &m_context, [this] { accept(); });
    }

    bool listen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return m_server.serverPort(); }
    QUrl stationUrl() const
    {
        return QUrl(QStringLiteral("ws://127.0.0.1:%1/v1/station").arg(port()));
    }

    bool autoSignIn = true;
    bool autoWebLink = true;
    std::vector<std::unique_ptr<FakeConnection>> connections;
    // The most station connections signed in at once, this one included.
    int maxSignedInStations = 0;

    int count(const QString& path) const
    {
        int n = 0;
        for (const auto& c : connections) {
            if (c->path == path) {
                ++n;
            }
        }
        return n;
    }

    FakeConnection* last(const QString& path) const
    {
        for (auto it = connections.rbegin(); it != connections.rend(); ++it) {
            if ((*it)->path == path) {
                return it->get();
            }
        }
        return nullptr;
    }

private:
    void accept()
    {
        while (m_server.hasPendingConnections()) {
            QWebSocket* socket = m_server.nextPendingConnection();
            socket->setParent(&m_context);
            auto connection = std::make_unique<FakeConnection>();
            FakeConnection* c = connection.get();
            c->socket = socket;
            c->path = socket->requestUrl().path();
            connections.push_back(std::move(connection));
            QObject::connect(socket, &QWebSocket::textMessageReceived, &m_context,
                             [this, c](const QString& text) { received(c, text); });
            QObject::connect(socket, &QWebSocket::disconnected, &m_context, [c] { c->closed = true; });
            if (c->path == QLatin1String("/v1/station") && autoSignIn) {
                c->nonce.resize(32);
                QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(c->nonce.data()), 8);
                c->send(QJsonObject{{QStringLiteral("type"), QStringLiteral("challenge")},
                                    {QStringLiteral("nonce"), Wire::toBase64Url(c->nonce)}});
            }
        }
    }

    void received(FakeConnection* c, const QString& text)
    {
        const bool station = c->path == QLatin1String("/v1/station");
        const Wire::Decoded decoded = Wire::decode(
            text, station ? Wire::Direction::StationToServer : Wire::Direction::LiveToServer);
        if (!decoded.ok) {
            c->allFramesValid = false;
            qWarning() << "fake server: refused" << decoded.error;
            return;
        }
        c->frames.append(decoded.message);
        const QString type = decoded.type;
        if (type == QLatin1String("hello") && autoSignIn) {
            bool ok = false;
            c->spki = Wire::fromBase64Url(decoded.message.value(QStringLiteral("spki")).toString(), &ok);
            const QByteArray sig = Wire::fromBase64Url(decoded.message.value(QStringLiteral("sig")).toString(), &ok);
            c->signatureVerified = StationIdentity::verify(
                c->spki, Wire::helloTranscript(QStringLiteral("127.0.0.1"), c->nonce), sig);
            c->signatureVerifiesWithPort = StationIdentity::verify(
                c->spki, Wire::helloTranscript(QStringLiteral("127.0.0.1:%1").arg(port()), c->nonce), sig);
            if (c->signatureVerified) {
                // 3.1: a second connection of one station closes the older
                // one with 4003.
                int signedIn = 1;
                for (const auto& other : connections) {
                    if (other.get() != c && other->path == c->path && other->welcomed && !other->closed) {
                        ++signedIn;
                        other->socket->close(static_cast<QWebSocketProtocol::CloseCode>(4003));
                    }
                }
                maxSignedInStations = std::max(maxSignedInStations, signedIn);
                c->welcomed = true;
                QJsonObject welcome = fixtureWire(QStringLiteral("server-station-welcome-core"));
                welcome.insert(QStringLiteral("station"), Wire::roundtableId(c->spki));
                c->send(welcome);
            } else {
                c->send(QJsonObject{{QStringLiteral("type"), QStringLiteral("error")},
                                    {QStringLiteral("code"), QStringLiteral("bad-sign-in")},
                                    {QStringLiteral("reason"), QStringLiteral("That sign-in did not check out.")},
                                    {QStringLiteral("about"), QStringLiteral("hello")},
                                    {QStringLiteral("retryAfterMs"), 0}});
                c->socket->close(static_cast<QWebSocketProtocol::CloseCode>(4001));
            }
        } else if (type == QLatin1String("weblink") && autoWebLink) {
            c->send(fixtureWire(QStringLiteral("server-station-weblink-ok")));
        }
    }

    QWebSocketServer m_server;
    // Declared last, destroyed first: the accepted sockets and every
    // connection made for them go before the server and the records.
    QObject m_context;
};

struct Clocks {
    qint64 wall = kWallStart;
    qint64 mono = kMonoStart;
    void advance(qint64 ms)
    {
        wall += ms;
        mono += ms;
    }
};

RoundtableSliceState usbSlice(bool keyed = false)
{
    RoundtableSliceState slice;
    slice.id = 0;
    slice.hz = 14212000;
    slice.mode = QStringLiteral("USB");
    slice.filterHz = 2700;
    slice.keyed = keyed;
    return slice;
}

} // namespace

class TstRoundtableClient : public QObject {
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    void signsInOverTheHostItConnectedTo();
    void unansweredOpensLiveOnlyAndSendsNothingAboutTheStation();
    void afterTheAnswerPresenceThenReadingsOnceASecond();
    void noReadingWhileKeyed();
    void hiddenIsTheLastFrameBeforeTheClose();
    void archiveWhileUnansweredSendsOnlyHelloAndWeblink();
    void retriesFollowTheLadder();
    void connectionCapWaitsItsRetryAfter();
    void refusedSignInStopsRetrying();
    void revokedKeyAndReplacedStopRetrying();
    void awayWithStopSendsNothing();
    void gapInTheBandListAsksForItAgain();
    void desktopKeyIsRemoteAccessKey();
    void refusedCallsignMeansNoStart();
    void archiveThenLiveMakesOneStationConnection();
    void hiddenReaderWatchesItsSlicesRoom();
    void aStationInfoWriteRestartsTheStationConnection();

private:
    // The client set up against the fake server with the fake clocks.
    void makeClient(bool answered);
    // Waits for the connection's frame count to reach `n`.
    bool waitFrames(FakeConnection* c, int n);
    // A frame the client sends at once (mentionsSeen live, watch reading):
    // once it arrives, everything sent before it has too.
    bool flush(FakeConnection* c);
    FakeConnection* waitConnection(const QString& path, int nth);
    void tick(qint64 ms);

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<StationIdentity> m_identity;
    std::unique_ptr<FakeServer> m_server;
    std::unique_ptr<RoundtableClient> m_client;
    Clocks m_clocks;
    int m_flushes = 0;
};

void TstRoundtableClient::init()
{
    m_dir = std::make_unique<QTemporaryDir>();
    QVERIFY(m_dir->isValid());
    m_identity = std::make_unique<StationIdentity>(StationIdentity::loadOrCreate(m_dir->path()));
    QVERIFY(m_identity->isValid());
    m_server = std::make_unique<FakeServer>();
    QVERIFY(m_server->listen());
    m_clocks = Clocks{};
    m_flushes = 0;
}

void TstRoundtableClient::cleanup()
{
    m_client.reset();
    m_server.reset();
    m_identity.reset();
    m_dir.reset();
}

void TstRoundtableClient::makeClient(bool answered)
{
    m_client = std::make_unique<RoundtableClient>();
    Clocks* clocks = &m_clocks;
    m_client->setClocksForTest([clocks] { return clocks->wall; }, [clocks] { return clocks->mono; });
    m_client->setServerUrl(m_server->stationUrl());
    m_client->setIdentity(m_identity.get());
    m_client->setStationInfo(QStringLiteral("KG4VCF"), QStringLiteral("EM73tu42"));
    m_client->setSlices({usbSlice()});
    m_client->setFirstRunAnswered(answered);
}

bool TstRoundtableClient::waitFrames(FakeConnection* c, int n)
{
    return QTest::qWaitFor([c, n] { return c->frames.size() >= n; }, kWaitMs);
}

bool TstRoundtableClient::flush(FakeConnection* c)
{
    const int before = static_cast<int>(c->frames.size());
    if (c->path == QLatin1String("/v1/station")) {
        m_client->markMentionsSeen(++m_flushes);
    } else {
        m_client->readRoom(QStringLiteral("usb-%1").arg(7000000 + ++m_flushes));
    }
    if (!waitFrames(c, before + 1)) {
        return false;
    }
    const QString type = typeOf(c->frames.last());
    const bool marker = type == QLatin1String("mentionsSeen") || type == QLatin1String("watch");
    if (marker) {
        c->frames.removeLast();
    }
    return marker;
}

FakeConnection* TstRoundtableClient::waitConnection(const QString& path, int nth)
{
    FakeServer* server = m_server.get();
    if (!QTest::qWaitFor([server, &path, nth] { return server->count(path) >= nth; }, kWaitMs)) {
        return nullptr;
    }
    return server->last(path);
}

void TstRoundtableClient::tick(qint64 ms)
{
    m_clocks.advance(ms);
    m_client->serviceTimers();
}

void TstRoundtableClient::signsInOverTheHostItConnectedTo()
{
    makeClient(true);
    QSignalSpy states(m_client.get(), &RoundtableClient::linkStateChanged);
    m_client->start();
    FakeConnection* c = waitConnection(QStringLiteral("/v1/station"), 1);
    QVERIFY(c != nullptr);
    QVERIFY(waitFrames(c, 1));
    QCOMPARE(typeOf(c->frames.first()), QStringLiteral("hello"));
    QVERIFY(c->signatureVerified);
    QVERIFY(!c->signatureVerifiesWithPort);
    QCOMPARE(c->spki, m_identity->publicKeySpki());
    QCOMPARE(c->frames.first().value(QStringLiteral("software")).toString(), QStringLiteral("NereusSDR"));
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Live, kWaitMs);
    QCOMPARE(m_client->stationId(), Wire::roundtableId(m_identity->publicKeySpki()));
    QCOMPARE(m_client->statusText(), QStringLiteral("Live as KG4VCF"));
    QVERIFY(c->allFramesValid);
    QCOMPARE(m_server->count(QStringLiteral("/v1/live")), 0);
}

void TstRoundtableClient::unansweredOpensLiveOnlyAndSendsNothingAboutTheStation()
{
    makeClient(false);
    m_client->start();
    FakeConnection* c = waitConnection(QStringLiteral("/v1/live"), 1);
    QVERIFY(c != nullptr);
    QVERIFY(waitFrames(c, 1));
    QCOMPARE(c->frames.first(), (QJsonObject{{QStringLiteral("type"), QStringLiteral("watchBand")},
                                             {QStringLiteral("band"), QStringLiteral("all")}}));
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Hidden, kWaitMs);

    // Meters, slices and time pass; still nothing about the station.
    for (int i = 1; i <= 5; ++i) {
        m_client->pushMeter(0, -110.0, i * 1000000LL);
        m_client->setSlices({usbSlice()});
        m_client->setAtRadio(i % 2 == 0);
        tick(1200);
    }
    m_client->markMentionsSeen(kWallStart);
    QVERIFY(flush(c));
    QCOMPARE(c->types(), QStringList{QStringLiteral("watchBand")});
    QCOMPARE(m_server->count(QStringLiteral("/v1/station")), 0);
    QVERIFY(c->allFramesValid);
}

void TstRoundtableClient::afterTheAnswerPresenceThenReadingsOnceASecond()
{
    makeClient(false);
    m_client->start();
    FakeConnection* live = waitConnection(QStringLiteral("/v1/live"), 1);
    QVERIFY(live != nullptr);
    QVERIFY(waitFrames(live, 1));

    m_client->setFirstRunAnswered(true);
    FakeConnection* c = waitConnection(QStringLiteral("/v1/station"), 1);
    QVERIFY(c != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(live->closed, kWaitMs);
    QVERIFY(waitFrames(c, 2));
    QCOMPARE(c->types(), (QStringList{QStringLiteral("hello"), QStringLiteral("presence")}));
    const QJsonObject presence = c->frames.at(1);
    QCOMPARE(presence.value(QStringLiteral("callsign")).toString(), QStringLiteral("KG4VCF"));
    QCOMPARE(presence.value(QStringLiteral("grid")).toString(), QStringLiteral("EM73tu"));
    QCOMPARE(presence.value(QStringLiteral("atRadio")).toBool(), true);
    QCOMPARE(presence.value(QStringLiteral("slices")).toArray().size(), 1);

    // The first reading goes on the next tick, with the wall time it was
    // taken at.
    const qint64 takenAt = m_clocks.wall;
    m_client->pushMeter(0, -110.0, 1000);
    tick(100);
    QVERIFY(waitFrames(c, 3));
    QCOMPARE(c->frames.at(2), (QJsonObject{{QStringLiteral("type"), QStringLiteral("reading")},
                                           {QStringLiteral("slice"), 0},
                                           {QStringLiteral("dbm"), -110},
                                           {QStringLiteral("t"), takenAt},
                                           {QStringLiteral("present"), false}}));
    // A new reading half a second later waits out the second.
    tick(400);
    m_client->pushMeter(0, -109.0, 2000);
    tick(400);
    QVERIFY(flush(c));
    QCOMPARE(c->frames.size(), 3);
    tick(200);
    QVERIFY(waitFrames(c, 4));
    QCOMPARE(typeOf(c->frames.at(3)), QStringLiteral("reading"));
    QCOMPARE(c->frames.at(3).value(QStringLiteral("dbm")).toDouble(), -109.0);

    // The meter's stamp did not move: no reading.
    tick(1100);
    tick(1100);
    m_client->pushMeter(0, -100.0, 2000);
    tick(1100);
    QVERIFY(flush(c));
    QCOMPARE(c->types().count(QStringLiteral("reading")), 2);

    // Presence again after 10 s with nothing changed.
    tick(10000);
    QVERIFY(flush(c));
    QCOMPARE(c->types().count(QStringLiteral("presence")), 2);
    QVERIFY(c->allFramesValid);
}

void TstRoundtableClient::noReadingWhileKeyed()
{
    makeClient(true);
    m_client->start();
    FakeConnection* c = waitConnection(QStringLiteral("/v1/station"), 1);
    QVERIFY(c != nullptr);
    QVERIFY(waitFrames(c, 2));
    m_client->pushMeter(0, -110.0, 1000);
    tick(100);
    QVERIFY(waitFrames(c, 3));

    // Keyed: a presence says so, at least 1100 ms after the last one.
    tick(1000);
    m_client->pushMeter(0, -100.0, 2000);  // taken before the key, not yet sent
    m_client->setSlices({usbSlice(true)});
    m_client->pushMeter(0, -90.0, 3000);
    tick(200);
    QVERIFY(waitFrames(c, 4));
    QCOMPARE(typeOf(c->frames.at(3)), QStringLiteral("presence"));
    const QJsonObject slice = c->frames.at(3).value(QStringLiteral("slices")).toArray().first().toObject();
    QCOMPARE(slice.value(QStringLiteral("keyed")).toBool(), true);
    for (int i = 0; i < 4; ++i) {
        m_client->pushMeter(0, -90.0, 4000 + i * 1000);
        tick(1100);
    }
    QVERIFY(flush(c));
    QCOMPARE(c->types().count(QStringLiteral("reading")), 1);

    // Unkeyed, readings resume with the next new one.
    m_client->setSlices({usbSlice(false)});
    tick(1200);
    m_client->pushMeter(0, -111.0, 9000);
    tick(1100);
    QVERIFY(flush(c));
    QCOMPARE(c->types().count(QStringLiteral("reading")), 2);
    QCOMPARE(c->frames.last().value(QStringLiteral("dbm")).toDouble(), -111.0);
    QVERIFY(c->allFramesValid);
}

void TstRoundtableClient::hiddenIsTheLastFrameBeforeTheClose()
{
    makeClient(true);
    m_client->start();
    FakeConnection* c = waitConnection(QStringLiteral("/v1/station"), 1);
    QVERIFY(c != nullptr);
    QVERIFY(waitFrames(c, 2));
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Live, kWaitMs);

    m_client->setHidden(true);
    QTRY_VERIFY_WITH_TIMEOUT(c->closed, kWaitMs);
    QCOMPARE(typeOf(c->frames.last()), QStringLiteral("hidden"));
    FakeConnection* live = waitConnection(QStringLiteral("/v1/live"), 1);
    QVERIFY(live != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Hidden, kWaitMs);
    QCOMPARE(m_client->statusText(), QStringLiteral("Hidden: reading only, nothing sent about this station"));
    tick(1200);
    m_client->pushMeter(0, -100.0, 5000);
    tick(1200);
    QVERIFY(flush(live));
    QCOMPARE(live->types(), QStringList{QStringLiteral("watchBand")});
    QCOMPARE(m_server->count(QStringLiteral("/v1/station")), 1);
}

void TstRoundtableClient::archiveWhileUnansweredSendsOnlyHelloAndWeblink()
{
    makeClient(false);
    m_client->start();
    FakeConnection* live = waitConnection(QStringLiteral("/v1/live"), 1);
    QVERIFY(live != nullptr);
    QVERIFY(waitFrames(live, 1));

    QSignalSpy links(m_client.get(), &RoundtableClient::webLinkReady);
    m_client->requestWebLink();
    FakeConnection* c = waitConnection(QStringLiteral("/v1/station"), 1);
    QVERIFY(c != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(links.size(), 1, kWaitMs);
    QCOMPARE(links.first().first().toUrl().path(), QStringLiteral("/signin"));
    QTRY_VERIFY_WITH_TIMEOUT(c->closed, kWaitMs);
    QCOMPARE(c->types(), (QStringList{QStringLiteral("hello"), QStringLiteral("weblink")}));
    QVERIFY(c->signatureVerified);
    // The live reader carried on beside it.
    QVERIFY(!live->closed);
    QCOMPARE(m_client->linkState(), RoundtableClient::LinkState::Hidden);
}

void TstRoundtableClient::retriesFollowTheLadder()
{
    // A station connection the server drops before any welcome (4005, the
    // sign-in timed out): the normal backoff every time.
    m_server->autoSignIn = false;
    makeClient(true);
    m_client->start();
    const QList<qint64> expected{1000, 2000, 5000, 10000, 30000, 30000, 30000};
    for (int i = 0; i < expected.size(); ++i) {
        FakeConnection* c = waitConnection(QStringLiteral("/v1/station"), i + 1);
        QVERIFY2(c != nullptr, qPrintable(QStringLiteral("attempt %1").arg(i + 1)));
        QTRY_VERIFY_WITH_TIMEOUT(c->socket->state() == QAbstractSocket::ConnectedState, kWaitMs);
        c->socket->close(static_cast<QWebSocketProtocol::CloseCode>(4005));
        QTRY_VERIFY_WITH_TIMEOUT(m_client->reconnectDueMsForTest() >= 0, kWaitMs);
        QCOMPARE(m_client->reconnectDueMsForTest() - m_clocks.mono, expected.at(i));
        QCOMPARE(m_client->linkState(), RoundtableClient::LinkState::Reconnecting);
        // Not before it is due.
        tick(expected.at(i) - 1);
        QTest::qWait(50);
        QCOMPARE(m_server->count(QStringLiteral("/v1/station")), i + 1);
        tick(1);
    }
}

void TstRoundtableClient::connectionCapWaitsItsRetryAfter()
{
    m_server->autoSignIn = false;
    makeClient(true);
    m_client->start();
    FakeConnection* c = waitConnection(QStringLiteral("/v1/station"), 1);
    QVERIFY(c != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(c->socket->state() == QAbstractSocket::ConnectedState, kWaitMs);
    c->send(QJsonObject{{QStringLiteral("type"), QStringLiteral("error")},
                        {QStringLiteral("code"), QStringLiteral("rate")},
                        {QStringLiteral("reason"), QStringLiteral("Too many at once. Try again in a moment.")},
                        {QStringLiteral("about"), QJsonValue::Null},
                        {QStringLiteral("retryAfterMs"), 7000}});
    c->socket->close(static_cast<QWebSocketProtocol::CloseCode>(1013));
    QTRY_VERIFY_WITH_TIMEOUT(m_client->reconnectDueMsForTest() >= 0, kWaitMs);
    QCOMPARE(m_client->reconnectDueMsForTest() - m_clocks.mono, 7000);
}

void TstRoundtableClient::refusedSignInStopsRetrying()
{
    makeClient(true);
    QSignalSpy errors(m_client.get(), &RoundtableClient::errorReceived);
    m_server->autoSignIn = false;
    m_client->start();
    FakeConnection* c = waitConnection(QStringLiteral("/v1/station"), 1);
    QVERIFY(c != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(c->socket->state() == QAbstractSocket::ConnectedState, kWaitMs);
    const QString reason = QStringLiteral("This version of NereusSDR is too old for Roundtable. Update to join.");
    c->send(QJsonObject{{QStringLiteral("type"), QStringLiteral("error")},
                        {QStringLiteral("code"), QStringLiteral("version")},
                        {QStringLiteral("reason"), reason},
                        {QStringLiteral("about"), QStringLiteral("hello")},
                        {QStringLiteral("retryAfterMs"), 0}});
    QTest::ignoreMessage(QtWarningMsg, QRegularExpression(QStringLiteral("closed the connection with 4001")));
    c->socket->close(static_cast<QWebSocketProtocol::CloseCode>(4001));
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Offline, kWaitMs);
    QCOMPARE(m_client->statusText(), reason);
    QCOMPARE(m_client->reconnectDueMsForTest(), -1);
    QCOMPARE(errors.size(), 1);
    QCOMPARE(errors.first().first().toString(), QStringLiteral("version"));
    tick(60000);
    QTest::qWait(100);
    QCOMPARE(m_server->count(QStringLiteral("/v1/station")), 1);
    // start() again tries again.
    m_server->autoSignIn = true;
    m_client->start();
    QVERIFY(waitConnection(QStringLiteral("/v1/station"), 2) != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Live, kWaitMs);
}

void TstRoundtableClient::revokedKeyAndReplacedStopRetrying()
{
    for (const int code : {4004, 4003, 4002}) {
        m_server->autoSignIn = true;
        makeClient(true);
        m_client->start();
        FakeConnection* c = waitConnection(QStringLiteral("/v1/station"),
                                           m_server->count(QStringLiteral("/v1/station")) + 1);
        QVERIFY(c != nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Live, kWaitMs);
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("closed the connection with %1").arg(code)));
        c->socket->close(static_cast<QWebSocketProtocol::CloseCode>(code));
        QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Offline, kWaitMs);
        QVERIFY(!m_client->statusText().isEmpty());
        QCOMPARE(m_client->reconnectDueMsForTest(), -1);
        const int before = m_server->count(QStringLiteral("/v1/station"));
        tick(60000);
        QTest::qWait(100);
        QCOMPARE(m_server->count(QStringLiteral("/v1/station")), before);
        m_client.reset();
    }
}

void TstRoundtableClient::awayWithStopSendsNothing()
{
    makeClient(true);
    m_client->setStopWhenAway(true);
    m_client->setAtRadio(false);
    m_client->start();
    FakeConnection* live = waitConnection(QStringLiteral("/v1/live"), 1);
    QVERIFY(live != nullptr);
    QVERIFY(waitFrames(live, 1));
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Hidden, kWaitMs);
    for (int i = 1; i <= 3; ++i) {
        m_client->pushMeter(0, -100.0, i * 1000);
        tick(1200);
    }
    QVERIFY(flush(live));
    QCOMPARE(live->types(), QStringList{QStringLiteral("watchBand")});
    QCOMPARE(m_server->count(QStringLiteral("/v1/station")), 0);

    // Someone back at the radio: live again as the station.
    m_client->setAtRadio(true);
    FakeConnection* c = waitConnection(QStringLiteral("/v1/station"), 1);
    QVERIFY(c != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Live, kWaitMs);
}

void TstRoundtableClient::gapInTheBandListAsksForItAgain()
{
    // roundtable/conformance/v1/sessions/resync-after-gap.json, the Core's
    // part: rooms gen 1, then a roomsDelta gen 3, and the Core asks for the
    // whole list with {"type":"resync","room":null}.
    makeClient(true);
    QSignalSpy rooms(m_client.get(), &RoundtableClient::roomsReceived);
    QSignalSpy deltas(m_client.get(), &RoundtableClient::roomsDeltaReceived);
    m_client->start();
    FakeConnection* c = waitConnection(QStringLiteral("/v1/station"), 1);
    QVERIFY(c != nullptr);
    QVERIFY(waitFrames(c, 2));
    c->send(fixtureWire(QStringLiteral("server-station-rooms-empty")));
    QTRY_COMPARE_WITH_TIMEOUT(rooms.size(), 1, kWaitMs);
    QJsonObject delta = fixtureWire(QStringLiteral("server-station-rooms-delta"));
    delta.insert(QStringLiteral("gen"), 3);
    c->send(delta);
    QVERIFY(waitFrames(c, 3));
    QCOMPARE(c->frames.at(2), (QJsonObject{{QStringLiteral("type"), QStringLiteral("resync")},
                                           {QStringLiteral("room"), QJsonValue::Null}}));
    QCOMPARE(deltas.size(), 0);
    // The list again, then deltas in order are taken.
    QJsonObject full = fixtureWire(QStringLiteral("server-station-rooms-empty"));
    full.insert(QStringLiteral("gen"), 3);
    c->send(full);
    QTRY_COMPARE_WITH_TIMEOUT(rooms.size(), 2, kWaitMs);
    delta.insert(QStringLiteral("gen"), 4);
    c->send(delta);
    QTRY_COMPARE_WITH_TIMEOUT(deltas.size(), 1, kWaitMs);
    QVERIFY(c->allFramesValid);
}

void TstRoundtableClient::desktopKeyIsRemoteAccessKey()
{
    // An Everything desktop with Remote Access off loads its key from the
    // settings profile directory; Remote Access, turned on later, loads it
    // from the same directory (GuiDesktopStationRuntime), so the Roundtable
    // id never changes.
    QTemporaryDir profile;
    QVERIFY(profile.isValid());
    const QString settingsFile = profile.path() + QStringLiteral("/sub/../NereusSDR.settings");
    const StationIdentity desktop = StationIdentity::loadOrCreate(RoundtableClient::profileDirectoryFor(settingsFile));
    QVERIFY(desktop.isValid());
    QVERIFY(desktop.wasCreatedThisRun());
    const StationIdentity remoteAccess =
        StationIdentity::loadOrCreate(QDir::cleanPath(QFileInfo(settingsFile).absolutePath()));
    QVERIFY(remoteAccess.isValid());
    QVERIFY(!remoteAccess.wasCreatedThisRun());
    QCOMPARE(remoteAccess.publicKeySpki(), desktop.publicKeySpki());
    QCOMPARE(QFileInfo(desktop.keyPath()).absolutePath(), QDir(profile.path()).absolutePath());
}

void TstRoundtableClient::refusedCallsignMeansNoStart()
{
    makeClient(true);
    m_client->setStationInfo(QStringLiteral("KG4//VCF"), QString());
    m_client->start();
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Offline, kWaitMs);
    QVERIFY(!m_client->statusText().isEmpty());
    QTest::qWait(100);
    QCOMPARE(m_server->count(QStringLiteral("/v1/station")), 0);
    // A good one starts it.
    m_client->setStationInfo(QStringLiteral("KG4VCF"), QString());
    QVERIFY(waitConnection(QStringLiteral("/v1/station"), 1) != nullptr);
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Live, kWaitMs);
}

void TstRoundtableClient::archiveThenLiveMakesOneStationConnection()
{
    // Hidden with the live reader up, the operator asks for the web
    // link and at once goes live, before the archive connection has its
    // answer. One station connection signs in at a time, so the server
    // never closes one with 4003 and the link still comes.
    makeClient(true);
    m_client->setHidden(true);
    QSignalSpy errors(m_client.get(), &RoundtableClient::errorReceived);
    QSignalSpy links(m_client.get(), &RoundtableClient::webLinkReady);
    m_client->start();
    FakeConnection* live = waitConnection(QStringLiteral("/v1/live"), 1);
    QVERIFY(live != nullptr);
    QVERIFY(waitFrames(live, 1));
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Hidden, kWaitMs);

    m_client->requestWebLink();
    m_client->setHidden(false);
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Live, kWaitMs);
    QTRY_COMPARE_WITH_TIMEOUT(links.size(), 1, kWaitMs);
    // Settle: nothing else opens or closes.
    QTest::qWait(200);
    QCOMPARE(m_server->maxSignedInStations, 1);
    QCOMPARE(m_client->linkState(), RoundtableClient::LinkState::Live);
    for (const QList<QVariant>& error : std::as_const(errors)) {
        QVERIFY2(error.first().toString() != QLatin1String("unreachable"),
                 qPrintable(error.value(1).toString()));
    }
    FakeConnection* station = m_server->last(QStringLiteral("/v1/station"));
    QVERIFY(station != nullptr);
    QVERIFY(!station->closed);
    QVERIFY(station->types().contains(QStringLiteral("presence")));
    QTRY_VERIFY_WITH_TIMEOUT(live->closed, kWaitMs);
}

void TstRoundtableClient::hiddenReaderWatchesItsSlicesRoom()
{
    // Hidden, the live reader shows the rooms the slices are in (7.8):
    // once the band list names the slice's room it watches it, and when
    // the slice moves out of every listed room it stops.
    makeClient(true);
    m_client->setHidden(true);
    m_client->start();
    FakeConnection* live = waitConnection(QStringLiteral("/v1/live"), 1);
    QVERIFY(live != nullptr);
    QVERIFY(waitFrames(live, 1));
    QCOMPARE(live->types(), QStringList{QStringLiteral("watchBand")});

    live->send(fixtureWire(QStringLiteral("server-live-rooms")));
    QVERIFY(waitFrames(live, 2));
    QCOMPARE(live->frames.at(1), Wire::watch(kRoom));

    RoundtableSliceState moved = usbSlice();
    moved.hz = 7200000;
    moved.mode = QStringLiteral("LSB");
    m_client->setSlices({moved});
    QVERIFY(waitFrames(live, 3));
    QCOMPARE(live->frames.at(2), Wire::unwatch(kRoom));
    QVERIFY(live->allFramesValid);
    QCOMPARE(m_server->count(QStringLiteral("/v1/station")), 0);
}

void TstRoundtableClient::aStationInfoWriteRestartsTheStationConnection()
{
    auto& settings = AppSettings::instance();
    settings.setValue(QStringLiteral("Roundtable/Callsign"), QStringLiteral("KG4VCF"));
    settings.setValue(QStringLiteral("Roundtable/GridSquare"), QStringLiteral("EM73tu42"));
    settings.setValue(QStringLiteral("Roundtable/AwayChoice"), QStringLiteral("Report"));
    makeClient(true);
    DxClusterClient dx;
    DxClusterClient rbn;
    WsjtxClient wsjtx;
    SpotCollectorClient spotCollector;
    PotaClient pota;
    PskReporterClient psk;
    SpotModel spots;
    SpotSourceHost host(&dx, &rbn, &wsjtx, &spotCollector, &pota, &psk, &spots);
    host.setRoundtableClient(m_client.get());
    const QString station = QStringLiteral("/v1/station");
    const auto presenceOf = [](FakeConnection* c) {
        for (const QJsonObject& frame : c->frames) {
            if (typeOf(frame) == QLatin1String("presence")) {
                return frame;
            }
        }
        return QJsonObject{};
    };

    // A write while stopped starts nothing.
    settings.setValue(QStringLiteral("Roundtable/GridSquare"), QStringLiteral("EM73tu"));
    QTest::qWait(100);
    QCOMPARE(m_server->count(station), 0);
    QCOMPARE(m_client->linkState(), RoundtableClient::LinkState::Off);

    QString reason;
    QVERIFY(host.connectSource(SpotSourceHost::kRoundtable, &reason));
    FakeConnection* first = waitConnection(station, 1);
    QVERIFY(first != nullptr);
    QVERIFY(waitFrames(first, 2));
    QCOMPARE(presenceOf(first).value(QStringLiteral("grid")).toString(), QStringLiteral("EM73tu"));
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Live, kWaitMs);

    // A new grid: the station connection starts again, signed in afresh.
    settings.setValue(QStringLiteral("Roundtable/GridSquare"), QStringLiteral("FN31pr"));
    FakeConnection* second = waitConnection(station, 2);
    QVERIFY(second != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(first->closed, kWaitMs);
    QVERIFY(waitFrames(second, 2));
    QCOMPARE(second->types().mid(0, 2), (QStringList{QStringLiteral("hello"), QStringLiteral("presence")}));
    QVERIFY(second->signatureVerified);
    QCOMPARE(presenceOf(second).value(QStringLiteral("grid")).toString(), QStringLiteral("FN31pr"));
    QCOMPARE(presenceOf(second).value(QStringLiteral("callsign")).toString(), QStringLiteral("KG4VCF"));
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Live, kWaitMs);

    // A callsign and a grid saved together: one new connection.
    settings.setValue(QStringLiteral("Roundtable/Callsign"), QStringLiteral("w1aw"));
    settings.setValue(QStringLiteral("Roundtable/GridSquare"), QStringLiteral("FN31"));
    FakeConnection* third = waitConnection(station, 3);
    QVERIFY(third != nullptr);
    QVERIFY(waitFrames(third, 2));
    QCOMPARE(presenceOf(third).value(QStringLiteral("callsign")).toString(), QStringLiteral("W1AW"));
    QCOMPARE(presenceOf(third).value(QStringLiteral("grid")).toString(), QStringLiteral("FN31"));
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Live, kWaitMs);
    QTest::qWait(200);
    QCOMPARE(m_server->count(station), 3);
    QCOMPARE(m_server->maxSignedInStations, 1);
    QCOMPARE(host.roundtableText(), QStringLiteral("Live as W1AW"));

    // One the server would refuse: stopped, with the start's reason, and
    // nothing sent under it.
    settings.setValue(QStringLiteral("Roundtable/Callsign"), QStringLiteral("KG4//VCF"));
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Off, kWaitMs);
    QTRY_VERIFY_WITH_TIMEOUT(third->closed, kWaitMs);
    QCOMPARE(host.roundtableState(), SpotSourceHost::kOff);
    QVERIFY2(host.roundtableText().contains(QStringLiteral("KG4//VCF")), qPrintable(host.roundtableText()));
    QTest::qWait(200);
    QCOMPARE(m_server->count(station), 3);

    // Put right, it starts again.
    settings.setValue(QStringLiteral("Roundtable/Callsign"), QStringLiteral("KG4VCF"));
    FakeConnection* fourth = waitConnection(station, 4);
    QVERIFY(fourth != nullptr);
    QVERIFY(waitFrames(fourth, 2));
    QCOMPARE(presenceOf(fourth).value(QStringLiteral("callsign")).toString(), QStringLiteral("KG4VCF"));
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Live, kWaitMs);

    // D25: unanswered, only the live reader runs, and a write opens no
    // station connection.
    m_client->setFirstRunAnswered(false);
    FakeConnection* live = waitConnection(QStringLiteral("/v1/live"), 1);
    QVERIFY(live != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(fourth->closed, kWaitMs);
    QTRY_COMPARE_WITH_TIMEOUT(m_client->linkState(), RoundtableClient::LinkState::Hidden, kWaitMs);
    settings.setValue(QStringLiteral("Roundtable/GridSquare"), QStringLiteral("EM74"));
    settings.setValue(QStringLiteral("Roundtable/Callsign"), QStringLiteral("K4ABC"));
    QTest::qWait(300);
    QCOMPARE(m_server->count(station), 4);
    QCOMPARE(m_server->count(QStringLiteral("/v1/live")), 1);
    QVERIFY(!live->closed);
    QVERIFY(flush(live));
    QVERIFY(!live->types().contains(QStringLiteral("presence")));

    // Stopped by the operator: a write starts nothing.
    QVERIFY(host.disconnectSource(SpotSourceHost::kRoundtable, &reason));
    settings.setValue(QStringLiteral("Roundtable/Callsign"), QStringLiteral("KG4VCF"));
    QTest::qWait(200);
    QCOMPARE(m_client->linkState(), RoundtableClient::LinkState::Off);
    QCOMPARE(m_server->count(station), 4);
    QVERIFY(first->allFramesValid && second->allFramesValid && third->allFramesValid && fourth->allFramesValid);

    for (const char* key : {"Roundtable/Callsign", "Roundtable/GridSquare", "Roundtable/AwayChoice"}) {
        settings.remove(QString::fromLatin1(key));
    }
}

QTEST_GUILESS_MAIN(TstRoundtableClient)
#include "tst_roundtable_client.moc"
