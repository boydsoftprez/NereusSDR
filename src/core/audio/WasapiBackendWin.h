// =================================================================
// src/core/audio/WasapiBackendWin.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Windows only (built inside
// if(WIN32) in CMakeLists.txt).  Windows audio, shared and exclusive, as
// one engine backend (R-AUD-01, R-AUD-02, R-AUD-03, R-AUD-16, D8, D12):
// the active endpoints, the console defaults, the endpoint notices, and
// outputs and inputs that honour AudioStreamRequest::exclusive.  Its
// decisions are WasapiPolicy's.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 9 (R-AUD-01, R-AUD-02, R-AUD-03,
//               R-AUD-11, R-AUD-16). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/IAudioEngineBackend.h"

#include <atomic>
#include <memory>
#include <mutex>

namespace NereusSDR {

class WasapiEndpointNotifier;

class WasapiBackendWin final : public IAudioEngineBackend {
public:
    WasapiBackendWin();
    ~WasapiBackendWin() override;

    AudioBackendId id() const override { return AudioBackendId::Wasapi; }
    bool running() const override;
    QList<AudioDeviceInfo> enumerate() override;
    std::optional<QString> defaultDeviceId(AudioDeviceDirection direction) override;
    void setNoticeSink(std::function<void(AudioNotice)> sink) override;
    std::unique_ptr<IAudioBus> createOutput(const AudioStreamRequest& request) override;
    std::unique_ptr<IAudioInputStream> createInput(const AudioStreamRequest& request,
                                                   MicChannelPick pick,
                                                   IAudioInputSink* sink) override;

private:
    std::atomic<bool> m_running{true};
    std::mutex m_notifierMutex;
    std::unique_ptr<WasapiEndpointNotifier> m_notifier;   // registered at the first sink
};

} // namespace NereusSDR
