// no-port-check: NereusSDR-original. R-R3-38 operator target selection.
//
// iPhone app Task 18 (R-IOS-08): pairing from the Connections window. A
// Core on this network that no device has paired with yet offers Pair
// (one click on the Core's own network, or its code when the Core takes
// only the code); Add a Core by code pairs with any Core by the code it
// shows. A pairing becomes a saved Core under Your Cores, trusted by its
// identity, and is connected to at once by this computer's key. A saved
// Core from before paired devices enrols this computer's key on its next
// token sign-in and is saved with its identity from then on.
// 2026-10-01: Authenticated Core address inventory and reconnect learning.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex. NereusSDR-original.

#include "gui/GuiConnectionController.h"

#include "core/AppSettings.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/StationIdentity.h"
#include "core/session/StationClient.h"
#include "core/session/RemoteDevicesState.h"
#include "core/session/RendezvousWire.h"
#include "gui/AddCustomRadioDialog.h"
#include "gui/ConnectionPanel.h"
#include "gui/ConnectionSelector.h"
#include "gui/CoreTargetEditor.h"
#include "gui/MainWindow.h"
#include "gui/OperatorReasonText.h"
#include "gui/StationLanSelection.h"
#include "gui/RemoteConnectionController.h"
#include "models/RadioModel.h"

#include <QDateTime>
#include <QNetworkInterface>
#include <QTimer>
#include <QUrl>

