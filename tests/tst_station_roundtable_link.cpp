// no-port-check: NereusSDR-original.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tests/tst_station_roundtable_link.cpp  (NereusSDR)
// =================================================================
//
// Roundtable on the station link (Roundtable design 10.3, plan Task 11):
// the stationRoundtableVersion capability and the read-only `roundtable`
// object only for a peer that declared roundtable 1; the roundtable.*
// verbs and their plain refusals; a room's stream there before the read
// answer that names it, and gone ten minutes after nothing needs it; two
// remote windows sharing the Core's first-run answer and mention read
// mark; the Core's Roundtable settings written from a window, and the two
// panel preferences kept by the window.
//
// The Core's RoundtableClient mostly never connects: each test feeds it
// the views a Roundtable server would send (its own signals). The one test
// that needs posts to wait for their answers signs it in to a fake
// Roundtable server on 127.0.0.1, so nothing leaves this computer.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: Created (Roundtable plan Task 11). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-24: Task 11 fix round: the Core's own Roundtable answers
//               refused from a peer, no Roundtable setting to an older
//               peer, the spotSources Roundtable properties per peer, and
//               two connections' posts with one clientId. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QWebSocket>
#include <QWebSocketServer>

#include <memory>
#include <optional>

#include "OperatorWording.h"
#include "SessionWait.h"
#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/RadioConnection.h"
#include "core/roundtable/RoundtableClient.h"
#include "core/roundtable/RoundtableState.h"
#include "core/roundtable/RoundtableWire.h"
#include "core/security/StationIdentity.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "models/RadioModel.h"
#include "models/RoundtableModel.h"

#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

const QString kRoom = QStringLiteral("usb-14200000");
const QString kOtherRoom = QStringLiteral("cw-7030000");
const QString kUnreadable = QStringLiteral("The Core could not read this request.");
const QString kNoRoom = QStringLiteral("Roundtable has no room there right now.");
const QString kUpdateApp = QStringLiteral("Update this app to use Roundtable on this Core.");
const QString kAnswered =
    QStringLiteral("Someone already answered the Roundtable question on this Core.");

MirrorUpdate utf8(const char* name, const QString& value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Utf8, QVariant(value)};
}
MirrorUpdate boolean(const char* name, bool value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Bool, QVariant(value)};
}
MirrorUpdate i64(const char* name, qint64 value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Int64, QVariant(qlonglong(value))};
}

QList<SessionMessage> receivedMessages(const LoopbackTransport* peer)
{
    QList<SessionMessage> messages;
    for (const QByteArray& wire : peer->received()) {
        SessionMessage message;
        if (SessionMessages::decode(wire, &message)) {
            messages.append(message);
        }
    }
    return messages;
}

bool snapshotDone(const LoopbackTransport* peer)
{
    for (const SessionMessage& m : receivedMessages(peer)) {
        if (m.kind == SessionMessageKind::SnapshotComplete) {
            return true;
        }
    }
    return false;
}

std::optional<SessionMessage> resultFor(const LoopbackTransport* peer, quint32 id)
{
    for (const SessionMessage& m : receivedMessages(peer)) {
        if (m.kind == SessionMessageKind::CommandResult && m.commandId == id) {
            return m;
        }
    }
    return std::nullopt;
}

std::optional<qint64> capabilityOf(const LoopbackTransport* peer, const QByteArray& name)
{
    for (const SessionMessage& m : receivedMessages(peer)) {
        if (m.kind != SessionMessageKind::Capabilities) {
            continue;
        }
        for (const MirrorUpdate& u : m.updates) {
            if (u.name == name) {
                return u.value.toLongLong();
            }
        }
    }
    return std::nullopt;
}

bool sawRoundtableObject(const LoopbackTransport* peer)
{
    for (const SessionMessage& m : receivedMessages(peer)) {
        if (m.objectKey == "roundtable" || m.className == "RoundtableState") {
            return true;
        }
    }
    return false;
}

// The record batches `peer` was sent for `stream`.
QList<RecordBatch> batchesFor(const LoopbackTransport* peer, const QString& stream)
{
    QList<RecordBatch> batches;
    for (const SessionMessage& m : receivedMessages(peer)) {
        if (m.kind == SessionMessageKind::RecordBatch && m.recordBatch.stream == stream) {
            batches.append(m.recordBatch);
        }
    }
    return batches;
}

QString valueOf(const SessionMessage& m, const QByteArray& name)
{
    for (const MirrorUpdate& u : m.updates) {
        if (u.name == name) {
            return u.value.toString();
        }
    }
    return {};
}

// Sends a command and returns its id without waiting for the answer.
quint32 sendCommand(LoopbackTransport* peer, const QByteArray& verb,
                    const QList<MirrorUpdate>& arguments)
{
    static quint32 nextId = 7000;
    const quint32 id = ++nextId;
    peer->sendText(SessionMessages::encode(SessionMessages::commandInvoke(verb, id, arguments)));
    return id;
}

