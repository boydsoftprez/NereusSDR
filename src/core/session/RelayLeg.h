#pragma once
// no-port-check: NereusSDR-original.
// =================================================================
// src/core/session/RelayLeg.h  (NereusSDR)
// =================================================================
//
// iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08; the rendezvous
// document, section 12; the options survey's section D, rung 3): one end's
// leg to the WebSocket relay, the floor for a network that passes only web
// traffic.
//
// A session runs two ICE connections at each end, control (lane 1) and
// media (lane 2). The leg keeps one loopback UDP socket for control and a
// legacy media socket. Negotiated routed media instead has one socket per
// connection UUID (at most current, replacement and retiring). It gives
// each ICE agent its socket's address as a low-priority remote candidate
// (kCandidatePriority), so the agent races it with every other pair and
// nominates it only when nothing better works. A datagram the agent sends
// to its lane's socket goes to the relay as one binary message, the lane's
// tag in front; routed tag-2 payloads carry the 16-byte connection UUID.
// A message from the relay goes, without tag or UUID, to that agent. The
// agent's address is learned from what it sends (both ends'
// agents send ICE checks to their own lane socket), or set explicitly
// (setAgent()).
//
// The WebSocket opens when the grant arrives (open(), at the introduction,
// section 12.1), sends JOIN with the token first and datagrams right after
// it (section 12.3). Its reader follows 12.3's rules; each END code of 12.4
// is handled as its table says: join again (timeout, shuttingDown), try
// again shortly (full, tooManyConnections, tooManySessions), do nothing
// (replaced), or the leg is over (ended()). A close without an END is
// joined again with the same token within kRejoinWindowMs, the loopback
// sockets kept, so the ICE agents never notice (the design's A.3).
//
// Bounded, drop-oldest queues per lane (kQueueFrames, kQueueBytes) sit in
// front of the WebSocket, which is written only while less than
// kWriteLimitBytes waits in its socket, taking the lanes in turn; the
// socket has TCP_NODELAY. The system proxy is honoured (SystemProxy).
//
// The token is opaque: it goes only to the relay, never to a log.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-27: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
// =================================================================

#include "core/session/IceConfiguration.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

#include <array>
#include <deque>
#include <memory>

QT_BEGIN_NAMESPACE
class QTimer;
class QUdpSocket;
class QWebSocket;
QT_END_NAMESPACE

namespace NereusSDR {

class RelayLeg : public QObject {
    Q_OBJECT

public:
    /// Section 12.3's tags.
    static constexpr quint8 kTagControl = 0x01;
    static constexpr quint8 kTagMedia = 0x02;
    static constexpr quint8 kTagWatch = 0x03;
    static constexpr quint8 kTagJoin = 0x80;
    static constexpr quint8 kTagReady = 0x81;
    static constexpr quint8 kTagPeer = 0x82;
    static constexpr quint8 kTagEnd = 0x83;
    /// A datagram's payload, at most (the message is one byte more).
    static constexpr int kMaxDatagramBytes = 1500;
    /// Each lane's queue in front of the WebSocket (section 12.5's, as the
    /// relay's own).
    static constexpr int kQueueFrames = 64;
    static constexpr int kQueueBytes = 24576;
    static constexpr int kWatchQueueFrames = 16;
    static constexpr int kWatchQueueBytes = 8192;
    /// Written only while less than this waits in the socket (the relay's
    /// write limit), so the queues, not the socket, hold the backlog.
    static constexpr int kWriteLimitBytes = 8192;
    /// A close without an END is joined again within this long (12.4).
    static constexpr int kRejoinWindowMs = 30000;
    /// "Tries again shortly" (full, tooManyConnections, tooManySessions):
    /// this often, for at most kRetryShortlyForMs (the grant's lifetime).
    static constexpr int kRetryShortlyMs = 2000;
    static constexpr int kRetryShortlyForMs = 120000;
    /// The remote candidate's priority: below every candidate an agent
    /// makes itself, so ICE prefers any other working pair.
    static constexpr quint32 kCandidatePriority = 1;

    enum class State {
        Idle,       ///< no grant yet
        Connecting, ///< the WebSocket is opening or JOIN is out
        Joined,     ///< READY came
        Waiting,    ///< closed; joining again soon
        Ended,      ///< over (ended() said why), or closed by this end
    };

    explicit RelayLeg(QObject* parent = nullptr);
    ~RelayLeg() override;

    /// A leg with its lanes bound, owned by whoever holds it (the ICE
    /// settings' candidate source factory, so it lives as long as a
    /// connection that may use it), deleted on its thread's event loop.
    /// Null when the lanes cannot be bound.
    static std::shared_ptr<RelayLeg> create();
    /// A separate watch-purpose socket and one loopback ICE lane. Its
    /// control-lane candidate carries only tag-3 datagrams.
    static std::shared_ptr<RelayLeg> createWatch();
    /// The factory for IceConfiguration::setCandidateSourceFactory: each
    /// call a new source on `leg`.
    static IceConfiguration::CandidateSourceFactory factoryFor(std::shared_ptr<RelayLeg> leg);

    RelayLeg(const RelayLeg&) = delete;
    RelayLeg& operator=(const RelayLeg&) = delete;

