// no-port-check: NereusSDR-original. Rotor control plan Task 4b tests.
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// tests/tst_station_rotor_link.cpp  (NereusSDR)
// =================================================================
//
// The Core's rotor on the station link (remote rotor control v1,
// docs/architecture/2026-10-07-remote-rotor-control-v1.md): the read-only
// `rotor` object, the seven rotor commands and every refusal they give, the
// minor and remoteRotorControlVersion gates, a window's hold ending with
// its session, the tools catalogue's Rotor entry, and a remote window's
// StationClient. The rotor is a fake byte stream; no test opens a serial
// port, starts rotctld or turns a rotor.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-08  J.J. Boyd / KG4VCF  Created (rotor control plan, Task 4b).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF  Rotor control plan Task 4c: a desktop
//                                    running its own radio owns the rotor
//                                    alone (enableStationRotor). AI-assisted
//                                    via Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF  Rotor control plan Task 5: RadioModel
//                                    routes a window's rotor commands to the
//                                    local rotor or the remote Core.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF  Rotor control plan Task 7: the setup
//                                    and presets through the GUI's sink, and
//                                    a refused setup on the accessory route
//                                    ("rotor"), not the slice one.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-08  J.J. Boyd / KG4VCF  Rotor control plan Task 8: refused
//                                    turns take the accessory route too.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest/QtTest>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QPointer>
#include <QRegularExpression>
#include <QTemporaryDir>

#include <cmath>
#include <limits>
#include <memory>

#include "OperatorWording.h"
#include "SessionWait.h"
#include "core/AppSettings.h"
#include "core/ConnectionState.h"
#include "core/RotctldProcess.h"
#include "core/RotorConnection.h"
#include "core/StationRotorController.h"
#include "core/session/SessionCommandDispatcher.h"
#include "core/session/SessionMessages.h"
#include "core/settings/SettingsProxy.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationCatalog.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "models/RadioModel.h"
#include "models/RotorModel.h"

#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;
using RotorState = NereusSDR::RotorLink::RotorModel;

namespace {

const QString kPort = QStringLiteral("/dev/ttyUSB0");
const QString kNoRotor = QStringLiteral("No rotor is set up on this Core.");
const QString kNotConnected = QStringLiteral("The rotor is not connected.");
const QString kAzimuthOnly = QStringLiteral("This rotor turns in azimuth only.");
const QString kNotANumber = QStringLiteral("That heading is not a number.");
const QString kOutOfRange = QStringLiteral("That heading is outside the rotor's range.");
const QString kNoGrid = QStringLiteral("Set your grid square in Setup to turn the beam to spots.");
const QString kUnknownPort = QStringLiteral("That serial port is not on the Core's computer.");
const QString kUnreadable = QStringLiteral("The Core could not read this request.");
const QString kSetupInvalid = QStringLiteral("That rotor setup is not valid.");
const QString kUpdateApp = QStringLiteral("Update this app to turn the rotor on this Core.");
const QString kCoreHasNoRotor =
    QStringLiteral("This Core does not control a rotor. Updating the Core may help.");

// The rotor's byte stream: whatever the Core writes is kept, and a test
// feeds the rotor's replies.
class FakeRotor : public RotorTransport {
public:
    QByteArray written;
    QByteArray pending;
    bool closed = false;

    void open() override { emit opened(); }
    void close() override { closed = true; }
    qint64 write(const QByteArray& bytes) override
    {
        written += bytes;
        return bytes.size();
    }
    QByteArray readAll() override
    {
        QByteArray out = pending;
        pending.clear();
        return out;
    }
    void feed(const QByteArray& bytes)
    {
        pending += bytes;
        emit readyRead();
    }
    QByteArray take()
    {
        QByteArray out = written;
        written.clear();
        return out;
    }
};

// Timers far in the future: the test steps every exchange itself.
RotorConnection::Timing steppedTiming()
{
    RotorConnection::Timing t;
    t.pollStillMs = 3600000;
    t.pollTurningMs = 3599000;
    t.replyTimeoutMs = 3600000;
    t.settleMs = 3600000;
    t.staleMs = 3600000;
    t.reconnectUnitMs = 3600000;
    t.rotctldStartDelayMs = 0;
    return t;
}

MirrorUpdate f64(const char* name, double value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Float64, QVariant(value)};
}
MirrorUpdate i64(const char* name, qint64 value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Int64, QVariant(qlonglong(value))};
}
MirrorUpdate enumArg(const char* name, qint64 value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Enum, QVariant(qlonglong(value))};
}
MirrorUpdate utf8(const char* name, const QString& value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Utf8, QVariant(value)};
}
MirrorUpdate boolean(const char* name, bool value)
{
    return MirrorUpdate{0, name, MirrorWireKind::Bool, QVariant(value)};
}

// configureRotor's ten arguments, the contract's defaults unless changed.
struct Setup {
    qint64 driver{2};
    QString serialPort{kPort};
    qint64 baud{9600};
    QString host;
    qint64 port{4533};
    qint64 hamlibModel{0};
    qint64 axes{0};
    qint64 endStop{1};
    qint64 rangeDeg{360};
    double offsetDeg{0.0};

    QList<MirrorUpdate> arguments() const
    {
        return {enumArg("driver", driver),     utf8("serialPort", serialPort),
                i64("baud", baud),             utf8("host", host),
                i64("port", port),             i64("hamlibModel", hamlibModel),
                enumArg("axes", axes),         enumArg("endStop", endStop),
                i64("rangeDeg", rangeDeg),     f64("offsetDeg", offsetDeg)};
    }
};

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

bool sawRotorObject(const LoopbackTransport* peer)
{
    for (const SessionMessage& m : receivedMessages(peer)) {
        if (m.objectKey == "rotor" || m.className == "RotorModel") {
            return true;
        }
    }
    return false;
}

// The catalogue lists the Rotor tool, last, with `offered` as given.
bool toolsListRotorAt(const QJsonObject& catalog, bool offered)
{
    const QJsonArray tools = catalog.value(QStringLiteral("tools")).toArray();
    if (tools.isEmpty()) {
        return false;
    }
    const QJsonObject o = tools.last().toObject();
    return o.value(QStringLiteral("id")).toString() == QStringLiteral("rotor")
        && o.value(QStringLiteral("label")).toString() == QStringLiteral("Rotor")
        && o.value(QStringLiteral("where")).toString() == QStringLiteral("station")
        && o.value(QStringLiteral("offered")).isBool()
        && o.value(QStringLiteral("offered")).toBool() == offered;
}

// The catalogue offers the Rotor tool.
bool toolsListRotor(const QJsonObject& catalog)
{
    return toolsListRotorAt(catalog, true);
}

