// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_roundtable_e2e.cpp  (NereusSDR)
// =================================================================
//
// Roundtable plan Task 20 (R-RT-24, R-RT-26's software half; wire spec 6,
// the whole path): the real Roundtable server against three in-process
// Cores. The server is started as its own process, `<python> -m
// nereus_roundtable` on the interpreter CMake resolved (NEREUS_TEST_PYTHON,
// as the rendezvous tests do), listening on free 127.0.0.1 ports with a temporary
// database and the real clock; each Core is a RoundtableClient with its
// own station key, all on one USB frequency:
//   - A keys; B's reading of A appears in A's room within 2 s of being
//     taken, stored with A's turn, and the turn is credited to A;
//   - A unkeys; the turn ends; an unknown turn opens from B's present
//     readings and B names it;
//   - A posts and B gets the mention;
//   - C has nobody at the radio: its row shows AWAY to the others, and its
//     post is refused `away`, as is its name;
//   - A's archive, through the web sign-in link it asked for, lists the
//     session with B's track of A's turn (and C's, marked AWAY); the link
//     works once;
//   - the server stops cleanly with exit code 0, and its log and files
//     hold no address, no whole station id, no callsign and no message
//     text (wire spec 9.3, 9.4). On POSIX the stop is SIGTERM, the
//     droplet's systemd stop; on Windows, which has no SIGTERM for a
//     console program, it is the end of the server's standard input
//     (--stop-on-stdin-eof).
// The cases run in order on one server and one set of Cores.
//
// Skipped with the reason when no Python 3 is found or it lacks the
// websockets or cryptography module; ctest reports that as Skipped, never
// Passed (tests/CMakeLists.txt). Nothing leaves 127.0.0.1. The 2 s
// window is measured against the wall clock, so the test is `realtime`:
// a failure prints the load average and the measured latency.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 20).
//   2026-10-25: the server runs on NEREUS_TEST_PYTHON, found on PATH as
//               python3 then python when CMake found none, not on
//               /usr/bin/python3, which Windows does not have; the skip
//               names what is missing. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-26: the server starts with --stop-on-stdin-eof and Windows
//               stops it by closing its standard input: terminate() there
//               posts WM_CLOSE, which a console Python never gets, so the
//               test force-killed it after 10 s. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-26: a Python under WindowsApps (the Microsoft Store alias) is
//               not taken. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

// 2026-10-30: Real-server frequency read/post regression, J.J. Boyd
// (KG4VCF), AI-assisted via OpenAI Codex. NereusSDR-original.
#include "RealtimeTestLoad.h"
#include "core/roundtable/RoundtableClient.h"
#include "core/roundtable/RoundtableWire.h"
#include "core/security/StationIdentity.h"

#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrlQuery>
#include <QtTest/QtTest>

#include <memory>

#if !defined(Q_OS_WIN)
#include <pwd.h>
#include <stdlib.h>
#include <unistd.h>
#endif

using namespace NereusSDR;

namespace {

// Windows' Microsoft Store python3.exe and python.exe under
// ...\WindowsApps\ are aliases that open the Store instead of running
// Python, so none is taken: the skip then says no Python was found.
bool isStoreAlias(const QString& path)
{
    return QDir::fromNativeSeparators(path).contains(QLatin1String("/WindowsApps/"),
                                                     Qt::CaseInsensitive);
}

// The interpreter CMake resolved for the Python-backed tests
// (NEREUS_TEST_PYTHON, tests/CMakeLists.txt): an absolute path, which CI
// sets to /usr/bin/python3 with Ubuntu's websockets and cryptography. When
// CMake found none it is the bare name python3, looked up here on PATH as
// python3 and then python (Windows installs python.exe). Empty when none
// is found.
QString findPython()
{
    const QString configured = QStringLiteral(NEREUS_TEST_PYTHON);
    if (QFileInfo(configured).isAbsolute()) {
        return QFileInfo(configured).isExecutable() && !isStoreAlias(configured) ? configured
                                                                                  : QString();
    }
    for (const QString& name : {configured, QStringLiteral("python3"), QStringLiteral("python")}) {
        const QString found = QStandardPaths::findExecutable(name);
        if (!found.isEmpty() && !isStoreAlias(found)) {
            return found;
        }
    }
    return {};
}

const QString kPython = findPython();
const QString kServerDir = QStringLiteral(NEREUS_ROUNDTABLE_SERVER_DIR);
const QString kHost = QStringLiteral("127.0.0.1");
const QString kRoom = QStringLiteral("usb-14212000");
constexpr qint64 kHz = 14212000;
// Design R-RT-26: a reading shows in the talker's room within 2 s.
constexpr qint64 kReadingWindowMs = 2000;
// The join dwell is 10 s (wire spec 6.4); the rest is slack.
constexpr int kJoinTimeoutMs = 25000;
// Ending and quiet are 3 s each (6.6); the rest is slack.
constexpr int kTurnTimeoutMs = 10000;
constexpr int kWaitMs = 8000;
// The signal floor's warm-up after the first reading (6.5), plus 1 s.
constexpr qint64 kFloorReadyMs = 11000;
constexpr int kMeterIntervalMs = 200;
constexpr double kQuietDbm = -120.0;
constexpr double kSignalDbm = -80.0;
const QString kName = QStringLiteral("W1AW");
const QString kPostText = QStringLiteral("W4XYZ good copy, thanks for the report");
const QString kAwayText = QStringLiteral("Anyone copy this one?");
// Wire spec 8.2.
const QString kAwayReason = QStringLiteral("Nobody is at the radio, so this station can't post or name turns.");

QString loadAverageText()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("load average not available on Windows");
#else
    double load[3] = {0.0, 0.0, 0.0};
    if (getloadavg(load, 3) != 3) {
        return QStringLiteral("load average could not be read");
    }
    return QStringLiteral("load average %1 %2 %3").arg(load[0], 0, 'f', 2).arg(load[1], 0, 'f', 2).arg(load[2], 0, 'f', 2);
#endif
}

