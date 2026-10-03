// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_station_pairing.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 14 (R-IOS-08, D37): pairing on the link.
//
// Refusal, burn and backoff first:
//
//   - one tap (LAN mode) is refused on a claimed Core, when
//     pairing_lan_click is deny, and from an address that is not on one of
//     the Core's directly connected networks (a relayed connection has
//     none); code mode is refused while the window is closed; every
//     refusal is pair.fail with a plain reason naming the Core, then the
//     connection ends and no device is added;
//   - a wrong code fails at the device's step 3 (the device says so with
//     pair.fail) or at the Core's step 4 (a device answering without the
//     code); either way the code is burned and the next appears after
//     5 s, then 10 s, and pair.fail's retryAfterMs says when; a connection
//     that ends after the Core committed to the code burns it too;
//   - the code goes to one exchange at a time;
//   - the code is never written through the logging categories, and a
//     connection signed in with the old pairing token never receives it,
//     in `pairingCode` or from pairing.open, even when its hello declares
//     deviceAuth; the pairing verbs are refused to a window that does not;
//   - a pairing connection cannot sign in;
//   - the code's hash runs on a worker: the event loop keeps serving a
//     paired device's session and a timer while it runs.
//
// Then the admit paths: one tap on an unclaimed Core and the right code
// each add the device (the code's name and kind from the device's box),
// hand the device the Core's identity key and label, end the connection,
// and the device then signs in by key on a new one. pairing.open reopens
// the window for one pairing and pairing.close closes it; the `devices`
// object follows. The Core declares features.pairing 1 and advertises
// pairingVersion 1, last.
//
// And nereus_pairing_peer, the Core's side over standard input and
// output, pairs with this file's C++ device side in code mode with the
// right code and a wrong one, and in LAN mode.
//
// Keys and codes are made at run time; a code is compared, never printed.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: Cover code-only reconnect and in-flight device revocation.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-27: Diagnose helper startup failures without changing deadlines.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave: the pairing code is never printed
//               to standard output (the journal on a packaged Core). J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-24: Part C fix wave (security Minors R1-M1, M2, M4,
//               M5): the confirm-step recheck, the step 1 point check, the
//               per-address handshake cap and 0600 on load. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-26  J.J. Boyd / KG4VCF  Parity Task 19 (R-IOS-25):
//                                    recordStreamVersion and the record
//                                    streams. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  thePeerRefusesAWrongCode: the retry
//                                    wait is checked inside the product's
//                                    window less the exchange's own time,
//                                    not as an exact 5000 (failed at load
//                                    35 with 4999). AI-assisted via
//                                    Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include "TestFunctionGroups.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QProcess>
#include <QProcessEnvironment>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QSemaphore>
#include <QThread>
#include <QTimer>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <functional>
#include <memory>
#include <optional>

#include "core/AppSettings.h"
#include "core/security/DeviceAuthenticator.h"
#include "core/security/DeviceStore.h"
#include "core/security/PairingCode.h"
#include "core/security/PairingWindow.h"
#include "core/security/SpakeExchange.h"
#include "core/security/StationIdentity.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationDevicesFacade.h"
#include "core/session/StationServer.h"
#include "core/station/StationRadios.h"
#include "models/RadioModel.h"

#include "OperatorWording.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

// ── Every log line, for the check that no code reaches one ──────────────

QMutex g_logMutex;
QStringList g_log;
QtMessageHandler g_previousHandler = nullptr;

void captureLog(QtMsgType, const QMessageLogContext& context, const QString& text)
{
    // Kept, not printed: a line here would otherwise go to the test's output.
    QMutexLocker lock(&g_logMutex);
    g_log.append(QString::fromLatin1(context.category ? context.category : "") + QLatin1Char(' ')
                 + text);
}

// ── A device and its side of the pairing ────────────────────────────────

struct Device {
    QTemporaryDir dir;
    StationIdentity key = StationIdentity::loadOrCreate(dir.path());
    QString name = QStringLiteral("Shack iPhone");
    QString kind = QStringLiteral("phone");

    QString publicKey() const { return StationIdentity::toBase64Url(key.publicKeySpki()); }
    QByteArray id() const { return key.fingerprint(); }

    SessionDeviceBlock block(const QByteArray& challenge, const QByteArray& certSha256,
                             const QByteArray& stationSpki) const
    {
        return SessionDeviceBlock{
            StationIdentity::toBase64Url(id()), publicKey(), name, kind,
            StationIdentity::toBase64Url(key.sign(DeviceAuthenticator::transcript(
                challenge, certSha256, stationSpki, key.publicKeySpki())))};
    }

    PairedDevice record() const
    {
        PairedDevice device;
        device.id = id();
        device.publicKeySpki = key.publicKeySpki();
        device.name = name;
        device.kind = kind;
        return device;
    }
};

// One end of a connection, as the device sees it: messages out, messages in.
class Link {
public:
    virtual ~Link() = default;
    virtual void send(const SessionMessage& message) = 0;
    /// The next message from the Core; empty when none arrives or the
    /// connection has ended.
    virtual QJsonObject next() = 0;
    virtual bool ended() = 0;
};

class LoopbackLink : public Link {
public:
    explicit LoopbackLink(LoopbackTransport* app) : m_app(app) {}
    void send(const SessionMessage& message) override
    {
        m_app->sendText(SessionMessages::encode(message));
    }
    QJsonObject next() override
    {
        const bool arrived = QTest::qWaitFor(
            [this]() { return m_app->received().size() > m_read || !m_app->isOpen(); }, 10000);
        if (!arrived || m_app->received().size() <= m_read) {
            return {};
        }
        return QJsonDocument::fromJson(m_app->received().at(m_read++)).object();
    }
    bool ended() override
    {
        return QTest::qWaitFor([this]() { return !m_app->isOpen(); }, 10000);
    }

private:
    LoopbackTransport* m_app;
    qsizetype m_read = 0;
};

// nereus_pairing_peer, one line per message.
class ProcessLink : public Link {
public:
    bool start(const QStringList& arguments)
    {
        QElapsedTimer startup;
        startup.start();
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("NEREUS_PAIRING_STARTUP_TIMINGS"), QStringLiteral("1"));
        m_process.setProcessEnvironment(environment);
        m_process.start(QStringLiteral(NEREUS_PAIRING_PEER), arguments);
        if (!m_process.waitForStarted(10000)) {
            qWarning() << "Pairing helper failed to start after" << startup.elapsed()
                       << "ms:" << m_process.errorString();
            return false;
        }
        m_ready = readLine();
        const bool ready = m_ready.value(QStringLiteral("type")).toString()
            == QLatin1String("peer.ready");
        if (!ready) {
            qWarning() << "Pairing helper did not report ready after" << startup.elapsed()
                       << "ms; process state" << m_process.state()
                       << "error" << m_process.errorString();
            // Print only our numeric phase markers, never the pairing wire or secrets.
            const auto stderrLines = m_process.readAllStandardError().split('\n');
            for (const QByteArray& line : stderrLines) {
                if (line.startsWith("pairing-startup ")) {
                    qWarning().noquote() << QString::fromUtf8(line);
                }
            }
        }
        return ready;
    }
    QJsonObject ready() const { return m_ready; }
    QJsonObject done() const { return m_done; }
    void send(const SessionMessage& message) override
    {
        m_process.write(SessionMessages::encode(message) + '\n');
    }
    QJsonObject next() override
    {
        const QJsonObject line = readLine();
        if (line.value(QStringLiteral("type")).toString() == QLatin1String("peer.done")) {
            m_done = line;
            return {};
        }
        return line;
    }
    bool ended() override
    {
        while (m_done.isEmpty()) {
            const QJsonObject line = readLine();
            if (line.isEmpty()) {
                break;
            }
            if (line.value(QStringLiteral("type")).toString() == QLatin1String("peer.done")) {
                m_done = line;
            }
        }
        m_process.closeWriteChannel();
        return !m_done.isEmpty() && m_process.waitForFinished(10000)
            && m_process.exitStatus() == QProcess::NormalExit && m_process.exitCode() == 0;
    }
    ~ProcessLink() override
    {
        if (m_process.state() != QProcess::NotRunning) {
            m_process.kill();
            m_process.waitForFinished(5000);
        }
    }

private:
    QJsonObject readLine()
    {
        while (!m_process.canReadLine()) {
            if (!m_process.waitForReadyRead(10000)) {
                return {};
            }
        }
        return QJsonDocument::fromJson(m_process.readLine().trimmed()).object();
    }

    QProcess m_process;
    QJsonObject m_ready;
    QJsonObject m_done;
};

// What the device saw of one pairing.
struct Outcome {
    QString type;             // "pair.accept", "pair.confirm", "pair.fail", "" (cut off)
    QString reason;
    qint64 retryAfterMs = -1;
    QJsonObject identity;     // the Core's, from pair.accept or its box
    QString label;
};