// The SettingsReject `peer` was sent for `key`, if any.
std::optional<SessionMessage> rejectFor(const LoopbackTransport* peer, const QString& key)
{
    for (const SessionMessage& m : receivedMessages(peer)) {
        if (m.kind == SessionMessageKind::SettingsReject
            && QString::fromUtf8(m.objectKey) == key) {
            return m;
        }
    }
    return std::nullopt;
}

// Whether `peer` was sent a schema for `className` with field `field`.
bool schemaHasField(const LoopbackTransport* peer, const QByteArray& className,
                    const QByteArray& field)
{
    for (const SessionMessage& m : receivedMessages(peer)) {
        if (m.kind != SessionMessageKind::Schema || m.className != className) {
            continue;
        }
        for (const SessionSchemaField& f : m.fields) {
            if (f.name == field) {
                return true;
            }
        }
    }
    return false;
}

bool sawSchema(const LoopbackTransport* peer, const QByteArray& className)
{
    for (const SessionMessage& m : receivedMessages(peer)) {
        if (m.kind == SessionMessageKind::Schema && m.className == className) {
            return true;
        }
    }
    return false;
}

SessionMessage invoke(LoopbackTransport* peer, const QByteArray& verb,
                      const QList<MirrorUpdate>& arguments)
{
    const quint32 id = sendCommand(peer, verb, arguments);
    std::optional<SessionMessage> answer;
    QDeadlineTimer deadline(5000);
    while (!(answer = resultFor(peer, id)) && !deadline.hasExpired()) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    if (!answer) {
        SessionMessage none;
        none.reason = QStringLiteral("no answer to ") + QString::fromUtf8(verb);
        return none;
    }
    return *answer;
}

QJsonObject roomsFull()
{
    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("rooms")},
        {QStringLiteral("rooms"),
         QJsonArray{QJsonObject{{QStringLiteral("id"), kRoom},
                                {QStringLiteral("family"), QStringLiteral("usb")},
                                {QStringLiteral("hz"), 14200000},
                                {QStringLiteral("members"), 2}},
                    QJsonObject{{QStringLiteral("id"), kOtherRoom},
                                {QStringLiteral("family"), QStringLiteral("cw")},
                                {QStringLiteral("hz"), 7030000},
                                {QStringLiteral("members"), 1}}}}};
}

QJsonObject roomFull(const QString& id)
{
    return QJsonObject{
        {QStringLiteral("type"), QStringLiteral("room")},
        {QStringLiteral("id"), id},
        {QStringLiteral("members"),
         QJsonArray{QJsonObject{{QStringLiteral("ref"), QStringLiteral("a/1")},
                                {QStringLiteral("callsign"), QStringLiteral("W1AW")},
                                {QStringLiteral("reading"), QJsonValue::Null}}}},
        {QStringLiteral("now"), QJsonArray{}},
        {QStringLiteral("earlier"), QJsonArray{}},
        {QStringLiteral("messages"),
         QJsonArray{QJsonObject{{QStringLiteral("id"), 31},
                                {QStringLiteral("t"), 5000},
                                {QStringLiteral("callsign"), QStringLiteral("W1AW")},
                                {QStringLiteral("text"), QStringLiteral("hello")}}}}};
}

// A Roundtable server as far as a station connection sees it: a
// challenge, a welcome for any hello, and a `posted` answer only when the
// test asks for one. Every post frame it hears is kept.
class FakeRoundtable {
public:
    FakeRoundtable()
        : m_server(QStringLiteral("fake-roundtable"), QWebSocketServer::NonSecureMode)
    {
        QObject::connect(&m_server, &QWebSocketServer::newConnection, &m_context,
                         [this] { accept(); });
    }

    bool listen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    QUrl stationUrl() const
    {
        return QUrl(QStringLiteral("ws://127.0.0.1:%1/v1/station").arg(m_server.serverPort()));
    }

    QList<QJsonObject> posts;

