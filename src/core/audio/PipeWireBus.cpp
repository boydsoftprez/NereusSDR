// =================================================================
// src/core/audio/PipeWireBus.cpp  (NereusSDR)
//   Copyright (C) 2026 J.J. Boyd (KG4VCF) — GPLv2-or-later.
//   2026-04-24 — created. AI-assisted via Claude Code.
// =================================================================
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of
// the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// Modification history (NereusSDR):
//   2026-04-24  J.J. Boyd (KG4VCF) — created for Phase 3O Task 12.
// =================================================================
#ifdef NEREUS_HAVE_PIPEWIRE

#include "core/audio/PipeWireBus.h"
#include "core/audio/PipeWireThreadLoop.h"

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcPw)

namespace NereusSDR {

// ---------------------------------------------------------------------------
// configFor() — role → StreamConfig translation.
//
// Quantum defaults per spec §6.1:
//   128  — Sidetone  (CW/voice monitoring: lowest latency path)
//   256  — TxInput, Monitor
//   512  — VAX 1-4, Primary, PerSlice
//
// media.class is "Stream/Output/Audio" for all TX-feed / VAX playback roles
// (these are client → daemon → sink streams, i.e. PW Output direction).
// TxInput is a capture path (daemon → client): "Stream/Input/Audio".
// ---------------------------------------------------------------------------
StreamConfig PipeWireBus::configFor(Role role,
                                    const QString& target,
                                    const QString& nodeName) const
{
    StreamConfig cfg;
    cfg.rate     = 48000;
    cfg.channels = 2;
    cfg.targetNodeName = target;

    switch (role) {
        case Role::Vax1:
            cfg.nodeName        = nodeName.isEmpty() ? QStringLiteral("nereussdr.vax-1") : nodeName;
            cfg.nodeDescription = QStringLiteral("NereusSDR VAX 1");
            cfg.direction       = StreamConfig::Output;
            cfg.mediaClass      = QStringLiteral("Audio/Source");
            cfg.mediaRole       = QStringLiteral("Music");
            cfg.quantum         = 512;
            break;
        case Role::Vax2:
            cfg.nodeName        = nodeName.isEmpty() ? QStringLiteral("nereussdr.vax-2") : nodeName;
            cfg.nodeDescription = QStringLiteral("NereusSDR VAX 2");
            cfg.direction       = StreamConfig::Output;
            cfg.mediaClass      = QStringLiteral("Audio/Source");
            cfg.mediaRole       = QStringLiteral("Music");
            cfg.quantum         = 512;
            break;
        case Role::Vax3:
            cfg.nodeName        = nodeName.isEmpty() ? QStringLiteral("nereussdr.vax-3") : nodeName;
            cfg.nodeDescription = QStringLiteral("NereusSDR VAX 3");
            cfg.direction       = StreamConfig::Output;
            cfg.mediaClass      = QStringLiteral("Audio/Source");
            cfg.mediaRole       = QStringLiteral("Music");
            cfg.quantum         = 512;
            break;
        case Role::Vax4:
            cfg.nodeName        = nodeName.isEmpty() ? QStringLiteral("nereussdr.vax-4") : nodeName;
            cfg.nodeDescription = QStringLiteral("NereusSDR VAX 4");
            cfg.direction       = StreamConfig::Output;
            cfg.mediaClass      = QStringLiteral("Audio/Source");
            cfg.mediaRole       = QStringLiteral("Music");
            cfg.quantum         = 512;
            break;
        case Role::TxInput:
            cfg.nodeName        = nodeName.isEmpty() ? QStringLiteral("nereussdr.tx-input") : nodeName;
            cfg.nodeDescription = QStringLiteral("NereusSDR TX input");
            cfg.direction       = StreamConfig::Input;
            cfg.mediaClass      = QStringLiteral("Stream/Input/Audio");
            cfg.mediaRole       = QStringLiteral("Phone");
            cfg.quantum         = 256;
            break;
        case Role::Primary:
            cfg.nodeName        = nodeName.isEmpty() ? QStringLiteral("nereussdr.rx-primary") : nodeName;
            cfg.nodeDescription = QStringLiteral("NereusSDR Primary Output");
            cfg.direction       = StreamConfig::Output;
            cfg.mediaClass      = QStringLiteral("Stream/Output/Audio");
            cfg.mediaRole       = QStringLiteral("Music");
            cfg.quantum         = 512;
            break;
        case Role::Sidetone:
            cfg.nodeName        = nodeName.isEmpty() ? QStringLiteral("nereussdr.cw-sidetone") : nodeName;
            cfg.nodeDescription = QStringLiteral("NereusSDR CW Sidetone");
            cfg.direction       = StreamConfig::Output;
            cfg.mediaClass      = QStringLiteral("Stream/Output/Audio");
            cfg.mediaRole       = QStringLiteral("Communication");
            cfg.quantum         = 128;
            break;
        case Role::Monitor:
            cfg.nodeName        = nodeName.isEmpty() ? QStringLiteral("nereussdr.tx-monitor") : nodeName;
            cfg.nodeDescription = QStringLiteral("NereusSDR TX Monitor");
            cfg.direction       = StreamConfig::Output;
            cfg.mediaClass      = QStringLiteral("Stream/Output/Audio");
            cfg.mediaRole       = QStringLiteral("Communication");
            cfg.quantum         = 256;
            break;
        case Role::PerSlice:
            // PerSlice is a target-keyed alias for Primary. The caller supplies
            // targetOverride to pin routing to a specific sink. nodeNameOverride
            // distinguishes multiple per-slice instances (e.g. "nereus-slice-1").
            cfg.nodeName        = nodeName.isEmpty() ? QStringLiteral("nereussdr.rx-primary") : nodeName;
            cfg.nodeDescription = QStringLiteral("NereusSDR Primary Output");
            cfg.direction       = StreamConfig::Output;
            cfg.mediaClass      = QStringLiteral("Stream/Output/Audio");
            cfg.mediaRole       = QStringLiteral("Music");
            cfg.quantum         = 512;
            break;
    }

    return cfg;
}

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
PipeWireBus::PipeWireBus(Role role,
                         PipeWireThreadLoop* loop,
                         QString targetOverride,
                         QString nodeNameOverride)
    : m_role(role)
    , m_loop(loop)
{
    StreamConfig cfg = configFor(role, targetOverride, nodeNameOverride);
    m_stream = std::make_unique<PipeWireStream>(m_loop, std::move(cfg), /*parent=*/nullptr);
}

PipeWireBus::~PipeWireBus()
{
    close();
}

// ---------------------------------------------------------------------------
// open() — validates AudioFormat then delegates to the underlying stream.
//
// Task 12 accepts only 48k / stereo / Float32. Task 14 widens this when it
// adds the negotiation surface; m_negotiatedFormat is cached here so that
// negotiatedFormat() returns the right value regardless of the validation
// window that is active at the time.
// ---------------------------------------------------------------------------
bool PipeWireBus::open(const AudioFormat& fmt)
{
    if (m_open) {
        qCWarning(lcPw) << "PipeWireBus::open() called while already open";
        return false;
    }

    // Validate: 48 kHz / stereo / Float32 only for Task 12.
    if (fmt.sampleRate != 48000
        || fmt.channels != 2
        || fmt.sample != AudioFormat::Sample::Float32)
    {
        m_err = QStringLiteral("PipeWireBus: unsupported format "
                               "(require 48000/stereo/Float32)");
        qCWarning(lcPw) << m_err;
        return false;
    }

    // Cache the negotiated format BEFORE delegating so that negotiatedFormat()
    // returns the correct value even if the caller queries it synchronously.
    m_negotiatedFormat = fmt;

    if (!m_stream->open()) {
        m_err = QStringLiteral("PipeWireBus: PipeWireStream::open() failed");
        qCWarning(lcPw) << m_err;
        return false;
    }

    m_open = true;
    m_err.clear();
    return true;
}

// ---------------------------------------------------------------------------
// close() — idempotent.
// ---------------------------------------------------------------------------
void PipeWireBus::close()
{
    if (!m_open) { return; }
    m_stream->close();
    m_open = false;
}

// ---------------------------------------------------------------------------
// push() / pull() — forward to the underlying ring via PipeWireStream.
// ---------------------------------------------------------------------------
qint64 PipeWireBus::push(const char* data, qint64 bytes)
{
    if (!m_open) { return -1; }
    return m_stream->push(data, bytes);
}

qint64 PipeWireBus::pull(char* data, qint64 maxBytes)
{
    if (!m_open) { return -1; }
    return m_stream->pull(data, maxBytes);
}

}  // namespace NereusSDR

#endif  // NEREUS_HAVE_PIPEWIRE
