// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_station_control_socket.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 17 (R-IOS-08; spec section 9): the Core's console
// socket and the commands it answers, against a Core (DaemonApp) started
// in the test.
//
// Invariants first, each with a red check recorded in task-17-report.md:
//
//   - the socket is owner-only: no group or other permission bit, so a
//     second account on the same computer cannot connect;
//   - its place comes from --config (state_directory) or --profile, never
//     from $HOME;
//   - `reset --unclaimed` without --yes changes nothing;
//   - `token retire` is refused while no device is paired;
//   - `reset --unclaimed --yes` removes every device (moving a damaged
//     list aside first), retires the token, ends every connection, opens
//     pairing unclaimed and prints the new code;
//   - the code never reaches a log line.
//
// Then each subcommand, over the socket; the real entry point finding the
// Core through --profile or --config; and the real nereusd binary finding
// it through --config with no usable $HOME. A Core with no command is
// started by the same code as before (tst_daemon_signals runs it).
//
// Everything listens on loopback or a local socket in a scratch
// directory. Keys are made at run time.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: Part C fix wave (R1-I1): the last device is not
//               revoked while no token is active. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (R1-I2): a reopened pairing window
//               lasts 10 minutes, five burned codes in a row close any window,
//               and reopening starts afresh. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (R2-M6): console commands report
//               an unreadable or invalid configuration file. J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic Claude
//               Code.
//   2026-09-26: a packaged Core found with no options, the no-answer
//               text, and a Core started by hand still found first
//               (R-IOS-08, R-R3-26). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: fix wave (INFRA-I4): every wait's result is checked. A
//               console request that outlives its deadline stops the test
//               with a named stage instead of a join that blocks to the
//               ctest timeout; the other waits say which stage did not
//               arrive. J.J. Boyd (KG4VCF), with AI-assisted implementation
//               via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QMutex>
#include <QProcess>
#include <QScopeGuard>
#include <QTcpServer>
#include <QTemporaryDir>

#include <atomic>
#include <memory>
#include <thread>

#ifdef Q_OS_UNIX
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cstring>

// The real entry point, run as a helper process below: a Core started the
// way nereusd starts, and console commands sent the way nereusd sends
// them, inside this test's settings sandbox.
#define main nereusdEntryPointForTest
#include "../src/server_main.cpp"
#undef main

#include "core/AppSettings.h"
#include "core/daemon/DaemonApp.h"
#include "core/daemon/DaemonConfig.h"
#include "core/daemon/StationControlCommands.h"
#include "core/daemon/StationControlSocket.h"
#include "core/security/CertificateStore.h"
#include "core/security/DeviceAuthenticator.h"
#include "core/security/DeviceStore.h"
#include "core/security/PairingWindow.h"
#include "core/security/StationIdentity.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationCapabilities.h"
#include "core/session/StationDevicesFacade.h"
#include "core/session/StationServer.h"
#include "core/station/StationRadios.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QWebSocket>

#include "OperatorWording.h"
#include "fakes/LoopbackTransport.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;
using NereusSDR::Test::LoopbackTransport;

namespace {

QMutex g_logMutex;
QStringList g_log;
QtMessageHandler g_previousHandler = nullptr;

void captureLog(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    {
        QMutexLocker lock(&g_logMutex);
        g_log.append(message);
    }
    if (g_previousHandler) {
        g_previousHandler(type, context, message);
    }
}

// A device key made at run time.
struct Device {
    QTemporaryDir dir;
    StationIdentity key = StationIdentity::loadOrCreate(dir.path());

    PairedDevice record(const QString& name = QStringLiteral("Shack iPhone")) const
    {
        PairedDevice device;
        device.id = key.fingerprint();
        device.publicKeySpki = key.publicKeySpki();
        device.name = name;
        device.kind = QStringLiteral("phone");
        return device;
    }

    QString id() const { return StationIdentity::toBase64Url(key.fingerprint()); }
};

QJsonObject firstOfType(const QList<QByteArray>& received, const QString& type)
{
    for (const QByteArray& wire : received) {
        const QJsonObject o = QJsonDocument::fromJson(wire).object();
        if (o.value(QStringLiteral("type")).toString() == type) {
            return o;
        }
    }
    return {};
}

int freeLoopbackPort()
{
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost, 0)) {
        return 0;
    }
    const int port = probe.serverPort();
    probe.close();
    return port;
}

// A console command from another thread, as a second nereusd process would
// send it, while this thread's event loop serves the Core.
StationControlReply ask(const QString& path, const QStringList& args)
{
    StationControlReply reply;
    std::atomic<bool> done{false};
    std::thread client([&]() {
        reply = StationControlSocket::request(path, args, 10000);
        done = true;
    });
    const bool waited = QTest::qWaitFor([&done]() { return done.load(); }, 15000);
    if (!waited) {
        // The request has its own 10 s deadline. Joining a thread that has
        // outlived it would block to the ctest timeout with no word of where.
        qFatal("ask(%s): the console request did not return within 15 s",
               qPrintable(args.join(QLatin1Char(' '))));
    }
    client.join();
    return reply;
}

// The same, looking through `paths` as a console command does.
StationControlReply ask(const QStringList& paths, const QStringList& args)
{
    StationControlReply reply;
    std::atomic<bool> done{false};
    std::thread client([&]() {
        reply = StationControlSocket::request(paths, args, 10000);
        done = true;
    });
    const bool waited = QTest::qWaitFor([&done]() { return done.load(); }, 15000);
    if (!waited) {
        // As above: never join a request that has outlived its own deadline.
        qFatal("ask(%s): the console request did not return within 15 s",
               qPrintable(args.join(QLatin1Char(' '))));
    }
    client.join();
    return reply;
}

// An app on the Core's real wss listener, speaking the link itself: the
// token, and a hello that declares the several-devices feature (so the
// confirm step's question and notices reach it).
struct RawApp {
    QWebSocket socket;
    QList<QJsonObject> received;

    RawApp(quint16 port, const QString& token)
        : m_token(token)
    {
        QObject::connect(&socket, &QWebSocket::sslErrors, &socket,
                         [this](const QList<QSslError>&) { socket.ignoreSslErrors(); });
        QObject::connect(&socket, &QWebSocket::textMessageReceived, &socket,
                         [this](const QString& text) {
            received.append(QJsonDocument::fromJson(text.toUtf8()).object());
        });
        socket.open(QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(port)));
    }

    bool signIn()
    {
        if (waitFor(QStringLiteral("hello")).isEmpty()) {
            return false;
        }
        send(SessionMessages::hello(kSessionProtocolMajor, kSessionProtocolMinor, 0,
                                    QStringLiteral("NereusSDR iPhone"), {kSessionProtocolMajor},
                                    {{"deviceAuth", 1}, {"sessionHolder", 1}}));
        send(SessionMessages::authRequest(m_token));
        return !waitFor(QStringLiteral("snapshot.complete")).isEmpty();
    }

    void send(const SessionMessage& message)
    {
        socket.sendTextMessage(QString::fromUtf8(SessionMessages::encode(message)));
    }

    void invoke(const QByteArray& verb, quint32 id, const QList<MirrorUpdate>& args)
    {
        send(SessionMessages::commandInvoke(verb, id, args));
    }

    // The first message of `type` (for a command.result, with `id`).
    QJsonObject waitFor(const QString& type, qint64 id = -1)
    {
        QJsonObject found;
        const bool arrived = QTest::qWaitFor([&]() {
            for (const QJsonObject& o : std::as_const(received)) {
                if (o.value(QStringLiteral("type")).toString() == type
                    && (id < 0 || o.value(QStringLiteral("id")).toInteger() == id)) {
                    found = o;
                    return true;
                }
            }
            return false;
        }, 10000);
        if (!arrived) {
            qWarning() << "RawApp::waitFor: no" << type << "within 10 s";
        }
        return found;
    }

    // Where the first message of `type` (with `id`) arrived; -1 if none.
    qsizetype indexOf(const QString& type, qint64 id = -1) const
    {
        for (qsizetype i = 0; i < received.size(); ++i) {
            const QJsonObject& o = received.at(i);
            if (o.value(QStringLiteral("type")).toString() == type
                && (id < 0 || o.value(QStringLiteral("id")).toInteger() == id)) {
                return i;
            }
        }
        return -1;
    }

    bool saw(const QString& text) const
    {
        for (const QJsonObject& o : received) {
            if (QJsonDocument(o).toJson().contains(text.toUtf8())) {
                return true;
            }
        }
        return false;
    }

