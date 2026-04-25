// =================================================================
// src/core/audio/PipeWireBus.h  (NereusSDR)
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
#pragma once

#ifdef NEREUS_HAVE_PIPEWIRE

#include <QString>
#include <atomic>
#include <memory>

#include "core/IAudioBus.h"
#include "core/audio/PipeWireStream.h"

namespace NereusSDR {

class PipeWireThreadLoop;

// ---------------------------------------------------------------------------
// PipeWireBus — IAudioBus concrete adapter wrapping a single PipeWireStream.
//
// One Role enum value per Linux audio channel. configFor() centralises the
// role → StreamConfig mapping (node name, media.class, media.role, quantum).
// Quantum defaults per spec §6.1:
//   128  — Sidetone (lowest latency, CW/voice monitoring)
//   256  — TxInput, Monitor
//   512  — VAX 1-4, Primary, PerSlice
//
// Task 14 wires this into AudioEngine::makeVaxBus. Task 24 adds the
// env-gated integration test for the whole stack.
// ---------------------------------------------------------------------------
class PipeWireBus : public IAudioBus {
public:
    enum class Role {
        Vax1,
        Vax2,
        Vax3,
        Vax4,
        TxInput,
        Primary,
        Sidetone,
        Monitor,
        PerSlice,   // alias for Primary, target keyed by targetOverride
    };

    // targetOverride  — node.name of the PipeWire sink/source to connect to.
    //                   Empty = follow PipeWire default routing.
    // nodeNameOverride — node.name advertised by this stream. Empty = use the
    //                   role default from configFor().
    explicit PipeWireBus(Role role,
                         PipeWireThreadLoop* loop,
                         QString targetOverride   = {},
                         QString nodeNameOverride = {});
    ~PipeWireBus() override;

    // Non-copyable / non-movable — owns a live PipeWire stream.
    PipeWireBus(const PipeWireBus&)            = delete;
    PipeWireBus& operator=(const PipeWireBus&) = delete;

    // -----------------------------------------------------------------------
    // IAudioBus overrides
    // -----------------------------------------------------------------------

    // Lifecycle.
    // open() validates AudioFormat (48k / stereo / Float32 only in Task 12;
    // negotiation surface widened in Task 14). Returns false on mismatch or
    // stream-open failure; errorString() has details.
    bool open(const AudioFormat& format) override;
    void close() override;
    bool isOpen() const override { return m_open; }

    // Producer (RX push) / consumer (TX pull). Audio-thread safe.
    qint64 push(const char* data, qint64 bytes) override;
    qint64 pull(char* data, qint64 maxBytes) override;

    // Metering. rxLevel defers to PipeWireStream which computes RMS on
    // every push() call. txLevel still returns the unwritten m_txLevel
    // atomic (TX-side metering lands when Phase 3M wires the consumer).
    float rxLevel() const override {
        return m_stream ? m_stream->rxLevel() : 0.0f;
    }
    float txLevel() const override { return m_txLevel.load(); }

    // Diagnostics.
    QString     backendName()      const override { return QStringLiteral("PipeWire"); }
    AudioFormat negotiatedFormat() const override { return m_negotiatedFormat; }
    QString     errorString()      const override { return m_err; }

    // -----------------------------------------------------------------------
    // Accessor for telemetry / testing (Task 24).
    // -----------------------------------------------------------------------
    PipeWireStream* stream() const { return m_stream.get(); }

private:
    StreamConfig configFor(Role role,
                           const QString& target,
                           const QString& nodeName) const;

    Role                         m_role;
    PipeWireThreadLoop*          m_loop;
    std::unique_ptr<PipeWireStream> m_stream;
    bool                         m_open = false;
    QString                      m_err;

    // Metering. Written by a later task; readers always see 0.0f for now.
    std::atomic<float>  m_rxLevel{0.0f};
    std::atomic<float>  m_txLevel{0.0f};

    // Cached on open(), returned by negotiatedFormat().
    AudioFormat m_negotiatedFormat;
};

}  // namespace NereusSDR

#endif  // NEREUS_HAVE_PIPEWIRE
