// =================================================================
// src/core/audio/WasapiOutputBusWin.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Windows only (built inside
// if(WIN32) in CMakeLists.txt).  One Windows audio output, shared or
// exclusive (R-AUD-02, R-AUD-11, R-AUD-15, R-AUD-16).
//
// push() takes 48 kHz stereo float into a DeviceRateMatcher
// (takesStereoMix).  The stream runs on its own thread in the Pro Audio
// MMCSS class, event-driven: at each buffer event it reads the matcher
// into stereo float and writes the device's format on the chosen pair
// (writeStereoToDevice).  That loop takes no lock, allocates nothing and
// makes no call but the stream's own buffer calls and the event wait.  A
// failed buffer call stops the stream and posts wasapiRunningStreamEvent.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 9 (R-AUD-02, R-AUD-11, R-AUD-16).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/IAudioBus.h"
#include "core/audio/IAudioEngineBackend.h"
#include "core/audio/WasapiPolicy.h"

#include <QString>

#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {

class WasapiOutputBusWin final : public IAudioBus {
public:
    explicit WasapiOutputBusWin(AudioStreamRequest request);
    ~WasapiOutputBusWin() override;

    bool open(const AudioFormat& format) override;
    void close() override;
    bool isOpen() const override;

    qint64 push(const char* data, qint64 bytes) override;
    qint64 pull(char* data, qint64 maxBytes) override;
    void flush() override;
    std::optional<OutputPacing> outputPacing() const override;

    float rxLevel() const override;
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

    // The last open's result, as wasapiOpenResult reads it.
    WasapiResult lastOpenResult() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace NereusSDR