private:
    QString m_token;
};

// One Core started in the test: the listener on loopback, no status page,
// its console socket in a scratch state directory.
struct Core {
    QTemporaryDir stateDir;
    DaemonConfig config;
    std::unique_ptr<DaemonApp> app;
    QString socketPath;
    QList<LoopbackTransport*> clients;

    explicit Core(bool remoteOn = true, bool upgradedWithToken = false,
                  const QByteArray& damagedDeviceList = {})
    {
        const QString security = CertificateStore::defaultDirectory();
        QDir(security).removeRecursively();
        if (upgradedWithToken) {
            NereusSDR::Test::seedUpgradedCoreToken(security);
        } else {
            NereusSDR::Test::seedCoreIdentity(security);
        }
        if (!damagedDeviceList.isEmpty()) {
            QFile file(QDir(security).filePath(QString::fromLatin1(DeviceStore::kFileName)));
            if (file.open(QIODevice::WriteOnly)) {
                file.write(damagedDeviceList);
            }
        }
        config = DaemonConfig::defaults();
        config.remotePort = remoteOn ? freeLoopbackPort() : 0;
        config.remoteBind = QStringLiteral("127.0.0.1");
        config.statusPage = false;
        config.stateDirectory = stateDir.path();
        app = std::make_unique<DaemonApp>();
        app->primeBoardForTest(HPSDRHW::HermesLite);
        socketPath = StationControlSocket::socketPathFor(config, AppSettings::profileOverride());
    }

    bool start()
    {
        return app->start(config) && app->startControlSocket(socketPath);
    }

    ~Core()
    {
        app->stop();
        app.reset();
        qDeleteAll(clients);
        QDir(CertificateStore::defaultDirectory()).removeRecursively();
    }

    StationServer* server() const { return app->stationServer(); }

    StationControlReply run(const QStringList& args) const { return ask(socketPath, args); }

    QByteArray certSha256() const
    {
        QString pin = server()->certificateFingerprint();
        pin.remove(QLatin1Char(':'));
        return QByteArray::fromHex(pin.toLatin1());
    }

    LoopbackTransport* open()
    {
        auto* client = new LoopbackTransport(QStringLiteral("app"));
        auto* station = new LoopbackTransport(QStringLiteral("station"));
        station->setPeerAddress(QStringLiteral("192.0.2.7"));
        station->linkTo(client);
        clients.append(client);
        server()->acceptTransport(station);
        if (!QTest::qWaitFor([client]() { return !client->received().isEmpty(); }, 5000)) {
            qWarning() << "open: the station sent nothing within 5 s";
        }
        return client;
    }

    static bool signIn(LoopbackTransport* client, const SessionMessage& auth)
    {
        client->sendText(SessionMessages::encode(SessionMessages::hello(
            kSessionProtocolMajor, kSessionProtocolMinor, 0, QStringLiteral("NereusSDR iPhone"),
            {kSessionProtocolMajor}, {{"deviceAuth", 1}})));
        client->sendText(SessionMessages::encode(auth));
        if (!QTest::qWaitFor(
                [client]() {
                    return client->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"))
                        || !client->isOpen();
                },
                5000)) {
            qWarning() << "signIn: no snapshot.complete and no close within 5 s"
                       << client->receivedKinds();
        }
        return client->isOpen()
               && client->receivedKinds().contains(QByteArrayLiteral("snapshot.complete"));
    }

    LoopbackTransport* deviceSession(const Device& device)
    {
        LoopbackTransport* client = open();
        const QJsonObject hello = firstOfType(client->received(), QStringLiteral("hello"));
        const QByteArray challenge =
            StationIdentity::fromBase64Url(hello.value(QStringLiteral("challenge")).toString());
        const QByteArray stationSpki = server()->stationIdentity().publicKeySpki();
        const SessionDeviceBlock block{
            device.id(), StationIdentity::toBase64Url(device.key.publicKeySpki()),
            QStringLiteral("Shack iPhone"), QStringLiteral("phone"),
            StationIdentity::toBase64Url(device.key.sign(DeviceAuthenticator::transcript(
                challenge, certSha256(), stationSpki, device.key.publicKeySpki())))};
        return signIn(client, SessionMessages::authRequest(QString(), block)) ? client : nullptr;
    }

    LoopbackTransport* tokenSession()
    {
        LoopbackTransport* client = open();
        return signIn(client, SessionMessages::authRequest(server()->token())) ? client : nullptr;
    }
};

bool endedWithCode(LoopbackTransport* client, const QString& code)
{
    if (!QTest::qWaitFor([client]() { return !client->isOpen(); }, 5000)) {
        qWarning() << "endedWithCode: the session was still open after 5 s";
    }
    const QJsonObject end = firstOfType(client->received(), QStringLiteral("session.end"));
    if (end.value(QStringLiteral("code")).toString() != code) {
        qWarning() << "ended with" << end << "open" << client->isOpen() << client->receivedKinds();
    }
    return !client->isOpen() && end.value(QStringLiteral("code")).toString() == code;
}

// The reply's words, without what is not words: the pairing code (random
// words) and device ids (base64url) are data, not wording.
// The real entry point as a helper process: this test binary, re-run with
// --daemon-helper; `--packaged-home <dir>` first stands a directory in for
// a packaged Core's HOME (StationControlSocket::setPackagedHomesForTest).
int runHelper(const QStringList& args, QString* out)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QCoreApplication::applicationFilePath(),
                  QStringList{QStringLiteral("--daemon-helper")} + args);
    const bool finished = QTest::qWaitFor(
        [&process]() { return process.state() == QProcess::NotRunning; }, 30000);
    *out = QString::fromUtf8(process.readAll());
    if (!finished) {
        qWarning() << "runHelper" << args << "did not exit within 30 s; output:" << *out;
        return -2;
    }
    return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
}

void verifyPlain(const StationControlReply& reply, const QString& code = QString())
{
    QStringList lines = reply.text.split(QLatin1Char('\n'));
    for (QString& line : lines) {
        if (line.startsWith(QStringLiteral("Pairing code: "))
            || line.startsWith(QStringLiteral("  id "))) {
            line = QStringLiteral("Pairing code");
        }
        if (!code.isEmpty()) {
            line.remove(code);
        }
    }
    const QString text = lines.join(QLatin1Char('\n'));
    QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
    QVERIFY2(OperatorWording::coreCalledStationIn(text).isEmpty(), qPrintable(text));
}

} // namespace

class TstStationControlSocket : public QObject {
    Q_OBJECT

