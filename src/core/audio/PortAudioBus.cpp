// =================================================================
// src/core/audio/PortAudioBus.cpp  (NereusSDR)
// =================================================================
// See PortAudioBus.h for contract. NereusSDR-original.
// =================================================================

#include "PortAudioBus.h"

#include <portaudio.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace NereusSDR {

PortAudioBus::PortAudioBus() {
    // 1 second stereo float ring at the nominal default rate. The push
    // path wraps modulo ring size, so a longer stream than 1 s simply
    // overwrites the oldest unread samples (underruns surface as silence
    // in paCallback, not overruns).
    m_ring.resize(48000 * 2);
}

PortAudioBus::~PortAudioBus() {
    close();
}

void PortAudioBus::setConfig(const PortAudioConfig& cfg) {
    m_cfg = cfg;
}

bool PortAudioBus::open(const AudioFormat& format) {
    if (m_stream) {
        close();
    }

    PaStreamParameters outParams;
    outParams.device = Pa_GetDefaultOutputDevice();
    if (outParams.device == paNoDevice) {
        m_err = QStringLiteral("No default output device");
        return false;
    }

    const PaDeviceInfo* di = Pa_GetDeviceInfo(outParams.device);
    if (di == nullptr) {
        m_err = QStringLiteral("Pa_GetDeviceInfo returned null for default device");
        return false;
    }

    outParams.channelCount              = format.channels;
    outParams.sampleFormat              = paFloat32;
    outParams.suggestedLatency          = di->defaultLowOutputLatency;
    outParams.hostApiSpecificStreamInfo = nullptr;

    const PaError err = Pa_OpenStream(
        &m_stream, nullptr, &outParams,
        format.sampleRate, m_cfg.bufferSamples,
        paClipOff, &PortAudioBus::paCallback, this);

    if (err != paNoError) {
        m_err = QString::fromUtf8(Pa_GetErrorText(err));
        m_stream = nullptr;
        return false;
    }

    Pa_StartStream(m_stream);
    m_negFormat = format;

    // Defensive null-check on host-API lookup. With a device handed back
    // by Pa_GetDefaultOutputDevice this should never be null, but keep
    // the backend name well-defined if it ever is.
    const PaHostApiInfo* hai = Pa_GetHostApiInfo(di->hostApi);
    if (hai != nullptr && hai->name != nullptr) {
        m_backendName = QString::fromUtf8(hai->name);
    } else {
        m_backendName.clear();
    }
    return true;
}

void PortAudioBus::close() {
    if (!m_stream) {
        return;
    }
    Pa_StopStream(m_stream);
    Pa_CloseStream(m_stream);
    m_stream = nullptr;
}

qint64 PortAudioBus::push(const char* data, qint64 bytes) {
    const int floatCount = static_cast<int>(bytes / sizeof(float));
    const qint64 ringSize = static_cast<qint64>(m_ring.size());
    qint64 w = m_ringWrite.load(std::memory_order_relaxed);
    const float* in = reinterpret_cast<const float*>(data);
    float peak = 0.0f;
    for (int i = 0; i < floatCount; ++i) {
        m_ring[w % ringSize] = in[i];
        w++;
        peak = std::max(peak, std::abs(in[i]));
    }
    m_ringWrite.store(w, std::memory_order_release);
    m_rxLevel.store(peak, std::memory_order_release);
    return bytes;
}

qint64 PortAudioBus::pull(char* /*data*/, qint64 /*maxBytes*/) {
    // Not implemented in this task; TX path wired in Sub-Phase 4.
    return 0;
}

int PortAudioBus::paCallback(const void* /*in*/, void* out,
                             unsigned long frames,
                             const PaStreamCallbackTimeInfo* /*timeInfo*/,
                             unsigned long /*flags*/,
                             void* userData) {
    PortAudioBus* self = static_cast<PortAudioBus*>(userData);
    float* o = static_cast<float*>(out);
    const int want = static_cast<int>(frames) * self->m_negFormat.channels;

    const qint64 ringSize = static_cast<qint64>(self->m_ring.size());
    qint64 r = self->m_ringRead.load(std::memory_order_relaxed);
    const qint64 w = self->m_ringWrite.load(std::memory_order_acquire);

    for (int i = 0; i < want; ++i) {
        if (r < w) {
            o[i] = self->m_ring[r % ringSize];
            r++;
        } else {
            o[i] = 0.0f;  // underrun -> silence
        }
    }
    self->m_ringRead.store(r, std::memory_order_release);
    return paContinue;
}

} // namespace NereusSDR
