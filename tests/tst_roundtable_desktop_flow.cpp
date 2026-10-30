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

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
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
#include <QMenu>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRandomGenerator>
#include <QScrollBar>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QWebSocket>
#include <QWebSocketServer>

#include <algorithm>
#include <memory>
#include <vector>

#include "OperatorWording.h"
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
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/RemoteWindowHarness.h"
#include "fakes/UpgradedCoreToken.h"
#include "gui/MainWindow.h"
#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"
#include "gui/applets/RoundtableApplet.h"
#include "gui/roundtable/RoundtableFirstRunDialog.h"
#include "gui/roundtable/RoundtablePanel.h"
#include "gui/roundtable/RoundtableTagLayout.h"
#include "gui/roundtable/RoundtableWindow.h"
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

        // ── Tune into the room by its tag ──
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
