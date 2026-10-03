#pragma once
// =================================================================
// src/core/session/DataChannelTransport.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original.
//
// iPhone app plan Task 28 (R-IOS-16): the control session over a reliable,
// ordered data channel labelled "control", on a peer connection of its own
// (the remote design, section 10.4; the R5 decision brief's separate
// control-only ICE connection). It is how a paired device reaches the Core
// through the remote access service after an introduction (the rendezvous
// document, section 6.3), and it carries exactly the session the WebSocket
// carries: StationServer and StationClient hold it as a SessionTransport and
// run the same code from the first message on.
//
// The wire (the link document, "Control over a data channel"):
//
//   - Each session message is sent as one or more binary data-channel
//     messages ("chunks") of at most kMaxChunkBytes bytes each. A chunk's
//     first byte is kChunkMore (0x01, more of this message follows) or
//     kChunkLast (0x02, this chunk ends the message); the rest is the next
//     piece of the message's UTF-8 bytes, at least one byte. A sender fills
//     every chunk but the last to kMaxChunkBytes.
//   - A receiver joins the pieces and refuses a message whose bytes pass
//     its inbound cap (the station 1 MiB, the client 8 MiB, as on the
//     WebSocket, link section 12.3) the moment they pass it, and ends the
//     connection as the WebSocket's cap does. An empty data-channel
//     message, a chunk with no piece, a first byte it does not know, a
//     chunk longer than kMaxChunkBytes or a text data-channel message ends
//     the connection too.
//   - Ping and pong are kPingBytes long: kPing (0x10) or kPong (0x11), then
//     a 4-byte id (big-endian). A pong answers with the ping's id, at once,
//     without the session's code (the WebSocket's pong is the same). Either
//     may arrive between two chunks of a message and leaves the message
//     being joined untouched.
//
// The certificate: the Core's side (Answerer) presents its own persistent
// TLS certificate in the DTLS handshake (Options::certificatePemPath, the
// one its identity key binds, link section 3.4), so the SHA-256 the device
// sees in DTLS is the one the Core's hello binds. peerCertificateSha256()
// gives the certificate the far end actually presented in the handshake
// (libdatachannel's remoteFingerprint(), computed from the DTLS
// certificate), never the fingerprint the SDP claims, which arrives through
// the remote access service; StationClient checks it at the same gate as a
// WebSocket's, before it sends anything.
//
// Threading: libdatachannel calls back on its own threads. Chunks are
// joined there, under a lock, so the inbound cap holds before anything is
// queued for this object's thread; complete messages, pongs and state
// changes are then handed over with a queued call. A ping is answered from
// the library's thread, as a WebSocket stack answers one. A message that
// arrives before anything listens to textReceived (the Core sends its
// hello the moment its end opens, which can be before the device's end is
// handed to its session) is held, in order, until something does.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-26: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code.
//   2026-09-26: Task 28 fix wave (review Minors 3, 5, 7): queues bounded
//               by bytes, the close waits for the channel's close.
//               J.J. Boyd (KG4VCF), with AI-assisted implementation via
//               Anthropic Claude Code.
//   2026-09-27: setLibraryLogForTest() (R-R3-49). J.J. Boyd (KG4VCF),
//               with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-29: logRemoteCandidate(), for the opt-in ICE check log
//               (IceDiagnostics). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-09-27: iPhone app plan Task 29 step 2b (R-IOS-16, R-IOS-08): the
//               web relay's leg (RelayLeg) and its per-connection candidate
//               sources; the computer's own proxy settings (SystemProxy).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27: separate bounded transmit-watch DTLS channel; AI-assisted
//               implementation via OpenAI Codex for J.J. Boyd (KG4VCF).
//   2026-10-01: lingerTargetExistsForTest(). J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-01: Control logging lane: linkDiagnostics() and
//               deliveringMessageWaitUs(), for the Core's log only. J.J.
//               Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/IceConfiguration.h"
#include "core/session/SessionTransport.h"
#include "core/session/media/IMediaTransport.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {

