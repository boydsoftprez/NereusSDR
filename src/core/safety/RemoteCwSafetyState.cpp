// no-port-check: NereusSDR-original packed remote CW safety state.
// =================================================================
// src/core/safety/RemoteCwSafetyState.cpp (NereusSDR)
// =================================================================
//
// Epoch occupies the upper 32 bits; independent hold reasons, validity and
// invalidation occupy the lower bits. A full-word CAS makes identity and
// state changes indivisible. This value has no production wiring yet.
//
// Modification history (NereusSDR):
//   2026-10-27: Implement the packed atomic safety value after observed RED.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-27: Prepare the inert safety-value RED baseline for
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// =================================================================

#include "core/safety/RemoteCwSafetyState.h"

namespace NereusSDR {

namespace {

constexpr unsigned kEpochShift = 32;
// Hold reason values come from TxChannel::RfHold, not a second enum.
constexpr quint64 kLinkBit = static_cast<quint64>(TxChannel::RfHold::Link);
constexpr quint64 kMicBit = static_cast<quint64>(TxChannel::RfHold::Mic);
constexpr quint64 kGuardBit = static_cast<quint64>(TxChannel::RfHold::Guard);
constexpr quint64 kHoldMask = kLinkBit | kMicBit | kGuardBit;
constexpr quint64 kValidBit = quint64{1} << 3;
constexpr quint64 kInvalidatedBit = quint64{1} << 4;

quint32 epochOf(quint64 state) noexcept
{
    return static_cast<quint32>(state >> kEpochShift);
}

bool isHoldReason(quint64 bit) noexcept
{
    return bit == kLinkBit || bit == kMicBit || bit == kGuardBit;
}

} // namespace

void RemoteCwSafetyState::begin(quint32 epoch)
{
    if (epoch == 0) {
        return;
    }
    quint64 inactive = 0;
    const quint64 admitted = (quint64{epoch} << kEpochShift) | kValidBit;
    // Admission is serialized by the caller. Refuse an uncleared activity
    // rather than overwrite its hold or invalidation latch.
    m_state.compare_exchange_strong(inactive, admitted, std::memory_order_acq_rel,
                                   std::memory_order_acquire);
}

RemoteCwSafetySnapshot RemoteCwSafetyState::snapshot() const noexcept
{
    const quint64 state = m_state.load(std::memory_order_acquire);
    return {epochOf(state), static_cast<quint32>(state & kHoldMask),
            (state & kValidBit) != 0, (state & kInvalidatedBit) != 0};
}

bool RemoteCwSafetyState::setHold(quint32 epoch, TxChannel::RfHold reason) noexcept
{
    const quint64 bit = static_cast<quint64>(reason);
    if (epoch == 0 || !isHoldReason(bit)) {
        return false;
    }
    quint64 state = m_state.load(std::memory_order_acquire);
    while (epochOf(state) == epoch && (state & kValidBit) != 0) {
        const quint64 held = state | bit;
        if (m_state.compare_exchange_weak(state, held, std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
            return true;
        }
    }
    return false;
}

bool RemoteCwSafetyState::clearHold(quint32 epoch, TxChannel::RfHold reason) noexcept
{
    const quint64 bit = static_cast<quint64>(reason);
    if (epoch == 0 || !isHoldReason(bit)) {
        return false;
    }
    quint64 state = m_state.load(std::memory_order_acquire);
    while (epochOf(state) == epoch && (state & kValidBit) != 0) {
        // bit is 64-bit: its complement must retain the upper epoch bits.
        const quint64 released = state & ~bit;
        if (m_state.compare_exchange_weak(state, released, std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
            return true;
        }
    }
    return false;
}

bool RemoteCwSafetyState::invalidate(quint32 epoch) noexcept
{
    if (epoch == 0) {
        return false;
    }
    quint64 state = m_state.load(std::memory_order_acquire);
    while (epochOf(state) == epoch && (state & kValidBit) != 0) {
        const quint64 invalidated = (state & ~kValidBit) | kInvalidatedBit;
        if (m_state.compare_exchange_weak(state, invalidated, std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
            return true;
        }
    }
    return false;
}

bool RemoteCwSafetyState::invalidatePhysicalPress(quint32 observedEpoch) noexcept
{
    if (observedEpoch == 0) {
        return false;
    }
    quint64 state = m_state.load(std::memory_order_acquire);
    while (epochOf(state) == observedEpoch && (state & kValidBit) != 0
           && (state & (kLinkBit | kGuardBit)) != 0) {
        // Match the epoch and network hold in the SAME CAS as invalidation.
        // A concurrent clear either wins first or observes invalidity.
        const quint64 invalidated = (state & ~kValidBit) | kInvalidatedBit;
        if (m_state.compare_exchange_weak(state, invalidated, std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
            return true;
        }
    }
    return false;
}

void RemoteCwSafetyState::finish(quint32 epoch) noexcept
{
    if (epoch == 0) {
        return;
    }
    quint64 state = m_state.load(std::memory_order_acquire);
    while (epochOf(state) == epoch) {
        if (m_state.compare_exchange_weak(state, quint64{0}, std::memory_order_acq_rel,
                                         std::memory_order_acquire)) {
            return;
        }
    }
}

bool RemoteCwSafetyState::suppressesCw() const noexcept
{
    const quint64 state = m_state.load(std::memory_order_acquire);
    return (state & (kHoldMask | kInvalidatedBit)) != 0;
}

} // namespace NereusSDR