    void answerPost(const QString& clientId, qint64 messageId)
    {
        if (m_socket == nullptr) {
            return;
        }
        QJsonObject posted = fixtureWire(QStringLiteral("server-station-posted-ok"));
        posted.insert(QStringLiteral("clientId"), clientId);
        posted.insert(QStringLiteral("msg"), messageId);
        sendJson(posted);
    }

private:
    static QJsonObject fixtureWire(const QString& name)
    {
        QFile file(QStringLiteral(NEREUS_ROUNDTABLE_CONFORMANCE_DIR "/control/") + name
                   + QStringLiteral(".json"));
        if (!file.open(QIODevice::ReadOnly)) {
            return {};
        }
        return QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("wire"))
            .toObject();
    }

    void sendJson(const QJsonObject& message)
    {
        m_socket->sendTextMessage(
            QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
    }

    void accept()
    {
        while (m_server.hasPendingConnections()) {
            QWebSocket* socket = m_server.nextPendingConnection();
            socket->setParent(&m_context);
            if (socket->requestUrl().path() != QLatin1String("/v1/station")) {
                continue;
            }
            m_socket = socket;
            QObject::connect(socket, &QWebSocket::textMessageReceived, &m_context,
                             [this](const QString& text) { received(text); });
            QByteArray nonce(32, '\0');
            QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(nonce.data()), 8);
            sendJson(QJsonObject{{QStringLiteral("type"), QStringLiteral("challenge")},
                                 {QStringLiteral("nonce"), RoundtableWire::toBase64Url(nonce)}});
        }
    }

    void received(const QString& text)
    {
        const RoundtableWire::Decoded decoded =
            RoundtableWire::decode(text, RoundtableWire::Direction::StationToServer);
        if (!decoded.ok) {
            return;
        }
        if (decoded.type == QLatin1String("hello")) {
            bool ok = false;
            const QByteArray spki = RoundtableWire::fromBase64Url(
                decoded.message.value(QStringLiteral("spki")).toString(), &ok);
            QJsonObject welcome = fixtureWire(QStringLiteral("server-station-welcome-core"));
            welcome.insert(QStringLiteral("station"), RoundtableWire::roundtableId(spki));
            sendJson(welcome);
        } else if (decoded.type == QLatin1String("post")) {
            posts.append(decoded.message);
        }
    }

    QWebSocketServer m_server;
    QWebSocket* m_socket = nullptr;
    // Declared last, destroyed first.
    QObject m_context;
};

// A Core: its radio model (whose Roundtable client never connects), its
// settings, and its station server.
struct Core {
    QTemporaryDir dir;
    RadioModel model;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<StationServer> server;
    int peers = 0;

    Core()
    {
        RadioInfo info;
        info.macAddress = QStringLiteral("aa:bb:cc:dd:ee:72");
        model.setLastRadioInfoForTest(info);
        model.setConnectionStateForTest(ConnectionState::Connected);
        settings = std::make_unique<AppSettings>(dir.filePath(QStringLiteral("core.settings")));
        server = std::make_unique<StationServer>(
            &model, *settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
    }

    RoundtableClient* client() const { return model.roundtableClient(); }
    RoundtableState* state() const { return model.roundtableState(); }

    // A peer signed in with the Core's token, declaring `features`.
    LoopbackTransport* connect(QObject* owner, const QHash<QByteArray, int>& features)
    {
        ++peers;
        auto* core = new LoopbackTransport(QStringLiteral("core-%1").arg(peers), owner);
        auto* peer = new LoopbackTransport(QStringLiteral("peer-%1").arg(peers), owner);
        core->linkTo(peer);
        server->acceptTransport(core);
        peer->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("roundtable-test"),
            {kSessionProtocolMajor}, features)));
        peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server->token())));
        return peer;
    }

    // Two rooms listed and kRoom in view, as the server would send them.
    void listRooms()
    {
        emit client()->roomsReceived(roomsFull());
        emit client()->roomReceived(roomFull(kRoom));
    }
};

// A remote window of `core`: its model, settings and link.
struct Window {
    QTemporaryDir dir;
    RadioModel model{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    std::unique_ptr<AppSettings> settings;

    Window(Core& core, QObject* owner, const QString& name)
    {
        settings = std::make_unique<AppSettings>(dir.filePath(QStringLiteral("window.settings")));
        settings->setRemoteBackend(&proxy);
        client = std::make_unique<StationClient>(&model, &proxy);
        auto* stationEnd = new LoopbackTransport(name + QStringLiteral("-station"), owner);
        auto* clientEnd = new LoopbackTransport(name + QStringLiteral("-client"), owner);
        stationEnd->linkTo(clientEnd);
        client->startSession(clientEnd, core.server->token());
        core.server->acceptTransport(stationEnd);
    }

    ~Window() { settings->setRemoteBackend(nullptr); }

    RoundtableModel* rooms() const { return model.roundtableModel(); }
};

} // namespace

