#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// tests/RendezvousTestHarness.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Tasks 27 to 29 (R-IOS-08, R-IOS-16): the remote access
// service on this computer (the real Python service, rendezvous/server,
// with the fake STUN/TURN server tests/tools/fake_turn_server.py) and a
// Core with its StationServer, as tst_rendezvous_client stands them up,
// shared with tst_path_racer. A test that includes this defines
// NEREUS_SOURCE_DIR and NEREUS_TEST_PYTHON (tests/CMakeLists.txt). Nothing here reaches beyond
// this computer.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: moved out of tst_rendezvous_client for Task 29 by J.J.
//               Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-29: LocalService waits for the service's own "listening on"
//               line, fails at once if it exits, and says why with its
//               output (startFailure). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-29: EndWatch, waitUntil and waitForHandshake, moved here from
//               tst_relay_session: a wait that ends when what it waits on
//               closes or fails, stops at its bound (QTRY_* runs on for
//               twice its timeout after it expires) and says why. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: turnReport(): the fake relay's output with its standard
//               error and whether it is still running, so a test that
//               waits on the relay says when the relay itself stopped;
//               EndWatch::watch(LocalService&) ends a wait when the fake
//               relay exits; turnSecret() and turnPort() for a test that
//               speaks to the fake relay itself. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-09-30: LocalService starts the service through
//               tests/tools/rendezvous_service_for_test.py with
//               --parent-pid, so it ends with the test process however that
//               ends (a ctest timeout left it running with parent 1);
//               setParentPidForTest() and serviceProcess() for the test of
//               that. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-09-30: INFRA-C1: the helpers start on testPython(), the
//               interpreter CMake resolved (NEREUS_TEST_PYTHON), not on
//               whatever python3 is first on PATH. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QDeadlineTimer>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QPointer>
#include <QProcess>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>

#include "core/session/RendezvousDialer.h"
#include <QUrl>

#include <algorithm>
#include <functional>
#include <memory>

#ifdef Q_OS_UNIX
#include <pwd.h>
#include <unistd.h>
#endif

#include "core/AppSettings.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/DeviceStore.h"
#include "core/security/StationIdentity.h"
#include "core/session/RendezvousClient.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/session/media/IMediaTransport.h"
#include "models/RadioModel.h"

#include "fakes/UpgradedCoreToken.h"

namespace NereusSDR::Test::Rendezvous {

inline QString readText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

/// Task 29 step 2b (the run rule on failures under load): how long a test
/// waits for a session through the service. One dial may take
/// RendezvousDialer::kDialDeadlineMs (the ICE gathering and connectivity
/// deadlines, 69.5 s) before it succeeds or fails, so a flat 60 s budget
/// failed a connect that the code itself still allowed (61.35 s at load
/// 152). The budget is that deadline and 10 s for the sign-in after it.
inline constexpr int kServiceConnectBudgetMs = RendezvousDialer::kDialDeadlineMs + 10000;

inline QByteArray randomBytes(int count)
{
    QByteArray bytes(count, Qt::Uninitialized);
    for (int index = 0; index < count; ++index) {
        bytes[index] = static_cast<char>(QRandomGenerator::system()->bounded(256));
    }
    return bytes;
}

// A P-256 key made at run time in a directory of its own.
struct TestKey {
    QTemporaryDir dir;
    StationIdentity identity = StationIdentity::loadOrCreate(dir.path());

    QByteArray spki() const { return identity.publicKeySpki(); }
    QByteArray sign(const QByteArray& message) const { return identity.sign(message); }
};

// The Python the service runs on, with this user's own packages: ctest
// gives every test a home of its own (tests/CMakeLists.txt), and Python
// finds a user's packages under the home directory, so the service is
// started with the account's real one.
// The interpreter the helpers run on: an absolute path CMake resolved at
// configure time (NEREUS_TEST_PYTHON), never a PATH lookup here. On the
// Linux CI runner a PATH lookup found install-qt-action's toolcache Python,
// which has no websockets, and every service start failed (INFRA-C1).
inline QString testPython()
{
    return QStringLiteral(NEREUS_TEST_PYTHON);
}

inline QProcessEnvironment pythonEnvironment()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#ifdef Q_OS_UNIX
    if (const passwd* account = getpwuid(getuid()); account != nullptr && account->pw_dir != nullptr) {
        env.insert(QStringLiteral("HOME"), QString::fromLocal8Bit(account->pw_dir));
    }
#endif
    return env;
}

