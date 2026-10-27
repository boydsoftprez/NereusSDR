// no-port-check: NereusSDR-original inert RED scaffold.
// =================================================================
// src/core/safety/RemoteCwSafetyState.cpp (NereusSDR)
// =================================================================
//
// Intentionally no epoch admission, hold mutation, recovery or CAS yet.
// This compile-compatible baseline must fail the positive behavioral tests.
// It cannot grant start/resume: every mutation returns false and the output
// gate stays suppressed. No production caller uses this scaffold.
//
// Modification history (NereusSDR):
//   2026-10-27: Prepare the inert safety-value RED baseline for
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// =================================================================

#include "core/safety/RemoteCwSafetyState.h"

namespace NereusSDR {

void RemoteCwSafetyState::begin(quint32 epoch)
{
    Q_UNUSED(epoch);
}

RemoteCwSafetySnapshot RemoteCwSafetyState::snapshot() const noexcept
{
    return {};
}

bool RemoteCwSafetyState::setHold(quint32 epoch, TxChannel::RfHold reason) noexcept
{
    Q_UNUSED(epoch);
    Q_UNUSED(reason);
    return false;
}

bool RemoteCwSafetyState::clearHold(quint32 epoch, TxChannel::RfHold reason) noexcept
{
    Q_UNUSED(epoch);
    Q_UNUSED(reason);
    return false;
}

bool RemoteCwSafetyState::invalidate(quint32 epoch) noexcept
{
    Q_UNUSED(epoch);
    return false;
}

bool RemoteCwSafetyState::invalidatePhysicalPress(quint32 observedEpoch) noexcept
{
    Q_UNUSED(observedEpoch);
    return false;
}

void RemoteCwSafetyState::finish(quint32 epoch) noexcept
{
    Q_UNUSED(epoch);
}

bool RemoteCwSafetyState::suppressesCw() const noexcept
{
    return true;
}

} // namespace NereusSDR
