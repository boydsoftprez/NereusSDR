// =================================================================
// src/core/audio/AudioBackendRegistry.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original.  Which engine backends this system
// and process register, in R-AUD-01 order, and R-AUD-02's default engine;
// no upstream logic.
//
// Modification history (NereusSDR):
//   2026-10-09: native audio plan Task 7 (R-AUD-01, R-AUD-02). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#pragma once

#include "core/audio/AudioDeviceTypes.h"
#include "core/audio/IAudioEngineBackend.h"

#include <memory>
#include <vector>

namespace NereusSDR {

struct AudioBackendContext {
    bool daemon = false;     // nereusd
    bool helper = false;     // the mic helper process
};

// In R-AUD-01 order for this system and process.  Engine tasks add theirs here.
std::vector<std::shared_ptr<IAudioEngineBackend>> makeSystemAudioBackends(const AudioBackendContext& context);

// R-AUD-02: the first native engine whose backend is registered and running, else PortAudio.
AudioEngineKind defaultAudioEngine(const std::vector<std::shared_ptr<IAudioEngineBackend>>& backends);

} // namespace NereusSDR