RoundtableSliceState usbSlice(bool keyed)
{
    RoundtableSliceState slice;
    slice.id = 0;
    slice.hz = kHz;
    slice.mode = QStringLiteral("USB");
    slice.filterHz = 2700;
    slice.keyed = keyed;
    return slice;
}

// One room as a Core's screens hold it: rows by ref, turns and messages by
// id, from the client's room and delta signals (wire spec 6.11).
struct RoomView {
    bool open = false;
    QHash<QString, QJsonObject> members;
    QHash<qint64, QJsonObject> turns;
    QHash<qint64, QJsonObject> messages;

    void applyRoom(const QJsonObject& full)
    {
        if (full.value(QStringLiteral("id")).toString() != kRoom) {
            return;
        }
        open = true;
        members.clear();
        turns.clear();
        messages.clear();
        for (const QJsonValue& v : full.value(QStringLiteral("members")).toArray()) {
            members.insert(v.toObject().value(QStringLiteral("ref")).toString(), v.toObject());
        }
        for (const QString& key : {QStringLiteral("earlier"), QStringLiteral("now")}) {
            for (const QJsonValue& v : full.value(key).toArray()) {
                turns.insert(v.toObject().value(QStringLiteral("id")).toInteger(), v.toObject());
            }
        }
        for (const QJsonValue& v : full.value(QStringLiteral("messages")).toArray()) {
            messages.insert(v.toObject().value(QStringLiteral("id")).toInteger(), v.toObject());
        }
    }

    void applyDelta(const QJsonObject& delta)
    {
        if (delta.value(QStringLiteral("room")).toString() != kRoom) {
            return;
        }
        for (const QJsonValue& v : delta.value(QStringLiteral("members")).toArray()) {
            members.insert(v.toObject().value(QStringLiteral("ref")).toString(), v.toObject());
        }
        for (const QJsonValue& v : delta.value(QStringLiteral("left")).toArray()) {
            members.remove(v.toString());
        }
        for (const QJsonValue& v : delta.value(QStringLiteral("turns")).toArray()) {
            turns.insert(v.toObject().value(QStringLiteral("id")).toInteger(), v.toObject());
        }
        for (const QJsonValue& v : delta.value(QStringLiteral("messages")).toArray()) {
            messages.insert(v.toObject().value(QStringLiteral("id")).toInteger(), v.toObject());
        }
        if (delta.value(QStringLiteral("closed")).toBool()) {
            open = false;
        }
    }

    QJsonObject row(const QString& callsign) const
    {
        for (const QJsonObject& m : members) {
            if (m.value(QStringLiteral("callsign")).toString() == callsign) {
                return m;
            }
        }
        return {};
    }

    QJsonObject ownRow() const
    {
        for (const QJsonObject& m : members) {
            if (m.value(QStringLiteral("you")).toBool()) {
                return m;
            }
        }
        return {};
    }

    // The open turn of this kind, or none.
    QJsonObject openTurn(const QString& kind) const
    {
        for (const QJsonObject& t : turns) {
            if (t.value(QStringLiteral("kind")).toString() == kind && t.value(QStringLiteral("end")).isNull()) {
                return t;
            }
        }
        return {};
    }

    bool allMembers(int n) const
    {
        if (members.size() != n) {
            return false;
        }
        for (const QJsonObject& m : members) {
            if (m.value(QStringLiteral("joining")).toBool()) {
                return false;
            }
        }
        return true;
    }
};

struct PostAnswer {
    QString clientId;
    bool ok = false;
    QString detail;
};

// One Core: its key, its client and what its screens would show.
struct Station {
    QString callsign;
    std::unique_ptr<QTemporaryDir> dir;
    std::unique_ptr<StationIdentity> identity;
    std::unique_ptr<RoundtableClient> client;
    RoomView view;
    QList<PostAnswer> posts;
    QList<QUrl> links;
    QList<QJsonObject> mentions;
    QStringList errors;
    bool metered = false;
    double dbm = kQuietDbm;
};

struct HttpAnswer {
    int status = 0;
    QByteArray body;
    QByteArray location;
    QByteArray setCookie;
    QJsonObject json() const { return QJsonDocument::fromJson(body).object(); }
};