// The device side over `link`: its hello, then pair.start and, in code
// mode, the exchange. `code` is typed as the operator would; `lie` answers
// step 2 without the code (random bytes for step 3); `stopAfterStep2`
// leaves once the Core has committed to the code.
struct DeviceSide {
    const Device& device;
    QString boxName;
    QString boxKind;
    QString boxKey;           // empty: the device's own key
    QJsonObject hello;
    // Runs just before the device sends its box (the confirm step).
    std::function<void()> beforeConfirm;
    // Through the remote access service's mailbox: no hello either way.
    bool throughService = false;

    explicit DeviceSide(const Device& d) : device(d), boxName(d.name), boxKind(d.kind) {}

    bool greet(Link& link, const QString& startName = QString(),
               const QString& startKind = QString(), const QString& mode = QStringLiteral("code"))
    {
        if (!throughService) {
            hello = link.next();
            if (hello.value(QStringLiteral("type")).toString() != QLatin1String("hello")) {
                return false;
            }
            link.send(SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor, 0,
                                             QStringLiteral("NereusSDR iPhone"),
                                             {kSessionProtocolMajor}, {{"deviceAuth", 1}}));
        }
        link.send(SessionMessages::pairStart(
            mode, SessionPairDevice{device.publicKey(),
                                    startName.isEmpty() ? device.name : startName,
                                    startKind.isEmpty() ? device.kind : startKind}));
        return true;
    }

    static Outcome finish(const QJsonObject& message)
    {
        Outcome outcome;
        outcome.type = message.value(QStringLiteral("type")).toString();
        outcome.reason = message.value(QStringLiteral("reason")).toString();
        outcome.retryAfterMs =
            message.contains(QStringLiteral("retryAfterMs"))
                ? static_cast<qint64>(message.value(QStringLiteral("retryAfterMs")).toDouble())
                : -1;
        outcome.identity = message.value(QStringLiteral("identity")).toObject();
        outcome.label = message.value(QStringLiteral("label")).toString();
        return outcome;
    }

    Outcome lan(Link& link)
    {
        if (!greet(link, {}, {}, QStringLiteral("lan"))) {
            return {};
        }
        return finish(link.next());
    }

    Outcome code(Link& link, const QString& typed, bool lie = false, bool stopAfterStep2 = false)
    {
        if (!greet(link)) {
            return {};
        }
        const QJsonObject step0 = link.next();
        if (step0.value(QStringLiteral("type")).toString() != QLatin1String("pair.spake")) {
            return finish(step0);
        }
        SpakeExchange exchange(SpakeExchange::Role::Device);
        const QByteArray response1 = exchange.deviceStep1(
            StationIdentity::fromBase64Url(step0.value(QStringLiteral("data")).toString()),
            PairingCode::normalise(typed));
        link.send(SessionMessages::pairSpake(1, StationIdentity::toBase64Url(response1)));
        const QJsonObject step2 = link.next();
        if (step2.value(QStringLiteral("type")).toString() != QLatin1String("pair.spake")) {
            return finish(step2);
        }
        if (stopAfterStep2) {
            return Outcome{};
        }
        QByteArray response3;
        if (lie) {
            response3 = QByteArray(SpakeExchange::kResponse3Bytes, '\x5a');
        } else {
            response3 = exchange.deviceStep3(
                StationIdentity::fromBase64Url(step2.value(QStringLiteral("data")).toString()));
            if (response3.isEmpty()) {
                // The codes differ: say so, and hear when to try again.
                link.send(SessionMessages::pairFail(QStringLiteral("The code did not match."), 0));
                return finish(link.next());
            }
        }
        link.send(SessionMessages::pairSpake(3, StationIdentity::toBase64Url(response3)));
        if (!lie && beforeConfirm) {
            beforeConfirm();
        }
        if (!lie) {
            const QJsonObject box{
                {QStringLiteral("publicKey"),
                 boxKey.isEmpty() ? device.publicKey() : boxKey},
                {QStringLiteral("name"), boxName},
                {QStringLiteral("kind"), boxKind},
            };
            link.send(SessionMessages::pairConfirm(StationIdentity::toBase64Url(
                exchange.sealConfirmation(QJsonDocument(box).toJson(QJsonDocument::Compact)))));
        }
        const QJsonObject answer = link.next();
        Outcome outcome = finish(answer);
        if (outcome.type == QLatin1String("pair.confirm")) {
            const std::optional<QByteArray> plain = exchange.openConfirmation(
                StationIdentity::fromBase64Url(answer.value(QStringLiteral("box")).toString()));
            const QJsonObject station =
                plain ? QJsonDocument::fromJson(*plain).object() : QJsonObject{};
            outcome.identity = station.value(QStringLiteral("identity")).toObject();
            outcome.label = station.value(QStringLiteral("label")).toString();
        }
        return outcome;
    }
};

// ── One Core over the loopback ──────────────────────────────────────────

QList<QJsonObject> ofType(const QList<QByteArray>& received, const QString& type)
{
    QList<QJsonObject> out;
    for (const QByteArray& wire : received) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        if (o.value(QStringLiteral("type")).toString() == type) {
            out.append(o);
        }
    }
    return out;
}

QJsonObject firstOfType(const QList<QByteArray>& received, const QString& type)
{
    const QList<QJsonObject> all = ofType(received, type);
    return all.isEmpty() ? QJsonObject{} : all.first();
}

// The last value `name` had on the `devices` object in what `app` received.
std::optional<QJsonValue> devicesValue(LoopbackTransport* app, const QString& name)
{
    std::optional<QJsonValue> value;
    for (const QByteArray& wire : app->received()) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        const QString type = o.value(QStringLiteral("type")).toString();
        if ((type != QLatin1String("object.create") && type != QLatin1String("delta"))
            || o.value(QStringLiteral("key")).toString() != QLatin1String("devices")) {
            continue;
        }
        for (const QJsonValue& entry : o.value(QStringLiteral("properties")).toArray()) {
            if (entry.toObject().value(QStringLiteral("name")).toString() == name) {
                value = entry.toObject().value(QStringLiteral("value"));
            }
        }
    }
    return value;
}

struct Core {
    QTemporaryDir settingsDir;
    QTemporaryDir securityDir;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<RadioModel> model;
    std::unique_ptr<StationServer> server;
    QList<LoopbackTransport*> clients;
    qint64 clock = 5000000;
    quint32 nextCommandId = 1;

