// no-port-check: NereusSDR-original CAT station admission/lifecycle.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "CatService.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "models/RadioModel.h"
#include <utility>
namespace NereusSDR {
CatService::CatService(RadioModel& model, QObject* parent)
    : QObject(parent), m_model(&model), m_adapter(model), m_txCoordinator(model), m_settings(AppSettings::instance()), m_rxCommands(m_adapter, m_txCoordinator, m_settings), m_dspCommands(m_adapter), m_txCommands(m_adapter,m_txCoordinator,m_settings), m_globalCommands(m_adapter,m_settings), m_parser(m_catalog)
{
    for (const QByteArray& code : CatRxCommands::codes()) {
        m_router.registerHandler(code, [this](const CatRequest& request, CatSessionContext& context) { return m_rxCommands.execute(request, context); });
    }
    for (const QByteArray& code : CatDspCommands::codes()) {
        m_router.registerHandler(code, [this](const CatRequest& request, CatSessionContext& context) { return m_dspCommands.execute(request, context); });
    }
    for (const QByteArray& code : CatTxCommands::codes()) {
        m_router.registerHandler(code, [this](const CatRequest& request, CatSessionContext& context) { return m_txCommands.execute(request, context); });
    }
    for (const QByteArray& code : CatGlobalCommands::codes()) {
        m_router.registerHandler(code, [this](const CatRequest& request, CatSessionContext& context) { return m_globalCommands.execute(request, context); });
    }
    m_reporter = std::make_unique<CatReporter>(*this, model);
    for (int channel = 1; channel <= 4; ++channel) { m_channels[channel - 1].config.channel = channel; }
    connect(&model, &RadioModel::connectionStateChanged, this, [this](ConnectionState state) {
        if (state == ConnectionState::Disconnected || state == ConnectionState::LinkLost) {
            const QPointer<CatService> self(this);
            m_txCoordinator.cancelAll();
            if (!self) { return; }
            emit radioDisconnected();
        }
    });
}
CatService::~CatService() { beginRetirement(); }
void CatService::beginRetirement()
{
    m_destroying = true;
    stopAll();
}
bool CatService::validChannel(int channel) const { return channel >= 1 && channel <= 4; }
CatEndpointConfig CatService::channelConfig(int channel) const
{
    return validChannel(channel) ? m_channels[channel - 1].config : CatEndpointConfig{};
}
bool CatService::applyChannelConfig(int channel, const CatEndpointConfig& supplied)
{
    if (m_destroying || !m_model || !m_model->ownsLocalDsp() || !validChannel(channel) || m_started) { return false; }
    CatEndpointConfig config = supplied; config.channel = channel;
    QString reason;
    if (!CatSettings::validate(config, &reason)) { qCWarning(lcCat) << reason; return false; }
    for (int other = 1; other <= 4; ++other) {
        if (other == channel) { continue; }
        const CatEndpointConfig& existing = m_channels[other - 1].config;
        if (config.serialEnabled && existing.serialEnabled && config.serialDevice == existing.serialDevice) { return false; }
    }
    const CatGlobalConfig global = globalConfig();
    if (config.serialEnabled && global.pttEnabled && global.pttDeviceSource == "Physical"
        && config.serialDevice == global.pttSerialDevice) { return false; }
    config.binding = m_adapter.snapshotBinding(config.binding);
    m_channels[channel - 1].config = config; m_channels[channel - 1].configured = true;
    const QPointer<CatService> self(this);
    CatSettings::save(AppSettings::instance(), config);
    if (!self) { return false; }
    emit configurationChanged(channel);
    return true;
}
void CatService::setState(int channel, const QString& state)
{
    Channel& endpoint = m_channels[channel - 1];
    if (endpoint.state == state) { return; }
    endpoint.state = state; emit channelStateChanged(channel, state);
}
void CatService::startConfigured()
{
    if (m_destroying || m_started || !m_model || !m_model->ownsLocalDsp()) { return; }
    if (!m_catalog.isValid()) { qCWarning(lcCat) << "CAT catalogue unavailable:" << m_catalog.errorString(); return; }
    const QList<CatEndpointConfig> restored = CatSettings::load(AppSettings::instance(), *m_model);
    const QPointer<CatService> self(this);
    const quint64 generation = ++m_lifecycleGeneration;
    m_started = true;
    for (int channel = 1; channel <= 4; ++channel) {
        if (!self || generation != m_lifecycleGeneration || !m_started) { return; }
        Channel& endpoint = m_channels[channel - 1];
        if (!endpoint.configured) { endpoint.config = restored[channel - 1]; }
        // Explicit start is a new binding capture; never recapture on disconnect.
        endpoint.config.binding = m_adapter.snapshotBinding(endpoint.config.binding);
        endpoint.configured = true;
        QString reason;
        if (!CatSettings::validate(endpoint.config, &reason)) { setState(channel, reason); continue; }
        startChannel(channel);
    }
}
void CatService::startChannel(int channel)
{
    Channel& endpoint = m_channels[channel - 1];
    if (!m_started || m_destroying) { return; }
    if (endpoint.config.tcpEnabled) {
        const auto tcp = std::make_shared<CatTcpTransport>();
        endpoint.tcp = tcp;
        const std::weak_ptr<CatTcpTransport> weak(tcp);
        const QPointer<CatService> self(this);
        const quint64 generation = m_lifecycleGeneration;
        connect(tcp.get(), &CatTcpTransport::clientAccepted, this, [this, self, weak, channel, generation](QTcpSocket* socket) {
            const auto transport = weak.lock();
            if (!self || !transport || generation != m_lifecycleGeneration || !m_started
                || m_channels[channel - 1].tcp != transport) { return; }
            const quint64 opened = createTransportSession(channel, CatTransportKind::Tcp,
                [weak](quint64 id, const QByteArray& bytes) { const auto owner = weak.lock(); return owner && owner->writeBytes(id, bytes); },
                [weak](quint64 id) { const auto owner = weak.lock(); if (owner) { owner->closeSession(id); } });
            if (!opened || !transport->attachSession(opened, socket)) {
                if (self) { closeSession(opened); }
                return;
            }
            if (!self || generation != m_lifecycleGeneration || !session(opened)) { return; }
            if (globalConfig().sendWelcome) { sendToSession(opened, "#NereusSDR TCP/IP Cat#;"); }
        });
        connect(tcp.get(), &CatTcpTransport::bytesReceived, this, [this, weak](quint64 id, const QByteArray& bytes) {
            const auto transport = weak.lock();
            if (transport) { processBytes(id, bytes); }
        });
        connect(tcp.get(), &CatTcpTransport::closeRequested, this, [this, weak](quint64 id) {
            const auto transport = weak.lock();
            if (transport) { closeSession(id); }
        });
        connect(tcp.get(), &CatTcpTransport::clientCountChanged, this, [this, weak, channel, generation](int count) {
            const auto transport = weak.lock();
            if (transport && generation == m_lifecycleGeneration && m_channels[channel - 1].tcp == transport) {
                emit clientCountChanged(channel, count);
            }
        });
        if (!tcp->start(QHostAddress(endpoint.config.tcpBindAddress), static_cast<quint16>(endpoint.config.tcpPort))) {
            setState(channel, QStringLiteral("TCP error: ") + tcp->errorString()); return;
        }
        setState(channel, QStringLiteral("Listening")); return;
    }
    const bool otherEnabled = endpoint.config.serialEnabled || endpoint.config.ptyEnabled || endpoint.config.rigctldEnabled;
    setState(channel, otherEnabled ? QStringLiteral("Transport backend unavailable") : QStringLiteral("Disabled"));
}
void CatService::stopChannel(int channel)
{
    // Private transport lifecycle for later serial/PTY and live channel configuration.
    const auto tcp = std::exchange(m_channels[channel - 1].tcp, {});
    const QPointer<CatService> self(this);
    const quint64 generation = m_lifecycleGeneration;
    const QList<quint64> ids = sessionIds(channel);
    for (quint64 id : ids) {
        closeSession(id);
        if (!self || generation != m_lifecycleGeneration) { return; }
    }
    if (tcp) { tcp->stop(); }
    if (!self || generation != m_lifecycleGeneration) { return; }
    setState(channel, QStringLiteral("Stopped"));
}
void CatService::stopAll()
{
    const QPointer<CatService> self(this);
    const quint64 generation = ++m_lifecycleGeneration;
    m_started = false;
    const auto sessions = std::exchange(m_sessions, {});
    std::array<std::shared_ptr<CatTcpTransport>, 4> transports;
    for (int index = 0; index < 4; ++index) { transports[index] = std::exchange(m_channels[index].tcp, {}); }
    // Detach old sessions, registrations, buffers and pending reports before cancellation callbacks.
    m_reporter->reset();
    for (const auto& current : sessions) { current->clearRuntime(); }
    m_txCoordinator.cancelAll();
    // Old hooks own only detached transports. Even callback deletion/restart cannot close a replacement run.
    for (const auto& current : sessions) { current->closeTransport(); }
    for (const auto& transport : transports) { if (transport) { transport->stop(); } }
    if (!self) { return; }
    for (quint64 id : sessions.keys()) {
        emit sessionClosed(id);
        if (!self) { return; }
    }
    if (generation != m_lifecycleGeneration) { return; }
    for (int channel = 1; channel <= 4; ++channel) {
        emit clientCountChanged(channel, 0);
        if (!self || generation != m_lifecycleGeneration) { return; }
        setState(channel, QStringLiteral("Stopped"));
        if (!self || generation != m_lifecycleGeneration) { return; }
    }
}
bool CatService::isListening(int channel) const
{
    return validChannel(channel) && m_channels[channel - 1].tcp && m_channels[channel - 1].tcp->isListening();
}
int CatService::clientCount(int channel) const
{
    return validChannel(channel) && m_channels[channel - 1].tcp ? m_channels[channel - 1].tcp->clientCount() : 0;
}
QHostAddress CatService::boundAddress(int channel) const
{
    return isListening(channel) ? m_channels[channel - 1].tcp->boundAddress() : QHostAddress();
}
quint16 CatService::boundPort(int channel) const
{
    return isListening(channel) ? m_channels[channel - 1].tcp->boundPort() : 0;
}
QList<quint64> CatService::sessionIds(int channel) const
{
    QList<quint64> ids;
    for (const auto& current : m_sessions) {
        if (current->context().channel == channel) { ids.append(current->context().sessionId); }
    }
    return ids;
}
QString CatService::channelState(int channel) const { return validChannel(channel) ? m_channels[channel - 1].state : QStringLiteral("Invalid channel"); }
CatGlobalConfig CatService::globalConfig() const { return m_settings.global(); }
bool CatService::applyGlobalConfig(const CatGlobalConfig& config)
{
    if (m_destroying || !m_model || !m_model->ownsLocalDsp()) { return false; }
    if (m_started) {
        const CatGlobalConfig current = globalConfig();
        if (config.pttEnabled != current.pttEnabled || config.pttDeviceSource != current.pttDeviceSource
            || config.pttSerialDevice != current.pttSerialDevice || config.pttUseCts != current.pttUseCts
            || config.pttUseDsr != current.pttUseDsr || config.pttChannel != current.pttChannel
            || config.pttSerialBaud != current.pttSerialBaud || config.pttSerialParity != current.pttSerialParity
            || config.pttSerialDataBits != current.pttSerialDataBits || config.pttSerialStopBits != current.pttSerialStopBits) { return false; }
    }
    if (config.pttEnabled && config.pttDeviceSource == "Physical") {
        for (const Channel& endpoint : m_channels) {
            if (endpoint.config.serialEnabled && endpoint.config.serialDevice == config.pttSerialDevice) { return false; }
        }
    }
    const QPointer<CatService> self(this);
    if (!m_settings.setGlobal(config) || !self) { return false; }
    emit globalConfigurationChanged(); return true;
}
quint64 CatService::createTransportSession(int channel, CatTransportKind kind,
    std::function<bool(quint64, const QByteArray&)> write, std::function<void(quint64)> close)
{
    if (m_destroying || !m_started || !validChannel(channel)) { return 0; }
    const quint64 id = ++m_nextSessionId;
    const auto current = std::make_shared<CatSession>(id, channel, kind, channelConfig(channel).binding);
    current->setOutputHooks([write = std::move(write), id](const QByteArray& bytes) { return write && write(id, bytes); },
        [close = std::move(close), id] { if (close) { close(id); } });
    m_sessions.insert(id, current);
    return id;
}
quint64 CatService::openSession(int channel, CatTransportKind transport)
{
    if (m_destroying || !m_started || !validChannel(channel) || transport != CatTransportKind::Tester) { return 0; }
    const quint64 id = ++m_nextSessionId;
    m_sessions.insert(id, std::make_shared<CatSession>(id, channel, transport, channelConfig(channel).binding));
    return id;
}
void CatService::closeSession(quint64 id)
{
    const auto current = m_sessions.take(id);
    if (!current) { return; }
    const QPointer<CatService> self(this);
    current->clearRuntime(); m_reporter->sessionsChanged(current->context().channel);
    m_txCoordinator.cancelSession(id);
    current->closeTransport();
    if (!self) { return; }
    emit sessionClosed(id);
}
CatSession* CatService::session(quint64 id) { return m_sessions.contains(id) ? m_sessions.value(id).get() : nullptr; }
QByteArray CatService::processFrame(quint64 id, const QByteArray& frame)
{
    const std::shared_ptr<CatSession> current = m_sessions.value(id);
    if (m_destroying || !m_started || !current) { return "?;"; }
    const CatValidation validation = m_parser.validate(frame);
    if (!validation.request) { return m_parser.formatValidationError(validation, current->context()); }
    const CatRequest& request = *validation.request;
    // Family handlers consume context.transmitAllowed before all key/tune/test actions.
    const QPointer<CatService> self(this);
    const quint64 generation = m_lifecycleGeneration;
    const CatCommandResult result = m_router.execute(request, current->context());
    if (!self || generation != m_lifecycleGeneration || !m_started || m_sessions.value(id) != current) { return "?;"; }
    current->applyGuidResult(request, result);
    return m_parser.format(*m_catalog.find(request.code), request, result, current->context());
}
void CatService::processBytes(quint64 id, const QByteArray& bytes)
{
    const auto current = m_sessions.value(id);
    if (m_destroying || !m_started || !current) { return; }
    const QPointer<CatService> self(this);
    const quint64 generation = m_lifecycleGeneration;
    const QList<QByteArray> frames = current->framer().feed(bytes);
    for (const QByteArray& frame : frames) {
        if (!self || generation != m_lifecycleGeneration || !m_started || m_sessions.value(id) != current) { return; }
        // Empty framer entry is one oversize event; no unbounded noise is logged.
        if (!frame.isEmpty()) {
            emit messageLogged(current->context().channel, true, frame);
            if (!self || generation != m_lifecycleGeneration || m_sessions.value(id) != current) { return; }
        }
        const QByteArray result = frame.isEmpty() ? QByteArray("?;") : processFrame(id, frame);
        if (!self || generation != m_lifecycleGeneration || !m_started || m_sessions.value(id) != current) { return; }
        if (!result.isEmpty()) { sendToSession(id, result); }
        if (!self) { return; }
    }
}
void CatService::sendToSession(quint64 id, const QByteArray& bytes)
{
    const auto current = m_sessions.value(id);
    if (m_destroying || !m_started || !current || bytes.isEmpty()) { return; }
    const QPointer<CatService> self(this);
    const quint64 generation = m_lifecycleGeneration;
    const bool accepted = current->writeBytes(bytes);
    if (!self || generation != m_lifecycleGeneration || m_sessions.value(id) != current) { return; }
    if (!accepted) { closeSession(id); return; }
    emit messageLogged(current->context().channel, false, bytes);
}
void CatService::sendToGuid(const QUuid& guid, const QByteArray& bytes)
{
    // Current session snapshots, not a GUID-to-single-owner map. GUIDs are routing labels.
    const QList<quint64> ids = m_sessions.keys();
    const QPointer<CatService> self(this);
    const quint64 generation = m_lifecycleGeneration;
    for (quint64 id : ids) {
        const auto current = m_sessions.value(id);
        if (current && current->transport() == CatTransportKind::Tcp && current->hasGuid(guid)) { sendToSession(id, bytes); }
        if (!self || generation != m_lifecycleGeneration) { return; }
    }
}
} // namespace NereusSDR
