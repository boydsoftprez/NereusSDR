// =================================================================
// src/core/audio/CoreAudioInputStream.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  A Core Audio input for the mic
// helper (R-AUD-07, R-AUD-11, R-AUD-14, R-AUD-17); no upstream logic.
// The AUHAL property order was studied in PortAudio's pa_mac_core.c and
// Apple's TN2091; nothing is copied from either.
//
// An AUHAL unit with input on and output off.  The input channel map
// picks the pair, the client format is two-channel float32 at the
// device's nominal rate, and the render buffer is allocated at open.  The
// input callback calls AudioUnitRender, applies the mic pick as
// readDeviceToStereo does and calls IAudioInputSink::onInput with the
// capture time of the block's first frame on audioProbeNowNs()'s clock:
// AudioConvertHostTimeToNanos(mHostTime), plus the offset between the two
// clocks measured once at open, less the input latency.  The device's
// alive, hog-mode and nominal-rate listeners post DeviceLost, DeviceBusy
// and FormatChanged.  While audioDevicesBarredForTestRun() is true open()
// fails (R-AUD-32).
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 8 (R-AUD-07, R-AUD-11, R-AUD-14).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/IAudioEngineBackend.h"

#include <QString>
#include <QtGlobal>

#include <cstdint>
#include <memory>
#include <optional>

namespace NereusSDR {

#ifdef Q_OS_MAC
// The capture clock (R-AUD-07): audioProbeNowNs() minus
// AudioConvertHostTimeToNanos(AudioGetCurrentHostTime()), read once.  The
// stream measures it at open and adds it to every block's host time.
std::int64_t coreAudioHostClockOffsetNs();

// A host time (mach host ticks) on audioProbeNowNs()'s clock, given the
// offset above.  Callable from the input callback.
std::int64_t coreAudioHostTimeToProbeNs(std::uint64_t hostTime, std::int64_t offsetNs);
#endif

class CoreAudioInputStream final : public IAudioInputStream {
public:
    CoreAudioInputStream(std::uint32_t deviceObjectId, int deviceInputChannels,
                         AudioStreamRequest request, MicChannelPick pick, IAudioInputSink* sink);
    ~CoreAudioInputStream() override;

    CoreAudioInputStream(const CoreAudioInputStream&) = delete;
    CoreAudioInputStream& operator=(const CoreAudioInputStream&) = delete;

    bool open() override;
    void close() override;
    bool isOpen() const override;
    QString errorString() const override;
    int sampleRate() const override;
    std::optional<std::int64_t> inputLatencyNs() const override;
    void setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink) override;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace NereusSDR
