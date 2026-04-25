// =================================================================
// src/core/audio/QAudioSinkAdapter.h  (NereusSDR)
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
//   2026-04-24  J.J. Boyd (KG4VCF) — created for Phase 3O Task 13.
// =================================================================
#pragma once

#include "core/IAudioBus.h"
#include <QAudioDevice>
#include <QAudioSink>
#include <atomic>
#include <memory>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// QAudioSinkAdapter — IAudioBus concrete adapter wrapping QAudioSink.
//
// This is the Qt6-Multimedia-based fallback for:
//   - Linux hosts where PipeWire detection returned Pactl or None
//   - Windows / macOS Primary speakers path (where PipeWire doesn't exist)
//
// Registered in cross-platform CORE_SOURCES (always built). Task 14 wires
// AudioEngine::makeVaxBus to dispatch between PipeWireBus and this adapter
// based on the detected backend. Task 24 adds the integration test.
//
// pull() is a no-op — this adapter is output-only. rxLevel/txLevel exist
// per IAudioBus contract but stay zero until metering wiring lands in a
// later task.
// ---------------------------------------------------------------------------
class QAudioSinkAdapter : public IAudioBus {
public:
    explicit QAudioSinkAdapter(const QAudioDevice& device);
    ~QAudioSinkAdapter() override;

    // Non-copyable / non-movable — owns a live QAudioSink.
    QAudioSinkAdapter(const QAudioSinkAdapter&)            = delete;
    QAudioSinkAdapter& operator=(const QAudioSinkAdapter&) = delete;

    // -----------------------------------------------------------------------
    // IAudioBus overrides
    // -----------------------------------------------------------------------

    // Lifecycle. open() validates format via QAudioDevice::isFormatSupported,
    // then starts the sink. Returns false if the format is unsupported OR if
    // QAudioSink::start() returns null; errorString() has details.
    bool open(const AudioFormat& fmt) override;
    void close() override;
    bool isOpen() const override { return m_sink != nullptr; }

    // Producer (RX push). Writes interleaved PCM bytes to the sink's IO device.
    // Audio-thread safe via QIODevice::write().
    qint64 push(const char* data, qint64 bytes) override;

    // Consumer (TX pull). No-op — this adapter is output-only.
    qint64 pull(char*, qint64) override { return 0; }

    // Metering — both atomics initialised to 0.0f. Written by metering wiring
    // in a later task; they exist here to satisfy the interface.
    float rxLevel() const override { return m_rxLevel.load(); }
    float txLevel() const override { return m_txLevel.load(); }

    // Diagnostics.
    QString     backendName()      const override { return QStringLiteral("QAudioSink"); }
    AudioFormat negotiatedFormat() const override { return m_negotiatedFormat; }
    QString     errorString()      const override { return m_err; }

private:
    QAudioDevice              m_device;
    std::unique_ptr<QAudioSink> m_sink;
    QIODevice*                m_io = nullptr;
    QString                   m_err;

    // Metering. Written by a later task; readers always see 0.0f for now.
    std::atomic<float>  m_rxLevel{0.0f};
    std::atomic<float>  m_txLevel{0.0f};

    // Cached on open() after both isFormatSupported and start() pass.
    // A failed open() leaves this empty rather than reporting a stale request.
    AudioFormat m_negotiatedFormat;
};

}  // namespace NereusSDR
