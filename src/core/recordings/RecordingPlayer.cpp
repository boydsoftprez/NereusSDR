// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 J.J. Boyd (KG4VCF)
// no-port-check: NereusSDR-original recording player.
// Modification history (NereusSDR):
// 2026-11-15  J.J. Boyd / KG4VCF; one playback of a recording (spec 6.1
//             Player, 6.3). AI tooling: Anthropic Claude Code.

#include "RecordingPlayer.h"

#include "core/Resampler.h"
#include "core/recordings/AudioFileReader.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace NereusSDR {

namespace {

// Room for r8brain's uneven output per call beyond the chunk's share.
constexpr int kResamplerSlackFrames = 4096;

}  // namespace

RecordingPlayer::RecordingPlayer(qint64 startMs, qint64 lengthMs)
    : m_startMs(std::max<qint64>(0, startMs))
    , m_lengthMs(std::max<qint64>(0, lengthMs))
{
}

RecordingPlayer::~RecordingPlayer() = default;

bool RecordingPlayer::open(const QString& path, QString* error)
{
    std::unique_ptr<AudioFileReader> reader = AudioFileReader::open(path, error);
    if (!reader) {
        return false;
    }
    const int rate = reader->sampleRateHz();
    const int channels = reader->channels();
    if (rate < kMinFileRateHz || channels < 1) {
        if (error) {
            *error = QStringLiteral("unsupported rate %1 Hz or channels %2").arg(rate).arg(channels);
        }
        return false;
    }
    const qint64 lengthFrames = reader->lengthFrames();
    if (lengthFrames > 0) {
        m_lengthMs.store(lengthFrames * 1000 / rate, std::memory_order_release);
    }
    qint64 startFrame = m_startMs * rate / 1000;
    if (lengthFrames > 0) {
        startFrame = std::min(startFrame, lengthFrames);
    }
    if (startFrame > 0 && !reader->seekFrame(startFrame)) {
        if (error) {
            *error = QStringLiteral("cannot seek to frame %1").arg(startFrame);
        }
        return false;
    }

    m_rateHz = rate;
    m_channels = channels;
    m_expectedFrames = lengthFrames > 0
        ? (lengthFrames - startFrame) * kOutputRateHz / rate
        : std::numeric_limits<qint64>::max();
    int outCapacity = kChunkFrames + kResamplerSlackFrames;
    if (rate != kOutputRateHz) {
        m_resampler = std::make_unique<Resampler>(static_cast<double>(rate),
                                                  static_cast<double>(kOutputRateHz), kChunkFrames);
        outCapacity = static_cast<int>(std::ceil(static_cast<double>(kChunkFrames) * kOutputRateHz / rate))
            + kResamplerSlackFrames;
    }
    m_read.assign(static_cast<size_t>(kChunkFrames) * static_cast<size_t>(channels), 0.0f);
    m_mono.assign(static_cast<size_t>(kChunkFrames), 0.0f);
    m_out.assign(static_cast<size_t>(outCapacity), 0.0f);
    m_reader = std::move(reader);
    return true;
}

void RecordingPlayer::fill()
{
    if (!m_reader || m_decodeDone.load(std::memory_order_acquire)) {
        return;
    }
    const int outCapacity = static_cast<int>(m_out.size());
    while (m_ring.available() < kFillFrames && m_ring.capacity() - m_ring.available() >= outCapacity) {
        if (!m_readerDone) {
            const int n = m_reader->read(m_read.data(), kChunkFrames);
            if (n <= 0) {
                m_readerDone = true;
                // What the resampler still holds comes out behind zeros.
                m_flushLeft = m_resampler ? m_resampler->latencyInputSamples() : 0;
                continue;
            }
            if (m_channels == 1) {
                std::copy(m_read.begin(), m_read.begin() + n, m_mono.begin());
            } else {
                // The channels averaged, as the contact recorder folds the
                // receiver's two sides.
                const float scale = 1.0f / static_cast<float>(m_channels);
                for (int i = 0; i < n; ++i) {
                    float sum = 0.0f;
                    for (int c = 0; c < m_channels; ++c) {
                        sum += m_read[static_cast<size_t>(i * m_channels + c)];
                    }
                    m_mono[static_cast<size_t>(i)] = sum * scale;
                }
            }
            if (m_resampler) {
                const int out = m_resampler->processInto(m_mono.data(), n, m_out.data(), outCapacity);
                push(m_out.data(), out);
            } else {
                push(m_mono.data(), n);
            }
        } else if (m_flushLeft > 0 && m_producedFrames < m_expectedFrames) {
            const int k = std::min(kChunkFrames, m_flushLeft);
            std::fill(m_mono.begin(), m_mono.begin() + k, 0.0f);
            m_flushLeft -= k;
            const int out = m_resampler->processInto(m_mono.data(), k, m_out.data(), outCapacity);
            push(m_out.data(), out);
        } else {
            m_decodeDone.store(true, std::memory_order_release);
            return;
        }
    }
}

void RecordingPlayer::push(const float* mono, int frames)
{
    const qint64 left = m_expectedFrames - m_producedFrames;
    const int n = static_cast<int>(std::min<qint64>(frames, std::max<qint64>(0, left)));
    if (n <= 0) {
        return;
    }
    // fill() leaves room for a whole chunk's output, so this always fits.
    if (m_ring.tryPush(mono, n)) {
        m_producedFrames += n;
    }
}

int RecordingPlayer::pull(float* mono48k, int frames) noexcept
{
    if (mono48k == nullptr || frames <= 0) {
        return 0;
    }
    const int n = m_ring.pop(mono48k, frames);
    if (n < frames) {
        std::fill(mono48k + n, mono48k + frames, 0.0f);
    }
    if (n > 0) {
        m_consumedFrames.fetch_add(n, std::memory_order_acq_rel);
    }
    return n;
}

qint64 RecordingPlayer::positionMs() const noexcept
{
    const qint64 at = m_startMs + m_consumedFrames.load(std::memory_order_acquire) * 1000 / kOutputRateHz;
    const qint64 length = m_lengthMs.load(std::memory_order_acquire);
    return length > 0 ? std::min(at, length) : at;
}

qint64 RecordingPlayer::lengthMs() const noexcept
{
    return m_lengthMs.load(std::memory_order_acquire);
}

bool RecordingPlayer::finished() const noexcept
{
    return m_decodeDone.load(std::memory_order_acquire) && m_ring.available() == 0;
}

}  // namespace NereusSDR