QJsonObject trackOf(const QJsonObject& reader, qint64 turnId)
{
    for (const QJsonValue& v : reader.value(QStringLiteral("tracks")).toArray()) {
        if (v.toObject().value(QStringLiteral("turn")).toInteger() == turnId) {
            return v.toObject();
        }
    }
    return {};
}

} // namespace

class TstRoundtableE2e : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanup();
    void cleanupTestCase();

    void readingAppearsInTheTalkersRoomWithinTwoSeconds();
    void unknownTurnOpensFromPresentReadingsAndIsNamed();
    void noDeviceAtTheRadioShowsAwayAndIsRefused();
    void archiveThroughTheSignInLinkListsTheSession();
    void negotiatedFrequencyReadAndGuardedPost();
    void serverKeepsNoAddressOrMessageText();

private:
    std::unique_ptr<Station> makeStation(const QString& callsign, const QString& grid, bool metered, bool atRadio);
    void pushMeters();
    HttpAnswer httpGet(const QString& pathAndQuery, const QByteArray& cookie = {}) const;
    void stopServer();

    QTemporaryDir m_root;
    QProcess m_server;
    quint16 m_wsPort = 0;
    quint16 m_httpPort = 0;
    bool m_serverUp = false;
    QElapsedTimer m_clock;
    qint64 m_meterStartMs = -1;
    QTimer m_meterTimer;
    std::unique_ptr<Station> m_a;
    std::unique_ptr<Station> m_b;
    std::unique_ptr<Station> m_c;
    qint64 m_turnA = 0;
    qint64 m_unknown = 0;
    QStringList m_stationIds;
};

void TstRoundtableE2e::initTestCase()
{
    if (kPython.isEmpty()) {
        const QByteArray why = QFileInfo(QStringLiteral(NEREUS_TEST_PYTHON)).isAbsolute()
            ? QByteArray("No Python 3: NEREUS_TEST_PYTHON is " NEREUS_TEST_PYTHON
                         ", which is not an executable here, so the Roundtable server cannot run")
            : QByteArray("No Python 3: CMake found none (NEREUS_TEST_PYTHON is " NEREUS_TEST_PYTHON
                         ") and no python3 or python is on PATH, so the Roundtable server cannot run");
        QSKIP(why.constData());
    }
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONDONTWRITEBYTECODE"), QStringLiteral("1"));
#if !defined(Q_OS_WIN)
    if (!env.contains(QStringLiteral("PYTHONUSERBASE"))) {
        // ctest gives every test a home of its own, but this Python finds
        // modules installed for the user (pip --user) under the user's own
        // home. Its user base points there, read only; nothing else of
        // that home is used.
        const struct passwd* account = getpwuid(getuid());
        if (account != nullptr && account->pw_dir != nullptr) {
            QProcessEnvironment homeEnv = env;
            homeEnv.insert(QStringLiteral("HOME"), QString::fromLocal8Bit(account->pw_dir));
            QProcess base;
            base.setProcessEnvironment(homeEnv);
            base.start(kPython, {QStringLiteral("-c"), QStringLiteral("import site; print(site.getuserbase())")});
            if (base.waitForFinished(60000) && base.exitStatus() == QProcess::NormalExit && base.exitCode() == 0) {
                const QString userBase = QString::fromUtf8(base.readAllStandardOutput()).trimmed();
                if (!userBase.isEmpty()) {
                    env.insert(QStringLiteral("PYTHONUSERBASE"), userBase);
                }
            }
        }
    }
#endif
    QProcess probe;
    probe.setProcessEnvironment(env);
    probe.start(kPython, {QStringLiteral("-c"), QStringLiteral("import websockets, cryptography")});
    if (!probe.waitForStarted(10000)) {
        const QByteArray why = kPython.toLocal8Bit() + " does not start: " + probe.errorString().toLocal8Bit();
        QSKIP(why.constData());
    }
    QVERIFY(probe.waitForFinished(60000));
    if (probe.exitStatus() != QProcess::NormalExit || probe.exitCode() != 0) {
        const QByteArray why = kPython.toLocal8Bit()
            + " cannot import websockets and cryptography, which the Roundtable server needs: "
            + probe.readAllStandardError().trimmed();
        QSKIP(why.constData());
    }
    QVERIFY(m_root.isValid());
    m_clock.start();

    // Two free loopback ports, held together so they differ.
    {
        QTcpServer ws;
        QTcpServer http;
        QVERIFY(ws.listen(QHostAddress::LocalHost, 0));
        QVERIFY(http.listen(QHostAddress::LocalHost, 0));
        m_wsPort = ws.serverPort();
        m_httpPort = http.serverPort();
    }
    const QString root = m_root.path();
    QVERIFY(QDir(root).mkpath(QStringLiteral("static")));
    QVERIFY(QDir(root).mkpath(QStringLiteral("backup")));
    QFile config(root + QStringLiteral("/roundtable.conf"));
    QVERIFY(config.open(QIODevice::WriteOnly | QIODevice::Text));
    // public_host is what the Cores sign (wire spec 4.3): the host of the
    // URL they connect to, with no port.
    config.write(QStringLiteral("[roundtable]\n"
                                "ws_listen = %1:%2\n"
                                "http_listen = %1:%3\n"
                                "public_host = %1\n"
                                "database = %4/roundtable.db\n"
                                "backup_dir = %4/backup\n"
                                "static_dir = %4/static\n"
                                "log_level = info\n")
                     .arg(kHost)
                     .arg(m_wsPort)
                     .arg(m_httpPort)
                     .arg(root)
                     .toUtf8());
    config.close();

    m_server.setProcessEnvironment(env);
    m_server.setWorkingDirectory(kServerDir);
    m_server.setStandardOutputFile(root + QStringLiteral("/server.out"));
    m_server.setStandardErrorFile(root + QStringLiteral("/server.log"));
    // --stop-on-stdin-eof: stopServer() closes the server's standard input
    // on Windows. Elsewhere it stays open until the server exits.
    m_server.start(kPython, {QStringLiteral("-m"), QStringLiteral("nereus_roundtable"), QStringLiteral("--config"),
                             config.fileName(), QStringLiteral("--stop-on-stdin-eof")});
    QVERIFY2(m_server.waitForStarted(10000), qPrintable(m_server.errorString()));

    QElapsedTimer listening;
    listening.start();
    for (const quint16 port : {m_wsPort, m_httpPort}) {
        bool up = false;
        while (!up && listening.elapsed() < 20000) {
            if (m_server.state() == QProcess::NotRunning) {
                QFile log(root + QStringLiteral("/server.log"));
                const QByteArray text = log.open(QIODevice::ReadOnly) ? log.readAll() : QByteArray();
                QFAIL(qPrintable(QStringLiteral("the server exited: ") + QString::fromUtf8(text)));
            }
            QTcpSocket probeSocket;
            probeSocket.connectToHost(QHostAddress::LocalHost, port);
            up = probeSocket.waitForConnected(300);
            if (!up) {
                QTest::qWait(50);
            }
        }
        QVERIFY2(up, qPrintable(QStringLiteral("the server did not listen on %1 in 20 s (%2)")
                                    .arg(port)
                                    .arg(loadAverageText())));
    }
    m_serverUp = true;
    qInfo("Roundtable server up on 127.0.0.1 after %lld ms", static_cast<long long>(listening.elapsed()));

    m_meterTimer.setInterval(kMeterIntervalMs);
    connect(&m_meterTimer, &QTimer::timeout, this, [this] { pushMeters(); });
}

