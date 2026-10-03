// =================================================================
// tests/fakes/RemoteWindowHarness.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Test fixture; see RemoteWindowHarness.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23  J.J. Boyd / KG4VCF  R3 remote window harness plan, Task 2.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
//   2026-09-27  J.J. Boyd / KG4VCF  R-R3-49: the Core's pending slice
//                                    saves are made before the window's
//                                    settings backend is installed and at
//                                    each admission.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 11 fix: records
//                                    the tx.setTxSlice commands the Core
//                                    received.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-29  J.J. Boyd / KG4VCF  Slice control plan Task 17: a window
//                                    that shares slices, held slice-access
//                                    updates and the slice access verbs the
//                                    Core received.
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "RemoteWindowHarness.h"
#include "MainWindowTestSettings.h"

#include <QAction>
#include <QCoreApplication>
#include <QFile>
#include <QHostAddress>
#include <QMenu>
#include <QWebSocket>

#include "core/HpsdrModel.h"
#include "core/session/SessionMessages.h"
#include "core/session/StationClient.h"
#include "gui/PanadapterApplet.h"
#include "gui/PanadapterStack.h"
#include "gui/RemoteConnectionController.h"
#include "gui/SpectrumWidget.h"
#include "gui/TitleBar.h"
#include "gui/widgets/StationBlock.h"
#include "UpgradedCoreToken.h"