// How a Core comes to own its rotor.
enum class Owns {
    Nothing,       // a model with no rotor controller
    Accessories,   // nereusd: enableStationAccessoryIdentity
    RotorOnly,     // a desktop running its own radio: enableStationRotor
};

// A Core: its radio model, its rotor on the fake byte stream (when it owns
// one), and its station server.
struct Core {
    QTemporaryDir dir;
    RadioModel model;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<StationServer> server;
    QPointer<FakeRotor> rotor;
    int peers = 0;

    explicit Core(bool ownsAccessories)
        : Core(ownsAccessories ? Owns::Accessories : Owns::Nothing)
    {
    }

    explicit Core(Owns owns)
    {
        if (owns == Owns::Accessories) {
            model.enableStationAccessoryIdentity();
        } else if (owns == Owns::RotorOnly) {
            model.enableStationRotor();
        }
        if (StationRotorController* controller = model.stationRotorController()) {
            controller->setSerialPortListerForTesting(
                [] { return QStringList{kPort, QStringLiteral("COM4")}; });
            controller->connection()->setTransportFactoryForTesting(
                [this](const RotorTransportTarget&) {
                    auto fake = std::make_unique<FakeRotor>();
                    rotor = fake.get();
                    return std::unique_ptr<RotorTransport>(std::move(fake));
                });
            controller->connection()->setTimingForTesting(steppedTiming());
            // The rotor object looks again with the lister in place.
            model.rotorModel()->bindController(controller);
        }
        RadioInfo info;
        info.macAddress = QStringLiteral("aa:bb:cc:dd:ee:71");
        model.setLastRadioInfoForTest(info);
        model.setConnectionStateForTest(ConnectionState::Connected);
        settings = std::make_unique<AppSettings>(dir.filePath(QStringLiteral("core.settings")));
        server = std::make_unique<StationServer>(
            &model, *settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
    }

    StationRotorController* controller() const { return model.stationRotorController(); }

    // A window signed in with the Core's token at `minor`.
    LoopbackTransport* connect(QObject* owner, quint16 minor)
    {
        ++peers;
        auto* core = new LoopbackTransport(QStringLiteral("core-%1").arg(peers), owner);
        auto* peer = new LoopbackTransport(QStringLiteral("window-%1").arg(peers), owner);
        core->linkTo(peer);
        server->acceptTransport(core);
        peer->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, minor, 0, QStringLiteral("rotor-test"))));
        peer->sendText(SessionMessages::encode(SessionMessages::authRequest(server->token())));
        return peer;
    }

    // A GS-232B rotor connected on the fake, reading `heading`.
    bool connectRotorAt(const QByteArray& heading)
    {
        RotorConfig config;
        config.driver = RotorDriver::Gs232b;
        config.serialPort = kPort;
        QString why;
        if (!controller()->configureRotor(config, &why) || rotor.isNull()) {
            qWarning() << "the rotor did not connect:" << why;
            return false;
        }
        rotor->take();
        rotor->feed("AZ=" + heading + "  EL=000\r\n");
        return controller()->connectionPhase() == RotorConnectionPhase::Connected;
    }
};

// One command from `peer`, its answer.
SessionMessage invoke(LoopbackTransport* peer, const QByteArray& verb,
                      const QList<MirrorUpdate>& arguments)
{
    static quint32 nextId = 5000;
    const quint32 id = ++nextId;
    peer->sendText(SessionMessages::encode(SessionMessages::commandInvoke(verb, id, arguments)));
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

// The answer to `arguments` in the dispatcher alone (no server gate).
SessionMessage dispatch(SessionCommandDispatcher& dispatcher, const QByteArray& verb,
                        const QList<MirrorUpdate>& arguments)
{
    SessionMessage answer;
    answer.reason = QStringLiteral("no answer");
    const auto connection = QObject::connect(
        &dispatcher, &SessionCommandDispatcher::commandResultReady, &dispatcher,
        [&answer](const SessionMessage& message) { answer = message; });
    dispatcher.dispatch(SessionMessages::commandInvoke(verb, 1, arguments));
    QObject::disconnect(connection);
    return answer;
}

} // namespace

