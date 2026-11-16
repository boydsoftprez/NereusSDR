// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original on-air source. Thetis is cited for
// behavior only (clsAudioRecordPlayback.cs [v2.10.3.15]); no logic is ported.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; the on-air source and the five processing
//             switches (spec 3, 6.1, 6.3, V5, V6, V7, A6, A7). AI tooling:
//             Anthropic Claude Code.

#include "OnAirSource.h"

#include "core/MicProfileManager.h"
#include "core/TxChannel.h"
#include "core/recordings/RecordingService.h"
#include "models/TransmitModel.h"

#include <algorithm>
#include <thread>

namespace NereusSDR {

OnAirSource::OnAirSource(RecordingService& service)
    : m_service(service)
{
}

OnAirSource::~OnAirSource()
{
    end();
}

void OnAirSource::setTransmitChain(std::function<TxChannel*()> channel, TransmitModel* model)
{
    m_channel = std::move(channel);
    m_model = model;
}

bool OnAirSource::pull(float* mono48k, int frames) noexcept
{
    if (mono48k == nullptr || frames <= 0 || m_kind.load(std::memory_order_seq_cst) == Kind::None) {
        return false;
    }
    m_pullsInFlight.fetch_add(1, std::memory_order_seq_cst);
    bool used = false;
    const Kind kind = m_kind.load(std::memory_order_seq_cst);
    if (kind == Kind::Playback) {
        const int channel = m_playChannel.load(std::memory_order_seq_cst);
        used = channel >= 0 && m_service.pullOnAir(channel, mono48k, frames) >= 0;
    } else if (kind == Kind::VoiceMessage) {
        if (std::function<int(float*, int)>* clip = m_clip.load(std::memory_order_seq_cst)) {
            const int n = std::clamp((*clip)(mono48k, frames), 0, frames);
            if (n > 0) {
                std::fill(mono48k + n, mono48k + frames, 0.0f);
                used = true;
            }
        }
    }
    m_pullsInFlight.fetch_sub(1, std::memory_order_seq_cst);
    return used;
}

OnAirSource::Kind OnAirSource::active() const noexcept
{
    return m_kind.load(std::memory_order_seq_cst);
}

void OnAirSource::beginPlayback(const ListenerId& who)
{
    if (m_kind.load(std::memory_order_seq_cst) != Kind::None) {
        return;
    }
    // The speakers stop pulling before the transmitter starts: one reader.
    const int channel = m_service.beginOnAir(who);
    if (channel < 0) {
        return;
    }
    m_listener = who;
    m_playChannel.store(channel, std::memory_order_seq_cst);
    holdSwitches();
    m_kind.store(Kind::Playback, std::memory_order_seq_cst);
}

void OnAirSource::beginVoiceMessage(const QString& slotId, std::function<int(float*, int)> pullClip)
{
    if (!pullClip) {
        return;
    }
    end();
    m_voiceSlot = slotId;
    m_clipOwner = std::make_unique<std::function<int(float*, int)>>(std::move(pullClip));
    m_clip.store(m_clipOwner.get(), std::memory_order_seq_cst);
    m_kind.store(Kind::VoiceMessage, std::memory_order_seq_cst);
}

void OnAirSource::end()
{
    const Kind was = m_kind.exchange(Kind::None, std::memory_order_seq_cst);
    // No transmit block reads from here once this returns.
    waitForPulls();
    const int channel = m_playChannel.exchange(-1, std::memory_order_seq_cst);
    m_clip.store(nullptr, std::memory_order_seq_cst);
    m_clipOwner.reset();
    m_voiceSlot.clear();
    if (was == Kind::Playback || m_listener.has_value()) {
        m_listener.reset();
        releaseSwitches();
        if (channel >= 0) {
            // Back to the speakers, from where the transmitter left it.
            m_service.endOnAir(channel);
        }
    }
}

std::optional<ListenerId> OnAirSource::playbackListener() const
{
    return m_listener;
}

void OnAirSource::holdSwitches()
{
    const RecordingSettings settings = m_service.settings();
    m_held = 0;
    if (!settings.onAirUseTxEq) {
        m_held |= kTxEq;
    }
    if (!settings.onAirUseCompressor) {
        m_held |= kCompressor;
    }
    if (!settings.onAirUseLeveler) {
        m_held |= kLeveler;
    }
    if (!settings.onAirUseCfc) {
        m_held |= kCfc;
    }
    if (!settings.onAirUsePhaseRotator) {
        m_held |= kPhaseRotator;
    }
    if (m_held == 0) {
        return;
    }
    assertHeld();
    // A change to one of them while on the air stays in TransmitModel and is
    // applied at the end (releaseSwitches). RadioModel's own connections,
    // made when the radio connected, run before these, so a held switch the
    // user turns on (or a profile that turns it on) is turned off again in
    // the same call, before the event loop runs again.
    if (m_model != nullptr) {
        const auto reassert = [this] { assertHeld(); };
        m_holdConnections.append(QObject::connect(m_model, &TransmitModel::txEqEnabledChanged, &m_context, reassert));
        m_holdConnections.append(QObject::connect(m_model, &TransmitModel::cpdrOnChanged, &m_context, reassert));
        m_holdConnections.append(QObject::connect(m_model, &TransmitModel::txLevelerOnChanged, &m_context, reassert));
        m_holdConnections.append(QObject::connect(m_model, &TransmitModel::cfcEnabledChanged, &m_context, reassert));
        m_holdConnections.append(
            QObject::connect(m_model, &TransmitModel::cfcEditProfileChanged, &m_context, reassert));
        m_holdConnections.append(
            QObject::connect(m_model, &TransmitModel::phaseRotatorEnabledChanged, &m_context, reassert));
    }
    if (MicProfileManager* profiles = m_service.micProfiles()) {
        m_holdConnections.append(QObject::connect(profiles, &MicProfileManager::activeProfileChanged, &m_context,
                                                  [this] { assertHeld(); }));
    }
}

void OnAirSource::assertHeld() const
{
    TxChannel* ch = m_channel ? m_channel() : nullptr;
    if (ch == nullptr) {
        return;
    }
    if ((m_held & kTxEq) != 0u) {
        ch->setTxEqRunning(false);
    }
    if ((m_held & kCompressor) != 0u) {
        ch->setTxCpdrOn(false);
    }
    if ((m_held & kLeveler) != 0u) {
        ch->setTxLevelerOn(false);
    }
    if ((m_held & kCfc) != 0u) {
        ch->setTxCfcRunning(false);
    }
    if ((m_held & kPhaseRotator) != 0u) {
        ch->setStageRunning(TxChannel::Stage::PhRot, false);
    }
}

void OnAirSource::releaseSwitches()
{
    for (const QMetaObject::Connection& c : std::as_const(m_holdConnections)) {
        QObject::disconnect(c);
    }
    m_holdConnections.clear();
    const unsigned held = m_held;
    m_held = 0;
    TxChannel* ch = m_channel ? m_channel() : nullptr;
    if (held == 0u || ch == nullptr || m_model == nullptr) {
        return;
    }
    // Whatever the user has now, as RadioModel applies it.
    if ((held & kTxEq) != 0u) {
        ch->setTxEqRunning(m_model->txEqEnabled());
    }
    if ((held & kCompressor) != 0u) {
        ch->setTxCpdrOn(m_model->cpdrOn());
    }
    if ((held & kLeveler) != 0u) {
        ch->setTxLevelerOn(m_model->txLevelerOn());
    }
    if ((held & kCfc) != 0u) {
        ch->setTxCfcRunning(m_model->cfcEnabled());
    }
    if ((held & kPhaseRotator) != 0u) {
        ch->setStageRunning(TxChannel::Stage::PhRot, m_model->phaseRotatorEnabled());
    }
}

void OnAirSource::waitForPulls() const
{
    while (m_pullsInFlight.load(std::memory_order_seq_cst) != 0) {
        std::this_thread::yield();
    }
}

}  // namespace NereusSDR
