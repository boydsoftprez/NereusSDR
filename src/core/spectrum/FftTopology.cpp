// =================================================================
// src/core/spectrum/FftTopology.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See FftTopology.h for what this
// replaces (the router-mutation half of MainWindow::rebuildFftRouting)
// and why.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  Remote daemon R1, extraction 5 of 9.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include "core/spectrum/FftTopology.h"

#include "core/FFTRouter.h"

namespace NereusSDR {

void FftTopology::subscribe(const QString& consumerId, int streamIndex)
{
    m_streamByConsumer[consumerId] = streamIndex;
}

void FftTopology::unsubscribe(const QString& consumerId)
{
    m_streamByConsumer.remove(consumerId);
}

void FftTopology::applyTo(FFTRouter& router) const
{
    // Drop every consumer the router learned about after the previous
    // call. FFTRouter::removePan clears a consumer from every receiver it
    // was mapped to and is a safe no-op for a consumer the router does
    // not currently have, so this is correct whether or not anything
    // changed since last time.
    for (auto it = m_lastApplied.cbegin(); it != m_lastApplied.cend(); ++it) {
        router.removePan(it.key());
    }
    // Remap the current subscription set. Every entry here was just
    // removed by the loop above (it was either in m_lastApplied already,
    // or it is new and was never in the router to begin with), so there
    // is nothing stale left for mapPanToReceiver to collide with.
    for (auto it = m_streamByConsumer.cbegin(); it != m_streamByConsumer.cend(); ++it) {
        router.mapPanToReceiver(it.key(), it.value());
    }
    m_lastApplied = m_streamByConsumer;
}

QList<SpectrumSubscription> FftTopology::subscriptions() const
{
    QList<SpectrumSubscription> result;
    result.reserve(m_streamByConsumer.size());
    for (auto it = m_streamByConsumer.cbegin(); it != m_streamByConsumer.cend(); ++it) {
        result.append(SpectrumSubscription{it.key(), it.value()});
    }
    return result;
}

} // namespace NereusSDR
