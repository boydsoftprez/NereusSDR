#pragma once
// =================================================================
// src/core/session/media/MediaPeer.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R3 Task 1.
//
// Strict session signalling bridge for one authenticated media connection.
// The controller owns authentication and session epochs. This class carries
// only bounded SDP/candidate control and delegates media to IMediaTransport.
//
// Modification history (NereusSDR):
//   2026-09-25: iPhone app plan Task 37 (R-IOS-13): the "tx" data channel
//               with the microphone line (sendTx, txReceived). J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//
//   2026-09-26: iPhone app plan Task 28 (R-IOS-16): setIceConfiguration()
//               and usesIce(). J.J. Boyd (KG4VCF), with AI-assisted
//               implementation via Anthropic Claude Code.
//   2026-09-30: LINK minor 14: the private part is held by unique_ptr.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-09-30: TX stall lane: micRtpReceived carries heldUs, how long
//               the packet waited between its receipt in the transport
//               and its report, so the microphone buffer times it at
//               receipt. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-10-01: TX mic thread (JJ approved): setMicPacketSink, the
//               microphone line delivered on the transport's own thread
//               with this peer's checks; txReceived carries heldUs.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: Control logging lane: rttMs(). Logging only. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/media/IMediaTransport.h"

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>

#include <functional>
#include <memory>
#include <optional>

namespace NereusSDR {

// The reasons the Core gives in a whole-peer `rejected` (endpointId 0,
// revision 0) when it drops a media peer on its own, so the app starts
// media again at once instead of waiting for its own peer to time out.
// Both ends read them: the Core sends them (DaemonMediaController) and the
// desktop's remote window starts over on them (RemoteMediaController),
// where every other whole-peer refusal settles for good. Plain words: the
// app may show them as sent.
/// The connection failed: ICE consent lost or the DTLS handshake failed.
inline constexpr char kMediaPeerLostReason[] =
    "The Core lost the audio and display connection.";
/// The connection closed.
inline constexpr char kMediaPeerClosedReason[] =
    "The audio and display connection to the Core closed.";

struct MediaPeerTelemetry {
    quint64 generation = 0;
    MediaTransportTelemetry traffic;
};

class MediaPeer final : public QObject {
    Q_OBJECT

public:
    using TransportFactory = std::function<IMediaTransport*(QObject* parent)>;

    explicit MediaPeer(QObject* parent = nullptr,
                       TransportFactory factory = {});
    ~MediaPeer() override;

    // audioTargetBitrate is the Opus target this side sends at; an offerer's
    // audio description never advertises more (R-R3-23). An answerer sends
    // no audio and keeps the default.
    // offerLosslessAudio (R-R3-23): an offerer adds the L16 rtpmap to its
    // audio description; see IMediaTransport::StartOptions.
    // receiverAudioStreams (R-R3-43): both sides set it for a GUI that asked
    // for receiver audio. The offerer declares the
    // IMediaTransport::kMaxReceiverAudioStreams receiver SSRCs of this
    // connection beside the main one, and both sides send and accept
    // exactly that set. Off, every description and filter is today's.
    // headphonesMixStream (R-R3-45): both sides set it for a GUI that
    // declared it can play the Core's headphones mix. The offerer declares
    // headphonesAudioSsrcForConnection() after any receiver streams, and
    // both sides send and accept it too. Off, nothing changes.
    // micLine (iPhone app plan Task 36, R-IOS-13): both sides set it for a
    // client whose media start carried remoteTxVersion. The offerer adds the
    // receive-only microphone line and the answerer takes it, sending
    // micAudioSsrcForConnection() on it. Off, nothing changes.
    // Task 37: the same clients get the "tx" data channel for the transmit
    // keepalive (IMediaTransport::StartOptions::txChannel) with the line.
    bool start(IMediaTransport::Role role, const QString& connectionId,
               int audioTargetBitrate = IMediaTransport::kDefaultAudioTargetBitrate,
               bool offerLosslessAudio = false,
               bool receiverAudioStreams = false,
               bool headphonesMixStream = false,
               bool micLine = false,
               bool iqChannel = false);
    void stop();

    /// iPhone app plan Task 28 (R-IOS-16): for a session whose control
    /// connection came through the remote access service, the ICE settings
    /// the media connection uses too (the same STUN server and relay,
    /// IMediaTransport::StartOptions::ice). Applies from the next start();
    /// none (the default) keeps host candidates only.
    void setIceConfiguration(const std::optional<IceConfiguration>& ice);
    bool usesIce() const;
    /// Task 29 step 2b: ICE that gathers from a STUN or TURN server (the
    /// service's), which may take up to IceConfiguration::
    /// kGatheringDeadlineMs; the media tunnel's settings gather only host
    /// candidates and the tunnel's, at once.
    bool gathersFromServers() const;

    bool acceptControl(const QJsonObject& control);
    bool sendDisplay(const QByteArray& message);
    IMediaTransport::DisplaySendResult submitDisplay(const QByteArray& message);
    IMediaTransport::DisplaySendResult submitIq(const QByteArray& message);
    bool iqBusy() const;
    bool displayBusy() const;
    bool sendRtp(const QByteArray& packet);
    /// Task 36: one packet on the microphone line (the answerer's), whose
    /// SSRC must be micAudioSsrc().
    bool sendMicRtp(const QByteArray& packet);
    /// Task 37: one message on the "tx" data channel (only with the
    /// microphone line); now or not at all.
    bool sendTx(const QByteArray& message);

