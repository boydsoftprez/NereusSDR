// no-port-check: NereusSDR-original.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tests/tst_roundtable_desktop_flow.cpp  (NereusSDR)
// =================================================================
//
// Roundtable plan Task 15: one operator's whole Roundtable session in a
// real MainWindow, against a fake Roundtable server on 127.0.0.1, run twice:
//
//   - Everything: a local MainWindow whose own radio model runs the
//     Roundtable connection.
//   - Window: a remote MainWindow (RemoteWindowHarness) whose Core runs it,
//     everything Roundtable reaching the window over the station link.
//
// The flow, in order: the first run (nothing about the station sent), Go
// live, tune into a room by its tag, the readings, a message posted, a
// mention (the flag's @1, the pill, read after 2 s on screen, and read on a
// second window too), another room read from the pop-out's list, Back to
// my slice, keyed (the TX line) and unkeyed, the unknown turn named, hidden
// (NOT SENT on your own reading, posting off, "You are hidden"), and the
// Archive's one-time link. After each step no widget runs past its
// container's width, and the window placement shows exactly the screens
// the Everything placement showed.
//
// The flag line's click is the real one, in a MainWindow with the managed
// workspace on, so the Roundtable applet it shows is a container's.
//
// Nothing leaves this computer: Roundtable/AutoStart is "False" and the
// connection points at the fake server before anything starts.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24: Created (Roundtable plan Task 15). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-25: Roundtable fix wave: the Core's meter pump stops while the
//               own reading is shown, since a clear now reaches the panel.
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <functional>

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLoggingCategory>
#include <QMetaProperty>
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollBar>
#include <QSignalSpy>
#include <QPointer>
#include <QSslSocket>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QWebSocket>
#include <QWebSocketServer>

#include <algorithm>
#include <memory>
#include <vector>

#include "OperatorWording.h"
#include "UnusedLoopbackPort.h"
#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/MoxController.h"
#include "core/audio/CompositeTxMicRouter.h"
#include "core/meters/SliceMeterPump.h"
#include "core/SpotSourceHost.h"
#include "core/roundtable/RoundtableClient.h"
#include "core/roundtable/RoundtableState.h"
#include "core/roundtable/RoundtableWire.h"
#include "core/session/StationClient.h"
#include "core/session/IStationLink.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/RemoteWindowHarness.h"
#include "fakes/UpgradedCoreToken.h"
#include "core/SliceOwnership.h"
#include "gui/DesktopStationController.h"
#include "gui/MainWindow.h"
#include "gui/PanadapterStack.h"
#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/AppletVisibilityController.h"
#include "gui/applets/RoundtableApplet.h"
#include "gui/roundtable/RoundtableFirstRunDialog.h"
#include "gui/roundtable/RoundtableFrequencyPeek.h"
#include "gui/roundtable/RoundtablePanel.h"
#include "gui/roundtable/RoundtableTagLayout.h"
#include "gui/roundtable/RoundtableWindow.h"
#include "gui/SMeterWidget.h"
#include "gui/meters/MeterPoller.h"
#include "gui/widgets/VfoLevelBar.h"
#include "gui/widgets/VfoWidget.h"
#include "models/RadioModel.h"
#include "models/RoundtableModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;
using NereusSDR::Test::RemoteWindowHarness;
namespace Wire = NereusSDR::RoundtableWire;

namespace {

const QString kSuite = QStringLiteral(NEREUS_ROUNDTABLE_CONFORMANCE_DIR);
const QString kRoom = QStringLiteral("usb-14212000");
const QString kOtherRoom = QStringLiteral("digu-14074000");
const QString kYouRef = QStringLiteral("p5vd4n2b");
const QString kPosted = QStringLiteral("Hello from the flow test");
constexpr qint64 kStartMs = 1800000006000LL;
constexpr qint64 kPostedId = 1001;
constexpr qint64 kTxTurn = 4411;
constexpr qint64 kUnknownTurn = 4407;
constexpr double kStartHz = 14200000.0;
constexpr double kPanSpanHz = 192000.0;
constexpr int kWaitMs = 10000;

qint64 g_now = kStartMs;

QtMessageHandler g_previousHandler = nullptr;

// The offscreen platform's notes about what it cannot do, its font
// aliases and the GPU spectrum's missing QRhi are not Roundtable's;
// everything else goes through.
void quietPlatformNotes(QtMsgType type, const QMessageLogContext& context, const QString& text)
{
    if (text.startsWith(QLatin1String("This plugin does not support"))
        || text.startsWith(QLatin1String("Populating font family aliases"))
        || text.startsWith(QLatin1String("QRhiWidget: QRhi is not supported"))) {
        return;
    }
    if (g_previousHandler != nullptr) {
        g_previousHandler(type, context, text);
    }
}

QJsonObject fixtureWire(const QString& name)
{
    QFile file(kSuite + QStringLiteral("/control/") + name + QStringLiteral(".json"));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("wire")).toObject();
}

QJsonObject memberNamed(const QJsonObject& room, const QString& callsign)
{
    for (const QJsonValue& value : room.value(QStringLiteral("members")).toArray()) {
        if (value.toObject().value(QStringLiteral("callsign")).toString() == callsign) {
            return value.toObject();
        }
    }
    return {};
}

QJsonObject turnWithId(const QJsonObject& room, qint64 id)
{
    for (const QString& list : {QStringLiteral("now"), QStringLiteral("earlier")}) {
        for (const QJsonValue& value : room.value(list).toArray()) {
            if (value.toObject().value(QStringLiteral("id")).toInteger() == id) {
                return value.toObject();
            }
        }
    }
    return {};
}

// The station's view of the room the slice is in: the fixture's, with the
// unknown turn not yet named by this station (wire spec 6.9: `mine` is the
// name this station gave it).
QJsonObject stationRoom()
{
    QJsonObject room = fixtureWire(QStringLiteral("server-station-room"));
    QJsonArray earlier;
    for (const QJsonValue& value : room.value(QStringLiteral("earlier")).toArray()) {
        QJsonObject turn = value.toObject();
        if (turn.value(QStringLiteral("id")).toInteger() == kUnknownTurn) {
            turn.insert(QStringLiteral("mine"), QJsonValue::Null);
        }
        earlier.append(turn);
    }
    room.insert(QStringLiteral("earlier"), earlier);
    return room;
}

// Another room on 20 m, quiet, for reading without tuning.
QJsonObject otherRoom()
{
    QJsonObject room = fixtureWire(QStringLiteral("server-station-room"));
    QJsonObject k4abc = memberNamed(room, QStringLiteral("K4ABC"));
    k4abc.insert(QStringLiteral("keyed"), false);
    QJsonObject portable = memberNamed(room, QStringLiteral("W4XYZ/P"));
    portable.insert(QStringLiteral("joining"), false);
    room.insert(QStringLiteral("id"), kOtherRoom);
    room.insert(QStringLiteral("family"), QStringLiteral("digu"));
    room.insert(QStringLiteral("hz"), 14074000);
    room.insert(QStringLiteral("members"), QJsonArray{k4abc, portable});
    room.insert(QStringLiteral("now"), QJsonArray{});
    room.insert(QStringLiteral("earlier"), QJsonArray{});
    room.insert(QStringLiteral("messages"), QJsonArray{});
    return room;
}

// The live reader's view of the room: as anyone sees it, and without this
// station, which is hidden.
QJsonObject liveRoom()
{
    QJsonObject room = fixtureWire(QStringLiteral("server-live-room"));
    QJsonArray members;
    for (const QJsonValue& value : room.value(QStringLiteral("members")).toArray()) {
        if (value.toObject().value(QStringLiteral("callsign")).toString() != QLatin1String("KG4VCF")) {
            members.append(value);
        }
    }
    room.insert(QStringLiteral("members"), members);
    return room;
}

// One connection the flow server accepted.
struct FlowConnection {
    QWebSocket* socket = nullptr;
    QString path;
    QList<QJsonObject> frames;
    QByteArray nonce;
    bool welcomed = false;
    bool closed = false;
    bool keyed = false;
    QHash<QString, qint64> gens;

    QStringList types() const
    {
        QStringList out;
        for (const QJsonObject& frame : frames) {
            out.append(frame.value(QStringLiteral("type")).toString());
        }
        return out;
    }

    bool sentPresence() const { return types().contains(QStringLiteral("presence")); }

    void send(const QJsonObject& message) const
    {
        socket->sendTextMessage(QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact)));
    }

    // A full view of `room`, gen 1 on this connection.
    void sendRoom(QJsonObject room)
    {
        const QString id = room.value(QStringLiteral("id")).toString();
        gens.insert(id, 1);
        room.insert(QStringLiteral("gen"), 1);
        send(room);
    }

    // A delta on `roomId`: the next gen on this connection.
    void sendDelta(const QString& roomId, const QJsonArray& members, const QJsonArray& turns,
                   const QJsonArray& messages)
    {
        QJsonObject delta = fixtureWire(QStringLiteral("server-station-delta"));
        const qint64 gen = gens.value(roomId, 1) + 1;
        gens.insert(roomId, gen);
        delta.insert(QStringLiteral("room"), roomId);
        delta.insert(QStringLiteral("gen"), gen);
        delta.insert(QStringLiteral("members"), members);
        delta.insert(QStringLiteral("left"), QJsonArray{});
        delta.insert(QStringLiteral("turns"), turns);
        delta.insert(QStringLiteral("messages"), messages);
        delta.insert(QStringLiteral("closed"), false);
        send(delta);
    }
};

// A Roundtable server for the flow (wire spec 5 and 6), on 127.0.0.1:
//   /v1/station: a challenge, a welcome for any hello, the band list, then
//     the room view once a presence places a slice in usb-14212000; a read
//     answered with that room's view; a post with `posted` and the message
//     in a delta; a keyed presence with a turn that is yours, an unkeyed
//     one ending it; a name with the turn's names; a weblink with the
//     fixture's link.
//   /v1/live: the band list for watchBand, the room for watch.
// Every frame it hears is kept, by connection.
class FlowServer {
public:
    FlowServer()
        : m_server(QStringLiteral("flow-roundtable"), QWebSocketServer::NonSecureMode)
    {
        QObject::connect(&m_server, &QWebSocketServer::newConnection, &m_context, [this] { accept(); });
    }

    bool listen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    QUrl stationUrl() const
    {
        return QUrl(QStringLiteral("ws://127.0.0.1:%1/v1/station").arg(m_server.serverPort()));
    }

    std::vector<std::unique_ptr<FlowConnection>> connections;

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

    // The station connection that sent a presence and is still open.
    FlowConnection* main() const
    {
        for (auto it = connections.rbegin(); it != connections.rend(); ++it) {
            if ((*it)->path == QLatin1String("/v1/station") && !(*it)->closed && (*it)->sentPresence()) {
                return it->get();
            }
        }
        return nullptr;
    }

    FlowConnection* last(const QString& path) const
    {
        for (auto it = connections.rbegin(); it != connections.rend(); ++it) {
            if ((*it)->path == path) {
                return it->get();
            }
        }
        return nullptr;
    }

    // Every frame of `type` heard on any connection.
    QList<QJsonObject> heard(const QString& type) const
    {
        QList<QJsonObject> out;
        for (const auto& c : connections) {
            for (const QJsonObject& frame : c->frames) {
                if (frame.value(QStringLiteral("type")).toString() == type) {
                    out.append(frame);
                }
            }
        }
        return out;
    }

    // Whether any frame on any connection carries `text`.
    bool anyFrameContains(const QString& text) const
    {
        for (const auto& c : connections) {
            for (const QJsonObject& frame : c->frames) {
                if (QString::fromUtf8(QJsonDocument(frame).toJson(QJsonDocument::Compact)).contains(text)) {
                    return true;
                }
            }
        }
        return false;
    }

    // W4XYZ names this station in the room: the message in a delta, then
    // the `mention` (wire spec 6.10).
    bool mention()
    {
        FlowConnection* c = main();
        if (c == nullptr) {
            return false;
        }
        const QJsonObject mention = fixtureWire(QStringLiteral("server-station-mention"));
        c->sendDelta(kRoom, {}, {}, QJsonArray{mention.value(QStringLiteral("message"))});
        c->send(mention);
        return true;
    }

private:
    void accept()
    {
        while (m_server.hasPendingConnections()) {
            QWebSocket* socket = m_server.nextPendingConnection();
            socket->setParent(&m_context);
            auto connection = std::make_unique<FlowConnection>();
            FlowConnection* c = connection.get();
            c->socket = socket;
            c->path = socket->requestUrl().path();
            connections.push_back(std::move(connection));
            QObject::connect(socket, &QWebSocket::textMessageReceived, &m_context,
                             [this, c](const QString& text) { received(c, text); });
            QObject::connect(socket, &QWebSocket::disconnected, &m_context, [c] { c->closed = true; });
            if (c->path == QLatin1String("/v1/station")) {
                c->nonce.resize(32);
                QRandomGenerator::global()->fillRange(reinterpret_cast<quint32*>(c->nonce.data()), 8);
                c->send(QJsonObject{{QStringLiteral("type"), QStringLiteral("challenge")},
                                    {QStringLiteral("nonce"), Wire::toBase64Url(c->nonce)}});
            }
        }
    }

    static bool inRoom(const QJsonObject& slice)
    {
        return std::llabs(slice.value(QStringLiteral("hz")).toInteger() - 14212000LL) <= 1500
            && slice.value(QStringLiteral("mode")).toString().compare(QLatin1String("usb"), Qt::CaseInsensitive)
            == 0;
    }

    void received(FlowConnection* c, const QString& text)
    {
        const bool station = c->path == QLatin1String("/v1/station");
        const Wire::Decoded decoded = Wire::decode(
            text, station ? Wire::Direction::StationToServer : Wire::Direction::LiveToServer);
        if (!decoded.ok) {
            qWarning() << "flow server: refused" << decoded.error;
            return;
        }
        c->frames.append(decoded.message);
        const QString type = decoded.type;
        const QJsonObject& m = decoded.message;
        if (!station) {
            if (type == QLatin1String("watchBand")) {
                c->send(fixtureWire(QStringLiteral("server-live-rooms")));
            } else if (type == QLatin1String("watch") && m.value(QStringLiteral("room")).toString() == kRoom) {
                c->sendRoom(liveRoom());
            }
            return;
        }
        if (type == QLatin1String("hello")) {
            bool ok = false;
            const QByteArray spki = Wire::fromBase64Url(m.value(QStringLiteral("spki")).toString(), &ok);
            QJsonObject welcome = fixtureWire(QStringLiteral("server-station-welcome-core"));
            welcome.insert(QStringLiteral("station"), Wire::roundtableId(spki));
            c->welcomed = true;
            c->send(welcome);
            c->send(fixtureWire(QStringLiteral("server-station-rooms")));
        } else if (type == QLatin1String("presence")) {
            presence(c, m);
        } else if (type == QLatin1String("read")) {
            const QString room = m.value(QStringLiteral("room")).toString();
            if (room == kOtherRoom) {
                c->sendRoom(otherRoom());
            } else if (room == kRoom) {
                c->sendRoom(stationRoom());
            }
        } else if (type == QLatin1String("post")) {
            QJsonObject posted = fixtureWire(QStringLiteral("server-station-posted-ok"));
            posted.insert(QStringLiteral("clientId"), m.value(QStringLiteral("clientId")));
            posted.insert(QStringLiteral("msg"), kPostedId);
            c->send(posted);
            const QJsonObject message{{QStringLiteral("id"), kPostedId},
                                      {QStringLiteral("ref"), kYouRef},
                                      {QStringLiteral("callsign"), QStringLiteral("KG4VCF")},
                                      {QStringLiteral("nth"), 1},
                                      {QStringLiteral("t"), g_now},
                                      {QStringLiteral("state"), QStringLiteral("shown")},
                                      {QStringLiteral("text"), m.value(QStringLiteral("text"))},
                                      {QStringLiteral("dbm"), -91.5}};
            c->sendDelta(m.value(QStringLiteral("room")).toString(), {}, {}, QJsonArray{message});
        } else if (type == QLatin1String("name")) {
            QJsonObject turn = turnWithId(stationRoom(), m.value(QStringLiteral("turn")).toInteger());
            if (turn.isEmpty()) {
                return;
            }
            const QString name = m.value(QStringLiteral("name")).toString();
            turn.insert(QStringLiteral("names"),
                        QJsonArray{QJsonObject{{QStringLiteral("name"), name}, {QStringLiteral("count"), 4}},
                                   QJsonObject{{QStringLiteral("name"), QStringLiteral("W4XYC")},
                                               {QStringLiteral("count"), 1}}});
            turn.insert(QStringLiteral("mine"), name);
            c->sendDelta(kRoom, {}, QJsonArray{turn}, {});
        } else if (type == QLatin1String("weblink")) {
            c->send(fixtureWire(QStringLiteral("server-station-weblink-ok")));
        }
    }

    void presence(FlowConnection* c, const QJsonObject& m)
    {
        bool placed = false;
        bool keyed = false;
        for (const QJsonValue& value : m.value(QStringLiteral("slices")).toArray()) {
            const QJsonObject slice = value.toObject();
            if (inRoom(slice)) {
                placed = true;
                keyed = keyed || slice.value(QStringLiteral("keyed")).toBool();
            }
        }
        if (!placed) {
            return;
        }
        if (!c->gens.contains(kRoom)) {
            c->sendRoom(stationRoom());
        }
        if (keyed == c->keyed) {
            return;
        }
        c->keyed = keyed;
        const QJsonObject room = stationRoom();
        QJsonObject you = memberNamed(room, QStringLiteral("KG4VCF"));
        you.insert(QStringLiteral("keyed"), keyed);
        const QJsonObject youTalker{{QStringLiteral("ref"), kYouRef},
                                    {QStringLiteral("callsign"), QStringLiteral("KG4VCF")},
                                    {QStringLiteral("nth"), 1}};
        QJsonObject txTurn{{QStringLiteral("id"), kTxTurn},
                           {QStringLiteral("kind"), QStringLiteral("talking")},
                           {QStringLiteral("talkers"), QJsonArray{youTalker}},
                           {QStringLiteral("start"), g_now},
                           {QStringLiteral("end"), QJsonValue::Null},
                           {QStringLiteral("ending"), false},
                           {QStringLiteral("names"), QJsonArray{}},
                           {QStringLiteral("heard"), 1},
                           {QStringLiteral("mine"), QJsonValue::Null}};
        if (keyed) {
            // K4ABC stood by; the turn now is yours.
            QJsonObject k4abc = memberNamed(room, QStringLiteral("K4ABC"));
            k4abc.insert(QStringLiteral("keyed"), false);
            QJsonObject ended = turnWithId(room, 4410);
            ended.insert(QStringLiteral("end"), g_now);
            c->sendDelta(kRoom, QJsonArray{you, k4abc}, QJsonArray{ended, txTurn}, {});
        } else {
            txTurn.insert(QStringLiteral("start"), g_now - 3000);
            txTurn.insert(QStringLiteral("end"), g_now);
            c->sendDelta(kRoom, QJsonArray{you}, QJsonArray{txTurn}, {});
        }
    }