inline quint16 freeTcpPort()
{
    QTcpServer probe;
    probe.listen(QHostAddress::LocalHost, 0);
    return probe.serverPort();
}

class LocalService {
public:
    /// How long a live service may take to be listening. A test executable
    /// gets ctest's default TIMEOUT of 120 s (tests/CMakeLists.txt,
    /// nereus_add_test); half of it leaves the other half for the test
    /// body, so a service that is merely slow to start is reported here,
    /// with its output, before ctest kills the executable with none. A
    /// service that fails exits and is reported at once, and a ready one
    /// returns as soon as it says so, so the bound costs nothing when all
    /// is well.
    static constexpr int kServiceReadyBoundMs = 60000;

    /// `stun`: the service's hello names the fake's STUN server. `relay`:
    /// the service mints relay credentials for the fake's TURN server.
    explicit LocalService(bool stun = true, bool relay = true) : m_stun(stun), m_relay(relay) {}

    /// The fake relay refuses every allocation with 486 (Allocation Quota
    /// Reached), as a full relay does. Before start().
    void setRelayFull(bool full) { m_relayFull = full; }
    /// Task 29 re-review: the service also mints relay grants (rendezvous
    /// section 12.1), with its own relay secret, naming `relayUrl`. Before
    /// start().
    void setRelayGrants(bool on, const QString& relayUrl = QStringLiteral("wss://127.0.0.1:1/v1/relay"))
    {
        m_relayGrants = on;
        m_relayUrl = relayUrl;
    }

    ~LocalService() { stop(); stopTurn(); }

    bool start()
    {
        m_startFailure.clear();
        if ((m_stun || m_relay) && !startTurn()) {
            m_startFailure = QStringLiteral("the fake TURN server did not start");
            return false;
        }
        m_port = freeTcpPort();
        QFile secret(m_dir.filePath(QStringLiteral("turn-secret")));
        if (!secret.open(QIODevice::WriteOnly)) {
            return false;
        }
        secret.write(m_secret);
        secret.close();
        QString relayLines;
        if (m_relayGrants) {
            QFile relaySecret(m_dir.filePath(QStringLiteral("relay-secret")));
            if (!relaySecret.open(QIODevice::WriteOnly)) {
                return false;
            }
            relaySecret.write(randomBytes(32).toHex());
            relaySecret.close();
            relayLines = QStringLiteral("relay_secret_file = %1\nrelay_url = %2\n")
                             .arg(relaySecret.fileName(), m_relayUrl);
        }
        QFile config(m_dir.filePath(QStringLiteral("rendezvous.conf")));
        if (!config.open(QIODevice::WriteOnly)) {
            m_startFailure = QStringLiteral("could not write %1").arg(config.fileName());
            return false;
        }
        const QString stun = m_stun ? QStringLiteral("stun:127.0.0.1:%1").arg(m_turnPort)
                                    : QString();
        const QString turn = m_relay
            ? QStringLiteral("turn:127.0.0.1:%1?transport=udp").arg(m_turnPort)
            : QString();
        config.write(QStringLiteral("[rendezvous]\n"
                                    "listen = 127.0.0.1:%1\n"
                                    "stun_urls = %2\n"
                                    "turn_urls = %3\n"
                                    "turn_secret_file = %4\n"
                                    "log_level = info\n")
                         .arg(m_port)
                         .arg(stun, turn,
                              m_relay ? secret.fileName() : QString())
                         .toUtf8()
                     + relayLines.toUtf8());
        config.close();
        return launch();
    }

