// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_station_status_page.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 17 (R-IOS-08; spec section 5.3 item 11; pairing
// design sections 4.2 and 4.3): the Core's read-only status page.
//
// Invariants first, each with a red check recorded in task-17-report.md:
//
//   - the code is on the page only while the Core is unclaimed: never on
//     a claimed Core, never while its window is reopened for one more
//     device, never on an upgraded Core still claimed through its token;
//   - no device name, device id or address is ever on it;
//   - a POST, any other method and any other path get 404 and change
//     nothing;
//   - a peer that is not on a directly connected network gets no page and
//     no reply at all;
//   - a Host that is not an address or this computer's name gets 404;
//   - the code never reaches a log line.
//
// Then what it says: the label, the radio (model and name) or off, whether
// a device has paired, "Core" for the computer, plain words
// (OperatorWording::isPlain), a refresh every 5 seconds, and the first
// start's notice.
//
// Then the bind (R-R3-26): a Core's page listens exactly where its remote
// listener does, DaemonConfig::listenAddressFor(remote_bind); with "::" it
// answers over IPv4 and IPv6.
//
// The page is served on loopback, except the "::" case, which binds every
// address as the listener's own dual-stack test does. Keys are made at run
// time in scratch directories.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-24: the page binds where the listener binds, dual stack for
//               "::" (R-R3-26). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-24: Part C fix wave (R1-I2): a reopened pairing window
//               lasts 10 minutes, five burned codes in a row close any window,
//               and reopening starts afresh. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24: Part C fix wave (R2-I1): the Host check accepts
//               only this computer's own names, whole. J.J. Boyd (KG4VCF), with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-09-30: fix wave (INFRA-I4): a page that leaves the connection
//               open is reported by name instead of passing silently into
//               the status check. J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
// =================================================================

#include <QtTest>

#include <QHostInfo>
#include <QMutex>
#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDir>
#include <QTemporaryDir>

#include <memory>

#include "core/AppSettings.h"
#include "core/daemon/DaemonApp.h"
#include "core/daemon/DaemonConfig.h"
#include "core/daemon/StationStatusPage.h"
#include "core/security/CertificateStore.h"
#include "core/security/DeviceStore.h"
#include "core/security/PairingWindow.h"
#include "core/security/StationIdentity.h"
#include "core/session/StationServer.h"
#include "models/RadioModel.h"

#include "OperatorWording.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;

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

    PairedDevice record(const QString& name) const
    {
        PairedDevice device;
        device.id = key.fingerprint();
        device.publicKeySpki = key.publicKeySpki();
        device.name = name;
        device.kind = QStringLiteral("phone");
        return device;
    }
};

// One Core and its page on loopback, in scratch directories.
struct Core {
    QTemporaryDir settingsDir;
    QTemporaryDir securityDir;
    std::unique_ptr<AppSettings> settings;
    std::unique_ptr<RadioModel> model;
    std::unique_ptr<StationServer> server;
    std::unique_ptr<StationStatusPage> page;
    StationRadioStatus radio{true, QStringLiteral("Hermes Lite 2"), QStringLiteral("Bench HL2")};
    bool remoteOn = true;

    explicit Core(bool upgradedWithToken = false)
    {
        settings = std::make_unique<AppSettings>(
            settingsDir.filePath(QStringLiteral("NereusSDR.settings")));
        model = std::make_unique<RadioModel>();
        const QString dir = upgradedWithToken
                                ? NereusSDR::Test::seedUpgradedCoreToken(securityDir.path())
                                : NereusSDR::Test::seedCoreIdentity(securityDir.path());
        server = std::make_unique<StationServer>(model.get(), *settings, dir);
        server->setHeartbeatIntervalMs(0);
        StationStatusPage::Sources sources;
        sources.server = [this]() { return remoteOn ? server.get() : nullptr; };
        sources.radio = [this]() { return radio; };
        sources.label = []() { return QStringLiteral("Shack Core"); };
        page = std::make_unique<StationStatusPage>(std::move(sources));
    }

    ~Core()
    {
        page.reset();
        server.reset();
    }

    bool listen() { return page->listen(QHostAddress::LocalHost, 0); }

    PairingWindow& window() const { return *server->pairingWindow(); }

