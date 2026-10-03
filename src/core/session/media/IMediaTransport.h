#pragma once
// =================================================================
// src/core/session/media/IMediaTransport.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R3 Task 1.
//
// One authenticated session's direct encrypted media peer. Authentication
// and model ownership remain above this interface. Implementations carry
// already-encoded display, audio, transmit-keepalive, and raw I/Q payloads.
//
// Modification history (NereusSDR):
//   2026-09-25: iPhone app plan Task 37 (R-IOS-13): the "tx" data channel
//               (StartOptions::txChannel, sendTx, txReceived) for the
//               transmit keepalive. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-26: iPhone app plan Task 27 (R-IOS-16): StartOptions::ice for a
//               connection that came through the remote access service,
//               gatherCandidates(), gatheringComplete() and selectedPath().
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-26: Task 27 follow-up: MediaIcePath::relayed() also counts a
//               remote at a relay candidate's address the far end sent.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//
//   2026-09-27: iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08): the
//               web relay's leg (RelayLeg) and its per-connection candidate
//               sources; the computer's own proxy settings (SystemProxy).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30: TX stall lane: micRtpReceived carries heldUs, how long
//               the packet waited between its receipt in the transport
//               and its report, so the microphone buffer times it at
//               receipt. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-01: TX mic thread (JJ approved): setMicPacketSink, the
//               microphone line delivered on a transport's own thread;
//               txReceived carries heldUs. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-01: Control logging lane: MediaIcePath's candidate
//               transports and rttMs(). Logging only. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/IceConfiguration.h"
#include "core/session/NetworkPathSnapshot.h"

#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

#include <functional>
#include <optional>

namespace NereusSDR {

/// iPhone app plan Task 27: the pair of candidates a connection settled on,
/// what the desktop's attempt record reads to tell a direct path from a
/// relayed one. Types are RFC 8839's (`host`, `srflx`, `prflx`, `relay`).
struct MediaIcePath {
    QString localType;
    QString remoteType;
    /// Control logging lane: each candidate's transport ("udp",
    /// "tcp-active", "tcp-passive", "tcp-so", "tcp"), empty where not
    /// known. For the log only.
    QString localTransport;
    QString remoteTransport;
    QString localAddress;
    quint16 localPort = 0;
    QString remoteAddress;
    quint16 remotePort = 0;
    /// The relay candidates (`typ relay`) the far end sent, as address and
    /// port.
    QList<QPair<QString, quint16>> farEndRelays;
    /// The selected remote endpoint matches a loopback candidate admitted
    /// from this transport's own CandidateSource, not merely a loopback IP.
    bool ownedLoopbackShim = false;
    std::optional<NetworkPathSnapshot> ownedSourcePath;

    std::optional<NetworkPathSnapshot> networkPathSnapshot() const
    {
        if (ownedLoopbackShim) {
            return ownedSourcePath;
        }
        const auto numeric = [](const QString& value) {
            QHostAddress address(value);
            if (address.isNull()) { return QString(); }
            bool ipv4 = false;
            const quint32 v4 = address.toIPv4Address(&ipv4);
            if (ipv4) { address = QHostAddress(v4); }
            address.setScopeId(QString());
            return address.toString();
        };
        NetworkPathSnapshot path;
        path.kind = relayed() ? NetworkPathSnapshot::Kind::Relayed
                              : NetworkPathSnapshot::Kind::Direct;
        path.carrier = NetworkPathSnapshot::Carrier::Ice;
        path.endpoints = NetworkPathSnapshot::Endpoints::IceCandidates;
        path.localAddress = numeric(localAddress);
        path.localPort = localPort;
        path.remoteAddress = numeric(remoteAddress);
        path.remotePort = remotePort;
        path.localCandidateType = localType;
        path.remoteCandidateType = remoteType;
        return path;
    }

    static std::optional<QPair<QString, quint16>> loopbackEndpoint(const QString& address,
                                                                    quint16 port)
    {
        if (port == 0) {
            return std::nullopt;
        }
        const QHostAddress parsed(address);
        bool ipv4 = false;
        const quint32 v4 = parsed.toIPv4Address(&ipv4);
        if (ipv4 && (v4 & 0xff000000U) == 0x7f000000U) {
            return qMakePair(QHostAddress(v4).toString(), port);
        }
        if (parsed == QHostAddress::LocalHostIPv6) {
            return qMakePair(QHostAddress(QHostAddress::LocalHostIPv6).toString(), port);
        }
        return std::nullopt;
    }

