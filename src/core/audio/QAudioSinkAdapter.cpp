// =================================================================
// src/core/audio/QAudioSinkAdapter.cpp  (NereusSDR)
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
#include "core/audio/QAudioSinkAdapter.h"

#include <QAudioFormat>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
QAudioSinkAdapter::QAudioSinkAdapter(const QAudioDevice& device)
    : m_device(device)
{}

QAudioSinkAdapter::~QAudioSinkAdapter()
{
    close();
}

// ---------------------------------------------------------------------------
// open() — validates AudioFormat, creates and starts QAudioSink.
//
// m_negotiatedFormat is cached ONLY after both isFormatSupported() AND
// start() succeed. This diverges from PipeWireBus (which caches on attempt)
// because the QAudioSink path has two distinct failure modes:
//   1. isFormatSupported() → false   (format incompatible with device)
//   2. start() → null               (device unavailable at runtime)
// Caching only on full success means negotiatedFormat() always reflects an
// actually-opened format, never a stale failed request.
// ---------------------------------------------------------------------------
bool QAudioSinkAdapter::open(const AudioFormat& fmt)
{
    QAudioFormat qf;
    qf.setSampleRate(int(fmt.sampleRate));
    qf.setChannelCount(int(fmt.channels));
    qf.setSampleFormat(QAudioFormat::Float);

    if (!m_device.isFormatSupported(qf)) {
        m_err = QStringLiteral("QAudioSinkAdapter: unsupported format");
        return false;
    }

    m_sink = std::make_unique<QAudioSink>(m_device, qf);
    m_io   = m_sink->start();
    if (!m_io) {
        m_sink.reset();
        m_err = QStringLiteral("QAudioSinkAdapter: QAudioSink::start() returned null");
        return false;
    }

    m_negotiatedFormat = fmt;
    m_err.clear();
    return true;
}

// ---------------------------------------------------------------------------
// close() — idempotent.
// ---------------------------------------------------------------------------
void QAudioSinkAdapter::close()
{
    if (m_sink) {
        m_sink->stop();
        m_sink.reset();
        m_io = nullptr;
    }
}

// ---------------------------------------------------------------------------
// push() — write interleaved PCM bytes to the sink's IO device.
// ---------------------------------------------------------------------------
qint64 QAudioSinkAdapter::push(const char* data, qint64 bytes)
{
    return m_io ? m_io->write(data, bytes) : 0;
}

}  // namespace NereusSDR