void TstRoundtableE2e::cleanup()
{
    RealtimeTestLoad::printLoadAverageIfFailed();
}

void TstRoundtableE2e::cleanupTestCase()
{
    m_meterTimer.stop();
    m_a.reset();
    m_b.reset();
    m_c.reset();
    stopServer();
}

void TstRoundtableE2e::stopServer()
{
    if (m_server.state() == QProcess::NotRunning) {
        return;
    }
#if defined(Q_OS_WIN)
    // terminate() posts WM_CLOSE, which a console program never gets; the
    // end of standard input is the server's clean stop here.
    m_server.closeWriteChannel();
#else
    // SIGTERM: the stop systemd sends the live server.
    m_server.terminate();
#endif
    if (!m_server.waitForFinished(10000)) {
        m_server.kill();
        m_server.waitForFinished(5000);
    }
}

std::unique_ptr<Station> TstRoundtableE2e::makeStation(const QString& callsign, const QString& grid, bool metered,
                                                       bool atRadio)
{
    auto s = std::make_unique<Station>();
    s->callsign = callsign;
    s->metered = metered;
    s->dir = std::make_unique<QTemporaryDir>();
    s->identity = std::make_unique<StationIdentity>(StationIdentity::loadOrCreate(s->dir->path()));
    s->client = std::make_unique<RoundtableClient>();
    Station* raw = s.get();
    RoundtableClient* client = s->client.get();
    connect(client, &RoundtableClient::roomReceived, client, [raw](const QJsonObject& m) { raw->view.applyRoom(m); });
    connect(client, &RoundtableClient::roomDeltaReceived, client,
            [raw](const QJsonObject& m) { raw->view.applyDelta(m); });
    connect(client, &RoundtableClient::postResult, client, [raw](const QString& id, bool ok, const QString& detail) {
        raw->posts.append(PostAnswer{id, ok, detail});
    });
    connect(client, &RoundtableClient::webLinkReady, client, [raw](const QUrl& url) { raw->links.append(url); });
    connect(client, &RoundtableClient::mentionReceived, client,
            [raw](const QJsonObject& m) { raw->mentions.append(m); });
    connect(client, &RoundtableClient::errorReceived, client,
            [raw](const QString& code, const QString&) { raw->errors.append(code); });
    client->setServerUrl(QUrl(QStringLiteral("ws://%1:%2/v1/station").arg(kHost).arg(m_wsPort)));
    client->setIdentity(s->identity.get());
    client->setStationInfo(callsign, grid);
    client->setSlices({usbSlice(false)});
    client->setAtRadio(atRadio);
    client->setFirstRunAnswered(true);
    return s;
}

