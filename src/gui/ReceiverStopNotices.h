// =================================================================
// src/gui/ReceiverStopNotices.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R-R3-42, R-R3-44 (R3 receiver audio
// fix wave): one plain notice per receiver-audio stop.
//
// In a remote window a receiver's audio can reach several apps on this
// computer at once: TCI and each VAX channel carrying the slice. When the
// Core stops that audio (the slice was removed, the radio went offline)
// every one of them used to raise its own toast ("TCI: ...", "VAX 1: ...",
// "VAX 2: ...") beside the window's own status. This decides, for each
// consumer's stop, whether a toast is still news:
//   - a stop the window already shows in its own status (the link not up
//     yet, the radio offline) or one this computer caused (an app stopped
//     asking) raises none;
//   - any other stop raises one plain notice per event: the same reason
//     for the same receiver (slice) from another consumer within
//     kSameEventMs is the same event; the same reason for another slice
//     is another event.
// The TCI applet, the TCI log window and the VAX channel state still
// show each stop.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-23: Written for NereusSDR by J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-23: Follow-up: an event is the reason and the slice.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
// =================================================================

#pragma once

#include <QHash>
#include <QString>

namespace NereusSDR {

class ReceiverStopNotices {
public:
    static constexpr qint64 kSameEventMs = 5000;

    /// True when `reason` says why a receiver's audio for an app on this
    /// computer stopped (a receiver audio wire reason, or the older-Core
    /// sentence).
    static bool isReceiverStop(const QString& reason);

    /// The toast for one consumer's stop of slice `sliceId`'s audio at
    /// `nowMs`, or empty when there should be none (see the header
    /// comment).
    QString toastFor(const QString& reason, int sliceId, qint64 nowMs);

private:
    QHash<QString, qint64> m_lastToastMs;
};

} // namespace NereusSDR
