// no-port-check: NereusSDR-original. R-R3-38 stop message of a remote window.
//
// R-R3-21 / R-R3-23 / R-R3-38, Task 5 of the R3 completion plan: when the
// Core ends a remote window for a reason that will not fix itself, the
// window stays as it is, says what happened in plain words over its
// content, offers the next steps as buttons and does not retry. A dropped
// link still retries as before. Each case runs a real remote MainWindow
// against a loopback Core: the real StationServer for the takeover and
// both version refusals (review finding I1: the reasons come from the
// Core's own code, never a copied string), and a scripted Core that speaks
// the link's own messages for any other refusal and for a dropped link.
//
// iPhone app Task 71: a Core no longer ends one window's session when
// another signs in, so the takeover's end (its words from
// SessionEndReasons, as before) comes from the relay in front of the real
// Core, as an older Core or a later version's fifth-device takeover sends
// it. J.J. Boyd (KG4VCF), 2026-09-25, AI-assisted via Anthropic Claude Code.
//
// iPhone app Task 12: a new Core has no pairing token, so each real Core
// here is the upgraded one that still has it (fakes/UpgradedCoreToken.h),
// and the window signs in with that token as before.
#include <QTest>
#include <QCoreApplication>
#include <QDockWidget>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QWebSocket>
#include <QWebSocketServer>

#include <memory>

#include "OperatorWording.h"
#include "core/AppSettings.h"
#include "core/RadioDiscovery.h"
#include "core/session/LinkVersion.h"
#include "core/session/SessionEndReasons.h"
#include "core/session/SessionMessages.h"
#include "core/session/SessionTransport.h"
#include "core/session/StationClient.h"
#include "core/session/StationServer.h"
#include "core/settings/SettingsProxy.h"
#include "gui/MainWindow.h"
#include "gui/OperatorReasonText.h"
#include "gui/RemoteConnectionController.h"
#include "models/RadioModel.h"
#include "fakes/MainWindowTestSettings.h"
#include "fakes/UpgradedCoreToken.h"

using namespace NereusSDR;