    QTemporaryDir m_home{QStringLiteral("/tmp/tcs-XXXXXX")};

private slots:
    void initTestCase()
    {
        // A short home: a local socket's name has a short limit, and
        // ctest's own test home is deep. The settings sandbox, and so every
        // profile's directory, lives under it (on macOS and Linux alike),
        // and the helper processes inherit it. Set before anything resolves
        // a settings path.
        QVERIFY(m_home.isValid());
        qputenv("HOME", QFile::encodeName(m_home.path()));
        // macOS resolves the home through CoreFoundation, which ctest points
        // with CFFIXED_USER_HOME (tests/CMakeLists.txt).
        qputenv("CFFIXED_USER_HOME", QFile::encodeName(m_home.path()));
        qunsetenv("XDG_CONFIG_HOME");
        qunsetenv("XDG_DATA_HOME");
        qunsetenv("XDG_CACHE_HOME");
        // The Core's identity and devices live in the profile's directory:
        // one of this test's own, emptied before and after each Core.
        AppSettings::setProfileOverride(
            QStringLiteral("tcs-%1").arg(QCoreApplication::applicationPid()));
    }

    void cleanupTestCase()
    {
        QDir(AppSettings::resolveConfigDir(AppSettings::profileOverride())).removeRecursively();
    }

    // ── Invariants ────────────────────────────────────────────────────

    void theSocketIsOwnerOnly()
    {
        Core core;
        QVERIFY(core.start());
        const QFileInfo info(core.socketPath);
        QVERIFY2(info.exists(), qPrintable(core.socketPath));
        const QFileDevice::Permissions others =
            QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
            | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
        QVERIFY2((info.permissions() & others) == 0,
                 "another account on this computer could open the console socket");
#ifdef Q_OS_UNIX
        struct stat st {};
        QVERIFY(::stat(QFile::encodeName(core.socketPath).constData(), &st) == 0);
        QVERIFY(S_ISSOCK(st.st_mode));
        QCOMPARE(st.st_mode & 077, mode_t(0));
        QCOMPARE(st.st_uid, ::getuid());
#endif
        QVERIFY(core.run({QStringLiteral("status")}).ok);
    }

    void theSocketIsFoundFromConfigOrProfileNeverHome()
    {
        DaemonConfig config = DaemonConfig::defaults();
        config.stateDirectory = QStringLiteral("/var/lib/nereusd");
        const QByteArray home = qgetenv("HOME");
        qputenv("HOME", QByteArrayLiteral("/nonexistent-home-for-this-test"));
        const QString fromConfig = StationControlSocket::socketPathFor(config, QStringLiteral("x"));
        qputenv("HOME", home);
        QCOMPARE(fromConfig, QStringLiteral("/var/lib/nereusd/nereusd-control"));
        QCOMPARE(StationControlSocket::socketPathFor(config, QStringLiteral("other")), fromConfig);

        config.stateDirectory.clear();
        QCOMPARE(StationControlSocket::socketPathFor(config, QStringLiteral("bench")),
                 AppSettings::resolveConfigDir(QStringLiteral("bench"))
                     + QStringLiteral("/nereusd-control"));

        // A Core found through its profile alone.
        Core core;
        core.config.stateDirectory.clear();
        core.socketPath = StationControlSocket::socketPathFor(core.config,
                                                              AppSettings::profileOverride());
        QCOMPARE(core.socketPath, AppSettings::resolveConfigDir(AppSettings::profileOverride())
                                      + QStringLiteral("/nereusd-control"));
        QVERIFY(core.start());
        QVERIFY(core.run({QStringLiteral("status")}).ok);
    }

    void aSecondCoreDoesNotTakeALiveSocket()
    {
        Core core;
        QVERIFY(core.start());
        StationControlSocket second([](const QStringList&) {
            return StationControlReply{true, QStringLiteral("the wrong Core")};
        });
        QVERIFY(!second.listen(core.socketPath));
        QVERIFY(core.run({QStringLiteral("status")}).text.startsWith(QStringLiteral("This Core")));
    }

    void resetWithoutYesChangesNothing()
    {
        Core core(/*remoteOn=*/true, /*upgradedWithToken=*/true);
        QVERIFY(core.start());
        Device device;
        QVERIFY(core.server()->deviceStore()->add(device.record()));
        LoopbackTransport* session = core.deviceSession(device);
        QVERIFY(session != nullptr);
        const StationControlReply reply =
            core.run({QStringLiteral("reset"), QStringLiteral("--unclaimed")});
        QVERIFY(!reply.ok);
        QVERIFY(reply.text.contains(QStringLiteral("Nothing was changed")));
        verifyPlain(reply);
        QCOMPARE(core.server()->deviceStore()->list().size(), 1);
        QVERIFY(core.server()->devicesFacade()->tokenActive());
        QCOMPARE(core.server()->pairingWindow()->state(), PairingWindow::State::ClosedClaimed);
        QVERIFY(session->isOpen());
        // Nor does a reset without --unclaimed.
        QVERIFY(!core.run({QStringLiteral("reset"), QStringLiteral("--yes")}).ok);
        QCOMPARE(core.server()->deviceStore()->list().size(), 1);
    }

    void tokenRetireIsRefusedUntilADeviceIsPaired()
    {
        Core core(/*remoteOn=*/true, /*upgradedWithToken=*/true);
        QVERIFY(core.start());
        const QStringList retire{QStringLiteral("token"), QStringLiteral("retire")};
        StationControlReply reply = core.run(retire);
        QVERIFY(!reply.ok);
        QCOMPARE(reply.text, QStringLiteral("Pair a device with this Core first, so a device can "
                                            "still sign in once the pairing token stops working."));
        QVERIFY(core.server()->devicesFacade()->tokenActive());

        LoopbackTransport* tokenWindow = core.tokenSession();
        QVERIFY(tokenWindow != nullptr);
        Device device;
        QVERIFY(core.server()->deviceStore()->add(device.record()));
        reply = core.run(retire);
        QVERIFY2(reply.ok, qPrintable(reply.text));
        verifyPlain(reply);
        QVERIFY(!core.server()->devicesFacade()->tokenActive());
        QVERIFY(endedWithCode(tokenWindow, QStringLiteral("pairingRequired")));
        // Again: nothing to retire.
        reply = core.run(retire);
        QVERIFY(reply.ok);
        QCOMPARE(reply.text, QStringLiteral("This Core has no pairing token to retire."));
    }

    void resetWithYesEndsEverythingAndOpensUnclaimed()
    {
        const QStringList reset{QStringLiteral("reset"), QStringLiteral("--unclaimed"),
                                QStringLiteral("--yes")};
        // One connection at a time: a device's, then (on a second Core) one
        // signed in with the token.
        {
            Core core(/*remoteOn=*/true, /*upgradedWithToken=*/true);
            QVERIFY(core.start());
            Device first;
            Device second;
            QVERIFY(core.server()->deviceStore()->add(first.record(QStringLiteral("Shack iPhone"))));
            QVERIFY(core.server()->deviceStore()->add(second.record(QStringLiteral("Shack iPad"))));
            LoopbackTransport* deviceWindow = core.deviceSession(first);
            QVERIFY(deviceWindow != nullptr);
            QCOMPARE(core.server()->pairingWindow()->state(), PairingWindow::State::ClosedClaimed);

            const StationControlReply reply = core.run(reset);
            QVERIFY2(reply.ok, qPrintable(reply.text));
            verifyPlain(reply, core.server()->pairingWindow()->currentCode());
            QVERIFY(core.server()->deviceStore()->list().isEmpty());
            QVERIFY(!core.server()->devicesFacade()->tokenActive());
            QVERIFY(!core.server()->deviceStore()->isClaimed());
            QCOMPARE(core.server()->pairingWindow()->state(), PairingWindow::State::OpenUnclaimed);
            const QString code = core.server()->pairingWindow()->currentCode();
            QVERIFY(!code.isEmpty());
            QVERIFY2(reply.text.contains(code), "the reset did not print the new code");
            QVERIFY(endedWithCode(deviceWindow, QStringLiteral("deviceRemoved")));
        }
        {
            Core core(/*remoteOn=*/true, /*upgradedWithToken=*/true);
            QVERIFY(core.start());
            LoopbackTransport* tokenWindow = core.tokenSession();
            QVERIFY(tokenWindow != nullptr);
            const StationControlReply reply = core.run(reset);
            QVERIFY2(reply.ok, qPrintable(reply.text));
            QVERIFY(!core.server()->devicesFacade()->tokenActive());
            QCOMPARE(core.server()->pairingWindow()->state(), PairingWindow::State::OpenUnclaimed);
            QVERIFY(endedWithCode(tokenWindow, QStringLiteral("pairingRequired")));
        }
    }