    explicit Core(bool upgradedWithToken = false)
    {
        settings = std::make_unique<AppSettings>(
            settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
        settings->setValue(QStringLiteral("StationCallsign"), QStringLiteral("KG4VCF"));
        model = std::make_unique<RadioModel>();
        model->setBoardForTest(HPSDRHW::HermesLite);
        RadioInfo info;
        info.macAddress = QStringLiteral("AA:BB:CC:DD:EE:01");
        info.name = QStringLiteral("Bench HL2");
        info.boardType = HPSDRHW::HermesLite;
        model->setLastRadioInfoForTest(info);
        model->setConnectionStateForTest(ConnectionState::Connected);
        model->addSlice(QStringLiteral("pan-0"));
        const QString dir = upgradedWithToken
                                ? NereusSDR::Test::seedUpgradedCoreToken(securityDir.path())
                                : NereusSDR::Test::seedCoreIdentity(securityDir.path());
        server = std::make_unique<StationServer>(model.get(), *settings, dir);
        server->setHeartbeatIntervalMs(0);
        window().setClock([this] { return clock; });
    }

    ~Core()
    {
        server.reset();
        qDeleteAll(clients);
    }

    PairingWindow& window() const { return *server->pairingWindow(); }
    DeviceStore& store() const { return *server->deviceStore(); }

    void advance(qint64 ms)
    {
        clock += ms;
        window().poll();
    }

    QByteArray certSha256() const
    {
        QString pin = server->certificateFingerprint();
        pin.remove(QLatin1Char(':'));
        return QByteArray::fromHex(pin.toLatin1());
    }

    LoopbackTransport* open(const QString& address = QStringLiteral("127.0.0.1"))
    {
        auto* app = new LoopbackTransport(QStringLiteral("app"));
        auto* station = new LoopbackTransport(QStringLiteral("station"));
        station->setPeerAddress(address);
        station->linkTo(app);
        clients.append(app);
        server->acceptTransport(station);
        return app;
    }

    // A control connection the remote access service introduced, as
    // StationRendezvous hands one over once its channel is open.
    LoopbackTransport* openIntroduced(const QString& address = QString())
    {
        auto* app = new LoopbackTransport(QStringLiteral("app"));
        auto* station = new LoopbackTransport(QStringLiteral("introduced"));
        station->setPeerAddress(address);
        station->linkTo(app);
        clients.append(app);
        server->acceptIntroducedTransport(station, QStringLiteral("introduction-1"));
        return app;
    }

    // A pairing mailbox, as StationRendezvous hands one over: no hello, no
    // address.
    LoopbackTransport* openMailbox()
    {
        auto* app = new LoopbackTransport(QStringLiteral("app"));
        auto* station = new LoopbackTransport(QStringLiteral("mailbox"));
        station->linkTo(app);
        clients.append(app);
        server->acceptPairingMailbox(station);
        return app;
    }

    Outcome pairByCodeThroughService(const Device& device, const QString& typed,
                                     bool lie = false)
    {
        LoopbackLink link(openMailbox());
        DeviceSide side(device);
        side.throughService = true;
        const Outcome outcome = side.code(link, typed, lie);
        link.ended();
        return outcome;
    }

    Outcome pairByCode(const Device& device, const QString& typed, bool lie = false,
                       const QString& address = QStringLiteral("127.0.0.1"))
    {
        LoopbackLink link(open(address));
        DeviceSide side(device);
        const Outcome outcome = side.code(link, typed, lie);
        link.ended();
        return outcome;
    }

    // pairByCode, with `beforeConfirm` run just before the device's box.
    Outcome pairByCodeClosingFirst(const Device& device, const std::function<void()>& beforeConfirm)
    {
        LoopbackLink link(open());
        DeviceSide side(device);
        side.beforeConfirm = beforeConfirm;
        const Outcome outcome = side.code(link, window().currentCode());
        link.ended();
        return outcome;
    }

    Outcome pairByTap(const Device& device, const QString& address = QStringLiteral("127.0.0.1"))
    {
        LoopbackLink link(open(address));
        DeviceSide side(device);
        const Outcome outcome = side.lan(link);
        link.ended();
        return outcome;
    }

    static bool connect(LoopbackTransport* app, const SessionMessage& auth,
                        const QHash<QByteArray, int>& features = {{"deviceAuth", 1}})
    {
        const bool greeted = QTest::qWaitFor([app]() { return !app->received().isEmpty(); }, 5000);
        Q_UNUSED(greeted);
        app->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("NereusSDR iPhone"),
            {kSessionProtocolMajor}, features)));
        app->sendText(SessionMessages::encode(auth));
        return QTest::qWaitFor(
                   [app]() {
                       return app->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"))
                           || !app->isOpen();
                   },
                   5000)
            && app->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"));
    }

    LoopbackTransport* deviceSession(const Device& device,
                                     const QHash<QByteArray, int>& features = {{"deviceAuth", 1}})
    {
        LoopbackTransport* app = open();
        static_cast<void>(QTest::qWaitFor([app]() { return !app->received().isEmpty(); }, 5000));
        const QJsonObject hello = firstOfType(app->received(), QStringLiteral("hello"));
        const QByteArray challenge =
            StationIdentity::fromBase64Url(hello.value(QStringLiteral("challenge")).toString());
        const SessionMessage auth = SessionMessages::authRequest(
            QString(), device.block(challenge, certSha256(),
                                    server->stationIdentity().publicKeySpki()));
        return connect(app, auth, features) ? app : nullptr;
    }

    LoopbackTransport* tokenSession()
    {
        LoopbackTransport* app = open();
        return connect(app, SessionMessages::authRequest(server->token())) ? app : nullptr;
    }

    QJsonObject invoke(LoopbackTransport* app, const QByteArray& verb,
                       const QList<MirrorUpdate>& arguments = {})
    {
        const quint32 id = nextCommandId++;
        app->sendText(SessionMessages::encode(SessionMessages::commandInvoke(verb, id, arguments)));
        const auto find = [app, id]() {
            for (const QJsonObject& o : ofType(app->received(), QStringLiteral("command.result"))) {
                if (o.value(QStringLiteral("id")).toInteger() == id) {
                    return o;
                }
            }
            return QJsonObject{};
        };
        const bool answered = QTest::qWaitFor([&find]() { return !find().isEmpty(); }, 5000);
        Q_UNUSED(answered);
        return find();
    }

    // The `devices` object's value of `name` once the deltas have flushed.
    std::optional<QJsonValue> settled(LoopbackTransport* app, const QString& name,
                                      const QJsonValue& expected)
    {
        static_cast<void>(QTest::qWaitFor([&]() { return devicesValue(app, name) == expected; }, 3000));
        return devicesValue(app, name);
    }
};

QString codeValueOf(const QJsonObject& result)
{
    for (const QJsonValue& entry : result.value(QStringLiteral("values")).toArray()) {
        if (entry.toObject().value(QStringLiteral("name")).toString() == QLatin1String("code")) {
            return entry.toObject().value(QStringLiteral("value")).toString();
        }
    }
    return QStringLiteral("<absent>");
}

// Every pair.fail reason is plain words and speaks of the Core.
void verifyPlainRefusal(const Outcome& outcome)
{
    QCOMPARE(outcome.type, QStringLiteral("pair.fail"));
    QVERIFY2(OperatorWording::isPlain(outcome.reason), qPrintable(outcome.reason));
    QVERIFY2(outcome.reason.contains(QStringLiteral("Core")), qPrintable(outcome.reason));
    QVERIFY2(OperatorWording::coreCalledStationIn(outcome.reason).isEmpty(),
             qPrintable(outcome.reason));
    QVERIFY(outcome.retryAfterMs >= 0);
}

} // namespace

class TstStationPairing : public QObject {
    Q_OBJECT

private slots:
    void anExistingDeviceCanConfirmTheCodeThroughEitherRoute_data()
    {
        QTest::addColumn<bool>("service");
        QTest::newRow("direct") << false;
        QTest::newRow("service") << true;
    }

    void anExistingDeviceCanConfirmTheCodeThroughEitherRoute()
    {
        QFETCH(bool, service);
        Core core;
        Device device;
        QVERIFY(core.store().add(device.record()));
        const PairedDevice before = *core.store().find(device.id());
        core.window().reopen();
        device.name = QStringLiteral("Do not replace the saved name");
        const QString code = core.window().currentCode();
        const Outcome outcome = service ? core.pairByCodeThroughService(device, code)
                                        : core.pairByCode(device, code);
        QCOMPARE(outcome.type, QStringLiteral("pair.confirm"));
        QCOMPARE(outcome.identity.value(QStringLiteral("publicKey")).toString(),
                 StationIdentity::toBase64Url(core.server->stationIdentity().publicKeySpki()));
        QCOMPARE(core.store().list().size(), 1);
        const PairedDevice after = *core.store().find(device.id());
        QCOMPARE(after.name, before.name);
        QCOMPARE(after.kind, before.kind);
        QCOMPARE(after.pairedAt, before.pairedAt);
        QCOMPARE(after.lastSeen, before.lastSeen);
        QCOMPARE(after.lastAddress, before.lastAddress);
        QCOMPARE(after.publicKeySpki, before.publicKeySpki);
        QCOMPARE(core.window().state(), PairingWindow::State::ClosedClaimed);
        QVERIFY(core.deviceSession(device) != nullptr);
    }

    void anExistingDeviceStillNeedsTheOpenWindowsCode()
    {
        Core core;
        Device device;
        QVERIFY(core.store().add(device.record()));
        verifyPlainRefusal(core.pairByCode(device, QStringLiteral("7-anvil-harbor")));
        core.window().reopen();
        verifyPlainRefusal(core.pairByCodeThroughService(
            device, core.window().currentCode(), /*lie=*/true));
        QCOMPARE(core.window().consecutiveServiceFailures(), 1);
        QCOMPARE(core.window().consecutiveFailures(), 0);
        QCOMPARE(core.store().list().size(), 1);
    }

    void revokingAnExistingDeviceDuringCodeConfirmationCannotRestoreIt_data()
    {
        QTest::addColumn<bool>("readd");
        QTest::newRow("removed") << false;
        QTest::newRow("removed-and-readded") << true;
    }

    void revokingAnExistingDeviceDuringCodeConfirmationCannotRestoreIt()
    {
        QFETCH(bool, readd);
        Core core;
        Device device;
        Device other;
        QVERIFY(core.store().add(device.record()));
        QVERIFY(core.store().add(other.record()));
        const PairedDevice original = *core.store().find(device.id());
        core.window().reopen();
        bool reachedConfirmation = false;
        const Outcome outcome = core.pairByCodeClosingFirst(device, [&] {
            reachedConfirmation = true;
            QVERIFY(core.store().remove(device.id()));
            if (readd) {
                QVERIFY(core.store().add(original));
            }
        });
        QVERIFY(reachedConfirmation);
        verifyPlainRefusal(outcome);
        QCOMPARE(core.store().find(device.id()).has_value(), readd);
    }

    // ── Refusals, burn and backoff ──────────────────────────────────────

    void oneTapIsRefusedOnAClaimedCore()
    {
        Core core;
        Device first;
        QVERIFY(core.store().add(first.record()));
        core.window().reopen();
        Device second;
        const Outcome outcome = core.pairByTap(second);
        verifyPlainRefusal(outcome);
        QCOMPARE(outcome.retryAfterMs, qint64(0));
        QVERIFY(!core.store().find(second.id()));
        // Refused, the reopened window stays open for the code.
        QCOMPARE(core.window().state(), PairingWindow::State::OpenReopened);
    }