    // Starts the service again on the same port, as after a restart.
    //
    // Ready means the service's own "listening on" line (nereus_rendezvous
    // __main__.run logs it once every listening socket is serving), read
    // from its standard error. A process that exits first fails at once,
    // with its output. The bound only governs a service still starting;
    // see kServiceReadyBoundMs.
    bool launch()
    {
        m_startFailure.clear();
        const qsizetype logFrom = m_log.size();
        m_process = std::make_unique<QProcess>();
        QProcessEnvironment env = pythonEnvironment();
        env.insert(QStringLiteral("PYTHONPATH"),
                   QStringLiteral(NEREUS_SOURCE_DIR "/rendezvous/server"));
        env.insert(QStringLiteral("PYTHONUNBUFFERED"), QStringLiteral("1"));
        m_process->setProcessEnvironment(env);
        m_process->setWorkingDirectory(m_dir.path());
        QElapsedTimer elapsed;
        elapsed.start();
        const QDeadlineTimer deadline(kServiceReadyBoundMs);
        // The destructor stops the service on a pass, a failure and an early
        // return; --parent-pid also ends it when this test process is killed
        // or crashes, when no destructor runs. The launcher runs the
        // service's own main unchanged.
        m_process->start(testPython(),
                         {QStringLiteral(NEREUS_SOURCE_DIR "/tests/tools/rendezvous_service_for_test.py"),
                          QStringLiteral("--parent-pid"),
                          QString::number(m_parentPid != 0 ? m_parentPid
                                                           : QCoreApplication::applicationPid()),
                          QStringLiteral("--config"),
                          m_dir.filePath(QStringLiteral("rendezvous.conf"))});
        if (!m_process->waitForStarted(static_cast<int>(deadline.remainingTime()))) {
            m_startFailure = QStringLiteral("%1 did not start: %2")
                                 .arg(testPython(), m_process->errorString());
            return false;
        }
        while (true) {
            m_log += QString::fromUtf8(m_process->readAllStandardError());
            m_stdout += QString::fromUtf8(m_process->readAllStandardOutput());
            if (m_log.indexOf(QLatin1String("listening on "), logFrom) >= 0) {
                m_readyMs = elapsed.elapsed();
                return true;
            }
            if (m_process->state() == QProcess::NotRunning) {
                m_startFailure = QStringLiteral("the service exited (code %1) before it was "
                                                "listening, after %2 ms")
                                     .arg(m_process->exitCode())
                                     .arg(elapsed.elapsed());
                return false;
            }
            if (deadline.hasExpired()) {
                m_startFailure = QStringLiteral("the service was not listening after %1 ms")
                                     .arg(elapsed.elapsed());
                return false;
            }
            m_process->waitForReadyRead(
                static_cast<int>(qMin<qint64>(100, deadline.remainingTime())));
        }
    }

    /// Why start() or launch() returned false, with everything the service
    /// wrote, for QVERIFY2.
    QString startFailure()
    {
        if (m_process) {
            m_log += QString::fromUtf8(m_process->readAllStandardError());
            m_stdout += QString::fromUtf8(m_process->readAllStandardOutput());
        }
        return QStringLiteral("%1\n--- service stderr ---\n%2\n--- service stdout ---\n%3")
            .arg(m_startFailure.isEmpty() ? QStringLiteral("(no failure recorded)")
                                          : m_startFailure,
                 m_log, m_stdout);
    }

    /// How long the last launch took to be ready.
    qint64 readyMs() const { return m_readyMs; }

    void stop()
    {
        if (m_process) {
            m_log += QString::fromUtf8(m_process->readAllStandardError());
            m_process->terminate();
            if (!m_process->waitForFinished(5000)) {
                m_process->kill();
                m_process->waitForFinished(2000);
            }
            m_log += QString::fromUtf8(m_process->readAllStandardError());
            m_process.reset();
        }
    }

    QString log()
    {
        if (m_process) {
            m_log += QString::fromUtf8(m_process->readAllStandardError());
        }
        return m_log;
    }

    QString turnOutput()
    {
        if (m_turn) {
            m_turnLog += QString::fromUtf8(m_turn->readAllStandardOutput());
        }
        return m_turnLog;
    }