namespace NereusSDR {
namespace {
QString networkFingerprint()
{
    QStringList addresses;
    for (const QNetworkInterface& interface : QNetworkInterface::allInterfaces()) {
        const auto flags = interface.flags();
        if (!flags.testFlag(QNetworkInterface::IsUp)
            || !flags.testFlag(QNetworkInterface::IsRunning)
            || flags.testFlag(QNetworkInterface::IsLoopBack)) { continue; }
        for (const QNetworkAddressEntry& entry : interface.addressEntries()) {
            if (entry.ip().isNull() || entry.ip().isLoopback()) { continue; }
            addresses.append(QString::number(interface.index()) + QLatin1Char(':')
                + entry.ip().toString() + QLatin1Char('/')
                + QString::number(entry.prefixLength()));
        }
    }
    addresses.removeDuplicates();
    addresses.sort();
    return addresses.join(QLatin1Char('|'));
}

QString endpointText(const QUrl& url)
{
    QString host = url.host();
    if (host.contains(QLatin1Char(':'))) { host = QLatin1Char('[') + host + QLatin1Char(']'); }
    return url.port() >= 0 ? host + QLatin1Char(':') + QString::number(url.port()) : host;
}

QString endpointText(const RemoteStationOptions& connection)
{
    if (connection.url.isEmpty() && !connection.rendezvousId.isEmpty()) {
        return QObject::tr("Remote access");
    }
    return endpointText(QUrl(connection.url));
}

bool sameConnection(const RemoteStationOptions& a, const RemoteStationOptions& b)
{
    return QUrl(a.url) == QUrl(b.url) && a.token == b.token
        && a.fingerprint == b.fingerprint && a.allowUnpinned == b.allowUnpinned
        && a.identityFingerprint == b.identityFingerprint
        && a.rendezvousId == b.rendezvousId
        && a.reachFromAnywhere == b.reachFromAnywhere;
}

// iPhone app Task 18: whether a Core on this network takes this computer
// as a new device, and how (an announcement from before pairing never
// does).
bool takesNewDevices(const StationLanAnnouncement& announcement)
{
    return announcement.schema >= kStationLanAnnouncementSchema2
        && announcement.pairing != StationLanPairing::Closed;
}

bool oneClickPairs(const StationLanAnnouncement& announcement)
{
    return takesNewDevices(announcement) && !announcement.claimed
        && announcement.pairing == StationLanPairing::Click;
}

bool selectionMatchesSaved(const StationStartupSelection& selection, const SavedCoreTarget& target)
{
    RemoteStationOptions original = selection.connection;
    if (!selection.savedAddressBeforeDiscovery.isEmpty()) {
        original.url = selection.savedAddressBeforeDiscovery;
        // Discovered connections always enforce the saved pin, even if a
        // record also carried the legacy bench flag.
        original.allowUnpinned = target.connection.allowUnpinned;
    }
    return sameConnection(original, target.connection);
}
}

GuiConnectionController::GuiConnectionController(QObject* parent)
    : QObject(parent), m_store(AppSettings::instance()), m_lan(this), m_selector(std::make_unique<ConnectionSelector>())
{
    m_negativeExpiryTimer.setSingleShot(true);
    connect(&m_negativeExpiryTimer, &QTimer::timeout, this, &GuiConnectionController::refresh);
    connect(&m_lan, &StationLanDiscovery::changed, this, &GuiConnectionController::refresh);
    connect(&m_sessions, &GuiSessionCoordinator::windowChanged,
            this, &GuiConnectionController::attachWindow);
    connect(&m_sessions, &GuiSessionCoordinator::connectionsRequested,
            this, &GuiConnectionController::showConnections);
    connect(&m_sessions, &GuiSessionCoordinator::stationOperationFailed, this,
            [this](const QString& reason) {
        if (m_shuttingDown) { return; }
        m_selector->setNotice(reason);
        showConnections();
    }, Qt::QueuedConnection);
    connect(m_selector.get(), &ConnectionSelector::connectRequested,
            this, &GuiConnectionController::queueConnect);
    connect(m_selector.get(), &ConnectionSelector::disconnectRequested,
            this, &GuiConnectionController::disconnectCurrent);
    connect(m_selector.get(), &ConnectionSelector::addCoreRequested,
            this, [this] { editCore(); });
    connect(m_selector.get(), &ConnectionSelector::addRadioRequested,
            this, [this] { editRadio(); });
    connect(m_selector.get(), &ConnectionSelector::pairRequested,
            this, &GuiConnectionController::pairTarget);
    connect(m_selector.get(), &ConnectionSelector::addByCodeRequested,
            this, [this] { addByCode(); });
    connect(m_selector.get(), &ConnectionSelector::editRequested, this, [this](const QString& key) {
        if (key.startsWith(QLatin1String("saved:"))) { editCore(key.mid(6)); }
        else if (key.startsWith(QLatin1String("radio:"))) { editRadio(key.mid(6)); }
        else if (key == QLatin1String("current")) { editCore(); }
    });
    connect(m_selector.get(), &ConnectionSelector::forgetRequested,
            this, &GuiConnectionController::forgetTarget);
    connect(m_selector.get(), &ConnectionSelector::detailsRequested,
            this, &GuiConnectionController::showDetails);
    connect(m_selector.get(), &ConnectionSelector::scanRequested,
            this, &GuiConnectionController::scan);
}

GuiConnectionController::~GuiConnectionController() { shutdown(); }

void GuiConnectionController::start(const StationStartupRequest& request)
{
    QString error;
    m_storeLoaded = m_store.load(&error);
    if (m_storeLoaded) { observeNetworkGeneration(); }
    const auto selected = m_storeLoaded ? resolveStationStartup(request, m_store, &error)
                                        : std::nullopt;
    // A corrupt address book or bad CLI must not start the old local radio
    // implicitly. Open an idle local-capable window with a visible error.
    m_sessions.replace(selected.value_or(StationStartupSelection{}),
                       selected && shouldStartStationConnection(request, *selected, m_store));
    if (!error.isEmpty()) {
        m_selector->setNotice(error);
        showConnections();
    }
}

void GuiConnectionController::shutdown()
{
    if (m_shuttingDown) { return; }
    m_shuttingDown = true;
    ++m_request;
    m_selector->hide();
    if (m_pairing) { m_pairing->cancel(); }
    m_lan.stop();
    m_sessions.shutdown();
}

void GuiConnectionController::attachWindow(MainWindow* window)
{
    for (const QMetaObject::Connection& connection : m_windowConnections) {
        QObject::disconnect(connection);
    }
    m_windowConnections.clear();
    m_windowTargetId.clear();
    m_windowTargetIncarnation = 0;
    m_windowCoordinatorGeneration = 0;
    m_remoteControls = nullptr;
    m_discovery = nullptr;
    m_radios.clear();
    m_seenAt.clear();
    if (!window || m_shuttingDown) { return; }
    const quint64 generation = m_sessions.generation();
    m_windowTargetId = m_sessions.selection().savedId;
    m_windowTargetIncarnation = m_store.targetIncarnation(m_windowTargetId);
    m_windowCoordinatorGeneration = generation;
    RadioModel* model = window->radioModel();
    m_discovery = model->discovery();
    m_remoteControls = window->findChild<RemoteConnectionController*>();
    const auto update = [this, generation] {
        if (generation == m_sessions.generation() && !m_shuttingDown) { refresh(); }
    };
    m_windowConnections.append(connect(model, &RadioModel::connectionStateChanged, this, update));
    m_windowConnections.append(connect(model, &RadioModel::infoChanged, this, update));
    const auto found = [this, generation](const RadioInfo& radio) {
        if (generation != m_sessions.generation() || m_shuttingDown) { return; }
        m_radios.insert(radio.macAddress, radio);
        m_seenAt.insert(radio.macAddress, QDateTime::currentMSecsSinceEpoch());
        refresh();
    };
    m_windowConnections.append(connect(m_discovery, &RadioDiscovery::radioDiscovered, this, found));
    m_windowConnections.append(connect(m_discovery, &RadioDiscovery::radioUpdated, this, found));
    m_windowConnections.append(connect(m_discovery, &RadioDiscovery::radioLost, this,
        [this, generation](const QString& mac) {
            if (generation != m_sessions.generation()) { return; }
            m_seenAt.remove(mac);
            refresh();
        }));
    if (m_remoteControls) {
        m_windowConnections.append(connect(m_remoteControls, &RemoteConnectionController::changed,
                                            this, update));
        // iPhone app plan Task 27 fix wave: a reconnect in this window reads
        // the store's current last good addresses (rememberAuthenticatedRadio()
        // adds to them after each connect), not the list the window was made
        // with.
        const QPointer<GuiConnectionController> self(this);
        const QString savedId = m_sessions.selection().savedId;
        const quint64 incarnation = m_store.targetIncarnation(savedId);
        const RemoteStationOptions trusted = m_sessions.selection().connection;
        m_remoteControls->setCurrentOptionsSource(
            [self, generation, savedId, incarnation, trusted]() -> std::optional<RemoteStationOptions> {
                if (!self || generation != self->m_sessions.generation() || !self->m_storeLoaded) {
                    return std::nullopt;
                }
                self->observeNetworkGeneration();
                const auto target = self->m_store.target(savedId);
                if (!target || incarnation == 0 || self->m_store.targetIncarnation(savedId) != incarnation
                    || self->m_sessions.selection().savedId != savedId
                    || target->connection.identityFingerprint != trusted.identityFingerprint
                    || target->connection.token != trusted.token || target->connection.fingerprint != trusted.fingerprint
                    || target->connection.allowUnpinned != trusted.allowUnpinned
                    || target->connection.reachFromAnywhere != trusted.reachFromAnywhere
                    || (!self->m_sessions.selection().savedAddressBeforeDiscovery.isEmpty()
                        ? target->connection.url != self->m_sessions.selection().savedAddressBeforeDiscovery
                        : QUrl(target->connection.url) != QUrl(trusted.url))) {
                    return std::nullopt;
                }
                RemoteStationOptions current = connectionOptionsForTarget(*target);
                current.url = trusted.url;
                return current;
            });
    }
    if (auto* client = window->findChild<StationClient*>()) {
        const QString nameTargetId = m_sessions.selection().savedId;
        const quint64 nameIncarnation = m_store.targetIncarnation(nameTargetId);
        const QByteArray nameIdentity = m_sessions.selection().connection.identityFingerprint;
        const QPointer<StationClient> nameClient(client);
        const auto rememberName = [this, generation, nameTargetId, nameIncarnation, nameIdentity, nameClient] {
            if (!nameClient) { return; }
            const quint32 epoch = nameClient->sessionEpoch();
            const auto target = m_store.target(nameTargetId);
            if (!m_storeLoaded || m_shuttingDown || generation != m_sessions.generation()
                || m_sessions.selection().savedId != nameTargetId || nameIncarnation == 0
                || m_store.targetIncarnation(nameTargetId) != nameIncarnation || !target
                || target->connection.identityFingerprint != nameIdentity || nameIdentity.size() != 32
                || target->connection.allowUnpinned || !nameClient->isHandshakeComplete()
                || !nameClient->signedInWithDeviceKey() || nameClient->stationIdentityFingerprint() != nameIdentity) { return; }
            const QString name = nameClient->remoteDevices()->coreInfo().stationLabel;
            if (name.isEmpty() || nameClient->sessionEpoch() != epoch) { return; }
            QString error;
            if (!m_store.rememberCoreName(nameTargetId, nameIdentity, name, &error)) {
                m_selector->setNotice(error);
            }
        };
        m_windowConnections.append(connect(client, &StationClient::stateSnapshotApplied, this, rememberName));
        m_windowConnections.append(connect(client->remoteDevices(), &RemoteDevicesState::coreInfoChanged, this, rememberName));
        const auto remember = [this, generation] {
            if (generation == m_sessions.generation() && !m_shuttingDown) {
                rememberAuthenticatedRadio();
                refresh();
            }
        };
        m_windowConnections.append(connect(client, &StationClient::stateSnapshotApplied, this,
            [this, remember] { rememberAuthenticatedCapability(); remember(); }));
        m_windowConnections.append(connect(client, &StationClient::handshakeComplete, this, remember));
        m_windowConnections.append(connect(client, &StationClient::pathChanged, this, remember));
        m_windowConnections.append(connect(client->remoteDevices(), &RemoteDevicesState::coreInfoChanged, this, remember));
        m_windowConnections.append(connect(client, &StationClient::stationIdentityLearned, this,
            [this, generation](const QByteArray& identity) {
                if (generation == m_sessions.generation() && !m_shuttingDown) {
                    rememberStationIdentity(identity);
                }
            }));
    }
    refresh();
    if (m_selector->isVisible()) { scan(); }
}

void GuiConnectionController::refresh()
{
    if (m_shuttingDown) { return; }
    observeNetworkGeneration();
    m_negativeExpiryTimer.stop();
    qint64 nextExpiry = 0;
    const qint64 wallNow = QDateTime::currentMSecsSinceEpoch();
    for (const SavedCoreTarget& target : m_store.targets()) {
        const auto& options = target.connection;
        if (options.effectiveControlChannelVersion(wallNow) == 0) {
            const qint64 remaining = options.negativeControlObservedMs
                + RemoteStationOptions::kNegativeControlLifetimeMs - wallNow;
            if (nextExpiry == 0 || remaining < nextExpiry) { nextExpiry = remaining; }
        }
    }
    if (nextExpiry > 0) {
        m_negativeExpiryTimer.start(static_cast<int>(nextExpiry));
    }
    MainWindow* window = m_sessions.window();
    if (!window) { return; }
    RadioModel* model = window->radioModel();
    const auto current = m_sessions.selection();
    QMap<QString, RadioInfo> radios = m_radios;
    for (const SavedRadio& saved : AppSettings::instance().savedRadios()) {
        if (!radios.contains(saved.info.macAddress)) { radios.insert(saved.info.macAddress, saved.info); }
    }
    QList<ConnectionTargetRow> rows;
    rows.append({QStringLiteral("local"), ConnectionTargetKind::LocalRadio,
        tr("This computer's Core"), tr("Choose a local radio"), tr("This computer"),
        model->ownsLocalDsp() ? tr("Selected") : tr("Available"), !model->ownsLocalDsp(), false, false});
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = radios.cbegin(); it != radios.cend(); ++it) {
        const RadioInfo& radio = it.value();
        const bool connected = model->ownsLocalDsp() && model->isConnected()
            && model->currentRadioMac().compare(radio.macAddress, Qt::CaseInsensitive) == 0;
        QString state = tr("Saved / not seen");
        if (connected) { state = tr("Connected locally"); }
        else if (ConnectionPanel::statePillForLastSeen(m_seenAt.value(it.key()), now)
                 == ConnectionPanel::StatePill::Online) {
            state = radio.inUse ? tr("In use") : tr("Available");
        }
        rows.append({QStringLiteral("radio:") + it.key(), ConnectionTargetKind::LocalRadio,
            radio.name.isEmpty() ? radio.displayName() : radio.name, tr("This computer's Core"),
            radio.address.toString(), state, !connected, true,
            AppSettings::instance().savedRadio(it.key()).has_value()});
    }
    for (const SavedCoreTarget& target : m_store.targets()) {
        const bool selected = current.savedId == target.id;
        const bool exact = selected && selectionMatchesSaved(current, target);
        ConnectionTargetRow row = savedCoreRow(target, m_storeLoaded);
        if (selected && m_remoteControls) {
            row.state = m_remoteControls->statusText();
            row.radioText = m_remoteControls->radioText();
            if (exact && m_remoteControls->state() == ConnectionState::Connected) {
                const auto* client = m_sessions.window()->findChild<StationClient*>();
                if (client && client->connectedUrl().isEmpty()) {
                    row.address = m_remoteControls->endpointText();
                }
            }
            if (!exact) { row.state += tr(", saved changes pending"); }
            else if (!current.savedAddressBeforeDiscovery.isEmpty()) { row.state += tr(", using the LAN address"); }
        }
        row.connectable = !exact || !m_remoteControls || m_remoteControls->canConnect();
        rows.append(row);
    }
    for (const StationLanEndpoint& endpoint : m_lan.endpoints()) {
        const auto matches = matchingSavedCores(endpoint, m_store.targets());
        const bool exact = matches.size() == 1 && current.savedId == matches.first().id
            && selectionMatchesSaved(current, matches.first())
            && QUrl(current.connection.url) == endpoint.url();
        ConnectionTargetRow row = lanCoreRow(endpoint, m_store.targets());
        if (exact && m_remoteControls && !row.pairable) {
            row.state = m_remoteControls->statusText();
            row.connectable = m_remoteControls->canConnect();
        }
        rows.append(row);
    }
    m_selector->setDiscoveryStatus(m_lan.port() == 0
        ? tr("LAN discovery is not running. Saved and manual addresses remain available.")
        // Part C fix wave (R2-M2): new Cores pair from their row now.
        : m_lan.lastError().isEmpty() ? tr("LAN discovery is active.")
        : OperatorReasonText::lanDiscoveryForDisplay(m_lan.lastError()));
    if (current.connection.isRemote() && !m_store.target(current.savedId)) {
        rows.append({QStringLiteral("current"), ConnectionTargetKind::SavedCore,
            tr("Current Core (not saved)"), m_remoteControls ? m_remoteControls->radioText() : QString(),
            endpointText(current.connection), m_remoteControls ? m_remoteControls->statusText() : QString(),
            m_remoteControls && m_remoteControls->canConnect(), m_storeLoaded, false});
    }
    m_selector->setTargets(rows);
    if (m_remoteControls) {
        m_selector->setCurrentConnection(m_remoteControls->statusText(), m_remoteControls->detailText(),
            m_remoteControls->canDisconnect(), m_remoteControls->state() == ConnectionState::LinkLost);
    } else {
        const bool active = model->connectionState() != ConnectionState::Disconnected;
        m_selector->setCurrentConnection(model->isConnected() ? tr("Connected using this computer's Core")
            : tr("This computer's Core, with no radio connected"), model->connectionIpText(), active,
            model->connectionState() == ConnectionState::LinkLost);
    }
}

