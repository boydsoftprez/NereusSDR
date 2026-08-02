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
//   2026-08-02  J.J. Boyd / KG4VCF  Fix round 1: widened to many streams
//                                    per consumer (coordinator spec
//                                    review finding 1) and added the
//                                    per-stream unsubscribe overload.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include "core/spectrum/FftTopology.h"

#include "core/FFTRouter.h"

namespace NereusSDR {

void FftTopology::subscribe(const QString& consumerId, int streamIndex)
{
    QList<int>& streams = m_streamsByConsumer[consumerId];
    if (!streams.contains(streamIndex)) {
        streams.append(streamIndex);
    }
}

void FftTopology::unsubscribe(const QString& consumerId)
{
    m_streamsByConsumer.remove(consumerId);
}

void FftTopology::unsubscribe(const QString& consumerId, int streamIndex)
{
    const auto it = m_streamsByConsumer.find(consumerId);
    if (it == m_streamsByConsumer.end()) { return; }
    it.value().removeAll(streamIndex);
    if (it.value().isEmpty()) {
        m_streamsByConsumer.erase(it);
    }
}

void FftTopology::applyTo(FFTRouter& router) const
{
    // Drop every consumer the router learned about after the previous
    // call. FFTRouter::removePan clears a consumer from every receiver it
    // was mapped to -- all of its streams in one call -- and is a safe
    // no-op for a consumer the router does not currently have, so this is
    // correct whether or not anything changed since last time.
    for (const QString& consumerId : m_lastAppliedConsumers) {
        router.removePan(consumerId);
    }
    // Remap every stream of the current subscription set. Every consumer
    // here was just removed by the loop above (it was either in
    // m_lastAppliedConsumers already, or it is new and was never in the
    // router to begin with), so there is nothing stale left for
    // mapPanToReceiver to collide with.
    for (auto it = m_streamsByConsumer.cbegin(); it != m_streamsByConsumer.cend(); ++it) {
        for (int stream : it.value()) {
            router.mapPanToReceiver(it.key(), stream);
        }
    }
    m_lastAppliedConsumers = m_streamsByConsumer.keys();
}

QList<SpectrumSubscription> FftTopology::subscriptions() const
{
    QList<SpectrumSubscription> result;
    for (auto it = m_streamsByConsumer.cbegin(); it != m_streamsByConsumer.cend(); ++it) {
        for (int stream : it.value()) {
            result.append(SpectrumSubscription{it.key(), stream});
        }
    }
    return result;
}

} // namespace NereusSDR