    /// For a failure message: what the fake relay printed, whether it is
    /// still running (and how it ended if not) and its standard error, so
    /// a relay that stopped is told apart from a release never sent.
    QString turnReport()
    {
        // A short look, so an exit and its last output are seen even when
        // the caller has not run the event loop (a blocking probe).
        if (m_turn && m_turn->state() == QProcess::Running) {
            m_turn->waitForFinished(kTurnReportLookMs);
        }
        const QString output = turnOutput();
        QString state = QStringLiteral("not started");
        if (m_turn) {
            m_turnErrors += QString::fromUtf8(m_turn->readAllStandardError());
            state = m_turn->state() == QProcess::Running
                ? QStringLiteral("running")
                : QStringLiteral("exited (code %1)").arg(m_turn->exitCode());
        }
        return QStringLiteral("fake TURN server %1\n--- its output:\n%2--- its errors:\n%3")
            .arg(state, output, m_turnErrors);
    }

    /// The process the service's end is tied to (this test process when
    /// 0), for the test that the service ends with it. Set before start().
    void setParentPidForTest(qint64 pid) { m_parentPid = pid; }
    /// The service's process (null before start() and after stop()).
    QProcess* serviceProcess() const { return m_process.get(); }

    /// The fake relay's process (null before start()), its shared secret
    /// and its UDP port, for a test that speaks to it directly.
    QProcess* turnProcess() const { return m_turn.get(); }
    QByteArray turnSecret() const { return m_secret; }
    quint16 turnPort() const { return m_turnPort; }

    QUrl url() const { return QUrl(QStringLiteral("ws://127.0.0.1:%1/").arg(m_port)); }
    quint16 port() const { return m_port; }

private:
    bool startTurn()
    {
        QFile secret(m_dir.filePath(QStringLiteral("turn-secret")));
        if (!secret.open(QIODevice::WriteOnly)) {
            return false;
        }
        secret.write(m_secret);
        secret.close();
        const QString portFile = m_dir.filePath(QStringLiteral("turn-port"));
        m_turn = std::make_unique<QProcess>();
        m_turn->setProcessEnvironment(pythonEnvironment());
        // The destructor stops the helper on a pass, a failure and an early
        // return; --parent-pid also ends it when this test process is
        // killed or crashes, when no destructor runs.
        QStringList arguments{QStringLiteral(NEREUS_SOURCE_DIR "/tests/tools/fake_turn_server.py"),
                              QStringLiteral("--secret-file"), secret.fileName(),
                              QStringLiteral("--port-file"), portFile,
                              QStringLiteral("--parent-pid"),
                              QString::number(QCoreApplication::applicationPid())};
        if (m_relayFull) {
            arguments.append(QStringLiteral("--quota-full"));
        }
        m_turn->start(testPython(), arguments);
        if (!m_turn->waitForStarted(10000)) {
            return false;
        }
        QDeadlineTimer deadline(10000);
        while (!QFile::exists(portFile) && !deadline.hasExpired()) {
            QTest::qWait(20);
        }
        m_turnPort = static_cast<quint16>(readText(portFile).toInt());
        return m_turnPort != 0;
    }

    void stopTurn()
    {
        if (m_turn) {
            m_turn->terminate();
            if (!m_turn->waitForFinished(3000)) {
                m_turn->kill();
                m_turn->waitForFinished(2000);
            }
            m_turn.reset();
        }
    }

    // How long turnReport() looks for the fake relay's exit.
    static constexpr int kTurnReportLookMs = 200;
    bool m_stun = true;
    bool m_relay = true;
    bool m_relayFull = false;
    bool m_relayGrants = false;
    QString m_relayUrl;
    QTemporaryDir m_dir;
    QByteArray m_secret = randomBytes(24).toHex();
    quint16 m_port = 0;
    quint16 m_turnPort = 0;
    std::unique_ptr<QProcess> m_process;
    std::unique_ptr<QProcess> m_turn;
    QString m_log;
    QString m_stdout;
    QString m_startFailure;
    qint64 m_readyMs = -1;
    qint64 m_parentPid = 0;
    QString m_turnLog;
    QString m_turnErrors;
};