class CandidateSourceLease;
class RelayLeg;

/// The chunking and the heartbeat bytes of the control data channel, apart
/// from any connection: what the transport runs and what the conformance
/// fixtures (tests/data/link/v1/framing/) check.
class ControlFraming {
public:
    /// The longest binary data-channel message the control channel carries:
    /// the first byte and at most kMaxChunkBytes - 1 bytes of the message.
    static constexpr qsizetype kMaxChunkBytes = 61440;
    static constexpr qsizetype kMaxChunkPayloadBytes = kMaxChunkBytes - 1;
    static constexpr quint8 kChunkMore = 0x01;
    static constexpr quint8 kChunkLast = 0x02;
    static constexpr quint8 kPing = 0x10;
    static constexpr quint8 kPong = 0x11;
    /// A ping or a pong: its first byte and a 4-byte id.
    static constexpr qsizetype kPingBytes = 5;

    /// The chunks of one session message, in order. Empty for an empty
    /// message (a session message never is).
    static QList<QByteArray> chunk(const QByteArray& message);
    static QByteArray ping(quint32 id);
    static QByteArray pong(quint32 id);

    /// Joins the chunks one connection receives.
    class Reassembler {
    public:
        enum class Result {
            Pending,  ///< a chunk that is not the last; nothing yet
            Message,  ///< `message` holds a whole session message
            Ping,     ///< `id` holds the ping's id
            Pong,     ///< `id` holds the pong's id
            Refused,  ///< the connection ends; `reason` says why (for the log)
        };

        explicit Reassembler(quint64 maxMessageBytes) : m_maxMessageBytes(maxMessageBytes) {}

        /// One binary data-channel message. After Refused every later call
        /// is Refused too.
        Result feed(const QByteArray& frame);

        QByteArray message() const { return m_message; }
        quint32 id() const { return m_id; }
        QString reason() const { return m_reason; }
        /// Bytes of the message being joined.
        qsizetype pendingBytes() const { return m_pending.size(); }

    private:
        Result refuse(const QString& reason);

        quint64 m_maxMessageBytes = 0;
        QByteArray m_pending;
        QByteArray m_message;
        quint32 m_id = 0;
        QString m_reason;
        bool m_refused = false;
    };
};

class DataChannelTransport : public SessionTransport {
    Q_OBJECT

public:
    enum class Role {
        Offerer, ///< the device: makes the offer and the channel
        Answerer, ///< the Core: answers, presenting its own certificate
    };
    enum class Purpose { Control, TxWatch };

    /// The channel's label (the link document, "Control over a data
    /// channel").
    static constexpr const char* kLabel = "control";
    static constexpr const char* kTxWatchLabel = "tx-watch-v1";
    static constexpr qsizetype kMaxWatchFrameBytes = 33;
    static constexpr qsizetype kMaxWatchQueuedFrames = 16;
    static constexpr qsizetype kMaxWatchQueuedBytes = 528;
    static constexpr std::size_t kMaxWatchEvents = 64;
    static constexpr std::size_t kMaxWatchOutboundBytes = 4096;

    /// NereusSDR's own bounds (the Task 28 safety review's Minor 3 and 7):
    ///
    /// The bytes of whole messages waiting for this object's thread, and
    /// separately the bytes held until something listens to textReceived,
    /// are each at most kMaxQueuedCaps times the inbound cap (4 MiB on the
    /// Core, 32 MiB on a device). Past either the owner has stopped keeping
    /// up and the connection ends, rather than grow without limit before
    /// anyone has signed in.
    static constexpr quint64 kMaxQueuedCaps = 4;
    /// closeLink() closes the channel and then the peer gracefully, and
    /// the connection is kept until the peer reports it has closed, or for
    /// at most this long, so what was already sent (a session.end behind a
    /// backlog) is delivered even when the transport is deleted at once,
    /// as a WebSocket's close waits for what it has written.
    static constexpr int kCloseDrainDeadlineMs = 5000;