ConnectionTargetRow GuiConnectionController::savedCoreRow(const SavedCoreTarget& target,
                                                         bool storeLoaded)
{
    const bool paired = !target.connection.identityFingerprint.isEmpty();
    return {QStringLiteral("saved:") + target.id, ConnectionTargetKind::SavedCore,
            target.label.isEmpty() ? endpointText(target.connection) : target.label,
            target.lastRadioName.isEmpty() ? tr("Radio unknown")
                                           : tr("%1 (last known)").arg(target.lastRadioName),
            target.connection.cachedAddresses.isEmpty()
                ? (target.connection.url.isEmpty() && !target.connection.coreAddresses.isEmpty()
                    ? tr("Core-supplied addresses") : endpointText(target.connection))
                : tr("%1 (last worked)").arg(endpointText(QUrl(target.connection.cachedAddresses.first()))),
            // iPhone app Task 18: a Core this computer paired with says so.
            !isReadyToConnect(target.connection) ? tr("Needs setup")
                : paired                         ? tr("Paired")
                                                 : tr("Disconnected"),
            true, storeLoaded, storeLoaded};
}

QString GuiConnectionController::savedCoreDetails(const SavedCoreTarget& target)
{
    QStringList lines;
    lines.append(tr("Name on this computer: %1").arg(target.label));
    lines.append(tr("Configured address: %1").arg(target.connection.url.isEmpty()
        ? tr("None") : endpointText(QUrl(target.connection.url))));
    if (!target.connection.cachedAddresses.isEmpty()) {
        lines.append(tr("Last worked: %1").arg(endpointText(QUrl(target.connection.cachedAddresses.first()))));
        lines.append(tr("Previously worked addresses:"));
        for (const QString& address : target.connection.cachedAddresses) {
            lines.append(tr("  %1").arg(endpointText(QUrl(address))));
        }
    }
    if (!target.connection.coreAddresses.isEmpty()) {
        lines.append(tr("Addresses supplied by the authenticated Core (may not be reachable):"));
        for (const QString& address : target.connection.coreAddresses) {
            lines.append(tr("  %1").arg(endpointText(QUrl(address))));
        }
    }
    lines.append(tr("Radio: %1 (as of the last connection to this Core)")
        .arg(target.lastRadioName.isEmpty() ? tr("unknown") : target.lastRadioName));
    if (!target.connection.identityFingerprint.isEmpty()) {
        lines.append(tr("Paired with this computer: it signs in with its own key."));
    }
    return lines.join(QLatin1Char('\n'));
}