// One Core with its StationServer, as the pairing tests stand it up.
struct Core {
    QTemporaryDir settingsDir;
    QTemporaryDir securityDir;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<RadioModel> model;
    std::unique_ptr<StationServer> server;

    // `upgraded`: a Core from before paired devices, which still signs a
    // window in by its token (a new Core has none). `tokenLike` names
    // another upgraded Core's security directory whose token this one
    // shares, so a test can show the token was never sent to it.
    explicit Core(bool upgraded = false, const QString& tokenLike = QString())
    {
        settings = std::make_unique<AppSettings>(
            settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
        settings->setValue(QStringLiteral("StationCallsign"), QStringLiteral("KG4VCF"));
        model = std::make_unique<RadioModel>();
        const QString security = NereusSDR::Test::seedCoreIdentity(securityDir.path());
        if (!tokenLike.isEmpty()) {
            QFile::copy(QDir(tokenLike).filePath(QStringLiteral("station-token")),
                        QDir(security).filePath(QStringLiteral("station-token")));
        }
        if (upgraded) {
            NereusSDR::Test::seedUpgradedCoreToken(security);
        }
        server = std::make_unique<StationServer>(model.get(), *settings, security);
        server->setHeartbeatIntervalMs(0);
    }

    ~Core() { server.reset(); }

    // This computer, paired as a device the way a pairing leaves it.
    bool pairComputer(const ClientDeviceIdentity& key)
    {
        PairedDevice device;
        device.id = key.fingerprint();
        device.publicKeySpki = key.publicKeySpki();
        device.name = QStringLiteral("Shack MacBook");
        device.kind = QStringLiteral("computer");
        return server->deviceStore()->add(device);
    }

    QUrl url() const
    {
        return QUrl(QStringLiteral("wss://127.0.0.1:%1").arg(server->serverPort()));
    }

    bool pair(const TestKey& key, const QString& name = QStringLiteral("Test phone"))
    {
        PairedDevice device;
        device.id = StationIdentity::fingerprintOf(key.spki());
        device.publicKeySpki = key.spki();
        device.name = name;
        device.kind = QStringLiteral("phone");
        return server->deviceStore()->add(device);
    }
};


// What a wait is waiting on, closing or failing first. The first such
// reason is kept; a wait ends as soon as there is one. Declared after the
// objects it watches, so it lets go of them before they are destroyed.
class EndWatch {
public:
    EndWatch() = default;
    EndWatch(const EndWatch&) = delete;
    EndWatch& operator=(const EndWatch&) = delete;
    ~EndWatch()
    {
        for (const QMetaObject::Connection& connection : m_connections) {
            QObject::disconnect(connection);
        }
    }

    void watch(StationClient* window)
    {
        m_connections.append(QObject::connect(
            window, &StationClient::sessionEnded, [this](const QString& reason) {
                note(QStringLiteral("the session ended: %1")
                         .arg(reason.isEmpty() ? QStringLiteral("(no reason given)") : reason));
            }));
    }
    void watch(IMediaTransport* transport, const QString& name)
    {
        m_connections.append(QObject::connect(
            transport, &IMediaTransport::connectionFailed, [this, name](const QString& message) {
                note(QStringLiteral("the %1 media connection failed: %2").arg(name, message));
            }));
        m_connections.append(QObject::connect(
            transport, &IMediaTransport::errorOccurred, [this, name](const QString& message) {
                note(QStringLiteral("the %1 media connection hit an error: %2").arg(name, message));
            }));
        m_connections.append(QObject::connect(transport, &IMediaTransport::closed, [this, name] {
            note(QStringLiteral("the %1 media connection closed").arg(name));
        }));
    }
    void watch(RendezvousClient* client)
    {
        m_connections.append(QObject::connect(client, &RendezvousClient::connectionLost, [this] {
            note(QStringLiteral("the Core lost the remote access service"));
        }));
    }

