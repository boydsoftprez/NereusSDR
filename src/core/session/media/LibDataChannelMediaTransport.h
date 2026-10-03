#pragma once
// =================================================================
// src/core/session/media/LibDataChannelMediaTransport.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote-daemon R3 Task 1.
//
// libdatachannel v0.24.5 adapter for one direct DTLS/SCTP and SRTP peer.
// The implementation hides all rtc types so the dependency stays private
// to NereusCore.
//
// Modification history (NereusSDR):
//   2026-09-25: iPhone app plan Task 37 (R-IOS-13): the "tx" data channel
//               for the transmit keepalive, and the receive-severed test
//               seam. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//               Code.
//   2026-09-26: iPhone app plan Task 27 (R-IOS-16): ICE settings for a
//               connection that came through the remote access service
//               (StartOptions::ice): one STUN server, two relay servers once
//               the credentials are known, a 996-byte MTU, every candidate
//               type. Host candidates only otherwise, as before. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX mic thread (JJ approved): setMicPacketSink, the
//               microphone line on a thread of its own. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: TX diagnostics lane, review round: MicLineTimingWatch, the
//               microphone line's own "media RTP timing" warning, which
//               takes a long idle gap as the line starting again. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//
//   2026-10-01: Control logging lane: rttMs(). Logging only. J.J. Boyd
//               (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-01: Mic 48k lane: kMicLineMaxAverageBitrate, the microphone
//               line's 48 kbit/s ceiling. J.J. Boyd (KG4VCF), AI-assisted
//               via Anthropic Claude Code.
// =================================================================

#include "core/session/media/IMediaTransport.h"

#include <chrono>
#include <memory>
#include <optional>

namespace NereusSDR {

/// Applies the IMediaTransport::kSctp* buffer sizes to libdatachannel's
/// process-wide SCTP settings. Only the first call in a process applies them
/// and returns true. LibDataChannelMediaTransport::start() calls it before it
/// creates any peer, so the daemon and the GUI both get the limits before
/// their first media peer; calling it earlier is harmless.
bool applyMediaSctpSettingsOnce();

/// What applyMediaSctpSettingsOnce() did in this process: how many times the
/// settings were applied (0 or 1) and how many peers existed at that moment.
struct MediaSctpSettingsRecord {
    int applications = 0;
    quint64 peersCreatedBeforeApplication = 0;
    quint64 peersCreated = 0;
};
MediaSctpSettingsRecord mediaSctpSettingsRecord();

/// The Opus a=fmtp parameters a send-only offer carries for an encoder
/// running at `targetBitrate` bit/s (R-R3-23). Under RFC 7587 section 6.1
/// only sprop-stereo describes what the offer's author sends (stereo);
/// stereo, useinbandfec, maxaveragebitrate and minptime describe what the
/// author prefers to receive. The Core receives no audio, so those mirror
/// its encoder rather than claim more: stereo, 10 ms minimum packet time,
/// useinbandfec left at its default of 0 (the encoder has FEC off) and an
/// average bitrate equal to the configured target. The encoder is reported
/// by the minor-8 audio context. Revisit when the m-line becomes sendrecv
/// (TX audio, R4).
QString opusOfferFormatParameters(int targetBitrate);

/// Mic 48k lane: the microphone line's maxaveragebitrate, bit/s. Under RFC
/// 7587 section 6.1 it is the most the Core will receive on average, so it
/// bounds every microphone encoder on the line. 48000 is the highest
/// profile in the measured Opus table (OpusAudioCodec.h
/// kOpusMeasuredProfiles), JJ's ruling for the phone's microphone, and above
/// a desktop remote window's RemoteMicConfig::kOpusBitrate.
inline constexpr int kMicLineMaxAverageBitrate = 48'000;

/// iPhone app plan Task 36 (R-IOS-13): the Opus a=fmtp parameters of the
/// microphone line, which the Core receives: mono 48 kHz, in-band FEC, an
/// average of at most kMicLineMaxAverageBitrate, 10 ms minimum packet time
/// (RFC 7587 section 6.1: the Core's receive preferences, which bound the
/// device's microphone encoder).
QString micLineOpusFormatParameters();

/// TX diagnostics lane: when the microphone line warns of its timing, on
/// its own thread ("media RTP timing (microphone)"). Per batch the lane
/// takes: the largest gap between its packets' receipts, and the first
/// packet's wait for the lane; either past kThreshold warns, at most once a
/// kInterval. A gap past kIdleBound is the line starting again (the device
/// sends only while it transmits or VOX is armed), not a stall: it is not
/// counted and leaves the once-a-kInterval slot alone. The lane's thread
/// only; no lock, no allocation.
class MicLineTimingWatch {
public:
    using Clock = std::chrono::steady_clock;
    /// The owner's "media RTP timing" threshold and interval.
    static constexpr std::chrono::milliseconds kThreshold{80};
    static constexpr std::chrono::milliseconds kInterval{1000};
    /// 2 s: eight times the microphone buffer's 250 ms starvation point
    /// (RemoteMicFeed's kStarvationMs), which a gap inside a key has long
    /// passed by then, and the unkey line places it as an underrun; and
    /// shorter than a pause between two keys, which is seconds.
    static constexpr std::chrono::milliseconds kIdleBound{2000};

