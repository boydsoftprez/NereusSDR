// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/TransmitCutLine.h  (NereusSDR)
// =================================================================
//
// The line a transmit cut travels from a thread other than the main thread
// to the radio: the TX channel's RF gate, then the radio connection's cut
// latch. RemoteTxCutGuard holds it (through a shared_ptr RadioModel owns)
// and calls cut() on its own thread when the Core's main thread has
// stalled with a remote device keyed.
//
// ---- The rules ----
// - setTargets(connection, tx) on the main thread, wherever RadioModel sets,
//   injects or clears either object, and before either can be destroyed.
// - cut() from any thread: tx->closeRfGate() if a TX channel is set, then
//   connection->cutTransmit() if a connection is set. Neither waits for the
//   main thread or the connection thread; closeRfGate waits only for a TX
//   I/Q block already inside sendTxIq (TxChannel's documented short spin).
// - hold(held) from any thread (2026-10-26): sets or clears only the TX
//   channel's RfHold::Guard bit, if a TX channel is set; it never closes
//   the gate or calls cutTransmit. On waits only for a TX I/Q block already
//   inside sendTxIq (TxChannel::setRfHold's short spin); off never waits.
// - One mutex guards both targets and is held across cut() and hold(), so
//   once setTargets returns, neither can reach an object it replaced.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-27: Propose unused isolated CW bridge RED bootstrap for
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-15: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), remote transmit cut off the main thread, with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut: hold(held) sets the TX
//               channel's Guard hold bit. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================
#pragma once

#include <mutex>
#include <memory>
#include <functional>
#include <QtGlobal>

#include "core/NereusCoreExport.h"

namespace NereusSDR {

class RadioConnection;
class CwxSender;
class RemoteCwSafetyState;
#ifdef NEREUS_BUILD_TESTS
struct TransmitCutLineTestAccess;
#endif
class TxChannel;

class NEREUS_CORE_EXPORT TransmitCutLine {
public:
    TransmitCutLine() = default;
    TransmitCutLine(const TransmitCutLine&) = delete;
    TransmitCutLine& operator=(const TransmitCutLine&) = delete;

    // Main thread. Either may be null.
    void setTargets(RadioConnection* connection, TxChannel* tx);
    // Any thread. Non-blocking apart from closeRfGate's short spin.
    void cut();
    // Any thread. The TX channel's Guard hold bit only; never cutTransmit.
    // Non-blocking apart from setRfHold's short spin.
    void hold(bool held);
    // UNUSED isolated bridge-RED bootstrap; no RadioModel producer wiring.
    // Shared state must already be installed in sender OUTSIDE this lock.
    void setRemoteCwTargets(CwxSender* sender, std::function<void(bool)> sidetoneHold,
                            std::function<void(quint32)> recoveryRequested);
    bool beginRemoteCw(quint32 epoch);
    void hold(quint32 expectedCwEpoch, bool held);
    void cut(quint32 expectedCwEpoch);
#ifdef NEREUS_BUILD_TESTS
    enum class RemoteCwLockPointForTest { BeginContended, BeginAcquired, TargetsContended, TargetsAcquired };
    using RemoteCwLockObserverForTest = std::function<void(RemoteCwLockPointForTest)>;
    // Immutable after setup. Records the ACTUAL same-mutex failed try_lock
    // and subsequent successful blocking acquisition; no production hooks.
    void setRemoteCwLockObserverForTest(RemoteCwLockObserverForTest observer);
    using RemoteCwCutValidatedObserverForTest = std::function<void(quint32)>;
    // Install before reachability; callback may block TEST cut only, at
    // actual invalidate-success BEFORE connection cut latch, under m_mutex.
    void setRemoteCwCutValidatedObserverForTest(RemoteCwCutValidatedObserverForTest observer);
#endif

private:
    std::mutex m_mutex;
    RadioConnection* m_connection{nullptr};
    TxChannel* m_tx{nullptr};
    std::shared_ptr<RemoteCwSafetyState> m_remoteCwSafety;
    CwxSender* m_cwxSender{nullptr};
    std::function<void(bool)> m_sidetoneHold;
    std::function<void(quint32)> m_recoveryRequested;
#ifdef NEREUS_BUILD_TESTS
    friend struct TransmitCutLineTestAccess;
    RemoteCwCutValidatedObserverForTest m_cutValidatedObserverForTest;
    RemoteCwLockObserverForTest m_lockObserverForTest;
#endif
};

} // namespace NereusSDR