    void theConsoleReopensAWindowTheAttemptCeilingClosed()
    {
        // Fix wave R1-I2: five burned codes in a row shut an unclaimed
        // Core's window; only its console opens it again, by pairing open
        // or by a reset.
        Core core;
        QVERIFY(core.start());
        PairingWindow* window = core.server()->pairingWindow();
        qint64 clock = 1000000;
        window->setClock([&clock] { return clock; });
        const auto burnFive = [window, &clock] {
            for (int i = 0; i < PairingWindow::kMaxConsecutiveFailures; ++i) {
                clock += window->retryAfterMs();
                window->poll();
                QVERIFY(window->takeCode(window->codeSerial()));
                window->pairingFailed();
            }
        };
        burnFive();
        QCOMPARE(window->state(), PairingWindow::State::ClosedUnclaimed);

        StationControlReply reply = core.run({QStringLiteral("status")});
        QVERIFY(reply.text.contains(QStringLiteral("Pairing: closed after too many wrong")));
        reply = core.run({QStringLiteral("pairing"), QStringLiteral("show")});
        QVERIFY(reply.ok);
        verifyPlain(reply);
        QVERIFY(reply.text.contains(QStringLiteral("Pairing closed after too many wrong "
                                                   "pairing codes.")));
        QVERIFY(!reply.text.contains(QStringLiteral("Pairing code:")));
        reply = core.run({QStringLiteral("pairing"), QStringLiteral("close")});
        QVERIFY(reply.ok);
        QCOMPARE(reply.text, QStringLiteral("Pairing is already closed."));

        reply = core.run({QStringLiteral("pairing"), QStringLiteral("open")});
        QVERIFY2(reply.ok, qPrintable(reply.text));
        verifyPlain(reply);
        QCOMPARE(window->state(), PairingWindow::State::OpenUnclaimed);
        QVERIFY(!window->currentCode().isEmpty());
        QVERIFY(reply.text.contains(window->currentCode()));
        QCOMPARE(window->consecutiveFailures(), 0);

        // A reset opens it too.
        burnFive();
        QCOMPARE(window->state(), PairingWindow::State::ClosedUnclaimed);
        reply = core.run(
            {QStringLiteral("reset"), QStringLiteral("--unclaimed"), QStringLiteral("--yes")});
        QVERIFY2(reply.ok, qPrintable(reply.text));
        QCOMPARE(window->state(), PairingWindow::State::OpenUnclaimed);
        QVERIFY(!window->currentCode().isEmpty());
    }

    void resetMovesADamagedDeviceListAside()
    {
        const QByteArray damaged("{ this is not a device list");
        Core core(/*remoteOn=*/true, /*upgradedWithToken=*/false, damaged);
        QVERIFY(core.start());
        DeviceStore* store = core.server()->deviceStore();
        QVERIFY(!store->isValid());
        QVERIFY(store->isClaimed());
        StationControlReply reply = core.run({QStringLiteral("devices")});
        QVERIFY(!reply.ok);
        verifyPlain(reply);

        reply = core.run(
            {QStringLiteral("reset"), QStringLiteral("--unclaimed"), QStringLiteral("--yes")});
        QVERIFY2(reply.ok, qPrintable(reply.text));
        QVERIFY(store->isValid());
        QVERIFY(!store->isClaimed());
        QCOMPARE(core.server()->pairingWindow()->state(), PairingWindow::State::OpenUnclaimed);
        QVERIFY(reply.text.contains(core.server()->pairingWindow()->currentCode()));
        const QFileInfo list(store->filePath());
        const QStringList aside = list.dir().entryList(
            {QString::fromLatin1(DeviceStore::kFileName) + QStringLiteral(".damaged-*")},
            QDir::Files);
        QCOMPARE(aside.size(), 1);
        QFile kept(list.dir().filePath(aside.first()));
        QVERIFY(kept.open(QIODevice::ReadOnly));
        QCOMPARE(kept.readAll(), damaged);
    }

    void theCodeNeverReachesALogLine()
    {
        {
            QMutexLocker lock(&g_logMutex);
            g_log.clear();
        }
        g_previousHandler = qInstallMessageHandler(captureLog);
        QStringList codes;
        {
            Core core;
            if (core.start()) {
                codes << core.server()->pairingWindow()->currentCode();
                core.run({QStringLiteral("pairing"), QStringLiteral("show")});
                Device device;
                core.server()->deviceStore()->add(device.record());
                core.run({QStringLiteral("pairing"), QStringLiteral("open")});
                codes << core.server()->pairingWindow()->currentCode();
                core.run({QStringLiteral("reset"), QStringLiteral("--unclaimed"),
                          QStringLiteral("--yes")});
                codes << core.server()->pairingWindow()->currentCode();
            }
        }
        qInstallMessageHandler(g_previousHandler);
        codes.removeAll(QString());
        QCOMPARE(codes.size(), 3);
        QMutexLocker lock(&g_logMutex);
        QVERIFY(!g_log.isEmpty());
        for (const QString& line : std::as_const(g_log)) {
            for (const QString& code : std::as_const(codes)) {
                QVERIFY2(!line.contains(code), "a log line holds a pairing code");
                QVERIFY2(!line.contains(code.section(QLatin1Char('-'), 1)),
                         "a log line holds a pairing code's words");
            }
        }
    }

    // ── Each command ──────────────────────────────────────────────────

    // Follow-up N3 (ruling (a)): the radio goes on the air between the
    // chooser's Confirm and the restart turn. The change is dropped: the
    // chooser's proceed is answered refused with the Core's on-air reason,
    // the other app is told nothing, nobody's session ends and the Core
    // keeps its run.
    void aRadioChangeDroppedOnTheAirIsRefusedAndNobodyIsTold()
    {
        Core core(/*remoteOn=*/true, /*upgradedWithToken=*/true);
        QVERIFY(core.start());
        StationRadios* const radios = core.app->stationRadios();
        QVERIFY(radios);
        const auto radio = [](const QString& mac, HPSDRHW board, const QString& name) {
            RadioInfo info;
            info.macAddress = mac;
            info.boardType = board;
            info.name = name;
            info.address = QHostAddress(QStringLiteral("192.0.2.30"));
            return info;
        };
        const RadioInfo hl2 = radio(QStringLiteral("AA:BB:CC:00:21:01"), HPSDRHW::HermesLite,
                                    QStringLiteral("Bench HL2"));
        const RadioInfo g2 = radio(QStringLiteral("AA:BB:CC:00:21:02"), HPSDRHW::Saturn,
                                   QStringLiteral("Bench G2"));
        radios->setVisible({hl2, g2});
        radios->setCurrent(hl2);
        StationServer* const before = core.server();
        const QPointer<StationServer> server(before);
        before->setTokenSessionsMayChangeRadioForTest(true);
        RadioModel* const model = core.app->radioModelForTest();
        QVERIFY(model);
        // The radio keys (a hardware PTT) in the same turn the change is
        // accepted, before the restart turn runs.
        const auto accept = radios->onSelect;
        radios->onSelect = [accept, model](const QString& mac) {
            accept(mac);
            model->transmitModel().setTune(true);
        };

        RawApp chooser(before->serverPort(), before->token());
        RawApp other(before->serverPort(), before->token());
        QVERIFY2(chooser.signIn(), "the chooser did not sign in");
        QVERIFY2(other.signIn(), "the other app did not sign in");

        chooser.invoke("station.selectRadio", 41,
                       {MirrorUpdate{0, "mac", MirrorWireKind::Utf8, g2.macAddress}});
        QVERIFY(!chooser.waitFor(QStringLiteral("command.result"), 41).isEmpty());
        const QJsonObject asked = chooser.waitFor(QStringLiteral("confirm.request"));
        QVERIFY2(!asked.isEmpty(), "the chooser was not asked");
        chooser.invoke("confirm.proceed", 42,
                       {MirrorUpdate{0, "id", MirrorWireKind::Int64,
                                     asked.value(QStringLiteral("id")).toInteger()},
                        MirrorUpdate{1, "choice", MirrorWireKind::Int64, qlonglong(-1)}});
        const QJsonObject answer = chooser.waitFor(QStringLiteral("command.result"), 42);
        QVERIFY2(!answer.isEmpty(), "the proceed was never answered");
        QVERIFY2(!answer.value(QStringLiteral("accepted")).toBool(true),
                 "a change dropped on the air was answered accepted");
        QCOMPARE(answer.value(QStringLiteral("reason")).toString(), RadioModel::onAirReason());

        // Nobody is told the radio changed, and nobody's session ends.
        QTest::qWait(300);
        QCOMPARE(other.indexOf(QStringLiteral("notice")), qsizetype(-1));
        for (RawApp* app : {&chooser, &other}) {
            QCOMPARE(app->indexOf(QStringLiteral("session.end")), qsizetype(-1));
        }
        QVERIFY(!server.isNull());
        QCOMPARE(core.server(), before);
        QVERIFY(radios->pendingChoice().isEmpty());
        QVERIFY(!radios->switching());
        model->transmitModel().setTune(false);
    }