ConnectionTargetRow GuiConnectionController::lanCoreRow(const StationLanEndpoint& endpoint,
                                                       const QList<SavedCoreTarget>& saved)
{
    const auto matches = matchingSavedCores(endpoint, saved);
    const auto& advertised = endpoint.announcement;
    ConnectionTargetRow row{QStringLiteral("lan:") + endpoint.key(), ConnectionTargetKind::LanCore,
        // iPhone app Task 16: a Core is listed by its label (schema 2),
        // or by its name when it sends none.
        tr("%1 (advertised)").arg(advertised.displayName()),
        advertised.radioConnected ? tr("%1 (advertised online)").arg(advertised.radioName)
            // iPhone app plan Task 25 (R-IOS-16): a Core waiting for a radio
            // to be chosen says so, as the phone's list does.
            : advertised.radio == StationLanRadio::Waiting ? tr("Waiting for a radio")
            : tr("%1 (advertised offline)").arg(advertised.radioName.isEmpty() ? tr("Radio") : advertised.radioName),
        endpointText(endpoint.url()), QString(),
        matches.size() < 2, false, false};
    // Part C fix wave (R2-M1): a saved Core found by its identity that now
    // announces itself unclaimed has forgotten this computer (a console
    // reset, or this computer was removed): Connect would only be refused
    // again, so the row offers Pair. A Core this computer is still paired
    // with never announces itself unclaimed.
    const bool identityMatch = matches.size() == 1
        && !matches.first().connection.identityFingerprint.isEmpty()
        && matches.first().connection.identityFingerprint == advertised.identity;
    const bool forgotThisComputer = identityMatch
        && advertised.schema >= kStationLanAnnouncementSchema2 && !advertised.claimed;
    if (matches.size() > 1) {
        row.state = tr("Choose a saved entry");
    } else if (matches.size() == 1 && !forgotThisComputer) {
        row.state = tr("Saved, ready to connect");
    } else if (oneClickPairs(advertised)) {
        // iPhone app Task 18: an unclaimed Core pairs with one click.
        row.state = tr("Not paired");
        row.pairable = true;
        row.connectable = false;
    } else if (takesNewDevices(advertised)) {
        // It takes new devices by its code only (a Core set to, or one
        // reopened for another device).
        row.state = tr("Pairs with its code");
        row.pairable = true;
        row.connectable = false;
    } else if (advertised.schema >= kStationLanAnnouncementSchema2 && !advertised.claimed) {
        // Unclaimed, but its pairing closed after too many wrong codes: it
        // opens again only from the Core's own computer.
        row.state = tr("Pairing is closed on the Core");
        row.connectable = false;
    } else if (advertised.schema >= kStationLanAnnouncementSchema2) {
        // Paired with other devices and not taking new ones: pairing opens
        // on the Core or on one of its devices.
        row.state = tr("Paired with other devices");
        row.connectable = false;
    } else {
        row.state = tr("Needs setup");
    }
    return row;
}

