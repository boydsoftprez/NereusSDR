// no-port-check: NereusSDR-original.
// =================================================================
// src/core/safety/TransmitCutLine.cpp  (NereusSDR)
// =================================================================
//
// See TransmitCutLine.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-27: Add approved remote CW hold/activity integration for
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-27: Propose unused isolated CW bridge RED bootstrap for
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-15: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), remote transmit cut off the main thread, with
//               AI-assisted implementation via Anthropic Claude Code.
//   2026-10-26: Remote transmit hold then cut: hold(). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/safety/TransmitCutLine.h"

#include "core/RadioConnection.h"
#include "core/CwxSender.h"
#include "core/safety/RemoteCwSafetyState.h"
#include "core/TxChannel.h"

namespace NereusSDR {

void TransmitCutLine::setTargets(RadioConnection* connection, TxChannel* tx)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_connection = connection;
    m_tx = tx;
    m_remoteCwSafety = connection ? connection->remoteCwSafetyState() : nullptr;
}

void TransmitCutLine::cut()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    // RF first: no further TX I/Q block reaches the connection once this
    // returns, and a channel-on still queued no longer opens the gate.
    if (m_tx != nullptr) {
        m_tx->closeRfGate();
    }
    // Then MOX and the relay, queued onto the connection's own thread.
    if (m_connection != nullptr) {
        m_connection->cutTransmit();
    }
}

void TransmitCutLine::hold(bool held)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    // RF silent while still keyed: the gate stays open and the connection
    // is not touched, so a receipt can give RF back at once.
    if (m_tx != nullptr) {
        m_tx->setRfHold(TxChannel::RfHold::Guard, held);
    }
}

void TransmitCutLine::setRemoteCwTargets(CwxSender* sender, std::function<void(bool)> sidetoneHold,
                                         std::function<void(quint32)> recoveryRequested)
{
#ifdef NEREUS_BUILD_TESTS
    std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
    if (!lock.try_lock()) {
        if (m_lockObserverForTest) {
            m_lockObserverForTest(RemoteCwLockPointForTest::TargetsContended);
        }
        lock.lock(); // Actual SAME lifetime mutex, not a test surrogate.
    }
    if (m_lockObserverForTest) {
        m_lockObserverForTest(RemoteCwLockPointForTest::TargetsAcquired);
    }
#else
    const std::lock_guard<std::mutex> lock(m_mutex);
#endif
    m_cwxSender = sender;
    m_sidetoneHold = std::move(sidetoneHold);
    m_recoveryRequested = std::move(recoveryRequested);
}

bool TransmitCutLine::beginRemoteCw(quint32 epoch)
{
#ifdef NEREUS_BUILD_TESTS
    std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
    if (!lock.try_lock()) {
        if (m_lockObserverForTest) {
            m_lockObserverForTest(RemoteCwLockPointForTest::BeginContended);
        }
        lock.lock(); // Actual SAME lifetime mutex, not a test surrogate.
    }
    if (m_lockObserverForTest) {
        m_lockObserverForTest(RemoteCwLockPointForTest::BeginAcquired);
    }
#else
    const std::lock_guard<std::mutex> lock(m_mutex);
#endif
    if (epoch == 0 || !m_connection || !m_remoteCwSafety || !m_cwxSender
        || m_connection->transmitCut() || m_remoteCwSafety->snapshot().epoch != 0) {
        return false;
    }
    m_remoteCwSafety->begin(epoch);
    const RemoteCwSafetySnapshot admitted = m_remoteCwSafety->snapshot();
    if (admitted.epoch != epoch || !admitted.valid || admitted.invalidated) { return false; }
    // Atomic-only; NEVER setRemoteSafetyState or a checkpoint mutex here.
    m_cwxSender->bindRemoteActivity(epoch);
    return true;
}

void TransmitCutLine::hold(quint32 expectedCwEpoch, bool held)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_connection || !m_remoteCwSafety || !m_cwxSender) { return; }
    const RemoteCwSafetySnapshot state = m_remoteCwSafety->snapshot();
    if (expectedCwEpoch == 0 || state.epoch != expectedCwEpoch || !state.valid) { return; }
    if (!held) {
        // Recovery is requested with captured identity; only the validated
        // recovery owner may prepare/resume and clear recovered reasons.
        if (m_recoveryRequested) {
            m_recoveryRequested(expectedCwEpoch);
        }
        return;
    }
    if (!m_connection->setRemoteCwHold(expectedCwEpoch, TxChannel::RfHold::Guard, true)) { return; }
    if (m_tx) { m_tx->setRfHold(TxChannel::RfHold::Guard, true); }
    if (m_sidetoneHold) {
        m_sidetoneHold(true);
    }
    m_cwxSender->requestRemotePause(expectedCwEpoch);
}

void TransmitCutLine::cut(quint32 expectedCwEpoch)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_connection || !m_remoteCwSafety || !m_cwxSender
        || !m_remoteCwSafety->invalidate(expectedCwEpoch)) { return; }
#ifdef NEREUS_BUILD_TESTS
    if (m_cutValidatedObserverForTest) { m_cutValidatedObserverForTest(expectedCwEpoch); }
#endif
    // Protocol cut latches before TX close can wait for an in-flight send.
    // Neither sidetone callback nor captured request waits for checkpoint.
    m_connection->cutTransmit();
    if (m_tx) {
        m_tx->closeRfGate();
    }
    if (m_sidetoneHold) {
        m_sidetoneHold(true);
    }
    m_cwxSender->requestRemoteCancel(expectedCwEpoch);
}

#ifdef NEREUS_BUILD_TESTS
void TransmitCutLine::setRemoteCwLockObserverForTest(RemoteCwLockObserverForTest observer)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_lockObserverForTest = std::move(observer);
}

void TransmitCutLine::setRemoteCwCutValidatedObserverForTest(RemoteCwCutValidatedObserverForTest observer)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_cutValidatedObserverForTest = std::move(observer);
}
#endif

} // namespace NereusSDR
