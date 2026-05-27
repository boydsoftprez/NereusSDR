// =================================================================
// src/core/FFTRouter.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See FFTRouter.h for full rationale
// and the Modification history block.
//
// =================================================================

#include "core/FFTRouter.h"

namespace NereusSDR {

void FFTRouter::mapPanToReceiver(const QString& panId, int receiverId)
{
    auto& list = m_receiverToPans[receiverId];
    if (!list.contains(panId)) { list.append(panId); }
}

void FFTRouter::removePan(const QString& panId)
{
    for (auto& list : m_receiverToPans) {
        list.removeAll(panId);
    }
}

void FFTRouter::removeReceiver(int receiverId)
{
    m_receiverToPans.remove(receiverId);
}

QList<QString> FFTRouter::pansForReceiver(int receiverId) const
{
    return m_receiverToPans.value(receiverId);
}

QList<int> FFTRouter::receiversForPan(const QString& panId) const
{
    QList<int> result;
    for (auto it = m_receiverToPans.cbegin(); it != m_receiverToPans.cend(); ++it) {
        if (it.value().contains(panId)) { result.append(it.key()); }
    }
    return result;
}

void FFTRouter::onFftFrame(int receiverId, const QVector<float>& binsDbm)
{
    for (const QString& panId : m_receiverToPans.value(receiverId)) {
        emit fftFrameForPan(panId, receiverId, binsDbm);
    }
}

} // namespace NereusSDR