    /// Through the relay: either candidate is a relay one, or the remote is
    /// at the address and port of a relay candidate the far end sent. The
    /// last is the follow-up to the Task 27 re-review: a remote learned as
    /// peer-reflexive from a check that came through the far end's relay,
    /// before its relay candidate arrived through the service, is typed
    /// `prflx` although the traffic goes through the relay.
    bool relayed() const
    {
        if (localType == QLatin1String("relay") || remoteType == QLatin1String("relay")) {
            return true;
        }
        return remotePort != 0 && farEndRelays.contains(qMakePair(remoteAddress, remotePort));
    }
    /// Task 29 step 2b: through this end's own loopback shim (RelayLeg or
    /// MediaTunnel). A genuine same-computer ICE peer may also be loopback.
    bool viaLoopbackShim() const
    {
        return ownedLoopbackShim;
    }
};

struct MediaTransportTelemetry {
    quint64 receivedDisplayPayloadBytes = 0;
    quint64 submittedDisplayPayloadBytes = 0;
    quint64 receivedRtpBytes = 0;
    quint64 submittedRtpBytes = 0;
    /// Received display messages this side discarded, oldest first, because
    /// more than kMaxPendingDisplayMessages (or their byte bound) were waiting
    /// for one drain. Each discarded message was already counted in
    /// receivedDisplayPayloadBytes, which measures arrival, not use.
    quint64 displayMessagesDropped = 0;
    // Application bytes on the dedicated keepalive and raw I/Q channels.
    // Receive counts precede local queue drops; submit counts precede the
    // library call and therefore do not prove delivery.
    quint64 receivedTxPayloadBytes = 0;
    quint64 submittedTxPayloadBytes = 0;
    quint64 receivedIqPayloadBytes = 0;
    quint64 submittedIqPayloadBytes = 0;
};

class IMediaTransport : public QObject {
    Q_OBJECT

public:
    enum class Role {
        Offerer,
        Answerer,
    };
    Q_ENUM(Role)

    // OpusAudioCodecConfig's default bitrate, named here so this interface
    // does not include the codec for one number; MediaPeer.cpp checks that
    // the two agree.
    static constexpr int kDefaultAudioTargetBitrate = 48000;

    struct StartOptions {
        Role role;
        // Per-session RTP routing identity. DTLS authenticates the peer;
        // this value is not an authentication token.
        quint32 localAudioSsrc;
        // The Opus encoder target this side sends at, bit/s (R-R3-23). An
        // offerer's audio description never advertises a higher average
        // bitrate than this. Defaults to the encoder's own default target,
        // so a caller that names only role and SSRC keeps today's offer.
        int audioTargetBitrate = kDefaultAudioTargetBitrate;
        // The canonical media generation for a routed WebSocket shim.
        QString connectionId {};
        // R-R3-23 lossless audio. An offerer adds the L16 rtpmap
        // (PcmAudioCodecConfig::kPayloadType, "L16/48000/2") to its one audio
        // m-line, after Opus, which stays first. Set only for a GUI that
        // declared it understands the lossless profile, so every other GUI
        // receives exactly today's offer. An answerer ignores it: its answer
        // follows the offer.
        bool offerLosslessAudio = false;
        // R-R3-43 receiver audio streams: the SSRCs of up to
        // kMaxReceiverAudioStreams further audio streams that share the one
        // audio m-line with the main stream. An offerer declares each with
        // its own a=ssrc line after the main one. Both roles' sendRtp()
        // accepts the main SSRC and exactly these, and a side's receive
        // queue holds kReceivedRtpPacketsPerStream packets for every
        // declared stream. Empty (the default) keeps today's offer, answer
        // and queue byte for byte. Set only for a GUI that asked for
        // receiver audio. Zero, the main SSRC, a repeat or more than
        // kMaxReceiverAudioStreams entries is a precondition refusal.
        QList<quint32> receiverAudioSsrcs {};
        // R-R3-45 headphones mix: the SSRC of one more audio stream on the
        // same m-line, the Core's mix of the receivers routed to the
        // headphones. 0 (the default) declares none and keeps today's
        // offer, answer and queue. Otherwise an offerer declares it with
        // its own a=ssrc line after the receiver streams, both roles'
        // sendRtp() accept it, and the receive queue holds
        // kReceivedRtpPacketsPerStream more packets. The main SSRC or a
        // receiver SSRC here is a precondition refusal.
        quint32 headphonesAudioSsrc = 0;
        // iPhone app plan Task 36 (R-IOS-13): the microphone line. 0 (the
        // default) declares none and keeps today's offer, answer and queue.
        // Otherwise an offerer adds a second audio m-line, mid "mic",
        // receive-only, after the main one: Opus (payload type 111,
        // micLineOpusFormatParameters()) and, with offerLosslessAudio, the
        // L16 rtpmap. An answerer takes that line send-only and declares
        // this SSRC on it (a=ssrc:<ssrc> cname:nereus-microphone), so the
        // offerer's library routes the line's packets to it. The receive
        // queue holds kReceivedRtpPacketsPerStream more packets. The main
        // SSRC, a receiver SSRC or the headphones SSRC here is a
        // precondition refusal.
        quint32 micAudioSsrc = 0;
        // iPhone app plan Task 37 (R-IOS-13): the transmit keepalive's
        // data channel, labelled "tx", unordered and never retransmitted
        // (maxRetransmits 0), so a lost keepalive is overtaken by the next
        // instead of holding anything behind it. False (the default) keeps
        // today's channels: an offerer creates it only when asked, and an
        // answerer takes one only when it was started with it (an answerer
        // from before refuses an unknown channel).
        bool txChannel = false;
        // Task 23: dedicated reliable ordered raw-I/Q channel, negotiated
        // only when the window declared remoteIqVersion 1.
        bool iqChannel = false;
        // iPhone app plan Task 27 (R-IOS-16): set for a connection that came
        // through the remote access service. Its one STUN server goes in at
        // once; automatic gathering is off, and gathering starts with the
        // relay servers once the credentials are known (at start() when
        // ice->relayKnown(), otherwise at gatherCandidates()); the MTU is
        // IceConfiguration::kMtuBytes; and candidates of every type are
        // taken from the far end, relay ones only when the relay is
        // allowed. Unset (the default): host candidates only, gathered at
        // once, exactly as before.
        std::optional<IceConfiguration> ice {};
    };