QString GuiConnectionController::lanCoreNextStep(const StationLanAnnouncement& advertised)
{
    // iPhone app Task 18: what to do next depends on whether the Core takes
    // new devices.
    if (oneClickPairs(advertised)) {
        return tr("No device is paired with this Core. Select Pair to pair this computer with it.");
    }
    if (takesNewDevices(advertised)) {
        return tr("This Core pairs with its code. Select Pair and type the code the Core shows.");
    }
    if (advertised.schema >= kStationLanAnnouncementSchema2 && !advertised.claimed) {
        // Part C follow-up (R-IOS-08): the branch lanCoreRow() gives
        // "Pairing is closed on the Core". Unclaimed, its pairing closed
        // after too many wrong codes, and only the Core's console reopens it.
        return tr("No device is paired with this Core. Its pairing closed after too many wrong codes. Run nereusd pairing open on the Core's computer to open it again.");
    }
    if (advertised.schema >= kStationLanAnnouncementSchema2) {
        return tr("This Core is paired with other devices. Open pairing on the Core, or on a device paired with it, then add it by code.");
    }
    return tr("Use a saved entry for it, or get its pairing token and certificate fingerprint from Core setup.");
}

bool GuiConnectionController::authenticatedSelectionMatchesSaved(const StationStartupSelection& selection,
                                                                  const SavedCoreTarget& target)
{
    if (selection.savedId != target.id) { return false; }
    StationStartupSelection selected = selection;
    if (selected.connection.identityFingerprint.size() == 32 && !selected.connection.allowUnpinned
        && selected.connection.identityFingerprint == target.connection.identityFingerprint) {
        selected.connection.rendezvousId = target.connection.rendezvousId;
    }
    return selectionMatchesSaved(selected, target);
}

RemoteStationOptions GuiConnectionController::connectionOptionsForTarget(const SavedCoreTarget& target)
{
    RemoteStationOptions options = target.connection;
    options.directCandidates.clear();
    if (options.identityFingerprint.size() == 32 && !options.allowUnpinned) {
        QList<QUrl> seen;
        for (const QString& address : target.manualAddresses + options.cachedAddresses + options.coreAddresses) {
            const QUrl url(address);
            if (RemoteStationOptions::isValidStationUrl(address) && !seen.contains(url)) {
                seen.append(url);
                options.directCandidates.append(url.toString());
            }
        }
    }
    return options;
}

bool GuiConnectionController::isReadyToConnect(const RemoteStationOptions& connection)
{
    if (!connection.isValidRemoteTarget()) { return false; }
    if (!connection.identityFingerprint.isEmpty()) {
        return !connection.url.isEmpty() || connection.hasAuthenticatedDirectAddresses()
            || (connection.reachFromAnywhere
                && connection.serviceConnectRefusal().isEmpty()
                && !configuredRemoteAccessServers().isEmpty());
    }
    return !connection.token.isEmpty()
        && (!connection.fingerprint.isEmpty() || connection.allowUnpinned);
}

void GuiConnectionController::showConnections()
{
    if (m_shuttingDown) { return; }
    refresh();
    m_selector->show();
    m_selector->raise();
    m_selector->activateWindow();
    scan();
}

void GuiConnectionController::scan()
{
    if (m_shuttingDown) { return; }
    m_lan.start();
    if (m_discovery) { m_discovery->startDiscovery(); }
    refresh();
}

void GuiConnectionController::queueConnect(const QString& key)
{
    const quint64 request = ++m_request;
    QTimer::singleShot(0, this, [this, key, request] {
        if (!m_shuttingDown && request == m_request) { connectTarget(key); }
    });
}

bool GuiConnectionController::choose(const StationStartupSelection& choice, bool startConnection)
{
    QString error;
    if (!m_sessions.canReplace(choice, &error)) {
        m_selector->setNotice(error);
        return false;
    }
    const QString previous = m_store.selectedId();
    const bool persist = m_storeLoaded && !choice.savedId.isEmpty();
    if (!m_storeLoaded && choice.connection.isRemote()) {
        m_selector->setNotice(tr("The saved Core list must be recovered before selecting a Core."));
        return false;
    }
    if (persist && !m_store.select(choice.savedId, &error)) {
        m_selector->setNotice(error);
        return false;
    }
    if (!m_sessions.replace(choice, startConnection, &error)) {
        if (persist) { m_store.select(previous); }
        m_selector->setNotice(error);
        return false;
    }
    m_selector->setNotice(m_storeLoaded ? QString()
        : tr("Using this computer's Core. The damaged saved Core list has been preserved."));
    return true;
}