void TstRoundtableE2e::pushMeters()
{
    for (Station* s : {m_a.get(), m_b.get(), m_c.get()}) {
        if (s != nullptr && s->metered) {
            s->client->pushMeter(0, s->dbm, m_clock.nsecsElapsed());
        }
    }
}

HttpAnswer TstRoundtableE2e::httpGet(const QString& pathAndQuery, const QByteArray& cookie) const
{
    QNetworkAccessManager manager;
    manager.setProxy(QNetworkProxy::NoProxy);
    QNetworkRequest request(QUrl(QStringLiteral("http://%1:%2%3").arg(kHost).arg(m_httpPort).arg(pathAndQuery)));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    if (!cookie.isEmpty()) {
        request.setRawHeader("Cookie", cookie);
    }
    QNetworkReply* reply = manager.get(request);
    HttpAnswer answer;
    if (!QTest::qWaitFor([reply] { return reply->isFinished(); }, kWaitMs)) {
        return answer;
    }
    answer.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    answer.body = reply->readAll();
    answer.location = reply->rawHeader("Location");
    answer.setCookie = reply->rawHeader("Set-Cookie");
    return answer;
}

void TstRoundtableE2e::readingAppearsInTheTalkersRoomWithinTwoSeconds()
{
    m_a = makeStation(QStringLiteral("KG4VCF"), QStringLiteral("EM73tu"), false, true);
    m_b = makeStation(QStringLiteral("W4XYZ"), QStringLiteral("FN42"), true, true);
    // C: no device at the radio (design D14).
    m_c = makeStation(QStringLiteral("K4ABC"), QStringLiteral("EM73"), true, false);
    m_meterStartMs = m_clock.elapsed();
    pushMeters();
    m_meterTimer.start();
    for (Station* s : {m_a.get(), m_b.get(), m_c.get()}) {
        s->client->start();
    }
    for (Station* s : {m_a.get(), m_b.get(), m_c.get()}) {
        QTRY_COMPARE_WITH_TIMEOUT(s->client->linkState(), RoundtableClient::LinkState::Live, kWaitMs);
        m_stationIds.append(s->client->stationId());
    }

    // All three become members after the 10 s dwell; the room opens.
    Station* a = m_a.get();
    Station* b = m_b.get();
    QVERIFY2(QTest::qWaitFor([a] { return a->view.open && a->view.allMembers(3); }, kJoinTimeoutMs),
             qPrintable(loadAverageText()));
    QVERIFY(a->view.ownRow().value(QStringLiteral("callsign")).toString() == QLatin1String("KG4VCF"));
    QCOMPARE(a->view.row(QStringLiteral("W4XYZ")).value(QStringLiteral("software")).toString(),
             QStringLiteral("NereusSDR"));

    // B's floor has formed before anyone keys, so its first strong reading
    // is present.
    QVERIFY(QTest::qWaitFor([this] { return m_clock.elapsed() - m_meterStartMs >= kFloorReadyMs; },
                            static_cast<int>(kFloorReadyMs) + kWaitMs));
    QVERIFY(a->view.turns.isEmpty());

    // A keys: a talking turn credited to A.
    a->client->setSlices({usbSlice(true)});
    QVERIFY(QTest::qWaitFor([a] { return !a->view.openTurn(QStringLiteral("talking")).isEmpty(); }, kWaitMs));
    const QJsonObject turn = a->view.openTurn(QStringLiteral("talking"));
    m_turnA = turn.value(QStringLiteral("id")).toInteger();
    const QJsonArray talkers = turn.value(QStringLiteral("talkers")).toArray();
    QCOMPARE(talkers.size(), 1);
    QCOMPARE(talkers.at(0).toObject().value(QStringLiteral("callsign")).toString(), QStringLiteral("KG4VCF"));
    QCOMPARE(talkers.at(0).toObject().value(QStringLiteral("ref")).toString(),
             a->view.ownRow().value(QStringLiteral("ref")).toString());
    QVERIFY(a->view.ownRow().value(QStringLiteral("keyed")).toBool());

    // B's reading, taken now, shows on B's row in A's room with A's turn.
    b->dbm = kSignalDbm;
    const qint64 takenMs = m_clock.elapsed();
    b->client->pushMeter(0, kSignalDbm, m_clock.nsecsElapsed());
    const qint64 turnA = m_turnA;
    auto readOfA = [a, turnA] {
        const QJsonObject reading = a->view.row(QStringLiteral("W4XYZ")).value(QStringLiteral("reading")).toObject();
        return reading.value(QStringLiteral("turn")).toInteger() == turnA
            && reading.value(QStringLiteral("present")).toBool();
    };
    const bool shown = QTest::qWaitFor(readOfA, kWaitMs);
    const qint64 latencyMs = m_clock.elapsed() - takenMs;
    qInfo("B's reading of A shown in A's room after %lld ms (%s)", static_cast<long long>(latencyMs),
          qPrintable(loadAverageText()));
    QVERIFY2(shown, qPrintable(QStringLiteral("B's reading never showed in A's room (%1)").arg(loadAverageText())));
    QVERIFY2(latencyMs <= kReadingWindowMs,
             qPrintable(QStringLiteral("B's reading took %1 ms, over the 2 s window (%2)")
                            .arg(latencyMs)
                            .arg(loadAverageText())));
    const QJsonObject bRow = a->view.row(QStringLiteral("W4XYZ"));
    QVERIFY(!bRow.value(QStringLiteral("km")).isNull());
    QCOMPARE(bRow.value(QStringLiteral("reading")).toObject().value(QStringLiteral("dbm")).toDouble(), kSignalDbm);
    QVERIFY(QTest::qWaitFor([a, turnA] {
        return a->view.turns.value(turnA).value(QStringLiteral("heard")).toInteger() >= 1;
    }, kWaitMs));

    // A unkeys: ending, then the turn ends, still A's.
    a->client->setSlices({usbSlice(false)});
    QVERIFY2(QTest::qWaitFor([a, turnA] { return !a->view.turns.value(turnA).value(QStringLiteral("end")).isNull(); },
                             kTurnTimeoutMs),
             qPrintable(loadAverageText()));
    const QJsonObject ended = a->view.turns.value(turnA);
    QCOMPARE(ended.value(QStringLiteral("kind")).toString(), QStringLiteral("talking"));
    QCOMPARE(ended.value(QStringLiteral("talkers")).toArray().at(0).toObject().value(QStringLiteral("callsign")).toString(),
             QStringLiteral("KG4VCF"));
}