    // LINK-I4 fix round 1: pairing through the remote access service, shut
    // after 20 wrong codes, stays shut when a radio change restarts the
    // Core's run (a new station server and pairing window). Only a
    // reopening at the Core turns it back on.
    void aShutServiceStaysShutAcrossARadioChange()
    {
        const auto clearKept = [] {
            AppSettings& settings = AppSettings::instance();
            settings.remove(QLatin1String(PairingWindow::kServiceFailuresTotalKey));
            settings.remove(QLatin1String(PairingWindow::kServiceShutKey));
            settings.save();
        };
        clearKept();
        const auto cleanup = qScopeGuard(clearKept);
        Core core(/*remoteOn=*/true, /*upgradedWithToken=*/true);
        QVERIFY(core.start());
        StationRadios* const radios = core.app->stationRadios();
        QVERIFY(radios);
        const auto radio = [](const QString& mac, HPSDRHW board, const QString& name) {
            RadioInfo info;
            info.macAddress = mac;
            info.boardType = board;
            info.name = name;
            info.address = QHostAddress(QStringLiteral("192.0.2.30"));
            return info;
        };
        const RadioInfo hl2 = radio(QStringLiteral("AA:BB:CC:00:21:01"), HPSDRHW::HermesLite,
                                    QStringLiteral("Bench HL2"));
        const RadioInfo g2 = radio(QStringLiteral("AA:BB:CC:00:21:02"), HPSDRHW::Saturn,
                                   QStringLiteral("Bench G2"));
        radios->setVisible({hl2, g2});
        radios->setCurrent(hl2);

        // Shut it: 20 codes burned through the service, on a test clock so
        // the pauses pass at once.
        PairingWindow* window = core.server()->pairingWindow();
        qint64 clock = 5000000;
        window->setClock([&clock] { return clock; });
        int burned = 0;
        while (burned < PairingWindow::kMaxServiceFailuresTotal) {
            if (!window->isOpen()) {
                window->reopen();
            } else if (window->isPaused(PairingWindow::Route::Service)) {
                clock += window->servicePauseRemainingMs();
                window->poll();
            } else if (window->currentCode().isEmpty()) {
                clock += window->retryAfterMs();
                window->poll();
            } else {
                QVERIFY(window->takeCode(window->codeSerial()));
                window->pairingFailed(PairingWindow::Route::Service);
                ++burned;
            }
        }
        QVERIFY(window->isServiceShut());

        // The radio change restarts the run: a new server, a new window.
        const QPointer<StationServer> oldServer(core.server());
        radios->onSelect(g2.macAddress);
        QTRY_VERIFY(oldServer.isNull());
        QTRY_VERIFY(core.server() != nullptr && core.server()->isListening());
        PairingWindow* const after = core.server()->pairingWindow();
        QVERIFY(after);
        QVERIFY2(after->isServiceShut(), "a radio change turned service pairing back on");
        QVERIFY(core.server()->devicesFacade()->servicePairingShut());

        // The Core's own console turns it back on.
        const StationControlReply open =
            core.run({QStringLiteral("pairing"), QStringLiteral("open")});
        QVERIFY2(open.ok, qPrintable(open.text));
        QVERIFY(!core.server()->pairingWindow()->isServiceShut());
        QVERIFY(!AppSettings::instance().contains(
            QLatin1String(PairingWindow::kServiceShutKey)));
    }

    // The operator's ruling of 2026-09-26 (fix wave after parity Tasks 19
    // and 21, I1 and I2): a radio change restarts the Core's run. Over the
    // Core's real listener, the chooser's answer, the confirm step's
    // notice to the other device and each app's end ("The Core is switching
    // to Bench G2. This app reconnects by itself.", retryable, code
    // radioChanging) all arrive, and the console commands answer after it.
    void aRadioChangeTellsEveryAppAndKeepsTheConsole()
    {
        Core core(/*remoteOn=*/true, /*upgradedWithToken=*/true);
        QVERIFY(core.start());
        StationRadios* const radios = core.app->stationRadios();
        QVERIFY(radios);
        const auto radio = [](const QString& mac, HPSDRHW board, const QString& name) {
            RadioInfo info;
            info.macAddress = mac;
            info.boardType = board;
            info.name = name;
            info.address = QHostAddress(QStringLiteral("192.0.2.30"));
            return info;
        };
        const RadioInfo hl2 = radio(QStringLiteral("AA:BB:CC:00:21:01"), HPSDRHW::HermesLite,
                                    QStringLiteral("Bench HL2"));
        const RadioInfo g2 = radio(QStringLiteral("AA:BB:CC:00:21:02"), HPSDRHW::Saturn,
                                   QStringLiteral("Bench G2"));
        radios->setVisible({hl2, g2});
        radios->setCurrent(hl2);
        // Both apps sign in with the token (no key here); this lets them
        // change the radio (StationServer's I5 seam).
        StationServer* const before = core.server();
        const QPointer<StationServer> oldServer(before);
        before->setTokenSessionsMayChangeRadioForTest(true);

        RawApp chooser(before->serverPort(), before->token());
        RawApp other(before->serverPort(), before->token());
        QVERIFY2(chooser.signIn(), "the chooser did not sign in");
        QVERIFY2(other.signIn(), "the other app did not sign in");

        chooser.invoke("station.selectRadio", 41,
                       {MirrorUpdate{0, "mac", MirrorWireKind::Utf8, g2.macAddress}});
        QJsonObject answer = chooser.waitFor(QStringLiteral("command.result"), 41);
        // With another app listening, the chooser is asked first.
        QVERIFY2(!answer.value(QStringLiteral("accepted")).toBool(true),
                 "the chooser was not asked first");
        {
            const QJsonObject asked = chooser.waitFor(QStringLiteral("confirm.request"));
            QVERIFY2(!asked.isEmpty(), "no answer and no question for the radio change");
            chooser.invoke("confirm.proceed", 42,
                           {MirrorUpdate{0, "id", MirrorWireKind::Int64,
                                         asked.value(QStringLiteral("id")).toInteger()},
                            MirrorUpdate{1, "choice", MirrorWireKind::Int64, qlonglong(-1)}});
            answer = chooser.waitFor(QStringLiteral("command.result"), 42);
            const QJsonObject notice = other.waitFor(QStringLiteral("notice"));
            QVERIFY2(!notice.isEmpty(), "the other app was not told");
            QCOMPARE(notice.value(QStringLiteral("change")).toObject()
                         .value(QStringLiteral("to")).toString(),
                     QStringLiteral("Bench G2"));
        }
        QVERIFY2(answer.value(QStringLiteral("accepted")).toBool(),
                 qPrintable(answer.value(QStringLiteral("reason")).toString()));

        const QString reason = DaemonApp::radioChangeReason(QStringLiteral("Bench G2"));
        QCOMPARE(reason, QStringLiteral("The Core is switching to Bench G2. This app "
                                        "reconnects by itself."));
        QVERIFY2(OperatorWording::isPlain(reason), qPrintable(reason));
        for (RawApp* app : {&chooser, &other}) {
            const QJsonObject end = app->waitFor(QStringLiteral("session.end"));
            QCOMPARE(end.value(QStringLiteral("reason")).toString(), reason);
            QVERIFY(end.value(QStringLiteral("retryable")).toBool());
            QCOMPARE(end.value(QStringLiteral("code")).toString(),
                     QStringLiteral("radioChanging"));
            QVERIFY(!app->saw(QStringLiteral("The Core is shutting down.")));
        }
        // Follow-up N3: the answer and the notice are held for the restart
        // turn, and still reach the wire before each end.
        QVERIFY(chooser.indexOf(QStringLiteral("command.result"), 42)
                < chooser.indexOf(QStringLiteral("session.end")));
        QVERIFY(other.indexOf(QStringLiteral("notice"))
                < other.indexOf(QStringLiteral("session.end")));
        QVERIFY(other.indexOf(QStringLiteral("notice")) >= 0);

        // The run restarted on the new choice: a new station server, and the
        // console still answers.
        QTRY_VERIFY(oldServer.isNull());
        QTRY_VERIFY(core.server() != nullptr && core.server()->isListening());
        QCOMPARE(radios->pendingChoice(), g2.macAddress);
        const StationControlReply status = core.run({QStringLiteral("status")});
        QVERIFY2(status.ok, qPrintable(status.text));
        const StationControlReply show =
            core.run({QStringLiteral("pairing"), QStringLiteral("show")});
        QVERIFY2(show.ok, qPrintable(show.text));
    }