    // Sends `request` and returns everything the page wrote before it
    // closed the connection.
    QByteArray fetch(const QByteArray& request) const
    {
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, page->serverPort());
        if (!QTest::qWaitFor([&socket]() {
                return socket.state() == QAbstractSocket::ConnectedState;
            }, 5000)) {
            return {};
        }
        socket.write(request);
        QByteArray received;
        if (!QTest::qWaitFor([&socket, &received]() {
                received += socket.readAll();
                return socket.state() == QAbstractSocket::UnconnectedState;
            }, 5000)) {
            qWarning() << "fetch: the page did not close the connection within 5 s";
        }
        received += socket.readAll();
        return received;
    }

    QByteArray get(const QByteArray& path = "/", const QByteArray& host = "127.0.0.1") const
    {
        return fetch("GET " + path + " HTTP/1.1\r\nHost: " + host + "\r\n\r\n");
    }
};

int statusOf(const QByteArray& reply)
{
    const QList<QByteArray> first = reply.left(reply.indexOf("\r\n")).split(' ');
    return first.size() >= 2 ? first.at(1).toInt() : 0;
}

QString bodyOf(const QByteArray& reply)
{
    return QString::fromUtf8(reply.mid(reply.indexOf("\r\n\r\n") + 4));
}

// What a reader sees: the page's text without its markup.
QString visibleText(const QString& html)
{
    QString text = html;
    text.remove(QRegularExpression(QStringLiteral("<style>.*</style>")));
    text.replace(QRegularExpression(QStringLiteral("<[^>]*>")), QStringLiteral(" "));
    return text.simplified();
}


int freePort(const QHostAddress& address)
{
    QTcpServer probe;
    if (!probe.listen(address, 0)) {
        return 0;
    }
    const int port = probe.serverPort();
    probe.close();
    return port;
}

// A GET over `address`; the status code, 0 when nothing answered.
int fetchStatus(const QHostAddress& address, quint16 port)
{
    QTcpSocket socket;
    socket.connectToHost(address, port);
    if (!socket.waitForConnected(3000)) {
        return 0;
    }
    const QString host = address.protocol() == QAbstractSocket::IPv6Protocol
                             ? QStringLiteral("[%1]").arg(address.toString())
                             : address.toString();
    socket.write("GET / HTTP/1.1\r\nHost: " + host.toLatin1() + "\r\n\r\n");
    QByteArray received;
    if (!QTest::qWaitFor([&socket, &received]() {
            received += socket.readAll();
            return socket.state() == QAbstractSocket::UnconnectedState;
        }, 5000)) {
        qWarning() << "fetchStatus: the page did not close the connection within 5 s";
    }
    received += socket.readAll();
    return statusOf(received);
}

// A DaemonApp's page on `remoteBind`, its listener on a port of its own.
struct DaemonCore {
    DaemonApp app;
    DaemonConfig config = DaemonConfig::defaults();

    explicit DaemonCore(const QString& remoteBind)
    {
        QDir(CertificateStore::defaultDirectory()).removeRecursively();
        NereusSDR::Test::seedCoreIdentity(CertificateStore::defaultDirectory());
        const QHostAddress probe = DaemonConfig::listenAddressFor(remoteBind);
        config.remoteBind = remoteBind;
        config.remotePort = freePort(probe);
        config.statusPort = freePort(probe);
        app.primeBoardForTest(HPSDRHW::HermesLite);
    }

    ~DaemonCore()
    {
        app.stop();
        QDir(CertificateStore::defaultDirectory()).removeRecursively();
    }
};

} // namespace

class TstStationStatusPage : public QObject {
    Q_OBJECT

private slots:
    // ── Where it listens (R-R3-26) ────────────────────────────────────

    void initTestCase()
    {
        // The daemon cases keep the Core's identity in a profile of this
        // test's own.
        AppSettings::setProfileOverride(
            QStringLiteral("tssp-%1").arg(QCoreApplication::applicationPid()));
    }

    void cleanupTestCase()
    {
        QDir(AppSettings::resolveConfigDir(AppSettings::profileOverride())).removeRecursively();
    }

