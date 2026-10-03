// =================================================================
// src/core/audio/CaptureAudioBus.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Lock-free reader ring behind the
// capture helper; no Thetis logic.
// =================================================================

#include "core/audio/CaptureAudioBus.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace NereusSDR {

namespace {

constexpr int kBytesPerFrame = static_cast<int>(sizeof(float));

} // namespace

bool CaptureAudioBus::open(const AudioFormat& /*format*/)
{
    return true;
}

void CaptureAudioBus::close()
{
}

bool CaptureAudioBus::isOpen() const
{
    return m_available.load(std::memory_order_acquire);
}

qint64 CaptureAudioBus::push(const char* /*data*/, qint64 /*bytes*/)
{
    return 0;
}

qint64 CaptureAudioBus::pull(char* data, qint64 maxBytes)
{
    if (data == nullptr || maxBytes < kBytesPerFrame || !m_available.load(std::memory_order_acquire)) {
        return 0;
    }
    quint64 read = m_read.load(std::memory_order_relaxed);
    const quint64 floor = m_discardFloor.load(std::memory_order_acquire);
    if (read < floor) {
        read = floor;
    }
    const quint64 written = m_written.load(std::memory_order_acquire);
    if (written <= read) {
        if (read != m_read.load(std::memory_order_relaxed)) {
            m_read.store(read, std::memory_order_release);
        }
        return 0;
    }
    const quint64 wanted = static_cast<quint64>(maxBytes / kBytesPerFrame);
    const quint64 frames = std::min(written - read, wanted);
    const int start = static_cast<int>(read % kRingFrames);
    const int first = std::min(static_cast<int>(frames), kRingFrames - start);
    std::memcpy(data, &m_ring[static_cast<std::size_t>(start)],
                static_cast<std::size_t>(first) * kBytesPerFrame);
    if (static_cast<quint64>(first) < frames) {
        std::memcpy(data + first * kBytesPerFrame, &m_ring[0],
                    static_cast<std::size_t>(frames - static_cast<quint64>(first)) * kBytesPerFrame);
    }
    m_read.store(read + frames, std::memory_order_release);
    return static_cast<qint64>(frames) * kBytesPerFrame;
}

void CaptureAudioBus::flush()
{
    const quint64 written = m_written.load(std::memory_order_acquire);
    quint64 floor = m_discardFloor.load(std::memory_order_relaxed);
    while (floor < written
           && !m_discardFloor.compare_exchange_weak(floor, written, std::memory_order_acq_rel,
                                                    std::memory_order_relaxed)) {
    }
}

float CaptureAudioBus::rxLevel() const
{
    return 0.0f;
}

float CaptureAudioBus::txLevel() const
{
    return m_level.load(std::memory_order_acquire);
}

QString CaptureAudioBus::backendName() const
{
    return QStringLiteral("Capture helper");
}

AudioFormat CaptureAudioBus::negotiatedFormat() const
{
    AudioFormat format;
    format.sampleRate = kSampleRate;
    format.channels = 1;
    format.sample = AudioFormat::Sample::Float32;
    return format;
}

int CaptureAudioBus::writeFrames(const float* samples, int frameCount)
{
    if (samples == nullptr || frameCount <= 0) {
        return 0;
    }
    float peak = 0.0f;
    for (int i = 0; i < frameCount; ++i) {
        peak = std::max(peak, std::fabs(samples[i]));
    }
    m_level.store(peak, std::memory_order_release);

    // Free space counts only frames the consumer has confirmed, never the
    // discard floor: a consumer may still be copying frames below it.
    const quint64 written = m_written.load(std::memory_order_relaxed);
    const quint64 read = m_read.load(std::memory_order_acquire);
    const quint64 used = written - std::min(read, written);
    const int space = kRingFrames - static_cast<int>(std::min<quint64>(used, kRingFrames));
    const int accepted = std::min(frameCount, space);
    if (accepted < frameCount) {
        m_dropped.fetch_add(static_cast<quint64>(frameCount - accepted), std::memory_order_relaxed);
    }
    if (accepted == 0) {
        return 0;
    }
    const int start = static_cast<int>(written % kRingFrames);
    const int first = std::min(accepted, kRingFrames - start);
    std::memcpy(&m_ring[static_cast<std::size_t>(start)], samples,
                static_cast<std::size_t>(first) * kBytesPerFrame);
    if (first < accepted) {
        std::memcpy(&m_ring[0], samples + first,
                    static_cast<std::size_t>(accepted - first) * kBytesPerFrame);
    }
    m_written.store(written + static_cast<quint64>(accepted), std::memory_order_release);
    return accepted;
}

void CaptureAudioBus::setAvailable(bool available)
{
    if (available) {
        m_available.store(true, std::memory_order_release);
        return;
    }
    m_available.store(false, std::memory_order_release);
    m_level.store(0.0f, std::memory_order_release);
    flush();
}

quint64 CaptureAudioBus::droppedFrames() const
{
    return m_dropped.load(std::memory_order_relaxed);
}

int CaptureAudioBus::bufferedFrames() const
{
    const quint64 written = m_written.load(std::memory_order_acquire);
    const quint64 read = std::max(m_read.load(std::memory_order_acquire),
                                  m_discardFloor.load(std::memory_order_acquire));
    return written > read ? static_cast<int>(written - read) : 0;
}

} // namespace NereusSDR