void GuiConnectionController::connectTarget(const QString& key)
{
    if (key == QLatin1String("local")) {
        if (choose({}, false)) { scan(); }
    } else if (key == QLatin1String("current")) {
        if (m_remoteControls) { m_remoteControls->connectToStation(); }
    } else if (key.startsWith(QLatin1String("saved:"))) {
        const auto target = m_store.target(key.mid(6));
        if (!target) { m_selector->setNotice(tr("That saved Core is no longer available.")); return; }
        const RemoteStationOptions options = connectionOptionsForTarget(*target);
        if (!isReadyToConnect(options)) {
            if (target->connection.url.isEmpty() && target->connection.isValidRemoteTarget()) {
                const QString reason = !target->connection.reachFromAnywhere
                    ? tr("Turn on remote access for this Core in Edit to connect.")
                    : !target->connection.serviceConnectRefusal().isEmpty()
                        ? target->connection.serviceConnectRefusal()
                        : tr("Remote access servers are unavailable. Check this computer's remote access setting.");
                m_selector->setNotice(reason);
                return;
            }
            editCore(target->id);
            return;
        }
        const auto current = m_sessions.selection();
        if (current.savedId == target->id && sameConnection(current.connection, target->connection)) {
            if (m_remoteControls) { m_remoteControls->connectToStation(); }
        } else {
            choose({options, target->id, {}}, true);
        }
    } else if (key.startsWith(QLatin1String("lan:"))) {
        for (const StationLanEndpoint& endpoint : m_lan.endpoints()) {
            if (key != QStringLiteral("lan:") + endpoint.key()) { continue; }
            const auto matches = matchingSavedCores(endpoint, m_store.targets());
            if (matches.size() == 1) {
                StationStartupSelection selection{matches.first().connection, matches.first().id, {}};
                selection.savedAddressBeforeDiscovery = selection.connection.url;
                selection.connection.url = endpoint.url().toString();
                selection.connection.allowUnpinned = false;
                const auto current = m_sessions.selection();
                if (current.savedId == selection.savedId
                    && sameConnection(current.connection, selection.connection)) {
                    if (m_remoteControls) { m_remoteControls->connectToStation(); }
                } else {
                    choose(selection, true);
                }
            } else if (matches.isEmpty() && takesNewDevices(endpoint.announcement)) {
                pairTarget(key);
            } else if (matches.isEmpty()) {
                editCore(); // Address/name only. Never adopt an advertised pin.
            } else {
                m_selector->setNotice(tr("Several saved entries use this Core identity. Choose the intended entry under Your Cores."));
            }
            refresh();
            return;
        }
        m_selector->setNotice(tr("That Core announcement has expired. Scan again or use its saved address."));
    } else if (key.startsWith(QLatin1String("radio:"))) {
        const QString mac = key.mid(6);
        const auto saved = AppSettings::instance().savedRadio(mac);
        RadioInfo radio = m_radios.contains(mac) ? m_radios.value(mac)
            : saved ? saved->info : RadioInfo{};
        if (radio.macAddress.isEmpty() || radio.address.isNull() || radio.port == 0) {
            m_selector->setNotice(tr("That radio has no usable address. Edit it or scan again."));
            return;
        }
        if (!choose({}, false)) { return; }
        // Reuse the existing local-radio persistence and connection APIs.
        AppSettings& settings = AppSettings::instance();
        const HPSDRModel modelOverride = settings.modelOverride(mac);
        if (modelOverride != HPSDRModel::FIRST) { radio.modelOverride = modelOverride; }
        settings.saveRadio(radio, saved ? saved->pinToMac : false, true);
        settings.setLastConnected(radio.macAddress);
        if (!settings.save()) { m_selector->setNotice(tr("The local radio preference could not be saved.")); }
        if (radio.inUse) { m_selector->setNotice(tr("This radio reports in use; attempting the explicitly selected connection.")); }
        m_sessions.window()->radioModel()->connectToRadio(radio);
    }
    refresh();
}

void GuiConnectionController::disconnectCurrent()
{
    ++m_request;
    if (m_remoteControls) { m_remoteControls->disconnectFromStation(); }
    else if (m_sessions.window()) { m_sessions.window()->radioModel()->disconnectFromRadio(); }
    refresh();
}

void GuiConnectionController::editCore(const QString& id)
{
    if (!m_storeLoaded) {
        m_selector->setNotice(tr("The saved Core list could not be loaded. Correct the settings file before changing it."));
        return;
    }
    SavedCoreTarget initial;
    if (!id.isEmpty()) {
        const auto target = m_store.target(id);
        if (!target) { return; }
        initial = *target;
    } else {
        initial.id = CoreTargetStore::createId();
        if (m_selector->selectedKey() == QLatin1String("current")) {
            initial.connection = m_sessions.selection().connection;
            initial.label = endpointText(initial.connection);
        } else {
            for (const StationLanEndpoint& endpoint : m_lan.endpoints()) {
                if (m_selector->selectedKey() == QStringLiteral("lan:") + endpoint.key()) {
                    initial.label = endpoint.announcement.displayName();
                    initial.connection.url = endpoint.url().toString();
                    break;
                }
            }
        }
    }
    CoreTargetEditor editor(initial, m_selector.get());
    if (!id.isEmpty()) {
        editor.setCurrentOptionsSource([this, id]() -> std::optional<RemoteStationOptions> {
            observeNetworkGeneration();
            if (const auto current = m_store.target(id)) { return current->connection; }
            return std::nullopt;
        });
    }
    if (editor.exec() != QDialog::Accepted) { return; }
    QString error;
    if (!m_store.upsert(editor.target(), &error)) { m_selector->setNotice(error); return; }
    refresh();
    m_selector->setSelectedKey(QStringLiteral("saved:") + initial.id);
    m_selector->setNotice(tr("Core saved. Select Connect to use it."));
}

void GuiConnectionController::editRadio(const QString& mac)
{
    AddCustomRadioDialog editor(m_selector.get());
    const auto saved = AppSettings::instance().savedRadio(mac);
    if (!mac.isEmpty()) {
        const RadioInfo info = m_radios.contains(mac) ? m_radios.value(mac)
            : saved ? saved->info : RadioInfo{};
        if (info.macAddress.isEmpty()) { return; }
        editor.setEditTarget(info, saved ? saved->pinToMac : false, saved ? saved->autoConnect : false);
    }
    if (editor.exec() != QDialog::Accepted) { return; }
    const RadioInfo radio = editor.result();
    AppSettings& settings = AppSettings::instance();
    if (!mac.isEmpty() && radio.macAddress != mac) { settings.forgetRadio(mac); m_radios.remove(mac); }
    settings.saveRadio(radio, editor.pinToMac(), editor.autoConnect());
    if (!settings.save()) { m_selector->setNotice(tr("The local radio could not be saved.")); }
    m_radios.insert(radio.macAddress, radio);
    refresh();
    m_selector->setSelectedKey(QStringLiteral("radio:") + radio.macAddress);
    if (!editor.savedOffline()) { queueConnect(QStringLiteral("radio:") + radio.macAddress); }
}

void GuiConnectionController::forgetTarget(const QString& key)
{
    QString error;
    if (key.startsWith(QLatin1String("saved:"))) {
        if (!m_store.remove(key.mid(6), &error)) { m_selector->setNotice(error); return; }
    } else if (key.startsWith(QLatin1String("radio:"))) {
        const QString mac = key.mid(6);
        AppSettings::instance().forgetRadio(mac);
        if (!AppSettings::instance().save()) { m_selector->setNotice(tr("The updated radio list could not be saved.")); return; }
        m_radios.remove(mac);
        m_seenAt.remove(mac);
    }
    m_selector->setNotice(tr("Saved entry forgotten. The current connection is unchanged."));
    refresh();
}