    void thePageBindsWhereTheListenerBinds_data()
    {
        QTest::addColumn<QString>("remoteBind");
        QTest::newRow("127.0.0.1") << QStringLiteral("127.0.0.1");
        QTest::newRow("::1") << QStringLiteral("::1");
    }

    void thePageBindsWhereTheListenerBinds()
    {
        QFETCH(QString, remoteBind);
        const QHostAddress expected = DaemonConfig::listenAddressFor(remoteBind);
        if (freePort(expected) == 0) {
            QSKIP(qPrintable(QStringLiteral("This host cannot listen on %1.").arg(remoteBind)));
        }
        DaemonCore core(remoteBind);
        QVERIFY(core.app.start(core.config));
        StationStatusPage* page = core.app.statusPage();
        QVERIFY2(page != nullptr, "the status page did not start");
        QCOMPARE(page->serverAddress(), expected);
        QCOMPARE(page->serverAddress(), DaemonApp::listenerAddressFor(core.config.remoteBind));
        QCOMPARE(fetchStatus(expected, page->serverPort()), 200);
        // Bound to one address: nothing on the other family's loopback.
        const QHostAddress other = expected.protocol() == QAbstractSocket::IPv4Protocol
                                       ? QHostAddress(QHostAddress::LocalHostIPv6)
                                       : QHostAddress(QHostAddress::LocalHost);
        QCOMPARE(fetchStatus(other, page->serverPort()), 0);
    }

    void aDualStackBindAnswersIpv4AndIpv6()
    {
        DaemonCore core(QStringLiteral("::"));
        QVERIFY(core.app.start(core.config));
        StationStatusPage* page = core.app.statusPage();
        QVERIFY2(page != nullptr, "the status page did not start");
        QCOMPARE(page->serverAddress(), DaemonConfig::listenAddressFor(QStringLiteral("::")));
        QCOMPARE(page->serverAddress(), QHostAddress(QHostAddress::Any));
        QCOMPARE(fetchStatus(QHostAddress(QHostAddress::LocalHost), page->serverPort()), 200);
        QTcpServer ipv6Probe;
        if (!ipv6Probe.listen(QHostAddress::LocalHostIPv6, 0)) {
            QSKIP("This host has no IPv6 loopback; the IPv4 half passed.");
        }
        ipv6Probe.close();
        QCOMPARE(fetchStatus(QHostAddress(QHostAddress::LocalHostIPv6), page->serverPort()), 200);
    }

    // ── Who sees the code ──────────────────────────────────────────────

    void anUnclaimedCoreShowsItsCode()
    {
        Core core;
        QVERIFY(core.listen());
        QCOMPARE(core.window().state(), PairingWindow::State::OpenUnclaimed);
        const QString code = core.window().currentCode();
        QVERIFY2(!code.isEmpty(), "an unclaimed Core has no code, so this slot proves nothing");
        const QByteArray reply = core.get();
        QCOMPARE(statusOf(reply), 200);
        QVERIFY(bodyOf(reply).contains(code));
        QVERIFY(core.page->showsCode());
    }

    void aClaimedCoreNeverShowsTheCode()
    {
        Core core;
        QVERIFY(core.listen());
        const QString unclaimedCode = core.window().currentCode();
        QVERIFY(!unclaimedCode.isEmpty());
        Device device;
        QVERIFY(core.server->deviceStore()->add(device.record(QStringLiteral("Shack iPhone"))));
        QCOMPARE(core.window().state(), PairingWindow::State::ClosedClaimed);
        QString body = bodyOf(core.get());
        QVERIFY(!body.contains(unclaimedCode));
        QVERIFY(body.contains(QStringLiteral("A device has paired with this Core.")));

        // Reopened for one more device: the code exists, but only the
        // console and paired devices get it.
        core.window().reopen();
        QCOMPARE(core.window().state(), PairingWindow::State::OpenReopened);
        const QString reopenedCode = core.window().currentCode();
        QVERIFY2(!reopenedCode.isEmpty(), "the reopened window has no code to leak");
        body = bodyOf(core.get());
        QVERIFY2(!body.contains(reopenedCode), "the reopened window's code is on the page");
        QVERIFY(!body.contains(reopenedCode.section(QLatin1Char('-'), 1)));
        QVERIFY(!core.page->showsCode());
    }

