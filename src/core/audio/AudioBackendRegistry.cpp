// =================================================================
// src/core/audio/AudioBackendRegistry.cpp  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  See AudioBackendRegistry.h.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 7 (R-AUD-01, R-AUD-02). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/audio/AudioBackendRegistry.h"

#include "core/audio/PortAudioBackend.h"

#include <optional>

namespace NereusSDR {

namespace {

// R-AUD-02: the engine a native backend stands for as the default.  ASIO
// is never a default, and PortAudio is the fallback, not a native choice.
std::optional<AudioEngineKind> nativeDefaultFor(AudioBackendId id)
{
    switch (id) {
    case AudioBackendId::CoreAudio:
        return AudioEngineKind::CoreAudio;
    case AudioBackendId::Wasapi:
        return AudioEngineKind::WindowsShared;
    case AudioBackendId::PipeWire:
        return AudioEngineKind::PipeWire;
    case AudioBackendId::PulseAudio:
        return AudioEngineKind::PulseAudio;
    case AudioBackendId::AlsaDirect:
        return AudioEngineKind::AlsaDirect;
    case AudioBackendId::PortAudio:
    case AudioBackendId::Asio:
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace

std::vector<std::shared_ptr<IAudioEngineBackend>> makeSystemAudioBackends(const AudioBackendContext& context)
{
    // The native engines join this list in their own tasks, ahead of the
    // older drivers, and read the context then (the Core and the mic
    // helper register different engines).
    static_cast<void>(context);

    std::vector<std::shared_ptr<IAudioEngineBackend>> backends;
    // Until the Mac's and Windows' native engines land, the older drivers
    // also list the host APIs those engines replace, so nothing goes
    // silent meanwhile.
    constexpr bool kIncludeReplacedHostApis = true;
    backends.push_back(std::make_shared<PortAudioBackend>(&listPortAudioDevices,
                                                          currentOlderDriverPlatform(),
                                                          kIncludeReplacedHostApis));
    return backends;
}

AudioEngineKind defaultAudioEngine(const std::vector<std::shared_ptr<IAudioEngineBackend>>& backends)
{
    for (const std::shared_ptr<IAudioEngineBackend>& backend : backends) {
        if (!backend) {
            continue;
        }
        const std::optional<AudioEngineKind> kind = nativeDefaultFor(backend->id());
        if (kind && backend->running()) {
            return *kind;
        }
    }
    return AudioEngineKind::PortAudio;
}

} // namespace NereusSDR