void TstRoundtableE2e::unknownTurnOpensFromPresentReadingsAndIsNamed()
{
    QVERIFY(m_turnA > 0);
    Station* a = m_a.get();
    Station* b = m_b.get();
    // B is still reading a present signal with nobody keyed.
    QVERIFY2(QTest::qWaitFor([b] { return !b->view.openTurn(QStringLiteral("unknown")).isEmpty(); }, kTurnTimeoutMs),
             qPrintable(loadAverageText()));
    m_unknown = b->view.openTurn(QStringLiteral("unknown")).value(QStringLiteral("id")).toInteger();
    QVERIFY(m_unknown > m_turnA);
    const qint64 unknown = m_unknown;
    QVERIFY(b->view.turns.value(unknown).value(QStringLiteral("talkers")).toArray().isEmpty());

    // The signal goes; the turn goes quiet.
    b->dbm = kQuietDbm;
    QVERIFY2(QTest::qWaitFor([b, unknown] { return !b->view.turns.value(unknown).value(QStringLiteral("end")).isNull(); },
                             kTurnTimeoutMs),
             qPrintable(loadAverageText()));

    b->client->nameTurn(QString::number(unknown), kName);
    const QJsonArray named{QJsonObject{{QStringLiteral("name"), kName}, {QStringLiteral("count"), 1}}};
    QVERIFY(QTest::qWaitFor([b, unknown, named] {
        return b->view.turns.value(unknown).value(QStringLiteral("names")).toArray() == named;
    }, kWaitMs));
    QCOMPARE(b->view.turns.value(unknown).value(QStringLiteral("mine")).toString(), kName);
    QVERIFY(QTest::qWaitFor([a, unknown, named] {
        return a->view.turns.value(unknown).value(QStringLiteral("names")).toArray() == named;
    }, kWaitMs));
    QVERIFY(a->view.turns.value(unknown).value(QStringLiteral("mine")).isNull());

    // A posts; B, whom it names, gets the mention.
    a->client->post(kRoom, kPostText, QStringLiteral("a-1"));
    QVERIFY(QTest::qWaitFor([a] { return !a->posts.isEmpty(); }, kWaitMs));
    QVERIFY2(a->posts.first().ok, qPrintable(a->posts.first().detail));
    QVERIFY(QTest::qWaitFor([b] { return !b->mentions.isEmpty(); }, kWaitMs));
    QCOMPARE(b->mentions.first().value(QStringLiteral("message")).toObject().value(QStringLiteral("text")).toString(),
             kPostText);
}

void TstRoundtableE2e::noDeviceAtTheRadioShowsAwayAndIsRefused()
{
    QVERIFY(m_unknown > 0);
    Station* b = m_b.get();
    Station* c = m_c.get();
    QVERIFY(b->view.row(QStringLiteral("K4ABC")).value(QStringLiteral("away")).toBool());
    QVERIFY(!b->view.row(QStringLiteral("KG4VCF")).value(QStringLiteral("away")).toBool());
    QVERIFY(c->view.ownRow().value(QStringLiteral("away")).toBool());
    QVERIFY(!c->view.ownRow().value(QStringLiteral("joining")).toBool());

    c->client->post(kRoom, kAwayText, QStringLiteral("c-1"));
    QVERIFY(QTest::qWaitFor([c] { return !c->posts.isEmpty(); }, kWaitMs));
    QCOMPARE(c->posts.first().clientId, QStringLiteral("c-1"));
    QVERIFY(!c->posts.first().ok);
    QCOMPARE(c->posts.first().detail, kAwayReason);

    c->client->nameTurn(QString::number(m_unknown), kName);
    QVERIFY(QTest::qWaitFor([c] { return c->errors.contains(QStringLiteral("away")); }, kWaitMs));
}