    void aWindowTheCeilingClosedShowsNoCodeAndSaysHowToReopen()
    {
        // Fix wave R1-I2: five burned codes in a row shut an unclaimed
        // Core's window until its console reopens it.
        Core core;
        QVERIFY(core.listen());
        qint64 clock = 1000000;
        core.window().setClock([&clock] { return clock; });
        QStringList codes;
        for (int i = 0; i < PairingWindow::kMaxConsecutiveFailures; ++i) {
            clock += core.window().retryAfterMs();
            core.window().poll();
            codes << core.window().currentCode();
            QVERIFY(core.window().takeCode(core.window().codeSerial()));
            core.window().pairingFailed();
        }
        QCOMPARE(core.window().state(), PairingWindow::State::ClosedUnclaimed);
        const QString body = bodyOf(core.get());
        QVERIFY(!core.page->showsCode());
        QVERIFY(body.contains(QStringLiteral("nereusd pairing open")));
        for (const QString& code : std::as_const(codes)) {
            QVERIFY(!body.contains(code));
        }
    }

    void anUpgradedCoreClaimedByItsTokenShowsNoCode()
    {
        Core core(/*upgradedWithToken=*/true);
        QVERIFY(core.listen());
        QVERIFY(core.server->deviceStore()->isClaimed());
        // Reopen, so a code exists that could leak.
        core.window().reopen();
        const QString code = core.window().currentCode();
        QVERIFY(!code.isEmpty());
        const QString body = bodyOf(core.get());
        QVERIFY(!body.contains(code));
        QVERIFY(!body.contains(core.server->token()));
        QVERIFY(!core.page->showsCode());
    }

    void noDeviceNameIdOrAddressIsShown()
    {
        Core core;
        QVERIFY(core.listen());
        Device device;
        QVERIFY(core.server->deviceStore()->add(device.record(QStringLiteral("Shack iPhone"))));
        core.server->deviceStore()->touch(device.key.fingerprint(), QStringLiteral("192.0.2.44"));
        const QString body = bodyOf(core.get());
        QVERIFY(!body.contains(QStringLiteral("Shack iPhone")));
        QVERIFY(!body.contains(StationIdentity::toBase64Url(device.key.fingerprint())));
        QVERIFY(!body.contains(StationIdentity::toBase64Url(device.key.publicKeySpki())));
        QVERIFY(!body.contains(QStringLiteral("192.0.2.44")));
        QVERIFY(!body.contains(
            StationIdentity::toBase64Url(core.server->stationIdentity().publicKeySpki())));
    }

    // ── It changes nothing ────────────────────────────────────────────

    void everythingButGetSlashIsNotFoundAndChangesNothing()
    {
        Core core;
        QVERIFY(core.listen());
        const QString code = core.window().currentCode();
        const quint64 serial = core.window().codeSerial();
        const QList<QByteArray> requests{
            "POST / HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 4\r\n\r\nopen",
            "PUT / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
            "DELETE / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
            "HEAD / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
            "GET /pair HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
            "GET /?open=1 HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
            "GET /../ HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n",
            "GET /\r\n\r\n",
        };
        for (const QByteArray& request : requests) {
            const QByteArray reply = core.fetch(request);
            QVERIFY2(statusOf(reply) == 404, request.constData());
            QVERIFY2(!reply.contains(code.toUtf8()), request.constData());
        }
        QCOMPARE(core.window().state(), PairingWindow::State::OpenUnclaimed);
        QCOMPARE(core.window().currentCode(), code);
        QCOMPARE(core.window().codeSerial(), serial);
        QVERIFY(core.server->deviceStore()->list().isEmpty());
    }

    void thePageHasNothingThatActs()
    {
        Core core;
        QVERIFY(core.listen());
        const QByteArray reply = core.get();
        const QString body = bodyOf(reply);
        QVERIFY(!body.contains(QStringLiteral("<form"), Qt::CaseInsensitive));
        QVERIFY(!body.contains(QStringLiteral("<script"), Qt::CaseInsensitive));
        QVERIFY(!body.contains(QStringLiteral("href"), Qt::CaseInsensitive));
        QVERIFY(reply.contains("Cache-Control: no-store"));
        QVERIFY(reply.contains("Content-Security-Policy: default-src 'none'"));
    }

