// no-port-check: NereusSDR-original remote CW safety contract.
// =================================================================
// src/core/safety/RemoteCwSafetyState.h (NereusSDR)
// =================================================================
//
// One lock-free atomic word owns the activity epoch and its safety state.
// Every mutation compares the captured epoch in the same word as the mask;
// a delayed callback cannot mutate an activity that replaced its own.
// No production caller is wired to this class yet.
//
// Modification history (NereusSDR):
//   2026-10-27: Implement the packed atomic safety value after observed RED.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-27: Declare the approved pure safety-value interface for
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// =================================================================

#pragma once

#include <QtGlobal>

#include <atomic>

#include "core/NereusCoreExport.h"
#include "core/TxChannel.h"

namespace NereusSDR {

struct RemoteCwSafetySnapshot {
    quint32 epoch{0};
    quint32 holdMask{0};
    bool valid{false};
    bool invalidated{false};
};

class NEREUS_CORE_EXPORT RemoteCwSafetyState {
public:
    RemoteCwSafetyState() = default;
    RemoteCwSafetyState(const RemoteCwSafetyState&) = delete;
    RemoteCwSafetyState& operator=(const RemoteCwSafetyState&) = delete;

    // Main-thread admission, called only after previous intent is cleared.
    // Epochs are caller-owned and unique. Only an inactive value admits a
    // nonzero epoch; finish the prior activity after clearing its intent.
    void begin(quint32 epoch);
    RemoteCwSafetySnapshot snapshot() const noexcept;
    bool setHold(quint32 epoch, TxChannel::RfHold reason) noexcept;
    bool clearHold(quint32 epoch, TxChannel::RfHold reason) noexcept;
    bool invalidate(quint32 epoch) noexcept;
    // Caller supplies a newly observed physical edge, not an already-down
    // level, USB input, mic PTT or combined radio PTT. The atomic
    // operation accepts only a matching valid epoch with Link/Guard held.
    bool invalidatePhysicalPress(quint32 observedEpoch) noexcept;
    // Caller must clear matching remote intent before finishing its epoch.
    void finish(quint32 epoch) noexcept;
    bool suppressesCw() const noexcept;

private:
    static_assert(std::atomic<quint64>::is_always_lock_free,
                  "Remote CW safety requires lock-free 64-bit atomics");
    std::atomic<quint64> m_state{0};
};

} // namespace NereusSDR
