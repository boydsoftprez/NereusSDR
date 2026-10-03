// no-port-check: NereusSDR-original ownership and accepted-value adapter.
// DSP implementation remains in pinned TAPR WDSP; no algorithm is duplicated.
// Modification history (NereusSDR):
//   2026-09-21 — J.J. Boyd (KG4VCF), with OpenAI Codex assistance.
//   2026-09-23 : requestLimit and the applied-limit readback (R-R3-40) by
//                J.J. Boyd (KG4VCF), with Anthropic Claude Code assistance.
#pragma once

#include "NnrSettings.h"
#include <optional>

namespace NereusSDR {

// Main/control-thread API. The native boundary holds the channel DSP lock.
// Channel lifetime is owned by WdspEngine; never retain internal NNR pointers.
class NnrAdapter final {
public:
    static std::optional<NnrSettings> readSettings(int channelId);
    static NnrDiagnostics diagnostics(int channelId);
    static std::optional<NnrSettings> apply(int channelId, const NnrSettings& requested,
                                            QString* reason = nullptr);
    static bool setRunning(int channelId, bool enabled, QString* reason = nullptr);
    static bool setDiagnostics(int channelId, int testMode, int outputMode,
                               QString* reason = nullptr);
    // R-R3-40: request a runtime limit (NnrLimit). Never takes the channel's
    // DSP lock and returns at once; the channel's worker applies it at its
    // next block. False only for a value outside NnrLimit.
    static bool requestLimit(int channelId, int limit);
};

} // namespace NereusSDR
