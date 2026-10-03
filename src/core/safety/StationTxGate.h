// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/StationTxGate.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 34 (remote design section 12.2; spec section 4.6
// items 4, 5 and 7; D51, D56): whether a session may transmit, which is
// the capability txPermitted each session is sent.
//
// A session is permitted only when all of these hold:
//   1. the Core's remote_transmit is allow (else stationReceiveOnly);
//   2. its hello declared the feature remoteTx (else notReady, "Update
//      this app"): an older desktop that reads the flag without
//      understanding it never sees it true;
//   3. it signed in with a paired device's key (else notReady, "Pair
//      this device");
//   4. its snapshot is complete (else notReady);
//   5. transmit is unheld or its device holds it (else the holder's
//      refusal: otherDeviceHolds, changingHands or stopNotConfirmed).
// The first that fails is the refusal decide() returns.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-25: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), iPhone app plan Task 34 (R-IOS-02), with
//               AI-assisted implementation via Anthropic Claude Code.
// =================================================================
#pragma once

#include <QByteArray>

#include "core/safety/TxRefusal.h"

namespace NereusSDR {

class TransmitHolder;

/// What the gate reads about one session.
struct SessionPeerInfo {
    /// The device the session is for (the session registry's id); empty
    /// before it is admitted.
    QByteArray deviceId;
    /// Its hello declared remoteTx 1 or later.
    bool declaresRemoteTx{false};
    /// It signed in with a paired device's key.
    bool paired{false};
    /// snapshot.complete has been sent to it.
    bool snapshotComplete{false};
};

struct TxDecision {
    bool permitted{false};
    /// Why not; empty when permitted.
    TxRefusal refusal;
};

class StationTxGate {
public:
    /// The config key remote_transmit (allow or deny).
    void setRemoteTransmitAllowed(bool allowed) { m_allowed = allowed; }
    bool remoteTransmitAllowed() const { return m_allowed; }

    /// Not owned; may be null (then transmit reads as unheld).
    void setTransmitHolder(const TransmitHolder* holder) { m_holder = holder; }

    TxDecision decide(const SessionPeerInfo& peer) const;

private:
    bool m_allowed{false};
    const TransmitHolder* m_holder{nullptr};
};

} // namespace NereusSDR
