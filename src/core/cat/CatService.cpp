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
// 2026-10-04 - Complete accepted nested PTT ingress retirement and preserve
//              replacement sampling on a retained shared serial transport.
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-04 - Composite release-armed input PTT and requesting serial close by
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-10-04 - Independently implemented native PTY lifecycle and transport diagnostics,
//              same author and AI tooling; no new upstream port.
// 2026-10-04 - Native live configuration, scoped write/lifecycle supersession and isolated Tester,
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex; no new upstream port.
// 2026-10-04 - Native separate Hamlib dialect and guarded lifecycle integration,
//              J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex; no new Thetis port.
#include "CatService.h"
#include "RigctlProtocol.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "core/TxSliceArbiter.h"
#include <utility>
#include <QScopeGuard>
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
bool CatService::applyChannelConfig(int channel, const CatEndpointConfig& config)
{
    if (m_started) { return false; }
    // The low-level stopped operation remains an explicit binding capture.
    CatEndpointConfig captured=config; captured.binding=m_adapter.snapshotBinding(config.binding);
    return reconfigureChannel(channel, captured);
}
bool CatService::reconfigureChannel(int channel, const CatEndpointConfig& supplied)
{
    if (m_destroying || !m_model || !m_model->ownsLocalDsp() || !validChannel(channel)) { return false; }
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
    const CatEndpointConfig previous = channelConfig(channel);
    if (!m_channels[channel - 1].configured) { config.binding = m_adapter.snapshotBinding(config.binding); }
    else {
        // Transport-only edits keep the old incarnation even if the reusable ID now names a new slice.
        // A caller explicitly rebinds by supplying the current live incarnation (the selectors do this).
        const bool primaryChanged = config.binding.primarySliceId != previous.binding.primarySliceId;
        const bool secondaryChanged = config.binding.secondarySliceId != previous.binding.secondarySliceId;
        const bool primaryRebound = !primaryChanged && config.binding.primaryIncarnation != 0
            && config.binding.primaryIncarnation != previous.binding.primaryIncarnation;
        const bool secondaryRebound = !secondaryChanged && config.binding.secondaryIncarnation.value_or(0) != 0
            && config.binding.secondaryIncarnation != previous.binding.secondaryIncarnation;
        if (primaryChanged || secondaryChanged || primaryRebound || secondaryRebound) {
            const CatBinding live = m_adapter.snapshotBinding(config.binding);
            if ((primaryRebound && config.binding.primaryIncarnation != live.primaryIncarnation)
                || (secondaryRebound && config.binding.secondaryIncarnation != live.secondaryIncarnation)) { return false; }
            config.binding.primaryIncarnation = primaryChanged || primaryRebound ? live.primaryIncarnation : previous.binding.primaryIncarnation;
            config.binding.secondaryIncarnation = secondaryChanged || secondaryRebound ? live.secondaryIncarnation : previous.binding.secondaryIncarnation;
        } else {
            config.binding = previous.binding;
        }
    }
    if (m_channels[channel - 1].configured && config == previous) { return true; }
    Channel& endpoint = m_channels[channel - 1];
    const quint64 revision = ++endpoint.revision;
    const quint64 run = m_lifecycleGeneration;
    const bool started = m_started;
    const int pttChannel = global.pttDeviceSource == "Physical" ? global.pttChannel : global.pttDeviceSource.right(1).toInt();
    const bool rearmPtt = global.pttEnabled && pttChannel == channel;
    endpoint.config = config; endpoint.configured = true;
    const QPointer<CatService> self(this);
    const auto current = [self, channel, revision] { return self && !self->m_destroying && self->m_channels[channel - 1].revision == revision; };
    stopChannel(channel);
    if (!current()) { return false; }
    if (!CatSettings::save(AppSettings::instance(), config, current) || !current()) { return false; }
    emit configurationChanged(channel);
    if (!current()) { return false; }
    if (started && m_started && run == m_lifecycleGeneration) {
        startChannel(channel);
        if (current() && m_started && run == m_lifecycleGeneration && rearmPtt && !m_ptt) { startPtt(); }
    }
    return current();
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
    const quint64 revision = m_channels[channel - 1].revision;
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
        connect(tcp.get(), &CatTcpTransport::clientAccepted, this, [this, self, weak, channel, generation, revision](QTcpSocket* socket) {
            const auto transport = weak.lock();
            if (!self || !transport || (generation != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) || !m_started
                || m_channels[channel - 1].tcp != transport) { return; }
            const quint64 opened = createTransportSession(channel, CatTransportKind::Tcp,
                [weak](quint64 id, const QByteArray& bytes) { const auto owner = weak.lock(); return owner && owner->writeBytes(id, bytes); },
                [weak](quint64 id) { const auto owner = weak.lock(); if (owner) { owner->closeSession(id); } });
            if (!opened || !transport->attachSession(opened, socket)) {
                if (self) { closeSession(opened); }
                return;
            }
            if (!self || (generation != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) || !session(opened)) { return; }
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
        connect(tcp.get(), &CatTcpTransport::clientCountChanged, this, [this, weak, channel, generation, revision](int count) {
            const auto transport = weak.lock();
            if (transport && generation == m_lifecycleGeneration && revision == m_channels[channel - 1].revision && m_channels[channel - 1].tcp == transport) {
                emit clientCountChanged(channel, count);
            }
        });
        if (!tcp->start(QHostAddress(endpoint.config.tcpBindAddress), static_cast<quint16>(endpoint.config.tcpPort))) {
            setTransportState(channel, CatTransportKind::Tcp, QStringLiteral("TCP error: ") + tcp->errorString());
        } else { setTransportState(channel, CatTransportKind::Tcp, QStringLiteral("Listening")); }
    }
    if (!lifetime || (run != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) || !m_started) { return; }
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
            if (!lifetime || (run != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) || !m_started || !serial) { return; }
            endpoint.serial = serial;
            const std::weak_ptr<CatSerialTransport> weak(serial);
            const quint64 opened = createTransportSession(channel, CatTransportKind::Serial,
                [weak](quint64, const QByteArray& bytes) {
                    const auto owner = weak.lock(); if (!owner || !owner->isOpen()) { return false; }
                    owner->writeBytes(bytes); return owner->isOpen();
                }, [weak](quint64) { const auto owner = weak.lock(); if (owner) { owner->stop(); } });
            connect(serial.get(), &CatSerialTransport::bytesReceived, this, [this, weak, opened, channel, run, revision](const QByteArray& bytes) {
                const auto owner = weak.lock();
                if (owner && run == m_lifecycleGeneration && revision == m_channels[channel - 1].revision && m_channels[channel - 1].serial == owner) { processBytes(opened, bytes); }
            });
            connect(serial.get(), &CatSerialTransport::failed, this, [this, weak, channel, run, revision](const QString& error) {
                const auto owner = weak.lock();
                if (!owner || (run != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) || m_channels[channel - 1].serial != owner) { return; }
                const QPointer<CatService> self(this);
                const bool pinSource = m_ptt && m_ptt->transport == owner;
                closeSerialChannel(channel, owner);
                if (self && run == m_lifecycleGeneration && revision == m_channels[channel - 1].revision && !m_channels[channel - 1].serial) {
                    setTransportState(channel, CatTransportKind::Serial, "Serial error: " + error);
                    if (self && run == m_lifecycleGeneration && revision == m_channels[channel - 1].revision && pinSource && !m_ptt) { setPttState("PTT error: " + error); }
                }
            });
            const bool started = serial->start(config);
            if (!lifetime || (run != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) || m_channels[channel - 1].serial != serial) { return; }
            if (started) { setTransportState(channel, CatTransportKind::Serial, "Listening"); }
            else { closeSerialChannel(channel, serial); }
        };
        startSerial();
    }
    if (!lifetime || (run != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) || !m_started) { return; }
    if (config.ptyEnabled) { startPty(channel); }
    if (!lifetime || (run != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) || !m_started) { return; }
    if (config.rigctldEnabled) { startRigctld(channel); }
    if (!lifetime || (run != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) || !m_started) { return; }
    updateChannelState(channel);
}
void CatService::startRigctld(int channel)
{
    Channel& endpoint = m_channels[channel - 1];
    const quint64 revision = endpoint.revision;
    if (endpoint.config.rigctldEnabled) {
        const auto tcp = std::make_shared<CatTcpTransport>();
        endpoint.rigctld = tcp;
        const std::weak_ptr<CatTcpTransport> weak(tcp);
        const QPointer<CatService> self(this);
        const quint64 generation = m_lifecycleGeneration;
        connect(tcp.get(), &CatTcpTransport::clientAccepted, this, [this, self, weak, channel, generation, revision](QTcpSocket* socket) {
            const auto transport = weak.lock();
            if (!self || !transport || (generation != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) || !m_started
                || m_channels[channel - 1].rigctld != transport) { return; }
            const quint64 opened = createTransportSession(channel, CatTransportKind::Rigctld,
                [weak](quint64 id, const QByteArray& bytes) { const auto owner = weak.lock(); return owner && owner->writeBytes(id, bytes); },
                [weak](quint64 id) { const auto owner = weak.lock(); if (owner) { owner->closeSession(id); } });
            if (!opened || !transport->attachSession(opened, socket)) {
                if (self) { closeSession(opened); }
                return;
            }
            if (!self || (generation != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) || !session(opened)) { return; }
        });
        connect(tcp.get(), &CatTcpTransport::bytesReceived, this, [this, weak](quint64 id, const QByteArray& bytes) {
            const auto transport = weak.lock();
            if (transport) { processBytes(id, bytes); }
        });
        connect(tcp.get(), &CatTcpTransport::closeRequested, this, [this, weak](quint64 id) {
            const auto transport = weak.lock();
            if (transport) { closeSession(id); }
        });
        connect(tcp.get(), &CatTcpTransport::clientCountChanged, this, [this, weak, channel, generation, revision](int count) {
            const auto transport = weak.lock();
            if (transport && generation == m_lifecycleGeneration && revision == m_channels[channel - 1].revision && m_channels[channel - 1].rigctld == transport) {
                emit rigctldClientCountChanged(channel, count);
            }
        });
        if (!tcp->start(QHostAddress(endpoint.config.rigctldBindAddress), static_cast<quint16>(endpoint.config.rigctldPort))) {
            setTransportState(channel, CatTransportKind::Rigctld, QStringLiteral("Rigctld error: ") + tcp->errorString());
        } else { setTransportState(channel, CatTransportKind::Rigctld, QStringLiteral("Listening")); }
    }
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
    const quint64 revision = m_channels[channel - 1].revision;
    updateChannelState(channel);
    if (self && generation == m_lifecycleGeneration && revision == m_channels[channel - 1].revision) { emit transportStateChanged(channel, kind, state); }
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
    const quint64 revision = m_channels[channel - 1].revision;
    Channel& endpoint = m_channels[channel - 1];
    const auto pty = std::make_shared<CatPtyTransport>();
    endpoint.pty = pty;
    const std::weak_ptr<CatPtyTransport> weak(pty);
    const QPointer<CatService> self(this);
    const quint64 generation = m_lifecycleGeneration;
    connect(pty.get(), &CatPtyTransport::peerOpened, this, [this, self, weak, channel, generation, revision] {
        const auto owner = weak.lock();
        if (!self || !owner || (generation != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) || !m_started || m_channels[channel - 1].pty != owner) { return; }
        const quint64 id = createTransportSession(channel, CatTransportKind::Pty,
            [weak](quint64 sessionId, const QByteArray& bytes) { const auto transport = weak.lock(); return transport && transport->writeBytes(sessionId, bytes); },
            [weak](quint64 sessionId) { const auto transport = weak.lock(); if (transport) { transport->closeSession(sessionId); } });
        if (!id || !owner->attachSession(id)) { closeSession(id); return; }
        m_channels[channel - 1].ptySession = id;
        m_reporter->sessionsChanged(channel);
    });
    connect(pty.get(), &CatPtyTransport::peerClosed, this, [this, self, weak, channel, generation, revision](quint64 id) {
        const auto owner = weak.lock();
        if (!self || !owner || (generation != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) || m_channels[channel - 1].pty != owner) { return; }
        // The transport already dropped peer identity and both queues, so the close hook preserves the endpoint.
        closeSession(id);
    });
    connect(pty.get(), &CatPtyTransport::bytesReceived, this, [this, weak, channel, generation, revision](const QByteArray& bytes) {
        const auto owner = weak.lock();
        if (owner && generation == m_lifecycleGeneration && revision == m_channels[channel - 1].revision && m_channels[channel - 1].pty == owner) { processBytes(m_channels[channel - 1].ptySession, bytes); }
    });
    connect(pty.get(), &CatPtyTransport::failed, this, [this, self, weak, channel, generation, revision](const QString& error) {
        const auto owner = weak.lock();
        if (!self || !owner || (generation != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) || m_channels[channel - 1].pty != owner) { return; }
        const quint64 id = std::exchange(m_channels[channel - 1].ptySession, 0);
        closeSession(id);
        if (!self || (generation != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) || m_channels[channel - 1].pty != owner) { return; }
        emit ptyPathChanged(channel, {});
        if (self && generation == m_lifecycleGeneration && revision == m_channels[channel - 1].revision && m_channels[channel - 1].pty == owner) {
            setTransportState(channel, CatTransportKind::Pty, "PTY error: " + error);
        }
    });
    const bool opened = pty->start(channel, endpoint.config);
    if (!self || (generation != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) || m_channels[channel - 1].pty != pty) { return; }
    if (opened) {
        setTransportState(channel, CatTransportKind::Pty, "Listening");
        if (self && generation == m_lifecycleGeneration && revision == m_channels[channel - 1].revision && m_channels[channel - 1].pty == pty) { emit ptyPathChanged(channel, pty->slavePath()); }
    }
}
void CatService::stopChannel(int channel)
{
    const quint64 revision = m_channels[channel - 1].revision;
    const auto tcp = std::exchange(m_channels[channel - 1].tcp, {});
    const auto rigctld = std::exchange(m_channels[channel - 1].rigctld, {});
    const auto serial = std::exchange(m_channels[channel - 1].serial, {});
    const auto pty = std::exchange(m_channels[channel - 1].pty, {});
    m_channels[channel - 1].ptySession = 0;
    m_channels[channel - 1].transportStates.clear();
    const auto input = m_ptt && m_ptt->channel == channel ? std::exchange(m_ptt, {}) : nullptr;
    QHash<quint64, std::shared_ptr<CatSession>> sessions;
    for (quint64 id : sessionIds(channel)) { sessions.insert(id, m_sessions.take(id)); }
    for (const auto& current : sessions) { current->clearRuntime(); }
    for (quint64 id:m_testers.keys()) {
        if (m_testers.value(id)->context().channel==channel) { m_testers.take(id)->clearRuntime(); }
    }
    m_reporter->sessionsChanged(channel);
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
    if (rigctld) { rigctld->stop(); }
    if (input && input->separate) { input->transport->stop(); }
    if (!self) { return; }
    for (quint64 id : sessions.keys()) { emit sessionClosed(id); if (!self) { return; } }
    if ((generation != m_lifecycleGeneration || revision != m_channels[channel - 1].revision)) { return; }
    m_reporter->sessionsChanged(channel);
    emit ptyPathChanged(channel, {});
    if (!self || (generation != m_lifecycleGeneration || revision != m_channels[channel - 1].revision)) { return; }
    if (input && !m_ptt) { setPttState("Stopped"); }
    if (!self || (generation != m_lifecycleGeneration || revision != m_channels[channel - 1].revision)) { return; }
    emit rigctldClientCountChanged(channel, 0);
    if (!self || generation != m_lifecycleGeneration || revision != m_channels[channel - 1].revision) { return; }
    emit clientCountChanged(channel, 0);
    if (!self || (generation != m_lifecycleGeneration || revision != m_channels[channel - 1].revision)) { return; }
    setState(channel, QStringLiteral("Stopped"));
}
void CatService::stopAll()
{
    const QPointer<CatService> self(this);
    const quint64 generation = ++m_lifecycleGeneration;
    m_started = false;
    const auto testers = std::exchange(m_testers, {});
    for (const auto& tester:testers) { tester->clearRuntime(); }
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
    std::array<std::shared_ptr<CatTcpTransport>, 4> rigctldTransports;
    for (int index = 0; index < 4; ++index) { rigctldTransports[index] = std::exchange(m_channels[index].rigctld, {}); }
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
    for (const auto& transport : rigctldTransports) { if (transport) { transport->stop(); } }
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
        emit rigctldClientCountChanged(channel, 0);
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
        || (endpoint.pty && endpoint.pty->isOpen()) || (endpoint.rigctld && endpoint.rigctld->isListening());
}
int CatService::rigctldClientCount(int channel) const
{
    return validChannel(channel) && m_channels[channel - 1].rigctld ? m_channels[channel - 1].rigctld->clientCount() : 0;
}
QHostAddress CatService::rigctldBoundAddress(int channel) const
{
    return validChannel(channel) && m_channels[channel - 1].rigctld ? m_channels[channel - 1].rigctld->boundAddress() : QHostAddress();
}
quint16 CatService::rigctldBoundPort(int channel) const
{
    return validChannel(channel) && m_channels[channel - 1].rigctld ? m_channels[channel - 1].rigctld->boundPort() : 0;
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
CatGlobalConfig CatService::globalConfig() const { return m_desiredGlobal.value_or(m_settings.global()); }
namespace {
bool samePttIngress(const CatGlobalConfig& a, const CatGlobalConfig& b)
{
    return a.pttEnabled == b.pttEnabled && a.pttDeviceSource == b.pttDeviceSource
        && a.pttSerialDevice == b.pttSerialDevice && a.pttUseCts == b.pttUseCts && a.pttUseDsr == b.pttUseDsr
        && a.pttChannel == b.pttChannel && a.pttSerialBaud == b.pttSerialBaud && a.pttSerialParity == b.pttSerialParity
        && a.pttSerialDataBits == b.pttSerialDataBits && a.pttSerialStopBits == b.pttSerialStopBits;
}
}
bool CatService::applyGlobalConfig(const CatGlobalConfig& config)
{
    if (m_started && !samePttIngress(config, globalConfig())) { return false; }
    return reconfigureGlobal(config);
}
bool CatService::reconfigureGlobal(const CatGlobalConfig& supplied)
{
    if (m_destroying || !m_model || !m_model->ownsLocalDsp()) { return false; }
    const CatGlobalConfig config = supplied;
    if (!CatSettings::validateGlobal(config)) { return false; }
    if (config.pttEnabled && config.pttDeviceSource == "Physical") {
        for (const Channel& endpoint : m_channels) {
            if (endpoint.config.serialEnabled && endpoint.config.serialDevice == config.pttSerialDevice) { return false; }
        }
    }
    const CatGlobalConfig previous = globalConfig();
    if (config == previous) { return true; }
    const bool pttChanged = !samePttIngress(config, previous);
    const quint64 revision = ++m_globalRevision;
    const quint64 run = m_lifecycleGeneration;
    const bool started = m_started;
    const std::optional<PttRestart> previousRestart = m_pendingPttRestart;
    const bool restartPtt = pttChanged || (previousRestart && previousRestart->generation == run
        && previousRestart->revision > m_consumedPttRestartRevision);
    if (pttChanged && started) {
        if (previousRestart) { m_consumedPttRestartRevision = qMax(m_consumedPttRestartRevision,previousRestart->revision); }
        m_pendingPttRestart = PttRestart{run,revision};
    }
    m_desiredGlobal = config;
    const QPointer<CatService> self(this);
    const auto restartScope = qScopeGuard([self,previousRestart] {
        if (self) { self->m_pendingPttRestart = previousRestart; }
    });
    const auto current = [self, revision] { return self && !self->m_destroying && self->m_globalRevision == revision; };
    const auto discardOwnDesired = [self, revision] {
        if (self && self->m_globalRevision == revision) { self->m_desiredGlobal.reset(); }
    };
    if (pttChanged) { stopPtt(); }
    if (!current()) { discardOwnDesired(); return false; }
    if (!m_settings.setGlobal(config, current) || !current()) { discardOwnDesired(); return false; }
    m_desiredGlobal.reset();
    emit globalConfigurationChanged();
    if (!current()) { return false; }
    if (restartPtt && started && m_started && run == m_lifecycleGeneration && !m_ptt) {
        // Consume before open/sampling can report failure and reenter preferences.
        if (m_pendingPttRestart) { m_consumedPttRestartRevision = qMax(m_consumedPttRestartRevision,m_pendingPttRestart->revision); }
        startPtt();
    }
    return current();
}
QByteArray CatService::testCommand(int channel, const QByteArray& frame)
{
    if (m_destroying || !m_model || !m_model->ownsLocalDsp() || !validChannel(channel)) { return "?;"; }
    const quint64 id=++m_nextSessionId;
    const auto tester=std::make_shared<CatSession>(id,channel,CatTransportKind::Tester,channelConfig(channel).binding);
    const QPointer<CatService> self(this);
    m_testers.insert(id,tester);
    // Family handlers resolve this isolated session, but transport enumeration/reporting never sees it.
    const auto cleanup=qScopeGuard([self,id,tester] {
        tester->clearRuntime();
        if (self && self->m_testers.value(id)==tester) { self->m_testers.remove(id); }
    });
    const auto current=[self,id,tester] { return self && !self->m_destroying && self->m_testers.value(id)==tester; };
    emit messageLogged(channel,true,frame);
    if (!current()) { return "?;"; }
    const CatValidation validation=m_parser.validate(frame);
    if (!validation.request) {
        const QByteArray reply=m_parser.formatValidationError(validation,tester->context());
        if (!reply.isEmpty()) { emit messageLogged(channel,false,reply); }
        return reply;
    }
    const CatRequest request=*validation.request;
    // Native Tester policy: ZZLI1 calls PureSignalCoordinator::setAutoCalEnabled(true),
    // which starts calibration. Ordinary CAT transport behavior remains in CatTxCommands.
    const bool calibration=request.code=="ZZLI" && request.form==CatForm::Set && request.suffix=="1";
    const CatCommandResult result=calibration ? CatCommandResult{CatResultKind::Error,"?;"} : m_router.execute(request,tester->context());
    if (!current()) { return "?;"; }
    const QByteArray reply=m_parser.format(*m_catalog.find(request.code),request,result,tester->context());
    if (!reply.isEmpty()) { emit messageLogged(channel,false,reply); }
    return reply;
}
quint64 CatService::createTransportSession(int channel, CatTransportKind kind,
    std::function<bool(quint64, const QByteArray&)> write, std::function<void(quint64)> close)
{
    if (m_destroying || !m_started || !validChannel(channel)) { return 0; }
    const quint64 id = ++m_nextSessionId;
    const CatEndpointConfig config = channelConfig(channel);
    const CatWireDialect dialect = kind == CatTransportKind::Rigctld || (kind == CatTransportKind::Pty && config.ptyDialect == "Rigctld")
        ? CatWireDialect::Rigctld : CatWireDialect::Thetis;
    const auto current = std::make_shared<CatSession>(id, channel, kind, config.binding, dialect);
    if (dialect == CatWireDialect::Rigctld) { current->setRigctlProtocol(std::make_shared<RigctlProtocol>(m_adapter, m_txCoordinator, channel, id)); }
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
CatSession* CatService::session(quint64 id) { return m_sessions.contains(id) ? m_sessions.value(id).get() : m_testers.value(id).get(); }
QByteArray CatService::processFrame(quint64 id, const QByteArray& frame)
{
    const std::shared_ptr<CatSession> current = m_sessions.value(id);
    if (m_destroying || !m_started || !current || current->dialect() != CatWireDialect::Thetis) { return "?;"; }
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
    const bool rigctld = current->dialect() == CatWireDialect::Rigctld;
    const auto protocol = current->rigctlProtocol();
    const QList<QByteArray> frames = rigctld ? current->feedLines(bytes) : current->framer().feed(bytes);
    for (const QByteArray& frame : frames) {
        if (!self || generation != m_lifecycleGeneration || !m_started || m_sessions.value(id) != current) { return; }
        // Empty framer entry is one oversize event; no unbounded noise is logged.
        if (!frame.isEmpty()) {
            emit messageLogged(current->context().channel, true, frame);
            if (!self || generation != m_lifecycleGeneration || m_sessions.value(id) != current) { return; }
        }
        const QByteArray result = rigctld
            ? (frame.isEmpty() ? QByteArray("RPRT -1\n") : protocol->handleLine(QString::fromLatin1(frame)).toLatin1())
            : (frame.isEmpty() ? QByteArray("?;") : processFrame(id, frame));
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
    const quint64 configuration = m_globalRevision;
    const quint64 channelRevision = m_channels[input->channel - 1].revision;
    const auto currentOperation = [self, generation, configuration, channelRevision, channel = input->channel] {
        return self && !self->m_destroying && self->m_started && generation == self->m_lifecycleGeneration
            && configuration == self->m_globalRevision && channelRevision == self->m_channels[channel - 1].revision;
    };
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
    if (!currentOperation()) { return; }
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
        if (!currentOperation() || m_ptt != input || !opened) { return; }
    }
    const bool sampled = input->transport->setPinSampling(true);
    if (!currentOperation() || m_ptt != input || !sampled) { return; }
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
    const bool replacementOwnsTransport = self && self->m_ptt
        && self->m_ptt->transport == input->transport;
    if (!replacementOwnsTransport) {
        input->transport->setPinSampling(false);
        if (input->separate) { input->transport->stop(); }
    }
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
    const quint64 revision = m_channels[channel - 1].revision;
    // Detach every affected claim before cancellation can invoke model or lifecycle callbacks.
    if (input) { input->armed = false; input->asserted = false; coordinator->cancelSession(input->sessionId); }
    for (quint64 id : sessions.keys()) { if (coordinator) { coordinator->cancelSession(id); } }
    // Only retained old sessions/handles are closed, even after callback deletion/restart.
    if (input) { input->transport->setPinSampling(false); }
    for (const auto& current : sessions) { current->closeTransport(); }
    transport->stop();
    if (!self) { return; }
    for (quint64 id : sessions.keys()) { emit sessionClosed(id); if (!self) { return; } }
    if ((generation != m_lifecycleGeneration || revision != m_channels[channel - 1].revision)) { return; }
    m_reporter->sessionsChanged(channel);
    if (input && !m_ptt) { setPttState("Stopped"); }
    if (!self || (generation != m_lifecycleGeneration || revision != m_channels[channel - 1].revision)) { return; }
    if (!m_channels[channel - 1].serial) {
        setTransportState(channel, CatTransportKind::Serial, "Stopped");
    }
}
} // namespace NereusSDR