    struct Warning {
        qint64 callbackGapMs{0};
        qint64 laneWaitMs{0};
        int batchPackets{0};
    };

    void beginBatch() { m_largestGap = {}; }
    void noteReceipt(Clock::time_point receivedAt);
    /// The batch's warning, if it warns; `firstReceivedAt` is its first
    /// packet's receipt, `takenAt` when the lane took it.
    std::optional<Warning> endBatch(Clock::time_point firstReceivedAt, Clock::time_point takenAt,
                                    int packets);

private:
    Clock::time_point m_lastReceipt{};
    Clock::time_point m_lastWarning{};
    Clock::duration m_largestGap{};
};

class LibDataChannelMediaTransport final : public IMediaTransport {
    Q_OBJECT

public:
    enum class CandidatePolicy {
        HostOnly,
        AnyIceType,
    };

    explicit LibDataChannelMediaTransport(
        QObject* parent = nullptr,
        CandidatePolicy candidatePolicy = CandidatePolicy::HostOnly);
    ~LibDataChannelMediaTransport() override;

    bool start(const StartOptions& options) override;
    void stop() override;

    bool acceptDescription(const QString& sdp, const QString& type) override;
    bool acceptCandidate(const QString& candidate, const QString& mid) override;

    bool sendDisplay(const QByteArray& message) override;
    DisplaySendResult submitDisplay(const QByteArray& message) override;
    DisplaySendResult submitIq(const QByteArray& message) override;
    bool iqBusy() const override;
    bool displayBusy() const override;
    bool sendRtp(const QByteArray& packet) override;
    bool sendMicRtp(const QByteArray& packet) override;
    bool sendTx(const QByteArray& message) override;
    bool setMicPacketSink(MicPacketSink sink) override;

    bool isReady() const override;
    bool gatherCandidates(const QList<IceRelayServer>& relays) override;
    std::optional<MediaIcePath> selectedPath() const override;
    std::optional<qint64> rttMs() const override;
    bool losslessAudioNegotiated() const override;
    bool micLosslessNegotiated() const override;
    std::optional<MediaTransportTelemetry> telemetry() const override;

    /// Test seam: while stalled, the library thread that delivers received
    /// display messages waits instead of handing them over, so SCTP stops
    /// reading and the peer's send side fills. stop() always releases it.
    void setDisplayReceiveStalledForTest(bool stalled);
    /// Test seam (Task 37): while severed, everything that arrives (RTP on
    /// every line, display messages and the "tx" channel's messages) is
    /// dropped as it comes off the network, as if the path had gone dead
    /// without closing. Sending is unchanged.
    void setReceiveSeveredForTest(bool severed);

private:
    struct Private;
    std::unique_ptr<Private> d;

    void drainCallbacks();
    bool admitCandidate(const QString& candidate, const QString& mid, bool fromOwnedSource);
    /// Task 27: gathers once the local description is set and the relay is
    /// known.
    void gatherIfReady();
    void stopInternal(bool notify);
};

} // namespace NereusSDR
