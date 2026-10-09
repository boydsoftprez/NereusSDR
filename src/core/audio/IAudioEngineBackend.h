// =================================================================
// src/core/audio/IAudioEngineBackend.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original interface for the native audio
// engines (R-AUD-03, R-AUD-07, R-AUD-32); no upstream logic.
//
// One backend per engine family (Core Audio, Windows audio, ASIO,
// PipeWire, PulseAudio, ALSA direct, and PortAudio for the older drivers).
// The device catalogue lists devices through enumerate() and
// defaultDeviceId() on its own thread and hears system notices through the
// notice sink; AudioEngine opens streams through createOutput() and
// createInput().  A test installs a fake backend (tests/fakes/
// FakeAudioEngineBackend.h); while audioDevicesBarredForTestRun() is true
// a real backend's streams refuse to open.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 3 (R-AUD-03, R-AUD-07, R-AUD-32).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-09: native audio plan early-review fix wave (R-AUD-08):
//               opensOneStreamAtATime().  J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/IAudioBus.h"
#include "core/audio/AudioDeviceTypes.h"

#include <QList>
#include <QString>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {

enum class AudioNotice { DevicesChanged, DefaultOutputChanged, DefaultInputChanged };

struct AudioStreamRequest {
    AudioDeviceDirection direction = AudioDeviceDirection::Output;
    QString deviceId;             // empty: the system default
    QString hostApi;              // older drivers only
    AudioChannelPair pair;
    int sampleRate = 48000;
    int bufferFrames = 0;         // 0: the engine's smallest
    int delayMs = 0;              // 0: automatic
    bool exclusive = false;       // Windows audio, exclusive
};

class IAudioInputSink {           // called on the input device's callback thread
public:
    virtual ~IAudioInputSink() = default;
    // Stereo float at the device rate; captureNsOfFrame0 on audioProbeNowNs()'s clock.
    virtual void onInput(const float* stereo, int frames, int sampleRate,
                         std::int64_t captureNsOfFrame0) = 0;
};

class IAudioInputStream {
public:
    virtual ~IAudioInputStream() = default;
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;
    virtual QString errorString() const = 0;
    virtual int sampleRate() const = 0;
    virtual std::optional<std::int64_t> inputLatencyNs() const = 0;
    // The sink may be called from a device thread; it only posts.
    virtual void setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink) = 0;
};

class IAudioEngineBackend {
public:
    virtual ~IAudioEngineBackend() = default;
    virtual AudioBackendId id() const = 0;
    virtual bool running() const = 0;                                   // its sound server answers
    virtual QList<AudioDeviceInfo> enumerate() = 0;                     // catalogue thread only
    virtual std::optional<QString> defaultDeviceId(AudioDeviceDirection direction) = 0; // catalogue thread only
    virtual void setNoticeSink(std::function<void(AudioNotice)> sink) = 0;    // the sink may be called from any thread
    virtual std::unique_ptr<IAudioBus> createOutput(const AudioStreamRequest& request) = 0;
    virtual std::unique_ptr<IAudioInputStream> createInput(const AudioStreamRequest& request,
                                                           MicChannelPick pick,
                                                           IAudioInputSink* sink) = 0;
    virtual bool hasControlPanel() const { return false; }
    virtual void openControlPanel(const QString& /*deviceId*/) {}
    virtual void rescan() {}
    // True when a second stream of this engine cannot open while one of
    // its streams is open (an ASIO driver is loaded once per process).
    // AudioEngine then closes a role's stream before opening its next one
    // on the same engine; otherwise the next opens first and replaces it.
    virtual bool opensOneStreamAtATime() const { return false; }
};

} // namespace NereusSDR
