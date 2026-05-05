// =================================================================
// src/core/audio/CompositeTxMicRouter.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original file. See CompositeTxMicRouter.h for design notes.
//
// Plan: 3M-1b F.3 (initial); radio-mic-input Thetis parity Task 11
// (Radio source switched from RadioMicSource to TxMicSource).
// Pre-code review §0.3 + master design §5.2.1.
// =================================================================

// no-port-check: NereusSDR-original file; no Thetis logic ported here.

#include "core/audio/CompositeTxMicRouter.h"
#include "core/audio/PcMicSource.h"
#include "core/audio/TxMicSource.h"
#include "core/audio/VaxTxMicSource.h"

namespace NereusSDR {

CompositeTxMicRouter::CompositeTxMicRouter(PcMicSource* pcSource,
                                           TxMicSource* txMicSource,
                                           bool         hasMicJack)
    : m_pcSource(pcSource)
    // Collapse: if hasMicJack is false, the Radio path is permanently
    // unavailable.  Storing nullptr here makes the pullSamples dispatch
    // a single atomic load + null check.
    , m_txMicSource(hasMicJack ? txMicSource : nullptr)
    , m_hasMicJack(hasMicJack && txMicSource != nullptr)
{
    // Default to Pc on construction regardless of persisted preference.
    // The caller (RadioModel) will call setActiveSource() with the
    // user's persisted selection after construction.
    m_activeSource.store(MicSource::Pc, std::memory_order_release);
    m_pendingSource.store(MicSource::Pc, std::memory_order_release);
}

void CompositeTxMicRouter::setActiveSource(MicSource source)
{
    // HL2 force-PC: if hasMicJack is false (collapsed at construction),
    // silently drop Radio requests. They can never be applied.
    if (!m_hasMicJack && source == MicSource::Radio) {
        return;
    }

    // MOX-locked: defer source switch if TX is active to prevent audible
    // clicks and mid-TX buffer-state inconsistency.
    if (m_moxActive.load(std::memory_order_acquire)) {
        m_pendingSource.store(source, std::memory_order_release);
        m_hasPendingSwitch.store(true, std::memory_order_release);
        return;
    }

    // Apply immediately.
    m_activeSource.store(source, std::memory_order_release);
}

void CompositeTxMicRouter::setMoxActive(bool active)
{
    const bool wasActive = m_moxActive.exchange(active, std::memory_order_acq_rel);

    // MOX-off transition: apply any pending source switch.
    if (wasActive && !active) {
        if (m_hasPendingSwitch.exchange(false, std::memory_order_acq_rel)) {
            const MicSource pending = m_pendingSource.load(std::memory_order_acquire);
            m_activeSource.store(pending, std::memory_order_release);
        }
    }
}

void CompositeTxMicRouter::setVaxSource(VaxTxMicSource* source)
{
    // Release ordering pairs with the audio-thread acquire load in
    // pullSamples(). RadioModel calls this from the main thread once
    // VaxTxMicSource is constructed; the audio thread may already be
    // dispatching to Pc/Radio so we must publish the pointer safely
    // before the next read.
    m_vaxSource.store(source, std::memory_order_release);
}

int CompositeTxMicRouter::pullSamples(float* dst, int n)
{
    if (dst == nullptr || n <= 0) {
        return 0;
    }

    const MicSource source = m_activeSource.load(std::memory_order_acquire);

    if (source == MicSource::Radio && m_txMicSource != nullptr) {
        return m_txMicSource->pullSamples(dst, n);
    }

    if (source == MicSource::Vax) {
        VaxTxMicSource* vax = m_vaxSource.load(std::memory_order_acquire);
        if (vax != nullptr) {
            return vax->pullSamples(dst, n);
        }
        // Vax selected but unregistered — fall through to Pc rather
        // than emitting silence so a misconfiguration is at least
        // audible rather than dead-air-mysterious.
    }

    if (m_pcSource != nullptr) {
        return m_pcSource->pullSamples(dst, n);
    }

    // No sources available — zero-fill (defensive; should not occur in
    // normal operation since pcSource is always provided).
    for (int i = 0; i < n; ++i) {
        dst[i] = 0.0f;
    }
    return n;
}

} // namespace NereusSDR
