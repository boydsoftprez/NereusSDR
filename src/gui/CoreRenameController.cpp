// no-port-check: NereusSDR-original.
// 2026-10-02 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "gui/CoreRenameController.h"
#include "gui/GuiConnectionController.h"
#include "gui/RemoteConnectionController.h"
#include "core/security/ClientDeviceIdentity.h"
#include "core/security/StationLabel.h"
#include "core/session/RemoteDevicesState.h"
namespace NereusSDR {
namespace {
bool sameTrust(const RemoteStationOptions& a, const RemoteStationOptions& b) {
    return QUrl(a.url) == QUrl(b.url) && a.token == b.token && a.fingerprint == b.fingerprint
        && a.identityFingerprint == b.identityFingerprint && a.allowUnpinned == b.allowUnpinned
        && a.reachFromAnywhere == b.reachFromAnywhere && a.relayAllowed == b.relayAllowed;
}
}
CoreRenameController::CoreRenameController(CoreTargetStore& store, SessionSource source,
    std::shared_ptr<const ClientDeviceIdentity> identity, RequestAuthority authority, QObject* parent,
    TemporaryFactory factory, Starter starter, Limits limits)
    : QObject(parent), m_store(store), m_source(std::move(source)), m_identity(std::move(identity)),
      m_authority(std::move(authority)), m_factory(std::move(factory)), m_starter(std::move(starter)), m_limits(limits)
{
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (!m_operation) { return; }
        if (m_operation->accepted) { acceptedName(); }
        else if (m_operation->commandId != 0) {
            finish(Outcome::UnknownOutcome, {}, QStringLiteral("The Core did not answer. The rename may have happened; it was not sent again."));
        } else { finish(Outcome::TimedOut, {}, QStringLiteral("The Core did not finish signing in for rename.")); }
    });
}
CoreRenameController::~CoreRenameController() {
    ++m_generation;
    m_timer.stop();
    if (m_operation) {
        for (const auto& connection : m_operation->connections) { disconnect(connection); }
        m_operation.reset();
    }
    if (m_temporary) {
        m_temporary->setAdmissionGuard([] { return false; });
        m_temporary->leaveSession();
        m_temporary->disconnectFromStation(QStringLiteral("rename closed"));
    }
}
void CoreRenameController::inspectTarget(const QString& id) {
    const quint64 incarnation = m_store.targetIncarnation(id);
    if (m_inspectedId != id || incarnation != m_inspectedIncarnation) {
        const QPointer<CoreRenameController> self(this);
        const quint64 expected = m_generation + 1;
        cancel();
        if (!self || m_generation != expected) { return; }
    }
    m_inspectedId = id;
    m_inspectedIncarnation = incarnation;
}
bool CoreRenameController::ownsTemporaryAdmission(const QByteArray& identity) const {
    if (m_operation && m_operation->temporary && m_operation->request.pairedIdentity == identity) { return true; }
    for (const Drain& drain : m_draining) {
        if (drain.client && drain.pairedIdentity == identity) { return true; }
    }
    return false;
}
bool CoreRenameController::current(const Operation& operation, bool requireClient) const {
    if (operation.generation != m_generation || !m_operation || m_operation->generation != operation.generation
        || m_inspectedId != operation.request.targetId || !m_authority || !m_source
        || !m_identity || !m_identity->isValid()) { return false; }
    const QPointer<const CoreRenameController> self(this);
    const RequestAuthority authority = m_authority;
    const SessionSource source = m_source;
    if (!authority(operation.request) || !self) { return false; }
    const Snapshot snapshot = source(operation.request.pairedIdentity);
    if (!self || operation.generation != m_generation || !m_operation) { return false; }
    const auto target = m_store.target(operation.request.targetId);
    if (!target || operation.request.incarnation == 0
        || m_store.targetIncarnation(operation.request.targetId) != operation.request.incarnation
        || target->connection.identityFingerprint != operation.request.pairedIdentity
        || operation.request.pairedIdentity.size() != 32 || target->connection.allowUnpinned
        || !sameTrust(target->connection, operation.trust)
        || snapshot.generation != operation.coordinatorGeneration
        || snapshot.deviceIdentity != m_identity->fingerprint()
        || (operation.temporary && snapshot.sameCoreActive)) { return false; }
    return !requireClient || (operation.client && operation.client->sessionEpoch() == operation.clientEpoch
        && operation.client->isHandshakeComplete() && operation.client->signedInWithDeviceKey()
        && operation.client->stationIdentityFingerprint() == operation.request.pairedIdentity
        && operation.client->deviceIdentityFingerprint() == m_identity->fingerprint());
}
bool CoreRenameController::admissionAllowed(quint64 generation) const {
    if (!m_operation || m_operation->generation != generation) { return false; }
    const Operation operation = *m_operation;
    return current(operation);
}
void CoreRenameController::rename(const Request& request) {
    const QPointer<CoreRenameController> self(this);
    const quint64 expectedAfterCancel = m_generation + 1;
    cancel();
    if (!self || m_generation != expectedAfterCancel) { return; }
    const quint64 generation = ++m_generation;
    const auto label = StationLabel::parse(request.name);
    const auto target = m_store.target(request.targetId);
    if (!label || request.operationId == 0 || !target || request.targetId != m_inspectedId
        || request.incarnation == 0 || request.incarnation != m_inspectedIncarnation
        || m_store.targetIncarnation(request.targetId) != request.incarnation
        || request.pairedIdentity.size() != 32 || target->connection.identityFingerprint != request.pairedIdentity
        || target->connection.allowUnpinned) {
        notify(request, !label ? Outcome::Refused : Outcome::StaleTarget, {},
               !label ? StationLabel::ruleText() : QStringLiteral("The inspected saved Core changed. Open it again."));
        return;
    }
    if (!m_source || !m_authority || !m_identity || !m_identity->isValid()) {
        notify(request, Outcome::Refused, {}, QStringLiteral("This computer's existing paired device key or rename authority is unavailable."));
        return;
    }
    const RequestAuthority authority = m_authority;
    const SessionSource source = m_source;
    if (!authority(request) || !self || generation != m_generation) {
        if (self && generation == m_generation) { notify(request, Outcome::StaleTarget, {}, QStringLiteral("The rename authority changed.")); }
        return;
    }
    const Snapshot snapshot = source(request.pairedIdentity);
    if (!self || generation != m_generation) { return; }
    Operation operation;
    operation.request = request;
    operation.trust = target->connection;
    operation.generation = generation;
    operation.coordinatorGeneration = snapshot.generation;
    operation.normalizedName = label->display();
    m_operation = operation;
    if (!current(operation)) {
        if (!self || generation != m_generation) { return; }
        finish(Outcome::StaleTarget, {}, QStringLiteral("The saved Core or desktop session changed."));
        return;
    }
    if (snapshot.client && snapshot.client->isHandshakeComplete()
        && snapshot.client->stationIdentityFingerprint() == request.pairedIdentity
        && snapshot.client->deviceIdentityFingerprint() == m_identity->fingerprint()
        && snapshot.client->deviceAdminAvailable()) {
        m_operation->client = snapshot.client;
        m_operation->clientEpoch = snapshot.client->sessionEpoch();
        bindClient();
        sendRename();
        return;
    }
    if (snapshot.sameCoreActive) {
        finish(Outcome::Refused, {}, QStringLiteral("This Core already has a desktop connection or retry. Wait for it or disconnect it before renaming."));
        return;
    }
    const RemoteStationOptions options = GuiConnectionController::connectionOptionsForTarget(*target);
    if (!GuiConnectionController::isReadyToConnect(options)) {
        finish(Outcome::Refused, {}, QStringLiteral("No retained direct listener or available remote access route can reach this paired Core."));
        return;
    }
    for (const Drain& draining : m_draining) {
        if (draining.client && draining.pairedIdentity == request.pairedIdentity) {
            finish(Outcome::Refused, {}, QStringLiteral("The earlier temporary rename session is still closing. Try again after it finishes."));
            return;
        }
    }
    m_operation->temporary = true;
    const TemporaryFactory factory = m_factory;
    std::unique_ptr<StationClient> temporary = factory ? factory()
        : std::make_unique<StationClient>(nullptr, nullptr, nullptr, LinkVersion::supportedMajors(), StationClient::SessionPurpose::RenameOnly);
    if (!self || generation != m_generation) {
        if (temporary) { temporary->disconnectFromStation(QStringLiteral("rename cancelled")); }
        return;
    }
    if (!temporary || temporary->sessionPurpose() != StationClient::SessionPurpose::RenameOnly
        || temporary->isConnectionActive() || !admissionAllowed(generation)) {
        if (!self || generation != m_generation) { return; }
        if (temporary) { temporary->disconnectFromStation(QStringLiteral("rename refused")); }
        finish(Outcome::Refused, {}, QStringLiteral("A temporary rename session is no longer permitted."));
        return;
    }
    temporary->setParent(this);
    temporary->setDeviceIdentity(m_identity, ClientDeviceIdentity::machineName(), ClientDeviceIdentity::machineShortName());
    temporary->setAdmissionGuard([self, generation] { return self && self->admissionAllowed(generation); });
    m_temporary = std::move(temporary);
    m_operation->client = m_temporary.get();
    bindClient();
    m_timer.start(qMax(1, m_limits.admissionMs));
    if (!admissionAllowed(generation)) {
        if (self && generation == m_generation) { finish(Outcome::StaleTarget, {}, QStringLiteral("The desktop connection changed.")); }
        return;
    }
    const Starter starter = m_starter;
    if (starter) { starter(*m_temporary, options); }
    else {
        QList<QUrl> addresses;
        for (const QString& address : options.directCandidates) { addresses.append(QUrl(address)); }
        m_temporary->setCachedAddresses(addresses);
        StationClient::ServiceRoute route;
        if (options.reachFromAnywhere && !options.rendezvousId.isEmpty()) {
            route.servers = configuredRemoteAccessServers();
            route.rendezvousId = options.rendezvousId;
            route.relayAllowed = options.relayAllowed != 0;
            route.controlChannelVersion = options.effectiveControlChannelVersion(QDateTime::currentMSecsSinceEpoch());
        }
        m_temporary->setServiceRoute(route);
        m_temporary->connectToStation(QUrl(options.url), {}, {}, false, request.pairedIdentity);
    }
}
void CoreRenameController::bindClient() {
    if (!m_operation || !m_operation->client) { return; }
    const quint64 generation = m_operation->generation;
    const QPointer<StationClient> client = m_operation->client;
    auto& connections = m_operation->connections;
    connections.append(connect(client, &StationClient::handshakeComplete, this, [this, generation] {
        if (m_operation && m_operation->generation == generation) { sendRename(); }
    }));
    connections.append(connect(client->remoteDevices(), &RemoteDevicesState::coreInfoChanged, this, [this, generation] {
        if (m_operation && m_operation->generation == generation) { observeName(); }
    }));
    connections.append(connect(client->remoteDevices(), &RemoteDevicesState::heldChanged, this, [this, generation, client] {
        if (m_operation && m_operation->generation == generation && client && client->remoteDevices()->held()) {
            finish(Outcome::Refused, {}, QStringLiteral("The Core is full. Rename did not take another device's place."));
        }
    }));
    connections.append(connect(client, &StationClient::commandResponse, this, [this, generation, client](const SessionMessage& message) {
        if (!client) { return; }
        const quint32 epoch = client->sessionEpoch();
        const bool proved = client->isHandshakeComplete() && client->signedInWithDeviceKey();
        QTimer::singleShot(0, this, [this, generation, client, epoch, proved, message] {
            if (!m_operation || m_operation->generation != generation || m_operation->client != client
                || m_operation->clientEpoch != epoch || !client || client->sessionEpoch() != epoch
                || m_operation->commandId == 0 || message.commandId != m_operation->commandId
                || message.commandVerb != QByteArrayLiteral("station.rename") || !proved) { return; }
            const Operation operation = *m_operation;
            const QPointer<CoreRenameController> self(this);
            if (!current(operation)) {
                if (self && generation == m_generation) { finish(Outcome::StaleTarget, {}, QStringLiteral("The saved Core changed before its answer.")); }
                return;
            }
            if (!message.accepted) { finish(Outcome::Refused, {}, message.reason); return; }
            // A synchronous mirror update may precede invokeCommand's return.
            observeName();
            if (!self || !m_operation || m_operation->generation != generation) { return; }
            m_operation->accepted = true;
            if (!m_operation->observedName.isEmpty()) { acceptedName(); }
            else { m_timer.start(qMax(1, m_limits.nameMs)); }
        });
    }));
    connections.append(connect(client, &StationClient::sessionEnded, this, [this, generation](const QString& reason) {
        QTimer::singleShot(0, this, [this, generation, reason] {
            if (!m_operation || m_operation->generation != generation) { return; }
            if (m_operation->accepted) { acceptedName(); }
            else { finish(m_operation->commandId ? Outcome::UnknownOutcome : Outcome::Refused, {}, reason); }
        });
    }));
}
void CoreRenameController::sendRename() {
    if (!m_operation || !m_operation->client || m_operation->commandId != 0) { return; }
    m_operation->clientEpoch = m_operation->client->sessionEpoch();
    const Operation operation = *m_operation;
    const QPointer<CoreRenameController> self(this);
    if (!current(operation, true) || !operation.client->deviceAdminAvailable()) {
        if (self && operation.generation == m_generation) { finish(Outcome::Refused, {}, QStringLiteral("This authenticated Core session does not support rename, or its authority changed.")); }
        return;
    }
    m_operation->baselineName = operation.client->remoteDevices()->coreInfo().stationLabel;
    m_timer.start(qMax(1, m_limits.commandMs));
    const quint32 id = operation.client->invokeCommand(QByteArrayLiteral("station.rename"),
        {MirrorUpdate{0, QByteArrayLiteral("label"), MirrorWireKind::Utf8, operation.normalizedName}});
    if (!self || !m_operation || m_operation->generation != operation.generation) { return; }
    m_operation->commandId = id;
    if (id == 0) { finish(Outcome::Refused, {}, QStringLiteral("The rename command could not be sent.")); }
}
void CoreRenameController::observeName() {
    if (!m_operation || !m_operation->client || m_operation->commandId == 0) { return; }
    const Operation operation = *m_operation;
    const QPointer<CoreRenameController> self(this);
    if (!current(operation, true) || !self) { return; }
    const auto label = StationLabel::parse(operation.client->remoteDevices()->coreInfo().stationLabel);
    if (label && (label->display() != operation.baselineName
                  || StationLabel::sameLabel(label->display(), operation.normalizedName))) {
        m_operation->observedName = label->display();
        if (m_operation->accepted) { acceptedName(); }
    }
}
void CoreRenameController::acceptedName() {
    if (!m_operation || !m_operation->accepted) { return; }
    const Operation operation = *m_operation;
    const QString name = operation.observedName.isEmpty() ? operation.normalizedName : operation.observedName;
    const QPointer<CoreRenameController> self(this);
    if (!current(operation)) {
        if (!self || operation.generation != m_generation) { return; }
        finish(Outcome::AcceptedLocalSaveFailed, name, QStringLiteral("The Core accepted the rename, but the saved target changed before this computer could remember it."));
        return;
    }
    QString error;
    if (!m_store.rememberCoreName(operation.request.targetId, operation.request.pairedIdentity, name, &error)) {
        finish(Outcome::AcceptedLocalSaveFailed, name, QStringLiteral("The Core accepted the rename, but this computer could not save its name: %1").arg(error));
    } else { finish(Outcome::Accepted, name, {}); }
}
void CoreRenameController::cancel() {
    if (!m_operation) { ++m_generation; return; }
    finish(Outcome::Cancelled, {}, QStringLiteral("Rename cancelled. A command already sent may still have happened on the Core."));
}
void CoreRenameController::finish(Outcome outcome, const QString& name, const QString& reason) {
    if (!m_operation) { return; }
    const Operation operation = *m_operation;
    m_operation.reset();
    ++m_generation;
    m_timer.stop();
    for (const auto& connection : operation.connections) { disconnect(connection); }
    const QPointer<CoreRenameController> self(this);
    if (m_temporary) { cleanup(m_temporary.release(), operation.request.pairedIdentity); }
    if (self) { notify(operation.request, outcome, name, reason); }
}
void CoreRenameController::notify(const Request& request, Outcome outcome, const QString& name, const QString& reason) {
    QTimer::singleShot(0, this, [this, request, outcome, name, reason] { emit finished(request, outcome, name, reason); });
}
void CoreRenameController::cleanup(StationClient* client, const QByteArray& identity) {
    // Only an owned temporary client reaches here. Retirement precedes leave.
    m_draining.removeIf([](const Drain& old) { return old.client.isNull(); });
    m_draining.append({client, identity});
    client->setAdmissionGuard([] { return false; });
    const QPointer<StationClient> guarded(client);
    const auto close = [guarded] {
        if (guarded) { guarded->disconnectFromStation(QStringLiteral("rename finished")); guarded->deleteLater(); }
    };
    const quint32 epoch = client->sessionEpoch();
    auto command = std::make_shared<quint32>(0);
    connect(client, &StationClient::commandResponse, client, [guarded, epoch, command, close](const SessionMessage& message) {
        if (!guarded) { return; }
        QTimer::singleShot(0, guarded, [guarded, epoch, command, close, message] {
            if (guarded && guarded->sessionEpoch() == epoch && *command != 0
                && message.commandId == *command && message.commandVerb == QByteArrayLiteral("session.leave")) { close(); }
        });
    });
    connect(client, &StationClient::sessionEnded, client, close);
    QTimer::singleShot(qMax(1, m_limits.cleanupMs), client, close);
    *command = client->leaveSession();
    if (*command == 0) { close(); }
}
} // namespace NereusSDR