    void statusSaysWhatTheCoreIs()
    {
        Core core;
        QVERIFY(core.start());
        const StationControlReply reply = core.run({QStringLiteral("status")});
        QVERIFY2(reply.ok, qPrintable(reply.text));
        verifyPlain(reply);
        QVERIFY(reply.text.contains(QStringLiteral("This Core: ")));
        QVERIFY(reply.text.contains(QStringLiteral("Radio: ")));
        QVERIFY(reply.text.contains(QStringLiteral("Remote access: on")));
        QVERIFY(reply.text.contains(QStringLiteral("Paired devices: 0")));
        QVERIFY(reply.text.contains(QStringLiteral("Pairing: open until the first device")));
        QVERIFY(reply.text.contains(QStringLiteral("Status page: off")));
        // The code is for pairing show, not status.
        QVERIFY(!reply.text.contains(core.server()->pairingWindow()->currentCode()));
    }

    void pairingShowOpenAndClose()
    {
        Core core;
        QVERIFY(core.start());
        const QStringList show{QStringLiteral("pairing"), QStringLiteral("show")};
        StationControlReply reply = core.run(show);
        QVERIFY(reply.ok);
        verifyPlain(reply);
        QVERIFY(reply.text.contains(core.server()->pairingWindow()->currentCode()));

        // An unclaimed Core stays open.
        reply = core.run({QStringLiteral("pairing"), QStringLiteral("close")});
        QVERIFY(!reply.ok);
        verifyPlain(reply);
        QCOMPARE(core.server()->pairingWindow()->state(), PairingWindow::State::OpenUnclaimed);

        Device device;
        QVERIFY(core.server()->deviceStore()->add(device.record()));
        reply = core.run(show);
        QVERIFY(reply.ok);
        QVERIFY(reply.text.contains(QStringLiteral("Pairing is closed.")));

        reply = core.run({QStringLiteral("pairing"), QStringLiteral("open")});
        QVERIFY2(reply.ok, qPrintable(reply.text));
        verifyPlain(reply);
        QCOMPARE(core.server()->pairingWindow()->state(), PairingWindow::State::OpenReopened);
        const QString code = core.server()->pairingWindow()->currentCode();
        QVERIFY(!code.isEmpty());
        QVERIFY(reply.text.contains(code));
        QVERIFY(reply.text.contains(QStringLiteral("one more device")));

        reply = core.run({QStringLiteral("pairing"), QStringLiteral("close")});
        QVERIFY(reply.ok);
        QCOMPARE(core.server()->pairingWindow()->state(), PairingWindow::State::ClosedClaimed);
    }

    void devicesListsAndRevokes()
    {
        Core core;
        QVERIFY(core.start());
        StationControlReply reply = core.run({QStringLiteral("devices")});
        QVERIFY(reply.ok);
        QCOMPARE(reply.text, QStringLiteral("No device has paired with this Core."));

        Device device;
        QVERIFY(core.server()->deviceStore()->add(device.record()));
        LoopbackTransport* session = core.deviceSession(device);
        QVERIFY(session != nullptr);
        reply = core.run({QStringLiteral("devices")});
        QVERIFY(reply.ok);
        verifyPlain(reply);
        QVERIFY(reply.text.contains(QStringLiteral("Shack iPhone (phone)")));
        QVERIFY(reply.text.contains(device.id()));
        QVERIFY(reply.text.contains(QStringLiteral("connected now")));

        reply = core.run({QStringLiteral("devices"), QStringLiteral("revoke"),
                          QStringLiteral("not-a-device")});
        QVERIFY(!reply.ok);
        QCOMPARE(core.server()->deviceStore()->list().size(), 1);

        // The last device is not removed while no token is active: the
        // console's reset is how a Core becomes unclaimed (fix wave R1-I1).
        reply = core.run({QStringLiteral("devices"), QStringLiteral("revoke"), device.id()});
        QVERIFY(!reply.ok);
        verifyPlain(reply);
        QCOMPARE(reply.text, QStringLiteral("Pair another device first, or reset this Core from "
                                            "its own computer."));
        QCOMPARE(core.server()->deviceStore()->list().size(), 1);

        Device other;
        QVERIFY(core.server()->deviceStore()->add(other.record(QStringLiteral("Shack iPad"))));
        reply = core.run({QStringLiteral("devices"), QStringLiteral("revoke"), device.id()});
        QVERIFY2(reply.ok, qPrintable(reply.text));
        verifyPlain(reply);
        QVERIFY(reply.text.contains(QStringLiteral("Shack iPhone was removed")));
        QCOMPARE(core.server()->deviceStore()->list().size(), 1);
        QVERIFY(endedWithCode(session, QStringLiteral("deviceRemoved")));
    }

    void remoteAccessOffSaysSoAndChangesNothing()
    {
        Core core(/*remoteOn=*/false);
        QVERIFY(core.start());
        QVERIFY(core.server() == nullptr);
        StationControlReply reply = core.run({QStringLiteral("status")});
        QVERIFY(reply.ok);
        QVERIFY(reply.text.contains(QStringLiteral("Remote access: off")));
        for (const QStringList& args :
             {QStringList{QStringLiteral("pairing"), QStringLiteral("show")},
              QStringList{QStringLiteral("pairing"), QStringLiteral("open")},
              QStringList{QStringLiteral("devices")},
              QStringList{QStringLiteral("token"), QStringLiteral("retire")},
              QStringList{QStringLiteral("reset"), QStringLiteral("--unclaimed"),
                          QStringLiteral("--yes")}}) {
            reply = core.run(args);
            QVERIFY2(!reply.ok, qPrintable(args.join(QLatin1Char(' '))));
            QVERIFY(reply.text.startsWith(QStringLiteral("Remote access is off on this Core")));
            verifyPlain(reply);
        }
    }