namespace NereusSDR::Test {

// ── CoreSessionTransport ────────────────────────────────────────────────

CoreSessionTransport::CoreSessionTransport(QWebSocket* socket, bool holdAfterCapabilities,
                                           const QString& radioName, QObject* parent)
    : WebSocketTransport(socket, StationServer::kMaxIncomingMessageBytes, parent)
    , m_holdAfterCapabilities(holdAfterCapabilities)
    , m_radioName(radioName)
{
}

QByteArray CoreSessionTransport::named(const QByteArray& wire) const
{
    // A bench Core model has no connected radio to take a name from, and a
    // real Core always reports one. Fill it in so the window sees what a
    // Core with a live radio sends.
    if (m_radioName.isEmpty() || !wire.contains("\"stationName\"")) { return wire; }
    SessionMessage message;
    if (!SessionMessages::decode(wire, &message)
        || message.kind != SessionMessageKind::Capabilities) {
        return wire;
    }
    StationCapabilities capabilities = StationCapabilities::fromUpdates(message.updates);
    if (!capabilities.stationName.isEmpty()) { return wire; }
    capabilities.stationName = m_radioName;
    return SessionMessages::encode(SessionMessages::capabilities(capabilities.toUpdates()));
}

void CoreSessionTransport::sendText(const QByteArray& original)
{
    const QByteArray wire = named(original);
    if (m_holdingAccess) {
        SessionMessage message;
        if (SessionMessages::decode(wire, &message) && message.objectKey.startsWith("access:")) {
            m_heldAccess.append(wire);
            return;
        }
    }
    if (m_holding) {
        m_held.append(wire);
        return;
    }
    WebSocketTransport::sendText(wire);
    if (m_capabilitiesSent || !m_holdAfterCapabilities) { return; }
    SessionMessage message;
    if (SessionMessages::decode(wire, &message)
        && message.kind == SessionMessageKind::Capabilities) {
        // The capability exchange tells the window the radio is connected.
        // Everything after it, the settings snapshot and every slice, is
        // what the window has to wait for.
        m_capabilitiesSent = true;
        m_holding = true;
    }
}

void CoreSessionTransport::sendDirect(const QByteArray& wire)
{
    WebSocketTransport::sendText(named(wire));
}

void CoreSessionTransport::release()
{
    m_holding = false;
    m_holdAfterCapabilities = false;
    const QList<QByteArray> held = std::exchange(m_held, {});
    for (const QByteArray& wire : held) {
        WebSocketTransport::sendText(wire);
    }
}

void CoreSessionTransport::releaseSliceAccessUpdates()
{
    m_holdingAccess = false;
    const QList<QByteArray> held = std::exchange(m_heldAccess, {});
    for (const QByteArray& wire : held) {
        sendText(wire);
    }
}

// ── RemoteWindowHarness ─────────────────────────────────────────────────

namespace {

// The path useIsolatedProfile() verified, or empty before it has. clear and
// remove act only while AppSettings still points at it, so nothing here can
// ever empty or delete the shared test sandbox's default settings file that
// parallel test binaries read.
QString& isolatedProfilePath()
{
    static QString path;
    return path;
}

bool pointsAtIsolatedProfile()
{
    const QString& expected = isolatedProfilePath();
    return !expected.isEmpty() && AppSettings::instance().filePath() == expected;
}

} // namespace

bool RemoteWindowHarness::useIsolatedProfile(const QString& tag)
{
    const QString profile = QStringLiteral("%1-%2").arg(tag).arg(QCoreApplication::applicationPid());
    AppSettings::setProfileOverride(profile);
    const QString expected = AppSettings::resolveSettingsPath(profile);
    if (AppSettings::instance().filePath() != expected) {
        // Something reached AppSettings::instance() before this call, so the
        // singleton already holds another file. Clearing it would wipe that
        // file, so refuse and leave it alone.
        qWarning("RemoteWindowHarness: AppSettings is not on the isolated profile "
                 "(%s, expected %s); leaving it untouched.",
                 qPrintable(AppSettings::instance().filePath()), qPrintable(expected));
        return false;
    }
    isolatedProfilePath() = expected;
    return clearIsolatedProfile();
}

bool RemoteWindowHarness::clearIsolatedProfile()
{
    if (!pointsAtIsolatedProfile()) { return false; }
    AppSettings::instance().clear();
    AppSettings::instance().save();
    return true;
}

bool RemoteWindowHarness::removeIsolatedProfile()
{
    if (!pointsAtIsolatedProfile()) { return false; }
    const QString path = AppSettings::instance().filePath();
    QFile::remove(path);
    QFile::remove(path + QStringLiteral(".bak"));
    return true;
}

RemoteWindowHarness::RemoteWindowHarness()
    : RemoteWindowHarness(Options{})
{
}

RemoteWindowHarness::RemoteWindowHarness(const Options& options)
    : m_options(options)
    , m_stationSettings(m_directory.filePath(QStringLiteral("station.settings")))
    , m_server(&m_station, m_stationSettings,
               NereusSDR::Test::seedUpgradedCoreToken(m_directory.path()))
    , m_listener(QStringLiteral("remote window harness"), QWebSocketServer::NonSecureMode)
{
    // A real StationServer over a model that reports its radio connected,
    // the same bench shape tst_remote_connection_controls uses. No device.
    m_station.setBoardForTest(HPSDRHW::Saturn);
    m_station.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
    m_station.setConnectionStateForTest(ConnectionState::Connected);
    for (int i = 0; i < m_options.stationSlices; ++i) {
        m_station.addSlice(QStringLiteral("pan-%1").arg(i));
    }
    // nereusd marks its settings profile on every startup
    // (src/server_main.cpp); the window's Setup gate relies on it.
    m_stationSettings.seedDaemonProfileMarker();
    // No heartbeat: a paused test must not look like a dead link.
    m_server.setHeartbeatIntervalMs(0);
    QObject::connect(&m_listener, &QWebSocketServer::newConnection,
                     &m_server, [this] { accept(); });
    // R-R3-49: admitting a window adopts the Core's unowned slices
    // (StationServer::placeSlicesForAdmission), which schedules the Core's
    // 500 ms coalesced save. Left to its timer, that save went through the
    // window's settings backend (see start()) before or after the window's
    // first look depending on how busy the machine was, and a Core that
    // never sent settings read as one that had. It is made at the
    // admission instead, into this process's own store with the window's
    // backend set aside, as a real Core writes its own store.
    QObject::connect(&m_server, &StationServer::clientAuthenticated, &m_station,
                     [this](const QString&) {
        AppSettings& settings = AppSettings::instance();
        ISettingsBackend* const window = settings.remoteBackend();
        settings.setRemoteBackend(nullptr);
        m_station.flushPendingSettingsSave();
        settings.setRemoteBackend(window);
    });
}

RemoteWindowHarness::~RemoteWindowHarness()
{
    // Window first (its StationClient and media), then the backend it
    // read through, then the proxy itself.
    m_window.reset();
    QCoreApplication::processEvents();
    if (m_backendInstalled) {
        AppSettings::instance().setRemoteBackend(nullptr);
    }
}

bool RemoteWindowHarness::start()
{
    if (!m_directory.isValid() || !m_listener.listen(QHostAddress::LocalHost, 0)) {
        return false;
    }
    // The saved operator layout, as a previous run would have left it.
    AppSettings::instance().setValue(QStringLiteral("PanLayoutId"), m_options.panLayout);
    // The window is remote, which already skips the Linux audio first-run
    // dialog; seeded anyway, as for every test that builds a MainWindow.
    suppressLinuxAudioFirstRun();

    // R-R3-49: the Core's model and the window share this process's
    // AppSettings (a real Core writes its own store), so a save the Core's
    // model makes after this line goes through the window's settings
    // backend. The slices the constructor added scheduled the Core's
    // 500 ms coalesced save (RadioModel::scheduleSettingsSave); made now,
    // it lands before the window's backend is installed, every run.
    m_station.flushPendingSettingsSave();
    AppSettings::instance().setRemoteBackend(&m_proxy);
    m_backendInstalled = true;
    const RemoteStationOptions options{
        QStringLiteral("ws://127.0.0.1:%1").arg(m_listener.serverPort()),
        m_server.token(), {}, true};
    m_window = std::make_unique<MainWindow>(options, nullptr,
                                            MainWindow::ConnectionStartup::Deferred);
    if (StationClient* stationClient = client()) {
        stationClient->setReconnectBackoffUnitMs(m_options.backoffUnitMs);
        if (m_options.sliceAccess) {
            stationClient->setTokenSessionHolderForTest(m_options.sessionHolderId);
            stationClient->setTokenSliceAccessForTest(true);
        }
    }
    m_window->resize(1280, 800);
    m_window->show();
    QCoreApplication::processEvents();
    return true;
}

void RemoteWindowHarness::startStartupConnection()
{
    if (m_window) { m_window->startInitialConnection(); }
}

void RemoteWindowHarness::accept()
{
    while (m_listener.hasPendingConnections()) {
        QWebSocket* socket = m_listener.nextPendingConnection();
        ++m_accepted;
        auto* link = new CoreSessionTransport(socket, std::exchange(m_holdNext, false),
                                             m_options.radioName);
        QObject::connect(link, &SessionTransport::textReceived, &m_server,
                         [this](const QByteArray& wire) { recordInbound(wire); });
        m_link = link;
        m_server.acceptTransport(link);
    }
}

void RemoteWindowHarness::recordInbound(const QByteArray& wire)
{
    SessionMessage message;
    if (!SessionMessages::decode(wire, &message)
        || message.kind != SessionMessageKind::CommandInvoke) {
        return;
    }
    if (message.commandVerb == "tx.setTxSlice") {
        for (const MirrorUpdate& argument : message.arguments) {
            if (argument.name == "sliceId") {
                m_txSliceCommands << argument.value.toInt();
            }
        }
        return;
    }
    if (message.commandVerb == "slice.listen" || message.commandVerb == "slice.stopListening"
        || message.commandVerb == "slice.takeControl" || message.commandVerb == "slice.release") {
        for (const MirrorUpdate& argument : message.arguments) {
            if (argument.name == "sliceId") {
                m_sliceAccessCommands << QStringLiteral("%1:%2").arg(
                    QString::fromUtf8(message.commandVerb)).arg(argument.value.toInt());
            }
        }
        return;
    }
    if (message.commandVerb != "addSlice" && message.commandVerb != "addSliceOnPan") {
        return;
    }
    QString pan;
    for (const MirrorUpdate& argument : message.arguments) {
        if (argument.name == "panId" || argument.name == "initialPanId") {
            pan = argument.value.toString();
        }
    }
    m_addSliceCommands << QStringLiteral("%1:%2").arg(QString::fromUtf8(message.commandVerb), pan);
}

void RemoteWindowHarness::releaseSnapshot()
{
    if (m_link) { m_link->release(); }
}

bool RemoteWindowHarness::snapshotHeld() const
{
    return m_link && m_link->holding();
}

void RemoteWindowHarness::dropLink(const QString& reason)
{
    if (m_link) { m_link->closeLink(reason); }
}

void RemoteWindowHarness::reportRadioOffline()
{
    m_station.setConnectionStateForTest(ConnectionState::Disconnected);
}

void RemoteWindowHarness::pushCapabilities(const StationCapabilities& capabilities)
{
    if (m_link) {
        m_link->sendDirect(SessionMessages::encode(
            SessionMessages::capabilities(capabilities.toUpdates())));
    }
}

RadioModel* RemoteWindowHarness::remoteModel() const
{
    return m_window ? m_window->radioModel() : nullptr;
}

StationClient* RemoteWindowHarness::client() const
{
    return m_window ? m_window->findChild<StationClient*>() : nullptr;
}

RemoteConnectionController* RemoteWindowHarness::controls() const
{
    return m_window ? m_window->findChild<RemoteConnectionController*>() : nullptr;
}

ConnectionSegment* RemoteWindowHarness::titleSegment() const
{
    return m_window ? m_window->findChild<ConnectionSegment*>() : nullptr;
}

StationBlock* RemoteWindowHarness::stationBlock() const
{
    return m_window ? m_window->findChild<StationBlock*>() : nullptr;
}

SpectrumWidget* RemoteWindowHarness::panSpectrum(const QString& panId) const
{
    if (!m_window) { return nullptr; }
    for (PanadapterApplet* applet : m_window->findChildren<PanadapterApplet*>()) {
        if (applet->panId() == panId) { return applet->spectrumWidget(); }
    }
    return nullptr;
}

QAction* RemoteWindowHarness::menuAction(const QString& menuTitle,
                                         const QString& actionText) const
{
    if (!m_window) { return nullptr; }
    // The window's menus, wherever the title bar hosts them.
    for (QMenu* menu : m_window->findChildren<QMenu*>()) {
        if (menu->title() != menuTitle) { continue; }
        for (QAction* action : menu->actions()) {
            if (action->text() == actionText) { return action; }
        }
    }
    return nullptr;
}

} // namespace NereusSDR::Test