namespace {

class ScopedRemoteBackend final {
public:
    explicit ScopedRemoteBackend(ISettingsBackend* backend)
    {
        AppSettings::instance().setRemoteBackend(backend);
    }
    ~ScopedRemoteBackend() { AppSettings::instance().setRemoteBackend(nullptr); }
};

// A Core that sends its hello on connect and, when told to, one session
// end after the app's hello: the order StationServer refuses in.
class ScriptedCore final : public QObject {
public:
    explicit ScriptedCore(quint16 helloMajor, QString endReason = {}, bool retryable = false,
                          QString endCode = {})
        : m_helloMajor(helloMajor), m_endReason(std::move(endReason)), m_retryable(retryable)
        , m_endCode(std::move(endCode))
    {
        connect(&m_server, &QWebSocketServer::newConnection, this, [this] {
            QWebSocket* socket = m_server.nextPendingConnection();
            socket->setParent(this);
            ++connections;
            socket->sendTextMessage(QString::fromUtf8(SessionMessages::encode(
                SessionMessages::hello(m_helloMajor, kSessionProtocolMinor, 7,
                                       QStringLiteral("scripted core")))));
            connect(socket, &QWebSocket::textMessageReceived, this,
                    [this, socket](const QString&) {
                if (dropWithoutEnd && !socket->property("ended").toBool()) {
                    // The link just goes: no session end, no close frame.
                    socket->setProperty("ended", true);
                    socket->abort();
                    return;
                }
                if (m_endReason.isEmpty() || socket->property("ended").toBool()) { return; }
                socket->setProperty("ended", true);
                socket->sendTextMessage(QString::fromUtf8(SessionMessages::encode(
                    SessionMessages::sessionEnd(m_endReason, m_retryable, m_endCode))));
                socket->close();
            });
        });
    }
    bool listen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    QString url() const
    {
        return QStringLiteral("ws://127.0.0.1:%1").arg(m_server.serverPort());
    }
    int connections = 0;
    // After the app's hello, drop the transport with no session end.
    bool dropWithoutEnd = false;

private:
    QWebSocketServer m_server{QStringLiteral("scripted core"), QWebSocketServer::NonSecureMode};
    quint16 m_helloMajor;
    QString m_endReason;
    bool m_retryable;
    QString m_endCode;
};

// The real Core on a loopback WebSocket listener, as the session tests run
// it, advertising `majors` (the build's own by default).
class RealCore final {
public:
    explicit RealCore(const QList<quint16>& majors = LinkVersion::supportedMajors())
        : m_settings(m_dir.filePath(QStringLiteral("station.settings")))
        , m_server(&m_station, m_settings, NereusSDR::Test::seedUpgradedCoreToken(m_dir.path()), nullptr, majors)
    {
        QObject::connect(&m_listener, &QWebSocketServer::newConnection, &m_server, [this] {
            m_server.acceptTransport(new WebSocketTransport(
                m_listener.nextPendingConnection(), StationServer::kMaxIncomingMessageBytes));
        });
    }
    bool listen() { return m_listener.listen(QHostAddress::LocalHost, 0); }
    QString url() const
    {
        return QStringLiteral("ws://127.0.0.1:%1").arg(m_listener.serverPort());
    }
    QString token() const { return m_server.token(); }
    StationServer& server() { return m_server; }

private:
    QTemporaryDir m_dir;
    AppSettings m_settings;
    RadioModel m_station;
    StationServer m_server;
    QWebSocketServer m_listener{QStringLiteral("stop notice core"),
                                QWebSocketServer::NonSecureMode};
};

// Passes the link between the window and a real Core unchanged, except
// that the app's hello names `major` and `majors` instead of its own: an
// app on other link versions, which this build of the app cannot be. The
// Core's answer, its refusal included, is its own.
class HelloRewritingRelay final : public QObject {
public:
    HelloRewritingRelay(QString coreUrl, quint16 major, QList<quint16> majors)
        : m_coreUrl(std::move(coreUrl)), m_major(major), m_majors(std::move(majors))
    {
        connect(&m_server, &QWebSocketServer::newConnection, this, [this] {
            QWebSocket* app = m_server.nextPendingConnection();
            app->setParent(this);
            ++connections;
            m_apps.append(QPointer<QWebSocket>(app));
            auto* core = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
            auto pending = std::make_shared<QStringList>();
            connect(core, &QWebSocket::connected, this, [core, pending] {
                for (const QString& text : *pending) { core->sendTextMessage(text); }
                pending->clear();
            });
            connect(core, &QWebSocket::textMessageReceived, app,
                    [app](const QString& text) { app->sendTextMessage(text); });
            connect(core, &QWebSocket::disconnected, app, [app] { app->close(); });
            connect(app, &QWebSocket::textMessageReceived, this,
                    [this, core, pending](const QString& text) {
                const QString out = rewriteHello(text);
                if (core->state() == QAbstractSocket::ConnectedState) {
                    core->sendTextMessage(out);
                } else {
                    pending->append(out);
                }
            });
            connect(app, &QWebSocket::disconnected, core, [core] { core->close(); });
            core->open(QUrl(m_coreUrl));
        });
    }
    bool listen() { return m_server.listen(QHostAddress::LocalHost, 0); }
    QString url() const
    {
        return QStringLiteral("ws://127.0.0.1:%1").arg(m_server.serverPort());
    }
    int connections = 0;

    /// iPhone app Task 71: a Core no longer ends one app's session when
    /// another signs in, so the relay stands in for an end with code
    /// takenOver (which a later version's fifth-device takeover sends, and
    /// an older Core still does): it sends `end` to every app connected
    /// through it and closes them.
    void endEveryApp(const SessionMessage& end)
    {
        const QString text = QString::fromUtf8(SessionMessages::encode(end));
        for (const QPointer<QWebSocket>& app : std::as_const(m_apps)) {
            if (app && app->state() == QAbstractSocket::ConnectedState) {
                app->sendTextMessage(text);
                app->close();
            }
        }
    }

private:
    QString rewriteHello(const QString& text) const
    {
        QJsonObject message = QJsonDocument::fromJson(text.toUtf8()).object();
        if (m_major == 0
            || message.value(QStringLiteral("type")).toString() != QLatin1String("hello")) {
            return text;
        }
        message.insert(QStringLiteral("major"), int(m_major));
        QJsonArray majors;
        for (const quint16 major : m_majors) { majors.append(int(major)); }
        message.insert(QStringLiteral("majors"), majors);
        return QString::fromUtf8(QJsonDocument(message).toJson(QJsonDocument::Compact));
    }