    struct Options {
        Role role = Role::Offerer;
        /// Control preserves the original framing and candidate rules.
        /// TxWatch is a separate peer and one raw binary channel.
        Purpose purpose = Purpose::Control;
        /// The inbound cap: StationServer::kMaxIncomingMessageBytes on the
        /// Core, StationClient::kMaxIncomingMessageBytes on a device. Zero
        /// is refused, as WebSocketTransport requires a number. TxWatch
        /// requires exactly kMaxWatchFrameBytes.
        quint64 maxIncomingBytes = 0;
        /// Through the remote access service: one STUN server at once,
        /// gathering held until gatherCandidates() (or at start() when the
        /// relay is already known), the 996-byte MTU, every candidate type
        /// (IceConfiguration). Unset: host candidates only, gathered at
        /// once (a connection on one computer, as the tests make).
        std::optional<IceConfiguration> ice;
        /// The Answerer's certificate and key (PEM files): the Core's own
        /// TLS certificate (CertificateStore). Empty: a one-off certificate
        /// the library makes, which is what an Offerer uses.
        QString certificatePemPath;
        QString privateKeyPemPath;
    };

    explicit DataChannelTransport(QObject* parent = nullptr);
    ~DataChannelTransport() override;

    DataChannelTransport(const DataChannelTransport&) = delete;
    DataChannelTransport& operator=(const DataChannelTransport&) = delete;

    /// Builds the peer connection. An Offerer makes the channel and its
    /// offer (localDescription follows); an Answerer waits for
    /// acceptDescription(). False when already started, when the options
    /// are not usable or the library refuses them.
    bool start(const Options& options);
    /// The far end's description: an Offerer takes an "answer", an
    /// Answerer an "offer" (and answers). A description carrying
    /// candidates is refused: they come one at a time through
    /// acceptCandidate().
    bool acceptDescription(const QString& sdp, const QString& type);
    /// One candidate from the far end (`candidate:...`, no `a=`). Through
    /// the service, a relay candidate only when the relay is allowed; at
    /// most IMediaTransport::kMaxRemoteCandidates. Without Options::ice,
    /// host candidates only.
    bool acceptCandidate(const QString& candidate);
    /// Through the service: gathers once this side's description is made,
    /// with `configured`'s relay servers (after IceConfiguration::setRelay;
    /// none when the relay is not allowed), which iceConfiguration() then
    /// reports. `configured` is the start()'s settings with the relay
    /// added; its STUN server is not changed. Once only.
    bool gatherCandidates(const IceConfiguration& configured);

    /// Negotiated by this primary introduction only. The grant is opaque and
    /// must never be logged or transferred to a replacement primary.
    struct WatchRelayGrant {
        QUrl url;
        QString token;
        qint64 expires = 0;
        std::weak_ptr<RelayLeg> primaryLeg;
    };
    bool setWatchRelayGrant(const WatchRelayGrant& grant);
    const std::optional<WatchRelayGrant>& watchRelayGrant() const { return m_watchRelayGrant; }
    /// Existing admitted watch sessions may outlive their grant's admission
    /// expiry. Both checks still require this exact live primary relay route.
    bool hasWatchRelayRoute() const;
    bool canOpenWatchRelay() const;
    /// Test seam: the clock (seconds since the epoch) a watch relay grant's
    /// expiry is read against, so a test moves time past the expiry rather
    /// than waiting it out. Empty function: the wall clock.
    using WatchRelayClock = std::function<qint64()>;
    static void setWatchRelayClockForTest(WatchRelayClock clock);

