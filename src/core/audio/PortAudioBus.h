// =================================================================
// src/core/audio/PortAudioBus.h  (NereusSDR)
// =================================================================
//
// Phase 3O VAX cross-platform IAudioBus backend built on PortAudio
// v19.7.0. NereusSDR-original.
//
// Design spec: docs/architecture/2026-04-19-vax-design.md §3.2
// Plan:        docs/architecture/2026-04-19-phase3o-vax-plan.md (3.2)
//
// This task (3.2) implements render-only (output) minimal lifecycle
// against the PortAudio default device. Host-API / device enumeration
// (3.3) and TX capture via pull() (3.4) land in follow-up tasks.
// =================================================================

#pragma once

#include "core/IAudioBus.h"

#include <atomic>
#include <mutex>
#include <vector>

// Forward declarations so consumers of this header don't have to drag
// in <portaudio.h>. The concrete type is typedef'd the same way by
// PortAudio itself (`typedef void PaStream`).
typedef void PaStream;
struct PaDeviceInfo;
struct PaStreamCallbackTimeInfo;

namespace NereusSDR {

struct PortAudioConfig {
    int     hostApiIndex  = -1;     // -1 = PortAudio default
    QString deviceName;             // empty = default
    int     bufferSamples = 256;
    bool    exclusiveMode = false;  // WASAPI only
    // Additional fields added in Tasks 3.3 / 3.4.
};

class PortAudioBus : public IAudioBus {
public:
    PortAudioBus();
    ~PortAudioBus() override;

    void setConfig(const PortAudioConfig& cfg);

    bool open(const AudioFormat& format) override;
    void close() override;
    bool isOpen() const override { return m_stream != nullptr; }

    qint64 push(const char* data, qint64 bytes) override;
    qint64 pull(char* data, qint64 maxBytes) override;

    float rxLevel() const override { return m_rxLevel.load(std::memory_order_acquire); }
    float txLevel() const override { return m_txLevel.load(std::memory_order_acquire); }

    QString     backendName() const override { return m_backendName; }
    AudioFormat negotiatedFormat() const override { return m_negFormat; }
    QString     errorString() const override { return m_err; }

private:
    PaStream*       m_stream{nullptr};
    PortAudioConfig m_cfg;
    AudioFormat     m_negFormat;
    QString         m_backendName;
    QString         m_err;

    // Ring buffer for push/pull. The audio callback drains from here
    // on the audio thread; push() writes from the main/DSP thread.
    // Single-producer / single-consumer, lock-free via std::atomic.
    std::vector<float>  m_ring;
    std::atomic<qint64> m_ringRead{0};
    std::atomic<qint64> m_ringWrite{0};

    std::atomic<float> m_rxLevel{0.0f};
    std::atomic<float> m_txLevel{0.0f};

    static int paCallback(const void* in, void* out,
                          unsigned long frames,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          unsigned long flags,
                          void* userData);
};

} // namespace NereusSDR
