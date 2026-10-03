#pragma once
// =================================================================
// src/core/session/media/RtpDuplicateFilter.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original.
//
// iPhone app plan Task 29 (R-IOS-16; the pairing design, section 5.4,
// "dual-receive across a switch window, timestamp-based deduplication";
// the remote media control document, "Replacing the media connection"):
// while a media connection is being replaced, the Core sends every audio
// packet on both peers with the same RTP timestamp, and the app takes each
// stream's packet once. This keeps, per stream, the last kWindow
// timestamps it let through and drops a packet whose timestamp is among
// them, so a packet lost on one path and carried on the other is heard
// once and none twice. A packet too short to be RTP is let through; the
// receiver judges it.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include <QByteArray>
#include <QHash>
#include <QtGlobal>

#include <deque>

namespace NereusSDR {

class RtpDuplicateFilter {
public:
    /// Timestamps remembered per stream: 64 packets, 2.56 s of 40 ms Opus
    /// audio and more than the deepest jitter hold (0.5 s) plus any path's
    /// delay difference; 0.64 s of 10 ms lossless packets.
    static constexpr int kWindow = 64;

    /// True the first time `timestamp` comes for stream `ssrc` among the
    /// last kWindow let through; false for a copy.
    bool admit(quint32 ssrc, quint32 timestamp);
    /// The same for an RTP packet, read from its header.
    bool admit(const QByteArray& packet);
    void clear();
    /// Copies dropped since the last clear().
    quint64 dropped() const { return m_dropped; }

private:
    QHash<quint32, std::deque<quint32>> m_seen;
    quint64 m_dropped = 0;
};

} // namespace NereusSDR