    void oneTapIsRefusedWhenDenied()
    {
        Core core;
        core.server->setPairingLanClickAllowed(false);
        Device device;
        verifyPlainRefusal(core.pairByTap(device));
        QVERIFY(core.store().list().isEmpty());
        QCOMPARE(core.window().state(), PairingWindow::State::OpenUnclaimed);
    }

    void oneTapIsRefusedOffTheCoresNetworks()
    {
        Core core;
        Device device;
        // TEST-NET-1 is on no interface of this machine; a relayed
        // connection reports no address at all.
        QVERIFY(!StationServer::isOnDirectNetwork(QStringLiteral("192.0.2.7")));
        verifyPlainRefusal(core.pairByTap(device, QStringLiteral("192.0.2.7")));
        verifyPlainRefusal(core.pairByTap(device, QString()));
        QVERIFY(core.store().list().isEmpty());
        QVERIFY(StationServer::isOnDirectNetwork(QStringLiteral("127.0.0.1")));
        QVERIFY(StationServer::isOnDirectNetwork(QStringLiteral("::1")));
        QVERIFY(!StationServer::isOnDirectNetwork(QStringLiteral("not an address")));
    }

    void theCodeIsRefusedWhileTheWindowIsClosed()
    {
        Core core;
        Device first;
        QVERIFY(core.store().add(first.record()));
        QCOMPARE(core.window().state(), PairingWindow::State::ClosedClaimed);
        Device second;
        const Outcome outcome = core.pairByCode(second, QStringLiteral("7-anvil-harbor"));
        verifyPlainRefusal(outcome);
        QCOMPARE(outcome.retryAfterMs, qint64(0));
        QVERIFY(!core.store().find(second.id()));
    }

    void anExchangeInFlightDoesNotPairOnceTheWindowHasClosed()
    {
        // Part C fix wave (R1-M1): the operator closes pairing after reading
        // the code to the wrong person; the exchange that already took the
        // code is refused at its confirm step, and the code is burned.
        const QString closedReason = QStringLiteral(
            "This Core is not taking new devices. Open pairing on the Core or on a paired "
            "device first.");
        {
            Core core;
            Device first;
            QVERIFY(core.store().add(first.record()));
            core.window().reopen();
            Device second;
            const Outcome outcome =
                core.pairByCodeClosingFirst(second, [&core] { core.window().close(); });
            QCOMPARE(outcome.type, QStringLiteral("pair.fail"));
            QCOMPARE(outcome.reason, closedReason);
            QCOMPARE(outcome.retryAfterMs, qint64(0));
            QVERIFY(!core.store().find(second.id()));
            QVERIFY(!core.window().codeInUse());
            QCOMPARE(core.window().consecutiveFailures(), 1);
            QCOMPARE(core.window().state(), PairingWindow::State::ClosedClaimed);
        }
        {
            // Closed and reopened in between: the new window's code is not
            // the one that exchange took.
            Core core;
            Device first;
            QVERIFY(core.store().add(first.record()));
            core.window().reopen();
            Device second;
            const Outcome outcome = core.pairByCodeClosingFirst(second, [&core] {
                core.window().close();
                core.window().reopen();
            });
            QCOMPARE(outcome.type, QStringLiteral("pair.fail"));
            QVERIFY(!core.store().find(second.id()));
        }
        {
            // The reopened window's ten minutes ran out mid-exchange.
            Core core;
            Device first;
            QVERIFY(core.store().add(first.record()));
            core.window().reopen();
            Device second;
            const Outcome outcome = core.pairByCodeClosingFirst(
                second, [&core] { core.advance(PairingWindow::kReopenedLifetimeMs); });
            QCOMPARE(outcome.type, QStringLiteral("pair.fail"));
            QCOMPARE(outcome.reason, closedReason);
            QVERIFY(!core.store().find(second.id()));
        }
    }

    void aWrongCodeFailsOnTheDevicesSideAndBurns()
    {
        Core core;
        const QString code = core.window().currentCode();
        QVERIFY(!code.isEmpty());
        QStringList parts = code.split(QLatin1Char('-'));
        const QStringList& words = PairingCode::wordList();
        parts[2] = words.at((words.indexOf(parts.at(2)) + 1) % words.size());
        Device device;
        const Outcome outcome = core.pairByCode(device, parts.join(QLatin1Char('-')));
        verifyPlainRefusal(outcome);
        QCOMPARE(outcome.retryAfterMs, qint64(5000));
        QVERIFY(core.store().list().isEmpty());
        QVERIFY(core.window().currentCode().isEmpty());
        core.advance(4999);
        QVERIFY(core.window().currentCode().isEmpty());
        core.advance(1);
        QVERIFY(!core.window().currentCode().isEmpty());
        QVERIFY(core.window().currentCode() != code);
    }

    void aWrongCodeFailsOnTheCoresSideAndTheWaitDoubles()
    {
        Core core;
        Device device;
        // No code at all: step 3 made up, which the Core's step 4 refuses.
        Outcome outcome = core.pairByCode(device, core.window().currentCode(), /*lie=*/true);
        verifyPlainRefusal(outcome);
        QCOMPARE(outcome.retryAfterMs, qint64(5000));
        // While the wait runs there is no code to try.
        outcome = core.pairByCode(device, QStringLiteral("7-anvil-harbor"));
        verifyPlainRefusal(outcome);
        QCOMPARE(outcome.retryAfterMs, qint64(5000));
        core.advance(5000);
        outcome = core.pairByCode(device, core.window().currentCode(), /*lie=*/true);
        verifyPlainRefusal(outcome);
        QCOMPARE(outcome.retryAfterMs, qint64(10000));
        QVERIFY(core.store().list().isEmpty());
        // A success resets the wait.
        core.advance(10000);
        QCOMPARE(core.pairByCode(device, core.window().currentCode()).type,
                 QStringLiteral("pair.confirm"));
        QCOMPARE(core.window().consecutiveFailures(), 0);
    }

    // The operator's ruling on Task 27 item I5 (2026-09-26): wrong codes
    // through the remote access service pause pairing through it, never the
    // window and never pairing on the home network. A paused mailbox
    // pairing is refused before it takes a code, so it burns nothing.
    void wrongCodesThroughTheServicePauseOnlyTheService()
    {
        Core core;
        Device device;
        for (int i = 0; i < PairingWindow::kMaxConsecutiveFailures; ++i) {
            if (core.window().currentCode().isEmpty()) {
                core.advance(core.window().retryAfterMs());
            }
            const Outcome outcome =
                core.pairByCodeThroughService(device, core.window().currentCode(), /*lie=*/true);
            verifyPlainRefusal(outcome);
        }
        QCOMPARE(core.window().state(), PairingWindow::State::OpenUnclaimed);
        QCOMPARE(core.window().consecutiveFailures(), 0);
        QVERIFY(core.window().isPaused(PairingWindow::Route::Service));
        QVERIFY(core.store().list().isEmpty());

        // The next code follows after the first wait (the follow-up to the
        // Task 27 re-review, new Minor 4), inside the first pause (1
        // minute): only the home network can take it.
        QCOMPARE(core.window().retryAfterMs(), PairingWindow::kFirstRetryMs);
        core.advance(core.window().retryAfterMs());
        QVERIFY(!core.window().currentCode().isEmpty());
        QVERIFY(core.window().isPaused(PairingWindow::Route::Service));
        QCOMPARE(core.window().servicePauseRemainingMs(),
                 PairingWindow::kFirstServicePauseMs - PairingWindow::kFirstRetryMs);
        const QString code = core.window().currentCode();
        const quint64 serial = core.window().codeSerial();
        const qint64 remaining = core.window().servicePauseRemainingMs();

        // Even the right code is refused through the service while paused,
        // and nothing is burned.
        const Outcome paused = core.pairByCodeThroughService(device, code);
        verifyPlainRefusal(paused);
        QCOMPARE(paused.reason,
                 QStringLiteral("The Core has paused pairing from outside its network after "
                                "several wrong codes. Try again later, or pair on the Core's "
                                "own network."));
        QCOMPARE(paused.retryAfterMs, remaining);
        QCOMPARE(core.window().currentCode(), code);
        QCOMPARE(core.window().codeSerial(), serial);
        QCOMPARE(core.window().consecutiveServiceFailures(), 0);
        QVERIFY(core.store().list().isEmpty());

        // On the home network the same code pairs at once.
        QCOMPARE(core.pairByCode(device, code).type, QStringLiteral("pair.confirm"));
        QVERIFY(core.store().find(device.id()));
        QVERIFY(!core.window().isPaused(PairingWindow::Route::Service));
    }

