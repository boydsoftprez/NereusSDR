// no-port-check: NereusSDR-original remote CW safety contract.
// =================================================================
// src/core/safety/RemoteCwSafetyState.h (NereusSDR)
// =================================================================
//
// SOURCE-ONLY RED SCAFFOLD. No production caller is wired to this class.
// The methods are intentionally inert and suppressesCw always fails closed.
// Epoch/mask CAS behavior belongs to the implementation after observed RED.
//
// Modification history (NereusSDR):
//   2026-10-27: Declare the approved pure safety-value interface for
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// =================================================================

#pragma once

#include <QtGlobal>

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
    // Zero never creates an activity. RED scaffold does not admit any epoch.
    void begin(quint32 epoch);
    RemoteCwSafetySnapshot snapshot() const noexcept;
    bool setHold(quint32 epoch, TxChannel::RfHold reason) noexcept;
    bool clearHold(quint32 epoch, TxChannel::RfHold reason) noexcept;
    bool invalidate(quint32 epoch) noexcept;
    // Caller supplies a newly observed physical edge, not an already-down
    // level, USB input, mic PTT or combined radio PTT. The eventual atomic
    // operation accepts only a matching valid epoch with Link/Guard held.
    bool invalidatePhysicalPress(quint32 observedEpoch) noexcept;
    // Caller must clear matching remote intent before finishing its epoch.
    void finish(quint32 epoch) noexcept;
    bool suppressesCw() const noexcept;
};

} // namespace NereusSDR
