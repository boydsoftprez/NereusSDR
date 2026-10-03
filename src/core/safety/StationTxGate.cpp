// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/StationTxGate.cpp  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 34 (R-IOS-02). See StationTxGate.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 34 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "core/safety/StationTxGate.h"

#include "core/safety/TransmitHolder.h"

namespace NereusSDR {

TxDecision StationTxGate::decide(const SessionPeerInfo& peer) const
{
    // In the order of the header comment: the first that fails refuses.
    if (!m_allowed) {
        return {false, TxRefusals::stationReceiveOnly()};
    }
    if (!peer.declaresRemoteTx) {
        return {false, TxRefusals::appCannotTransmit()};
    }
    if (!peer.paired) {
        return {false, TxRefusals::deviceNotPaired()};
    }
    if (!peer.snapshotComplete) {
        return {false, TxRefusals::notReady()};
    }
    if (m_holder != nullptr) {
        // The key question, asked only: would this device's key be admitted
        // now (unheld, or its own)?
        const TxRefusal refusal = m_holder->keyRefusalFor(peer.deviceId);
        if (!refusal.isEmpty()) {
            return {false, refusal};
        }
    }
    return {true, {}};
}

} // namespace NereusSDR