    /// The candidate pair the connection settled on, once it has.
    std::optional<MediaIcePath> selectedPath() const;
    /// Test seam (Task 29 step 2a re-review, Minor 7): when set, every
    /// connection reports what this returns for it instead of the agent's
    /// pair. Empty function: the agent's own.
    using SelectedPathOverride =
        std::function<std::optional<MediaIcePath>(const DataChannelTransport*)>;
    static void setSelectedPathOverrideForTest(SelectedPathOverride override);
    /// Test seam: whether the object a lingering close's deadline timers
    /// live on exists. The application's teardown deletes it; one left
    /// after an application has gone is a leak.
    static bool lingerTargetExistsForTest();
    /// The ICE settings it was started with, the relay included once known.
    std::optional<IceConfiguration> iceConfiguration() const { return m_options.ice; }
    /// The ICE settings the session's media connection uses (the Task 28
    /// safety review's Important 4): the same STUN server, and this end's
    /// relay only when this connection's selected path goes through a
    /// relay, so a direct session takes no relay allocation for its media.
    /// The relay is kept while no path is selected. None without
    /// Options::ice (not through the service).
    std::optional<IceConfiguration> mediaIceConfiguration() const
    {
        if (m_options.purpose == Purpose::TxWatch) {
            return std::nullopt;
        }
        return mediaIceFor(m_options.ice, selectedPath());
    }
    /// mediaIceConfiguration()'s rule, apart from any connection.
    static std::optional<IceConfiguration> mediaIceFor(
        const std::optional<IceConfiguration>& control,
        const std::optional<MediaIcePath>& controlPath)
    {
        if (!control) {
            return std::nullopt;
        }
        // Task 29 step 2b: over the web relay the session's media may need
        // TURN too (and keeps the relay's leg, which every copy carries).
        if (controlPath && !controlPath->relayed() && !controlPath->viaLoopbackShim()) {
            return control->withoutOwnRelay();
        }
        return control;
    }
    Role role() const { return m_options.role; }

    // ---- SessionTransport ----
    void sendText(const QByteArray& wire) override;
    bool sendBinary(const QByteArray& message) override;
    bool carriesBinary() const override { return m_options.purpose == Purpose::TxWatch; }
    qint64 backlogBytes() const override;
    void ping() override;
    void closeLink(const QString& reason) override;
    bool isOpen() const override;
    QString peerDescription() const override;
    /// The far end's address on the selected pair, empty through a relay
    /// (a relayed connection has no address of the peer's own).
    QString peerAddress() const override;
    /// An Offerer: SHA-256 of the certificate the Core presented in the
    /// DTLS handshake (32 bytes), empty before it did. An Answerer: empty,
    /// as a WebSocket station sees no client certificate.
    QByteArray peerCertificateSha256() const override;
    std::optional<SessionTransportTelemetry> telemetry() const override;
    std::optional<NetworkPathSnapshot> networkPathSnapshot() const override;
    SessionLinkDiagnostics linkDiagnostics() const override;
    std::optional<qint64> deliveringMessageWaitUs() const override { return m_deliveringWaitUs; }

    // ---- Test seams ----

    /// While false, a ping arriving here gets no pong, so the far end sees
    /// a peer gone silent without closing (LoopbackTransport's
    /// setAnswersPings()). Default true.
    void setAnswersPingsForTest(bool answers);

    /// Test seam: what peerAddress() reports instead of the selected pair's
    /// address. The session conformance fixtures were traced over a link
    /// with no address of its own (what a relayed connection reports), and
    /// the runner's data-channel mode keeps them so on this computer.
    void setPeerAddressForTest(const std::optional<QString>& address)
    {
        m_peerAddressForTest = address;
    }

    /// What crossed this end, for a test waiting until both ends have
    /// handled everything the other sent.
    struct Counts {
        quint64 messagesSent = 0;
        quint64 messagesDelivered = 0;
        quint64 pingsSent = 0;
        quint64 pingsReceived = 0;
        quint64 pongsSent = 0;
        quint64 pongsReceived = 0;
        quint64 chunksSent = 0;
    };
    Counts countsForTest() const;

