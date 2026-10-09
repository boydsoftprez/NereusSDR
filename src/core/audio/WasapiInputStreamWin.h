// =================================================================
// src/core/audio/WasapiInputStreamWin.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Windows only (built inside
// if(WIN32) in CMakeLists.txt).  One Windows audio input, shared or
// exclusive (R-AUD-02, R-AUD-11, R-AUD-16), for the mic helper.
//
// The stream runs on its own thread in the Pro Audio MMCSS class,
// event-driven: at each buffer event it reads every packet, makes stereo
// float from the chosen pair by the mic pick (readDeviceToStereo) and
// hands it to the sink.  That loop takes no lock, allocates nothing and
// makes no call but the stream's own buffer calls and the event wait.  A
// failed buffer call stops the stream and posts wasapiRunningStreamEvent.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 9 (R-AUD-02, R-AUD-11, R-AUD-16).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/IAudioEngineBackend.h"
#include "core/audio/WasapiPolicy.h"

#include <functional>
#include <memory>

namespace NereusSDR {

class WasapiInputStreamWin final : public IAudioInputStream {
public:
    WasapiInputStreamWin(AudioStreamRequest request, MicChannelPick pick, IAudioInputSink* sink);
    ~WasapiInputStreamWin() override;

    bool open() override;
    void close() override;
    bool isOpen() const override;
    QString errorString() const override;
    int sampleRate() const override;
    std::optional<std::int64_t> inputLatencyNs() const override;
    void setStreamEventSink(std::function<void(const AudioStreamEvent&)> sink) override;

    // The last open's result, as wasapiOpenResult reads it.
    WasapiResult lastOpenResult() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace NereusSDR