    // LINK-I4 (JJ's ruling, 2026-09-30): 19 wrong codes through the service
    // in total still let the 20th be tried; the 20th shuts pairing through
    // the service. Then even the right code through the service is refused
    // with no time to try again, burning nothing, while the home network
    // still pairs; that pairing turns the service back on.
    void twentyWrongCodesThroughTheServiceShutIt()
    {
        Core core;
        Device device;
        const QString shut = QStringLiteral(
            "The Core has turned off pairing from outside its network after too many wrong "
            "codes. Pair on the Core's own network, or reopen pairing at the Core.");
        const auto nextServiceTry = [&core] {
            if (core.window().isPaused(PairingWindow::Route::Service)) {
                core.advance(core.window().servicePauseRemainingMs());
            }
            if (core.window().currentCode().isEmpty()) {
                core.advance(core.window().retryAfterMs());
            }
        };
        for (int i = 1; i <= PairingWindow::kMaxServiceFailuresTotal; ++i) {
            nextServiceTry();
            QVERIFY(!core.window().isServiceShut());
            const Outcome outcome =
                core.pairByCodeThroughService(device, core.window().currentCode(), /*lie=*/true);
            verifyPlainRefusal(outcome);
            QVERIFY2(outcome.reason != shut, qPrintable(QString::number(i)));
            QCOMPARE(core.window().serviceFailuresTotal(), i);
        }
        QCOMPARE(PairingWindow::kMaxServiceFailuresTotal, 20);
        QVERIFY(core.window().isServiceShut());
        QCOMPARE(core.window().state(), PairingWindow::State::OpenUnclaimed);

        // Waiting out any pause changes nothing: still shut.
        nextServiceTry();
        QVERIFY(!core.window().isPaused(PairingWindow::Route::Service));
        const QString code = core.window().currentCode();
        const quint64 serial = core.window().codeSerial();
        const Outcome refused = core.pairByCodeThroughService(device, code);
        verifyPlainRefusal(refused);
        QCOMPARE(refused.reason, shut);
        QCOMPARE(refused.retryAfterMs, qint64(0));
        QCOMPARE(core.window().currentCode(), code);
        QCOMPARE(core.window().codeSerial(), serial);
        QCOMPARE(core.window().serviceFailuresTotal(), PairingWindow::kMaxServiceFailuresTotal);
        QVERIFY(core.store().list().isEmpty());

        // The home network pairs with the same code, and that turns the
        // service back on.
        QCOMPARE(core.pairByCode(device, code).type, QStringLiteral("pair.confirm"));
        QVERIFY(core.store().find(device.id()));
        QVERIFY(!core.window().isServiceShut());
        QCOMPARE(core.window().serviceFailuresTotal(), 0);
    }

    // Task 28 fix wave (review Important 1): a connection the service
    // introduced never pairs, by code or by tap; pairing through the
    // service is the mailbox's. Nothing is burned and nothing is paired.
    void aConnectionThroughTheServiceCannotPair()
    {
        Core core;
        Device device;
        const QString code = core.window().currentCode();
        const quint64 serial = core.window().codeSerial();
        QVERIFY(!code.isEmpty());
        const QString reason = QStringLiteral(
            "A device cannot pair over a connection through the remote access service. Pair it "
            "with the Core's pairing code.");
        for (const QString& address : {QString(), QStringLiteral("198.51.100.4")}) {
            {
                LoopbackLink link(core.openIntroduced(address));
                DeviceSide side(device);
                const Outcome outcome = side.code(link, code);
                verifyPlainRefusal(outcome);
                QCOMPARE(outcome.reason, reason);
                QVERIFY(link.ended());
            }
            {
                LoopbackLink link(core.openIntroduced(address));
                DeviceSide side(device);
                const Outcome outcome = side.lan(link);
                verifyPlainRefusal(outcome);
                QCOMPARE(outcome.reason, reason);
                QVERIFY(link.ended());
            }
        }
        QCOMPARE(core.window().currentCode(), code);
        QCOMPARE(core.window().codeSerial(), serial);
        QCOMPARE(core.window().consecutiveFailures(), 0);
        QCOMPARE(core.window().consecutiveServiceFailures(), 0);
        QVERIFY(core.store().list().isEmpty());
        // The same code pairs through the mailbox.
        QCOMPARE(core.pairByCodeThroughService(device, code).type, QStringLiteral("pair.confirm"));
        QVERIFY(core.store().find(device.id()));
    }

    void leavingAfterTheCoreCommittedBurnsTheCode()
    {
        Core core;
        Device device;
        const QString code = core.window().currentCode();
        {
            LoopbackLink link(core.open());
            DeviceSide side(device);
            side.code(link, code, false, /*stopAfterStep2=*/true);
            core.clients.last()->closeLink(QStringLiteral("gone"));
            QVERIFY(QTest::qWaitFor([&core]() { return core.server->peerCount() == 0; }, 5000));
        }
        QCOMPARE(core.window().consecutiveFailures(), 1);
        QVERIFY(core.window().currentCode().isEmpty());
        QCOMPARE(core.window().retryAfterMs(), qint64(5000));
        // Leaving before the Core committed costs nothing.
        core.advance(5000);
        const QString next = core.window().currentCode();
        LoopbackTransport* app = core.open();
        LoopbackLink link(app);
        DeviceSide side(device);
        QVERIFY(side.greet(link));
        QCOMPARE(link.next().value(QStringLiteral("type")).toString(),
                 QStringLiteral("pair.spake"));
        app->closeLink(QStringLiteral("gone"));
        QVERIFY(QTest::qWaitFor([&core]() { return core.server->peerCount() == 0; }, 5000));
        QVERIFY(core.window().currentCode() == next);
        QCOMPARE(core.window().consecutiveFailures(), 1);
    }

    void theCodeGoesToOneExchangeAtATime()
    {
        Core core;
        const QString code = core.window().currentCode();
        Device first;
        Device second;
        // Both hear step 0 on the same code.
        LoopbackLink a(core.open());
        LoopbackLink b(core.open());
        DeviceSide sideA(first);
        DeviceSide sideB(second);
        QVERIFY(sideA.greet(a));
        QVERIFY(sideB.greet(b));
        const QJsonObject stepA = a.next();
        const QJsonObject stepB = b.next();
        QCOMPARE(stepA.value(QStringLiteral("step")).toInt(), 0);
        QCOMPARE(stepB.value(QStringLiteral("step")).toInt(), 0);
        SpakeExchange exchangeA(SpakeExchange::Role::Device);
        SpakeExchange exchangeB(SpakeExchange::Role::Device);
        a.send(SessionMessages::pairSpake(
            1, StationIdentity::toBase64Url(exchangeA.deviceStep1(
                   StationIdentity::fromBase64Url(stepA.value(QStringLiteral("data")).toString()),
                   code))));
        QCOMPARE(a.next().value(QStringLiteral("step")).toInt(), 2);
        // The code is now A's; B's guess is not taken.
        b.send(SessionMessages::pairSpake(
            1, StationIdentity::toBase64Url(exchangeB.deviceStep1(
                   StationIdentity::fromBase64Url(stepB.value(QStringLiteral("data")).toString()),
                   code))));
        const Outcome refused = DeviceSide::finish(b.next());
        verifyPlainRefusal(refused);
        QVERIFY(refused.retryAfterMs > 0);
        QVERIFY(b.ended());
    }

    void thePairingCodeNeverReachesTheLog()
    {
        {
            QMutexLocker lock(&g_logMutex);
            g_log.clear();
        }
        g_previousHandler = qInstallMessageHandler(captureLog);
        QStringList codes;
        {
            Core core;
            QObject::connect(&core.window(), &PairingWindow::codeChanged, &core.window(),
                             [&codes](const QString& code) {
                                 if (!code.isEmpty()) {
                                     codes.append(code);
                                 }
                             });
            codes.append(core.window().currentCode());
            Device wrong;
            core.pairByCode(wrong, core.window().currentCode(), /*lie=*/true);
            core.advance(5000);
            Device device;
            QCOMPARE(core.pairByCode(device, core.window().currentCode()).type,
                     QStringLiteral("pair.confirm"));
            LoopbackTransport* app = core.deviceSession(device);
            QVERIFY(app != nullptr);
            const QJsonObject opened = core.invoke(app, "pairing.open");
            QVERIFY(opened.value(QStringLiteral("accepted")).toBool());
            QVERIFY(!codeValueOf(opened).isEmpty());
            core.settled(app, QStringLiteral("pairingWindowOpen"), true);
            Device third;
            third.name = QStringLiteral("Shack iPad");
            QCOMPARE(core.pairByCode(third, core.window().currentCode()).type,
                     QStringLiteral("pair.confirm"));
            // Part C fix wave: nothing prints the code either (standard
            // output is the journal on a packaged Core); the console's
            // `nereusd pairing show` gives it on request
            // (tst_station_control_socket).
        }
        qInstallMessageHandler(g_previousHandler);
        QVERIFY(codes.size() >= 3);
        QMutexLocker lock(&g_logMutex);
        QVERIFY(!g_log.isEmpty());
        for (const QString& line : std::as_const(g_log)) {
            for (const QString& code : std::as_const(codes)) {
                QVERIFY2(!line.contains(code), "a log line holds a pairing code");
                // Nor any of its words beside its number.
                QVERIFY2(!line.contains(code.section(QLatin1Char('-'), 1)),
                         "a log line holds a pairing code's words");
            }
        }
    }