    // The fake relay stopping ends a wait on anything that goes through it
    // (a connection through the relay, an allocation given back). Its
    // output and errors are in LocalService::turnReport().
    void watch(LocalService& service)
    {
        QProcess* turn = service.turnProcess();
        if (turn == nullptr || turn->state() != QProcess::Running) {
            note(QStringLiteral("the fake TURN server was not running"));
            return;
        }
        m_connections.append(QObject::connect(
            turn, &QProcess::finished, [this](int code, QProcess::ExitStatus) {
                note(QStringLiteral("the fake TURN server exited (code %1)").arg(code));
            }));
    }

    bool ended() const { return !m_reason.isEmpty(); }
    QString reason() const { return m_reason; }

private:
    void note(const QString& reason)
    {
        if (m_reason.isEmpty()) {
            m_reason = reason;
        }
    }

    QString m_reason;
    QList<QMetaObject::Connection> m_connections;
};

// Waits until `done`, until `ends` has a reason, or for `boundMs`, whichever
// is first, and says which through `why`. Unlike QTRY_*, which after its
// timeout waits twice as long again to report whether more time would have
// helped, it stops at the bound, and it stops at once when what it waits on
// has closed or failed.
inline bool waitUntil(const std::function<bool()>& done, int boundMs, const EndWatch& ends,
               const QString& what, QString* why, qint64* elapsedMs = nullptr)
{
    QElapsedTimer elapsed;
    elapsed.start();
    const QDeadlineTimer deadline(boundMs);
    while (!done() && !ends.ended() && !deadline.hasExpired()) {
        QTest::qWait(int(std::clamp<qint64>(deadline.remainingTime(), 1, 50)));
    }
    if (elapsedMs != nullptr) {
        *elapsedMs = elapsed.elapsed();
    }
    if (done()) {
        return true;
    }
    if (why != nullptr) {
        *why = ends.ended()
            ? QStringLiteral("waiting for %1, %2 after %3 ms")
                  .arg(what, ends.reason())
                  .arg(elapsed.elapsed())
            : QStringLiteral("%1 had not happened after %2 ms (the bound)")
                  .arg(what)
                  .arg(elapsed.elapsed());
    }
    return false;
}

// Adds the window's own account of its attempt to a failed wait's reason.
inline QString withAttempt(const QString& why, const StationClient& window)
{
    return QStringLiteral("%1\nlast error: %2\nattempt: %3")
        .arg(why, window.lastError(), window.connectionAttempt().summary());
}

// Waits for `window`'s handshake for `boundMs`, ending at once if its
// session ends first (StationClient::sessionEnded: the handshake deadline,
// a heartbeat, a failed dial or race, a refusal), with the reason, the
// window's last error and its attempt summary in `why`. Pass `armed`, a
// watch already on the window, to see a session that ends inside the
// connect call itself; without it the watch starts here, and such an end
// runs the wait to its bound.
inline bool waitForHandshake(StationClient& window, int boundMs, QString* why,
                             qint64* elapsedMs = nullptr, const EndWatch* armed = nullptr)
{
    EndWatch own;
    if (armed == nullptr) {
        own.watch(&window);
    }
    const EndWatch& ends = armed != nullptr ? *armed : own;
    const bool ok = waitUntil([&window] { return window.isHandshakeComplete(); }, boundMs,
                              ends, QStringLiteral("the window's handshake"), why, elapsedMs);
    if (!ok && why != nullptr) {
        *why = withAttempt(*why, window);
    }
    return ok;
}

// The same for a window that is still being created: `find` returns its
// client once there is one (nullptr until then), and the session watch
// starts from that moment. One bound covers both.
inline bool waitForHandshake(const std::function<StationClient*()>& find, int boundMs,
                             QString* why)
{
    QPointer<StationClient> client;
    EndWatch ends;
    const bool ok = waitUntil(
        [&] {
            if (!client) {
                client = find();
                if (client) {
                    ends.watch(client);
                }
            }
            return client && client->isHandshakeComplete();
        },
        boundMs, ends, QStringLiteral("the window's handshake"), why);
    if (!ok && why != nullptr) {
        *why = client ? withAttempt(*why, *client)
                      : QStringLiteral("%1 (no window client was ever created)").arg(*why);
    }
    return ok;
}

} // namespace NereusSDR::Test::Rendezvous