class TstStationRoundtableLink : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    // ── The gate ───────────────────────────────────────────────────

    void onlyAPeerThatDeclaredRoundtableIsSentIt()
    {
        Core core;
        LoopbackTransport* older = core.connect(this, {});
        LoopbackTransport* newer = core.connect(this, {{"roundtable", 1}});
        NEREUS_TRY_VERIFY(snapshotDone(older));
        NEREUS_TRY_VERIFY(snapshotDone(newer));
        QVERIFY(!capabilityOf(older, "stationRoundtableVersion").has_value());
        QVERIFY(!sawRoundtableObject(older));
        QCOMPARE(capabilityOf(newer, "stationRoundtableVersion").value_or(-1), 1);
        QVERIFY(sawRoundtableObject(newer));

        // An older app's verb is refused in words it can show.
        const SessionMessage refused =
            invoke(older, "roundtable.read", {utf8("roomId", kRoom)});
        QVERIFY(!refused.accepted);
        QCOMPARE(refused.reason, kUpdateApp);
        // And so is a subscription to the rooms.
        const SessionMessage subscribed = invoke(
            older, "records.subscribe",
            {utf8("stream", QStringLiteral("roundtableRooms")), i64("backlog", 10)});
        QVERIFY(!subscribed.accepted);
        QVERIFY(OperatorWording::isPlain(subscribed.reason));

        // Nothing about Roundtable reaches the older peer, from the hello on
        // and as it changes, but the answers to its own two requests.
        core.listRooms();
        QVERIFY(invoke(newer, "roundtable.answerFirstRun", {boolean("live", true)}).accepted);
        QTest::qWait(150);
        for (qsizetype i = 0; i < older->received().size(); ++i) {
            const QByteArray& wire = older->received().at(i);
            SessionMessage decoded;
            if (SessionMessages::decode(wire, &decoded)
                && decoded.kind == SessionMessageKind::CommandResult
                && (decoded.commandId == refused.commandId
                    || decoded.commandId == subscribed.commandId)) {
                continue;
            }
            QVERIFY2(!wire.toLower().contains("roundtable"), wire.constData());
        }
    }

    void noRoundtableSettingReachesAnOlderPeer()
    {
        // Roundtable/ keys in the Core's settings, before and after the
        // peers connect: in the snapshot and the relay for a peer that
        // declared roundtable, in neither for one that did not.
        Core core;
        core.settings->setValue(QStringLiteral("Roundtable/Callsign"), QStringLiteral("KG4VCF"));
        core.settings->setValue(QStringLiteral("Roundtable/MuteList"), QStringLiteral("W1AW"));
        LoopbackTransport* older = core.connect(this, {});
        LoopbackTransport* newer = core.connect(this, {{"roundtable", 1}});
        NEREUS_TRY_VERIFY(snapshotDone(older));
        NEREUS_TRY_VERIFY(snapshotDone(newer));
        core.settings->setValue(QStringLiteral("Roundtable/Grid"), QStringLiteral("EM73"));
        core.settings->setValue(QStringLiteral("Roundtable/Callsign"), QStringLiteral("KG4VCG"));
        core.settings->remove(QStringLiteral("Roundtable/MuteList"));

        const auto sawText = [](const LoopbackTransport* peer, const QByteArray& text) {
            for (const QByteArray& wire : peer->received()) {
                if (wire.contains(text)) {
                    return true;
                }
            }
            return false;
        };
        NEREUS_TRY_VERIFY(sawText(newer, "KG4VCG"));
        QVERIFY(sawText(newer, "KG4VCF"));
        QVERIFY(sawText(newer, "EM73"));
        QTest::qWait(150);
        for (qsizetype i = 0; i < older->received().size(); ++i) {
            QVERIFY2(!older->received().at(i).toLower().contains("roundtable"),
                     older->received().at(i).constData());
        }
    }

    void theSpotSourcesRoundtablePropertiesArePerPeer()
    {
        Core core;
        LoopbackTransport* older = core.connect(this, {});
        LoopbackTransport* newer = core.connect(this, {{"roundtable", 1}});
        NEREUS_TRY_VERIFY(snapshotDone(older));
        NEREUS_TRY_VERIFY(snapshotDone(newer));
        QVERIFY(sawSchema(older, "SpotSourceHost"));
        QVERIFY(sawSchema(newer, "SpotSourceHost"));
        QVERIFY(schemaHasField(newer, "SpotSourceHost", "roundtableState"));
        QVERIFY(schemaHasField(newer, "SpotSourceHost", "roundtableText"));
        QVERIFY(!schemaHasField(older, "SpotSourceHost", "roundtableState"));
        QVERIFY(!schemaHasField(older, "SpotSourceHost", "roundtableText"));
        // The rest of the host reaches both.
        QVERIFY(schemaHasField(older, "SpotSourceHost", "pskReporterText"));
    }

    void theObjectIsReadOnly()
    {
        Core core;
        LoopbackTransport* peer = core.connect(this, {{"roundtable", 1}});
        NEREUS_TRY_VERIFY(snapshotDone(peer));
        peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "roundtable", {MirrorUpdate{0, "hidden", MirrorWireKind::Bool, QVariant(true)}})));
        QTest::qWait(150);
        QVERIFY(!core.state()->hidden());
    }

    void aPeerCannotWriteTheCoresOwnAnswers()
    {
        // The Core changes its first-run answer, hidden choice, mute list
        // and mention mark only through Roundtable's own verbs; a peer's
        // settings write or remove of them is refused, and the Core stays
        // hidden.
        Core core;
        core.settings->setValue(QStringLiteral("Roundtable/FirstRunAnswer"),
                                QStringLiteral("Hidden"));
        core.settings->setValue(QStringLiteral("Roundtable/Hidden"), QStringLiteral("True"));
        core.settings->setValue(QStringLiteral("Roundtable/MuteList"), QStringLiteral("W1AW"));
        core.settings->setValue(QStringLiteral("Roundtable/MentionsSeenMs"),
                                QStringLiteral("1000"));
        NEREUS_TRY_VERIFY(core.state()->hidden());
        LoopbackTransport* peer = core.connect(this, {{"roundtable", 1}});
        NEREUS_TRY_VERIFY(snapshotDone(peer));

        const QList<std::pair<QString, QString>> writes{
            {QStringLiteral("Roundtable/FirstRunAnswer"), QStringLiteral("Live")},
            {QStringLiteral("Roundtable/Hidden"), QStringLiteral("False")},
            {QStringLiteral("roundtable/mutelist"), QString()},
            {QStringLiteral("Roundtable/MentionsSeenMs"), QStringLiteral("9000")}};
        for (const auto& [key, value] : writes) {
            peer->sendText(SessionMessages::encode(
                SessionMessages::settingsWrite(key, value, QStringLiteral("peer"))));
            NEREUS_TRY_VERIFY(rejectFor(peer, key).has_value());
            QVERIFY2(OperatorWording::isPlain(rejectFor(peer, key)->reason),
                     qPrintable(rejectFor(peer, key)->reason));
        }
        // Over an earlier write's reject for the same key.
        const qsizetype before = receivedMessages(peer).size();
        peer->sendText(SessionMessages::encode(
            SessionMessages::settingsRemove(QStringLiteral("Roundtable/Hidden"))));
        NEREUS_TRY_VERIFY(receivedMessages(peer).size() > before);
        bool removeRefused = false;
        const QList<SessionMessage> after = receivedMessages(peer).mid(before);
        for (const SessionMessage& m : after) {
            if (m.kind == SessionMessageKind::SettingsReject
                && m.objectKey == QByteArrayLiteral("Roundtable/Hidden")) {
                removeRefused = true;
            }
        }
        QVERIFY(removeRefused);

        QTest::qWait(150);
        QVERIFY(core.state()->hidden());
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/FirstRunAnswer")).toString(),
                 QStringLiteral("Hidden"));
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/Hidden")).toString(),
                 QStringLiteral("True"));
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/MuteList")).toString(),
                 QStringLiteral("W1AW"));
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/MentionsSeenMs")).toString(),
                 QStringLiteral("1000"));
    }

    void aHiddenAnswerStaysHiddenWithoutItsHiddenKey()
    {
        Core core;
        core.settings->setValue(QStringLiteral("Roundtable/FirstRunAnswer"),
                                QStringLiteral("Hidden"));
        core.settings->setValue(QStringLiteral("Roundtable/Hidden"), QStringLiteral("True"));
        NEREUS_TRY_VERIFY(core.state()->hidden());
        core.settings->remove(QStringLiteral("Roundtable/Hidden"));
        QTest::qWait(50);
        QVERIFY(core.state()->hidden());
        // Only an explicit False takes it live.
        core.settings->setValue(QStringLiteral("Roundtable/Hidden"), QStringLiteral("False"));
        NEREUS_TRY_VERIFY(!core.state()->hidden());
    }

    // ── The verbs ──────────────────────────────────────────────────

    void twoConnectionsPostingOneClientIdEachGetTheirOwnAnswer()
    {
        // The key outlives the Core that signs with it.
        QTemporaryDir keyDir;
        QVERIFY(keyDir.isValid());
        const StationIdentity identity = StationIdentity::loadOrCreate(keyDir.path());
        Core core;
        FakeRoundtable roundtable;
        QVERIFY(roundtable.listen());
        QVERIFY(identity.isValid());
        core.client()->setServerUrl(roundtable.stationUrl());
        core.client()->setIdentity(&identity);
        core.client()->setStationInfo(QStringLiteral("KG4VCF"), QStringLiteral("EM73"));
        core.settings->setValue(QStringLiteral("Roundtable/FirstRunAnswer"),
                                QStringLiteral("Live"));
        core.settings->setValue(QStringLiteral("Roundtable/Hidden"), QStringLiteral("False"));
        core.client()->start();
        NEREUS_TRY_VERIFY(core.client()->linkState() == RoundtableClient::LinkState::Live);
        core.listRooms();

        LoopbackTransport* first = core.connect(this, {{"roundtable", 1}});
        LoopbackTransport* second = core.connect(this, {{"roundtable", 1}});
        NEREUS_TRY_VERIFY(snapshotDone(first));
        NEREUS_TRY_VERIFY(snapshotDone(second));
        const QString clientId = QStringLiteral("m-1-1");
        const quint32 firstId = sendCommand(
            first, "roundtable.post",
            {utf8("roomId", kRoom), utf8("text", QStringLiteral("from one")),
             utf8("clientId", clientId)});
        const quint32 secondId = sendCommand(
            second, "roundtable.post",
            {utf8("roomId", kRoom), utf8("text", QStringLiteral("from two")),
             utf8("clientId", clientId)});
        NEREUS_TRY_COMPARE(roundtable.posts.size(), 2);
        QVERIFY(!resultFor(first, firstId).has_value());
        QVERIFY(!resultFor(second, secondId).has_value());
        const QString firstWire =
            roundtable.posts.at(0).value(QStringLiteral("clientId")).toString();
        const QString secondWire =
            roundtable.posts.at(1).value(QStringLiteral("clientId")).toString();
        QVERIFY(firstWire != secondWire);
        QCOMPARE(roundtable.posts.at(0).value(QStringLiteral("text")).toString(),
                 QStringLiteral("from one"));

        // The second answer first: only the second connection hears it.
        roundtable.answerPost(secondWire, 52);
        NEREUS_TRY_VERIFY(resultFor(second, secondId).has_value());
        QVERIFY(resultFor(second, secondId)->accepted);
        QCOMPARE(valueOf(*resultFor(second, secondId), "messageId"), QStringLiteral("52"));
        QVERIFY(!resultFor(first, firstId).has_value());
        roundtable.answerPost(firstWire, 51);
        NEREUS_TRY_VERIFY(resultFor(first, firstId).has_value());
        QVERIFY(resultFor(first, firstId)->accepted);
        QCOMPARE(valueOf(*resultFor(first, firstId), "messageId"), QStringLiteral("51"));

        // A second post of one clientId on one connection still waits its
        // turn.
        const quint32 again = sendCommand(
            first, "roundtable.post",
            {utf8("roomId", kRoom), utf8("text", QStringLiteral("again")),
             utf8("clientId", QStringLiteral("m-1-2"))});
        const SessionMessage twice = invoke(
            first, "roundtable.post",
            {utf8("roomId", kRoom), utf8("text", QStringLiteral("twice")),
             utf8("clientId", QStringLiteral("m-1-2"))});
        QVERIFY(!twice.accepted);
        NEREUS_TRY_COMPARE(roundtable.posts.size(), 3);
        QVERIFY(!resultFor(first, again).has_value());
        core.client()->stop();
    }

    // ── The verbs (no Roundtable connection) ───────────────────────

    void theVerbsAnswerPlainly()
    {
        Core core;
        LoopbackTransport* peer = core.connect(this, {{"roundtable", 1}});
        NEREUS_TRY_VERIFY(snapshotDone(peer));

        // A request missing a name, or with one it does not know.
        SessionMessage answer = invoke(peer, "roundtable.read", {utf8("room", kRoom)});
        QCOMPARE(answer.reason, kUnreadable);
        answer = invoke(peer, "roundtable.setHidden", {utf8("hidden", QStringLiteral("yes"))});
        QCOMPARE(answer.reason, kUnreadable);

        // A room that is not listed.
        answer = invoke(peer, "roundtable.read", {utf8("roomId", QStringLiteral("usb-1"))});
        QVERIFY(!answer.accepted);
        QCOMPARE(answer.reason, kNoRoom);

        // The station connection is off: the verbs that need it say so.
        for (const auto& [verb, arguments] :
             QList<std::pair<QByteArray, QList<MirrorUpdate>>>{
                 {"roundtable.delete", {utf8("messageId", QStringLiteral("31"))}},
                 {"roundtable.name",
                  {utf8("turnId", QStringLiteral("4")), utf8("name", QStringLiteral("W1AW"))}},
                 {"roundtable.report",
                  {utf8("kind", QStringLiteral("message")), utf8("target", QStringLiteral("31")),
                   utf8("reason", QStringLiteral("spam"))}},
                 {"roundtable.post",
                  {utf8("roomId", kRoom), utf8("text", QStringLiteral("hi")),
                   utf8("clientId", QStringLiteral("c-1"))}},
                 {"roundtable.webSignIn", {}},
                 {"roundtable.deleteArchive",
                  {utf8("confirm", QStringLiteral("delete my archive"))}}}) {
            answer = invoke(peer, verb, arguments);
            QVERIFY2(!answer.accepted, verb.constData());
            QVERIFY2(OperatorWording::isPlain(answer.reason), qPrintable(answer.reason));
            QVERIFY2(answer.reason != kUnreadable, verb.constData());
        }
        answer = invoke(peer, "roundtable.deleteArchive",
                        {utf8("confirm", QStringLiteral("yes"))});
        QCOMPARE(answer.reason, QStringLiteral("Type delete my archive to confirm."));
        answer = invoke(peer, "roundtable.mute", {utf8("callsign", QStringLiteral("not a call"))});
        QCOMPARE(answer.reason, QStringLiteral("Roundtable can only mute a callsign."));

        // The Core's own answers.
        QVERIFY(invoke(peer, "roundtable.mute", {utf8("callsign", QStringLiteral("W1AW/P"))})
                    .accepted);
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/MuteList")).toString(),
                 QStringLiteral("W1AW"));
        QVERIFY(invoke(peer, "roundtable.unmute", {utf8("callsign", QStringLiteral("W1AW"))})
                    .accepted);
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/MuteList")).toString(), QString());
        QVERIFY(invoke(peer, "roundtable.setHidden", {boolean("hidden", true)}).accepted);
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/Hidden")).toString(),
                 QStringLiteral("True"));
        NEREUS_TRY_VERIFY(core.state()->hidden());
        QVERIFY(invoke(peer, "roundtable.markMentionRead", {i64("upToMs", 1000)}).accepted);
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/MentionsSeenMs")).toString(),
                 QStringLiteral("1000"));
        // The mark only moves forward.
        QVERIFY(invoke(peer, "roundtable.markMentionRead", {i64("upToMs", 10)}).accepted);
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/MentionsSeenMs")).toString(),
                 QStringLiteral("1000"));
    }

    void aReadsStreamIsThereBeforeItsAnswer()
    {
        Core core;
        LoopbackTransport* peer = core.connect(this, {{"roundtable", 1}});
        NEREUS_TRY_VERIFY(snapshotDone(peer));
        core.listRooms();

        const SessionMessage read = invoke(peer, "roundtable.read", {utf8("roomId", kRoom)});
        QVERIFY2(read.accepted, qPrintable(read.reason));
        const QString stream = valueOf(read, "stream");
        QCOMPARE(stream, RoundtableModel::roomStreamName(kRoom));

        const SessionMessage subscribed =
            invoke(peer, "records.subscribe", {utf8("stream", stream), i64("backlog", 400)});
        QVERIFY2(subscribed.accepted, qPrintable(subscribed.reason));
        NEREUS_TRY_VERIFY(!batchesFor(peer, stream).isEmpty());
        const RecordBatch first = batchesFor(peer, stream).first();
        QVERIFY(first.reset);
        QStringList ids;
        for (const RecordUpsert& u : first.upserts) {
            ids.append(u.id);
        }
        QVERIFY2(ids.contains(QStringLiteral("member:a/1")), qPrintable(ids.join(',')));
        QVERIFY2(ids.contains(QStringLiteral("message:31")), qPrintable(ids.join(',')));

        // The room's changes follow.
        emit core.client()->roomDeltaReceived(QJsonObject{
            {QStringLiteral("room"), kRoom},
            {QStringLiteral("messages"),
             QJsonArray{QJsonObject{{QStringLiteral("id"), 32},
                                    {QStringLiteral("t"), 6000},
                                    {QStringLiteral("callsign"), QStringLiteral("W1AW")},
                                    {QStringLiteral("text"), QStringLiteral("again")}}}}});
        NEREUS_TRY_VERIFY(batchesFor(peer, stream).size() >= 2);
        const RecordBatch next = batchesFor(peer, stream).last();
        QVERIFY(!next.reset);
        QCOMPARE(next.upserts.size(), 1);
        QCOMPARE(next.upserts.first().id, QStringLiteral("message:32"));
    }

    void aStreamNobodyNeedsGoesAfterTenMinutes()
    {
        Core core;
        LoopbackTransport* peer = core.connect(this, {{"roundtable", 1}});
        NEREUS_TRY_VERIFY(snapshotDone(peer));
        core.listRooms();
        QVERIFY(invoke(peer, "roundtable.read", {utf8("roomId", kRoom)}).accepted);
        const QString stream = RoundtableModel::roomStreamName(kRoom);

        // Still read: kept however long.
        core.server->sweepRoundtableStreamsForTest(60 * 60 * 1000);
        QVERIFY(invoke(peer, "records.subscribe", {utf8("stream", stream), i64("backlog", 1)})
                    .accepted);

        QVERIFY(invoke(peer, "roundtable.unread", {utf8("roomId", kRoom)}).accepted);
        core.server->sweepRoundtableStreamsForTest(9 * 60 * 1000);
        QVERIFY(invoke(peer, "records.subscribe", {utf8("stream", stream), i64("backlog", 1)})
                    .accepted);
        core.server->sweepRoundtableStreamsForTest(10 * 60 * 1000);
        QVERIFY(!invoke(peer, "records.subscribe", {utf8("stream", stream), i64("backlog", 1)})
                     .accepted);

        // A slice in the room keeps its stream.
        core.state()->setSlices({RoundtableSliceState{0, 14200300, QStringLiteral("USB"), 2700,
                                                      false}});
        QCOMPARE(core.state()->sliceRoomId(0), kRoom);
        core.server->sweepRoundtableStreamsForTest(60 * 60 * 1000);
        QVERIFY(invoke(peer, "records.subscribe", {utf8("stream", stream), i64("backlog", 1)})
                    .accepted);
    }

    // ── Two windows ────────────────────────────────────────────────

    void aFirstRunAnswerReachesEveryWindow()
    {
        Core core;
        Window first(core, this, QStringLiteral("first"));
        Window second(core, this, QStringLiteral("second"));
        NEREUS_TRY_VERIFY(first.client->roundtableAvailable());
        NEREUS_TRY_VERIFY(second.client->roundtableAvailable());
        QVERIFY(!second.rooms()->firstRunAnswered());

        first.rooms()->answerFirstRun(true);
        NEREUS_TRY_VERIFY(second.rooms()->firstRunAnswered());
        QVERIFY(first.rooms()->firstRunAnswered());
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/FirstRunAnswer")).toString(),
                 QStringLiteral("Live"));
        QCOMPARE(core.settings->value(QStringLiteral("Roundtable/Hidden")).toString(),
                 QStringLiteral("False"));

        // Asked again from the other window: answered already.
        QSignalSpy refused(second.rooms(), &RoundtableModel::actionRefused);
        second.rooms()->answerFirstRun(false);
        NEREUS_TRY_VERIFY(!refused.isEmpty());
        QCOMPARE(refused.first().at(0).toString(), QStringLiteral("answerFirstRun"));
        QCOMPARE(refused.first().at(1).toString(), kAnswered);
    }

    void aMentionReadOnOneWindowClearsOnTheOther()
    {
        Core core;
        Window first(core, this, QStringLiteral("first"));
        Window second(core, this, QStringLiteral("second"));
        NEREUS_TRY_VERIFY(first.client->roundtableAvailable());
        NEREUS_TRY_VERIFY(second.client->roundtableAvailable());

        emit core.client()->mentionReceived(QJsonObject{
            {QStringLiteral("room"), kRoom},
            {QStringLiteral("message"),
             QJsonObject{{QStringLiteral("id"), 41},
                         {QStringLiteral("t"), 8000},
                         {QStringLiteral("callsign"), QStringLiteral("W1AW")},
                         {QStringLiteral("text"), QStringLiteral("KG4VCF are you there")}}}});
        NEREUS_TRY_COMPARE(first.rooms()->mentionCount(kRoom), 1);
        NEREUS_TRY_COMPARE(second.rooms()->mentionCount(kRoom), 1);

        first.rooms()->markMentionRead(8000);
        NEREUS_TRY_COMPARE(second.rooms()->mentionCount(kRoom), 0);
        QCOMPARE(first.rooms()->mentionCount(kRoom), 0);
    }

    void aWindowFollowsTheRoomsItReadsAndItsSlicesAreIn()
    {
        Core core;
        core.listRooms();
        emit core.client()->roomReceived(roomFull(kOtherRoom));
        Window window(core, this, QStringLiteral("window"));
        NEREUS_TRY_VERIFY(window.client->roundtableAvailable());

        // The band list.
        NEREUS_TRY_COMPARE(window.rooms()->rooms().size(), 2);
        QCOMPARE(window.rooms()->rooms(), core.model.roundtableModel()->rooms());

        // A room read from the window.
        window.rooms()->readRoom(kRoom);
        NEREUS_TRY_VERIFY(!window.rooms()->room(kRoom).isEmpty());
        QCOMPARE(window.rooms()->room(kRoom), core.model.roundtableModel()->room(kRoom));
        window.rooms()->unreadRoom();
        NEREUS_TRY_VERIFY(window.rooms()->room(kRoom).isEmpty());

        // A room the Core's slice is in, without reading it.
        core.state()->setSlices({RoundtableSliceState{0, 7030100, QStringLiteral("CWU"), 500,
                                                      false}});
        NEREUS_TRY_COMPARE(window.rooms()->sliceRoomId(0), kOtherRoom);
        NEREUS_TRY_VERIFY(!window.rooms()->room(kOtherRoom).isEmpty());
        QCOMPARE(window.rooms()->room(kOtherRoom),
                 core.model.roundtableModel()->room(kOtherRoom));

        // A read of a room that is not listed comes back refused.
        QSignalSpy refused(window.rooms(), &RoundtableModel::actionRefused);
        window.rooms()->readRoom(QStringLiteral("usb-1"));
        NEREUS_TRY_VERIFY(!refused.isEmpty());
        QCOMPARE(refused.first().at(1).toString(), kNoRoom);
        QVERIFY(window.rooms()->readingRoomId().isEmpty());
    }

    // ── Settings ───────────────────────────────────────────────────

    void theCoresSettingsAreWrittenAndThePanelsAreNot()
    {
        Core core;
        Window window(core, this, QStringLiteral("window"));
        NEREUS_TRY_VERIFY(window.client->roundtableAvailable());
        NEREUS_TRY_VERIFY(window.proxy.ready());

        window.settings->setValue(QStringLiteral("Roundtable/Callsign"),
                                  QStringLiteral("KG4VCF"));
        NEREUS_TRY_COMPARE(core.settings->value(QStringLiteral("Roundtable/Callsign")).toString(),
                           QStringLiteral("KG4VCF"));

        window.settings->setValue(QStringLiteral("Roundtable/PanelOpensOn"),
                                  QStringLiteral("Band"));
        QVERIFY(!window.proxy.handlesKey(QStringLiteral("Roundtable/PanelOpensOn")));
        QCOMPARE(window.settings->value(QStringLiteral("Roundtable/PanelOpensOn")).toString(),
                 QStringLiteral("Band"));
        QTest::qWait(150);
        QVERIFY(!core.settings->contains(QStringLiteral("Roundtable/PanelOpensOn")));
    }
};

QTEST_MAIN(TstStationRoundtableLink)
#include "tst_station_roundtable_link.moc"