    void theCodeIsHashedOffTheEventLoop()
    {
        // A pairing never stalls another device's session: the code's
        // Argon2id hash runs on a worker while the Core's event loop keeps
        // serving a paired device and a timer. The test holds the hash on
        // the worker until it has seen both.
        Core core;
        Device paired;
        QVERIFY(core.store().add(paired.record()));
        core.window().reopen();
        LoopbackTransport* session = core.deviceSession(paired);
        QVERIFY(session != nullptr);

        QSemaphore entered;
        QSemaphore gate;
        QThread* hashThread = nullptr;
        core.server->setPairingHasherForTest([&](const QString& code) {
            hashThread = QThread::currentThread();
            entered.release();
            // Bounded, so a hash run on the event loop fails the test
            // below instead of hanging it.
            static_cast<void>(gate.tryAcquire(1, 3000));
            return SpakeExchange::storedData(code);
        });
        // Never leave the worker blocked, whatever fails below.
        auto release = qScopeGuard([&gate]() { gate.release(); });

        Device device;
        LoopbackTransport* app = core.open();
        LoopbackLink link(app);
        DeviceSide side(device);
        QVERIFY(side.greet(link));
        QVERIFY(QTest::qWaitFor([&entered]() { return entered.available() > 0; }, 5000));
        QVERIFY(core.server->isHashingPairingCodeForTest());
        QVERIFY(hashThread != nullptr);
        QVERIFY(hashThread != QThread::currentThread());

        // The event loop serves while the hash is held: a timer ticks, and
        // the paired device's request is answered.
        int ticks = 0;
        QTimer timer;
        timer.setInterval(1);
        QObject::connect(&timer, &QTimer::timeout, [&ticks]() { ++ticks; });
        timer.start();
        QVERIFY(QTest::qWaitFor([&ticks]() { return ticks >= 5; }, 5000));
        const QJsonObject answer = core.invoke(session, "pairing.open");
        QVERIFY(answer.value(QStringLiteral("accepted")).toBool());
        QVERIFY(session->isOpen());
        // And the pairing waits for its hash: no step 0 yet.
        QVERIFY(ofType(app->received(), QStringLiteral("pair.spake")).isEmpty());

        // Released, the hash comes back and the pairing completes.
        release.dismiss();
        gate.release();
        const QString code = core.window().currentCode();
        const QJsonObject step0 = link.next();
        QCOMPARE(step0.value(QStringLiteral("type")).toString(), QStringLiteral("pair.spake"));
        QCOMPARE(step0.value(QStringLiteral("step")).toInt(), 0);
        QVERIFY(!core.server->isHashingPairingCodeForTest());
        SpakeExchange exchange(SpakeExchange::Role::Device);
        link.send(SessionMessages::pairSpake(
            1, StationIdentity::toBase64Url(exchange.deviceStep1(
                   StationIdentity::fromBase64Url(step0.value(QStringLiteral("data")).toString()),
                   code))));
        const QJsonObject step2 = link.next();
        QCOMPARE(step2.value(QStringLiteral("step")).toInt(), 2);
        link.send(SessionMessages::pairSpake(
            3, StationIdentity::toBase64Url(exchange.deviceStep3(
                   StationIdentity::fromBase64Url(step2.value(QStringLiteral("data")).toString())))));
        const QJsonObject box{{QStringLiteral("publicKey"), device.publicKey()},
                              {QStringLiteral("name"), device.name},
                              {QStringLiteral("kind"), device.kind}};
        link.send(SessionMessages::pairConfirm(StationIdentity::toBase64Url(
            exchange.sealConfirmation(QJsonDocument(box).toJson(QJsonDocument::Compact)))));
        QCOMPARE(link.next().value(QStringLiteral("type")).toString(),
                 QStringLiteral("pair.confirm"));
        QVERIFY(core.store().find(device.id()).has_value());
        QVERIFY(session->isOpen());
    }

    void aTokenSignInNeverReceivesTheCode()
    {
        Core core(/*upgradedWithToken=*/true);
        QCOMPARE(core.window().state(), PairingWindow::State::ClosedClaimed);
        LoopbackTransport* token = core.tokenSession();
        QVERIFY(token != nullptr);
        // Its hello declares deviceAuth, so it has the devices object and
        // the verbs, but it may not reopen pairing (Part C follow-up,
        // R-IOS-08: "from a paired device"), and never receives the code.
        const QJsonObject refused = core.invoke(token, "pairing.open");
        QVERIFY(!refused.value(QStringLiteral("accepted")).toBool(true));
        const QString reason = refused.value(QStringLiteral("reason")).toString();
        QCOMPARE(reason,
                 QStringLiteral("Open pairing from a paired device or from the Core's console."));
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        QCOMPARE(codeValueOf(refused), QStringLiteral("<absent>"));
        QCOMPARE(core.window().state(), PairingWindow::State::ClosedClaimed);

        // A device signed in with its own key opens it and receives the code.
        Device device;
        QVERIFY(core.store().add(device.record()));
        LoopbackTransport* app = core.deviceSession(device);
        QVERIFY(app != nullptr);
        const QJsonObject opened = core.invoke(app, "pairing.open");
        QVERIFY(opened.value(QStringLiteral("accepted")).toBool());
        QCOMPARE(core.window().state(), PairingWindow::State::OpenReopened);
        QVERIFY(!core.window().currentCode().isEmpty());
        QVERIFY(codeValueOf(opened) == core.window().currentCode());
        QVERIFY(core.settled(app, QStringLiteral("pairingCode"), core.window().currentCode())
                == std::optional<QJsonValue>(core.window().currentCode()));

        // The token window, signed in again, sees the window open and a
        // blank code, and never the code itself.
        LoopbackTransport* again = core.tokenSession();
        QVERIFY(again != nullptr);
        QVERIFY(core.settled(again, QStringLiteral("pairingWindowOpen"), true)
                == std::optional<QJsonValue>(true));
        QVERIFY(devicesValue(again, QStringLiteral("pairingCode"))
                == std::optional<QJsonValue>(QString()));
        for (LoopbackTransport* link : {token, again}) {
            for (const QByteArray& wire : link->received()) {
                QVERIFY2(!wire.contains(core.window().currentCode().toUtf8()),
                         "a token connection received the pairing code");
            }
        }

        // pairing.close stays open to it: closing only narrows who can pair.
        const QJsonObject closed = core.invoke(again, "pairing.close");
        QVERIFY(closed.value(QStringLiteral("accepted")).toBool());
        QCOMPARE(core.window().state(), PairingWindow::State::ClosedClaimed);
    }

    // Fix wave, I5: changing, editing, forgetting or scanning for the
    // Core's radio is for a device signed in with its own key. A window
    // signed in with the pairing token is refused as pairing.open is.
    void theRadioVerbsNeedAPairedDevice()
    {
        Core core(/*upgradedWithToken=*/true);
        StationRadios radios(*core.settings);
        int rescans = 0;
        radios.onRescan = [&rescans]() { ++rescans; };
        core.server->setStationRadios(&radios);
        LoopbackTransport* token = core.tokenSession();
        QVERIFY(token != nullptr);
        const QString why = QStringLiteral("Change the Core's radio from a paired device.");
        QVERIFY2(OperatorWording::isPlain(why), qPrintable(why));
        for (const QByteArray verb : {QByteArrayLiteral("station.rescanRadios"),
                                      QByteArrayLiteral("station.selectRadio"),
                                      QByteArrayLiteral("station.setRadioModel"),
                                      QByteArrayLiteral("station.forgetRadio")}) {
            const QJsonObject refused = core.invoke(token, verb);
            QVERIFY2(!refused.value(QStringLiteral("accepted")).toBool(true), verb.constData());
            QCOMPARE(refused.value(QStringLiteral("reason")).toString(), why);
        }
        QCOMPARE(rescans, 0);
        // Parity ruling C4: the radio's sample rate, a radio-wide change, is
        // for a paired device too.
        const QString rateWhy =
            QStringLiteral("Change the radio's sample rate from a paired device.");
        QVERIFY2(OperatorWording::isPlain(rateWhy), qPrintable(rateWhy));
        const QJsonObject rate = core.invoke(
            token, "setRadioSampleRate",
            {MirrorUpdate{0, "rateHz", MirrorWireKind::Int64, QVariant(qlonglong(96000))}});
        QVERIFY(!rate.value(QStringLiteral("accepted")).toBool(true));
        QCOMPARE(rate.value(QStringLiteral("reason")).toString(), rateWhy);

        // A device signed in with its own key (a desktop window after its
        // enrolment, or the phone) is unaffected.
        Device device;
        QVERIFY(core.store().add(device.record()));
        LoopbackTransport* app = core.deviceSession(device);
        QVERIFY(app != nullptr);
        const QJsonObject scanned = core.invoke(app, "station.rescanRadios");
        QVERIFY(scanned.value(QStringLiteral("accepted")).toBool());
        QCOMPARE(rescans, 1);
        core.server->setStationRadios(nullptr);
    }