void TstRoundtableE2e::archiveThroughTheSignInLinkListsTheSession()
{
    QVERIFY(m_turnA > 0);
    Station* a = m_a.get();
    a->client->requestWebLink();
    QVERIFY(QTest::qWaitFor([a] { return !a->links.isEmpty(); }, kWaitMs));
    const QUrl link = a->links.first();
    QCOMPARE(link.scheme(), QStringLiteral("https"));
    QCOMPARE(link.host(), kHost);
    QCOMPARE(link.path(), QStringLiteral("/signin"));
    const QString token = QUrlQuery(link).queryItemValue(QStringLiteral("t"));
    QCOMPARE(token.size(), 43);

    // The link swaps for a session cookie, once.
    const HttpAnswer signin = httpGet(QStringLiteral("/signin?t=") + token);
    QCOMPARE(signin.status, 303);
    QCOMPARE(signin.location, QByteArray("/archive"));
    QVERIFY(signin.setCookie.startsWith("rt_session="));
    const QByteArray cookie = signin.setCookie.split(';').first();
    QCOMPARE(httpGet(QStringLiteral("/signin?t=") + token).status, 403);

    const HttpAnswer me = httpGet(QStringLiteral("/v1/me"), cookie);
    QCOMPARE(me.status, 200);
    const QJsonObject station = me.json().value(QStringLiteral("station")).toObject();
    QCOMPARE(station.value(QStringLiteral("callsign")).toString(), QStringLiteral("KG4VCF"));
    QCOMPARE(station.value(QStringLiteral("kind")).toString(), QStringLiteral("core"));

    const HttpAnswer list = httpGet(QStringLiteral("/v1/me/sessions"), cookie);
    QCOMPARE(list.status, 200);
    const QJsonArray sessions = list.json().value(QStringLiteral("sessions")).toArray();
    QCOMPARE(sessions.size(), 1);
    const QJsonObject summary = sessions.at(0).toObject();
    QCOMPARE(summary.value(QStringLiteral("room")).toString(), kRoom);
    QCOMPARE(summary.value(QStringLiteral("turns")).toInteger(), 1);
    QCOMPARE(summary.value(QStringLiteral("reported")).toInteger(), 2);

    const qint64 sessionId = summary.value(QStringLiteral("id")).toInteger();
    const HttpAnswer one = httpGet(QStringLiteral("/v1/me/session/%1").arg(sessionId), cookie);
    QCOMPARE(one.status, 200);
    const QJsonObject detail = one.json();
    const QJsonArray turns = detail.value(QStringLiteral("turns")).toArray();
    QCOMPARE(turns.size(), 1);
    QCOMPARE(turns.at(0).toObject().value(QStringLiteral("id")).toInteger(), m_turnA);
    QJsonObject readerB;
    QJsonObject readerC;
    for (const QJsonValue& v : detail.value(QStringLiteral("readers")).toArray()) {
        const QString call = v.toObject().value(QStringLiteral("callsign")).toString();
        if (call == QLatin1String("W4XYZ")) {
            readerB = v.toObject();
        } else if (call == QLatin1String("K4ABC")) {
            readerC = v.toObject();
        }
    }
    QVERIFY2(!readerB.isEmpty(), one.body.constData());
    const QJsonObject track = trackOf(readerB, m_turnA);
    QVERIFY(!track.isEmpty());
    QVERIFY(!track.value(QStringLiteral("away")).toBool());
    QVERIFY(track.value(QStringLiteral("present")).toArray().contains(QJsonValue(true)));
    QVERIFY(readerB.value(QStringLiteral("watched")).toInteger() >= 1);
    QVERIFY(!readerB.value(QStringLiteral("km")).isNull());
    QVERIFY2(!readerC.isEmpty(), one.body.constData());
    QVERIFY(readerC.value(QStringLiteral("away")).toBool());
}

