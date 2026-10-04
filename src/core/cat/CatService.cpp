// no-port-check: NereusSDR-original CAT station admission/lifecycle.
// 2026-10-04 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
#include "CatService.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "models/RadioModel.h"
namespace NereusSDR {
CatService::CatService(RadioModel& model, QObject* parent)
    : QObject(parent), m_model(&model), m_adapter(model), m_txCoordinator(model), m_settings(AppSettings::instance()), m_rxCommands(m_adapter, m_txCoordinator, m_settings), m_dspCommands(m_adapter), m_parser(m_catalog)
{
    for (const QByteArray& code : CatRxCommands::codes()) {
        m_router.registerHandler(code, [this](const CatRequest& request, CatSessionContext& context) { return m_rxCommands.execute(request, context); });
    }
    for (const QByteArray& code : CatDspCommands::codes()) {
        m_router.registerHandler(code, [this](const CatRequest& request, CatSessionContext& context) { return m_dspCommands.execute(request, context); });
    }
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
        const bool enabled = endpoint.config.tcpEnabled || endpoint.config.serialEnabled || endpoint.config.ptyEnabled || endpoint.config.rigctldEnabled;
        setState(channel, enabled ? QStringLiteral("Transport backend unavailable") : QStringLiteral("Disabled"));
    }
}
void CatService::stopAll()
{
    const QPointer<CatService> self(this);
    const quint64 generation = ++m_lifecycleGeneration;
    m_started = false;
    const QList<quint64> ids = m_sessions.keys();
    // Detach every old session before callbacks can start a replacement run.
    m_sessions.clear();
    m_txCoordinator.cancelAll();
    if (!self) { return; }
    for (quint64 id : ids) {
        emit sessionClosed(id);
        if (!self) { return; }
    }
    if (generation != m_lifecycleGeneration) { return; }
    for (int channel = 1; channel <= 4; ++channel) {
        setState(channel, QStringLiteral("Stopped"));
        if (!self || generation != m_lifecycleGeneration) { return; }
    }
}
bool CatService::isListening(int channel) const
{
    Q_UNUSED(channel);
    // Native transport implementation belongs to Tasks 8–10.
    return false;
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
quint64 CatService::openSession(int channel, CatTransportKind transport)
{
    if (m_destroying || !m_started || !validChannel(channel)) { return 0; }
    // Only the in-process tester exists until actual transports are supplied.
    if (transport != CatTransportKind::Tester) { return 0; }
    const quint64 id = ++m_nextSessionId;
    m_sessions.insert(id, std::make_shared<CatSession>(id, channel, transport, channelConfig(channel).binding));
    return id;
}
void CatService::closeSession(quint64 id)
{
    if (!m_sessions.remove(id)) { return; }
    const QPointer<CatService> self(this);
    m_txCoordinator.cancelSession(id);
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
    return m_parser.format(*m_catalog.find(request.code), request, result, current->context());
}
} // namespace NereusSDR