    /// Why the last start() returned false (R-R3-28, amended 2026-09-23).
    /// Only TransportConstructionFailed is transient and worth a retry;
    /// every other refusal is permanent.
    enum class StartRefusal {
        /// The last start() succeeded, or none has run.
        None,
        /// This peer is already started, or the connection id is not
        /// canonical.
        Precondition,
        /// The factory returned no transport, or one on another thread.
        InvalidTransport,
        /// The transport refused without reporting an error: a
        /// precondition of its own, such as an SSRC of zero.
        TransportRefused,
        /// The factory threw, or the transport reported an error and
        /// refused because it could not build its peer.
        TransportConstructionFailed,
    };
    StartRefusal lastStartRefusal() const;

    bool isReady() const;
    /// Task 29 step 2b: the candidate pair the connection settled on (for
    /// the harness and the log), nullopt before or where the transport
    /// cannot say.
    std::optional<MediaIcePath> selectedPath() const;
    /// Control logging lane: the transport's rttMs(), for the log only.
    std::optional<qint64> rttMs() const;
    /// Both descriptions carry the L16 rtpmap (R-R3-23).
    bool losslessAudioNegotiated() const;
    /// Task 36: both descriptions carry the L16 rtpmap on the microphone
    /// line.
    bool micLosslessNegotiated() const;
    QString connectionId() const;
    quint32 audioSsrc() const;
    /// R-R3-43: the declared receiver audio SSRCs, receiver 0 first; empty
    /// when receiver audio streams were not asked for or the peer is stopped.
    QList<quint32> receiverAudioSsrcs() const;

    /// R-R3-43: the SSRCs of receiver audio streams 0 to
    /// IMediaTransport::kMaxReceiverAudioStreams - 1 for a connection.
    /// Receiver n's is derived by SHA-256 over the ASCII
    /// "NereusSDR/media-receiver-ssrc/v1:<n>:" followed by the connection id,
    /// first four digest bytes big-endian. Zero, the main SSRC or an earlier
    /// receiver's SSRC is replaced by the next integer (wrapping) until it is
    /// none of these, so every id is distinct and both peers agree.
    static QList<quint32> receiverAudioSsrcsForConnection(const QString& connectionId);
    /// R-R3-45: the declared headphones mix SSRC, or 0 when the headphones
    /// mix was not asked for or the peer is stopped.
    quint32 headphonesAudioSsrc() const;
    /// R-R3-45: the headphones mix SSRC for a connection: SHA-256 over the
    /// ASCII "NereusSDR/media-headphones-ssrc/v1:" followed by the
    /// connection id, first four digest bytes big-endian. Zero, the main
    /// SSRC or any of the four receiver SSRCs is replaced by the next
    /// integer (wrapping) until it is none of these.
    static quint32 headphonesAudioSsrcForConnection(const QString& connectionId);
    /// Task 36: the microphone line's SSRC, or 0 without the line or while
    /// stopped.
    quint32 micAudioSsrc() const;
    /// Task 36: the microphone line's SSRC for a connection: SHA-256 over the
    /// ASCII "NereusSDR/media-mic-ssrc/v1:" followed by the connection id,
    /// first four digest bytes big-endian. Zero, the main SSRC, any of the
    /// four receiver SSRCs or the headphones SSRC is replaced by the next
    /// integer (wrapping) until it is none of these.
    static quint32 micAudioSsrcForConnection(const QString& connectionId);
    /// TX mic thread (JJ approved 2026-10-01): delivers the microphone
    /// line's packets to `sink` on the transport's own thread, with the
    /// checks micRtpReceived() has (the line's SSRC and size), for this
    /// start and every later one, until an empty sink is set. True when the
    /// current transport takes it; otherwise (or before a start) the line
    /// stays on micRtpReceived(). The sink must be safe on any thread.
    bool setMicPacketSink(IMediaTransport::MicPacketSink sink);
    std::optional<MediaPeerTelemetry> telemetry() const;

signals:
    void controlReady(const QJsonObject& control);
    void displayReceived(const QByteArray& message);
    void iqReceived(const QByteArray& message);
    void iqErrorOccurred(const QString& reason);
    void rtpReceived(const QByteArray& packet);
    /// Task 36: a packet on the microphone line carrying micAudioSsrc().
    /// TX stall lane: `heldUs` as IMediaTransport::micRtpReceived.
    void micRtpReceived(const QByteArray& packet, qint64 heldUs = 0);
    /// Task 37: a message on the "tx" data channel. TX mic thread:
    /// `heldUs` as IMediaTransport::txReceived.
    void txReceived(const QByteArray& message, qint64 heldUs = 0);
    void ready();
    void closed();
    void connectionFailed(const QString& message);
    void errorOccurred(const QString& message);
    void displayWritable();
    void displayErrorOccurred(const QString& message);

private:
    struct Private;
    // LINK minor 14: owned, not a raw pointer.
    std::unique_ptr<Private> d;

    bool isCurrent(const IMediaTransport* transport, quint64 generation) const;
    bool isDeclaredAudioSsrc(quint32 ssrc) const;
    void stopInternal(bool notify);
    /// TX mic thread: installs the stored sink on the current transport.
    bool installMicSink();
};

} // namespace NereusSDR