    /// Task 37: the largest message the "tx" channel carries.
    static constexpr qsizetype kMaxTxMessageBytes = 256;

    /// R-R3-43: the most receiver audio streams one media connection
    /// declares beside the main stream.
    static constexpr int kMaxReceiverAudioStreams = 4;
    /// R-R3-43, R-R3-05: received RTP packets a side holds between drains,
    /// per declared audio stream (the main stream plus each receiver
    /// stream). Lossless audio is 250 packets/s per stream (48 kHz, 192
    /// frames a packet), so 64 packets are 256 ms of cushion for every
    /// stream whether one stream or five are running.
    static constexpr int kReceivedRtpPacketsPerStream = 64;

    static constexpr qsizetype kMaxDescriptionBytes = 64 * 1024;
    static constexpr qsizetype kMaxCandidateBytes = 4 * 1024;
    static constexpr qsizetype kMaxCandidateMidBytes = 256;
    static constexpr qsizetype kMaxDisplayMessageBytes = 64 * 1024;
    static constexpr qsizetype kMaxIqMessageBytes = 24 + 1024 * 8;
    static constexpr qsizetype kMaxRawRtpBytes = 940;
    static constexpr qsizetype kMinRawRtpBytes = 12;
    static constexpr int kMaxRemoteCandidates = 64;
    static constexpr int kConfiguredMtuBytes = 1000;

    // Process-wide SCTP limits, applied once before the first peer in each
    // process (applyMediaSctpSettingsOnce() in the libdatachannel adapter).
    // Latest-value-wins at the producer: a slow link must refuse new display
    // frames, not hold seconds of stale ones. libdatachannel v0.24.5 defaults
    // both buffers to 1 MiB (src/impl/sctptransport.cpp:101-106).
    //
    // Send buffer: the floor is the 64 KiB maximum message, which PureSignal
    // display chunks need. libdatachannel raises SO_SNDBUF to that size on
    // every socket anyway (sctptransport.cpp:299-310), and usrsctp refuses a
    // message larger than the buffer (usrsctp fec583d5
    // sctp_output.c:14090-14096), so the buffer cannot be smaller.
    static constexpr int kSctpSendBufferBytes = 65536;
    // Receive buffer: twice the maximum message. usrsctp holds a message
    // until it reaches half the receive buffer before partial delivery
    // (sctp_indata.c:1078, 1131), so the window always admits a whole 64 KiB
    // message, and libdatachannel joins partial reads up to end of record
    // (sctptransport.cpp:498-533): every message reaches the callback whole.
    static constexpr int kSctpReceiveBufferBytes = 131072;
    // User bytes carried by one SCTP DATA chunk, which is one UDP datagram.
    // libdatachannel disables path MTU discovery and sets the SCTP path MTU
    // to 1000 - 12 (SCTP) - 48 (DTLS) - 8 (UDP) - 40 (IPv6) = 892
    // (sctptransport.cpp:247); usrsctp adds the 12-byte SCTP common header
    // back for its AF_CONN socket, 904 (sctp_pcb.c:4465-4481), and subtracts
    // that header plus the 16-byte DATA chunk header to fragment:
    // 904 - 12 - 16 = 876 (sctp_output.c:6856-6905). A display message of
    // n bytes therefore leaves as ceil(n / 876) datagrams.
    static constexpr int kSctpDataPayloadBytes = 876;

