#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/MediaTunnel.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 29 step 2b (R-IOS-16; the options survey's B.3 and
// section D, rung 4; the link document, "Paths", the media tunnel): the
// media connection's datagrams inside a direct WebSocket session, for a
// network that reaches the Core's wss but not its UDP. It costs the
// remote access service nothing.
//
// The same shim as the web relay's leg (RelayLeg), on the session's own
// transport: each media generation has its own loopback UDP socket and
// lowest-priority remote candidate. A datagram the agent sends there goes
// to the far end as one binary WebSocket message: tag 2, the exact 16-byte
// connection UUID, then the datagram. The receiver strips the prefix and
// delivers only to that generation's agent. DTLS and SRTP stay end to end
// inside the already pinned TLS. ICE prefers any UDP pair that works, so on an open network
// nothing changes.
//
// Each end makes one per session, and only when both declared the tunnel
// (the Core's capability mediaTunnelVersion 1, the window's media start
// saying so), since an older peer ignores binary messages. The queue in
// front of the transport is bounded and drops its oldest, and the tunnel
// writes only while little waits in the socket, as the web relay's leg.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-30: LINK-I3: one retry timer while the link is full, in place
//               of a single-shot per flush. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-29: the direct media ladder: iceFor takes the Core's STUN
//               server, and directIceFor makes a direct-only replacement.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29: direct media fix wave: tunnelIceFor, the tunnel alone for
//               the window's fallback. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
// =================================================================

#include "core/session/IceConfiguration.h"

#include <QByteArray>
#include <QHostAddress>
#include <QHash>
#include <QObject>
#include <QPointer>

#include <deque>
#include <memory>

QT_BEGIN_NAMESPACE
class QTimer;
class QUdpSocket;
QT_END_NAMESPACE

namespace NereusSDR {

class SessionTransport;

class MediaTunnel : public QObject {
    Q_OBJECT

public:
    /// The media lane's tag (the rendezvous document's, section 12.3).
    static constexpr quint8 kTagMedia = 0x02;
    static constexpr int kMaxDatagramBytes = 1500;
    static constexpr int kQueueFrames = 64;
    static constexpr int kQueueBytes = 24576;
    static constexpr int kWriteLimitBytes = 8192;
    /// A tunnel on `transport` (the session's, a SwitchableTransport).
    static std::shared_ptr<MediaTunnel> create(SessionTransport* transport);
    ~MediaTunnel() override;

    /// ICE settings for a media connection that uses the tunnel (a direct
    /// session has none of its own): the Core's STUN server when one is
    /// known (the direct media ladder: a global IPv6 host behind a stateful
    /// firewall, or an IPv4 NAT, still needs a server-reflexive candidate
    /// for a direct pair), no TURN, and the tunnel's candidate source on
    /// the media lane, which libjuice ranks lowest.
    static IceConfiguration iceFor(std::shared_ptr<MediaTunnel> tunnel,
                                   std::optional<IceServerAddress> stun);
    /// The direct media ladder: ICE settings for a direct-only replacement
    /// (the replace's `mediaDirectVersion`): host candidates and `stun`'s
    /// server-reflexive one, no relay and no tunnel, so the connection is
    /// direct or it fails and the tunnel keeps carrying media.
    static IceConfiguration directIceFor(std::optional<IceServerAddress> stun);
    /// The direct media ladder's fallback: the tunnel's candidate source
    /// alone. No STUN, no relay, and none of this computer's host
    /// candidates, so the replacement runs on the tunnel or not at all.
    static IceConfiguration tunnelIceFor(std::shared_ptr<MediaTunnel> tunnel);

    quint64 datagramsSent() const { return m_sent; }
    quint64 datagramsDelivered() const { return m_delivered; }
    quint64 droppedQueueFull() const { return m_droppedQueue; }
    quint64 droppedOversize() const { return m_droppedOversize; }
    quint64 droppedWrongSender() const { return m_droppedWrongSender; }
    quint64 droppedNoRoute() const { return m_droppedNoRoute; }
    /// Test seam: how many times flush() has asked the link for its
    /// backlog (one per pass), so a test can see that a stalled link
    /// is retried by one timer, not one per datagram.
    quint64 flushPassesForTest() const { return m_flushPasses; }

private:
    explicit MediaTunnel(SessionTransport* transport);
    friend class MediaTunnelSource;
    quint64 claim(const QByteArray& id, quint16& port);
    void release(const QByteArray& id, quint64 claim);
    void readLane(const QByteArray& id);
    void onBinary(const QByteArray& message);
    void flush();

    QPointer<SessionTransport> m_transport;
    /// LINK-I3: the one retry while the link is full. Every datagram
    /// flushes; a timer per flush would multiply while the link stalls.
    QTimer* m_flushRetry = nullptr;
    quint64 m_flushPasses = 0;
    struct Route {
        QUdpSocket* socket = nullptr;
        QHostAddress agentAddress;
        quint16 agentPort = 0;
        quint64 claim = 0;
    };
    QHash<QByteArray, Route> m_routes;
    quint64 m_nextClaim = 1;
    std::deque<QByteArray> m_queue;
    int m_queuedBytes = 0;
    quint64 m_sent = 0;
    quint64 m_delivered = 0;
    quint64 m_droppedQueue = 0;
    quint64 m_droppedOversize = 0;
    quint64 m_droppedWrongSender = 0;
    quint64 m_droppedNoRoute = 0;
};

} // namespace NereusSDR
