// =================================================================
// src/core/IAudioBus.h  (NereusSDR)
// =================================================================
//
// Phase 3O abstract audio bus. Concrete implementations live in
// src/core/audio/: CoreAudioHalBus (macOS), LinuxPipeBus (Linux),
// PortAudioBus (Windows + Mac/Linux fallback).
//
// Design spec: docs/architecture/2026-04-19-vax-design.md §3.2
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 7 (R-AUD-06): requestFadeOut() and
//               fadedOut() for Rescan. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-09: native audio plan Task 8 (R-AUD-18): audioWorkgroupDevice().
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/AudioDelayParts.h"

#include <QString>

#include <cstdint>
#include <functional>
#include <optional>

namespace NereusSDR {

struct AudioFormat {
    int sampleRate = 48000;  // Hz
    int channels   = 2;       // 1 or 2
    enum class Sample { Float32, Int16, Int24, Int32 } sample = Sample::Float32;

    bool operator==(const AudioFormat& o) const {
        return sampleRate == o.sampleRate && channels == o.channels && sample == o.sample;
    }
    bool operator!=(const AudioFormat& o) const { return !(*this == o); }
};

// A stream event a backend reports (native audio plan, R-AUD-03): the
// device went away, another program holds it, its format changed, or the
// system asks for the stream to be rebuilt.  Posted to the main thread.
struct AudioStreamEvent {
    enum class Kind { DeviceLost, DeviceBusy, FormatChanged, ResetRequested };
    Kind kind = Kind::DeviceLost;
    QString detail;
};

class IAudioBus {
public:
    struct OutputPacing {
        quint64 consumedFrames = 0;
        int queuedFrames = 0;
        int capacityFrames = 0;
        int callbackFrames = 0; // largest/configured output callback quantum
        // R-R3-35: how long audio the device callback has taken still takes
        // to be heard, as the backend reports it. Absent when the backend
        // does not know it.
        std::optional<qint64> deviceLatencyNs;
    };

    virtual ~IAudioBus() = default;

    // Lifecycle. open() returns false on failure; errorString() has details.
    virtual bool open(const AudioFormat& format) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    // Producer side (RX taps). Interleaved PCM bytes. Returns bytes actually
    // written, or -1 on error. Must be callable from the audio thread.
    virtual qint64 push(const char* data, qint64 bytes) = 0;

    // Consumer side (TX). Returns bytes read, or -1 on error. Audio-thread safe.
    virtual qint64 pull(char* data, qint64 maxBytes) = 0;

    // Drop any samples queued in the bus's internal buffer that have been
    // pushed but not yet consumed (or pulled but not yet read).  Used by
    // AudioEngine::setMasterMuted to stop already-buffered pre-mute audio
    // from draining out the speakers device after the mute click — see
    // issue #201.  Default no-op for buses without an internal ring (HAL
    // shm, PipeWire, FIFO). PortAudioBus uses a monotonic output discard
    // floor so an in-flight device callback cannot republish stale reads.
    // Safe to call from any thread.
    virtual void flush() {}

    // Output-device pacing observation. A receiver worker uses this only to
    // replenish a physical-output queue after real callback consumption; it
    // must not estimate device time from its own timer cadence. Unsupported
    // buses return nullopt.
    virtual std::optional<OutputPacing> outputPacing() const { return std::nullopt; }

    // R-R3-44: whether an app is reading this output right now, where the
    // platform reports it (a VAX output on macOS or PipeWire). nullopt when
    // the backend cannot tell, which callers treat as "maybe": a stream is
    // then kept while the output is assigned. Owner (GUI) thread; may ask
    // the platform, so never call it from an audio callback.
    virtual std::optional<bool> outputHasReader() const { return std::nullopt; }

    // Metering (RMS of last block). 0.0–1.0. Published atomically for UI.
    virtual float rxLevel() const = 0;
    virtual float txLevel() const = 0;

    // Diagnostics.
    virtual QString backendName() const = 0;
    virtual AudioFormat negotiatedFormat() const = 0;
    virtual QString errorString() const { return {}; }

    // Native audio engines (R-AUD-03, R-AUD-15).  Every default keeps an
    // existing bus as it is.
    //
    // The sink may be called from a device thread; it only posts.
    virtual void setStreamEventSink(std::function<void(const AudioStreamEvent&)> /*sink*/) {}
    // The delay readout's parts; matcherFillMs -1 when the bus has no
    // clock matcher.
    virtual AudioDelayParts delayParts() const { return {}; }
    // True: push() takes 48 kHz stereo float into a DeviceRateMatcher.
    virtual bool takesStereoMix() const { return false; }
    virtual std::optional<DeviceRateMatcherStats> matcherStats() const { return std::nullopt; }
    virtual void restartClockMatch() {}
    // R-AUD-06: the output slews to silence at its next read and stays
    // silent; fadedOut() is true once it is (or when the bus has nothing
    // to fade).  Any thread.
    virtual void requestFadeOut() {}
    virtual bool fadedOut() const { return true; }
    // R-AUD-18: the device whose audio workgroup the DSP thread joins
    // while this bus plays the speakers: its AudioObjectID on Core Audio,
    // 0 elsewhere.
    virtual std::uint32_t audioWorkgroupDevice() const { return 0; }
};

} // namespace NereusSDR
