// --- From SerialPortPTT.cs ---
//=================================================================
// SerialPortPTT.cs
//=================================================================
// Copyright (C) 2005  Bill Tracey
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//=================================================================
// This class is used to implement a PTT using RTS or DTS 
//=================================================================

// --- From CATCommands.cs ---
//=================================================================
// CATCommands.cs
//=================================================================
// Copyright (C) 2005  Bob Tracy
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact the author via email at: k5kdn@arrl.net
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
/*
Modifications to support the Behringer Midi controllers
by Chris Codella, W2PA, April 2017.  Indicated by //-W2PA comment lines.
Added extended CAT commands for APF funtions - May 2017.
*/
//=================================================================

// Ported from Thetis Project Files/Source/Console/CAT/SerialPortPTT.cs and CATCommands.cs
// Modification history (NereusSDR):
// 2026-10-04 - Composite release-armed input PTT and requesting serial close by
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-04 - Independently implemented native PTY lifecycle and transport diagnostics,
//              same author and AI tooling; no new upstream port.
#include "CatService.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/TxSliceArbiter.h"
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
    // Prepare all restored assignments before any open so duplicate devices are rejected symmetrically.
    for (int channel = 1; channel <= 4; ++channel) {
        Channel& endpoint = m_channels[channel - 1];
        if (!endpoint.configured) { endpoint.config = restored[channel - 1]; }
    }
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
    if (self && generation == m_lifecycleGeneration && m_started) { startPtt(); }
}
void CatService::startChannel(int channel)
{
    Channel& endpoint = m_channels[channel - 1];
    const CatEndpointConfig config = endpoint.config;
    const QPointer<CatService> lifetime(this);
    const quint64 run = m_lifecycleGeneration;
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
            setTransportState(channel, CatTransportKind::Tcp, QStringLiteral("TCP error: ") + tcp->errorString());
        } else { setTransportState(channel, CatTransportKind::Tcp, QStringLiteral("Listening")); }
    }
    if (!lifetime || run != m_lifecycleGeneration || !m_started) { return; }
    if (config.serialEnabled) {
        const auto startSerial = [&] {
            for (int other = 1; other <= 4; ++other) {
                const CatEndpointConfig& assigned = m_channels[other - 1].config;
                if (other != channel && assigned.serialEnabled && assigned.serialDevice == config.serialDevice) {
                    setTransportState(channel, CatTransportKind::Serial, "Serial error: duplicate CAT device assignment"); return;
                }
            }
            const CatGlobalConfig global = globalConfig();
            if (global.pttEnabled && global.pttDeviceSource == "Physical" && global.pttSerialDevice == config.serialDevice) {
                setTransportState(channel, CatTransportKind::Serial, "Serial error: device assigned to exclusive physical PTT"); return;
            }
            const auto serial = m_serialFactory ? m_serialFactory() : std::make_shared<CatSerialTransport>();
            if (!lifetime || run != m_lifecycleGeneration || !m_started || !serial) { return; }
            endpoint.serial = serial;
            const std::weak_ptr<CatSerialTransport> weak(serial);
            const quint64 opened = createTransportSession(channel, CatTransportKind::Serial,
                [weak](quint64, const QByteArray& bytes) {
                    const auto owner = weak.lock(); if (!owner || !owner->isOpen()) { return false; }
                    owner->writeBytes(bytes); return owner->isOpen();
                }, [weak](quint64) { const auto owner = weak.lock(); if (owner) { owner->stop(); } });
            connect(serial.get(), &CatSerialTransport::bytesReceived, this, [this, weak, opened, channel, run](const QByteArray& bytes) {
                const auto owner = weak.lock();
                if (owner && run == m_lifecycleGeneration && m_channels[channel - 1].serial == owner) { processBytes(opened, bytes); }
            });
            connect(serial.get(), &CatSerialTransport::failed, this, [this, weak, channel, run](const QString& error) {
                const auto owner = weak.lock();
                if (!owner || run != m_lifecycleGeneration || m_channels[channel - 1].serial != owner) { return; }
                const QPointer<CatService> self(this);
                const bool pinSource = m_ptt && m_ptt->transport == owner;
                closeSerialChannel(channel, owner);
                if (self && run == m_lifecycleGeneration && !m_channels[channel - 1].serial) {
                    setTransportState(channel, CatTransportKind::Serial, "Serial error: " + error);
                    if (self && run == m_lifecycleGeneration && pinSource && !m_ptt) { setPttState("PTT error: " + error); }
                }
            });
            const bool started = serial->start(config);
            if (!lifetime || run != m_lifecycleGeneration || m_channels[channel - 1].serial != serial) { return; }
            if (started) { setTransportState(channel, CatTransportKind::Serial, "Listening"); }
            else { closeSerialChannel(channel, serial); }
        };
        startSerial();
    }
    if (!lifetime || run != m_lifecycleGeneration || !m_started) { return; }
    if (config.ptyEnabled) { startPty(channel); }
    if (!lifetime || run != m_lifecycleGeneration || !m_started) { return; }
    if (config.rigctldEnabled) { setTransportState(channel, CatTransportKind::Rigctld, "Rigctld backend unavailable"); }
    if (!lifetime || run != m_lifecycleGeneration || !m_started) { return; }
    updateChannelState(channel);
}
QString CatService::ptySlavePath(int channel) const {
    return validChannel(channel) && m_channels[channel - 1].pty ? m_channels[channel - 1].pty->slavePath() : QString();
}
CatPtyTransport* CatService::ptyTransport(int channel) const {
    return validChannel(channel) ? m_channels[channel - 1].pty.get() : nullptr;
}
QString CatService::transportState(int channel, CatTransportKind kind) const {
    return validChannel(channel) ? m_channels[channel - 1].transportStates.value(kind, "Stopped") : "Invalid channel";
}
void CatService::setTransportState(int channel, CatTransportKind kind, const QString& state) {
    m_channels[channel - 1].transportStates.insert(kind, state);
    const QPointer<CatService> self(this);
    const quint64 generation = m_lifecycleGeneration;
    updateChannelState(channel);
    if (self && generation == m_lifecycleGeneration) { emit transportStateChanged(channel, kind, state); }
}
void CatService::updateChannelState(int channel) {
    const Channel& endpoint = m_channels[channel - 1];
    QStringList errors;
    bool listening = false;
    for (CatTransportKind kind : {CatTransportKind::Tcp, CatTransportKind::Serial, CatTransportKind::Pty, CatTransportKind::Rigctld}) {
        const QString state = endpoint.transportStates.value(kind);
        if (state.contains("error", Qt::CaseInsensitive) || state.contains("unavailable", Qt::CaseInsensitive)) { errors.append(state); }
        if (state == "Listening") { listening = true; }
    }
    setState(channel, !errors.isEmpty() ? errors.join("; ") : listening ? "Listening"
        : (endpoint.config.tcpEnabled || endpoint.config.serialEnabled || endpoint.config.ptyEnabled || endpoint.config.rigctldEnabled) ? "Stopped" : "Disabled");
}
// One logical PTY stream owns one session while a kernel peer is observed. Multiple
// slave handles are indistinguishable; a close/reopen gap the event loop never sees
// cannot establish a new identity. Only observed HUP retires claims/framer for reopen.
void CatService::startPty(int channel) {
    Channel& endpoint = m_channels[channel - 1];
    const auto pty = std::make_shared<CatPtyTransport>();
    endpoint.pty = pty;
    const std::weak_ptr<CatPtyTransport> weak(pty);
    const QPointer<CatService> self(this);
    const quint64 generation = m_lifecycleGeneration;
    connect(pty.get(), &CatPtyTransport::peerOpened, this, [this, self, weak, channel, generation] {
        const auto owner = weak.lock();
        if (!self || !owner || generation != m_lifecycleGeneration || !m_started || m_channels[channel - 1].pty != owner) { return; }
        const quint64 id = createTransportSession(channel, CatTransportKind::Pty,
            [weak](quint64 sessionId, const QByteArray& bytes) { const auto transport = weak.lock(); return transport && transport->writeBytes(sessionId, bytes); },
            [weak](quint64 sessionId) { const auto transport = weak.lock(); if (transport) { transport->closeSession(sessionId); } });
        if (!id || !owner->attachSession(id)) { closeSession(id); return; }
        m_channels[channel - 1].ptySession = id;
        m_reporter->sessionsChanged(channel);
    });
    connect(pty.get(), &CatPtyTransport::peerClosed, this, [this, self, weak, channel, generation](quint64 id) {
        const auto owner = weak.lock();
        if (!self || !owner || generation != m_lifecycleGeneration || m_channels[channel - 1].pty != owner) { return; }
        // The transport already dropped peer identity and both queues, so the close hook preserves the endpoint.
        closeSession(id);
    });
    connect(pty.get(), &CatPtyTransport::bytesReceived, this, [this, weak, channel, generation](const QByteArray& bytes) {
        const auto owner = weak.lock();
        if (owner && generation == m_lifecycleGeneration && m_channels[channel - 1].pty == owner) { processBytes(m_channels[channel - 1].ptySession, bytes); }
    });
    connect(pty.get(), &CatPtyTransport::failed, this, [this, self, weak, channel, generation](const QString& error) {
        const auto owner = weak.lock();
        if (!self || !owner || generation != m_lifecycleGeneration || m_channels[channel - 1].pty != owner) { return; }
        const quint64 id = std::exchange(m_channels[channel - 1].ptySession, 0);
        closeSession(id);
        if (!self || generation != m_lifecycleGeneration || m_channels[channel - 1].pty != owner) { return; }
        emit ptyPathChanged(channel, {});
        if (self && generation == m_lifecycleGeneration && m_channels[channel - 1].pty == owner) {
            setTransportState(channel, CatTransportKind::Pty, "PTY error: " + error);
        }
    });
    const bool opened = pty->start(channel, endpoint.config);
    if (!self || generation != m_lifecycleGeneration || m_channels[channel - 1].pty != pty) { return; }
    if (opened) {
        setTransportState(channel, CatTransportKind::Pty, "Listening");
        if (self && generation == m_lifecycleGeneration && m_channels[channel - 1].pty == pty) { emit ptyPathChanged(channel, pty->slavePath()); }
    }
}
void CatService::stopChannel(int channel)
{
    const auto tcp = std::exchange(m_channels[channel - 1].tcp, {});
    const auto serial = std::exchange(m_channels[channel - 1].serial, {});
    const auto pty = std::exchange(m_channels[channel - 1].pty, {});
    m_channels[channel - 1].ptySession = 0;
    m_channels[channel - 1].transportStates.clear();
    const auto input = m_ptt && m_ptt->channel == channel ? std::exchange(m_ptt, {}) : nullptr;
    QHash<quint64, std::shared_ptr<CatSession>> sessions;
    for (quint64 id : sessionIds(channel)) { sessions.insert(id, m_sessions.take(id)); }
    for (const auto& current : sessions) { current->clearRuntime(); }
    const QPointer<CatService> self(this);
    const QPointer<CatTxCoordinator> coordinator(&m_txCoordinator);
    const quint64 generation = m_lifecycleGeneration;
    if (input) { input->armed = false; input->asserted = false; coordinator->cancelSession(input->sessionId); }
    for (quint64 id : sessions.keys()) { if (coordinator) { coordinator->cancelSession(id); } }
    if (input) { input->transport->setPinSampling(false); }
    for (const auto& current : sessions) { current->closeTransport(); }
    if (serial) { serial->stop(); }
    if (pty) { pty->stop(); }
    if (tcp) { tcp->stop(); }
    if (input && input->separate) { input->transport->stop(); }
    if (!self) { return; }
    for (quint64 id : sessions.keys()) { emit sessionClosed(id); if (!self) { return; } }
    if (generation != m_lifecycleGeneration) { return; }
    m_reporter->sessionsChanged(channel);
    emit ptyPathChanged(channel, {});
    if (!self || generation != m_lifecycleGeneration) { return; }
    if (input && !m_ptt) { setPttState("Stopped"); }
    if (!self || generation != m_lifecycleGeneration) { return; }
    emit clientCountChanged(channel, 0);
    if (!self || generation != m_lifecycleGeneration) { return; }
    setState(channel, QStringLiteral("Stopped"));
}
void CatService::stopAll()
{
    const QPointer<CatService> self(this);
    const quint64 generation = ++m_lifecycleGeneration;
    m_started = false;
    const auto sessions = std::exchange(m_sessions, {});
    const auto ptt = std::exchange(m_ptt, {});
    std::array<std::shared_ptr<CatPtyTransport>, 4> ptys;
    for (int index = 0; index < 4; ++index) {
        ptys[index] = std::exchange(m_channels[index].pty, {});
        m_channels[index].ptySession = 0;
        m_channels[index].transportStates.clear();
    }
    std::array<std::shared_ptr<CatSerialTransport>, 4> serials;
    for (int index = 0; index < 4; ++index) { serials[index] = std::exchange(m_channels[index].serial, {}); }
    std::array<std::shared_ptr<CatTcpTransport>, 4> transports;
    for (int index = 0; index < 4; ++index) { transports[index] = std::exchange(m_channels[index].tcp, {}); }
    // Detach old sessions, registrations, buffers and pending reports before cancellation callbacks.
    m_reporter->reset();
    for (const auto& current : sessions) { current->clearRuntime(); }
    m_txCoordinator.cancelAll();
    // Old hooks own only detached transports. Even callback deletion/restart cannot close a replacement run.
    for (const auto& current : sessions) { current->closeTransport(); }
    for (const auto& serial : serials) { if (serial) { serial->stop(); } }
    for (const auto& pty : ptys) { if (pty) { pty->stop(); } }
    if (ptt && ptt->separate) { ptt->transport->stop(); }
    for (const auto& transport : transports) { if (transport) { transport->stop(); } }
    if (!self) { return; }
    for (quint64 id : sessions.keys()) {
        emit sessionClosed(id);
        if (!self) { return; }
    }
    if (generation != m_lifecycleGeneration) { return; }
    setPttState("Stopped");
    if (!self || generation != m_lifecycleGeneration) { return; }
    for (int channel = 1; channel <= 4; ++channel) {
        emit ptyPathChanged(channel, {});
        if (!self || generation != m_lifecycleGeneration) { return; }
        emit clientCountChanged(channel, 0);
        if (!self || generation != m_lifecycleGeneration) { return; }
        setState(channel, QStringLiteral("Stopped"));
        if (!self || generation != m_lifecycleGeneration) { return; }
    }
}
bool CatService::isListening(int channel) const
{
    if (!validChannel(channel)) { return false; }
    const Channel& endpoint = m_channels[channel - 1];
    return (endpoint.tcp && endpoint.tcp->isListening()) || (endpoint.serial && endpoint.serial->isOpen())
        || (endpoint.pty && endpoint.pty->isOpen());
}
int CatService::clientCount(int channel) const
{
    return validChannel(channel) && m_channels[channel - 1].tcp ? m_channels[channel - 1].tcp->clientCount() : 0;
}
QHostAddress CatService::boundAddress(int channel) const
{
    return validChannel(channel) && m_channels[channel - 1].tcp ? m_channels[channel - 1].tcp->boundAddress() : QHostAddress();
}
quint16 CatService::boundPort(int channel) const
{
    return validChannel(channel) && m_channels[channel - 1].tcp ? m_channels[channel - 1].tcp->boundPort() : 0;
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
    const auto existing = m_sessions.value(id);
    if (existing && existing->transport() == CatTransportKind::Serial) {
        const int channel = existing->context().channel;
        const auto serial = m_channels[channel - 1].serial;
        if (serial) { closeSerialChannel(channel, serial); return; }
    }
    const auto current = m_sessions.take(id);
    if (!current) { return; }
    const QPointer<CatService> self(this);
    const quint64 generation = m_lifecycleGeneration;
    const int channel = current->context().channel;
    const auto pty = current->transport() == CatTransportKind::Pty ? m_channels[channel - 1].pty : nullptr;
    if (pty && m_channels[channel - 1].ptySession == id) { m_channels[channel - 1].ptySession = 0; }
    current->clearRuntime(); m_reporter->sessionsChanged(channel);
    m_txCoordinator.cancelSession(id);
    current->closeTransport();
    if (!self) { return; }
    if (pty && generation == m_lifecycleGeneration && m_channels[channel - 1].pty == pty && !pty->isOpen()) {
        setTransportState(channel, CatTransportKind::Pty, "Stopped");
        if (!self) { return; }
        if (generation == m_lifecycleGeneration && m_channels[channel - 1].pty == pty) { emit ptyPathChanged(channel, {}); }
        if (!self) { return; }
    }
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
    // From Thetis CAT/CATCommands.cs:8552-8556 [v2.10.3.15].
    // Adapt source first-port close to this current requesting serial endpoint only.
    if (request.code == "ZZZZ" && current->transport() == CatTransportKind::Serial) {
        closeSerialChannel(current->context().channel, m_channels[current->context().channel - 1].serial);
        return {};
    }
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
namespace NereusSDR {
CatSerialTransport* CatService::serialTransport(int channel) const {
    return validChannel(channel) ? m_channels[channel - 1].serial.get() : nullptr;
}
bool CatService::setSerialTransportFactoryForTest(std::function<std::shared_ptr<CatSerialTransport>()> factory) {
    if (m_started || m_destroying) { return false; }
    m_serialFactory = std::move(factory); return true;
}
void CatService::setPttState(const QString& state) {
    if (m_pttState == state) { return; }
    m_pttState = state; emit pttStateChanged(state);
}
void CatService::startPtt() {
    const CatGlobalConfig config = globalConfig();
    if (!config.pttEnabled) { setPttState("Disabled"); return; }
    const QPointer<CatService> self(this);
    const quint64 generation = m_lifecycleGeneration;
    const auto input = std::make_shared<PttInput>();
    input->channel = config.pttDeviceSource == "Physical" ? config.pttChannel : config.pttDeviceSource.right(1).toInt();
    input->useCts = config.pttUseCts; input->useDsr = config.pttUseDsr;
    input->separate = config.pttDeviceSource == "Physical";
    if (!validChannel(input->channel)) { setPttState("Invalid PTT channel"); return; }
    input->binding = channelConfig(input->channel).binding;
    input->sessionId = ++m_nextSessionId;
    if (input->separate) {
        for (const Channel& endpoint : m_channels) {
            if (endpoint.config.serialEnabled && endpoint.config.serialDevice == config.pttSerialDevice) {
                setPttState("PTT serial device is already assigned to CAT"); return;
            }
        }
        input->transport = m_serialFactory ? m_serialFactory() : std::make_shared<CatSerialTransport>();
    } else { input->transport = m_channels[input->channel - 1].serial; }
    if (!self || generation != m_lifecycleGeneration || !m_started) { return; }
    if (!input->transport || (!input->separate && !input->transport->isOpen())) {
        setPttState("PTT input pins require an open serial CAT endpoint"); return;
    }
    m_ptt = input;
    const std::weak_ptr<PttInput> weak(input);
    connect(input->transport.get(), &CatSerialTransport::pttSampled, this, [this, weak, generation](bool cts, bool dsr) {
        const auto current = weak.lock();
        if (current && current == m_ptt && generation == m_lifecycleGeneration) { applyPttSample(current->channel, cts, dsr); }
    });
    connect(input->transport.get(), &CatSerialTransport::failed, this, [this, weak, generation](const QString& error) {
        const auto current = weak.lock();
        if (!current || current != m_ptt || generation != m_lifecycleGeneration) { return; }
        const QPointer<CatService> lifetime(this);
        stopPtt();
        if (lifetime && generation == m_lifecycleGeneration && !m_ptt) { setPttState("PTT error: " + error); }
    });
    if (input->separate) {
        CatEndpointConfig serial = channelConfig(input->channel);
        serial.tcpEnabled = false; serial.ptyEnabled = false; serial.rigctldEnabled = false;
        serial.serialEnabled = true; serial.serialDevice = config.pttSerialDevice;
        serial.serialBaud = config.pttSerialBaud; serial.serialParity = config.pttSerialParity;
        serial.serialDataBits = config.pttSerialDataBits; serial.serialStopBits = config.pttSerialStopBits;
        const bool opened = input->transport->start(serial);
        if (!self || generation != m_lifecycleGeneration || m_ptt != input || !opened) { return; }
    }
    const bool sampled = input->transport->setPinSampling(true);
    if (!self || generation != m_lifecycleGeneration || m_ptt != input || !sampled) { return; }
    setPttState(input->armed ? "Armed" : "Waiting for release");
}
void CatService::stopPtt() {
    const auto input = std::exchange(m_ptt, {});
    if (!input) { return; }
    input->armed = false; input->asserted = false;
    const QPointer<CatService> self(this);
    const quint64 generation = m_lifecycleGeneration;
    m_txCoordinator.cancelSession(input->sessionId);
    // Detached old input cannot stop a callback-started replacement lifecycle.
    input->transport->setPinSampling(false);
    if (input->separate) { input->transport->stop(); }
    if (self && generation == m_lifecycleGeneration && !m_ptt) { setPttState("Stopped"); }
}
void CatService::applyPttSample(int channel, bool cts, bool dsr) {
    const auto input = m_ptt;
    if (m_destroying || !m_started || !m_model || !input || input->channel != channel
        || !input->transport->isOpen()) { return; }
    // From Thetis CAT/SerialPortPTT.cs:84-93 [v2.10.3.15].
    // Composite source levels; Nereus requires all selected inputs to release before arming.
    const bool asserted = (input->useCts && cts) || (input->useDsr && dsr);
    const QPointer<CatService> self(this);
    const quint64 generation = m_lifecycleGeneration;
    if (!asserted) {
        input->asserted = false; input->armed = true;
        m_txCoordinator.releasePtt(input->sessionId);
        if (self && generation == m_lifecycleGeneration && m_ptt == input) { setPttState("Armed"); }
        return;
    }
    if (!input->armed || input->asserted) { return; }
    input->armed = false; input->asserted = true;
    const int selected = m_model->txSliceArbiter()->txBoundSliceId();
    SliceModel* target = nullptr;
    if (selected == input->binding.primarySliceId) { target = m_adapter.resolveSlice(input->binding, CatVfo::Primary); }
    else if (input->binding.secondarySliceId && selected == *input->binding.secondarySliceId) {
        target = m_adapter.resolveSlice(input->binding, CatVfo::Secondary);
    }
    // No implicit slice selection or handoff: the coordinator receives the actual live selected target only.
    const bool accepted = target && m_txCoordinator.requestPtt(input->sessionId, selected);
    if (self && generation == m_lifecycleGeneration && m_ptt == input) { setPttState(accepted ? "Asserted" : "PTT request refused"); }
}
void CatService::closeSerialChannel(int channel, const std::shared_ptr<CatSerialTransport>& supplied) {
    const auto transport = supplied;
    if (!validChannel(channel) || !transport || m_channels[channel - 1].serial != transport) { return; }
    m_channels[channel - 1].serial.reset();
    const auto input = m_ptt && m_ptt->transport == transport ? std::exchange(m_ptt, {}) : nullptr;
    QHash<quint64, std::shared_ptr<CatSession>> sessions;
    for (quint64 id : m_sessions.keys()) {
        const auto current = m_sessions.value(id);
        if (current->context().channel == channel && current->transport() == CatTransportKind::Serial) {
            sessions.insert(id, m_sessions.take(id)); current->clearRuntime();
        }
    }
    const QPointer<CatService> self(this);
    const QPointer<CatTxCoordinator> coordinator(&m_txCoordinator);
    const quint64 generation = m_lifecycleGeneration;
    // Detach every affected claim before cancellation can invoke model or lifecycle callbacks.
    if (input) { input->armed = false; input->asserted = false; coordinator->cancelSession(input->sessionId); }
    for (quint64 id : sessions.keys()) { if (coordinator) { coordinator->cancelSession(id); } }
    // Only retained old sessions/handles are closed, even after callback deletion/restart.
    if (input) { input->transport->setPinSampling(false); }
    for (const auto& current : sessions) { current->closeTransport(); }
    transport->stop();
    if (!self) { return; }
    for (quint64 id : sessions.keys()) { emit sessionClosed(id); if (!self) { return; } }
    if (generation != m_lifecycleGeneration) { return; }
    m_reporter->sessionsChanged(channel);
    if (input && !m_ptt) { setPttState("Stopped"); }
    if (!self || generation != m_lifecycleGeneration) { return; }
    if (!m_channels[channel - 1].serial) {
        setTransportState(channel, CatTransportKind::Serial, "Stopped");
    }
}
} // namespace NereusSDR
