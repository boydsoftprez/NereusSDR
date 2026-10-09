// =================================================================
// src/core/audio/CoreAudioOutputBus.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  A Core Audio output on the clock
// matcher (R-AUD-07, R-AUD-11, R-AUD-15, R-AUD-18); no upstream logic.
// The AUHAL property order was studied in PortAudio's pa_mac_core.c and
// Apple's TN2091; nothing is copied from either.
//
// An AUHAL output unit (kAudioUnitSubType_HALOutput) plays on one device.
// Its client format is two-channel interleaved float32 at the device's
// nominal rate; kAudioOutputUnitProperty_ChannelMap, sized to the device's
// output channels, puts the pair on its two channels and nothing on the
// others.  push() takes 48 kHz stereo float into a DeviceRateMatcher whose
// reader the render callback calls; the callback does nothing else (a
// one-channel pair also folds left and right into one).  The device's
// alive, hog-mode and nominal-rate listeners post DeviceLost, DeviceBusy
// and FormatChanged.  While audioDevicesBarredForTestRun() is true open()
// fails (R-AUD-32).
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 8 (R-AUD-07, R-AUD-11, R-AUD-15,
//               R-AUD-18). J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
// =================================================================

#pragma once

#include "core/IAudioBus.h"
#include "core/audio/DeviceRateMatcher.h"
#include "core/audio/IAudioEngineBackend.h"

#include <QString>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace NereusSDR {

// The frames per device callback the bus asks for: the request's, or 128
// when it is 0, clamped to kAudioDevicePropertyBufferFrameSizeRange.
inline constexpr int kCoreAudioDefaultBufferFrames = 128;
int coreAudioBufferFrames(int requestedFrames, double rangeMinimum, double rangeMaximum);

// kAudioOutputUnitProperty_ChannelMap for an output: deviceChannels
// entries, the pair's first channel 0 and its second 1, every other -1.
// A one-channel pair maps its channel to 0.  Empty when the pair is not on
// the device.
std::vector<std::int32_t> coreAudioOutputChannelMap(int deviceChannels, AudioChannelPair pair);

// The same property for an input: two entries (the client's channels),
// the device channels (0-based) of the pair; a one-channel pair feeds
// both.  Empty when the pair is not on the device.
std::vector<std::int32_t> coreAudioInputChannelMap(int deviceChannels, AudioChannelPair pair);

// The output's clock matcher: 48 kHz in, the device's rate out, 64-frame
// writes, the device callback's frames and the request's delay.
DeviceRateMatcher::Config coreAudioMatcherConfig(int deviceRate, int bufferFrames, int delayMs);

// Latency frames at a rate, in nanoseconds (0 for a rate of 0 or less).
std::int64_t coreAudioFramesToNs(std::int64_t frames, double sampleRate);

// The open error every Core Audio stream gives in a test run.
QString coreAudioTestRunError();

class CoreAudioOutputBus final : public IAudioBus {
public:
    CoreAudioOutputBus(std::uint32_t deviceObjectId, int deviceOutputChannels,
                       AudioStreamRequest request);
    ~CoreAudioOutputBus() override;

    CoreAudioOutputBus(const CoreAudioOutputBus&) = delete;
    CoreAudioOutputBus& operator=(const CoreAudioOutputBus&) = delete;

    bool open(const AudioFormat& format) override;
    void close() override;
    bool isOpen() const override;

    qint64 push(const char* data, qint64 bytes) override;
    qint64 pull(char* data, qint64 maxBytes) override;
    void flush() override;

    std::optional<OutputPacing> outputPacing() const override;
    float rxLevel() const override { return 0.0f; }
    float txLevel() const override { return 0.0f; }
    QString backendName() const override;
    AudioFormat negotiatedFormat() const override;
    QString errorString() const override;

    void setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink) override;
    AudioDelayParts delayParts() const override;
    bool takesStereoMix() const override { return true; }
    std::optional<DeviceRateMatcherStats> matcherStats() const override;
    void restartClockMatch() override;
    void requestFadeOut() override;
    bool fadedOut() const override;
    std::uint32_t audioWorkgroupDevice() const override { return m_deviceObjectId; }

    int deviceRate() const;
    int bufferFrames() const;

private:
    struct Impl;

    std::uint32_t m_deviceObjectId;
    std::unique_ptr<Impl> d;
};

} // namespace NereusSDR