    // ── Who gets a page ───────────────────────────────────────────────

    void aPeerOffTheDirectNetworksGetsNoPage()
    {
        Core core;
        QList<QHostAddress> asked;
        core.page->setPeerCheck([&asked](const QHostAddress& peer) {
            asked.append(peer);
            return false;  // as a peer elsewhere would be
        });
        QVERIFY(core.listen());
        const QByteArray reply = core.get();
        QVERIFY2(reply.isEmpty(), "a peer elsewhere got a reply");
        QCOMPARE(asked.size(), 1);

        // The rule itself: a documentation address is on no network here,
        // this computer is.
        QVERIFY(!StationServer::isOnDirectNetwork(QStringLiteral("203.0.113.9")));
        QVERIFY(StationServer::isOnDirectNetwork(QStringLiteral("127.0.0.1")));
    }

    void aForeignHostNameGetsNotFound()
    {
        Core core;
        QVERIFY(core.listen());
        const QString code = core.window().currentCode();
        QByteArray reply = core.get("/", "attacker.example");
        QCOMPARE(statusOf(reply), 404);
        QVERIFY(!reply.contains(code.toUtf8()));
        reply = core.get("/", "127.0.0.1:" + QByteArray::number(core.page->serverPort()));
        QCOMPARE(statusOf(reply), 200);
        reply = core.get("/", "[::1]:47911");
        QCOMPARE(statusOf(reply), 200);
        QVERIFY(StationStatusPage::isAcceptedHost(QHostInfo::localHostName()));
        QVERIFY(StationStatusPage::isAcceptedHost(QHostInfo::localHostName()
                                                  + QStringLiteral(".local:47911")));
        QVERIFY(!StationStatusPage::isAcceptedHost(QStringLiteral("nereus.attacker.example")));
    }

    void aNameThatOnlyStartsWithThisComputersNameGetsNotFound()
    {
        // Fix wave R2-I1: a rebinding page on <own name>.attacker.example
        // would otherwise pass as this computer and read the code.
        const QString full = QHostInfo::localHostName();
        const QString first = full.section(QLatin1Char('.'), 0, 0);
        QVERIFY(!first.isEmpty());
        for (const QString& own : {full, first}) {
            QVERIFY2(!StationStatusPage::isAcceptedHost(own + QStringLiteral(".attacker.example")),
                     qPrintable(own));
            QVERIFY(!StationStatusPage::isAcceptedHost(own + QStringLiteral(".attacker.example:47911")));
            QVERIFY(!StationStatusPage::isAcceptedHost(own + QStringLiteral(".lan.attacker.example")));
            QVERIFY(!StationStatusPage::isAcceptedHost(QStringLiteral("x") + own));
        }
        // These three names are this computer's own.
        QVERIFY(StationStatusPage::isAcceptedHost(full));
        QVERIFY(StationStatusPage::isAcceptedHost(full.toUpper() + QStringLiteral(":47911")));
        QVERIFY(StationStatusPage::isAcceptedHost(full + QStringLiteral(".local")));
        const QString domain = QHostInfo::localDomainName();
        if (!domain.isEmpty()) {
            QVERIFY(StationStatusPage::isAcceptedHost(full + QLatin1Char('.') + domain));
        }

        Core core;
        QVERIFY(core.listen());
        const QString code = core.window().currentCode();
        const QByteArray reply =
            core.get("/", (first + QStringLiteral(".attacker.example:47911")).toUtf8());
        QCOMPARE(statusOf(reply), 404);
        QVERIFY(!reply.contains(code.toUtf8()));
    }

    void theCodeNeverReachesALogLine()
    {
        {
            QMutexLocker lock(&g_logMutex);
            g_log.clear();
        }
        g_previousHandler = qInstallMessageHandler(captureLog);
        QString code;
        {
            Core core;
            if (core.listen()) {
                code = core.window().currentCode();
                core.get();
                core.fetch("POST / HTTP/1.1\r\nHost: 127.0.0.1\r\n\r\n");
            }
        }
        qInstallMessageHandler(g_previousHandler);
        QVERIFY(!code.isEmpty());
        QMutexLocker lock(&g_logMutex);
        for (const QString& line : std::as_const(g_log)) {
            QVERIFY2(!line.contains(code), "a log line holds the pairing code");
            QVERIFY2(!line.contains(code.section(QLatin1Char('-'), 1)),
                     "a log line holds the pairing code's words");
        }
    }