class StationRotorLinkTest : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        QLoggingCategory::setFilterRules(QStringLiteral(
            "nereus.rotor.debug=false\nnereus.rotor.info=false\n"
            "nereus.rotor.controller.info=false"));
    }
    void init()
    {
        AppSettings::instance().clear();
        RotctldProcess::setBinaryOverrideForTesting(QString());
    }
    void cleanup()
    {
        RotctldProcess::setBinaryOverrideForTesting(std::nullopt);
        AppSettings::instance().clear();
    }

    // ── The verbs' table rows ───────────────────────────────────────

    void everyRotorVerbCarriesTheRotorCapability()
    {
        const QList<QByteArray> verbs{"setRotorTarget", "turnRotorToCall", "stopRotor",
                                      "nudgeRotor",     "configureRotor",  "disconnectRotor",
                                      "setRotorPresets"};
        for (const QByteArray& verb : verbs) {
            bool found = false;
            for (const CommandVerbSpec& spec : SessionCommandDispatcher::verbSpecs()) {
                if (spec.verb == verb) {
                    found = true;
                    QCOMPARE(spec.capability, QByteArray("remoteRotorControlVersion"));
                    QCOMPARE(spec.capabilityVersion, 1);
                }
            }
            QVERIFY2(found, verb.constData());
        }
    }

    // ── Capability and the gates ────────────────────────────────────

    void theCapabilityIsOneOnlyOnACoreWithARotor()
    {
        Core owning(true);
        QCOMPARE(owning.server->remoteRotorControlVersion(), 1);
        LoopbackTransport* current = owning.connect(this, kRadioIdentitySessionProtocolMinor);
        QTRY_VERIFY(snapshotDone(current));
        QCOMPARE(capabilityOf(current, "remoteRotorControlVersion"), std::optional<qint64>(1));
        QVERIFY(sawRotorObject(current));

        // An older app neither hears of the rotor nor sees its object.
        LoopbackTransport* older = owning.connect(this, kRadioIdentitySessionProtocolMinor - 1);
        QTRY_VERIFY(snapshotDone(older));
        QVERIFY(!capabilityOf(older, "remoteRotorControlVersion").has_value());
        QVERIFY(!sawRotorObject(older));

        // A Core without a rotor controller sends no capability (absent
        // reads as 0) and no object.
        Core plain(false);
        QCOMPARE(plain.server->remoteRotorControlVersion(), 0);
        LoopbackTransport* peer = plain.connect(this, kRadioIdentitySessionProtocolMinor);
        QTRY_VERIFY(snapshotDone(peer));
        QVERIFY(!capabilityOf(peer, "remoteRotorControlVersion").has_value());
        QVERIFY(!sawRotorObject(peer));
        StationCapabilities caps;
        QCOMPARE(caps.remoteRotorControlVersion, 0);
    }

    void anOlderAppAndACoreWithoutARotorAreRefusedPlainly()
    {
        const QList<QPair<QByteArray, QList<MirrorUpdate>>> commands{
            {"setRotorTarget", {f64("azimuthDeg", 90.0), f64("elevationDeg", -1.0)}},
            {"turnRotorToCall", {utf8("call", QStringLiteral("W1AW")), boolean("longPath", false)}},
            {"stopRotor", {}},
            {"nudgeRotor", {enumArg("direction", 1), boolean("active", true)}},
            {"configureRotor", Setup{}.arguments()},
            {"disconnectRotor", {}},
            {"setRotorPresets", {utf8("presets", QStringLiteral("Home\t90"))}}};

        Core owning(true);
        QVERIFY(owning.connectRotorAt("090"));
        LoopbackTransport* older = owning.connect(this, kRadioIdentitySessionProtocolMinor - 1);
        QTRY_VERIFY(snapshotDone(older));
        for (const auto& [verb, args] : commands) {
            const SessionMessage answer = invoke(older, verb, args);
            QVERIFY2(!answer.accepted, verb.constData());
            QCOMPARE(answer.reason, kUpdateApp);
        }
        // Nothing moved or changed.
        QCOMPARE(owning.controller()->motion(), RotorMotion::Stopped);
        QCOMPARE(owning.controller()->targetAzimuthDeg(), -1.0);
        QCOMPARE(owning.controller()->config().driver, RotorDriver::Gs232b);
        QVERIFY(owning.controller()->presets().isEmpty());

        Core plain(false);
        LoopbackTransport* peer = plain.connect(this, kRadioIdentitySessionProtocolMinor);
        QTRY_VERIFY(snapshotDone(peer));
        for (const auto& [verb, args] : commands) {
            const SessionMessage answer = invoke(peer, verb, args);
            QVERIFY2(!answer.accepted, verb.constData());
            QCOMPARE(answer.reason, kCoreHasNoRotor);
        }
        QVERIFY(OperatorWording::isPlain(kUpdateApp));
        QVERIFY(OperatorWording::isPlain(kCoreHasNoRotor));
    }

    // ── Every command and refusal on the wire ──────────────────────

    void theCommandsTurnAndRefuseAsTheContractSays()
    {
        Core core(true);
        LoopbackTransport* peer = core.connect(this, kRadioIdentitySessionProtocolMinor);
        QTRY_VERIFY(snapshotDone(peer));
        StationRotorController* rotor = core.controller();

        // No rotor set up yet.
        for (const auto& [verb, args] : QList<QPair<QByteArray, QList<MirrorUpdate>>>{
                 {"setRotorTarget", {f64("azimuthDeg", 90.0), f64("elevationDeg", -1.0)}},
                 {"turnRotorToCall",
                  {utf8("call", QStringLiteral("W1AW")), boolean("longPath", false)}},
                 {"stopRotor", {}},
                 {"nudgeRotor", {enumArg("direction", 0), boolean("active", true)}}}) {
            const SessionMessage answer = invoke(peer, verb, args);
            QVERIFY2(!answer.accepted, verb.constData());
            QCOMPARE(answer.reason, kNoRotor);
        }

        QVERIFY(core.connectRotorAt("090"));
        // A heading: accepted, sent to the rotor.
        SessionMessage answer =
            invoke(peer, "setRotorTarget", {f64("azimuthDeg", 180.0), f64("elevationDeg", -1.0)});
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QVERIFY(answer.reason.isEmpty());
        QCOMPARE(rotor->targetAzimuthDeg(), 180.0);
        QVERIFY(!core.rotor->take().isEmpty());

        // Refusals change nothing.
        const auto refused = [&](const QList<MirrorUpdate>& args, const QString& reason) {
            const SessionMessage a = invoke(peer, "setRotorTarget", args);
            QVERIFY(!a.accepted);
            QCOMPARE(a.reason, reason);
            QCOMPARE(rotor->targetAzimuthDeg(), 180.0);
            QVERIFY(core.rotor->take().isEmpty());
        };
        refused({f64("azimuthDeg", 90.0), f64("elevationDeg", 10.0)}, kAzimuthOnly);
        refused({f64("azimuthDeg", std::numeric_limits<double>::quiet_NaN()),
                 f64("elevationDeg", -1.0)},
                kNotANumber);
        refused({f64("azimuthDeg", 400.0), f64("elevationDeg", -1.0)}, kOutOfRange);
        refused({i64("azimuthDeg", 90), f64("elevationDeg", -1.0)}, kUnreadable);
        refused({f64("azimuthDeg", 90.0)}, kUnreadable);

        // A callsign with no grid square set.
        answer = invoke(peer, "turnRotorToCall",
                        {utf8("call", QStringLiteral("W1AW")), boolean("longPath", false)});
        QVERIFY(!answer.accepted);
        QCOMPARE(answer.reason, kNoGrid);
        QCOMPARE(rotor->targetAzimuthDeg(), 180.0);
        answer = invoke(peer, "turnRotorToCall", {utf8("call", QStringLiteral("W1AW"))});
        QCOMPARE(answer.reason, kUnreadable);

        // Stop.
        answer = invoke(peer, "stopRotor", {});
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QCOMPARE(core.rotor->take(), QByteArray("S\r"));
        answer = invoke(peer, "stopRotor", {boolean("now", true)});
        QCOMPARE(answer.reason, kUnreadable);

        // A held turn, then let go.
        answer = invoke(peer, "nudgeRotor", {enumArg("direction", 1), boolean("active", true)});
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QCOMPARE(rotor->motion(), RotorMotion::Nudging);
        answer = invoke(peer, "nudgeRotor", {enumArg("direction", 1), boolean("active", false)});
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QCOMPARE(rotor->motion(), RotorMotion::Stopped);
        answer = invoke(peer, "nudgeRotor", {enumArg("direction", 3), boolean("active", true)});
        QVERIFY(!answer.accepted);
        QCOMPARE(answer.reason, kAzimuthOnly);
        QCOMPARE(rotor->motion(), RotorMotion::Stopped);
        for (const qint64 direction : {qint64(-1), qint64(4)}) {
            answer = invoke(peer, "nudgeRotor",
                            {enumArg("direction", direction), boolean("active", true)});
            QCOMPARE(answer.reason, kUnreadable);
        }
        answer = invoke(peer, "nudgeRotor", {i64("direction", 1), boolean("active", true)});
        QCOMPARE(answer.reason, kUnreadable);
        QCOMPARE(rotor->motion(), RotorMotion::Stopped);

        // Presets.
        answer = invoke(peer, "setRotorPresets",
                        {utf8("presets", QStringLiteral("Home\t90\nEurope\t45"))});
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QCOMPARE(rotor->presets(), QStringLiteral("Home\t90\nEurope\t45"));
        answer = invoke(peer, "setRotorPresets", {utf8("presets", QStringLiteral("Home\tnorth"))});
        QCOMPARE(answer.reason, kNotANumber);
        answer = invoke(peer, "setRotorPresets", {utf8("presets", QStringLiteral("Home\t400"))});
        QCOMPARE(answer.reason, kOutOfRange);
        answer = invoke(peer, "setRotorPresets", {i64("presets", 1)});
        QCOMPARE(answer.reason, kUnreadable);
        QCOMPARE(rotor->presets(), QStringLiteral("Home\t90\nEurope\t45"));

        // Disconnect keeps the setup; a heading is then refused.
        answer = invoke(peer, "disconnectRotor", {});
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QCOMPARE(rotor->connectionPhase(), RotorConnectionPhase::Disconnected);
        QCOMPARE(rotor->config().driver, RotorDriver::Gs232b);
        answer = invoke(peer, "setRotorTarget",
                        {f64("azimuthDeg", 90.0), f64("elevationDeg", -1.0)});
        QCOMPARE(answer.reason, kNotConnected);
        answer = invoke(peer, "disconnectRotor", {boolean("now", true)});
        QCOMPARE(answer.reason, kUnreadable);

        // Every refusal reads plainly.
        for (const QString& reason : {kNoRotor, kNotConnected, kAzimuthOnly, kNotANumber,
                                      kOutOfRange, kNoGrid, kUnknownPort, kUnreadable,
                                      kSetupInvalid}) {
            QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        }
    }

    void aSetupOutsideTheTablesIsRefusedBeforeTheController()
    {
        Core core(true);
        LoopbackTransport* peer = core.connect(this, kRadioIdentitySessionProtocolMinor);
        QTRY_VERIFY(snapshotDone(peer));
        StationRotorController* rotor = core.controller();

        QList<Setup> invalid;
        for (const qint64 driver : {qint64(-1), qint64(5), qint64(9)}) {
            Setup s;
            s.driver = driver;
            invalid.append(s);
        }
        for (const qint64 axes : {qint64(-1), qint64(2)}) {
            Setup s;
            s.axes = axes;
            invalid.append(s);
        }
        for (const qint64 endStop : {qint64(-1), qint64(3)}) {
            Setup s;
            s.endStop = endStop;
            invalid.append(s);
        }
        for (const qint64 port : {qint64(0), qint64(65536), qint64(-4533)}) {
            Setup s;
            s.port = port;
            invalid.append(s);
        }
        // Integers that do not fit an int, for each integer argument.
        constexpr qint64 kTooWide = qint64(std::numeric_limits<int>::max()) + 1;
        constexpr qint64 kTooNarrow = qint64(std::numeric_limits<int>::min()) - 1;
        for (const qint64 wide : {kTooWide, kTooNarrow}) {
            Setup s;
            s.driver = wide;
            invalid.append(s);
            s = Setup{};
            s.axes = wide;
            invalid.append(s);
            s = Setup{};
            s.endStop = wide;
            invalid.append(s);
            s = Setup{};
            s.port = wide;
            invalid.append(s);
            s = Setup{};
            s.baud = wide;
            invalid.append(s);
            s = Setup{};
            s.hamlibModel = wide;
            invalid.append(s);
            s = Setup{};
            s.rangeDeg = wide;
            invalid.append(s);
        }
        // A range other than 360 or 450.
        for (const qint64 range : {qint64(0), qint64(359), qint64(361), qint64(449),
                                   qint64(451), qint64(-360), qint64(720)}) {
            Setup s;
            s.rangeDeg = range;
            invalid.append(s);
        }
        // A baud of 0 or below.
        for (const qint64 baud : {qint64(0), qint64(-9600)}) {
            Setup s;
            s.baud = baud;
            invalid.append(s);
        }
        // Driver 4 with a Hamlib model of 0 or below.
        for (const qint64 model : {qint64(0), qint64(-404)}) {
            Setup s;
            s.driver = 4;
            s.hamlibModel = model;
            invalid.append(s);
        }
        // An offset that is not a number.
        for (const double offset : {std::numeric_limits<double>::quiet_NaN(),
                                    std::numeric_limits<double>::infinity(),
                                    -std::numeric_limits<double>::infinity()}) {
            Setup s;
            s.offsetDeg = offset;
            invalid.append(s);
        }
        for (const Setup& s : invalid) {
            const SessionMessage answer = invoke(peer, "configureRotor", s.arguments());
            QVERIFY(!answer.accepted);
            QCOMPARE(answer.reason, kSetupInvalid);
            QCOMPARE(rotor->config().driver, RotorDriver::None);
            QVERIFY(!AppSettings::instance().contains(QStringLiteral("Rotor/Driver")));
            QVERIFY(core.rotor.isNull());
        }
        // Nothing of any refused setup was saved.
        for (const char* key : {"Rotor/Driver", "Rotor/SerialPort", "Rotor/Baud", "Rotor/Host",
                                "Rotor/Port", "Rotor/HamlibModel", "Rotor/Axes", "Rotor/EndStop",
                                "Rotor/RangeDeg", "Rotor/OffsetDeg"}) {
            QVERIFY2(!AppSettings::instance().contains(QString::fromLatin1(key)), key);
        }
        // Driver 4 with a Hamlib model above 0 passes the setup checks
        // (and is refused here only for the missing rotctld).
        {
            Setup s;
            s.driver = 4;
            s.hamlibModel = 1;
            QCOMPARE(invoke(peer, "configureRotor", s.arguments()).reason,
                     RotctldProcess::notInstalledReason());
        }

        // A serial port the Core does not have, and rotctld it lacks.
        Setup missingPort;
        missingPort.serialPort = QStringLiteral("/dev/ttyNotHere");
        SessionMessage answer = invoke(peer, "configureRotor", missingPort.arguments());
        QCOMPARE(answer.reason, kUnknownPort);
        Setup started;
        started.driver = 4;
        started.hamlibModel = 601;
        answer = invoke(peer, "configureRotor", started.arguments());
        QCOMPARE(answer.reason, RotctldProcess::notInstalledReason());
        QCOMPARE(rotor->config().driver, RotorDriver::None);

        // Unreadable: a missing, extra or wrongly kinded argument.
        QList<MirrorUpdate> missing = Setup{}.arguments();
        missing.removeLast();
        QCOMPARE(invoke(peer, "configureRotor", missing).reason, kUnreadable);
        QList<MirrorUpdate> extra = Setup{}.arguments();
        extra.append(boolean("now", true));
        QCOMPARE(invoke(peer, "configureRotor", extra).reason, kUnreadable);
        QList<MirrorUpdate> wrongKind = Setup{}.arguments();
        wrongKind[0] = i64("driver", 2);
        QCOMPARE(invoke(peer, "configureRotor", wrongKind).reason, kUnreadable);
        QCOMPARE(rotor->config().driver, RotorDriver::None);

        // Ports 1 and 65535 are inside the range.
        for (const qint64 port : {qint64(1), qint64(65535)}) {
            Setup s;
            s.port = port;
            answer = invoke(peer, "configureRotor", s.arguments());
            QVERIFY2(answer.accepted, qPrintable(answer.reason));
            QCOMPARE(rotor->config().port, quint16(port));
        }

        // Driver none forgets the setup.
        Setup none;
        none.driver = 0;
        none.serialPort.clear();
        answer = invoke(peer, "configureRotor", none.arguments());
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QCOMPARE(rotor->config().driver, RotorDriver::None);
        QVERIFY(!AppSettings::instance().contains(QStringLiteral("Rotor/Driver")));
    }

    void hamlibModelReachesTheControllerOnlyForDriver4()
    {
        Core core(true);
        LoopbackTransport* peer = core.connect(this, kRadioIdentitySessionProtocolMinor);
        QTRY_VERIFY(snapshotDone(peer));
        StationRotorController* rotor = core.controller();

        Setup serial;
        serial.hamlibModel = 601;
        SessionMessage answer = invoke(peer, "configureRotor", serial.arguments());
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QCOMPARE(rotor->config().driver, RotorDriver::Gs232b);
        QCOMPARE(rotor->config().hamlibModel, 0);
        QCOMPARE(AppSettings::instance().value(QStringLiteral("Rotor/HamlibModel")).toString(),
                 QStringLiteral("0"));
        QCOMPARE(core.model.rotorModel()->hamlibModel(), 0);

        // A rotctld already running (driver 3) takes no model either.
        Setup running;
        running.driver = 3;
        running.serialPort.clear();
        running.host = QStringLiteral("127.0.0.1");
        running.hamlibModel = 601;
        answer = invoke(peer, "configureRotor", running.arguments());
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QCOMPARE(rotor->config().driver, RotorDriver::Rotctld);
        QCOMPARE(rotor->config().hamlibModel, 0);

        // Driver 4 keeps it. rotctld is "installed" at a path where nothing
        // is, so starting it fails and no program runs.
        QTemporaryDir nowhere;
        QVERIFY(nowhere.isValid());
        RotctldProcess::setBinaryOverrideForTesting(nowhere.filePath(QStringLiteral("rotctld")));
        Setup started;
        started.driver = 4;
        started.hamlibModel = 601;
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("rotctld would not start")));
        answer = invoke(peer, "configureRotor", started.arguments());
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QCOMPARE(rotor->config().driver, RotorDriver::RotctldStarted);
        QCOMPARE(rotor->config().hamlibModel, 601);
        QTRY_COMPARE(core.model.rotorModel()->hamlibModel(), 601);
        QVERIFY(core.controller()->configureRotor(RotorConfig{}, nullptr));

        // The model as the rotor object states it: driver 4's own, else 0.
        RotorState::State state;
        state.driver = RotorState::Driver::RotctldStarted;
        state.hamlibModel = 601;
        RotorState window;
        window.setState(state);
        QCOMPARE(window.hamlibModel(), 601);
    }

    // ── The hold dead man ──────────────────────────────────────────

    void aWindowsSessionEndingEndsItsHold()
    {
        Core core(true);
        QVERIFY(core.connectRotorAt("090"));
        LoopbackTransport* holder = core.connect(this, kRadioIdentitySessionProtocolMinor);
        LoopbackTransport* other = core.connect(this, kRadioIdentitySessionProtocolMinor);
        QTRY_VERIFY(snapshotDone(holder));
        QTRY_VERIFY(snapshotDone(other));
        const SessionMessage answer =
            invoke(holder, "nudgeRotor", {enumArg("direction", 0), boolean("active", true)});
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QCOMPARE(core.controller()->motion(), RotorMotion::Nudging);
        core.rotor->take();

        // The holder repeats its turn well inside the controller's own
        // lapse, so only the session's end can stop the rotor below.
        const auto keepHolding = [&] {
            const SessionMessage repeat =
                invoke(holder, "nudgeRotor", {enumArg("direction", 0), boolean("active", true)});
            QVERIFY2(repeat.accepted, qPrintable(repeat.reason));
        };
        static_assert(StationRotorController::kHoldLapseMs > 300,
                      "the stop below must come well inside the hold lapse");

        // Another window's session ending leaves the hold alone, past a
        // full lapse while the holder keeps holding.
        other->closeLink(QStringLiteral("gone"));
        for (int i = 0; i < 5; ++i) {
            QTest::qWait(StationRotorController::kHoldRepeatMs);
            keepHolding();
            QCOMPARE(core.controller()->motion(), RotorMotion::Nudging);
        }

        // The holder's own session ending stops the rotor at once, not a
        // lapse later.
        keepHolding();
        core.rotor->take();
        holder->closeLink(QStringLiteral("gone"));
        QTRY_COMPARE_WITH_TIMEOUT(core.controller()->motion(), RotorMotion::Stopped, 300);
        QCOMPARE(core.rotor->take(), QByteArray("S\r"));
    }

    void theDispatcherEndsOnlyTheEndingSessionsHold()
    {
        RadioModel model;
        model.enableStationAccessoryIdentity();
        StationRotorController* rotor = model.stationRotorController();
        QPointer<FakeRotor> fake;
        rotor->setSerialPortListerForTesting([] { return QStringList{kPort}; });
        rotor->connection()->setTransportFactoryForTesting([&fake](const RotorTransportTarget&) {
            auto t = std::make_unique<FakeRotor>();
            fake = t.get();
            return std::unique_ptr<RotorTransport>(std::move(t));
        });
        rotor->connection()->setTimingForTesting(steppedTiming());
        RotorConfig config;
        config.driver = RotorDriver::Gs232b;
        config.serialPort = kPort;
        QVERIFY(rotor->configureRotor(config, nullptr));
        QVERIFY(fake);
        fake->feed("AZ=090  EL=000\r\n");

        SessionCommandDispatcher dispatcher(&model);
        dispatcher.setSessionOwner(QStringLiteral("station:7"));
        const SessionMessage answer = dispatch(
            dispatcher, "nudgeRotor", {enumArg("direction", 1), boolean("active", true)});
        QVERIFY2(answer.accepted, qPrintable(answer.reason));
        QCOMPARE(rotor->motion(), RotorMotion::Nudging);
        dispatcher.endSessionOwner(QStringLiteral("station:8"));
        QCOMPARE(rotor->motion(), RotorMotion::Nudging);
        dispatcher.endSessionOwner(QStringLiteral("station:7"));
        QCOMPARE(rotor->motion(), RotorMotion::Stopped);

        // A dispatcher on a Core without a rotor refuses plainly.
        RadioModel plain;
        SessionCommandDispatcher bare(&plain);
        QCOMPARE(dispatch(bare, "stopRotor", {}).reason, kCoreHasNoRotor);
        QCOMPARE(dispatch(bare, "configureRotor", Setup{}.arguments()).reason, kCoreHasNoRotor);
    }

    // ── The read-only object ───────────────────────────────────────

    void writingTheRotorObjectIsRefused()
    {
        Core core(true);
        LoopbackTransport* peer = core.connect(this, kRadioIdentitySessionProtocolMinor);
        QTRY_VERIFY(snapshotDone(peer));
        peer->sendText(SessionMessages::encode(SessionMessages::propertyWrite(
            "rotor", {utf8("presets", QStringLiteral("Home\t0"))}, 41)));
        QTRY_VERIFY([&] {
            for (const SessionMessage& m : receivedMessages(peer)) {
                if (m.kind == SessionMessageKind::PropertyResult && m.writeId == 41) {
                    return true;
                }
            }
            return false;
        }());
        for (const SessionMessage& m : receivedMessages(peer)) {
            if (m.kind == SessionMessageKind::PropertyResult && m.writeId == 41) {
                QCOMPARE(m.propertyResults.size(), 1);
                QVERIFY(!m.propertyResults.first().accepted);
                QCOMPARE(m.propertyResults.first().reason, RotorState::readOnlyReason());
            }
        }
        QVERIFY(core.controller()->presets().isEmpty());
        QVERIFY(OperatorWording::isPlain(RotorState::readOnlyReason()));
    }

    void theObjectFollowsTheRotor()
    {
        Core core(true);
        LoopbackTransport* peer = core.connect(this, kRadioIdentitySessionProtocolMinor);
        QTRY_VERIFY(snapshotDone(peer));
        RotorState* object = core.model.rotorModel();
        QCOMPARE(object->driver(), RotorState::Driver::None);
        QCOMPARE(object->serialPorts(), QStringLiteral("/dev/ttyUSB0\nCOM4"));
        QVERIFY(!object->rotctldAvailable());
        QCOMPARE(object->azimuthDeg(), -1.0);

        QVERIFY(core.connectRotorAt("090"));
        QCOMPARE(object->connectionPhase(), TunerModel::ConnectionPhase::Connected);
        QCOMPARE(object->driver(), RotorState::Driver::Gs232b);
        QCOMPARE(object->label(), QStringLiteral("Yaesu GS-232B on /dev/ttyUSB0"));
        QCOMPARE(object->azimuthDeg(), 90.0);
        QVERIFY(object->positionFresh());

        // The window hears it as a delta on `rotor`.
        QTRY_VERIFY([&] {
            for (const SessionMessage& m : receivedMessages(peer)) {
                if (m.kind != SessionMessageKind::Delta || m.objectKey != "rotor") {
                    continue;
                }
                for (const MirrorUpdate& u : m.updates) {
                    if (u.name == "azimuthDeg" && u.value.toDouble() == 90.0) {
                        return true;
                    }
                }
            }
            return false;
        }());

        // Off connected: headings and targets unknown, motion stopped.
        QVERIFY(core.controller()->disconnectRotor(nullptr));
        QCOMPARE(object->connectionPhase(), TunerModel::ConnectionPhase::Disconnected);
        QCOMPARE(object->azimuthDeg(), -1.0);
        QCOMPARE(object->targetAzimuthDeg(), -1.0);
        QCOMPARE(object->travelDeg(), 0.0);
        QVERIFY(object->routeKnown());
        QCOMPARE(object->motion(), RotorState::Motion::Stopped);
    }

    void aWindowsCopyKeepsToTheTables()
    {
        RotorState window;
        QVERIFY(window.applyStationValue("driver", qlonglong(3)));
        QCOMPARE(window.driver(), RotorState::Driver::Rotctld);
        QVERIFY(window.applyStationValue("driver", qlonglong(9)));
        QCOMPARE(window.driver(), RotorState::Driver::None);
        QVERIFY(window.applyStationValue("endStop", qlonglong(2)));
        QCOMPARE(window.endStop(), RotorState::EndStop::South);
        QVERIFY(window.applyStationValue("azimuthDeg", 271.5));
        QCOMPARE(window.azimuthDeg(), 271.5);
        QVERIFY(window.applyStationValue("azimuthDeg",
                                         std::numeric_limits<double>::infinity()));
        QCOMPARE(window.azimuthDeg(), -1.0);
        QVERIFY(window.applyStationValue("port", qlonglong(70000)));
        QCOMPARE(window.port(), 65535);
        QVERIFY(window.applyStationValue("motion", qlonglong(2)));
        QCOMPARE(window.motion(), RotorState::Motion::Nudging);
        QVERIFY(!window.applyStationValue("rotorSpeed", qlonglong(1)));

        // Position and the rest notify apart.
        QSignalSpy position(&window, &RotorState::positionChanged);
        QSignalSpy state(&window, &RotorState::stateChanged);
        QVERIFY(window.applyStationValue("azimuthDeg", 10.0));
        QCOMPARE(position.count(), 1);
        QCOMPARE(state.count(), 0);
        QVERIFY(window.applyStationValue("presets", QStringLiteral("Home\t0")));
        QCOMPARE(position.count(), 1);
        QCOMPARE(state.count(), 1);
        QVERIFY(window.applyStationValue("presets", QStringLiteral("Home\t0")));
        QCOMPARE(state.count(), 1);
    }

    // ── The tools catalogue ────────────────────────────────────────

    void theCatalogueOffersTheRotorOnlyWhenOneIsSetUp()
    {
        StationCatalog::Inputs inputs;
        QVERIFY(toolsListRotorAt(StationCatalog::build(inputs), false));
        QVERIFY(!toolsListRotor(StationCatalog::build(inputs)));
        inputs.rotorConfigured = true;
        QVERIFY(toolsListRotor(StationCatalog::build(inputs)));

        Core core(true);
        StationCatalog catalog;
        catalog.bind(&core.model);
        catalog.refresh();
        const auto listed = [&catalog] {
            return toolsListRotor(QJsonDocument::fromJson(catalog.json().toUtf8()).object());
        };
        QVERIFY(!listed());
        QVERIFY(toolsListRotorAt(QJsonDocument::fromJson(catalog.json().toUtf8()).object(), false));
        const quint32 before = catalog.revision();
        QVERIFY(core.connectRotorAt("090"));
        QTRY_VERIFY(listed());
        QVERIFY(catalog.revision() != before);
        // A heading alone does not rebuild the catalogue.
        const quint32 configured = catalog.revision();
        core.rotor->feed("AZ=120  EL=000\r\n");
        QTest::qWait(20);
        QCOMPARE(catalog.revision(), configured);
        QVERIFY(core.controller()->configureRotor(RotorConfig{}, nullptr));
        QTRY_VERIFY(!listed());
        // Listed still, greyed: disabled, never hidden.
        QVERIFY(toolsListRotorAt(QJsonDocument::fromJson(catalog.json().toUtf8()).object(), false));
    }

    // ── A desktop running its own radio (Task 4c) ──────────────────

    void aDesktopRunningItsOwnRadioOwnsTheRotorAlone()
    {
        Core desktop(Owns::RotorOnly);
        // Only the rotor: no Tuner Genius, Power Genius or RF-Kit.
        QVERIFY(!desktop.model.stationAccessoryIdentityEnabled());
        QVERIFY(desktop.model.stationRfKitController() == nullptr);
        QPointer<StationRotorController> controller = desktop.controller();
        QVERIFY(controller);
        QCOMPARE(desktop.model.findChildren<StationRotorController*>().size(), 1);

        // A second call makes nothing; nor does nereusd's call after it.
        desktop.model.enableStationRotor();
        QCOMPARE(desktop.controller(), controller.data());
        QCOMPARE(desktop.model.findChildren<StationRotorController*>().size(), 1);

        // Its hosted Core tells a phone it controls a rotor.
        QCOMPARE(desktop.server->remoteRotorControlVersion(), 1);
        LoopbackTransport* phone = desktop.connect(this, kRadioIdentitySessionProtocolMinor);
        QTRY_VERIFY(snapshotDone(phone));
        QCOMPARE(capabilityOf(phone, "remoteRotorControlVersion"), std::optional<qint64>(1));
        QVERIFY(sawRotorObject(phone));

        // The Rotor tool is listed greyed until a rotor is set up, then
        // offered.
        StationCatalog* catalog = desktop.server->catalog();
        QVERIFY(catalog);
        const auto tools = [catalog] {
            return QJsonDocument::fromJson(catalog->json().toUtf8()).object();
        };
        QVERIFY(toolsListRotorAt(tools(), false));
        QVERIFY(desktop.connectRotorAt("090"));
        QTRY_VERIFY(toolsListRotorAt(tools(), true));

        // The phone turns it.
        const SessionMessage turned =
            invoke(phone, "setRotorTarget", {f64("azimuthDeg", 200.0), f64("elevationDeg", -1.0)});
        QVERIFY2(turned.accepted, qPrintable(turned.reason));
        QCOMPARE(desktop.controller()->targetAzimuthDeg(), 200.0);
    }

    void nereusdStillMakesOneRotorThroughItsAccessories()
    {
        Core core(Owns::Accessories);
        QVERIFY(core.model.stationAccessoryIdentityEnabled());
        StationRotorController* controller = core.controller();
        QVERIFY(controller);
        core.model.enableStationRotor();
        QCOMPARE(core.controller(), controller);
        QCOMPARE(core.model.findChildren<StationRotorController*>().size(), 1);
        QCOMPARE(core.model.rotorModel()->label(), QString());

        // The rotor first, then the accessories: still one.
        RadioModel desktop;
        desktop.enableStationRotor();
        StationRotorController* first = desktop.stationRotorController();
        desktop.enableStationAccessoryIdentity();
        QCOMPARE(desktop.stationRotorController(), first);
        QCOMPARE(desktop.findChildren<StationRotorController*>().size(), 1);
    }

    void aWindowOnARemoteCoreNeverOwnsARotor()
    {
        RadioModel window(RadioModel::Role::Remote);
        window.enableStationRotor();
        QVERIFY(window.stationRotorController() == nullptr);
        window.enableStationAccessoryIdentity();
        QVERIFY(window.stationRotorController() == nullptr);
        QVERIFY(window.findChildren<StationRotorController*>().isEmpty());
    }

    void theRotorsPortClosesWithItsModel()
    {
        QPointer<FakeRotor> port;
        {
            Core desktop(Owns::RotorOnly);
            QVERIFY(desktop.connectRotorAt("090"));
            port = desktop.rotor;
            QVERIFY(port);
        }
        // A switch to a remote Core retires the whole model; the rotor's
        // port is closed then, and nothing is left holding it.
        QVERIFY(port.isNull() || port->closed);
        QTRY_VERIFY(port.isNull());
    }

    // ── A remote window ────────────────────────────────────────────

    void aRemoteWindowMirrorsTheRotorAndSendsItsCommands()
    {
        Core core(true);
        QVERIFY(core.connectRotorAt("090"));
        RadioModel window(RadioModel::Role::Remote);
        SettingsProxy proxy;
        StationClient client(&window, &proxy);
        window.attachStation(&client);
        QVERIFY(!client.rotorControlAvailable());
        QCOMPARE(client.requestStopRotor().reason, kCoreHasNoRotor);

        auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
        stationEnd->linkTo(clientEnd);
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        client.startSession(clientEnd, core.server->token());
        core.server->acceptTransport(stationEnd);
        QVERIFY(completed.wait(5000) || !completed.isEmpty());
        NEREUS_TRY_VERIFY(client.rotorControlAvailable());
        NEREUS_TRY_COMPARE(window.rotorModel()->label(),
                           QStringLiteral("Yaesu GS-232B on /dev/ttyUSB0"));
        NEREUS_TRY_COMPARE(window.rotorModel()->azimuthDeg(), 90.0);

        IStationLink::CommandOutcome sent = client.requestRotorPresets(QStringLiteral("Home\t45"));
        QVERIFY2(sent.sent, qPrintable(sent.reason));
        NEREUS_TRY_COMPARE(core.controller()->presets(), QStringLiteral("Home\t45"));
        NEREUS_TRY_COMPARE(window.rotorModel()->presets(), QStringLiteral("Home\t45"));

        sent = client.requestRotorTarget(200.0, -1.0);
        QVERIFY2(sent.sent, qPrintable(sent.reason));
        NEREUS_TRY_COMPARE(core.controller()->targetAzimuthDeg(), 200.0);
        sent = client.requestNudgeRotor(1, true);
        QVERIFY(sent.sent);
        NEREUS_TRY_COMPARE(core.controller()->motion(), RotorMotion::Nudging);
        sent = client.requestStopRotor();
        QVERIFY(sent.sent);
        NEREUS_TRY_COMPARE(core.controller()->motion(), RotorMotion::Stopped);
        QVERIFY(client.requestTurnRotorToCall(QStringLiteral("W1AW"), false).sent);
        sent = client.requestConfigureRotor(2, kPort, 4800, QString(), 4533, 601, 0, 2, 450, 1.5);
        QVERIFY(sent.sent);
        NEREUS_TRY_COMPARE(core.controller()->config().baud, 4800);
        QCOMPARE(core.controller()->config().hamlibModel, 0);
        QCOMPARE(core.controller()->config().endStop, RotorRoute::EndStop::South);
        NEREUS_TRY_COMPARE(window.rotorModel()->rangeDeg(), 450);
        QVERIFY(client.requestDisconnectRotor().sent);
        NEREUS_TRY_COMPARE(window.rotorModel()->connectionPhase(),
                           TunerModel::ConnectionPhase::Disconnected);
    }

    // ── The GUI's one way to turn the rotor (Task 5) ───────────────

    void theGuisRotorCommandsReachTheLocalRotor()
    {
        // A desktop running its own radio: its own controller.
        Core desktop(Owns::RotorOnly);
        QVERIFY(desktop.connectRotorAt("090"));
        RotorCommandSink& local = desktop.model;
        QString why;
        QVERIFY2(local.requestRotorTarget(200.0, -1.0, &why), qPrintable(why));
        QCOMPARE(desktop.controller()->targetAzimuthDeg(), 200.0);
        QVERIFY2(local.requestStopRotor(&why), qPrintable(why));
        NEREUS_TRY_COMPARE(desktop.controller()->motion(), RotorMotion::Stopped);
        // The controller's own refusal comes back.
        QVERIFY(!local.requestRotorTarget(400.0, -1.0, &why));
        QVERIFY(!why.isEmpty());
    }

    // Its own function: the desktop above saved a rotor setup, and a second
    // Core in the same test would open that port before its fake is in.
    void theGuisRotorCommandsReachARemoteCoresRotor()
    {
        // A window on a remote Core: the Core's rotor, over the link.
        Core core(true);
        QVERIFY(core.connectRotorAt("090"));
        RadioModel window(RadioModel::Role::Remote);
        QString why;
        // Not yet linked: refused with the reason.
        QVERIFY(!window.requestRotorTarget(10.0, -1.0, &why));
        QCOMPARE(why, kCoreHasNoRotor);
        SettingsProxy proxy;
        StationClient client(&window, &proxy);
        window.attachStation(&client);
        auto* stationEnd = new LoopbackTransport(QStringLiteral("station-end"), this);
        auto* clientEnd = new LoopbackTransport(QStringLiteral("client-end"), this);
        stationEnd->linkTo(clientEnd);
        QSignalSpy completed(&client, &StationClient::handshakeComplete);
        client.startSession(clientEnd, core.server->token());
        core.server->acceptTransport(stationEnd);
        QVERIFY(completed.wait(5000) || !completed.isEmpty());
        NEREUS_TRY_VERIFY(client.rotorControlAvailable());
        QVERIFY2(window.requestRotorTarget(250.0, -1.0, &why), qPrintable(why));
        NEREUS_TRY_COMPARE(core.controller()->targetAzimuthDeg(), 250.0);
        QVERIFY2(window.requestStopRotor(&why), qPrintable(why));
        NEREUS_TRY_COMPARE(core.controller()->motion(), RotorMotion::Stopped);

        // Task 7: the setup and presets. A refused setup takes the
        // accessory route ("rotor"), so the Rotor Setup page shows it or a
        // notice does, and never the slice one.
        QSignalSpy refused(&window, &RadioModel::accessoryRequestRefused);
        QSignalSpy sliceRefused(&window, &RadioModel::sliceAddRejected);
        QSignalSpy finished(&window, &RadioModel::stationCommandFinished);
        RotorCommandSink::Setup setup;
        setup.driver = 2;
        setup.serialPort = QStringLiteral("/dev/ttyNOTHERE");
        QVERIFY2(window.requestConfigureRotor(setup, &why), qPrintable(why));
        const quint32 id = window.lastRotorCommandId();
        QVERIFY(id != 0);
        NEREUS_TRY_VERIFY(!refused.isEmpty());
        QCOMPARE(refused.at(0).at(0).toString(), QStringLiteral("rotor"));
        QCOMPARE(refused.at(0).at(1).toString(),
                 QStringLiteral("That serial port is not on the Core's computer."));
        QVERIFY(!refused.at(0).at(2).toBool());   // no page claimed it: a notice
        NEREUS_TRY_VERIFY(!finished.isEmpty());
        QCOMPARE(finished.constLast().at(0).toUInt(), id);
        QVERIFY(!finished.constLast().at(1).toBool());
        QCOMPARE(sliceRefused.count(), 0);
        QVERIFY2(window.requestRotorPresets(QStringLiteral("Home\t45"), &why), qPrintable(why));
        NEREUS_TRY_COMPARE(core.controller()->presets(), QStringLiteral("Home\t45"));
        setup.serialPort = kPort;
        setup.endStop = 2;
        setup.rangeDeg = 450;
        QVERIFY2(window.requestConfigureRotor(setup, &why), qPrintable(why));
        NEREUS_TRY_COMPARE(core.controller()->config().rangeDeg, 450.0);
        QCOMPARE(core.controller()->config().endStop, RotorRoute::EndStop::South);

        // Task 8: every rotor command takes the accessory route, the turns
        // as well as the setup, so a refused Turn beam from a spot (or the
        // applet's) is said by the page or a notice, never as a slice
        // refusal. An elevation on an azimuth rotor is refused by the Core.
        refused.clear();
        finished.clear();
        QVERIFY2(window.requestRotorTarget(10.0, 45.0, &why), qPrintable(why));
        const quint32 turnId = window.lastRotorCommandId();
        QVERIFY(turnId != 0);
        NEREUS_TRY_VERIFY(!refused.isEmpty());
        QCOMPARE(refused.at(0).at(0).toString(), QStringLiteral("rotor"));
        QCOMPARE(refused.at(0).at(1).toString(), StationRotorController::azimuthOnlyReason());
        QVERIFY(!refused.at(0).at(2).toBool());
        NEREUS_TRY_VERIFY(!finished.isEmpty());
        QCOMPARE(finished.constLast().at(0).toUInt(), turnId);
        QVERIFY(!finished.constLast().at(1).toBool());
        // A call the Core cannot turn to.
        refused.clear();
        QVERIFY2(window.requestTurnRotorToCall(QStringLiteral("QQ1ABC"), false, &why),
                 qPrintable(why));
        NEREUS_TRY_VERIFY(!refused.isEmpty());
        QCOMPARE(refused.at(0).at(0).toString(), QStringLiteral("rotor"));
        QVERIFY(!refused.at(0).at(1).toString().isEmpty());
        QCOMPARE(sliceRefused.count(), 0);
        window.detachStation();
    }
};

QTEST_MAIN(StationRotorLinkTest)
#include "tst_station_rotor_link.moc"