    QWebSocketServer m_server{QStringLiteral("hello relay"), QWebSocketServer::NonSecureMode};
    QString m_coreUrl;
    quint16 m_major;  // 0: the hello passes unchanged
    QList<quint16> m_majors;
    QList<QPointer<QWebSocket>> m_apps;
};

struct StopBannerView {
    CoreStopBanner* banner = nullptr;
    QLabel* title = nullptr;
    QLabel* text = nullptr;
    QPushButton* takeBack = nullptr;
    QPushButton* chooseCore = nullptr;
    QPushButton* checkUpdates = nullptr;
};

StopBannerView bannerOf(MainWindow& window)
{
    StopBannerView view;
    view.banner = window.findChild<CoreStopBanner*>(QStringLiteral("coreStopBanner"));
    if (view.banner) {
        view.title = view.banner->findChild<QLabel*>(QStringLiteral("coreStopTitle"));
        view.text = view.banner->findChild<QLabel*>(QStringLiteral("coreStopText"));
        view.takeBack = view.banner->findChild<QPushButton*>(QStringLiteral("coreStopTakeBack"));
        view.chooseCore = view.banner->findChild<QPushButton*>(QStringLiteral("coreStopChooseCore"));
        view.checkUpdates =
            view.banner->findChild<QPushButton*>(QStringLiteral("coreStopCheckUpdates"));
    }
    return view;
}

// Every string the stop message shows is in plain words.
void verifyPlain(const StopBannerView& view)
{
    for (const QString& text : {view.title->text(), view.text->text()}) {
        QVERIFY2(OperatorWording::isPlain(text), qPrintable(text));
        QVERIFY2(!text.contains(QChar(0x2014)), qPrintable(text));
    }
    for (QPushButton* button : {view.takeBack, view.chooseCore, view.checkUpdates}) {
        QVERIFY2(OperatorWording::isPlain(button->text()), qPrintable(button->text()));
    }
}

} // namespace

class TestRemoteCoreStopNotice : public QObject {
    Q_OBJECT
private slots:
    void initTestCase()
    {
        const QString profile = QStringLiteral("remote-core-stop-notice-%1")
                                    .arg(QCoreApplication::applicationPid());
        AppSettings::setProfileOverride(profile);
        QCOMPARE(AppSettings::instance().filePath(), AppSettings::resolveSettingsPath(profile));
        AppSettings::instance().clear();
        QVERIFY(AppSettings::instance().save());
        RadioDiscovery::clearHoldOffForTest();
    }

    void init()
    {
        AppSettings::instance().clear();
        Test::suppressLinuxAudioFirstRun();
        QVERIFY(AppSettings::instance().save());
        RadioDiscovery::clearHoldOffForTest();
    }

    void cleanupTestCase()
    {
        const QString path = AppSettings::instance().filePath();
        QFile::remove(path);
        QFile::remove(path + QStringLiteral(".bak"));
        RadioDiscovery::clearHoldOffForTest();
    }