    void anUnknownCommandListsTheCommands()
    {
        Core core;
        QVERIFY(core.start());
        const StationControlReply reply = core.run({QStringLiteral("pairing"),
                                                    QStringLiteral("sideways")});
        QVERIFY(!reply.ok);
        QVERIFY(reply.text.contains(QStringLiteral("nereusd reset --unclaimed --yes")));
        verifyPlain(reply);
    }

    void noCoreAnsweringSaysWhatToTry()
    {
        QTemporaryDir empty;
        const StationControlReply reply = StationControlSocket::request(
            QDir(empty.path()).filePath(QStringLiteral("nereusd-control")),
            {QStringLiteral("status")}, 1000);
        QVERIFY(!reply.ok);
        QVERIFY(reply.text.contains(QStringLiteral("sudo")));
        verifyPlain(reply);
    }

    // ── A packaged Core (R-IOS-08, R-R3-26) ───────────────────────────
    // A packaged unit pins the Core's HOME to /var/lib/nereusd and its
    // /etc/nereusd.conf may have no state_directory, so the socket is in
    // the profile's directory under that HOME; `sudo nereusd <command>`
    // runs with root's HOME. A short temporary directory stands in for
    // /var/lib/nereusd.

    void thePackagedHomesAreTheUnitsOwn()
    {
#ifdef Q_OS_LINUX
        QCOMPARE(StationControlSocket::packagedHomes(),
                 (QStringList{QStringLiteral("/var/lib/nereusd"),
                              QStringLiteral("/var/lib/private/nereusd")}));
        // The packaged service uses HOME/.config. The command uses Qt's
        // config location, which may differ in test mode or with XDG_CONFIG_HOME.
        const DaemonConfig config = DaemonConfig::defaults();
        const QString home = QString::fromLocal8Bit(qgetenv("HOME"));
        const QString own = StationControlSocket::socketPathFor(config, QStringLiteral("daemon"));
        const QString packaged = QDir::cleanPath(QDir(home).filePath(
            QStringLiteral(".config/NereusSDR/profiles/daemon/nereusd-control")));
        QStringList expected{own};
        if (packaged != own) { expected.append(packaged); }
        QCOMPARE(StationControlSocket::candidatePathsFor(config, QStringLiteral("daemon"),
                                                         {home, home}), expected);
#else
        QVERIFY(StationControlSocket::packagedHomes().isEmpty());
#endif
    }

    void theCommandLooksInThePackagedHomeAfterItsOwn()
    {
        const QStringList homes{QStringLiteral("/var/lib/nereusd"),
                                QStringLiteral("/var/lib/private/nereusd")};
        DaemonConfig config = DaemonConfig::defaults();
        QCOMPARE(StationControlSocket::candidatePathsFor(config, QStringLiteral("daemon"), homes),
                 (QStringList{
                     StationControlSocket::socketPathFor(config, QStringLiteral("daemon")),
                     QStringLiteral("/var/lib/nereusd/.config/NereusSDR/profiles/daemon/"
                                    "nereusd-control"),
                     QStringLiteral("/var/lib/private/nereusd/.config/NereusSDR/profiles/daemon/"
                                    "nereusd-control")}));
        // With state_directory the Core is where it says, and nowhere else.
        config.stateDirectory = QStringLiteral("/var/lib/nereusd");
        QCOMPARE(StationControlSocket::candidatePathsFor(config, QStringLiteral("daemon"), homes),
                 QStringList{QStringLiteral("/var/lib/nereusd/nereusd-control")});
    }

    void aPackagedCoreIsFoundWithNoOptions()
    {
        QTemporaryDir packagedHome(QStringLiteral("/tmp/tcp-XXXXXX"));
        QVERIFY(packagedHome.isValid());
        Core core;
        core.config.stateDirectory.clear();
        const QStringList candidates = StationControlSocket::candidatePathsFor(
            core.config, QStringLiteral("daemon"), {packagedHome.path()});
        QCOMPARE(candidates.size(), 2);
        core.socketPath = candidates.at(1);
        QVERIFY(core.socketPath.startsWith(packagedHome.path()));
        QVERIFY(!QFileInfo::exists(candidates.at(0)));
        QVERIFY(core.start());
        QVERIFY(ask(candidates, {QStringLiteral("status")}).ok);

        // The real entry point, as `sudo nereusd pairing show`: no
        // --profile and no state_directory, with this test's HOME standing
        // in for root's.
        QTemporaryDir configDir;
        const QString noConfig = configDir.filePath(QStringLiteral("absent.conf"));
        QString out;
        QCOMPARE(runHelper({QStringLiteral("--packaged-home"), packagedHome.path(),
                            QStringLiteral("--config"), noConfig, QStringLiteral("pairing"),
                            QStringLiteral("show")},
                           &out),
                 0);
        QVERIFY2(out.contains(core.server()->pairingWindow()->currentCode()), qPrintable(out));
        QCOMPARE(runHelper({QStringLiteral("--packaged-home"), packagedHome.path(),
                            QStringLiteral("--config"), noConfig, QStringLiteral("status")},
                           &out),
                 0);
        QVERIFY2(out.contains(QStringLiteral("This Core: ")), qPrintable(out));
    }

    void aCoreStartedByHandIsStillFoundFirst()
    {
        QTemporaryDir packagedHome(QStringLiteral("/tmp/tcp-XXXXXX"));
        QVERIFY(packagedHome.isValid());
        Core core;
        core.config.stateDirectory.clear();
        const QStringList candidates = StationControlSocket::candidatePathsFor(
            core.config, AppSettings::profileOverride(), {packagedHome.path()});
        QCOMPARE(candidates.size(), 2);
        core.socketPath = candidates.at(0);
        QVERIFY(core.start());
        StationControlSocket packaged([](const QStringList&) {
            return StationControlReply{true, QStringLiteral("the packaged Core")};
        });
        QVERIFY(packaged.listen(candidates.at(1)));
        const StationControlReply reply = ask(candidates, {QStringLiteral("status")});
        QVERIFY(reply.ok);
        QVERIFY2(reply.text.startsWith(QStringLiteral("This Core")), qPrintable(reply.text));
    }

    void noCoreAnsweringNamesWhereItLookedAndWhatWorks()
    {
        QTemporaryDir packagedHome(QStringLiteral("/tmp/tcp-XXXXXX"));
        QVERIFY(packagedHome.isValid());
        const QStringList candidates = StationControlSocket::candidatePathsFor(
            DaemonConfig::defaults(), QStringLiteral("elsewhere"), {packagedHome.path()});
        QCOMPARE(candidates.size(), 2);
        const StationControlReply reply =
            StationControlSocket::request(candidates, {QStringLiteral("status")}, 1000);
        QVERIFY(!reply.ok);
        for (const QString& path : candidates) {
            QVERIFY2(reply.text.contains(path), qPrintable(reply.text));
        }
        QVERIFY2(reply.text.contains(QStringLiteral("sudo nereusd status")),
                 qPrintable(reply.text));
        QVERIFY2(reply.text.contains(QStringLiteral("systemctl status nereusd")),
                 qPrintable(reply.text));
        QVERIFY2(reply.text.contains(QStringLiteral("--config and --profile")),
                 qPrintable(reply.text));
        verifyPlain(reply);

        // A socket file left with no Core behind it is the one named.
        QVERIFY(QDir().mkpath(QFileInfo(candidates.at(1)).absolutePath()));
        {
            QFile leftBehind(candidates.at(1));
            QVERIFY(leftBehind.open(QIODevice::WriteOnly));
        }
        const StationControlReply stale =
            StationControlSocket::request(candidates, {QStringLiteral("status")}, 1000);
        QVERIFY(!stale.ok);
        QVERIFY2(stale.text.contains(candidates.at(1)), qPrintable(stale.text));
        QVERIFY2(!stale.text.contains(candidates.at(0)), qPrintable(stale.text));
        verifyPlain(stale);
    }