    static constexpr quint64 sctpFragmentCount(quint64 messageBytes)
    {
        return (messageBytes + kSctpDataPayloadBytes - 1) / kSctpDataPayloadBytes;
    }
    static_assert(kSctpSendBufferBytes >= kMaxDisplayMessageBytes,
                  "the SCTP send buffer must hold the largest display message");
    static_assert(kSctpReceiveBufferBytes >= 2 * kMaxDisplayMessageBytes,
                  "the SCTP receive buffer must deliver the largest message whole");

    explicit IMediaTransport(QObject* parent = nullptr) : QObject(parent) {}
    ~IMediaTransport() override = default;

    /// Returns false to refuse. A refusal that has emitted errorOccurred()
    /// first means the backend could not be built, which is transient and
    /// retried; a refusal without an error is a precondition (already
    /// started, an SSRC of zero) and is permanent (R-R3-28). MediaPeer reads
    /// the difference to type its own refusal.
    virtual bool start(const StartOptions& options) = 0;
    virtual void stop() = 0;

    virtual bool acceptDescription(const QString& sdp, const QString& type) = 0;
    virtual bool acceptCandidate(const QString& candidate, const QString& mid) = 0;

    /// What became of one display message handed to submitDisplay().
    enum class DisplaySendResult {
        /// The transport library passed it to SCTP at once.
        Sent,
        /// The library took it but holds it until SCTP has room; it is still
        /// sent, exactly once. At most one message is ever held this way.
        Queued,
        /// Not taken: the library still holds an earlier message. The caller
        /// may offer it (or a newer one) again after displayWritable().
        Busy,
        /// Not taken, and never will be: not ready, invalid, or failed.
        Refused,
    };
    Q_ENUM(DisplaySendResult)

    /// Send without waiting for transport backpressure. The adapter adds no
    /// application-side queue. True when the message was taken (Sent or
    /// Queued), so it will be delivered unless the network loses it.
    virtual bool sendDisplay(const QByteArray& message) = 0;
    /// sendDisplay() with the outcome spelled out. A transport without a
    /// library-side hold reports only Sent or Refused.
    virtual DisplaySendResult submitDisplay(const QByteArray& message)
    {
        return sendDisplay(message) ? DisplaySendResult::Sent : DisplaySendResult::Refused;
    }
    /// True while the library still holds a display message it took, so a
    /// new one would be Busy. displayWritable() follows when it clears.
    virtual bool displayBusy() const { return false; }
    virtual DisplaySendResult submitIq(const QByteArray& message)
    {
        Q_UNUSED(message);
        return DisplaySendResult::Refused;
    }
    virtual bool iqBusy() const { return false; }
    virtual bool sendRtp(const QByteArray& packet) = 0;
    /// Task 36: one RTP packet on the microphone line, which only an
    /// answerer started with micAudioSsrc sends on. False without that
    /// line, before ready, for a packet whose SSRC is not micAudioSsrc, and
    /// for the reasons sendRtp() gives.
    virtual bool sendMicRtp(const QByteArray& packet)
    {
        Q_UNUSED(packet);
        return false;
    }

    /// Task 37: one message on the "tx" data channel (either role). False
    /// without that channel, before it is open, and for an empty or
    /// oversized message. Never queued behind anything: it goes now or not
    /// at all.
    virtual bool sendTx(const QByteArray& message)
    {
        Q_UNUSED(message);
        return false;
    }