    // A takeover ends the window's session (iPhone app Task 71: no longer
    // another app signing in, which a Core now admits beside it; the end
    // arrives from a relay, as an older Core or a fifth device's takeover
    // sends it): the window stays, names the other app by the address the
    // end gives, offers Take it back and Choose another Core, and does not
    // retry. Take it back connects again.
    void takeoverStaysPutAndTakeItBackReconnects()
    {
        QTemporaryDir dir;
        AppSettings settings(dir.filePath(QStringLiteral("station.settings")));
        RadioModel station;
        StationServer server(&station, settings, NereusSDR::Test::seedUpgradedCoreToken(dir.path()));
        QWebSocketServer listener(QStringLiteral("stop notice test"),
                                  QWebSocketServer::NonSecureMode);
        QVERIFY(listener.listen(QHostAddress::LocalHost, 0));
        connect(&listener, &QWebSocketServer::newConnection, &server, [&] {
            server.acceptTransport(new WebSocketTransport(listener.nextPendingConnection(),
                                                           StationServer::kMaxIncomingMessageBytes));
        });
        HelloRewritingRelay relay(QStringLiteral("ws://127.0.0.1:%1").arg(listener.serverPort()),
                                  0, {});
        QVERIFY(relay.listen());
        const QString url = relay.url();

        SettingsProxy proxy;
        ScopedRemoteBackend remoteBackend(&proxy);
        MainWindow window({url, server.token(), {}, true}, nullptr,
                          MainWindow::ConnectionStartup::Deferred);
        StationClient* const client = window.findChild<StationClient*>();
        QVERIFY(client);
        client->setReconnectBackoffUnitMs(20);
        window.startInitialConnection();
        QTRY_VERIFY(client->isHandshakeComplete());
        StopBannerView view = bannerOf(window);
        QVERIFY(view.banner && view.title && view.text && view.takeBack
                && view.chooseCore && view.checkUpdates);
        QVERIFY(!view.banner->isVisibleTo(&window));

        relay.endEveryApp(SessionMessages::sessionEnd(
            SessionEndReasons::takenOver(QStringLiteral("127.0.0.1:50123")), /*retryable=*/false,
            QString::fromLatin1(SessionEndCode::kTakenOver)));
        QTRY_VERIFY(!client->isConnectionActive());

        QCOMPARE(client->lastEndReport().kind, StationEndReport::Kind::TakenOver);
        QCOMPARE(client->lastEndReport().takenOverBy, QStringLiteral("127.0.0.1"));
        QVERIFY(view.banner->isVisibleTo(&window));
        QCOMPARE(view.title->text(), QStringLiteral("Core taken over"));
        QCOMPARE(view.text->text(),
                 QStringLiteral("Another app at 127.0.0.1 connected to the Core and took "
                                "over. This window does not reconnect by itself. Take it "
                                "back to use the Core here again."));
        QVERIFY(view.takeBack->isVisibleTo(&window));
        // A --station window has no Connections to open.
        QVERIFY(!view.chooseCore->isVisibleTo(&window));
        QVERIFY(!view.checkUpdates->isVisibleTo(&window));
        verifyPlain(view);

        // No retry: well past the first backoff step, nothing dials.
        const quint32 epoch = client->sessionEpoch();
        QTest::qWait(200);
        QVERIFY(!client->isReconnectPending());
        QCOMPARE(client->sessionEpoch(), epoch);

        // A window the connection picker manages: Choose another Core
        // opens Connections.
        window.setConnectionPickerManaged(true);
        QVERIFY(view.chooseCore->isVisibleTo(&window));
        QSignalSpy connections(&window, &MainWindow::connectionsRequested);
        view.chooseCore->click();
        QCOMPARE(connections.size(), 1);
        QVERIFY(!client->isConnectionActive());

        // Take it back connects again.
        view.takeBack->click();
        QTRY_VERIFY(client->isHandshakeComplete());
        QVERIFY(!view.banner->isVisibleTo(&window));
    }