    QWebSocketServer m_server;
    // Declared last, destroyed first.
    QObject m_context;
};

// A second window of the same Core: a remote model over the loopback, with
// its own settings, signed in with the Core's token.
struct SecondWindow {
    QTemporaryDir dir;
    RadioModel model{RadioModel::Role::Remote};
    SettingsProxy proxy;
    std::unique_ptr<StationClient> client;
    std::unique_ptr<AppSettings> settings;

    SecondWindow(StationServer& server, QObject* owner)
    {
        settings = std::make_unique<AppSettings>(dir.filePath(QStringLiteral("window.settings")));
        settings->setRemoteBackend(&proxy);
        client = std::make_unique<StationClient>(&model, &proxy);
        auto* stationEnd = new LoopbackTransport(QStringLiteral("second-station"), owner);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("second-client"), owner);
        stationEnd->linkTo(clientEnd);
        client->startSession(clientEnd, server.token());
        server.acceptTransport(stationEnd);
    }

    ~SecondWindow() { settings->setRemoteBackend(nullptr); }

    RoundtableModel* rooms() const { return model.roundtableModel(); }
};

// Where the flow runs: the window the operator uses, the Core whose
// client talks to Roundtable, and the Core's station server.
class Placement {
public:
    virtual ~Placement() = default;
    virtual MainWindow* window() const = 0;
    virtual RadioModel* core() = 0;
    virtual StationServer& server() = 0;
    virtual int sliceId() const = 0;
    RadioModel* model() const { return window()->radioModel(); }
    RoundtableClient* client() { return core()->roundtableClient(); }
};

class Everything final : public Placement {
public:
    Everything()
    {
        AppSettings::instance().setValue(QStringLiteral("PanLayoutId"), QStringLiteral("1"));
        m_window = std::make_unique<MainWindow>(RemoteStationOptions{}, nullptr,
                                                MainWindow::ConnectionStartup::Deferred);
        RadioModel* m = m_window->radioModel();
        if (m->roundtableClient() != nullptr) {
            m->roundtableClient()->setServerUrl(QUrl(QStringLiteral("ws://127.0.0.1:9/v1/station")));
        }
        m->setBoardForTest(HPSDRHW::Saturn);
        m->configureStreamPool(5, 5, 192000);
        m->setConnectionStateForTest(ConnectionState::Connected);
        m_slice = m->addSlice(QStringLiteral("pan-0"));
        m_window->resize(1512, 869);
        m_window->show();
        QVERIFY(m_tokens.isValid());
        m_server = std::make_unique<StationServer>(m, AppSettings::instance(),
                                                   NereusSDR::Test::seedUpgradedCoreToken(m_tokens.path()));
    }

    MainWindow* window() const override { return m_window.get(); }
    RadioModel* core() override { return m_window->radioModel(); }
    StationServer& server() override { return *m_server; }
    int sliceId() const override { return m_slice; }

private:
    std::unique_ptr<MainWindow> m_window;
    QTemporaryDir m_tokens;
    std::unique_ptr<StationServer> m_server;
    int m_slice = -1;
};

class WindowOfCore final : public Placement {
public:
    WindowOfCore()
    {
        m_harness = std::make_unique<RemoteWindowHarness>();
        RadioModel& station = m_harness->station();
        station.roundtableClient()->setServerUrl(QUrl(QStringLiteral("ws://127.0.0.1:9/v1/station")));
        m_harness->stationSettings().setValue(QStringLiteral("Roundtable/AutoStart"), QStringLiteral("False"));
        m_ok = m_harness->start();
        if (m_ok) {
            m_harness->window()->resize(1512, 869);
            m_harness->startStartupConnection();
        }
    }

    bool ok() const { return m_ok; }
    RemoteWindowHarness& harness() { return *m_harness; }
    MainWindow* window() const override { return m_harness->window(); }
    RadioModel* core() override { return &m_harness->station(); }
    StationServer& server() override { return m_harness->server(); }
    int sliceId() const override { return 0; }

private:
    std::unique_ptr<RemoteWindowHarness> m_harness;
    bool m_ok = false;
};

// A Core that reads any frequency for this window.
class ReadingLink : public IStationLink {
public:
    QStringList reads;
    bool refuse = false;
    bool roundtableAvailable() const override { return true; }
    bool roundtableFrequencyAvailable() const override { return true; }
    CommandOutcome requestAddSlice(const QString&) override { return {}; }
    CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
    CommandOutcome requestRemoveSlice(int) override { return {}; }
    CommandOutcome requestActiveSlice(int) override { return {}; }
    CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
    CommandOutcome requestRoundtableReadAt(qint64 hz, const QString& family) override
    {
        reads.append(QStringLiteral("%1-%2").arg(family).arg(hz));
        if (refuse) { return {false, QStringLiteral("Not now")}; }
        return {true, {}};
    }
    CommandOutcome requestRoundtableUnreadAt(qint64, const QString&) override { return {true, {}}; }
};

void settle()
{
    for (int i = 0; i < 6; ++i) {
        QCoreApplication::processEvents();
    }
}

RoundtableApplet* appletOf(MainWindow* window)
{
    return window->findChild<RoundtableApplet*>();
}

VfoWidget* flagFor(MainWindow* window, int sliceId)
{
    for (VfoWidget* flag : window->findChildren<VfoWidget*>()) {
        if (flag->sliceIndex() == sliceId) {
            return flag;
        }
    }
    return nullptr;
}

SpectrumWidget* panOf(MainWindow* window)
{
    for (PanadapterApplet* applet : window->findChildren<PanadapterApplet*>()) {
        if (applet->panId() == QLatin1String("pan-0")) {
            return applet->spectrumWidget();
        }
    }
    return nullptr;
}

template <typename T>
T* visibleChild(QWidget* root, const QString& name)
{
    for (T* w : root->findChildren<T*>(name)) {
        if (w->isVisibleTo(root)) {
            return w;
        }
    }
    return nullptr;
}

QStringList visibleTexts(QWidget* root, const QString& name)
{
    QStringList out;
    for (QLabel* label : root->findChildren<QLabel*>(name)) {
        if (label->isVisibleTo(root)) {
            out.append(label->text());
        }
    }
    return out;
}

QMenu* openMenu(const QString& name)
{
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (auto* menu = qobject_cast<QMenu*>(w); menu != nullptr && menu->objectName() == name
                                                  && menu->isVisible()) {
            return menu;
        }
    }
    return nullptr;
}

// Chooses `text` from the panel's menu.
bool choosePanelMenu(RoundtablePanel* panel, const QString& text)
{
    QAbstractButton* any = visibleChild<QAbstractButton>(panel, QStringLiteral("roundtableMenuBtn"));
    if (any == nullptr) {
        return false;
    }
    any->click();
    settle();
    QMenu* menu = openMenu(QStringLiteral("roundtablePanelMenu"));
    if (menu == nullptr) {
        return false;
    }
    for (QAction* action : menu->actions()) {
        if (action->text() == text) {
            menu->close();
            action->trigger();
            settle();
            return true;
        }
    }
    menu->close();
    return false;
}

// Every visible widget under `root` that ends past its parent's right edge.
QStringList widthOverruns(QWidget* root)
{
    QStringList out;
    if (root == nullptr || !root->isVisible()) {
        return out;
    }
    for (QWidget* w : root->findChildren<QWidget*>()) {
        QWidget* parent = w->parentWidget();
        if (parent == nullptr || w->isWindow() || !w->isVisibleTo(root) || qobject_cast<QScrollBar*>(w)) {
            continue;
        }
        if (w->x() < 0 || w->x() + w->width() > parent->width()) {
            out.append(QStringLiteral("%1 %2 spans %3..%4 in %5 %6 %7 wide")
                           .arg(QString::fromLatin1(w->metaObject()->className()), w->objectName())
                           .arg(w->x())
                           .arg(w->x() + w->width())
                           .arg(QString::fromLatin1(parent->metaObject()->className()), parent->objectName())
                           .arg(parent->width()));
            // What asks for the width, for the failure message.
            QList<std::pair<int, QString>> asking;
            for (QWidget* inner : w->findChildren<QWidget*>()) {
                if (!inner->isVisibleTo(w)) {
                    continue;
                }
                const int hint = std::max({inner->minimumWidth(), inner->minimumSizeHint().width(),
                                           inner->layout() != nullptr ? inner->layout()->minimumSize().width() : 0});
                if (hint > parent->width()) {
                    asking.append({hint, QStringLiteral("%1 %2 asks %3")
                                             .arg(QString::fromLatin1(inner->metaObject()->className()),
                                                  inner->objectName())
                                             .arg(hint)});
                }
            }
            std::sort(asking.begin(), asking.end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });
            for (qsizetype i = 0; i < std::min<qsizetype>(asking.size(), 6); ++i) {
                out.append(QStringLiteral("  ") + asking.at(i).second);
            }
        }
    }
    return out;
}

// What the operator sees: the flag's Roundtable line, the pan's tags and
// every Roundtable label, button and box on the panel, top to bottom.
QStringList screenOf(MainWindow* window, int sliceId)
{
    QStringList out;
    if (VfoWidget* flag = flagFor(window, sliceId)) {
        const RoundtableFlagLine line = flag->roundtableLine();
        out.append(QStringLiteral("flag: %1 %2 glyph %3 @%4")
                       .arg(line.visible ? QStringLiteral("shown") : QStringLiteral("hidden"), line.text)
                       .arg(line.glyph)
                       .arg(line.mentions));
    }
    if (SpectrumWidget* pan = panOf(window)) {
        for (const RoundtableTag& tag : pan->roundtableTags()) {
            out.append(QStringLiteral("tag: %1 %2 state %3 members %4%5%6")
                           .arg(tag.roomId, tag.label)
                           .arg(tag.state)
                           .arg(tag.members)
                           .arg(tag.mine ? QStringLiteral(" mine") : QString(),
                                tag.mentionsMe ? QStringLiteral(" names you") : QString()));
        }
        out.append(QStringLiteral("tags shown as %1").arg(static_cast<int>(pan->roundtableTagDisplay())));
    }
    RoundtableApplet* applet = appletOf(window);
    RoundtablePanel* panel = applet != nullptr ? applet->panel() : nullptr;
    if (panel == nullptr) {
        return out;
    }
    struct Item {
        QPoint at;
        QString line;
    };
    QList<Item> items;
    for (QWidget* w : panel->findChildren<QWidget*>()) {
        if (!w->objectName().startsWith(QLatin1String("roundtable")) || !w->isVisibleTo(panel)) {
            continue;
        }
        QString text;
        if (auto* label = qobject_cast<QLabel*>(w)) {
            text = label->text();
        } else if (auto* button = qobject_cast<QAbstractButton*>(w)) {
            text = button->text() + (button->isEnabled() ? QString() : QStringLiteral(" [off]"));
        } else if (auto* box = qobject_cast<QPlainTextEdit*>(w)) {
            text = box->toPlainText() + QStringLiteral(" | ") + box->placeholderText()
                + (box->isEnabled() && !box->isReadOnly() ? QString() : QStringLiteral(" [off]"));
        } else if (auto* edit = qobject_cast<QLineEdit*>(w)) {
            text = edit->text() + QStringLiteral(" | ") + edit->placeholderText();
        } else {
            continue;
        }
        items.append({w->mapTo(panel, QPoint(0, 0)), w->objectName() + QStringLiteral(": ") + text});
    }
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.at.y() != b.at.y()) {
            return a.at.y() < b.at.y();
        }
        if (a.at.x() != b.at.x()) {
            return a.at.x() < b.at.x();
        }
        return a.line < b.line;
    });
    for (const Item& item : items) {
        out.append(item.line);
    }
    return out;
}

struct Shot {
    QString step;
    QStringList screen;
};

} // namespace

class TstRoundtableDesktopFlow : public QObject {
    Q_OBJECT

private:
    QList<Shot> m_everything;
    QStringList m_overruns;
    bool m_everythingDone = false;

    // After each step: nothing runs past its container, and what shows.
    void take(Placement& p, const QString& step, QList<Shot>& shots)
    {
        settle();
        RoundtableApplet* applet = appletOf(p.window());
        QVERIFY(applet != nullptr);
        QWidget* panelWindow = applet->isPoppedOut() ? applet->floatingWindow() : nullptr;
        QStringList overruns = widthOverruns(applet->panel());
        overruns += widthOverruns(panelWindow);
        if (VfoWidget* flag = flagFor(p.window(), p.sliceId())) {
            overruns += widthOverruns(flag);
        }
        // Collected, and checked at the end of the flow, so one overrun
        // does not hide what the later steps show.
        if (!overruns.isEmpty()) {
            m_overruns.append(step + QStringLiteral(": ") + overruns.join(QStringLiteral("; ")));
        }
        shots.append({step, screenOf(p.window(), p.sliceId())});
        const QString directory = qEnvironmentVariable("NEREUS_ROUNDTABLE_CAPTURE_DIR");
        if (!directory.isEmpty()) {
            QVERIFY(QDir().mkpath(directory));
            QString name = QString::fromLatin1(QTest::currentTestFunction()) + QLatin1Char('-') + step;
            name.replace(QLatin1Char(' '), QLatin1Char('-'));
            name.replace(QLatin1Char('/'), QLatin1Char('-'));
            const QString path = directory + QLatin1Char('/') + name;
            QVERIFY(p.window()->grab().save(path + QStringLiteral("-window.png")));
            QVERIFY(applet->panel()->grab().save(path + QStringLiteral("-panel.png")));
            if (panelWindow != nullptr) {
                QVERIFY(panelWindow->grab().save(path + QStringLiteral("-floating.png")));
            }
        }
    }