void TstRoundtableE2e::negotiatedFrequencyReadAndGuardedPost()
{
    QVERIFY(m_a && m_b);
    RoundtableClient* sender = m_a->client.get();
    RoundtableClient* reader = m_b->client.get();
    QCOMPARE(sender->frequencyVersion(), 1);
    QCOMPARE(reader->frequencyVersion(), 1);
    QSignalSpy full(reader, &RoundtableClient::roomReceived);
    QSignalSpy delta(reader, &RoundtableClient::roomDeltaReceived);
    QSignalSpy posted(sender, &RoundtableClient::postResult);
    // Reading at the inclusive boundary crosses RF-room IDs without tuning.
    const qint64 contextHz = kHz + 5000;
    const QString context = QStringLiteral("usb-%1").arg(contextHz);
    QVERIFY(reader->readAt(contextHz, QStringLiteral("usb")));
    const auto frequencyFull = [&full, &context]() {
        for (const QList<QVariant>& args : full) {
            const QJsonObject value = args.first().toJsonObject();
            if (value.value(QStringLiteral("view")).toString() == QLatin1String("frequency")
                && value.value(QStringLiteral("id")).toString() == context) { return true; }
        }
        return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT(frequencyFull(), kWaitMs);
    const QString text = QStringLiteral("Exact-frequency integration check");
    sender->postAt(0, kHz, QStringLiteral("usb"), text, QStringLiteral("frequency-valid"));
    QTRY_COMPARE_WITH_TIMEOUT(posted.size(), 1, kWaitMs);
    QVERIFY(posted.first().at(1).toBool());
    const auto delivered = [&delta, &context, &text]() {
        for (const QList<QVariant>& args : delta) {
            const QJsonObject packet = args.first().toJsonObject();
            if (packet.value(QStringLiteral("view")).toString() != QLatin1String("frequency")
                || packet.value(QStringLiteral("room")).toString() != context) { continue; }
            for (const QJsonValue& item : packet.value(QStringLiteral("messages")).toArray()) {
                const QJsonObject message = item.toObject();
                if (message.value(QStringLiteral("text")).toString() == text
                    && message.value(QStringLiteral("hz")).toInteger() == kHz
                    && message.value(QStringLiteral("family")).toString() == QLatin1String("usb")
                    && message.value(QStringLiteral("sourceRoom")).toString() == kRoom) { return true; }
            }
        }
        return false;
    };
    QTRY_VERIFY_WITH_TIMEOUT(delivered(), kWaitMs);
    sender->postAt(0, kHz + 1, QStringLiteral("usb"), text, QStringLiteral("frequency-stale"));
    QTRY_COMPARE_WITH_TIMEOUT(posted.size(), 2, kWaitMs);
    QCOMPARE(posted.last().at(0).toString(), QStringLiteral("frequency-stale"));
    QVERIFY(!posted.last().at(1).toBool());
    reader->unreadAt(contextHz, QStringLiteral("usb"));
    // A retained received ID remains checkable without reopening its view.
    const QString messageId = posted.first().at(2).toString();
    QVERIFY(messageId.toLongLong() > 0);
    QSignalSpy history(reader, &RoundtableClient::historyResult);
    QVERIFY(reader->requestHistory(QJsonArray{messageId.toLongLong()}, QStringLiteral("history-shown")));
    QTRY_COMPARE_WITH_TIMEOUT(history.size(), 1, kWaitMs);
    QVERIFY(history.first().at(1).toBool());
    QCOMPARE(history.first().at(2).toJsonArray().first().toObject().value(QStringLiteral("text")).toString(), text);
    sender->deleteMessage(messageId);
    QTRY_COMPARE_WITH_TIMEOUT(m_b->view.messages.value(messageId.toLongLong()).value(QStringLiteral("state")).toString(),
                              QStringLiteral("deleted"), kWaitMs);
    QVERIFY(reader->requestHistory(QJsonArray{messageId.toLongLong()}, QStringLiteral("history-deleted")));
    QTRY_COMPARE_WITH_TIMEOUT(history.size(), 2, kWaitMs);
    QVERIFY(history.last().at(1).toBool());
    const QJsonObject removed = history.last().at(2).toJsonArray().first().toObject();
    QCOMPARE(removed.value(QStringLiteral("state")).toString(), QStringLiteral("deleted"));
    QVERIFY(removed.value(QStringLiteral("text")).isNull());
}

void TstRoundtableE2e::serverKeepsNoAddressOrMessageText()
{
    QVERIFY(m_serverUp);
    m_meterTimer.stop();
    m_a.reset();
    m_b.reset();
    m_c.reset();
    stopServer();
    QCOMPARE(m_server.exitStatus(), QProcess::NormalExit);
    QCOMPARE(m_server.exitCode(), 0);

    QFile logFile(m_root.path() + QStringLiteral("/server.log"));
    QVERIFY(logFile.open(QIODevice::ReadOnly));
    const QByteArray log = logFile.readAll();
    QVERIFY2(!log.contains("Traceback"), log.constData());
    QVERIFY2(log.contains("signed in (core)"), log.constData());
#if defined(Q_OS_WIN)
    // The clean stop came through standard input, not the kill() fallback.
    QVERIFY2(log.contains("standard input closed"), log.constData());
#endif
    QStringList forbidden{kHost, QStringLiteral("KG4VCF"), QStringLiteral("W4XYZ"), QStringLiteral("K4ABC"), kPostText,
                          kAwayText, QStringLiteral("Exact-frequency integration check")};
    forbidden.append(m_stationIds);
    for (const QString& word : std::as_const(forbidden)) {
        QVERIFY2(!word.isEmpty(), "a station had no id");
        QVERIFY2(!log.contains(word.toUtf8()), qPrintable(QStringLiteral("the log holds %1").arg(word)));
    }
    const QStringList files = QDir(m_root.path()).entryList({QStringLiteral("roundtable.db*")}, QDir::Files);
    QVERIFY(files.contains(QStringLiteral("roundtable.db")));
    for (const QString& name : files) {
        QFile file(m_root.path() + QLatin1Char('/') + name);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY2(!file.readAll().contains(kHost.toUtf8()), qPrintable(name + QStringLiteral(" holds an address")));
    }
}

QTEST_MAIN(TstRoundtableE2e)
#include "tst_roundtable_e2e.moc"