    void thePairingVerbsNeedAHelloThatDeclaresDeviceAuth()
    {
        Core core(/*upgradedWithToken=*/true);
        LoopbackTransport* window = core.open();
        QVERIFY(Core::connect(window, SessionMessages::authRequest(core.server->token()), {}));
        for (const QByteArray verb : {QByteArrayLiteral("pairing.open"),
                                      QByteArrayLiteral("pairing.close")}) {
            const QJsonObject result = core.invoke(window, verb);
            QVERIFY(!result.value(QStringLiteral("accepted")).toBool(true));
            const QString reason = result.value(QStringLiteral("reason")).toString();
            QCOMPARE(reason, QStringLiteral("Update this app to pair new devices with this Core."));
            QVERIFY(OperatorWording::isPlain(reason));
        }
        QCOMPARE(core.window().state(), PairingWindow::State::ClosedClaimed);
        QVERIFY(devicesValue(window, QStringLiteral("pairingCode")) == std::nullopt);
    }

    void aPairingConnectionCannotSignIn()
    {
        Core core;
        LoopbackTransport* app = core.open();
        LoopbackLink link(app);
        Device device;
        DeviceSide side(device);
        QVERIFY(side.greet(link));
        QCOMPARE(link.next().value(QStringLiteral("type")).toString(),
                 QStringLiteral("pair.spake"));
        link.send(SessionMessages::authRequest(QString()));
        QVERIFY(link.ended());
        const QJsonObject end = firstOfType(app->received(), QStringLiteral("session.end"));
        QCOMPARE(end.value(QStringLiteral("code")).toString(), QStringLiteral("protocolError"));
        QVERIFY(firstOfType(app->received(), QStringLiteral("auth.result")).isEmpty());
        // A step out of order ends the connection the same way.
        LoopbackTransport* other = core.open();
        LoopbackLink second(other);
        DeviceSide sideTwo(device);
        QVERIFY(sideTwo.greet(second));
        second.next();
        second.send(SessionMessages::pairSpake(3, QStringLiteral("AAAA")));
        QVERIFY(second.ended());
        QCOMPARE(firstOfType(other->received(), QStringLiteral("session.end"))
                     .value(QStringLiteral("code"))
                     .toString(),
                 QStringLiteral("protocolError"));
        QCOMPARE(core.window().consecutiveFailures(), 0);
    }

    // ── Admit paths ─────────────────────────────────────────────────────

    void theCoreDeclaresPairing()
    {
        Core core;
        QCOMPARE(core.server->pairingVersion(), 1);
        LoopbackTransport* app = core.open();
        static_cast<void>(QTest::qWaitFor([app]() { return !app->received().isEmpty(); }, 5000));
        const QJsonObject hello = firstOfType(app->received(), QStringLiteral("hello"));
        QCOMPARE(hello.value(QStringLiteral("features")).toObject()
                     .value(QStringLiteral("pairing")).toInt(),
                 1);
        StationCapabilities caps;
        caps.radioIdentityEntries = true;
        caps.pairingVersion = 1;
        const QList<MirrorUpdate> updates = caps.toUpdates();
        // Later capabilities may follow this deployed contiguous block.
        const QList<QByteArray> originalBlock{
            "pairingVersion", "stationCatalogVersion", "displayExtrasVersion",
            "transmitSettingsVersion", "bandSelectVersion", "meterReadingsVersion",
            "dspInfoVersion", "recordStreamVersion", "stationRadiosVersion",
            "txDisplayVersion", "displayClockVersion", "controlChannelVersion",
            "txMonitorAudioVersion", "stationFreedvVersion", "mediaReplaceVersion",
            "controlSwitchVersion", "relayAllowed", "supportBundleVersion",
            "mediaTunnelVersion", "mediaRelayRoutingVersion"};
        qsizetype first = -1;
        for (qsizetype i = 0; i < updates.size(); ++i) {
            if (updates.at(i).name == originalBlock.first()) {
                QVERIFY(first < 0);
                first = i;
            }
        }
        QVERIFY(first >= 0);
        QVERIFY(first + originalBlock.size() <= updates.size());
        for (qsizetype i = 0; i < originalBlock.size(); ++i) {
            QCOMPARE(updates.at(first + i).name, originalBlock.at(i));
        }
        QCOMPARE(StationCapabilities::fromUpdates(updates).pairingVersion, 1);
    }

    void oneTapPairsAnUnclaimedCoreAndEnds()
    {
        Core core;
        QCOMPARE(core.window().state(), PairingWindow::State::OpenUnclaimed);
        Device device;
        LoopbackTransport* app = core.open();
        LoopbackLink link(app);
        DeviceSide side(device);
        const Outcome outcome = side.lan(link);
        QCOMPARE(outcome.type, QStringLiteral("pair.accept"));
        QCOMPARE(outcome.identity.value(QStringLiteral("publicKey")).toString(),
                 StationIdentity::toBase64Url(core.server->stationIdentity().publicKeySpki()));
        const QByteArray binding = StationIdentity::fromBase64Url(
            outcome.identity.value(QStringLiteral("certBinding")).toString());
        QVERIFY(StationIdentity::verify(core.server->stationIdentity().publicKeySpki(),
                                        StationIdentity::certBindingMessage(core.certSha256()),
                                        binding));
        QCOMPARE(outcome.label, QStringLiteral("KG4VCF"));
        QVERIFY(link.ended());
        QVERIFY(firstOfType(app->received(), QStringLiteral("session.end")).isEmpty());
        const std::optional<PairedDevice> paired = core.store().find(device.id());
        QVERIFY(paired.has_value());
        QCOMPARE(paired->name, device.name);
        QCOMPARE(paired->kind, device.kind);
        QCOMPARE(core.window().state(), PairingWindow::State::ClosedClaimed);
        // Then it signs in by key on a new connection.
        QVERIFY(core.deviceSession(device) != nullptr);
    }

    void theRightCodePairsAndTheBoxNamesTheDevice()
    {
        Core core;
        Device device;
        LoopbackTransport* app = core.open(QStringLiteral("192.0.2.7"));
        LoopbackLink link(app);
        DeviceSide side(device);
        // The plain pair.start says one thing; the box, under the code,
        // says another, and wins.
        side.boxName = QStringLiteral("Shack iPad");
        side.boxKind = QStringLiteral("tablet");
        QVERIFY(side.greet(link, QStringLiteral("Somebody else"), QStringLiteral("computer")));
        // Re-run the exchange on this link with the greeting already sent.
        const QJsonObject step0 = link.next();
        QCOMPARE(step0.value(QStringLiteral("step")).toInt(), 0);
        SpakeExchange exchange(SpakeExchange::Role::Device);
        link.send(SessionMessages::pairSpake(
            1, StationIdentity::toBase64Url(exchange.deviceStep1(
                   StationIdentity::fromBase64Url(step0.value(QStringLiteral("data")).toString()),
                   core.window().currentCode()))));
        const QJsonObject step2 = link.next();
        QCOMPARE(step2.value(QStringLiteral("step")).toInt(), 2);
        link.send(SessionMessages::pairSpake(
            3, StationIdentity::toBase64Url(exchange.deviceStep3(
                   StationIdentity::fromBase64Url(step2.value(QStringLiteral("data")).toString())))));
        const QJsonObject box{{QStringLiteral("publicKey"), device.publicKey()},
                              {QStringLiteral("name"), side.boxName},
                              {QStringLiteral("kind"), side.boxKind}};
        link.send(SessionMessages::pairConfirm(StationIdentity::toBase64Url(
            exchange.sealConfirmation(QJsonDocument(box).toJson(QJsonDocument::Compact)))));
        const QJsonObject confirm = link.next();
        QCOMPARE(confirm.value(QStringLiteral("type")).toString(), QStringLiteral("pair.confirm"));
        const std::optional<QByteArray> plain = exchange.openConfirmation(
            StationIdentity::fromBase64Url(confirm.value(QStringLiteral("box")).toString()));
        QVERIFY(plain.has_value());
        const QJsonObject station = QJsonDocument::fromJson(*plain).object();
        QCOMPARE(station.value(QStringLiteral("identity")).toObject()
                     .value(QStringLiteral("publicKey")).toString(),
                 StationIdentity::toBase64Url(core.server->stationIdentity().publicKeySpki()));
        QCOMPARE(station.value(QStringLiteral("label")).toString(), QStringLiteral("KG4VCF"));
        QVERIFY(link.ended());
        const std::optional<PairedDevice> paired = core.store().find(device.id());
        QVERIFY(paired.has_value());
        QCOMPARE(paired->name, QStringLiteral("Shack iPad"));
        QCOMPARE(paired->kind, QStringLiteral("tablet"));
        QCOMPARE(core.window().state(), PairingWindow::State::ClosedClaimed);
        QVERIFY(core.deviceSession(device) != nullptr);
    }

    void aBoxForAnotherKeyIsRefused()
    {
        Core core;
        Device device;
        Device other;
        LoopbackLink link(core.open());
        DeviceSide side(device);
        side.boxKey = other.publicKey();
        const Outcome outcome = side.code(link, core.window().currentCode());
        verifyPlainRefusal(outcome);
        QVERIFY(core.store().list().isEmpty());
        QCOMPARE(core.window().consecutiveFailures(), 1);
    }