    // The usage text says what works on each kind of Core.
    void theCommandListSaysWhatWorks()
    {
        const QString usage = StationControlCommands::usage();
        QVERIFY(usage.contains(QStringLiteral("sudo nereusd status")));
        QVERIFY(usage.contains(QStringLiteral("--config and --profile")));
        QVERIFY2(OperatorWording::isPlain(usage), qPrintable(usage));
    }

    // ── The real binary ───────────────────────────────────────────────

    void theNereusdBinaryFindsTheSocketThroughConfig()
    {
        Core core;
        QVERIFY(core.start());
        QTemporaryDir configDir;
        const QString configPath = configDir.filePath(QStringLiteral("nereusd.conf"));
        QFile config(configPath);
        QVERIFY(config.open(QIODevice::WriteOnly));
        config.write("state_directory = " + core.stateDir.path().toUtf8() + "\n");
        config.close();

        const auto runBinary = [](const QStringList& args, QString* out) {
            QProcess process;
            process.setProgram(QStringLiteral(NEREUSD_BINARY));
            process.setArguments(args);
            // The command reads no $HOME: point it nowhere.
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            env.insert(QStringLiteral("HOME"), QStringLiteral("/nonexistent-home-for-this-test"));
            process.setProcessEnvironment(env);
            process.start();
            const bool finished = QTest::qWaitFor(
                [&process]() { return process.state() == QProcess::NotRunning; }, 30000);
            *out = QString::fromUtf8(process.readAllStandardOutput()
                                     + process.readAllStandardError());
            if (!finished) {
                qWarning() << "nereusd" << args << "did not exit within 30 s; output:" << *out;
                return -2;
            }
            return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
        };

        QString out;
        QCOMPARE(runBinary({QStringLiteral("--config"), configPath, QStringLiteral("status")},
                           &out),
                 0);
        QVERIFY2(out.contains(QStringLiteral("This Core: ")), qPrintable(out));

        QCOMPARE(runBinary({QStringLiteral("--config"), configPath, QStringLiteral("--profile"),
                            QStringLiteral("anything"), QStringLiteral("pairing"),
                            QStringLiteral("show")},
                           &out),
                 0);
        QVERIFY(out.contains(core.server()->pairingWindow()->currentCode()));

        QCOMPARE(runBinary({QStringLiteral("--config"), configPath, QStringLiteral("reset"),
                            QStringLiteral("--unclaimed")},
                           &out),
                 1);
        QVERIFY(out.contains(QStringLiteral("Nothing was changed")));

        QCOMPARE(runBinary({QStringLiteral("--config"), configPath, QStringLiteral("sideways")},
                           &out),
                 2);
        QVERIFY(out.contains(QStringLiteral("Unknown command.")));
    }
    // The real entry point (server_main.cpp) as a console command: it finds
    // a running Core through --profile alone, or through --config's
    // state_directory. The Core here is this test's; the helper process
    // shares this test's short home, so the profile's directory is the same
    // on both sides.
    void theEntryPointFindsTheCoreThroughProfileOrConfig()
    {
        Core core;
        core.config.stateDirectory.clear();
        core.socketPath = StationControlSocket::socketPathFor(core.config,
                                                              AppSettings::profileOverride());
        QVERIFY(core.start());
        QTemporaryDir configDir;
        const QString noConfig = configDir.filePath(QStringLiteral("absent.conf"));

        const auto command = [](const QStringList& args, QString* out) {
            QProcess process;
            process.setProcessChannelMode(QProcess::MergedChannels);
            process.start(QCoreApplication::applicationFilePath(),
                          QStringList{QStringLiteral("--daemon-helper")} + args);
            const bool finished = QTest::qWaitFor(
                [&process]() { return process.state() == QProcess::NotRunning; }, 30000);
            *out = QString::fromUtf8(process.readAll());
            if (!finished) {
                qWarning() << "helper" << args << "did not exit within 30 s; output:" << *out;
                return -2;
            }
            return process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
        };
        QString out;
        QCOMPARE(command({QStringLiteral("--config"), noConfig, QStringLiteral("--profile"),
                          AppSettings::profileOverride(), QStringLiteral("status")},
                         &out),
                 0);
        QVERIFY2(out.contains(QStringLiteral("This Core: ")), qPrintable(out));
        QCOMPARE(command({QStringLiteral("--config"), noConfig, QStringLiteral("--profile"),
                          AppSettings::profileOverride(), QStringLiteral("pairing"),
                          QStringLiteral("show")},
                         &out),
                 0);
        QVERIFY(out.contains(core.server()->pairingWindow()->currentCode()));
        // Another profile finds no Core, and says what to try.
        QCOMPARE(command({QStringLiteral("--config"), noConfig, QStringLiteral("--profile"),
                          QStringLiteral("elsewhere"), QStringLiteral("status")},
                         &out),
                 1);
        QVERIFY(out.contains(QStringLiteral("sudo")));
        // The command words are checked before anything else.
        QCOMPARE(command({QStringLiteral("--config"), noConfig, QStringLiteral("sideways")}, &out),
                 2);

        // Part C fix wave (R2-M6): a configuration file the Core would not
        // start with is reported, not "No Core answered".
        const QString invalid = configDir.filePath(QStringLiteral("invalid.conf"));
        {
            QFile file(invalid);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("radio_mac = not a mac\n");
        }
        QCOMPARE(command({QStringLiteral("--config"), invalid, QStringLiteral("--profile"),
                          AppSettings::profileOverride(), QStringLiteral("status")},
                         &out),
                 2);
        QVERIFY2(out.contains(QStringLiteral("is not valid")), qPrintable(out));
        QVERIFY(!out.contains(QStringLiteral("No Core answered")));
        // One that is there but cannot be read says so, and how to fix it.
#ifndef Q_OS_WIN
        const QString unreadable = configDir.filePath(QStringLiteral("unreadable.conf"));
        {
            QFile file(unreadable);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("remote_port = 47910\n");
        }
        QVERIFY(QFile::setPermissions(unreadable, QFileDevice::WriteOwner));
        QFile probe(unreadable);
        if (!probe.open(QIODevice::ReadOnly)) {  // running as root reads it anyway
            QCOMPARE(command({QStringLiteral("--config"), unreadable, QStringLiteral("--profile"),
                              AppSettings::profileOverride(), QStringLiteral("status")},
                             &out),
                     2);
            QVERIFY2(out.contains(QStringLiteral("Could not read the configuration file")),
                     qPrintable(out));
            QVERIFY(out.contains(QStringLiteral("sudo")));
        }
        QFile::setPermissions(unreadable, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#endif
    }
};

int main(int argc, char* argv[])
{
    if (argc > 1 && std::strcmp(argv[1], "--daemon-helper") == 0) {
        int shift = 1;
        if (argc > 3 && std::strcmp(argv[2], "--packaged-home") == 0) {
            StationControlSocket::setPackagedHomesForTest({QString::fromLocal8Bit(argv[3])});
            shift = 3;
        }
        argv[shift] = argv[0];
        return nereusdEntryPointForTest(argc - shift, argv + shift);
    }
    QCoreApplication app(argc, argv);
    TstStationControlSocket test;
    return QTest::qExec(&test, argc, argv);
}
#include "tst_station_control_socket.moc"