    void runFlow(Placement& p, QList<Shot>& shots)
    {
        FlowServer server;
        QVERIFY(server.listen());
        RoundtableClient* client = p.client();
        QVERIFY(client != nullptr);
        client->setServerUrl(server.stationUrl());
        QCOMPARE(client->linkState(), RoundtableClient::LinkState::Off);
        MainWindow* window = p.window();
        RoundtableApplet* applet = appletOf(window);
        QVERIFY(applet != nullptr);
        QUrl opened;
        applet->setUrlOpener([&opened](const QUrl& url) { opened = url; });
        RoundtableModel* rooms = p.model()->roundtableModel();
        QVERIFY(rooms != nullptr);

        // A second window of the same Core, for the mention read on both.
        SecondWindow second(p.server(), this);
        QTRY_VERIFY_WITH_TIMEOUT(second.client->isHandshakeComplete(), kWaitMs);

        // Roundtable starts as the Spot Hub's Start does, on the Core.
        QString reason;
        QVERIFY2(p.core()->spotSourceHost()->connectSource(SpotSourceHost::kRoundtable, &reason),
                 qPrintable(reason));
        QTRY_VERIFY_WITH_TIMEOUT(server.count(QStringLiteral("/v1/live")) >= 1, kWaitMs);

        // ── First run: the panel opens from the flag; nothing is sent ──
        VfoWidget* flag = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((flag = flagFor(window, p.sliceId())) != nullptr
                                     && flag->roundtableLine().visible,
                                 kWaitMs);
        QVERIFY(flag->roundtableLineButtonForTest() != nullptr);
        flag->roundtableLineButtonForTest()->click();
        settle();
        QVERIFY(applet->isVisible());
        RoundtableFirstRunDialog* firstRun = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((firstRun = window->findChild<RoundtableFirstRunDialog*>()) != nullptr
                                     && firstRun->isVisible(),
                                 kWaitMs);
        QCOMPARE(server.count(QStringLiteral("/v1/station")), 0);
        QVERIFY(!server.anyFrameContains(QStringLiteral("KG4VCF")));
        // The rooms on the band reach a window of the Core a moment later
        // than the Core's own: the shot waits for both rooms and their tags.
        QTRY_COMPARE_WITH_TIMEOUT(rooms->rooms().size(), qsizetype(2), kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(panOf(window) != nullptr && panOf(window)->roundtableTags().size() == 2, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(applet->findChildren<QPushButton*>(QStringLiteral("roundtableEmptyRoom")).size(),
                                  qsizetype(2), kWaitMs);
        take(p, QStringLiteral("first run"), shots);

        // ── Go live ──
        auto* goLive = firstRun->findChild<QAbstractButton*>(QStringLiteral("roundtableFirstRunGoLive"));
        QVERIFY(goLive != nullptr);
        goLive->click();
        QTRY_VERIFY_WITH_TIMEOUT(server.main() != nullptr, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(rooms->linkState(), RoundtableModel::LinkState::Live, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(rooms->rooms().size(), 3, kWaitMs);
        QCOMPARE(server.heard(QStringLiteral("presence")).last().value(QStringLiteral("callsign")).toString(),
                 QStringLiteral("KG4VCF"));
        take(p, QStringLiteral("live"), shots);

        // ── Tune into the room by its tag: this is a legacy server (D21) ──
        SpectrumWidget* pan = panOf(window);
        QVERIFY(pan != nullptr);
        pan->setFrequencyRange(kStartHz, kPanSpanHz);
        QTRY_VERIFY_WITH_TIMEOUT(std::any_of(pan->roundtableTags().cbegin(), pan->roundtableTags().cend(),
                                             [](const RoundtableTag& t) { return t.roomId == kRoom; }),
                                 kWaitMs);
        {
            QImage image(pan->size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::black);
            QPainter painter(&image);
            pan->drawRoundtableTagsForTest(painter, pan->roundtableTagSpectrumRectForTest());
        }
        QRect tagRect;
        for (const SpectrumWidget::RoundtableTagHit& hit : pan->roundtableTagHitsForTest()) {
            if (!hit.folded && hit.roomIds.contains(kRoom)) {
                tagRect = hit.rect;
            }
        }
        QVERIFY(tagRect.isValid());
        QTest::mouseClick(pan, Qt::LeftButton, Qt::NoModifier, tagRect.center());
        SliceModel* slice = p.model()->sliceById(p.sliceId());
        QVERIFY(slice != nullptr);
        QTRY_COMPARE_WITH_TIMEOUT(std::llround(slice->frequency()), 14212000LL, kWaitMs);
        RoundtablePanel* panel = applet->panel();
        QTRY_VERIFY2_WITH_TIMEOUT(panel->roomId() == kRoom,
                                  qPrintable(QStringLiteral("panel follows %1 in %2; the Core places %3; this "
                                                            "window places %4")
                                                 .arg(panel->followedSlice())
                                                 .arg(panel->roomId(),
                                                      p.core()->roundtableState()->sliceRooms(),
                                                      rooms->sliceRoomId(p.sliceId()))
                                             + QStringLiteral(" (window state %4) (%1 x %2, panel %3)")
                                                   .arg(applet->width())
                                                   .arg(applet->height())
                                                   .arg(panel->width())
                                                   .arg(p.model()->roundtableState() != nullptr
                                                            ? p.model()->roundtableState()->sliceRooms()
                                                            : QStringLiteral("none"))),
                                  kWaitMs);
        // The slice's room reaches the window that tuned it. In a window of
        // the Core the tune is that window's own write, and the Core places
        // the room a turn of the event loop after applying it
        // (RadioModel::pushRoundtableSlices), so the mirror does not
        // withhold it from the writer.
        QTRY_COMPARE_WITH_TIMEOUT(rooms->sliceRoomId(p.sliceId()), kRoom, kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(!rooms->room(kRoom).isEmpty(), kWaitMs);
        take(p, QStringLiteral("tuned into the room"), shots);

        // ── The readings ──
        QTRY_VERIFY_WITH_TIMEOUT(!visibleTexts(panel, QStringLiteral("roundtableReadingCall")).isEmpty(), kWaitMs);
        const QStringList calls = visibleTexts(panel, QStringLiteral("roundtableReadingCall"));
        QVERIFY2(calls.join(QLatin1Char(' ')).contains(QStringLiteral("KG4VCF")), qPrintable(calls.join(' ')));
        QVERIFY(!visibleTexts(panel, QStringLiteral("roundtableReadingValue")).isEmpty());
        QVERIFY(visibleChild<QLabel>(panel, QStringLiteral("roundtableNotSentTag")) == nullptr);
        take(p, QStringLiteral("readings"), shots);

        // ── Post a message ──
        auto* box = visibleChild<QPlainTextEdit>(panel, QStringLiteral("roundtableBox"));
        auto* send = visibleChild<QPushButton>(panel, QStringLiteral("roundtableSendBtn"));
        QVERIFY(box != nullptr && send != nullptr);
        box->setFocus();
        box->setPlainText(kPosted);
        QTRY_VERIFY_WITH_TIMEOUT(send->isEnabled(), kWaitMs);
        QString postAnswer;
        const QMetaObject::Connection answers = connect(
            rooms, &RoundtableModel::postResult, this,
            [&postAnswer](const QString&, bool ok, const QString& why) {
                postAnswer = (ok ? QStringLiteral("posted ") : QStringLiteral("refused ")) + why;
            });
        QSignalSpy clicks(send, &QPushButton::clicked);
        send->click();
        QTRY_VERIFY2_WITH_TIMEOUT(server.heard(QStringLiteral("post")).size() == 1,
                                  qPrintable(QStringLiteral("answer '%1', %2 clicks, enabled %3, room %4, box '%5', "
                                                            "client link %6, rooms model %7 panel model %8")
                                                 .arg(postAnswer).arg(clicks.size()).arg(send->isEnabled())
                                                 .arg(panel->roomId(), box->toPlainText())
                                                 .arg(static_cast<int>(client->linkState()))
                                                 .arg(reinterpret_cast<quintptr>(rooms))
                                                 .arg(reinterpret_cast<quintptr>(applet->panel()->findChild<QObject*>()))),
                                  kWaitMs);
        disconnect(answers);
        QCOMPARE(server.heard(QStringLiteral("post")).first().value(QStringLiteral("text")).toString(), kPosted);
        QTRY_VERIFY_WITH_TIMEOUT(visibleTexts(panel, QStringLiteral("roundtableMessageText")).join('\n').contains(kPosted),
                                 kWaitMs);
        QVERIFY(visibleChild<QWidget>(panel, QStringLiteral("roundtableNotSent")) == nullptr);
        QVERIFY(box->toPlainText().isEmpty());
        take(p, QStringLiteral("posted"), shots);

        // ── A mention: the flag's @1 and the pill, read after 2 s on screen,
        // and read on the second window too ──
        QVERIFY(server.mention());
        QTRY_COMPARE_WITH_TIMEOUT(flag->roundtableLine().mentions, 1, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(second.rooms()->mentionCount(kRoom), 1, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(rooms->room(kRoom).value(QStringLiteral("messages")).toArray().size(), 4, kWaitMs);
        settle();
        auto* pill = panel->findChild<QPushButton*>(QStringLiteral("roundtableMentionPill"));
        QVERIFY(pill != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(pill->isVisibleTo(panel), kWaitMs);
        QVERIFY2(pill->text().startsWith(QStringLiteral("@1 ")), qPrintable(pill->text()));
        take(p, QStringLiteral("mention"), shots);
        pill->click();
        settle();
        panel->refreshNow();
        g_now += RoundtablePanel::kOnScreenMs + 500;
        panel->refreshNow();
        QTRY_COMPARE_WITH_TIMEOUT(flag->roundtableLine().mentions, 0, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(second.rooms()->mentionCount(kRoom), 0, kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(!server.heard(QStringLiteral("mentionsSeen")).isEmpty(), kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(!pill->isVisibleTo(panel), kWaitMs);
        take(p, QStringLiteral("mention read"), shots);

        // ── Read another room from the pop-out's list ──
        auto* pop = visibleChild<QAbstractButton>(panel, QStringLiteral("roundtablePopBtn"));
        QVERIFY(pop != nullptr);
        pop->click();
        QTRY_VERIFY_WITH_TIMEOUT(applet->isPoppedOut() && applet->floatingWindow()->isVisible(), kWaitMs);
        RoundtableWindow* popped = applet->floatingWindow();
        auto* list = popped->findChild<QListWidget*>(QStringLiteral("roundtableRoomList"));
        QVERIFY(list != nullptr);
        QListWidgetItem* other = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT(
            [&] {
                for (int i = 0; i < list->count(); ++i) {
                    if (list->item(i)->text().startsWith(QStringLiteral("14.074"))) {
                        other = list->item(i);
                        return true;
                    }
                }
                return false;
            }(),
            kWaitMs);
        list->scrollToItem(other);
        settle();
        QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier, list->visualItemRect(other).center());
        QTRY_COMPARE_WITH_TIMEOUT(panel->roomId(), kOtherRoom, kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(!rooms->room(kOtherRoom).isEmpty(), kWaitMs);
        QVERIFY(panel->isReading());
        QTRY_VERIFY_WITH_TIMEOUT(visibleChild<QWidget>(panel, QStringLiteral("roundtableReadBar")) != nullptr,
                                 kWaitMs);
        QCOMPARE(std::llround(slice->frequency()), 14212000LL);
        take(p, QStringLiteral("reading another room"), shots);

        // ── Back to my slice ──
        auto* back = visibleChild<QAbstractButton>(panel, QStringLiteral("roundtableBackBtn"));
        QVERIFY(back != nullptr);
        back->click();
        QTRY_COMPARE_WITH_TIMEOUT(panel->roomId(), kRoom, kWaitMs);
        QVERIFY(!panel->isReading());
        take(p, QStringLiteral("back to my slice"), shots);

        // ── Key: the TX line; unkey ──
        MoxController* mox = p.core()->moxController();
        QVERIFY(mox != nullptr);
        // A station server's Core is receive only until it allows
        // transmit (its remote_transmit setting).
        p.server().setRemoteTransmitAllowed(true);
        // The radio's own mic, since no PC microphone opens offscreen.
        p.core()->transmitModel().setMicSource(MicSource::Radio);
        QSignalSpy refusals(mox, &MoxController::moxRejected);
        mox->setMox(true);
        QTRY_VERIFY2_WITH_TIMEOUT(mox->isMox(),
                                  qPrintable(refusals.isEmpty() ? QStringLiteral("no refusal")
                                                                : refusals.first().first().toString()),
                                  kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(visibleChild<QWidget>(panel, QStringLiteral("roundtableTxText")) != nullptr,
                                 kWaitMs);
        take(p, QStringLiteral("keyed"), shots);
        g_now += 3000;
        mox->setMox(false);
        QTRY_VERIFY_WITH_TIMEOUT(!mox->isMox(), kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(visibleChild<QWidget>(panel, QStringLiteral("roundtableTxText")) == nullptr,
                                 kWaitMs);
        take(p, QStringLiteral("unkeyed"), shots);

        // ── Name the unknown turn ──
        QAbstractButton* name = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT((name = visibleChild<QAbstractButton>(panel, QStringLiteral("roundtableTurnNameBtn")))
                                         != nullptr
                                     && name->isEnabled(),
                                 kWaitMs);
        name->click();
        settle();
        auto* input = visibleChild<QLineEdit>(panel, QStringLiteral("roundtableNameInput"));
        QVERIFY(input != nullptr);
        input->setFocus();
        QTest::keyClicks(input, QStringLiteral("W4XYZ"));
        QTest::keyClick(input, Qt::Key_Return);
        QTRY_COMPARE_WITH_TIMEOUT(server.heard(QStringLiteral("name")).size(), 1, kWaitMs);
        QCOMPARE(server.heard(QStringLiteral("name")).first().value(QStringLiteral("turn")).toInteger(),
                 kUnknownTurn);
        // The server's count for the name (4), not only the name: a window
        // of the Core shows the room's answer a turn of the link later.
        QTRY_VERIFY_WITH_TIMEOUT(visibleTexts(panel, QStringLiteral("roundtableTurnNames")).join(' ').contains(
                                     QStringLiteral("W4XYZ (4)")),
                                 kWaitMs);
        take(p, QStringLiteral("turn named"), shots);

        // ── Go hidden, from Roundtable's settings ──
        QVERIFY(choosePanelMenu(panel, QStringLiteral("Settings")));
        QCheckBox* hide = nullptr;
        QTRY_VERIFY_WITH_TIMEOUT(
            [&] {
                for (QWidget* top : QApplication::topLevelWidgets()) {
                    if (auto* box = top->findChild<QCheckBox*>(QStringLiteral("roundtableHideChk"));
                        box != nullptr && box->isVisible()) {
                        hide = box;
                        return true;
                    }
                }
                return false;
            }(),
            kWaitMs);
        QVERIFY(hide->isEnabled());
        hide->click();
        QTRY_VERIFY_WITH_TIMEOUT(rooms->hidden(), kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(rooms->linkState(), RoundtableModel::LinkState::Hidden, kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(!server.heard(QStringLiteral("hidden")).isEmpty(), kWaitMs);
        hide->window()->close();
        settle();
        // Your own meter, shown only here.
        SliceModel* coreSlice = p.core()->sliceById(p.sliceId());
        QVERIFY(coreSlice != nullptr);
        // A Core with no radio clears every slice's readings each tick
        // (SliceMeterPump::poll, not Connected), and a clear now reaches
        // the panel. The pump stops here so the reading holds as a steady
        // signal's would, on the Core and in a window of it alike.
        SliceMeterPump* pump = p.core()->sliceMeterPump();
        if (pump != nullptr) {
            pump->stop();
        }
        coreSlice->setSignalStrengthDbm(-73.0);
        coreSlice->setMeterReadingNs(coreSlice->meterReadingNs() + 1000000);
        QTRY_VERIFY_WITH_TIMEOUT(visibleChild<QLabel>(panel, QStringLiteral("roundtableNotSentTag")) != nullptr,
                                 kWaitMs);
        QCOMPARE(visibleChild<QLabel>(panel, QStringLiteral("roundtableNotSentTag"))->text(),
                 QStringLiteral("NOT SENT"));
        // The reading just taken (S9, 0 s), not an earlier one: a window of
        // the Core gets the meter a turn of the link later, and the Core's
        // own meter pump may write over one injected reading before the
        // link carries it, so the meter holds -73 dBm as a steady signal
        // would until the panel shows it.
        const bool freshOwn = QTest::qWaitFor(
            [&]() {
                if (visibleTexts(panel, QStringLiteral("roundtableReadingValue")).contains(QStringLiteral("S9"))
                    && visibleTexts(panel, QStringLiteral("roundtableReadingAge")).contains(QStringLiteral("0 s"))) {
                    return true;
                }
                coreSlice->setSignalStrengthDbm(-73.0);
                coreSlice->setMeterReadingNs(coreSlice->meterReadingNs() + 1000000);
                return false;
            },
            kWaitMs);
        QVERIFY2(freshOwn, qPrintable(visibleTexts(panel, QStringLiteral("roundtableReadingValue")).join(' ')));
        QTRY_VERIFY_WITH_TIMEOUT(visibleChild<QLabel>(panel, QStringLiteral("roundtableConnectionLine")) != nullptr
                                     && visibleChild<QLabel>(panel, QStringLiteral("roundtableConnectionLine"))
                                            ->text()
                                            .contains(QStringLiteral("You are hidden")),
                                 kWaitMs);
        send = panel->findChild<QPushButton*>(QStringLiteral("roundtableSendBtn"));
        QVERIFY(send != nullptr);
        QVERIFY(!send->isEnabled());
        QCOMPARE(visibleTexts(panel, QStringLiteral("roundtableHint")),
                 QStringList{QStringLiteral("You are hidden, so nothing you type is sent.")});
        take(p, QStringLiteral("hidden"), shots);

        // ── The Archive asks for its one-time link ──
        const int stationConnections = server.count(QStringLiteral("/v1/station"));
        QVERIFY(choosePanelMenu(panel, QStringLiteral("Archive")));
        QTRY_COMPARE_WITH_TIMEOUT(opened, QUrl(fixtureWire(QStringLiteral("server-station-weblink-ok"))
                                                   .value(QStringLiteral("url"))
                                                   .toString()),
                                  kWaitMs);
        QCOMPARE(server.count(QStringLiteral("/v1/station")), stationConnections + 1);
        QCOMPARE(server.last(QStringLiteral("/v1/station"))->types(),
                 (QStringList{QStringLiteral("hello"), QStringLiteral("weblink")}));
        take(p, QStringLiteral("archive"), shots);
        if (pump != nullptr) {
            pump->start();
        }

        // Every word the panel showed is plain.
        for (const Shot& shot : std::as_const(shots)) {
            for (const QString& line : shot.screen) {
                const QString words = line.section(QStringLiteral(": "), 1);
                QVERIFY2(!words.contains(QStringLiteral(" yet")), qPrintable(shot.step + ": " + line));
            }
        }

        if (applet->isPoppedOut()) {
            applet->dockFloatingWindow();
        }
        client->stop();
        settle();
    }

private slots:
    void initTestCase()
    {
        g_previousHandler = qInstallMessageHandler(quietPlatformNotes);
        QLoggingCategory::setFilterRules(QStringLiteral("nereus.*.debug=false\nnereus.*.info=false"));
        QVERIFY(RemoteWindowHarness::useIsolatedProfile(QStringLiteral("roundtable-desktop-flow")));
        RoundtableModel::setClockForTest([] { return g_now; });
    }

    void init()
    {
        QVERIFY(RemoteWindowHarness::clearIsolatedProfile());
        g_now = kStartMs;
        m_overruns.clear();
        AppSettings& s = AppSettings::instance();
        s.setValue(QStringLiteral("Roundtable/AutoStart"), QStringLiteral("False"));
        s.setValue(QStringLiteral("Roundtable/Callsign"), QStringLiteral("KG4VCF"));
    }

    void cleanupTestCase()
    {
        RoundtableModel::setClockForTest({});
        QVERIFY(RemoteWindowHarness::removeIsolatedProfile());
        qInstallMessageHandler(g_previousHandler);
    }

    void frequencyTagsPresentStructureImmediatelyAndCoalesceSampleBursts()
    {
        // A controlled record source keeps this presentation test independent
        // of a remote handshake's later initial empty-stream reset. The
        // existing placement flow covers the real remote record transport.
        class FrequencyLink : public IStationLink {
        public:
            bool roundtableAvailable() const override { return true; }
            bool roundtableFrequencyAvailable() const override { return true; }
            CommandOutcome requestAddSlice(const QString&) override { return {}; }
            CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
            CommandOutcome requestRemoveSlice(int) override { return {}; }
            CommandOutcome requestActiveSlice(int) override { return {}; }
            CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
            CommandOutcome requestRoundtableReadAt(qint64, const QString&) override { return {true, {}}; }
            CommandOutcome requestRoundtableUnreadAt(qint64, const QString&) override { return {true, {}}; }
        } link;
        RoundtableState state;
        Everything p;
        SliceModel* slice = p.model()->sliceById(p.sliceId());
        QVERIFY(slice);
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14200000);
        RoundtableModel* model = p.model()->roundtableModel();
        QVERIFY(model);
        model->attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        const QJsonObject actor{{"sourceRoom", "usb-14200000"}, {"ref", "tx"}, {"callsign", "W1AW"},
            {"hz", 14200000}, {"family", "usb"}, {"keyed", true}};
        QJsonObject reporter{{"sourceRoom", "usb-14200000"}, {"ref", "rx"}, {"callsign", "K4ABC"},
            {"hz", 14200000}, {"family", "usb"}, {"keyed", false},
            {"reading", QJsonObject{{"turn", 1}, {"present", true}, {"dbm", -73}, {"t", g_now}}}};
        model->applyRooms({{"frequencyVersion", 1}, {"rooms", QJsonArray{
            QJsonObject{{"id", "usb-14200000"}, {"stations", QJsonArray{actor, reporter}}}}}});
        QVERIFY(model->frequencyAvailable());
        SpectrumWidget* pan = panOf(p.window());
        QVERIFY(pan);
        const auto shown = [pan]() {
            QStringList rows;
            for (const RoundtableTag& tag : pan->roundtableTags()) { rows.append(tag.reportLines); }
            return rows.join(QLatin1Char(' '));
        };
        QJsonObject turn{{"id", 1}, {"kind", "talking"}, {"sourceRoom", "usb-14200000"}, {"hz", 14200000},
            {"start", g_now - 1000}, {"end", QJsonValue::Null}, {"talkers", QJsonArray{actor}}};
        QJsonObject view{{"id", "usb-14200000"}, {"view", "frequency"}, {"family", "usb"}, {"hz", 14200000},
            {"now", QJsonArray{turn}}, {"members", QJsonArray{actor, reporter}}};
        model->applyRoom(view);
        QVERIFY2(shown().contains(QStringLiteral("Talking: W1AW")), qPrintable(shown()));
        // Drain the initial structural flag/layout changes before measuring
        // report-only texture invalidation. Those geometry changes correctly
        // dirty the static marker cache independently of Roundtable samples.
        settle();
        turn.insert("start", g_now);
        view.insert("now", QJsonArray{turn}); model->applyRoom(view);
        auto* ageTimer = p.window()->findChild<QTimer*>(QStringLiteral("roundtableReportAgeTimer"));
        QVERIFY(ageTimer);
        QVERIFY(QMetaObject::invokeMethod(ageTimer, "timeout", Qt::DirectConnection));
        const auto report = [&](int dbm) {
            QJsonObject sample = reporter.value("reading").toObject(); sample.insert("dbm", dbm);
            reporter.insert("reading", sample); view.insert("members", QJsonArray{actor, reporter}); model->applyRoom(view);
        };
        report(-74);
        QVERIFY2(shown().contains(QStringLiteral("-74")), qPrintable(shown()));
        pan->clearOverlayStaticDirtyForTest();
        class PresentationCacheProbe : public QObject {
        public:
            SpectrumWidget* pan = nullptr;
            bool observed = false;
            bool dirtied = false;
            bool eventFilter(QObject*, QEvent* event) override {
                if (event->type() == QEvent::Timer) { pan->clearOverlayStaticDirtyForTest(); }
                return false;
            }
        } cache;
        cache.pan = pan;
        auto* coalescer = p.window()->findChild<QTimer*>(QStringLiteral("roundtableReportCoalescer"));
        QVERIFY(coalescer);
        coalescer->installEventFilter(&cache);
        connect(coalescer, &QTimer::timeout, &cache, [&] {
            cache.observed = true;
            cache.dirtied = pan->overlayStaticDirtyForTest();
        });
        report(-75); report(-76);
        QVERIFY(!pan->overlayStaticDirtyForTest());
        QVERIFY(shown().contains(QStringLiteral("-74")));
        QVERIFY(!shown().contains(QStringLiteral("-76")));
        QTRY_VERIFY2_WITH_TIMEOUT(shown().contains(QStringLiteral("-76")),
            qPrintable(QStringLiteral("shown=%1 frequency=%2 view=%3").arg(shown()).arg(model->frequencyAvailable())
                .arg(QString::fromUtf8(QJsonDocument(model->room(QStringLiteral("frequency:usb-14200000"))).toJson(QJsonDocument::Compact)))), kWaitMs);
        QVERIFY(cache.observed);
        QVERIFY(!cache.dirtied);
        qInfo("MainWindow trailing report presentation: static texture invalidations=%d", cache.dirtied ? 1 : 0);
        QJsonObject next = actor; next.insert("callsign", "N3NEW"); next.insert("ref", "next-tx");
        turn.insert("id", 2); turn.insert("talkers", QJsonArray{next});
        QJsonObject sample = reporter.value("reading").toObject(); sample.insert("turn", 2); sample.insert("dbm", -61);
        reporter.insert("reading", sample); view.insert("now", QJsonArray{turn});
        view.insert("members", QJsonArray{next, reporter}); model->applyRoom(view);
        QVERIFY2(shown().contains(QStringLiteral("Talking: N3NEW")), qPrintable(shown()));
        QVERIFY(shown().contains(QStringLiteral("-61")));
    }

    // Overlay design D21 to D27: looking at another frequency moves nothing.
    void aTagViewsAFrequencyWithoutTuning()
    {
        class FrequencyLink : public IStationLink {
        public:
            QStringList reads;
            QString refuseWith;
            bool roundtableAvailable() const override { return true; }
            bool roundtableFrequencyAvailable() const override { return true; }
            CommandOutcome requestAddSlice(const QString&) override { return {}; }
            CommandOutcome requestAddSliceOnPan(const QString&) override { return {}; }
            CommandOutcome requestRemoveSlice(int) override { return {}; }
            CommandOutcome requestActiveSlice(int) override { return {}; }
            CommandOutcome requestSliceSampleRate(int, int) override { return {}; }
            CommandOutcome requestRoundtableReadAt(qint64 hz, const QString& family) override
            {
                reads.append(QStringLiteral("%1-%2").arg(family).arg(hz));
                if (!refuseWith.isEmpty()) { return {false, refuseWith}; }
                return {true, {}};
            }
            CommandOutcome requestRoundtableUnreadAt(qint64, const QString&) override { return {true, {}}; }
        } link;
        RoundtableState state;
        Everything p;
        SliceModel* slice = p.model()->sliceById(p.sliceId());
        QVERIFY(slice);
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14200000);
        RoundtableModel* model = p.model()->roundtableModel();
        QVERIFY(model);
        model->attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        const qint64 viewedHz = 14188600;
        const QString viewed = QStringLiteral("frequency:usb-%1").arg(viewedHz);
        const QString mine = QStringLiteral("frequency:usb-14200000");
        const auto person = [](const char* ref, const char* call, qint64 hz, bool keyed) {
            return QJsonObject{{"sourceRoom", "usb-14188600"}, {"ref", ref}, {"callsign", call}, {"hz", hz},
                               {"family", "usb"}, {"keyed", keyed}};
        };
        const QJsonObject talker = person("tx", "VE3XYZ", viewedHz, true);
        const QJsonObject second = person("tx2", "KD8LMN", viewedHz + 60, true);
        const QJsonObject quiet = person("rx", "W2ABC", viewedHz + 20, false);
        model->applyRooms({{"frequencyVersion", 1}, {"rooms", QJsonArray{
            QJsonObject{{"id", "usb-14188600"}, {"stations", QJsonArray{talker, quiet}}}}}});
        QVERIFY(model->frequencyAvailable());
        SpectrumWidget* pan = panOf(p.window());
        RoundtableApplet* applet = appletOf(p.window());
        QVERIFY(pan && applet);
        RoundtablePanel* panel = applet->panel();
        pan->setFrequencyRange(14200000.0, kPanSpanHz);
        settle();
        QTRY_VERIFY_WITH_TIMEOUT(!pan->roundtableTags().isEmpty(), kWaitMs);

        const auto clickHit = [&](const QString& room) {
            QImage image(pan->size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::black);
            QPainter painter(&image);
            pan->drawRoundtableTagsForTest(painter, pan->roundtableTagSpectrumRectForTest());
            painter.end();
            for (const SpectrumWidget::RoundtableTagHit& hit : pan->roundtableTagHitsForTest()) {
                if (hit.roomIds.contains(room)) {
                    QTest::mouseClick(pan, Qt::LeftButton, Qt::NoModifier, hit.rect.center());
                    return true;
                }
            }
            return false;
        };
        // Through the window's own switch, which the panel's Show goes through too.
        auto* appletSwitch = p.window()->findChild<AppletVisibilityController*>();
        QVERIFY(appletSwitch != nullptr);
        const auto hideApplet = [&]() { appletSwitch->setVisible(QStringLiteral("Roundtable"), false); };
        const auto peek = [&]() { return pan->findChild<RoundtableFrequencyPeek*>(); };
        const auto open = [&]() { return peek() != nullptr && peek()->isOpen(); };
        const auto button = [&](const char* name) {
            return peek()->findChild<QAbstractButton*>(QString::fromLatin1(name));
        };

        // ── The click: a read of that frequency, the view, and nothing else ──
        hideApplet();
        QVERIFY(clickHit(viewed));
        QVERIFY(link.reads.contains(QStringLiteral("usb-%1").arg(viewedHz)));
        QVERIFY(open());
        QCOMPARE(peek()->hz(), static_cast<quint64>(viewedHz));
        QCOMPARE(std::llround(slice->frequency()), 14200000LL);
        QVERIFY(!applet->isVisible());
        QCOMPARE(pan->roundtableViewedRoom(), viewed);
        // Your pan is sampled only while a view is open (D23).
        auto* peekTimer = p.window()->findChild<QTimer*>(QStringLiteral("roundtablePeekTimer"));
        QVERIFY(peekTimer != nullptr);
        QVERIFY(peekTimer->isActive());
        QCOMPARE(peekTimer->interval(), 200);
        QCOMPARE(panel->roomId(), viewed);
        QVERIFY(panel->isReading());
        // The active frequency keeps its own slot, so its card stays live.
        QCOMPARE(model->activeFrequencyContext(), mine);
        QVERIFY(model->wantedFrequencyContexts().contains(mine));
        QVERIFY(model->wantedFrequencyContexts().contains(viewed));

        // Before the stream: the directory's stations, no readings.
        QCOMPARE(peek()->viewForTest().card.kind, RoundtableCardModel::Kind::Talking);
        QCOMPARE(peek()->viewForTest().card.onFrequency.size(), qsizetype(2));
        QVERIFY(peek()->viewForTest().card.rows.isEmpty());
        QCOMPARE(roundtableViewSubText(peek()->viewForTest()),
                 QStringLiteral("Viewing \u00b7 \u221211.4 kHz"));

        // The stream, with offsets from the viewed frequency.
        QJsonObject reporter = quiet;
        reporter.insert("reading", QJsonObject{{"turn", 1}, {"present", true}, {"dbm", -73}, {"t", g_now}});
        reporter.insert("km", 2000);
        reporter.insert("bearingDeg", 87);
        QJsonObject turn{{"id", 1}, {"kind", "talking"}, {"sourceRoom", "usb-14188600"}, {"hz", viewedHz},
            {"start", g_now - 1000}, {"end", QJsonValue::Null}, {"talkers", QJsonArray{talker}}};
        QJsonObject view{{"id", QStringLiteral("usb-%1").arg(viewedHz)}, {"view", "frequency"}, {"family", "usb"},
            {"hz", viewedHz}, {"now", QJsonArray{turn}}, {"members", QJsonArray{talker, reporter}}};
        model->applyRoom(view);
        QTRY_COMPARE_WITH_TIMEOUT(peek()->viewForTest().card.rows.size(), qsizetype(1), kWaitMs);
        QCOMPARE(peek()->viewForTest().card.rows.first().call, QStringLiteral("W2ABC"));
        QCOMPARE(peek()->viewForTest().card.rows.first().km.value_or(0), 2000);
        QCOMPARE(peek()->viewForTest().card.onFrequency.first().name, QStringLiteral("VE3XYZ"));
        QVERIFY(peek()->viewForTest().card.onFrequency.first().keyed);
        QCOMPARE(peek()->viewForTest().card.onFrequency.at(1).offsetHz, qint64(20));
        QVERIFY(peek()->viewForTest().card.clockMs >= 0);

        // A double: combined RF, no path.
        QJsonObject doubledTurn = turn;
        doubledTurn.insert("kind", "doubled");
        doubledTurn.insert("talkers", QJsonArray{talker, second});
        QJsonObject doubledView = view;
        doubledView.insert("now", QJsonArray{doubledTurn});
        doubledView.insert("members", QJsonArray{talker, second, reporter});
        model->applyRoom(doubledView);
        QTRY_COMPARE_WITH_TIMEOUT(peek()->viewForTest().card.kind, RoundtableCardModel::Kind::Double, kWaitMs);
        QVERIFY(!peek()->viewForTest().card.rows.first().km.has_value());
        QCOMPARE(peek()->viewForTest().card.onFrequency.at(1).offsetHz, qint64(60));
        model->applyRoom(view);

        // ── The link drops: last known, still open ──
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Reconnecting));
        QTRY_VERIFY_WITH_TIMEOUT(peek()->viewForTest().card.frozen, kWaitMs);
        QVERIFY(open());
        QCOMPARE(roundtableViewLinkText(peek()->viewForTest().card), QStringLiteral("Reconnecting \u00b7 last known"));
        QVERIFY(button("roundtablePeekTune")->isEnabled());
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        QTRY_VERIFY_WITH_TIMEOUT(!peek()->viewForTest().card.frozen, kWaitMs);

        // ── Escape closes, and the panel goes back to the slice ──
        // Wait on what the shortcut checks, the application's active window. The
        // first-run audio dialog opens in a fresh profile, and it can hold the
        // focus here. QWindow::isActive counts a window's own dialog as active,
        // so qWaitForWindowActive returns before this activation is delivered.
        p.window()->activateWindow();
        QTRY_VERIFY_WITH_TIMEOUT(QApplication::activeWindow() == p.window(), kWaitMs);
        QTest::keyClick(p.window(), Qt::Key_Escape);
        QVERIFY(!open());
        QVERIFY(!panel->isReading());
        QVERIFY(pan->roundtableViewedRoom().isEmpty());
        QVERIFY(!peekTimer->isActive());
        QCOMPARE(std::llround(slice->frequency()), 14200000LL);

        // ── A click on the band tunes as always and leaves it open on what it shows (D27) ──
        QVERIFY(clickHit(viewed));
        QVERIFY(open());
        QSignalSpy closedSpy(peek(), &RoundtableFrequencyPeek::closed);
        const QRect spec = pan->roundtableTagSpectrumRectForTest();
        QSignalSpy bandTune(pan, &SpectrumWidget::frequencyClicked);
        QTest::mouseClick(pan, Qt::LeftButton, Qt::NoModifier, QPoint(spec.left() + 300, spec.center().y()));
        QCOMPARE(bandTune.size(), 1);
        QVERIFY(open());
        QCOMPARE(peek()->hz(), static_cast<quint64>(viewedHz));
        QVERIFY(!peek()->showsOwn());
        QCOMPARE(pan->roundtableViewedRoom(), viewed);
        QCOMPARE(closedSpy.size(), 0);
        slice->setFrequency(14200000);
        settle();

        // ── Tuning by any route leaves it, and it keeps reading that frequency ──
        slice->setFrequency(14201000);
        QVERIFY(open());
        QVERIFY(!peek()->showsOwn());
        QCOMPARE(peek()->hz(), static_cast<quint64>(viewedHz));
        QVERIFY(panel->isReading());
        QCOMPARE(panel->roomId(), viewed);
        QCOMPARE(model->activeFrequencyContext(), QStringLiteral("frequency:usb-14201000"));
        QVERIFY(model->wantedFrequencyContexts().contains(viewed));
        slice->setFrequency(14200000);
        settle();
        QVERIFY(open());
        QCOMPARE(model->activeFrequencyContext(), mine);
        QCOMPARE(closedSpy.size(), 0);

        // ── Your slice moves onto the viewed frequency: your own, in place ──
        slice->setFrequency(static_cast<double>(viewedHz));
        QVERIFY(open());
        QVERIFY(peek()->showsOwn());
        QCOMPARE(peek()->hz(), static_cast<quint64>(viewedHz));
        QCOMPARE(closedSpy.size(), 0);
        QVERIFY(!panel->isReading());
        // Your own follows your slice.
        slice->setFrequency(14200000);
        settle();
        QVERIFY(peek()->showsOwn());
        QCOMPARE(peek()->hz(), quint64(14200000));
        QCOMPARE(pan->roundtableViewedRoom(), mine);
        button("roundtablePeekClose")->click();
        QVERIFY(!open());
        QCOMPARE(closedSpy.size(), 1);

        // ── Tune slice A here tunes and closes ──
        QVERIFY(clickHit(viewed));
        QCOMPARE(button("roundtablePeekTune")->text(), QStringLiteral("Tune slice A here"));
        button("roundtablePeekTune")->click();
        QCOMPARE(std::llround(slice->frequency()), viewedHz);
        QVERIFY(!open());
        slice->setFrequency(14200000);
        settle();

        // ── Messages: the applet reads this frequency, read only ──
        QVERIFY(clickHit(viewed));
        button("roundtablePeekMessages")->click();
        settle();
        QVERIFY(applet->isVisible());
        QVERIFY(open());
        QCOMPARE(panel->roomId(), viewed);
        auto* box = visibleChild<QPlainTextEdit>(panel, QStringLiteral("roundtableBox"));
        QVERIFY(box != nullptr);
        QVERIFY(!box->isEnabled());
        QCOMPARE(std::llround(slice->frequency()), 14200000LL);
        // Closing the view leaves the applet reading, until Back to my slice.
        button("roundtablePeekClose")->click();
        QVERIFY(!open());
        QCOMPARE(panel->roomId(), viewed);
        auto* back = visibleChild<QPushButton>(panel, QStringLiteral("roundtableBackBtn"));
        QVERIFY(back != nullptr);
        back->click();
        QVERIFY(!panel->isReading());
        // Back to my slice with the view still open ends the view too.
        QVERIFY(clickHit(viewed));
        button("roundtablePeekMessages")->click();
        settle();
        back = visibleChild<QPushButton>(panel, QStringLiteral("roundtableBackBtn"));
        QVERIFY(back != nullptr);
        back->click();
        QVERIFY(!open());
        hideApplet();

        // ── A refused read: no view, a message, the slice alone ──
        link.refuseWith = QStringLiteral("Not now");
        QVERIFY(clickHit(viewed));
        QVERIFY(!open());
        QVERIFY(!panel->isReading());
        QCOMPARE(p.window()->statusBar()->currentMessage(), QStringLiteral("Not now"));
        QCOMPARE(std::llround(slice->frequency()), 14200000LL);
        link.refuseWith.clear();
        p.window()->statusBar()->clearMessage();

        // The Core refuses after the view opened: it closes and says why.
        QVERIFY(clickHit(viewed));
        QVERIFY(open());
        model->applyRefusal(QStringLiteral("readAt"), QStringLiteral("Lost it"), viewed);
        QVERIFY(!open());
        QCOMPARE(p.window()->statusBar()->currentMessage(), QStringLiteral("Lost it"));
        QCOMPARE(std::llround(slice->frequency()), 14200000LL);
        p.window()->statusBar()->clearMessage();
        settle();

        // ── Your pan is read on the view's timer, however often the tags refresh (D23) ──
        QVERIFY(clickHit(viewed));
        QVERIFY(open());
        pan->setRenderedPixelsForTest(QVector<float>(193, -90.0f));
        QTRY_VERIFY_WITH_TIMEOUT(peek()->viewForTest().panDbm.has_value(), kWaitMs);
        const double sampled = peek()->viewForTest().panDbm.value_or(0.0);
        peekTimer->stop();
        pan->setRenderedPixelsForTest(QVector<float>(193, -70.0f));
        QJsonObject refreshedView = view;
        QJsonObject newerReporter = reporter;
        newerReporter.insert("reading", QJsonObject{{"turn", 1}, {"present", true}, {"dbm", -61}, {"t", g_now}});
        refreshedView.insert("members", QJsonArray{talker, newerReporter});
        model->applyRoom(refreshedView);
        QTRY_COMPARE_WITH_TIMEOUT(peek()->viewForTest().card.rows.first().dbm, -61.0, kWaitMs);
        QCOMPARE(peek()->viewForTest().panDbm.value_or(0.0), sampled);
        peekTimer->start();
        QTRY_COMPARE_WITH_TIMEOUT(peek()->viewForTest().panDbm.value_or(0.0), sampled + 20.0, kWaitMs);

        // ── Keying: Tune greys at once in the view and the panel; the view stays ──
        MoxController* mox = p.core()->moxController();
        QVERIFY(mox != nullptr);
        p.server().setRemoteTransmitAllowed(true);
        p.core()->transmitModel().setMicSource(MicSource::Radio);
        mox->setMox(true);
        QTRY_VERIFY_WITH_TIMEOUT(p.model()->isTransmitting(), kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(!button("roundtablePeekTune")->isEnabled(), kWaitMs);
        QVERIFY(open());
        // A pan that is not the transmitting one has no overlay, only the key:
        // Your pan is blank there too, and its card's Tune waits (D22, D23).
        QTRY_VERIFY_WITH_TIMEOUT(pan->displayKeyed(), kWaitMs);
        pan->setMoxOverlay(false);
        QTest::qWait(2 * peekTimer->interval() + 50);
        QVERIFY(!peek()->viewForTest().panDbm.has_value());
        {
            QMenu cardMenu;
            pan->buildRoundtableCardMenu(viewed, viewedHz, false, cardMenu);
            QVERIFY(!cardMenu.findChild<QAction*>(QStringLiteral("roundtableMenuTune"))->isEnabled());
        }
        auto* panelTune = panel->findChild<QPushButton*>(QStringLiteral("roundtableTuneHereBtn"));
        QVERIFY(panelTune != nullptr);
        QVERIFY(!panelTune->isEnabled());
        panelTune->click();
        QCOMPARE(std::llround(slice->frequency()), 14200000LL);
        // Unkeying shows nothing held from the key: the level the pan showed while
        // keyed is gone at once, not shown for a tick and fed to the peak.
        pan->setRenderedPixelsForTest(QVector<float>(193, -20.0f));
        QTest::qWait(2 * peekTimer->interval() + 50);
        bool sawUnkey = false;
        std::optional<double> panAtUnkey;
        const QMetaObject::Connection unkeyProbe = connect(p.model(), &RadioModel::transmittingChanged, this,
            [&](bool transmitting) {
                if (!transmitting && !sawUnkey) {
                    sawUnkey = true;
                    panAtUnkey = peek()->viewForTest().panDbm;
                }
            });
        mox->setMox(false);
        QTRY_VERIFY_WITH_TIMEOUT(!p.model()->isTransmitting(), kWaitMs);
        disconnect(unkeyProbe);
        QVERIFY(sawUnkey);
        QVERIFY2(!panAtUnkey.has_value(), qPrintable(QString::number(panAtUnkey.value_or(0.0))));
        QTRY_VERIFY_WITH_TIMEOUT(button("roundtablePeekTune")->isEnabled(), kWaitMs);
        QVERIFY(panelTune->isEnabled());
        // With no radio there is nothing to tune, whatever Roundtable's link says.
        p.core()->setConnectionStateForTest(ConnectionState::Disconnected);
        QTRY_VERIFY_WITH_TIMEOUT(!button("roundtablePeekTune")->isEnabled(), kWaitMs);
        p.core()->setConnectionStateForTest(ConnectionState::Connected);
        QTRY_VERIFY_WITH_TIMEOUT(button("roundtablePeekTune")->isEnabled(), kWaitMs);
        button("roundtablePeekClose")->click();
        settle();

        // ── Another slice's flag line views it and leaves the active slice alone ──
        const int otherId = p.model()->addSlice(QStringLiteral("pan-0"));
        QVERIFY(otherId >= 0);
        p.model()->setActiveSlice(p.sliceId());
        settle();
        QCOMPARE(p.model()->activeSlice(), slice);
        SliceModel* other = p.model()->sliceById(otherId);
        QVERIFY(other != nullptr);
        other->setDspMode(DSPMode::USB);
        other->setFrequency(14230000);
        settle();
        VfoWidget* otherFlag = flagFor(p.window(), otherId);
        QVERIFY(otherFlag != nullptr && otherFlag->roundtableLineButtonForTest() != nullptr);
        hideApplet();
        // A real press: the line takes it, and the flag under it does not activate.
        QTRY_VERIFY_WITH_TIMEOUT(otherFlag->roundtableLineButtonForTest()->isVisible(), kWaitMs);
        QTest::mouseClick(otherFlag->roundtableLineButtonForTest(), Qt::LeftButton);
        QVERIFY(open());
        QCOMPARE(peek()->hz(), quint64(14230000));
        QCOMPARE(p.model()->activeSlice(), slice);
        QCOMPARE(std::llround(other->frequency()), 14230000LL);
        QCOMPARE(std::llround(slice->frequency()), 14200000LL);
        QVERIFY(!applet->isVisible());
        // The same line again closes it, and your own line swaps in place.
        QTest::mouseClick(otherFlag->roundtableLineButtonForTest(), Qt::LeftButton);
        QVERIFY(!open());
        QTest::mouseClick(otherFlag->roundtableLineButtonForTest(), Qt::LeftButton);
        QVERIFY(open());
        VfoWidget* ownFlag = flagFor(p.window(), p.sliceId());
        QVERIFY(ownFlag != nullptr);
        QSignalSpy swapped(peek(), &RoundtableFrequencyPeek::closed);
        ownFlag->roundtableLineButtonForTest()->click();
        QVERIFY(open());
        QVERIFY(peek()->showsOwn());
        QCOMPARE(swapped.size(), 0);
        QVERIFY(!applet->isVisible());
        QVERIFY(!panel->isReading());
        // Messages is what opens your own room.
        button("roundtablePeekMessages")->click();
        settle();
        QVERIFY(applet->isVisible());
        QCOMPARE(panel->roomId(), mine);
        QVERIFY(!panel->isReading());
        hideApplet();
        ownFlag->roundtableLineButtonForTest()->click();
        QVERIFY(!open());
        QCOMPARE(swapped.size(), 1);

        // ── Your own card: it toggles, and it is hidden while the inspector stands ──
        const QJsonObject onMine{{"sourceRoom", "usb-14200000"}, {"ref", "cq"}, {"callsign", "W1AW"},
            {"hz", 14200000}, {"family", "usb"}, {"keyed", true}};
        QJsonObject mineTurn{{"id", 5}, {"kind", "talking"}, {"sourceRoom", "usb-14200000"}, {"hz", 14200000},
            {"start", g_now - 1000}, {"end", QJsonValue::Null}, {"talkers", QJsonArray{onMine}}};
        model->applyRoom(QJsonObject{{"id", "usb-14200000"}, {"view", "frequency"}, {"family", "usb"},
            {"hz", 14200000}, {"now", QJsonArray{mineTurn}}, {"members", QJsonArray{onMine}}});
        const auto cardShown = [&]() {
            QImage image(pan->size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::black);
            QPainter painter(&image);
            pan->drawRoundtableTagsForTest(painter, pan->roundtableTagSpectrumRectForTest());
            painter.end();
            const QList<SpectrumWidget::RoundtableTagHit>& hits = pan->roundtableTagHitsForTest();
            return std::any_of(hits.cbegin(), hits.cend(), [](const SpectrumWidget::RoundtableTagHit& hit) {
                return hit.kind == SpectrumWidget::RoundtableTagHit::Kind::Card;
            });
        };
        QTRY_VERIFY_WITH_TIMEOUT(cardShown(), kWaitMs);
        const qsizetype readsBefore = link.reads.size();
        QVERIFY(clickHit(mine));
        QVERIFY(open());
        QVERIFY(peek()->showsOwn());
        // No read of your own room, which the panel would take for going back to the slice.
        QCOMPARE(link.reads.size(), readsBefore);
        QVERIFY(!panel->isReading());
        QVERIFY(!cardShown());
        // The inspector carries the same talker.
        QTRY_COMPARE_WITH_TIMEOUT(peek()->viewForTest().card.kind, RoundtableCardModel::Kind::Talking, kWaitMs);
        emit pan->roundtableReadRequested(mine);
        QVERIFY(!open());
        QVERIFY(cardShown());

        // Viewing another, the card stays hidden too, and Back goes to your own in place.
        QVERIFY(clickHit(viewed));
        QVERIFY(open());
        QVERIFY(!cardShown());
        QSignalSpy backClosed(peek(), &RoundtableFrequencyPeek::closed);
        button("roundtablePeekBack")->click();
        QVERIFY(open());
        QVERIFY(peek()->showsOwn());
        QCOMPARE(backClosed.size(), 0);
        QVERIFY(!panel->isReading());
        QCOMPARE(std::llround(slice->frequency()), 14200000LL);

        // ⋯: Signal changes nothing else, and Read reports opens your own room.
        button("roundtablePeekMore")->click();
        auto* menu = p.window()->findChild<QMenu*>(QStringLiteral("roundtableShortMenu"), Qt::FindDirectChildrenOnly);
        QVERIFY(menu != nullptr);
        auto* signalRow = menu->findChild<QAction*>(QStringLiteral("roundtableMenuSignal"));
        QVERIFY(signalRow != nullptr);
        QVERIFY(menu->findChild<QAction*>(QStringLiteral("roundtableMenuInspector")) == nullptr);
        // On your slice, so Tune has nowhere to go.
        QVERIFY(!menu->findChild<QAction*>(QStringLiteral("roundtableMenuTune"))->isEnabled());
        const QString before = AppSettings::instance().value(QStringLiteral("Roundtable/SignalFormat"), QStringLiteral("dBm")).toString();
        signalRow->trigger();
        QVERIFY(AppSettings::instance().value(QStringLiteral("Roundtable/SignalFormat")).toString() != before);
        QVERIFY(open());
        QTRY_COMPARE_WITH_TIMEOUT(peek()->viewForTest().format,
            AppSettings::instance().value(QStringLiteral("Roundtable/SignalFormat")).toString(), kWaitMs);
        AppSettings::instance().remove(QStringLiteral("Roundtable/SignalFormat"));
        hideApplet();
        menu->findChild<QAction*>(QStringLiteral("roundtableMenuRead"))->trigger();
        QVERIFY(applet->isVisible());
        QCOMPARE(panel->roomId(), mine);
        QVERIFY(open());
        menu->close();
        button("roundtablePeekClose")->click();
        model->detach();
    }

    void runSMeterSource(MainWindow* window, RadioModel* radio, RadioModel* core, const QList<int>& sliceIds)
    {
        SMeterWidget* meter = window->findChild<SMeterWidget*>();
        auto* poller = window->findChild<MeterPoller*>();
        QVERIFY(meter && poller);
        QList<VfoWidget*> flags;
        for (const int id : sliceIds) {
            VfoWidget* flag = flagFor(window, id);
            QVERIFY(flag != nullptr);
            flags.append(flag);
        }
        const auto flagsShow = [&](const QString& source, const QString& tag) {
            for (VfoWidget* flag : flags) {
                if (flag->sMeterSource() != source || flag->findChild<VfoLevelBar*>()->sourceTag() != tag) {
                    return false;
                }
            }
            return true;
        };
        // A new flag starts on the window's choice.
        QVERIFY(flagsShow(QStringLiteral("Signal"), QString()));

        // What is not a reading, on the window's model and on what it stands for. The
        // choice is display only: not a slice property, not a radio one.
        const auto stateOf = [](const QObject* object) {
            QStringList values;
            const QMetaObject* type = object->metaObject();
            for (int i = type->propertyOffset(); i < type->propertyCount(); ++i) {
                const QMetaProperty property = type->property(i);
                const QString name = QString::fromLatin1(property.name());
                if (name.contains(QStringLiteral("signal"), Qt::CaseInsensitive)
                    || name.startsWith(QStringLiteral("adc")) || name.startsWith(QStringLiteral("agc"))
                    || name == QStringLiteral("meterReadingNs")) {
                    continue;
                }
                values << name + QLatin1Char('=') + property.read(object).toString();
            }
            return values;
        };
        const auto everything = [&]() {
            QStringList values = stateOf(radio) + stateOf(core);
            for (SliceModel* slice : radio->slices()) { values += stateOf(slice); }
            for (SliceModel* slice : core->slices()) { values += stateOf(slice); }
            return values;
        };
        const QStringList before = everything();

        // Through a flag's own menu, pressed as a person presses it.
        const auto pick = [&](VfoWidget* flag, const QString& source) {
            QTimer::singleShot(0, flag, [flag, source]() {
                for (QMenu* menu : flag->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly)) {
                    if (!menu->isVisible()) { continue; }
                    for (QAction* row : menu->actions()) {
                        if (!row->menu() || !row->text().startsWith(QStringLiteral("S-meter"))) { continue; }
                        for (QAction* choice : row->menu()->actions()) {
                            if (choice->text() == source) { choice->trigger(); }
                        }
                    }
                    menu->close();
                    return;
                }
            });
            flag->moreButtonForTest()->click();
        };
        pick(flags.first(), QStringLiteral("Sig Avg"));
        QCOMPARE(meter->rxMode(), SMeterWidget::RxMode::SignalAverage);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("SMeter_RxSelect")).toInt(), 1);
        QVERIFY(flagsShow(QStringLiteral("Sig Avg"), QStringLiteral("AV")));

        // From the panel's own control, every flag follows.
        meter->setRxMode(QStringLiteral("Max Bin"));
        QVERIFY(flagsShow(QStringLiteral("Max Bin"), QStringLiteral("MAX")));
        QCOMPARE(AppSettings::instance().value(QStringLiteral("SMeter_RxSelect")).toInt(), 3);

        meter->setRxMode(QStringLiteral("Signal Peak"));
        QVERIFY(flagsShow(QStringLiteral("Signal Peak"), QString()));

        // The value is what the selector already sends, on a window of a Core.
        if (radio->role() == RadioModel::Role::Remote) {
            SliceModel* slice = radio->sliceById(sliceIds.first());
            QVERIFY(slice != nullptr);
            VfoLevelBar* bar = flags.first()->findChild<VfoLevelBar*>();
            slice->setSignalPeakDbm(-62);
            slice->setSignalAverageDbm(-81);
            QVERIFY(QMetaObject::invokeMethod(poller, "poll", Qt::DirectConnection));
            QCOMPARE(bar->value(), -62.0f);
            pick(flags.first(), QStringLiteral("Sig Avg"));
            QVERIFY(QMetaObject::invokeMethod(poller, "poll", Qt::DirectConnection));
            QCOMPARE(bar->value(), -81.0f);
        }

        meter->setRxMode(QStringLiteral("Signal"));
        QVERIFY(flagsShow(QStringLiteral("Signal"), QString()));
        // The two readings set above are readings, so they sit outside this.
        QCOMPARE(everything(), before);

        // Max Bin greys, with the reason, while no pan shows the slice. The menu
        // asks at the moment it opens. This one test edit of the slice's stream comes after
        // the comparison above: it is the test's own write, and the slice records it.
        SliceModel* slice = radio->sliceById(sliceIds.first());
        QVERIFY(slice != nullptr);
        VfoWidget* flag = flags.first();
        flag->setSMeterSource(flag->sMeterSource(), true);
        const int stream = slice->streamIndex();
        const auto openAndClose = [&]() {
            QTimer::singleShot(0, flag, [flag]() {
                for (QMenu* menu : flag->findChildren<QMenu*>(QString(), Qt::FindDirectChildrenOnly)) {
                    if (menu->isVisible()) { menu->close(); return; }
                }
            });
            flag->moreButtonForTest()->click();
        };
        openAndClose();
        const bool shown = flag->sMeterMaxBinAvailable();
        slice->setStreamIndex(-1);
        openAndClose();
        QVERIFY(!flag->sMeterMaxBinAvailable());
        slice->setStreamIndex(stream);
        openAndClose();
        QCOMPARE(flag->sMeterMaxBinAvailable(), shown);
    }

    // Every route of the one inspector, by real clicks, moves nothing but what Tune and
    // the band are for (overlay design D21 to D27, D34, D36, D37).
    void runInspectorRouting(MainWindow* window, RadioModel* radio, RadioModel* core, int sliceId, RoundtableModel* model,
                             ReadingLink& link, const std::function<void(bool)>& key)
    {
        SliceModel* slice = radio->sliceById(sliceId);
        SliceModel* coreSlice = core->sliceById(sliceId);
        QVERIFY(slice && coreSlice);
        const qint64 home = 14200000;
        coreSlice->setDspMode(DSPMode::USB);
        coreSlice->setFrequency(static_cast<double>(home));
        QTRY_COMPARE_WITH_TIMEOUT(std::llround(slice->frequency()), home, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(slice->dspMode(), DSPMode::USB, kWaitMs);
        const qint64 viewedHz = 14188600;
        const qint64 otherHz = 14175000;
        const qint64 lsbHz = 14240000;
        const QString viewed = RoundtableModel::frequencyContextId(viewedHz, QStringLiteral("usb"));
        const QString other = RoundtableModel::frequencyContextId(otherHz, QStringLiteral("usb"));
        const QString lsb = RoundtableModel::frequencyContextId(lsbHz, QStringLiteral("lsb"));
        const QString mine = RoundtableModel::frequencyContextId(home, QStringLiteral("usb"));
        const QStringList foldRooms{RoundtableModel::frequencyContextId(14150000, QStringLiteral("usb")),
                                    RoundtableModel::frequencyContextId(14150050, QStringLiteral("usb")),
                                    RoundtableModel::frequencyContextId(14150100, QStringLiteral("usb"))};
        const auto room = [](const char* family, qint64 hz, const char* call, bool keyed) {
            const QString id = QStringLiteral("%1-%2").arg(QString::fromLatin1(family)).arg(hz);
            return QJsonObject{{"id", id}, {"stations", QJsonArray{QJsonObject{{"sourceRoom", id}, {"ref", "q"},
                {"callsign", QString::fromLatin1(call)}, {"hz", hz}, {"family", QString::fromLatin1(family)}, {"keyed", keyed}}}}};
        };
        const QJsonObject rooms{{"frequencyVersion", 1}, {"rooms", QJsonArray{
            room("usb", viewedHz, "VE3XYZ", true), room("usb", otherHz, "W2ABC", false), room("lsb", lsbHz, "K4ABC", false),
            room("usb", 14150000, "N1AAA", false), room("usb", 14150050, "N1BBB", false), room("usb", 14150100, "N1CCC", false),
            room("usb", home, "W1AW", false)}}};
        // A window of the Core: its rooms stream says "no frequency reads" whenever it lands.
        QObject capabilityHold;
        connect(model, &RoundtableModel::roomsChanged, &capabilityHold, [model, rooms]() {
            if (!model->frequencyAvailable()) { model->applyRooms(rooms); }
        });
        model->applyRooms(rooms);
        QVERIFY(model->frequencyAvailable());
        SpectrumWidget* pan = panOf(window);
        RoundtableApplet* applet = appletOf(window);
        QVERIFY(pan && applet);
        RoundtablePanel* panel = applet->panel();
        pan->setFrequencyRange(static_cast<double>(home), kPanSpanHz);
        settle();
        const int activeId = radio->activeSlice() ? radio->activeSlice()->sliceIndex() : -1;
        const QString tuneText = QStringLiteral("Tune slice %1 here").arg(QString(slice->sliceLetter()));
        const auto draw = [&]() {
            // A window of the Core takes the Core's pan whenever it lands; the tags here need this one.
            if (pan->bandwidth() != kPanSpanHz) {
                pan->setFrequencyRange(static_cast<double>(home), kPanSpanHz);
            }
            QImage image(pan->size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::black);
            QPainter painter(&image);
            pan->drawRoundtableTagsForTest(painter, pan->roundtableTagSpectrumRectForTest());
        };
        const auto hitOf = [&](const QString& id, SpectrumWidget::RoundtableTagHit* out) {
            draw();
            for (const SpectrumWidget::RoundtableTagHit& hit : pan->roundtableTagHitsForTest()) {
                if (hit.roomIds.contains(id)) {
                    *out = hit;
                    return true;
                }
            }
            return false;
        };
        const auto click = [&](const QString& id, Qt::MouseButton which = Qt::LeftButton) {
            SpectrumWidget::RoundtableTagHit hit;
            if (!hitOf(id, &hit)) { return false; }
            QTest::mouseClick(pan, which, Qt::NoModifier, hit.rect.center());
            return true;
        };
        const auto peek = [&]() { return pan->findChild<RoundtableFrequencyPeek*>(); };
        const auto open = [&]() { return peek() != nullptr && peek()->isOpen(); };
        const auto button = [&](const char* name) { return peek()->findChild<QAbstractButton*>(QString::fromLatin1(name)); };
        const auto nothingMoved = [&]() {
            return std::llround(slice->frequency()) == home && std::llround(coreSlice->frequency()) == home
                && slice->dspMode() == DSPMode::USB && coreSlice->dspMode() == DSPMode::USB
                && !radio->isTransmitting() && !core->isTransmitting()
                && (radio->activeSlice() ? radio->activeSlice()->sliceIndex() : -1) == activeId;
        };
        const auto tagsClear = [&]() {
            draw();
            for (const SpectrumWidget::RoundtableTagHit& hit : pan->roundtableTagHitsForTest()) {
                if (hit.rect.intersects(peek()->inspectorRect())) { return false; }
            }
            return true;
        };
        const auto shortMenu = [&]() {
            QMenu* found = nullptr;
            for (QMenu* menu : window->findChildren<QMenu*>(QStringLiteral("roundtableShortMenu"), Qt::FindDirectChildrenOnly)) {
                if (menu->isVisible()) { found = menu; }
            }
            return found;
        };
        const auto rowsOf = [](QMenu* menu) {
            QStringList rows;
            for (QAction* row : menu->actions()) {
                if (!row->isSeparator()) { rows.append(row->text()); }
            }
            return rows;
        };
        const auto closeMenu = [&](QMenu* menu) {
            menu->close();
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        };
        const QString captures = qEnvironmentVariable("NEREUS_ROUNDTABLE_CAPTURE_DIR");
        const auto capture = [&](const char* step) {
            if (captures.isEmpty()) { return; }
            settle();
            QDir().mkpath(captures);
            // Not grab(): an opaque window's pixmap starts uncleared, and offscreen the pan
            // paints nothing, so the last capture's pixels would show through.
            QImage image(window->size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::black);
            window->render(&image);
            image.save(QStringLiteral("%1/%2-%3.png").arg(captures, QString::fromLatin1(QTest::currentTestFunction()),
                                                          QString::fromLatin1(step)));
        };
        QTRY_VERIFY_WITH_TIMEOUT(!pan->roundtableTags().isEmpty(), kWaitMs);
        
        SpectrumWidget::RoundtableTagHit probe;
        QTRY_VERIFY_WITH_TIMEOUT(hitOf(viewed, &probe), kWaitMs);
        QVERIFY(!probe.folded);

        // ── A tag toggles: the same again closes, and nothing moves ──
        QVERIFY(click(viewed));
        QVERIFY(open());
        QCOMPARE(peek()->hz(), quint64(viewedHz));
        QVERIFY(!peek()->showsOwn());
        QVERIFY(link.reads.contains(QStringLiteral("usb-%1").arg(viewedHz)));
        QVERIFY(tagsClear());
        QVERIFY(nothingMoved());
        capture("another-frequency");
        QSignalSpy closed(peek(), &RoundtableFrequencyPeek::closed);

        // ── Another tag swaps, with no close and no slice moved ──
        QVERIFY(click(other));
        QVERIFY(open());
        QCOMPARE(peek()->hz(), quint64(otherHz));
        QCOMPARE(closed.size(), 0);
        VfoWidget* lineFlag = flagFor(window, sliceId);
        QVERIFY(lineFlag != nullptr);
        // The flag's line reads open only while the inspector shows its frequency.
        QVERIFY(!lineFlag->roundtableLineOpen());
        QCOMPARE(panel->roomId(), other);
        QVERIFY(tagsClear());
        QVERIFY(nothingMoved());
        QVERIFY(click(other));
        QVERIFY(!open());
        QCOMPARE(closed.size(), 1);
        QVERIFY(!panel->isReading());
        QVERIFY(nothingMoved());

        // ── Your own compact tag: your own inspector, no read, and the tag goes ──
        const qsizetype reads = link.reads.size();
        QVERIFY(!lineFlag->roundtableLineOpen());
        QVERIFY(click(mine));
        QVERIFY(open());
        QVERIFY(peek()->showsOwn());
        QVERIFY(lineFlag->roundtableLineOpen());
        QCOMPARE(link.reads.size(), reads);
        QVERIFY(!panel->isReading());
        SpectrumWidget::RoundtableTagHit gone;
        QVERIFY(!hitOf(mine, &gone));
        QVERIFY(!button("roundtablePeekTune")->isEnabled());
        QVERIFY(button("roundtablePeekTune")->isVisible());
        QVERIFY(nothingMoved());
        capture("your-own");
        // While it shows another, your compact tag stays, and a click goes to your own.
        QVERIFY(click(viewed));
        QVERIFY(!peek()->showsOwn());
        QVERIFY(!lineFlag->roundtableLineOpen());
        QVERIFY(hitOf(mine, &gone));
        const qsizetype closedBefore = closed.size();
        QVERIFY(click(mine));
        QVERIFY(peek()->showsOwn());
        QVERIFY(lineFlag->roundtableLineOpen());
        QCOMPARE(closed.size(), closedBefore);
        QVERIFY(!panel->isReading());
        QVERIFY(nothingMoved());

        // ── Your flag line: the same toggle, on any slice's line ──
        VfoWidget* flag = flagFor(window, sliceId);
        QVERIFY(flag != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(flag->roundtableLineButtonForTest()->isVisible(), kWaitMs);
        QTest::mouseClick(flag->roundtableLineButtonForTest(), Qt::LeftButton);
        QVERIFY(!open());
        QVERIFY(!flag->roundtableLineOpen());
        QTest::mouseClick(flag->roundtableLineButtonForTest(), Qt::LeftButton);
        QVERIFY(open());
        QVERIFY(peek()->showsOwn());
        QVERIFY(nothingMoved());

        // ── Back: from another frequency to your own, in place ──
        QVERIFY(click(viewed));
        const qsizetype beforeBack = closed.size();
        button("roundtablePeekBack")->click();
        QVERIFY(open());
        QVERIFY(peek()->showsOwn());
        QCOMPARE(closed.size(), beforeBack);
        QVERIFY(!panel->isReading());
        QVERIFY(nothingMoved());
        button("roundtablePeekClose")->click();
        QVERIFY(!open());

        // ── A tag's right-click: the short menu, and the pan's own does not open ──
        QVERIFY(click(viewed, Qt::RightButton));
        QMenu* menu = shortMenu();
        QVERIFY(menu != nullptr);
        QCOMPARE(rowsOf(menu), (QStringList{QStringLiteral("%1 MHz").arg(roundtableFrequencyText(viewedHz)),
                                            QStringLiteral("Open inspector"), tuneText, QStringLiteral("Messages"),
                                            QStringLiteral("Signal: %1 \u00b7 Change").arg(
                                                AppSettings::instance().value(QStringLiteral("Roundtable/SignalFormat"), QStringLiteral("dBm")).toString())}));
        QVERIFY(nothingMoved());
        capture("short-menu");
        menu->findChild<QAction*>(QStringLiteral("roundtableMenuInspector"))->trigger();
        closeMenu(menu);
        QVERIFY(open());
        QCOMPARE(peek()->hz(), quint64(viewedHz));
        QVERIFY(click(viewed, Qt::RightButton));
        menu = shortMenu();
        QVERIFY(menu != nullptr);
        QCOMPARE(menu->findChild<QAction*>(QStringLiteral("roundtableMenuInspector"))->text(), QStringLiteral("Close inspector"));
        menu->findChild<QAction*>(QStringLiteral("roundtableMenuInspector"))->trigger();
        closeMenu(menu);
        QVERIFY(!open());
        QVERIFY(nothingMoved());
        // Signal, from a tag, changes nothing else.
        QVERIFY(click(viewed, Qt::RightButton));
        menu = shortMenu();
        QVERIFY(menu != nullptr);
        menu->findChild<QAction*>(QStringLiteral("roundtableMenuSignal"))->trigger();
        closeMenu(menu);
        QVERIFY(!open());
        QVERIFY(nothingMoved());
        AppSettings::instance().remove(QStringLiteral("Roundtable/SignalFormat"));

        // ── The flag line's right-click: the same list, not the flag's own ──
        QCOMPARE(flag->roundtableLineButtonForTest()->contextMenuPolicy(), Qt::CustomContextMenu);
        {
            const QPoint at(5, 5);
            QContextMenuEvent event(QContextMenuEvent::Mouse, at, flag->roundtableLineButtonForTest()->mapToGlobal(at));
            QApplication::sendEvent(flag->roundtableLineButtonForTest(), &event);
        }
        menu = shortMenu();
        QVERIFY(menu != nullptr);
        QVERIFY(menu->findChild<QAction*>(QStringLiteral("roundtableMenuInspector")) != nullptr);
        QVERIFY(!menu->findChild<QAction*>(QStringLiteral("roundtableMenuTune"))->isEnabled());
        menu->findChild<QAction*>(QStringLiteral("roundtableMenuInspector"))->trigger();
        closeMenu(menu);
        QVERIFY(open());
        QVERIFY(peek()->showsOwn());
        QVERIFY(nothingMoved());

        // ── ⋯: no Open or Close, with Read reports ──
        QVERIFY(click(viewed));
        QTest::mouseClick(button("roundtablePeekMore"), Qt::LeftButton);
        menu = shortMenu();
        QVERIFY(menu != nullptr);
        QVERIFY(menu->findChild<QAction*>(QStringLiteral("roundtableMenuInspector")) == nullptr);
        QVERIFY(menu->findChild<QAction*>(QStringLiteral("roundtableMenuRead")) != nullptr);
        QVERIFY(menu->findChild<QAction*>(QStringLiteral("roundtableMenuTune"))->isEnabled());
        // Read reports on another frequency turns it to your own in place.
        const qsizetype beforeReports = closed.size();
        menu->findChild<QAction*>(QStringLiteral("roundtableMenuRead"))->trigger();
        closeMenu(menu);
        settle();
        QVERIFY(open());
        QVERIFY(peek()->showsOwn());
        QCOMPARE(closed.size(), beforeReports);
        QVERIFY(applet->isVisible());
        QCOMPARE(panel->roomId(), mine);
        QVERIFY(nothingMoved());
        button("roundtablePeekClose")->click();

        // ── Messages from a tag's menu reads it in the panel and opens no inspector ──
        QVERIFY(click(viewed, Qt::RightButton));
        menu = shortMenu();
        QVERIFY(menu != nullptr);
        menu->findChild<QAction*>(QStringLiteral("roundtableMenuMessages"))->trigger();
        closeMenu(menu);
        settle();
        QVERIFY(!open());
        QCOMPARE(panel->roomId(), viewed);
        panel->backToSlice();
        QVERIFY(nothingMoved());

        // ── A fold lists in the inspector; a row opens that frequency; again closes ──
        SpectrumWidget::RoundtableTagHit foldHit;
        QTRY_VERIFY_WITH_TIMEOUT(hitOf(foldRooms.first(), &foldHit), kWaitMs);
        QVERIFY(foldHit.folded);
        QVERIFY(click(foldRooms.first()));
        QVERIFY(open());
        QVERIFY(peek()->showsFold());
        QCOMPARE(peek()->foldRowsForTest().size(), foldRooms.size());
        QVERIFY(tagsClear());
        QVERIFY(nothingMoved());
        capture("fold-list");
        QVERIFY(click(foldRooms.first()));
        QVERIFY(!open());
        // The list re-lays the tags, so the same fold can come back with other members:
        // a room in common still closes it; a fold with none in common swaps in place.
        emit pan->roundtableFoldClicked(foldRooms);
        QVERIFY(peek()->showsFold());
        emit pan->roundtableFoldClicked(foldRooms.mid(1));
        QVERIFY(!open());
        emit pan->roundtableFoldClicked(foldRooms);
        QVERIFY(peek()->showsFold());
        emit pan->roundtableFoldClicked(QStringList{viewed, other});
        QVERIFY(open());
        QVERIFY(peek()->showsFold());
        QCOMPARE(peek()->foldRowsForTest().size(), 2);
        peek()->closePeek();
        QVERIFY(!open());
        // Your own room leading a fold leaves your compact tag in place.
        emit pan->roundtableFoldClicked(QStringList{mine, viewed});
        QVERIFY(peek()->showsFold());
        SpectrumWidget::RoundtableTagHit ownHit;
        QVERIFY(hitOf(mine, &ownHit));
        peek()->closePeek();
        QVERIFY(!open());
        QVERIFY(click(foldRooms.first()));
        QVERIFY(peek()->showsFold());
        const qsizetype beforeRow = closed.size();
        QTest::mouseClick(peek(), Qt::LeftButton, Qt::NoModifier, QPoint(40, roundtableFoldListBodyTop() + 14));
        QVERIFY(open());
        QVERIFY(!peek()->showsFold());
        QCOMPARE(peek()->hz(), quint64(14150000));
        QCOMPARE(panel->roomId(), foldRooms.first());
        QCOMPARE(closed.size(), beforeRow);
        QVERIFY(nothingMoved());
        button("roundtablePeekClose")->click();
        QVERIFY(!open());
        QVERIFY(!panel->isReading());

        // ── Keying greys Tune at once with the inspector open, and in the menu ──
        if (key) {
            QVERIFY(click(viewed));
            QTRY_VERIFY_WITH_TIMEOUT(button("roundtablePeekTune")->isEnabled(), kWaitMs);
            key(true);
            QTRY_VERIFY_WITH_TIMEOUT(radio->isTransmitting(), kWaitMs);
            QVERIFY(!button("roundtablePeekTune")->isEnabled());
            QVERIFY(open());
            QVERIFY(click(lsb, Qt::RightButton));
            menu = shortMenu();
            QVERIFY(menu != nullptr);
            QAction* keyedTune = menu->findChild<QAction*>(QStringLiteral("roundtableMenuTune"));
            QVERIFY(!keyedTune->isEnabled());
            keyedTune->trigger();
            closeMenu(menu);
            QCOMPARE(std::llround(slice->frequency()), home);
            QVERIFY(open());
            key(false);
            QTRY_VERIFY_WITH_TIMEOUT(!radio->isTransmitting(), kWaitMs);
            QTRY_VERIFY_WITH_TIMEOUT(button("roundtablePeekTune")->isEnabled(), kWaitMs);
            button("roundtablePeekClose")->click();
        }

        // ── A band click tunes, and leaves it open showing the same ──
        QVERIFY(click(viewed));
        const qsizetype beforeBand = closed.size();
        const QRect spec = pan->roundtableTagSpectrumRectForTest();
        QSignalSpy bandTune(pan, &SpectrumWidget::frequencyClicked);
        QTest::mouseClick(pan, Qt::LeftButton, Qt::NoModifier, QPoint(spec.left() + 300, spec.center().y()));
        QCOMPARE(bandTune.size(), 1);
        QVERIFY(open());
        QCOMPARE(peek()->hz(), quint64(viewedHz));
        QVERIFY(!peek()->showsOwn());
        QCOMPARE(panel->roomId(), viewed);
        QCOMPARE(closed.size(), beforeBand);
        // The band click really moved the slice, all the way to the radio.
        const qint64 clicked = std::llround(slice->frequency());
        QVERIFY(clicked != home);
        QTRY_COMPARE_WITH_TIMEOUT(std::llround(coreSlice->frequency()), clicked, kWaitMs);
        coreSlice->setFrequency(static_cast<double>(home));
        QTRY_COMPARE_WITH_TIMEOUT(std::llround(slice->frequency()), home, kWaitMs);
        settle();
        QVERIFY(open());
        QCOMPARE(panel->roomId(), viewed);
        // Landing on the viewed frequency turns it into your own, in place.
        coreSlice->setFrequency(static_cast<double>(viewedHz));
        QTRY_VERIFY_WITH_TIMEOUT(peek()->showsOwn(), kWaitMs);
        QCOMPARE(peek()->hz(), quint64(viewedHz));
        QCOMPARE(closed.size(), beforeBand);
        QVERIFY(!panel->isReading());
        coreSlice->setFrequency(static_cast<double>(home));
        QTRY_COMPARE_WITH_TIMEOUT(std::llround(slice->frequency()), home, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(peek()->hz(), quint64(home), kWaitMs);
        button("roundtablePeekClose")->click();
        QVERIFY(!open());

        // ── Tune moves the slice in the right mode, closes it, and nothing else ──
        SpectrumWidget::RoundtableTagHit lsbHit;
        QTRY_VERIFY_WITH_TIMEOUT(hitOf(lsb, &lsbHit), kWaitMs);
        QVERIFY(click(lsb));
        QVERIFY(open());
        QCOMPARE(button("roundtablePeekTune")->text(), tuneText);
        QTRY_VERIFY_WITH_TIMEOUT(button("roundtablePeekTune")->isEnabled(), kWaitMs);
        button("roundtablePeekTune")->click();
        QVERIFY(!open());
        QTRY_COMPARE_WITH_TIMEOUT(std::llround(coreSlice->frequency()), lsbHz, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(coreSlice->dspMode(), DSPMode::LSB, kWaitMs);
        QCOMPARE(std::llround(slice->frequency()), lsbHz);
        QCOMPARE(slice->dspMode(), DSPMode::LSB);
        coreSlice->setDspMode(DSPMode::USB);
        coreSlice->setFrequency(static_cast<double>(home));
        QTRY_COMPARE_WITH_TIMEOUT(std::llround(slice->frequency()), home, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(slice->dspMode(), DSPMode::USB, kWaitMs);
        settle();

        // ── The menu's Tune closes it too ──
        QVERIFY(click(viewed));
        QVERIFY(click(other, Qt::RightButton));
        menu = shortMenu();
        QVERIFY(menu != nullptr);
        menu->findChild<QAction*>(QStringLiteral("roundtableMenuTune"))->trigger();
        closeMenu(menu);
        QVERIFY(!open());
        QTRY_COMPARE_WITH_TIMEOUT(std::llround(coreSlice->frequency()), otherHz, kWaitMs);
        coreSlice->setFrequency(static_cast<double>(home));
        QTRY_COMPARE_WITH_TIMEOUT(std::llround(slice->frequency()), home, kWaitMs);
        settle();

        // ── A refused swap closes with the app's message and lets go of both reads ──
        QVERIFY(click(viewed));
        link.refuse = true;
        QVERIFY(click(other));
        QVERIFY(!open());
        link.refuse = false;
        QVERIFY(!panel->isReading());
        QVERIFY(model->readingRoomId().isEmpty());
        QVERIFY(!model->wantedFrequencyContexts().contains(viewed));
        QVERIFY(!model->wantedFrequencyContexts().contains(other));
        QVERIFY(model->wantedFrequencyContexts().contains(mine));
        QVERIFY(nothingMoved());
    }

    // A pan goes away under an open view: the view ends with it, the panel goes
    // back to the slice, and nothing is left running.
    void removingAPanEndsItsView()
    {
        ReadingLink link;
        RoundtableState state;
        Everything p;
        SliceModel* slice = p.model()->sliceById(p.sliceId());
        QVERIFY(slice);
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14200000);
        RoundtableModel* model = p.model()->roundtableModel();
        QVERIFY(model);
        model->attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        const QString viewed = RoundtableModel::frequencyContextId(14230000, QStringLiteral("usb"));
        model->applyRooms({{"frequencyVersion", 1}, {"rooms", QJsonArray{QJsonObject{{"id", "usb-14230000"},
            {"stations", QJsonArray{QJsonObject{{"sourceRoom", "usb-14230000"}, {"ref", "q"}, {"callsign", "W2ABC"},
                {"hz", 14230000}, {"family", "usb"}, {"keyed", false}}}}}}}});
        RoundtableApplet* applet = appletOf(p.window());
        auto* stack = p.window()->findChild<PanadapterStack*>();
        QVERIFY(applet && stack);
        QVERIFY(stack->addPanadapter(QStringLiteral("pan-going")) != nullptr);
        SpectrumWidget* pan = stack->spectrum(QStringLiteral("pan-going"));
        QVERIFY(pan != nullptr);
        settle();
        // A new pan takes a slice with it, and the pan goes with its flag still on
        // it: the window must not be left holding the dead flag.
        QVERIFY(!pan->findChildren<VfoWidget*>().isEmpty());
        emit pan->roundtableReadRequested(viewed);
        QPointer<RoundtableFrequencyPeek> peek = pan->findChild<RoundtableFrequencyPeek*>();
        QVERIFY(peek && peek->isOpen());
        QVERIFY(applet->panel()->isReading());
        auto* peekTimer = p.window()->findChild<QTimer*>(QStringLiteral("roundtablePeekTimer"));
        QVERIFY(peekTimer && peekTimer->isActive());

        stack->removePanadapter(QStringLiteral("pan-going"));
        QTRY_VERIFY_WITH_TIMEOUT(peek.isNull(), kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(!peekTimer->isActive(), kWaitMs);
        QVERIFY(!applet->panel()->isReading());
        QCOMPARE(std::llround(slice->frequency()), 14200000LL);
        // The next refresh walks the window's flags: this crashed on the dead one.
        model->applyRooms({{"frequencyVersion", 1}, {"rooms", QJsonArray{}}});
        settle();
        model->detach();
    }

    // Every flag on the frequency the inspector shows reads open: it opens, swaps,
    // closes, and a slice tuning onto or off the frequency follows. A fold shows no
    // one frequency.
    void everyFlagOnTheInspectorsFrequencyReadsOpen()
    {
        ReadingLink link;
        RoundtableState state;
        Everything p;
        RadioModel* radio = p.model();
        const int aId = p.sliceId();
        const int bId = radio->addSlice(QStringLiteral("pan-0"));
        const int cId = radio->addSlice(QStringLiteral("pan-0"));
        SliceModel* a = radio->sliceById(aId);
        SliceModel* b = radio->sliceById(bId);
        SliceModel* c = radio->sliceById(cId);
        QVERIFY(a && b && c);
        const qint64 home = 14200000;
        const qint64 elsewhere = 14230000;
        for (SliceModel* slice : {a, b, c}) { slice->setDspMode(DSPMode::USB); }
        a->setFrequency(home);
        b->setFrequency(home);
        c->setFrequency(elsewhere);
        RoundtableModel* model = radio->roundtableModel();
        QVERIFY(model);
        model->attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        model->applyRooms({{"frequencyVersion", 1}, {"rooms", QJsonArray{}}});
        radio->setActiveSliceById(aId);
        settle();
        SpectrumWidget* pan = panOf(p.window());
        QVERIFY(pan != nullptr);
        VfoWidget* flagA = flagFor(p.window(), aId);
        VfoWidget* flagB = flagFor(p.window(), bId);
        VfoWidget* flagC = flagFor(p.window(), cId);
        QVERIFY(flagA && flagB && flagC);
        const auto states = [&]() {
            return QList<bool>{flagA->roundtableLineOpen(), flagB->roundtableLineOpen(), flagC->roundtableLineOpen()};
        };
        QCOMPARE(states(), (QList<bool>{false, false, false}));

        // Opens on your own: both flags on that frequency.
        const QString homeContext = RoundtableModel::frequencyContextId(home, QStringLiteral("usb"));
        emit pan->roundtableReadRequested(homeContext);
        settle();
        auto* peek = pan->findChild<RoundtableFrequencyPeek*>();
        QVERIFY(peek != nullptr && peek->isOpen());
        QCOMPARE(states(), (QList<bool>{true, true, false}));

        // Swaps in place to another frequency: the lines swap too.
        emit pan->roundtableReadRequested(RoundtableModel::frequencyContextId(elsewhere, QStringLiteral("usb")));
        settle();
        QVERIFY(peek->isOpen());
        QCOMPARE(states(), (QList<bool>{false, false, true}));

        // A slice tuning off the shown frequency closes its line; back on opens it.
        c->setFrequency(elsewhere + 1000);
        QCOMPARE(states(), (QList<bool>{false, false, false}));
        c->setFrequency(elsewhere);
        QCOMPARE(states(), (QList<bool>{false, false, true}));
        // Another mode family is another frequency.
        c->setDspMode(DSPMode::LSB);
        QCOMPARE(states(), (QList<bool>{false, false, false}));
        c->setDspMode(DSPMode::USB);
        QCOMPARE(states(), (QList<bool>{false, false, true}));

        // Closes: none.
        peek->closePeek();
        QCOMPARE(states(), (QList<bool>{false, false, false}));
        model->detach();
    }

    // Overlay design D40, D41: the flag and the panel's meter set one choice for
    // this window, every flag follows, and nothing reaches a slice or the radio.
    void aFlagsSMeterChoiceIsThisWindowsAloneOnARadio()
    {
        Everything p;
        const int second = p.model()->addSlice(QStringLiteral("pan-0"));
        settle();
        runSMeterSource(p.window(), p.model(), p.core(), {p.sliceId(), second});
    }

    void aFlagsSMeterChoiceIsThisWindowsAloneOnACore()
    {
        WindowOfCore p;
        QVERIFY(p.ok());
        QTRY_VERIFY_WITH_TIMEOUT(p.harness().client()->isHandshakeComplete(), kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(p.model()->sliceById(0) != nullptr, kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(flagFor(p.window(), 0) != nullptr, kWaitMs);
        runSMeterSource(p.window(), p.model(), p.core(), {0});
    }

    void theInspectorsRoutesMoveNothingInTheEverythingPlacement()
    {
        ReadingLink link;
        RoundtableState state;
        Everything p;
        RoundtableModel* model = p.model()->roundtableModel();
        QVERIFY(model);
        model->attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        const auto key = [&p](bool on) {
            p.server().setRemoteTransmitAllowed(true);
            p.core()->transmitModel().setMicSource(MicSource::Radio);
            p.core()->moxController()->setMox(on);
        };
        runInspectorRouting(p.window(), p.model(), p.core(), p.sliceId(), model, link, key);
        model->detach();
    }

    void theInspectorsRoutesMoveNothingInAWindowOfTheCore()
    {
        WindowOfCore p;
        QVERIFY(p.ok());
        QTRY_VERIFY_WITH_TIMEOUT(p.harness().client()->isHandshakeComplete(), kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(p.model()->sliceById(0) != nullptr, kWaitMs);
        p.model()->setActiveSliceById(0);
        QTRY_VERIFY_WITH_TIMEOUT(p.model()->activeSlice() != nullptr && p.model()->activeSlice()->sliceIndex() == 0, kWaitMs);
        ReadingLink link;
        RoundtableState state;
        RoundtableModel* model = p.model()->roundtableModel();
        QVERIFY(model);
        model->attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        const auto key = [&p](bool on) {
            p.server().setRemoteTransmitAllowed(true);
            p.core()->transmitModel().setMicSource(MicSource::Radio);
            p.core()->moxController()->setMox(on);
        };
        runInspectorRouting(p.window(), p.model(), p.core(), 0, model, link, key);
        model->detach();
    }

    // D34: your own frequency stands on your slice's pan: when the active slice moves
    // to another, the inspector follows and no two boxes show it.
    void yourOwnInspectorFollowsTheActiveSliceToItsPan()
    {
        ReadingLink link;
        RoundtableState state;
        Everything p;
        SliceModel* slice = p.model()->sliceById(p.sliceId());
        QVERIFY(slice);
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14200000);
        RoundtableModel* model = p.model()->roundtableModel();
        QVERIFY(model);
        model->attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        model->applyRooms({{"frequencyVersion", 1}, {"rooms", QJsonArray{}}});
        auto* stack = p.window()->findChild<PanadapterStack*>();
        QVERIFY(stack != nullptr);
        SpectrumWidget* first = panOf(p.window());
        QVERIFY(first != nullptr);
        QVERIFY(stack->addPanadapter(QStringLiteral("pan-second")) != nullptr);
        SpectrumWidget* second = stack->spectrum(QStringLiteral("pan-second"));
        QVERIFY(second != nullptr && second != first);
        settle();
        const QList<VfoWidget*> flags = second->findChildren<VfoWidget*>();
        QCOMPARE(flags.size(), 1);
        const int otherId = flags.first()->sliceIndex();
        QVERIFY(otherId != p.sliceId());
        SliceModel* other = p.model()->sliceById(otherId);
        QVERIFY(other);
        other->setDspMode(DSPMode::USB);
        other->setFrequency(14230000);
        p.model()->setActiveSliceById(p.sliceId());
        settle();
        emit first->roundtableReadRequested(RoundtableModel::frequencyContextId(14200000, QStringLiteral("usb")));
        auto* before = first->findChild<RoundtableFrequencyPeek*>();
        QVERIFY(before != nullptr && before->showsOwn());
        const QString beforeRoom = first->roundtableViewedRoom();
        QVERIFY(!beforeRoom.isEmpty());

        p.model()->setActiveSliceById(otherId);
        QTRY_VERIFY_WITH_TIMEOUT(second->findChild<RoundtableFrequencyPeek*>() != nullptr
                                     && second->findChild<RoundtableFrequencyPeek*>()->showsOwn(), kWaitMs);
        auto* after = second->findChild<RoundtableFrequencyPeek*>();
        QCOMPARE(after->hz(), quint64(14230000));
        QCOMPARE(second->roundtableInspectorRect(), after->inspectorRect());
        QVERIFY(first->roundtableInspectorRect().isNull());
        QVERIFY(first->roundtableViewedRoom().isEmpty());
        // The old one is closed, on its way out: one box.
        const auto* left = first->findChild<RoundtableFrequencyPeek*>();
        QVERIFY(left == nullptr || !left->isOpen());
        after->closePeek();
        stack->removePanadapter(QStringLiteral("pan-second"));
        settle();
        model->detach();
    }

    // Tune's reason names the cause: a dropped radio is not a transmission.
    void aMissingRadioIsNotCalledTransmitting()
    {
        ReadingLink link;
        RoundtableState state;
        Everything p;
        SliceModel* slice = p.model()->sliceById(p.sliceId());
        QVERIFY(slice);
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14200000);
        RoundtableModel* model = p.model()->roundtableModel();
        QVERIFY(model);
        model->attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        model->applyRooms({{"frequencyVersion", 1}, {"rooms", QJsonArray{}}});
        SpectrumWidget* pan = panOf(p.window());
        QVERIFY(pan != nullptr);
        settle();
        emit pan->roundtableReadRequested(RoundtableModel::frequencyContextId(14230000, QStringLiteral("usb")));
        auto* peek = pan->findChild<RoundtableFrequencyPeek*>();
        QVERIFY(peek != nullptr && peek->isOpen());
        auto* tune = peek->findChild<QAbstractButton*>(QStringLiteral("roundtablePeekTune"));
        QVERIFY(tune != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(tune->isEnabled(), kWaitMs);
        p.model()->setConnectionStateForTest(ConnectionState::Disconnected);
        QTRY_VERIFY_WITH_TIMEOUT(!tune->isEnabled(), kWaitMs);
        QCOMPARE(tune->toolTip(), QStringLiteral("No radio connected"));
        QVERIFY(!p.model()->isTransmitting());
        peek->closePeek();
        model->detach();
    }

    // Disabled, never hidden: a flag line's right click in a mode with no Roundtable
    // frequency shows the short menu, greyed.
    void aFlagLineWithNoFrequencyShowsTheMenuGreyed()
    {
        Everything p;
        SliceModel* slice = p.model()->sliceById(p.sliceId());
        QVERIFY(slice);
        slice->setDspMode(DSPMode::SPEC);
        settle();
        SpectrumWidget* pan = panOf(p.window());
        QVERIFY(pan != nullptr);
        VfoWidget* flag = pan->vfoWidget(p.sliceId());
        QVERIFY(flag != nullptr);
        emit flag->roundtableLineMenuRequested(p.window()->mapToGlobal(QPoint(200, 200)));
        const QList<QMenu*> menus = p.window()->findChildren<QMenu*>(QStringLiteral("roundtableShortMenu"), Qt::FindDirectChildrenOnly);
        QCOMPARE(menus.size(), 1);
        QStringList rows;
        for (QAction* row : menus.first()->actions()) {
            if (!row->isSeparator()) {
                QVERIFY(!row->isEnabled());
                rows.append(row->text());
            }
        }
        QCOMPARE(rows, (QStringList{QStringLiteral("Roundtable"), QStringLiteral("Open inspector"),
                                    QStringLiteral("Tune slice here"), QStringLiteral("Messages")}));
        menus.first()->close();
    }

    // The flag bar follows the analog meter's choice, on the window's real selector.
    void theFlagsSourceFollowsTheMeterChoice()
    {
        Everything p;
        SliceMeterPump* pump = p.core()->sliceMeterPump();
        QVERIFY(pump != nullptr);
        auto* meter = p.window()->findChild<SMeterWidget*>();
        QVERIFY(meter != nullptr);
        const auto source = [&](const char* mode) {
            meter->setRxMode(QString::fromLatin1(mode));
            return pump->selectedSourceForTest();
        };
        QCOMPARE(source("S-Meter"), std::optional(SliceMeterPump::MeterSource::SignalPeak));
        QCOMPARE(source("S-Meter Peak"), std::optional(SliceMeterPump::MeterSource::SignalPeak));
        QCOMPARE(source("Sig Avg"), std::optional(SliceMeterPump::MeterSource::SignalAverage));
        QCOMPARE(source("Max Bin"), std::optional(SliceMeterPump::MeterSource::MaxBin));
        meter->setRxMode(QStringLiteral("S-Meter"));
    }

    // D37: a pan that does not hold the active slice places it by its own slice's flag.
    void aPanWithoutTheActiveSlicePlacesByItsOwnFlag()
    {
        ReadingLink link;
        RoundtableState state;
        Everything p;
        SliceModel* slice = p.model()->sliceById(p.sliceId());
        QVERIFY(slice);
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(14200000);
        RoundtableModel* model = p.model()->roundtableModel();
        QVERIFY(model);
        model->attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        model->applyRooms({{"frequencyVersion", 1}, {"rooms", QJsonArray{}}});
        auto* stack = p.window()->findChild<PanadapterStack*>();
        QVERIFY(stack != nullptr);
        QVERIFY(stack->addPanadapter(QStringLiteral("pan-second")) != nullptr);
        SpectrumWidget* second = stack->spectrum(QStringLiteral("pan-second"));
        QVERIFY(second != nullptr);
        settle();
        QVERIFY(second->vfoWidget(p.sliceId()) == nullptr);
        const QList<VfoWidget*> flags = second->findChildren<VfoWidget*>();
        QVERIFY(!flags.isEmpty());
        p.model()->setActiveSliceById(p.sliceId());
        settle();
        emit second->roundtableReadRequested(RoundtableModel::frequencyContextId(14230000, QStringLiteral("usb")));
        auto* peek = second->findChild<RoundtableFrequencyPeek*>();
        QVERIFY(peek != nullptr && peek->isOpen());
        const SpectrumWidget::RoundtableInspectorSpot spot = second->roundtableInspectorSpot(p.sliceId(), 14230000);
        QVERIFY(!spot.flags.isEmpty());
        QCOMPARE(second->roundtableInspectorRect(), peek->inspectorRect());
        QCOMPARE(peek->inspectorRect(), placeRoundtableInspector(spot.pan, spot.dialX, spot.flags, spot.flagFlipped, peek->height()));
        // A resize moves it with the spot, not by the one it had.
        second->resize(second->width() - 60, second->height());
        settle();
        const SpectrumWidget::RoundtableInspectorSpot after = second->roundtableInspectorSpot(p.sliceId(), 14230000);
        QVERIFY(after.pan.contains(peek->inspectorRect()));
        QCOMPARE(second->roundtableInspectorRect(), peek->inspectorRect());
        // A view on another pan: a change of pan is a close and an open.
        peek->closePeek();
        QVERIFY(second->roundtableInspectorRect().isNull());
        stack->removePanadapter(QStringLiteral("pan-second"));
        settle();
        model->detach();
    }

    // A window hosting the Core selects for itself: another slice's flag line
    // views it, a tag views too, and neither moves a slice or the selection.
    void aHostingWindowViewsAnotherSliceAndMovesNothing()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        AppSettings::instance().setValue(QStringLiteral("PanLayoutId"), QStringLiteral("1"));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* radio = window.radioModel();
        radio->setBoardForTest(HPSDRHW::Saturn);
        radio->configureStreamPool(5, 5, 192000);
        radio->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = radio->addSlice(QStringLiteral("pan-0"));
        const int bId = radio->addSlice(QStringLiteral("pan-0"));
        SliceModel* a = radio->sliceById(aId);
        SliceModel* b = radio->sliceById(bId);
        QVERIFY(a && b);
        a->setDspMode(DSPMode::USB);
        b->setDspMode(DSPMode::USB);
        a->setFrequency(14200000);
        b->setFrequency(14230000);
        window.resize(1512, 869);
        window.show();
        StationHostOptions options;
        options.settings = &settings;
        options.securityDirectory = directory.path();
        options.hostingDevice = StationHostOptions::HostingDevice{QStringLiteral("Shack Mac mini"), QStringLiteral("Shack")};
        options.remoteBind = QStringLiteral("127.0.0.1");
        options.statusPage = false;
        options.remotePort = unusedLoopbackPort();
        DesktopStationController controller(radio, options);
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        SliceOwnership* ownership = radio->sliceOwnership();
        QVERIFY(radio->setActiveRxFor(SliceOwnership::stationDevice(), aId));
        QCOMPARE(ownership->activeFor(SliceOwnership::stationDevice()), aId);

        ReadingLink link;
        RoundtableState state;
        RoundtableModel* model = radio->roundtableModel();
        QVERIFY(model);
        model->attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        const QString viewed = RoundtableModel::frequencyContextId(14188600, QStringLiteral("usb"));
        model->applyRooms({{"frequencyVersion", 1}, {"rooms", QJsonArray{QJsonObject{{"id", "usb-14188600"},
            {"stations", QJsonArray{QJsonObject{{"sourceRoom", "usb-14188600"}, {"ref", "q"}, {"callsign", "W2ABC"},
                {"hz", 14188600}, {"family", "usb"}, {"keyed", false}}}}}}}});
        SpectrumWidget* pan = panOf(&window);
        RoundtableApplet* applet = appletOf(&window);
        QVERIFY(pan && applet);
        pan->setFrequencyRange(14200000.0, kPanSpanHz);
        settle();
        const auto peek = [&]() { return pan->findChild<RoundtableFrequencyPeek*>(); };
        const auto open = [&]() { return peek() != nullptr && peek()->isOpen(); };
        const auto nothingMoved = [&]() {
            return ownership->activeFor(SliceOwnership::stationDevice()) == aId
                && ownership->activeRxFor(SliceOwnership::stationDevice()) == aId
                && radio->activeSlice() == a && std::llround(a->frequency()) == 14200000
                && std::llround(b->frequency()) == 14230000;
        };

        // Another slice's flag line.
        VfoWidget* flagB = flagFor(&window, bId);
        QVERIFY(flagB != nullptr && flagB->roundtableLineButtonForTest() != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(flagB->roundtableLineButtonForTest()->isVisible(), kWaitMs);
        QTest::mouseClick(flagB->roundtableLineButtonForTest(), Qt::LeftButton);
        QVERIFY(open());
        QCOMPARE(peek()->hz(), quint64(14230000));
        QVERIFY(nothingMoved());
        QVERIFY(link.reads.contains(QStringLiteral("usb-14230000")));

        // A tag, on the same pan.
        QImage image(pan->size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::black);
        {
            QPainter painter(&image);
            pan->drawRoundtableTagsForTest(painter, pan->roundtableTagSpectrumRectForTest());
        }
        bool clicked = false;
        for (const SpectrumWidget::RoundtableTagHit& hit : pan->roundtableTagHitsForTest()) {
            if (hit.roomIds.contains(viewed)) {
                QTest::mouseClick(pan, Qt::LeftButton, Qt::NoModifier, hit.rect.center());
                clicked = true;
                break;
            }
        }
        QVERIFY(clicked);
        QVERIFY(open());
        QCOMPARE(peek()->hz(), quint64(14188600));
        QVERIFY(nothingMoved());

        // The window selects B: now A's line is the other one, and B's is its own.
        QVERIFY(radio->setActiveRxFor(SliceOwnership::stationDevice(), bId));
        settle();
        VfoWidget* flagA = flagFor(&window, aId);
        QVERIFY(flagA != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT(flagA->roundtableLineButtonForTest()->isVisible(), kWaitMs);
        QTest::mouseClick(flagA->roundtableLineButtonForTest(), Qt::LeftButton);
        QVERIFY(open());
        QCOMPARE(peek()->hz(), quint64(14200000));
        QCOMPARE(ownership->activeFor(SliceOwnership::stationDevice()), bId);
        QCOMPARE(std::llround(a->frequency()), 14200000LL);
        QCOMPARE(std::llround(b->frequency()), 14230000LL);
        // The same line again closes it, with nothing moved.
        QTest::mouseClick(flagA->roundtableLineButtonForTest(), Qt::LeftButton);
        QVERIFY(!open());
        QCOMPARE(ownership->activeFor(SliceOwnership::stationDevice()), bId);
        QCOMPARE(std::llround(a->frequency()), 14200000LL);

        // Every route in a hosting window, on its own slice.
        QVERIFY(radio->setActiveRxFor(SliceOwnership::stationDevice(), aId));
        settle();
        const auto key = [radio](bool on) {
            radio->transmitModel().setMicSource(MicSource::Radio);
            radio->moxController()->setMox(on);
        };
        runInspectorRouting(&window, radio, radio, aId, model, link, key);
        QCOMPARE(ownership->activeFor(SliceOwnership::stationDevice()), aId);
        QCOMPARE(std::llround(b->frequency()), 14230000LL);

        model->detach();
        window.setDesktopStationController(nullptr);
    }

    // A hosting window whose phone holds transmit and chose its own slice: the
    // radio's active slice is the phone's. The panel's label, its stream and Tune
    // follow this window's own slice, and it never moves a slice it does not control.
    void aHostingWindowFollowsItsOwnSliceAndNeverThePhones()
    {
        if (!QSslSocket::supportsSsl()) { QSKIP("Qt reports no working TLS backend."); }
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppSettings settings(directory.filePath(QStringLiteral("station.settings")));
        AppSettings::instance().setValue(QStringLiteral("PanLayoutId"), QStringLiteral("1"));
        MainWindow window({}, nullptr, MainWindow::ConnectionStartup::Deferred);
        RadioModel* radio = window.radioModel();
        radio->setBoardForTest(HPSDRHW::Saturn);
        radio->configureStreamPool(5, 5, 192000);
        radio->setConnectionStateForTest(ConnectionState::Connected);
        const int aId = radio->addSlice(QStringLiteral("pan-0"));
        const int bId = radio->addSlice(QStringLiteral("pan-0"));
        const int cId = radio->addSlice(QStringLiteral("pan-0"));
        SliceModel* a = radio->sliceById(aId);
        SliceModel* b = radio->sliceById(bId);
        SliceModel* c = radio->sliceById(cId);
        QVERIFY(a && b && c);
        for (SliceModel* slice : {a, b, c}) { slice->setDspMode(DSPMode::USB); }
        a->setFrequency(14200000);
        b->setFrequency(14230000);
        c->setFrequency(14210000);
        window.resize(1512, 869);
        window.show();
        StationHostOptions options;
        options.settings = &settings;
        options.securityDirectory = directory.path();
        options.hostingDevice = StationHostOptions::HostingDevice{QStringLiteral("Shack Mac mini"), QStringLiteral("Shack")};
        options.remoteBind = QStringLiteral("127.0.0.1");
        options.statusPage = false;
        options.remotePort = unusedLoopbackPort();
        DesktopStationController controller(radio, options);
        window.setDesktopStationController(&controller);
        QVERIFY(controller.start(true));
        ReadingLink link;
        RoundtableState state;
        RoundtableModel* model = radio->roundtableModel();
        QVERIFY(model);
        model->attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        const QString viewed = RoundtableModel::frequencyContextId(14188600, QStringLiteral("usb"));
        model->applyRooms({{"frequencyVersion", 1}, {"rooms", QJsonArray{QJsonObject{{"id", "usb-14188600"},
            {"stations", QJsonArray{QJsonObject{{"sourceRoom", "usb-14188600"}, {"ref", "q"}, {"callsign", "W2ABC"},
                {"hz", 14188600}, {"family", "usb"}, {"keyed", false}}}}}}}});

        SliceOwnership* ownership = radio->sliceOwnership();
        const QByteArray phone = QByteArrayLiteral("token:phone");
        const QByteArray station = SliceOwnership::stationDevice();
        QVERIFY(radio->setActiveRxFor(station, aId));
        ownership->setOwner(bId, phone);
        QVERIFY(radio->setActiveSliceByIdFor(phone, bId));
        radio->setTransmitHolder(phone);
        settle();
        QCOMPARE(radio->activeSlice(), b);
        QCOMPARE(ownership->activeFor(station), aId);

        SpectrumWidget* pan = panOf(&window);
        RoundtableApplet* applet = appletOf(&window);
        QVERIFY(pan && applet);
        RoundtablePanel* panel = applet->panel();
        pan->setFrequencyRange(14200000.0, kPanSpanHz);
        settle();
        const auto contextOf = [](const SliceModel* slice) {
            return RoundtableModel::frequencyContextId(std::llround(slice->frequency()), QStringLiteral("usb"));
        };

        // The stream, the card and the flag are A's.
        QCOMPARE(panel->roomId(), contextOf(a));
        QVERIFY(link.reads.contains(QStringLiteral("usb-14200000")));
        QVERIFY(!link.reads.contains(QStringLiteral("usb-14230000")));
        QCOMPARE(model->readingRoomId(), contextOf(a));

        // Moving the phone's slice is not this window's news.
        b->setFrequency(14231000);
        settle();
        QCOMPARE(panel->roomId(), contextOf(a));

        const auto clickTag = [&]() {
            QImage image(pan->size(), QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::black);
            {
                QPainter painter(&image);
                pan->drawRoundtableTagsForTest(painter, pan->roundtableTagSpectrumRectForTest());
            }
            for (const SpectrumWidget::RoundtableTagHit& hit : pan->roundtableTagHitsForTest()) {
                if (hit.roomIds.contains(viewed)) {
                    QTest::mouseClick(pan, Qt::LeftButton, Qt::NoModifier, hit.rect.center());
                    return true;
                }
            }
            return false;
        };
        const auto peek = [&]() { return pan->findChild<RoundtableFrequencyPeek*>(); };
        const auto tuneButton = [&]() { return peek()->findChild<QPushButton*>(QStringLiteral("roundtablePeekTune")); };

        // The view's label names A, and Tune moves A, not the phone's slice.
        QVERIFY(clickTag());
        QVERIFY(peek() != nullptr && peek()->isOpen());
        QCOMPARE(tuneButton()->text(), QStringLiteral("Tune slice A here"));
        auto* panelTune = panel->findChild<QPushButton*>(QStringLiteral("roundtableTuneHereBtn"));
        QVERIFY(panelTune != nullptr);
        QCOMPARE(panelTune->text(), QStringLiteral("Tune A here"));
        QTRY_VERIFY_WITH_TIMEOUT(tuneButton()->isEnabled(), kWaitMs);
        tuneButton()->click();
        settle();
        QCOMPARE(std::llround(a->frequency()), 14188600LL);
        QCOMPARE(std::llround(b->frequency()), 14231000LL);
        a->setFrequency(14200000);
        settle();

        // This station picks C while the phone holds transmit: the station-level
        // active slice does not change, so activeSliceChanged stays silent.
        QSignalSpy radioActive(radio, &RadioModel::activeSliceChanged);
        QVERIFY(radio->setActiveRxFor(station, cId));
        settle();
        QCOMPARE(radioActive.count(), 0);
        QCOMPARE(radio->activeSlice(), b);
        QCOMPARE(panel->roomId(), contextOf(c));
        QCOMPARE(model->readingRoomId(), contextOf(c));
        QVERIFY(clickTag());
        QVERIFY(peek()->isOpen());
        QTRY_COMPARE_WITH_TIMEOUT(tuneButton()->text(), QStringLiteral("Tune slice C here"), kWaitMs);
        QCOMPARE(panelTune->text(), QStringLiteral("Tune C here"));
        tuneButton()->click();
        settle();
        QCOMPARE(std::llround(c->frequency()), 14188600LL);
        QCOMPARE(std::llround(a->frequency()), 14200000LL);
        QCOMPARE(std::llround(b->frequency()), 14231000LL);
        c->setFrequency(14210000);
        settle();

        // This window controls no slice now: the panel follows nothing, a tag still
        // views, and Tune stays in view, greyed. Nothing moves and nothing is selected.
        ownership->setOwner(aId, phone);
        ownership->setOwner(cId, phone);
        settle();
        QCOMPARE(ownership->activeFor(station), -1);
        QVERIFY(clickTag());
        QVERIFY(peek() != nullptr && peek()->isOpen());
        QCOMPARE(peek()->hz(), quint64(14188600));
        QTRY_COMPARE_WITH_TIMEOUT(tuneButton()->text(), QStringLiteral("Tune slice here"), kWaitMs);
        QVERIFY(tuneButton()->isVisible());
        QVERIFY(!tuneButton()->isEnabled());
        QCOMPARE(panelTune->text(), QStringLiteral("Tune here"));
        QVERIFY(!panelTune->isEnabled());
        QCOMPARE(roundtableViewSubText(peek()->viewForTest()), QStringLiteral("Viewing"));
        const QString captures = qEnvironmentVariable("NEREUS_ROUNDTABLE_CAPTURE_DIR");
        if (!captures.isEmpty()) {
            settle();
            QVERIFY(window.grab().save(captures + QStringLiteral("/no-slice-view-tune-disabled.png")));
        }
        applet->tuneToRoom(viewed);
        settle();
        QVERIFY(peek()->isOpen());
        const qint64 aHz = std::llround(a->frequency());
        const qint64 cHz = std::llround(c->frequency());
        // A flag line of another slice views too, rather than selecting it.
        if (VfoWidget* flag = flagFor(&window, bId); flag != nullptr && flag->roundtableLineButtonForTest() != nullptr) {
            flag->roundtableLineButtonForTest()->click();
            settle();
        }
        QCOMPARE(ownership->activeFor(station), -1);
        QCOMPARE(std::llround(a->frequency()), aHz);
        QCOMPARE(std::llround(b->frequency()), 14231000LL);
        QCOMPARE(std::llround(c->frequency()), cHz);
        if (peek() != nullptr) { peek()->closePeek(); }

        model->detach();
        window.setDesktopStationController(nullptr);
    }

    // The same from a window of the Core: the selection is the Core's, and a view
    // asks it for nothing.
    void aRemoteWindowViewsAnotherSliceAndMovesNothing()
    {
        WindowOfCore p;
        QVERIFY(p.ok());
        SliceModel* coreA = p.core()->sliceById(0);
        QVERIFY(coreA != nullptr);
        coreA->setDspMode(DSPMode::USB);
        coreA->setFrequency(14200000);
        const int otherId = p.core()->addSlice(QStringLiteral("pan-0"));
        QVERIFY(otherId >= 0);
        SliceModel* coreB = p.core()->sliceById(otherId);
        coreB->setDspMode(DSPMode::USB);
        coreB->setFrequency(14230000);
        QTRY_VERIFY_WITH_TIMEOUT(p.harness().client()->isHandshakeComplete(), kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(p.model()->sliceById(otherId) != nullptr, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(std::llround(p.model()->sliceById(otherId)->frequency()), 14230000LL, kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(flagFor(p.window(), otherId) != nullptr, kWaitMs);
        p.model()->setActiveSliceById(0);
        QTRY_VERIFY_WITH_TIMEOUT(p.model()->activeSlice() != nullptr && p.model()->activeSlice()->sliceIndex() == 0, kWaitMs);
        SliceModel* coreActive = p.core()->activeSlice();

        ReadingLink link;
        RoundtableState state;
        RoundtableModel* model = p.model()->roundtableModel();
        QVERIFY(model);
        model->attachStation(&link, &state);
        state.applyStationValue("linkState", static_cast<int>(RoundtableModel::LinkState::Live));
        // This Core has no Roundtable server, and the real client still feeds this
        // model: its rooms stream says "no frequency reads" whenever it lands, which
        // can be after the line below. Hold the capability the test is about.
        QObject capabilityHold;
        connect(model, &RoundtableModel::roomsChanged, &capabilityHold, [model]() {
            if (!model->frequencyAvailable()) {
                model->applyRooms({{"frequencyVersion", 1}, {"rooms", QJsonArray{}}});
            }
        });
        model->applyRooms({{"frequencyVersion", 1}, {"rooms", QJsonArray{}}});
        QVERIFY(model->frequencyAvailable());
        SpectrumWidget* pan = panOf(p.window());
        QVERIFY(pan != nullptr);
        settle();

        VfoWidget* flag = flagFor(p.window(), otherId);
        QTRY_VERIFY_WITH_TIMEOUT(flag->roundtableLineButtonForTest() != nullptr
                                 && flag->roundtableLineButtonForTest()->isVisible(), kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(model->frequencyAvailable(), kWaitMs);
        QTest::mouseClick(flag->roundtableLineButtonForTest(), Qt::LeftButton);
        settle();
        auto* peek = pan->findChild<RoundtableFrequencyPeek*>();
        QVERIFY(peek != nullptr && peek->isOpen());
        QCOMPARE(peek->hz(), quint64(14230000));
        QVERIFY(link.reads.contains(QStringLiteral("usb-14230000")));
        QCOMPARE(p.model()->activeSlice()->sliceIndex(), 0);
        QCOMPARE(p.core()->activeSlice(), coreActive);
        QCOMPARE(std::llround(coreA->frequency()), 14200000LL);
        QCOMPARE(std::llround(coreB->frequency()), 14230000LL);
        // The same line again closes it, and the Core is asked for nothing.
        QTest::mouseClick(flag->roundtableLineButtonForTest(), Qt::LeftButton);
        settle();
        QVERIFY(!peek->isOpen());
        QCOMPARE(p.core()->activeSlice(), coreActive);
        QCOMPARE(std::llround(coreA->frequency()), 14200000LL);
        QCOMPARE(std::llround(coreB->frequency()), 14230000LL);
        model->detach();
    }

    void theWholeFlowInTheEverythingPlacement()
    {
        Everything p;
        QVERIFY(!QTest::currentTestFailed());
        SliceModel* slice = p.model()->sliceById(p.sliceId());
        QVERIFY(slice != nullptr);
        slice->setDspMode(DSPMode::USB);
        slice->setFrequency(kStartHz);
        QVERIFY(appletOf(p.window()) != nullptr);
        runFlow(p, m_everything);
        m_everythingDone = !QTest::currentTestFailed();
        QVERIFY2(m_overruns.isEmpty(), qPrintable(m_overruns.join(QStringLiteral("\n"))));
    }

    void aWindowOfTheCoreShowsTheSameScreens()
    {
        if (!m_everythingDone) {
            QSKIP("The Everything placement's flow did not finish.");
        }
        WindowOfCore p;
        QVERIFY(p.ok());
        SliceModel* coreSlice = p.core()->sliceById(0);
        QVERIFY(coreSlice != nullptr);
        coreSlice->setDspMode(DSPMode::USB);
        coreSlice->setFrequency(kStartHz);
        QTRY_VERIFY_WITH_TIMEOUT(p.harness().client()->isHandshakeComplete(), kWaitMs);
        QTRY_VERIFY_WITH_TIMEOUT(p.model()->sliceById(0) != nullptr, kWaitMs);
        QTRY_COMPARE_WITH_TIMEOUT(std::llround(p.model()->sliceById(0)->frequency()), 14200000LL, kWaitMs);
        // The window's Roundtable callsign is the Core's.
        p.harness().stationSettings().setValue(QStringLiteral("Roundtable/Callsign"), QStringLiteral("KG4VCF"));
        QTRY_COMPARE_WITH_TIMEOUT(AppSettings::instance().value(QStringLiteral("Roundtable/Callsign")).toString(),
                                  QStringLiteral("KG4VCF"), kWaitMs);
        QList<Shot> shots;
        runFlow(p, shots);
        if (QTest::currentTestFailed()) {
            return;
        }
        QVERIFY2(m_overruns.isEmpty(), qPrintable(m_overruns.join(QStringLiteral("\n"))));
        QCOMPARE(shots.size(), m_everything.size());
        for (qsizetype i = 0; i < shots.size(); ++i) {
            QCOMPARE(shots.at(i).step, m_everything.at(i).step);
            if (shots.at(i).screen != m_everything.at(i).screen) {
                const QString message = QStringLiteral("%1\nEverything:\n  %2\nWindow:\n  %3")
                                            .arg(shots.at(i).step,
                                                 m_everything.at(i).screen.join(QStringLiteral("\n  ")),
                                                 shots.at(i).screen.join(QStringLiteral("\n  ")));
                QFAIL(qPrintable(message));
            }
        }
    }
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_Use96Dpi, true);
    TstRoundtableDesktopFlow test;
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&test, argc, argv);
}

#include "tst_roundtable_desktop_flow.moc"