void GuiConnectionController::showDetails(const QString& key)
{
    if (key == QLatin1String("local")) {
        m_selector->setNotice(tr("This computer runs its own Core and does its own signal processing. Choose a radio under Radios on this network to operate it directly from this computer."));
        return;
    }
    if (key == QLatin1String("current")) {
        if (m_remoteControls) { m_selector->setNotice(m_remoteControls->detailText()); return; }
    }
    if (key.startsWith(QLatin1String("saved:"))) {
        const auto target = m_store.target(key.mid(6));
        if (target) {
            QString detail = savedCoreDetails(*target);
            if (target->id == m_sessions.selection().savedId && m_remoteControls) {
                detail += tr("\n\nCurrent connection:\n%1").arg(m_remoteControls->detailText());
            }
            m_selector->setNotice(detail);
        }
    } else if (key.startsWith(QLatin1String("lan:"))) {
        for (const StationLanEndpoint& endpoint : m_lan.endpoints()) {
            if (key == QStringLiteral("lan:") + endpoint.key()) {
                const auto& advertised = endpoint.announcement;
                const QString next = lanCoreNextStep(advertised);
                m_selector->setNotice(tr("Core seen on this network, not verified: %1\nAddress: %2\nRadio MAC: %3\n%4")
                    .arg(advertised.displayName(), endpointText(endpoint.url()),
                         advertised.radioMac, next));
                return;
            }
        }
    } else if (key.startsWith(QLatin1String("radio:"))) {
        const QString mac = key.mid(6);
        m_selector->setNotice(tr("Local radio: %1\nThis computer runs the Core and does the signal processing. Edit to see its saved address and model.").arg(mac));
    }
}

bool GuiConnectionController::observationLeaseCurrent(StationClient* client) const
{
    if (!client || !client->isHandshakeComplete() || m_windowTargetIncarnation == 0
        || m_windowCoordinatorGeneration != m_sessions.generation()
        || m_sessions.selection().savedId != m_windowTargetId
        || m_store.targetIncarnation(m_windowTargetId) != m_windowTargetIncarnation) { return false; }
    const auto target = m_store.target(m_windowTargetId);
    if (!target || !authenticatedSelectionMatchesSaved(m_sessions.selection(), *target)) { return false; }
    return target->connection.identityFingerprint.isEmpty()
        || (!target->connection.allowUnpinned && client->signedInWithDeviceKey()
            && target->connection.identityFingerprint.size() == 32
            && client->stationIdentityFingerprint() == target->connection.identityFingerprint);
}

void GuiConnectionController::rememberAuthenticatedRadio()
{
    if (!m_storeLoaded || !m_sessions.window()) { return; }
    auto* client = m_sessions.window()->findChild<StationClient*>();
    if (!observationLeaseCurrent(client)) { return; }
    auto target = m_store.target(m_sessions.selection().savedId);
    if (!target || !authenticatedSelectionMatchesSaved(m_sessions.selection(), *target)) { return; }
    // iPhone app plan Task 27 (R-IOS-16): where this computer reached the
    // Core, tried first next time.
    if (client->connectedUrl().isValid()) {
        QString error;
        if (!m_store.rememberAddress(target->id, client->connectedUrl().toString(), &error)) {
            m_selector->setNotice(error);
        }
        target = m_store.target(target->id);
        if (!target) { return; }
    }
    const auto& caps = client->capabilities();
    // Link 7.1: only this authenticated paired identity, after negotiated
    // minor/capability support. Empty/absent/malformed lists retain history.
    if (client->agreedMinor() >= 11 && caps.coreAddressesVersion >= 1
        && client->signedInWithDeviceKey()
        && !target->connection.identityFingerprint.isEmpty()) {
        QString error;
        if (!m_store.rememberCoreAddresses(target->id, client->stationIdentityFingerprint(),
                                          client->remoteDevices()->coreInfo().coreAddresses, &error)) {
            m_selector->setNotice(error);
        }
        target = m_store.target(target->id);
        if (!target) { return; }
    }
    if (!target->connection.identityFingerprint.isEmpty() && !target->connection.allowUnpinned) {
        QList<QUrl> addresses;
        for (const QString& url : target->connection.cachedAddresses + target->connection.coreAddresses) {
            if (!addresses.contains(QUrl(url))) { addresses.append(QUrl(url)); }
        }
        client->setCachedAddresses(addresses);
    }
    // iPhone app plan Task 29 (R-IOS-16; link section 21.1): the Core's
    // rendezvous id and whether it allows the relay, so the next connect
    // races the internet service beside its addresses.
    if (!target->connection.identityFingerprint.isEmpty()
        && (target->connection.rendezvousId != client->stationRendezvousId()
            || (caps.relayAllowedEntry
                && target->connection.relayAllowed != (caps.relayAllowed ? 1 : 0)))) {
        QString error;
        if (!m_store.rememberServiceRoute(target->id, client->stationRendezvousId(),
                                          caps.relayAllowedEntry ? (caps.relayAllowed ? 1 : 0)
                                                                 : -1,
                                          &error)) {
            m_selector->setNotice(error);
        }
        target = m_store.target(target->id);
        if (!target) { return; }
    }
    if (!caps.radioConnected || caps.macAddress.isEmpty()) { return; }
    if (target->lastRadioName == caps.stationName && target->lastRadioMac == caps.macAddress) { return; }
    target->lastRadioName = caps.stationName;
    target->lastRadioMac = caps.macAddress;
    QString error;
    if (!m_store.upsert(*target, &error)) { m_selector->setNotice(error); }
}