    /// Test seam: the bytes of whole messages waiting for this object's
    /// thread (the bound is kMaxQueuedCaps times the inbound cap).
    quint64 pendingBytesForTest() const;
    /// Gathered host candidates retained only for the local DTLS test shim;
    /// watch mode never emits them through localCandidate.
    QStringList localCandidatesForTest() const { return m_localCandidatesForTest; }
    /// Candidate-source lifetime regression: a closed peer cannot accrue
    /// another pending candidate or owned endpoint from its old lease.
    qsizetype pendingSourceCandidateCountForTest() const { return m_pendingSourceCandidates.size(); }
    qsizetype ownedShimEndpointCountForTest() const { return m_ownedShimEndpoints.size(); }

    /// Test seam: sends `frame` as one binary data-channel message exactly
    /// as given, to show what the far end does with frames a conforming
    /// sender never makes.
    bool sendRawFrameForTest(const QByteArray& frame);
    bool sendRawTextForTest(const QByteArray& frame);
    bool openUnexpectedChannelForTest(const QString& label, bool unordered);

signals:
    /// This end's description, for the far end (through the service: the
    /// offer of `introduce`, or the Core's `answer`).
    void localDescription(const QString& sdp, const QString& type);
    /// One of this end's candidates (`candidate:...`).
    void localCandidate(const QString& candidate);
    /// Every local candidate has been reported.
    void gatheringComplete();
    /// The channel opened: the session may start. StationServer and
    /// StationClient take the transport from here.
    void opened();
    /// The connection could not be made, or failed before it opened.
    /// `reason` is for the log. closed() follows once it had opened.
    void failed(const QString& reason);

public:
    /// What carries the library's callbacks to this object's thread
    /// (defined in the .cpp).
    struct Bridge;

protected:
    /// Messages that arrived before anything listened to textReceived go
    /// out, in order, once something does.
    void connectNotify(const QMetaMethod& signal) override;

private:
    void scheduleHeldDelivery();
    void deliverHeld();
    void drain();
    void handleOpen();
    void gatherIfReady();
    bool admitCandidate(const QString& candidate, bool fromOwnedSource);
    bool acceptOwnedWatchCandidate(const QString& candidate);
    /// A remote candidate, admitted or refused, to the opt-in ICE check log.
    void logRemoteCandidate(const QString& candidate, bool fromOwnedSource, bool admitted) const;
    /// Cancels the bridge and closes the connection; with `linger`, an open
    /// channel closes first and the peer after it (closeLink()).
    void stopPeer(bool linger = false);
    void finishClose();

    Options m_options;
    std::optional<WatchRelayGrant> m_watchRelayGrant;
    std::shared_ptr<Bridge> m_bridge;
    bool m_started = false;
    bool m_open = false;
    bool m_closing = false;
    bool m_closedEmitted = false;
    bool m_remoteDescriptionAccepted = false;
    bool m_localDescriptionEmitted = false;
    bool m_gatherRequested = false;
    bool m_gatheringStarted = false;
    QList<IceRelayServer> m_relays;
    int m_acceptedCandidates = 0;
    QList<QPair<QString, quint16>> m_farEndRelays;
    QList<QPair<QString, quint16>> m_ownedShimEndpoints;
    /// Step 2b: this connection's own candidate source on the control lane.
    std::shared_ptr<CandidateSourceLease> m_candidateSourceLease;
    QStringList m_pendingSourceCandidates;
    QStringList m_localCandidatesForTest;
    quint32 m_nextPingId = 1;
    SessionTransportTelemetry m_telemetry;
    QElapsedTimer m_pongAge;
    QElapsedTimer m_pingSentAt;
    quint64 m_messagesSent = 0;
    quint64 m_pingsSent = 0;
    quint64 m_chunksSent = 0;
    quint64 m_messagesDelivered = 0;
    quint64 m_pongsReceived = 0;
    std::optional<QString> m_peerAddressForTest;
    QList<QByteArray> m_held;
    quint64 m_heldBytes = 0;
    bool m_heldDeliveryPosted = false;
    /// Control logging lane: set only while textReceived() is emitted.
    std::optional<qint64> m_deliveringWaitUs;
};

} // namespace NereusSDR