    // ── What it says ──────────────────────────────────────────────────

    void itShowsTheLabelTheRadioAndRefreshes()
    {
        Core core;
        QVERIFY(core.listen());
        QString body = bodyOf(core.get());
        QVERIFY(body.contains(QStringLiteral("Shack Core")));
        QVERIFY(body.contains(QStringLiteral("Radio: Hermes Lite 2, Bench HL2, connected.")));
        QVERIFY(body.contains(QStringLiteral("<meta http-equiv=\"refresh\" content=\"5\">")));
        QVERIFY(body.contains(QStringLiteral("No device is paired with this Core.")));

        core.radio = StationRadioStatus{};
        body = bodyOf(core.get());
        QVERIFY(body.contains(QStringLiteral("Radio: off.")));

        core.remoteOn = false;
        body = bodyOf(core.get());
        QVERIFY(body.contains(QStringLiteral("Remote access is off on this Core.")));
    }

    void aLabelIsShownAsTextNotMarkup()
    {
        Core core;
        core.radio.name = QStringLiteral("<b>HL2</b>");
        QVERIFY(core.listen());
        const QString body = bodyOf(core.get());
        QVERIFY(!body.contains(QStringLiteral("<b>HL2</b>")));
        QVERIFY(body.contains(QStringLiteral("&lt;b&gt;HL2&lt;/b&gt;")));
    }

    void theWordsArePlainAndSayCore()
    {
        Core core;
        QVERIFY(core.listen());
        QStringList texts;
        // The code is random words, not wording: left out of the check.
        texts << visibleText(bodyOf(core.get()).remove(core.window().currentCode()));
        Device device;
        QVERIFY(core.server->deviceStore()->add(device.record(QStringLiteral("Shack iPhone"))));
        texts << visibleText(bodyOf(core.get()));
        core.radio = StationRadioStatus{};
        core.remoteOn = false;
        texts << visibleText(bodyOf(core.get()));
        texts << StationStatusPage::formatFirstStartNotice(
            QStringLiteral("Shack Core"), QStringLiteral("http://192.0.2.5:47911/"));
        for (const QString& text : std::as_const(texts)) {
            QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
            QVERIFY2(OperatorWording::coreCalledStationIn(text).isEmpty(), qPrintable(text));
            QVERIFY2(text.contains(QStringLiteral("Core")), qPrintable(text));
        }
    }

    void theFirstStartNoticeNamesTheLabelAndThePage()
    {
        const QString notice = StationStatusPage::formatFirstStartNotice(
            QStringLiteral("Shack Core"), QStringLiteral("http://192.0.2.5:47911/"));
        QVERIFY(notice.contains(QStringLiteral("This Core: Shack Core")));
        QVERIFY(notice.contains(QStringLiteral("Status page: http://192.0.2.5:47911/")));
        QVERIFY(notice.contains(QStringLiteral("sudo nereusd status")));
        // The identity key banner beside it names the key's full path and
        // asks for a backup (StationServer::formatFirstRunBanner).
        const QString banner = StationServer::formatFirstRunBanner(
            QStringLiteral("AA:BB"), QStringLiteral("/var/lib/nereusd/station-identity.pem"));
        QVERIFY(banner.contains(QStringLiteral("/var/lib/nereusd/station-identity.pem")));
        QVERIFY(banner.contains(QStringLiteral("Back up")));
        // The page's address: never loopback, which no other computer
        // reaches.
        QVERIFY(StationStatusPage::addressForOperator(QHostAddress::LocalHost, 47911).isEmpty());
        QCOMPARE(StationStatusPage::addressForOperator(QHostAddress(QStringLiteral("192.0.2.5")),
                                                       47911),
                 QStringLiteral("http://192.0.2.5:47911/"));
    }
};

QTEST_MAIN(TstStationStatusPage)
#include "tst_station_status_page.moc"
