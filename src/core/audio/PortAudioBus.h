// =================================================================
// src/core/audio/PortAudioBus.h  (NereusSDR)
// =================================================================
//
// Phase 3O VAX cross-platform IAudioBus backend built on PortAudio
// v19.7.0. NereusSDR-original.
//
// Design spec: docs/architecture/2026-04-19-vax-design.md §3.2
// Plan:        docs/architecture/2026-04-19-phase3o-vax-plan.md (3.2–3.4)
//
// Supports both render (Output) and capture (Input) modes via
// PortAudioConfig::direction. Output: push() feeds the ring, the audio
// callback drains it to the device. Input: the audio callback captures
// from the device into the ring, pull() drains it. Host-API / device
// enumeration helpers are available statically (Task 3.3).
// =================================================================

#pragma once

#include "core/IAudioBus.h"

#include <atomic>
#include <vector>

#include <QVector>

// Forward declarations so consumers of this header don't have to drag
// in <portaudio.h>. The concrete type is typedef'd the same way by
// PortAudio itself (`typedef void PaStream`).
typedef void PaStream;
struct PaDeviceInfo;
struct PaStreamCallbackTimeInfo;

namespace NereusSDR {

enum class AudioDirection { Output, Input };

struct PortAudioConfig {
    AudioDirection direction = AudioDirection::Output;  // Output = render; Input = capture
    int     hostApiIndex  = -1;     // -1 = PortAudio default
    QString deviceName;             // empty = default
    // 128 frames @ 48 kHz = 2.67 ms per callback. On macOS this maps
    // to a CoreAudio HAL output latency of ~10-12 ms (CoreAudio
    // queues ~4 buffers).  Was 256 (22 ms PA latency); reduced to
    // shave ~11 ms off the end-to-end RX path now that the upstream
    // jitter (main-thread I/Q dispatch) has been fixed by Lever 2.
    // 128 is still well above any sensible Apple Silicon minimum and
    // doubles callback rate vs. 256, but Apple's audio I/O thread is
    // RT-prio and trivially handles this.  If a slower platform
    // shows stress here, the call site can override via setConfig().
    int     bufferSamples = 128;
    bool    exclusiveMode = false;  // WASAPI only
};

class PortAudioBus : public IAudioBus {
public:
    PortAudioBus();
    ~PortAudioBus() override;

    // Call before open(). m_cfg is read on the main thread in open() only.
    void setConfig(const PortAudioConfig& cfg);

    struct HostApiInfo {
        int     index;
        QString name;
    };
    struct DeviceInfo {
        int     index;
        QString name;
        int     maxOutputChannels;
        int     maxInputChannels;
        int     defaultSampleRate;
        int     hostApiIndex;
    };

    // Enumeration helpers require Pa_Initialize() to have been called
    // (owned by the application lifecycle, not this class).
    static QVector<HostApiInfo> hostApis();
    static QVector<DeviceInfo>  outputDevicesFor(int hostApiIndex);
    static QVector<DeviceInfo>  inputDevicesFor(int hostApiIndex);

    bool open(const AudioFormat& format) override;
    void close() override;
    bool isOpen() const override { return m_stream != nullptr; }

    qint64 push(const char* data, qint64 bytes) override;
    qint64 pull(char* data, qint64 maxBytes) override;
    void   flush() override;

    float rxLevel() const override { return m_rxLevel.load(std::memory_order_acquire); }
    float txLevel() const override { return m_txLevel.load(std::memory_order_acquire); }

    QString     backendName() const override { return m_backendName; }
    AudioFormat negotiatedFormat() const override { return m_negFormat; }
    QString     errorString() const override { return m_err; }

    // Diagnostics: drop-oldest overrun accounting.  Output-mode push()
    // counts the event + samples lost every time the writer outruns the
    // reader by more than the ring's size.  paCallback then performs the
    // catch-up jump on its next entry.  Reset by clearDropStats().  Both
    // counters are loaded with relaxed semantics; they are observational
    // only and not safety-critical.
    quint32 ringOverrunEvents() const {
        return m_dropEvents.load(std::memory_order_relaxed);
    }
    quint64 ringOverrunSamples() const {
        return m_dropSamples.load(std::memory_order_relaxed);
    }
    quint32 ringUnderrunEvents() const {
        return m_underrunEvents.load(std::memory_order_relaxed);
    }
    void clearDropStats() {
        m_dropEvents.store(0, std::memory_order_relaxed);
        m_dropSamples.store(0, std::memory_order_relaxed);
        m_underrunEvents.store(0, std::memory_order_relaxed);
    }

private:
    PaStream*       m_stream{nullptr};
    PortAudioConfig m_cfg;
    AudioFormat     m_negFormat;
    QString         m_backendName;
    QString         m_err;

    // Ring buffer for push/pull. SPSC, lock-free via std::atomic.
    // Output mode: push() (DSP thread) writes, audio callback reads.
    // Input mode:  audio callback writes, pull() (caller thread) reads.
    std::vector<float>  m_ring;
    std::atomic<qint64> m_ringRead{0};
    std::atomic<qint64> m_ringWrite{0};

    std::atomic<float> m_rxLevel{0.0f};
    std::atomic<float> m_txLevel{0.0f};

    // Drop-oldest accounting (see ringOverrunEvents above).  Producer-only
    // writers (push() on the DSP thread); observers read.
    std::atomic<quint32> m_dropEvents{0};
    std::atomic<quint64> m_dropSamples{0};

    // Underrun accounting: paCallback increments on the leading edge of
    // every silence run (ring empty when audio device asked for samples).
    // Counts distinct events, not silent frames.  Bench diagnostic for
    // the audio jitter / crackle hunt.
    std::atomic<quint32> m_underrunEvents{0};

    // PortAudio backend-reported anomalies via the callback's flags
    // parameter.  paOutputUnderflow = the OS audio device ran out of
    // samples; paOutputOverflow = data we supplied was discarded.
    // Distinct from m_underrunEvents (our ring-side check) — these
    // come from the OS/CoreAudio layer regardless of what our ring
    // looks like.
    std::atomic<quint32> m_paOutputUnderflowEvents{0};
    std::atomic<quint32> m_paOutputOverflowEvents{0};

    // Crossfade state for discontinuity smoothing in paCallback.  Only
    // read / written from the audio callback (single-threaded by
    // PortAudio contract), so plain non-atomic floats are safe.
    // m_lastOutL / m_lastOutR hold the last sample emitted to each
    // channel; the crossfade ramps from those values up to the next
    // ring sample over kCrossfadeFrames stereo frames whenever a
    // drop-oldest catch-up OR an underrun-to-resume transition is
    // detected.
    static constexpr int kCrossfadeFrames = 128;  // ~2.7 ms at 48 kHz
    float m_lastOutL{0.0f};
    float m_lastOutR{0.0f};
    int   m_crossfadeFramesRem{0};

    static int paCallback(const void* in, void* out,
                          unsigned long frames,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          unsigned long flags,
                          void* userData);
};

} // namespace NereusSDR