    // The Core refuses an app on link versions it does not run: here an app
    // two majors ahead (versions 2 and 3; this build of the app cannot be
    // one, so a relay rewrites its hello). The Core's own refusal reaches
    // the window, which says to update the Core, offers Choose another
    // Core but not Check for updates, and does not retry.
    void coreRefusesAnAppAhead()
    {
        RealCore core;
        QVERIFY(core.listen());
        HelloRewritingRelay relay(core.url(), 3, {2, 3});
        QVERIFY(relay.listen());
        SettingsProxy proxy;
        ScopedRemoteBackend remoteBackend(&proxy);
        MainWindow window({relay.url(), core.token(), {}, true}, nullptr,
                          MainWindow::ConnectionStartup::Deferred);
        window.setConnectionPickerManaged(true);
        StationClient* const client = window.findChild<StationClient*>();
        QVERIFY(client);
        client->setReconnectBackoffUnitMs(20);
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("^Refusing a client on link major")));
        window.startInitialConnection();
        QTRY_COMPARE(client->lastEndReport().kind, StationEndReport::Kind::VersionRefused);
        QTRY_VERIFY(!client->isConnectionActive());
        QCOMPARE(client->lastEndReport().reason,
                 SessionEndReasons::versionRefused(LinkVersion::supportedMajors(), {2, 3}));
        QCOMPARE(client->lastEndReport().reason,
                 QStringLiteral("This Core runs link version 1 and this app runs version 3. "
                                "Update the Core."));
        QCOMPARE(client->lastEndReport().coreMajor, 1);
        QCOMPARE(client->lastEndReport().appMajor, 3);
        QVERIFY(!core.server().hasAuthenticatedSession());

        const StopBannerView view = bannerOf(window);
        QVERIFY(view.banner);
        QVERIFY(view.banner->isVisibleTo(&window));
        QCOMPARE(view.title->text(), QStringLiteral("Update the Core"));
        QCOMPARE(view.text->text(),
                 QStringLiteral("This Core is too old to work with this app. Update "
                                "NereusSDR on the Core's computer to use it. This window "
                                "does not reconnect by itself."));
        QVERIFY(view.chooseCore->isVisibleTo(&window));
        QVERIFY(!view.checkUpdates->isVisibleTo(&window));
        QVERIFY(!view.takeBack->isVisibleTo(&window));
        verifyPlain(view);

        QTest::qWait(200);
        QCOMPARE(relay.connections, 1);
        QVERIFY(!client->isReconnectPending());
    }

    // This app refuses a real Core that runs only a newer link version
    // (3): the app finds the mismatch itself, records the same end as the
    // Core's refusal, and the window says to update this app and offers
    // Check for updates. It sends no hello and does not retry.
    void appRefusesACoreAhead()
    {
        RealCore core({3});
        QVERIFY(core.listen());
        SettingsProxy proxy;
        ScopedRemoteBackend remoteBackend(&proxy);
        MainWindow window({core.url(), core.token(), {}, true}, nullptr,
                          MainWindow::ConnectionStartup::Deferred);
        window.setConnectionPickerManaged(true);
        StationClient* const client = window.findChild<StationClient*>();
        QVERIFY(client);
        client->setReconnectBackoffUnitMs(20);
        QTest::ignoreMessage(QtWarningMsg,
                             QRegularExpression(QStringLiteral("^No link major shared")));
        window.startInitialConnection();
        QTRY_VERIFY(!client->isConnectionActive());
        QCOMPARE(client->lastEndReport().kind, StationEndReport::Kind::VersionRefused);
        QCOMPARE(client->lastEndReport().reason,
                 QStringLiteral("This Core runs link version 3 and this app runs version 1. "
                                "Update this app."));
        QCOMPARE(client->lastEndReport().coreMajor, 3);
        QCOMPARE(client->lastEndReport().appMajor, 1);
        QVERIFY(!core.server().hasAuthenticatedSession());

        const StopBannerView view = bannerOf(window);
        QVERIFY(view.banner->isVisibleTo(&window));
        QCOMPARE(view.title->text(), QStringLiteral("Update this app"));
        QCOMPARE(view.text->text(),
                 QStringLiteral("This app is too old to work with this Core. Update "
                                "NereusSDR on this computer to use it. This window does "
                                "not reconnect by itself."));
        QVERIFY(view.chooseCore->isVisibleTo(&window));
        QVERIFY(view.checkUpdates->isVisibleTo(&window));
        QVERIFY(!view.takeBack->isVisibleTo(&window));
        verifyPlain(view);

        const quint32 epoch = client->sessionEpoch();
        QTest::qWait(200);
        QVERIFY(!client->isReconnectPending());
        QCOMPARE(client->sessionEpoch(), epoch);
    }

    // Any other end the Core marks not retryable: the Core's reason in
    // plain words and Choose another Core; no retry.
    void otherRefusalShowsTheCoresReason()
    {
        const QString reason = QStringLiteral("undecodable message");
        ScriptedCore core(kSessionProtocolMajor, reason);
        QVERIFY(core.listen());
        SettingsProxy proxy;
        ScopedRemoteBackend remoteBackend(&proxy);
        MainWindow window({core.url(), QStringLiteral("token"), {}, true}, nullptr,
                          MainWindow::ConnectionStartup::Deferred);
        window.setConnectionPickerManaged(true);
        StationClient* const client = window.findChild<StationClient*>();
        QVERIFY(client);
        client->setReconnectBackoffUnitMs(20);
        window.startInitialConnection();
        QTRY_VERIFY(!client->isConnectionActive());
        QCOMPARE(client->lastEndReport().kind, StationEndReport::Kind::Refused);

        const StopBannerView view = bannerOf(window);
        QVERIFY(view.banner->isVisibleTo(&window));
        QCOMPARE(view.title->text(), QStringLiteral("Core refused this window"));
        QCOMPARE(view.text->text(),
                 OperatorReasonText::forDisplay(reason)
                     + QStringLiteral(" This window does not reconnect by itself."));
        QVERIFY(view.text->text() != reason + QStringLiteral(" This window does not "
                                                            "reconnect by itself."));
        QVERIFY(view.chooseCore->isVisibleTo(&window));
        QVERIFY(!view.takeBack->isVisibleTo(&window));
        QVERIFY(!view.checkUpdates->isVisibleTo(&window));
        verifyPlain(view);
        QTest::qWait(200);
        QCOMPARE(core.connections, 1);
    }

    // iPhone app Task 18 (R-IOS-08, R-IOS-17): the Core's end code chooses
    // the stop message, whatever its words; an older Core that sends no
    // code is still read by its words.
    void endCodesChooseTheStopMessage_data()
    {
        QTest::addColumn<QString>("reason");
        QTest::addColumn<QString>("code");
        QTest::addColumn<int>("notice");
        QTest::addColumn<QString>("title");
        QTest::newRow("deviceRemoved")
            << QStringLiteral("This device was removed from the Core.")
            << QStringLiteral("deviceRemoved") << int(CoreStopNotice::DeviceRemoved)
            << QStringLiteral("Removed from the Core");
        QTest::newRow("deviceNotPaired")
            << QStringLiteral("This device is not paired with this Core. Pair it first.")
            << QStringLiteral("deviceNotPaired") << int(CoreStopNotice::DeviceRemoved)
            << QStringLiteral("Removed from the Core");
        QTest::newRow("pairingRequired")
            << QStringLiteral("This Core uses paired devices. Pair this device first.")
            << QStringLiteral("pairingRequired") << int(CoreStopNotice::PairingRequired)
            << QStringLiteral("Pair with the Core");
        QTest::newRow("no code: an older Core's takeover words")
            << QStringLiteral("Displaced by a newer authenticated connection from "
                              "192.0.2.9:5000")
            << QString() << int(CoreStopNotice::TakenOver) << QStringLiteral("Core taken over");
        QTest::newRow("no code: removal words alone choose nothing")
            << QStringLiteral("This device was removed from the Core.") << QString()
            << int(CoreStopNotice::Refused) << QStringLiteral("Core refused this window");
    }

    void endCodesChooseTheStopMessage()
    {
        QFETCH(QString, reason);
        QFETCH(QString, code);
        QFETCH(int, notice);
        QFETCH(QString, title);
        ScriptedCore core(kSessionProtocolMajor, reason, /*retryable=*/false, code);
        QVERIFY(core.listen());
        SettingsProxy proxy;
        ScopedRemoteBackend remoteBackend(&proxy);
        MainWindow window({core.url(), QStringLiteral("token"), {}, true}, nullptr,
                          MainWindow::ConnectionStartup::Deferred);
        window.setConnectionPickerManaged(true);
        StationClient* const client = window.findChild<StationClient*>();
        auto* const controls = window.findChild<RemoteConnectionController*>();
        QVERIFY(client && controls);
        client->setReconnectBackoffUnitMs(20);
        window.startInitialConnection();
        QTRY_VERIFY(!client->isConnectionActive());
        QCOMPARE(int(controls->stopNotice()), notice);
        const StopBannerView view = bannerOf(window);
        QVERIFY(view.banner->isVisibleTo(&window));
        QCOMPARE(view.title->text(), title);
        QVERIFY(view.text->text().contains(
            QStringLiteral("This window does not reconnect by itself.")));
        QVERIFY(view.chooseCore->isVisibleTo(&window));
        verifyPlain(view);
        QVERIFY2(OperatorWording::coreCalledStationIn(view.text->text()).isEmpty(),
                 qPrintable(view.text->text()));
        QTest::qWait(200);
        QCOMPARE(core.connections, 1);
    }

    // A dropped link (an end the Core marks retryable) behaves as today:
    // the window retries and says so, and no stop message is shown.
    void droppedLinkStillRetries()
    {
        ScriptedCore core(kSessionProtocolMajor, QStringLiteral("heartbeat timeout"),
                          /*retryable=*/true);
        QVERIFY(core.listen());
        SettingsProxy proxy;
        ScopedRemoteBackend remoteBackend(&proxy);
        MainWindow window({core.url(), QStringLiteral("token"), {}, true}, nullptr,
                          MainWindow::ConnectionStartup::Deferred);
        StationClient* const client = window.findChild<StationClient*>();
        auto* const controls = window.findChild<RemoteConnectionController*>();
        QVERIFY(client && controls);
        client->setReconnectBackoffUnitMs(10000);
        QSignalSpy retries(client, &StationClient::reconnectScheduled);
        window.startInitialConnection();
        QTRY_VERIFY(retries.size() >= 1);
        QVERIFY(client->isReconnectPending());
        QCOMPARE(client->lastEndReport().kind, StationEndReport::Kind::None);
        QCOMPARE(controls->stopNotice(), CoreStopNotice::None);
        QCOMPARE(controls->statusText(), QStringLiteral("Retrying Core (attempt 1)"));
        const StopBannerView view = bannerOf(window);
        QVERIFY(view.banner);
        QVERIFY(!view.banner->isVisibleTo(&window));
        controls->disconnectFromStation();
    }

    // R-R3-38: the transport drops with no session end at all (a network
    // cut, a Core that dies): the window retries and shows no stop message.
    void droppedTransportWithNoEndStillRetries()
    {
        ScriptedCore core(kSessionProtocolMajor);
        core.dropWithoutEnd = true;
        QVERIFY(core.listen());
        SettingsProxy proxy;
        ScopedRemoteBackend remoteBackend(&proxy);
        MainWindow window({core.url(), QStringLiteral("token"), {}, true}, nullptr,
                          MainWindow::ConnectionStartup::Deferred);
        window.setConnectionPickerManaged(true);
        StationClient* const client = window.findChild<StationClient*>();
        auto* const controls = window.findChild<RemoteConnectionController*>();
        QVERIFY(client && controls);
        client->setReconnectBackoffUnitMs(10000);
        QSignalSpy retries(client, &StationClient::reconnectScheduled);
        window.startInitialConnection();
        QTRY_VERIFY(retries.size() >= 1);
        QCOMPARE(core.connections, 1);
        QVERIFY(client->isReconnectPending());
        QCOMPARE(client->lastEndReport().kind, StationEndReport::Kind::None);
        QCOMPARE(controls->stopNotice(), CoreStopNotice::None);
        QCOMPARE(controls->statusText(), QStringLiteral("Retrying Core (attempt 1)"));
        const StopBannerView view = bannerOf(window);
        QVERIFY(view.banner);
        QVERIFY(!view.banner->isVisibleTo(&window));
        controls->disconnectFromStation();
    }

    // R-R3-38: the stop message sits over the window's content. When that
    // area changes size with the window itself unchanged (a dock opens),
    // the message moves with it and never sits over the dock.
    void stopMessageFollowsTheContentArea()
    {
        ScriptedCore core(kSessionProtocolMajor, QStringLiteral("undecodable message"));
        QVERIFY(core.listen());
        SettingsProxy proxy;
        ScopedRemoteBackend remoteBackend(&proxy);
        MainWindow window({core.url(), QStringLiteral("token"), {}, true}, nullptr,
                          MainWindow::ConnectionStartup::Deferred);
        StationClient* const client = window.findChild<StationClient*>();
        QVERIFY(client);
        client->setReconnectBackoffUnitMs(20);
        window.resize(1200, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        window.startInitialConnection();
        QTRY_VERIFY(!client->isConnectionActive());
        const StopBannerView view = bannerOf(window);
        QVERIFY(view.banner && view.banner->isVisibleTo(&window));
        QWidget* const content = window.centralWidget();
        QVERIFY(content);
        const auto placedOverContent = [&] {
            const QRect area = content->geometry();
            const QRect banner = view.banner->geometry();
            return banner.top() == area.top() + 16
                && banner.center().x() - area.center().x() >= -1
                && banner.center().x() - area.center().x() <= 1;
        };
        QTRY_VERIFY(placedOverContent());
        const QSize windowSize = window.size();

        QDockWidget dock(QStringLiteral("Dock"), &window);
        auto* body = new QWidget(&dock);
        body->setFixedSize(300, 150);
        dock.setWidget(body);
        window.addDockWidget(Qt::TopDockWidgetArea, &dock);
        dock.show();
        QTRY_VERIFY(content->geometry().top() >= dock.geometry().bottom());
        QCOMPARE(window.size(), windowSize);
        QTRY_VERIFY(placedOverContent());
        QVERIFY(!view.banner->geometry().intersects(dock.geometry()));

        window.addDockWidget(Qt::LeftDockWidgetArea, &dock);
        QTRY_VERIFY(content->geometry().left() >= dock.geometry().right());
        QTRY_VERIFY(placedOverContent());
        QVERIFY(!view.banner->geometry().intersects(dock.geometry()));
    }
};

QTEST_MAIN(TestRemoteCoreStopNotice)
#include "tst_remote_core_stop_notice.moc"