    void pairingOpenAndCloseFromAPairedDevice()
    {
        Core core;
        Device device;
        QVERIFY(core.store().add(device.record()));
        LoopbackTransport* app = core.deviceSession(device);
        QVERIFY(app != nullptr);
        QVERIFY(devicesValue(app, QStringLiteral("pairingWindowOpen"))
                == std::optional<QJsonValue>(false));
        QVERIFY(devicesValue(app, QStringLiteral("pairingCode"))
                == std::optional<QJsonValue>(QString()));
        const QJsonObject opened = core.invoke(app, "pairing.open");
        QVERIFY(opened.value(QStringLiteral("accepted")).toBool());
        QCOMPARE(opened.value(QStringLiteral("affected")).toArray(),
                 QJsonArray{QStringLiteral("devices")});
        QVERIFY(codeValueOf(opened) == core.window().currentCode());
        QVERIFY(core.settled(app, QStringLiteral("pairingWindowOpen"), true)
                == std::optional<QJsonValue>(true));
        QVERIFY(core.settled(app, QStringLiteral("pairingCode"), core.window().currentCode())
                == std::optional<QJsonValue>(core.window().currentCode()));
        const QJsonObject closed = core.invoke(app, "pairing.close");
        QVERIFY(closed.value(QStringLiteral("accepted")).toBool());
        QCOMPARE(core.window().state(), PairingWindow::State::ClosedClaimed);
        QVERIFY(core.settled(app, QStringLiteral("pairingWindowOpen"), false)
                == std::optional<QJsonValue>(false));
        QVERIFY(core.settled(app, QStringLiteral("pairingCode"), QString())
                == std::optional<QJsonValue>(QString()));
    }

    void aReopenedWindowClosesAfterOnePairing()
    {
        Core core;
        Device first;
        QVERIFY(core.store().add(first.record()));
        core.window().reopen();
        Device second;
        second.name = QStringLiteral("Shack laptop");
        second.kind = QStringLiteral("computer");
        QCOMPARE(core.pairByCode(second, core.window().currentCode()).type,
                 QStringLiteral("pair.confirm"));
        QVERIFY(core.store().find(second.id()).has_value());
        QCOMPARE(core.window().state(), PairingWindow::State::ClosedClaimed);
        Device third;
        verifyPlainRefusal(core.pairByCode(third, QStringLiteral("7-anvil-harbor")));
    }

    // ── nereus_pairing_peer against this file's device side ─────────────

    void thePeerPairsByCode()
    {
        ProcessLink link;
        QVERIFY(link.start({}));
        const QString code = link.ready().value(QStringLiteral("code")).toString();
        QVERIFY(!code.isEmpty());
        Device device;
        DeviceSide side(device);
        const Outcome outcome = side.code(link, code);
        QCOMPARE(outcome.type, QStringLiteral("pair.confirm"));
        // The identity in the box is the hello's, and its binding signs the
        // peer's certificate.
        QCOMPARE(outcome.identity.value(QStringLiteral("publicKey")).toString(),
                 side.hello.value(QStringLiteral("identity")).toObject()
                     .value(QStringLiteral("publicKey")).toString());
        const QByteArray spki = StationIdentity::fromBase64Url(
            outcome.identity.value(QStringLiteral("publicKey")).toString());
        QVERIFY(StationIdentity::verify(
            spki,
            StationIdentity::certBindingMessage(StationIdentity::fromBase64Url(
                link.ready().value(QStringLiteral("certSha256")).toString())),
            StationIdentity::fromBase64Url(
                outcome.identity.value(QStringLiteral("certBinding")).toString())));
        QVERIFY(link.ended());
        QCOMPARE(link.done().value(QStringLiteral("paired")).toBool(), true);
        QCOMPARE(link.done().value(QStringLiteral("devices")).toInt(), 1);
    }

    void thePeerRefusesAWrongCode()
    {
        ProcessLink link;
        QVERIFY(link.start({}));
        const QString code = link.ready().value(QStringLiteral("code")).toString();
        QStringList parts = code.split(QLatin1Char('-'));
        const QStringList& words = PairingCode::wordList();
        parts[1] = words.at((words.indexOf(parts.at(1)) + 1) % words.size());
        Device device;
        DeviceSide side(device);
        // The next code follows kFirstRetryMs after the burn, and the peer
        // reports what is left of that wait when it answers. The peer is
        // another process, so there is no clock to hold still: the answer
        // is inside the window the product promises, less no more than the
        // time this exchange took (a loaded machine lets milliseconds pass).
        QElapsedTimer exchange;
        exchange.start();
        const Outcome outcome = side.code(link, parts.join(QLatin1Char('-')));
        const qint64 took = exchange.elapsed();
        verifyPlainRefusal(outcome);
        QVERIFY2(outcome.retryAfterMs <= PairingWindow::kFirstRetryMs
                     && outcome.retryAfterMs >= PairingWindow::kFirstRetryMs - took,
                 qPrintable(QStringLiteral("retryAfterMs %1, exchange took %2 ms")
                                .arg(outcome.retryAfterMs).arg(took)));
        QVERIFY(link.ended());
        QCOMPARE(link.done().value(QStringLiteral("paired")).toBool(true), false);
    }

    void thePeerPairsByOneTap()
    {
        ProcessLink link;
        QVERIFY(link.start({}));
        Device device;
        DeviceSide side(device);
        const Outcome outcome = side.lan(link);
        QCOMPARE(outcome.type, QStringLiteral("pair.accept"));
        QCOMPARE(outcome.identity.value(QStringLiteral("publicKey")).toString(),
                 side.hello.value(QStringLiteral("identity")).toObject()
                     .value(QStringLiteral("publicKey")).toString());
        QVERIFY(link.ended());
        QCOMPARE(link.done().value(QStringLiteral("paired")).toBool(), true);
    }

    void thePeerRefusesOneTapWhenClaimedOrDenied()
    {
        for (const QStringList& arguments :
             {QStringList{QStringLiteral("--claimed")}, QStringList{QStringLiteral("--lan-deny")},
              QStringList{QStringLiteral("--address"), QStringLiteral("192.0.2.7")}}) {
            ProcessLink link;
            QVERIFY(link.start(arguments));
            Device device;
            DeviceSide side(device);
            verifyPlainRefusal(side.lan(link));
            QVERIFY(link.ended());
            QCOMPARE(link.done().value(QStringLiteral("paired")).toBool(true), false);
        }
    }

    void thePeerPairsByCodeWhenReopened()
    {
        ProcessLink link;
        QVERIFY(link.start({QStringLiteral("--claimed")}));
        const QString code = link.ready().value(QStringLiteral("code")).toString();
        QVERIFY(!code.isEmpty());
        Device device;
        DeviceSide side(device);
        QCOMPARE(side.code(link, code).type, QStringLiteral("pair.confirm"));
        QVERIFY(link.ended());
        QCOMPARE(link.done().value(QStringLiteral("devices")).toInt(), 2);
    }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    TstStationPairing test;
    QTEST_SET_MAIN_SOURCE_PATH
    // Load findings 3: about 13 s on a quiet machine, past ctest's 120 s
    // under a loaded full run (the limit is not raised). The wrong-code and
    // code-handling cases and the pairing peer's cases run as their own
    // ctest entries, tst_station_pairing_code and tst_station_pairing_peer
    // (tests/CMakeLists.txt); the rest as tst_station_pairing.
    const std::optional<QStringList> arguments = NereusSDR::TestFunctionGroups::arguments(
        test.metaObject(), app.arguments(), "NEREUS_STATION_PAIRING_GROUP",
        {{QStringLiteral("code"),
          {QStringLiteral("anExchangeInFlightDoesNotPairOnceTheWindowHasClosed"),
           QStringLiteral("aWrongCodeFailsOnTheDevicesSideAndBurns"),
           QStringLiteral("aWrongCodeFailsOnTheCoresSideAndTheWaitDoubles"),
           QStringLiteral("wrongCodesThroughTheServicePauseOnlyTheService"),
           QStringLiteral("aConnectionThroughTheServiceCannotPair"),
           QStringLiteral("leavingAfterTheCoreCommittedBurnsTheCode"),
           QStringLiteral("theCodeGoesToOneExchangeAtATime"),
           QStringLiteral("thePairingCodeNeverReachesTheLog"),
           QStringLiteral("theCodeIsHashedOffTheEventLoop")}},
         {QStringLiteral("peer"),
          {QStringLiteral("thePeerPairsByCode"),
           QStringLiteral("thePeerRefusesAWrongCode"),
           QStringLiteral("thePeerPairsByOneTap"),
           QStringLiteral("thePeerRefusesOneTapWhenClaimedOrDenied"),
           QStringLiteral("thePeerPairsByCodeWhenReopened")}}});
    return arguments ? QTest::qExec(&test, *arguments) : 1;
}

#include "tst_station_pairing.moc"