    /// Binds the two lane sockets on 127.0.0.1. False when one cannot be
    /// bound (then the leg offers nothing).
    bool bindLanes();
    /// The lane socket's port (lane 1 or 2), 0 when not bound.
    quint16 lanePort(int lane) const;
    /// The remote candidate an agent is given for a lane socket.
    static QString candidateLine(int lane, quint16 port);
    /// A candidate source for one ICE connection on `lane`
    /// (IceConfiguration::kControlLane or kMediaLane): starting it gives
    /// the agent the lane's candidate and makes that connection the lane's
    /// (legacy mode: a later connection on the same lane takes it over;
    /// routed media: every UUID has its own socket). Stopping it releases
    /// only its own claim.
    std::shared_ptr<IceConfiguration::CandidateSource> sourceFor(int lane,
                                                                 const QString& connectionId = {},
                                                                 bool routed = false);

    /// Opens the leg to the relay at `url` with `token` (the grant).
    /// Nothing happens when the leg is already open or over.
    void open(const QUrl& url, const QString& token);
    /// Closes the leg for good (nothing more is sent, no rejoin).
    void close();

    State state() const { return m_state; }
    bool peerPresent() const { return m_peerPresent; }
    /// The END code that ended the leg (or "lost" when a rejoin window ran
    /// out), empty while it runs.
    QString endCode() const { return m_endCode; }
    /// Section 12.4's words for an END code, empty for one that shows
    /// none (replaced, idle).
    static QString wordsFor(const QString& code);

    /// Counters, for tests and the log.
    quint64 datagramsSent() const { return m_sent; }
    quint64 datagramsDelivered() const { return m_delivered; }
    quint64 droppedQueueFull() const { return m_droppedQueue; }
    quint64 droppedNoAgent() const { return m_droppedNoAgent; }
    quint64 droppedUnknownTag() const { return m_droppedUnknownTag; }
    quint64 droppedOversize() const { return m_droppedOversize; }
    quint64 droppedWrongSender() const { return m_droppedWrongSender; }
    int connections() const { return m_connections; }

    /// Test seam (the relay-leg conformance runner): the agent on `lane`
    /// is at `address`:`port`, before it has sent anything.
    void setAgentForTest(int lane, const QHostAddress& address, quint16 port);
    /// Test seam: every leg opened from now on goes to `url` instead of its
    /// grant's (a stand-in relay on this computer; the service's grants
    /// always name a wss:// address). An empty URL ends it.
    static void setRelayUrlForTest(const QUrl& url);

signals:
    /// READY: joined; `peerPresent` the other leg is there.
    void joined(bool peerPresent);
    void peerChanged(bool present);
    /// The leg is over: `code` the END code (or "lost"), `words` 12.4's
    /// words for it (empty for none).
    void ended(const QString& code, const QString& words);

private:
    enum class Purpose { Primary, Watch };
    explicit RelayLeg(Purpose purpose);
    struct Lane {
        QUdpSocket* socket = nullptr;
        QHostAddress agentAddress;
        quint16 agentPort = 0;
        quint64 claim = 0;
        std::deque<QByteArray> queue;
        int queuedBytes = 0;
    };
    struct Route {
        QUdpSocket* socket = nullptr;
        QHostAddress agentAddress;
        quint16 agentPort = 0;
        quint64 claim = 0;
    };

    friend class RelayLaneSource;
    quint64 claimLane(int lane);
    void releaseLane(int lane, quint64 claim);
    quint64 claimRoute(const QByteArray& id, quint16& port);
    void releaseRoute(const QByteArray& id, quint64 claim);
    void readRoute(const QByteArray& id);
    Lane* laneFor(int lane);

    void connectNow();
    void onConnected();
    void onDisconnected();
    void onMessage(const QByteArray& message);
    void onEnd(const QString& code);
    void readLane(int lane);
    void enqueue(int lane, const QByteArray& frame);
    void flush();
    qint64 socketBacklog() const;
    void scheduleConnect(int delayMs);
    void finish(const QString& code);
    void dropSocket();

    std::array<Lane, 2> m_lanes;
    Purpose m_purpose = Purpose::Primary;
    QHash<QByteArray, Route> m_routes;
    bool m_mediaRouted = false;
    bool m_mediaModeChosen = false;
    int m_nextLane = 0;
    QUrl m_url;
    QString m_token;
    QPointer<QWebSocket> m_socket;
    QTimer* m_connectTimer = nullptr;
    State m_state = State::Idle;
    bool m_peerPresent = false;
    /// An END came on the current connection: what follows on it is
    /// ignored, and its close does what the END said.
    bool m_endSeen = false;
    enum class AfterClose { Rejoin, RetryShortly, Stop };
    AfterClose m_afterClose = AfterClose::Rejoin;
    QElapsedTimer m_since; ///< since the drop (rejoin) or the first "shortly" retry
    bool m_sinceRunning = false;
    int m_rejoinDelayMs = 0;
    QString m_endCode;
    // Step 2b: a sign-in page or an inspecting network, in plain words.
    QString m_networkTrouble;
    quint64 m_nextClaim = 1;
    quint64 m_sent = 0;
    quint64 m_delivered = 0;
    quint64 m_droppedQueue = 0;
    quint64 m_droppedNoAgent = 0;
    quint64 m_droppedUnknownTag = 0;
    quint64 m_droppedOversize = 0;
    quint64 m_droppedWrongSender = 0;
    int m_connections = 0;
};

} // namespace NereusSDR
