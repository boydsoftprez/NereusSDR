// =================================================================
// src/gui/ReceiverStopNotices.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R-R3-42, R-R3-44; see
// ReceiverStopNotices.h.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23: Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include "gui/ReceiverStopNotices.h"

#include "gui/OperatorReasonText.h"
#include "gui/RemoteMediaController.h"

namespace NereusSDR {

bool ReceiverStopNotices::isReceiverStop(const QString& reason)
{
    if (reason == QLatin1String(RemoteMediaController::kReceiverAudioUnavailableReason)) {
        return true;
    }
    return reason == QLatin1String("client-disabled")
        || reason == QLatin1String("media-not-ready")
        || reason == QLatin1String("radio-offline")
        || reason == QLatin1String("encoder-unavailable")
        || reason == QLatin1String("slice-removed")
        || reason == QLatin1String("receiver-limit");
}

QString ReceiverStopNotices::toastFor(const QString& reason, int sliceId, qint64 nowMs)
{
    // The window's own status already says these (the media link not up,
    // the radio offline), and an app that stopped asking caused its own.
    if (reason == QLatin1String("media-not-ready")
        || reason == QLatin1String("radio-offline")
        || reason == QLatin1String("client-disabled")) {
        return {};
    }
    const QString event = QString::number(sliceId) + QLatin1Char(':') + reason;
    const auto last = m_lastToastMs.constFind(event);
    if (last != m_lastToastMs.constEnd() && nowMs - last.value() < kSameEventMs) {
        return {};
    }
    m_lastToastMs.insert(event, nowMs);
    return OperatorReasonText::forDisplay(reason);
}

} // namespace NereusSDR