void GuiConnectionController::rememberAuthenticatedCapability()
{
    if (!m_storeLoaded || !m_sessions.window()) { return; }
    auto* client = m_sessions.window()->findChild<StationClient*>();
    if (!observationLeaseCurrent(client)) { return; }
    const auto target = m_store.target(m_sessions.selection().savedId);
    if (!target || !authenticatedSelectionMatchesSaved(m_sessions.selection(), *target)
        || target->connection.identityFingerprint.isEmpty()) { return; }
    QString error;
    if (!m_store.rememberControlChannelVersion(target->id,
                                               client->capabilities().controlChannelVersion,
                                               &error)) {
        m_selector->setNotice(error);
    }
}

void GuiConnectionController::observeNetworkGeneration()
{
    if (!m_storeLoaded) { return; }
    QString error;
    if (!m_store.invalidateFutureNegativeObservations(&error)) {
        m_selector->setNotice(error);
        return;
    }
    if (!m_store.observeNetworkFingerprint(networkFingerprint(), &error)) {
        m_selector->setNotice(error);
    }
}

// ── iPhone app Task 18 (R-IOS-08): pairing ───────────────────────────────

StationPairingClient* GuiConnectionController::pairingClient()
{
    if (!m_pairing) {
        m_pairing = std::make_unique<StationPairingClient>(
            ClientDeviceIdentity::forThisProfile(),
            ClientDeviceIdentity::machineName(AppSettings::profileOverride()));
        connect(m_pairing.get(), &StationPairingClient::paired,
                this, &GuiConnectionController::onPaired);
        connect(m_pairing.get(), &StationPairingClient::failed,
                this, &GuiConnectionController::onPairingFailed);
    }
    return m_pairing.get();
}

void GuiConnectionController::pairTarget(const QString& key)
{
    if (m_shuttingDown) { return; }
    if (!m_storeLoaded) {
        m_selector->setNotice(tr("The saved Core list could not be loaded. Correct the settings file before pairing."));
        return;
    }
    for (const StationLanEndpoint& endpoint : m_lan.endpoints()) {
        if (key != QStringLiteral("lan:") + endpoint.key()) { continue; }
        const QUrl url = endpoint.url();
        if (oneClickPairs(endpoint.announcement)) {
            m_selector->setNotice(tr("Pairing with %1…").arg(endpoint.announcement.displayName()));
            pairingClient()->pairOnThisNetwork(url.host(), static_cast<quint16>(url.port()));
        } else {
            // The Core takes only its code: ask for it, the address filled in.
            addByCode(endpointText(url));
        }
        return;
    }
    m_selector->setNotice(tr("That Core announcement has expired. Scan again or add it by code."));
}

void GuiConnectionController::addByCode(const QString& address)
{
    if (m_shuttingDown) { return; }
    if (!m_storeLoaded) {
        m_selector->setNotice(tr("The saved Core list could not be loaded. Correct the settings file before pairing."));
        return;
    }
    AddCoreByCodeDialog dialog(address, m_selector.get());
    if (dialog.exec() != QDialog::Accepted) { return; }
    // The nested dialog event loop can process application shutdown.
    if (m_shuttingDown) { return; }
    m_selector->setNotice(tr("Pairing with the Core by its code…"));
    if (dialog.host().isEmpty()) {
        pairingClient()->pairByCodeFromAnywhere(dialog.code(), configuredRemoteAccessServers());
    } else {
        pairingClient()->pairByCode(dialog.code(), dialog.host(), dialog.port());
    }
}

void GuiConnectionController::onPaired(const PairedStationRecord& record)
{
    if (m_shuttingDown) { return; }
    if (!StationIdentity::isP256Spki(record.identityKey)
        || record.identityFingerprint.size() != 32
        || StationIdentity::fingerprintOf(record.identityKey) != record.identityFingerprint
        || (record.host.isEmpty() != (record.port == 0))) {
        m_selector->setNotice(tr("The Core's pairing identity could not be verified. Pair again."));
        return;
    }
    const QString rendezvousId = RendezvousWire::rendezvousId(record.identityKey);
    if (!RendezvousWire::isRendezvousId(rendezvousId)) {
        m_selector->setNotice(tr("The Core's remote access identity could not be verified. Pair again."));
        return;
    }
    // One saved Core per identity: pairing again with a Core already under
    // Your Cores updates that entry's address rather than adding another.
    SavedCoreTarget target;
    target.id = CoreTargetStore::createId();
    for (const SavedCoreTarget& existing : m_store.targets()) {
        if (existing.connection.identityFingerprint == record.identityFingerprint) {
            target = existing;
            break;
        }
    }
    if (!record.label.isEmpty()) { target.label = record.label; }
    if (!record.host.isEmpty()) {
        target.connection.url = StationPairingClient::coreUrl(record.host, record.port).toString();
    }
    target.connection.identityFingerprint = record.identityFingerprint;
    target.connection.rendezvousId = rendezvousId;
    if (target.connection.url.isEmpty()) {
        target.connection.token.clear();
        target.connection.fingerprint.clear();
        target.connection.allowUnpinned = false;
    }
    if (target.label.isEmpty()) { target.label = endpointText(target.connection); }
    QString error;
    if (!m_store.upsert(target, &error)) {
        m_selector->setNotice(error);
        return;
    }
    refresh();
    m_selector->setSelectedKey(QStringLiteral("saved:") + target.id);
    m_selector->setNotice(tr("Paired with %1. Connecting…").arg(target.label));
    queueConnect(QStringLiteral("saved:") + target.id);
}

void GuiConnectionController::onPairingFailed(const QString& reason)
{
    if (m_shuttingDown) { return; }
    // The Core's reasons are plain words already; anything else is worded
    // for the operator (R-R3-17).
    m_selector->setNotice(OperatorReasonText::forDisplay(reason));
}

void GuiConnectionController::rememberStationIdentity(const QByteArray& identityFingerprint)
{
    // The saved Core this window signed in to is trusted by its identity
    // from now on: it shows as paired and connects by key, nothing typed.
    const StationStartupSelection current = m_sessions.selection();
    auto target = m_store.target(current.savedId);
    const bool matches = target && selectionMatchesSaved(current, *target);
    m_sessions.noteStationIdentity(identityFingerprint);
    if (!m_storeLoaded || !matches) {
        refresh();
        return;
    }
    target->connection.identityFingerprint = identityFingerprint;
    QString error;
    if (!m_store.upsert(*target, &error)) {
        m_selector->setNotice(error);
    }
    refresh();
}
} // namespace NereusSDR