    /// TX mic thread (JJ approved 2026-10-01): where a transport with a
    /// thread of its own for the microphone line delivers that line's
    /// packets, on that thread, as they arrive: the packet and how long it
    /// waited since its receipt off the network, in microseconds. Such a
    /// packet is never also reported by micRtpReceived(). The sink must be
    /// safe on that thread and must not call back into the transport.
    using MicPacketSink = std::function<void(const QByteArray& packet, qint64 heldUs)>;
    /// Installs `sink` (an empty one goes back to micRtpReceived()). After
    /// it returns, the sink it replaced is never called again. False when
    /// this transport has no such thread (the line stays on
    /// micRtpReceived(), on the owner's thread). Owner's thread only.
    virtual bool setMicPacketSink(MicPacketSink sink)
    {
        Q_UNUSED(sink);
        return false;
    }

    virtual bool isReady() const = 0;

    /// Task 27: starts gathering, with these relay servers (none when the
    /// relay is not allowed or not offered), on a transport started with
    /// StartOptions::ice whose relay was not yet known. Once, and only then:
    /// false otherwise, and for a transport without that support.
    virtual bool gatherCandidates(const QList<IceRelayServer>& relays)
    {
        Q_UNUSED(relays);
        return false;
    }

    /// Task 27: the candidate pair in use once connected; nullopt before, or
    /// where the transport cannot say.
    virtual std::optional<MediaIcePath> selectedPath() const { return std::nullopt; }
    /// Control logging lane: the connection's SCTP round-trip estimate in
    /// milliseconds; nullopt before, or where the transport cannot say.
    /// For the log only.
    virtual std::optional<qint64> rttMs() const { return std::nullopt; }

    /// R-R3-23: true once both this side's description and the remote
    /// description carry the L16 rtpmap on the audio m-line, so lossless
    /// packets may be sent. False before negotiation and for a transport
    /// without the lossless profile.
    virtual bool losslessAudioNegotiated() const { return false; }
    /// Task 36: likewise for the microphone line: both descriptions carry
    /// the L16 rtpmap on it. False without a microphone line.
    virtual bool micLosslessNegotiated() const { return false; }

    /// Cumulative application payload bytes for the current transport start.
    /// Submitted values count preflight-valid calls into the transport
    /// library, including calls which return false or throw; they do not
    /// assert network delivery or SCTP queue acceptance. Unsupported
    /// transports return nullopt.
    virtual std::optional<MediaTransportTelemetry> telemetry() const
    {
        return std::nullopt;
    }

signals:
    void localDescription(const QString& sdp, const QString& type);
    void localCandidate(const QString& candidate, const QString& mid);
    /// Task 27: every local candidate has been reported; a connection
    /// through the remote access service sends the end of candidates.
    void gatheringComplete();
    void displayReceived(const QByteArray& message);
    void iqReceived(const QByteArray& message);
    void iqErrorOccurred(const QString& reason);
    void rtpReceived(const QByteArray& packet);
    /// Task 36: an RTP packet that arrived on the microphone line. Never
    /// also reported by rtpReceived(). TX stall lane: `heldUs` is how long
    /// the packet waited between its receipt off the network and this
    /// report (0 when the transport does not know), so a stall of the
    /// thread that reports it is not taken for the link's jitter.
    void micRtpReceived(const QByteArray& packet, qint64 heldUs = 0);
    /// Task 37: a message that arrived on the "tx" data channel. TX mic
    /// thread: `heldUs` is how long it waited since its receipt off the
    /// network (0 when the transport does not know), so the transmit
    /// watchdog judges a keepalive by when it came, not by when a stalled
    /// event loop reached it.
    void txReceived(const QByteArray& message, qint64 heldUs = 0);
    /// The connection is up and the display channel and audio line are
    /// open. Reported before any display message, raw I/Q message or audio
    /// packet that arrived with that opening, so a handler of the first one
    /// finds isReady() true (addendum G-127). Messages on the "tx" channel
    /// are reported in arrival order with the other events and may come
    /// before ready(); they do not depend on it (sendTx() needs only the tx
    /// channel open, never isReady()).
    void ready();
    void closed();
    /// The underlying peer connection entered a terminal transport-failure
    /// state.  Kept separate from validation/decoder errors so session
    /// recovery never depends on matching an error string.
    void connectionFailed(const QString& message);
    void errorOccurred(const QString& message);
    /// The display message the library held has gone to SCTP; the display
    /// channel takes a new message again.
    void displayWritable();
    /// An error on the display channel itself (a failed display send or the
    /// display channel's own error). Reported here only, not also through
    /// errorOccurred.
    void displayErrorOccurred(const QString& message);
};

} // namespace NereusSDR
