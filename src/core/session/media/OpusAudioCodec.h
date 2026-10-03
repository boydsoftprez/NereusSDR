#pragma once
// =================================================================
// src/core/session/media/OpusAudioCodec.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote daemon R3 Task 5. Uses the
// existing RADE-pinned Opus source; it owns no audio device, jitter queue,
// transport, or session identity.
//
// =================================================================

#include <QByteArray>
#include <QVector>

#include <array>
#include <memory>
#include <optional>

namespace NereusSDR {

struct OpusAudioCodecConfig {
    static constexpr int kSampleRate = 48'000;
    static constexpr int kChannels = 2;
    static constexpr int kFrameSamples = 1'920; // 40 ms at 48 kHz
    static constexpr int kPayloadType = 111;
    static constexpr int kRtpHeaderBytes = 12;
    static constexpr int kMaxRtpPacketBytes = 940;
    static constexpr int kMaxPayloadBytes = kMaxRtpPacketBytes - kRtpHeaderBytes;

    // R-R3-21: 48 kbit/s fullband is the default for every mode (operator
    // decision 2026-09-26); 24 kbit/s wideband stays accepted.
    int bitrate {48'000};
};

/// iPhone app plan Task 23 (R-IOS-09, R-R3-23): the Opus profiles the
/// station has measured, and so offers a device, in the order of the
/// catalogue's `audio.opusProfiles`. Each row is a run of
/// docs/architecture/2026-09-20-remote-daemon-r3-verification/
/// opus-profile-probe.txt (the R-R3-23 product profile against the pinned
/// Opus): 24000 bit/s forced WIDEBAND (1103, sound to 8 kHz), 48000 bit/s
/// forced FULLBAND (1105, sound to 20 kHz), every packet's TOC bandwidth
/// as forced. A device's `opusBitrate` must be one of these.
struct OpusMeasuredProfile {
    int bitrate {0};
    int bandwidthHz {0};
};
inline constexpr std::array<OpusMeasuredProfile, 2> kOpusMeasuredProfiles{{
    {24'000, 8'000},   // opus-profile-probe.txt: bitrate=24000 forced_bandwidth=1103
    {48'000, 20'000},  // opus-profile-probe.txt: bitrate=48000 forced_bandwidth=1105
}};
constexpr bool isMeasuredOpusBitrate(int bitrate)
{
    for (const OpusMeasuredProfile& profile : kOpusMeasuredProfiles) {
        if (profile.bitrate == bitrate) {
            return true;
        }
    }
    return false;
}

/// The coded audio bandwidth the encoder forces for a supported target
/// bitrate, as an Opus OPUS_BANDWIDTH_* value: 24000 bit/s codes wideband
/// (sound up to 8 kHz), 48000 bit/s codes fullband (sound up to 20 kHz).
/// Returns 0 for any other bitrate, which the encoder and decoder refuse.
int bandwidthForBitrate(int bitrate);

enum class OpusAudioCodecStatus {
    Accepted,
    Concealed,
    InvalidInput,
    EncodeFailed,
    DecodeFailed,
    MalformedRtp,
    UnexpectedSsrc,
    Oversized,
};

// ---- Profile-neutral RTP audio framing (R-R3-23) ----
// Opus and the lossless L16 profile (PcmAudioCodec.h) share one RTP
// boundary: the same 940-byte packet cap, the same RTP version 2 header and
// the same CSRC, extension and padding rules. Only the payload type and the
// payload differ, so a receiver reads the payload type first and hands the
// packet to the matching decoder.

/// One validated RTP audio packet: header fields and the payload alone.
struct AudioRtpPacket {
    int payloadType {0};
    quint16 sequence {0};
    quint32 timestamp {0};
    quint32 ssrc {0};
    QByteArray payload; // CSRC, extension and padding bytes excluded
};

/// The payload type of an RTP version 2 packet, or -1 when the packet is
/// shorter than the fixed header or not version 2. Reads nothing else.
int audioRtpPayloadType(const QByteArray& packet);

/// Validates the RTP boundary for `payloadType`: at most
/// OpusAudioCodecConfig::kMaxRtpPacketBytes (Oversized otherwise), version 2,
/// that payload type, well-formed CSRC, extension and padding, and a
/// non-empty payload no larger than kMaxPayloadBytes (MalformedRtp
/// otherwise). The SSRC is reported, not checked.
OpusAudioCodecStatus parseAudioRtp(const QByteArray& packet, int payloadType,
                                   AudioRtpPacket& parsed);

/// The header fields and where the payload lies, without copying it.
struct AudioRtpView {
    int payloadType {0};
    quint16 sequence {0};
    quint32 timestamp {0};
    quint32 ssrc {0};
    qsizetype payloadOffset {0};
    qsizetype payloadBytes {0}; // CSRC, extension and padding bytes excluded
};

/// The same checks as parseAudioRtp, reporting the payload's place in
/// `packet` instead of a copy, so a receiver can check a packet's shape per
/// packet without allocating.
OpusAudioCodecStatus inspectAudioRtpHeader(const QByteArray& packet, int payloadType,
                                           AudioRtpView& view);

/// A 12-byte RTP version 2 header (no CSRC, extension, padding or marker)
/// followed by `payload`.
QByteArray buildAudioRtp(int payloadType, quint16 sequence, quint32 timestamp,
                         quint32 ssrc, const QByteArray& payload);

struct OpusPacketInfo {
    int channels {0};
    int bandwidth {0}; // Opus OPUS_BANDWIDTH_* value
    int samplesPerChannel {0};
};

struct OpusRtpEncodeResult {
    OpusAudioCodecStatus status {OpusAudioCodecStatus::InvalidInput};
    QByteArray packet;
    OpusPacketInfo packetInfo;
};

struct OpusRtpDecodeResult {
    OpusAudioCodecStatus status {OpusAudioCodecStatus::InvalidInput};
    quint16 sequence {0};
    quint32 timestamp {0};
    QVector<float> pcmInterleaved;
    OpusPacketInfo packetInfo;
};

struct OpusRtpInspection {
    OpusAudioCodecStatus status {OpusAudioCodecStatus::InvalidInput};
    quint16 sequence {0};
    quint32 timestamp {0};
    // Validated Opus payload only: RTP CSRC, extension and padding bytes are
    // excluded by the parser before this observer-facing value is published.
    qsizetype payloadBytes {0};
    OpusPacketInfo packetInfo;
};

// Validates the same RTP/profile boundary as decodeRtp without advancing
// decoder history, so packets can be ordered before decoding.
OpusRtpInspection inspectOpusRtp(const QByteArray& packet, quint32 expectedSsrc);

/// The profile an encoder is actually running, as Core reports it to a GUI.
struct OpusEncoderProfile {
    int sampleRate {0};       // RTP clock and decoder rate, Hz
    int channels {0};
    int frameSamples {0};     // per channel per packet
    int targetBitrate {0};    // constrained-VBR encoder target, bit/s; not measured traffic
    int audioBandwidthHz {0}; // coded audio bandwidth limit
    friend bool operator==(const OpusEncoderProfile&, const OpusEncoderProfile&) = default;
};

/// RAII encoder for the approved 48 kHz, stereo, 40 ms AUDIO/MUSIC profile.
/// Caller owns the RTP sequence, timestamp, and SSRC/session generation.
class OpusAudioEncoder {
public:
    explicit OpusAudioEncoder(const OpusAudioCodecConfig& config = {});
    ~OpusAudioEncoder();
    OpusAudioEncoder(const OpusAudioEncoder&) = delete;
    OpusAudioEncoder& operator=(const OpusAudioEncoder&) = delete;

    bool isReady() const;
    /// Sample rate and target bitrate are read back from libopus. The coded
    /// bandwidth is the forced bandwidth the constructor configured from
    /// bandwidthForBitrate():
    /// OPUS_GET_BANDWIDTH describes the last encoded frame instead, and reads
    /// FULLBAND after construction and after reset(), which is exactly when
    /// Core announces a new audio context. Valid immediately after
    /// construction or reset(), before any encode.
    std::optional<OpusEncoderProfile> profile() const; // nullopt when !isReady()
    /// R-R3-35: the codec's algorithmic delay in frames at 48 kHz, as libopus
    /// reports it (OPUS_GET_LOOKAHEAD). A sample given to encode() comes out
    /// of the decoder this many frames later on the RTP clock. 0 when
    /// !isReady().
    int lookaheadFrames() const;
    OpusRtpEncodeResult encode(const QVector<float>& pcmInterleaved,
                               quint16 sequence, quint32 timestamp,
                               quint32 ssrc);
    void reset();

private:
    struct State;
    std::unique_ptr<State> m_state;
};

/// R-R3-35: the algorithmic delay, in frames at 48 kHz, of the Opus audio
/// the Core sends: lookaheadFrames() of an encoder built like the Core's
/// (the delay does not depend on the bitrate). Computed once; 0 only when
/// libopus cannot build that encoder.
int opusCodecDelayFrames();

/// RAII decoder. `decodeMissing()` is explicit packet-loss concealment; a bad
/// RTP packet is never converted into PLC by this boundary.
class OpusAudioDecoder {
public:
    explicit OpusAudioDecoder(const OpusAudioCodecConfig& config = {});
    ~OpusAudioDecoder();
    OpusAudioDecoder(const OpusAudioDecoder&) = delete;
    OpusAudioDecoder& operator=(const OpusAudioDecoder&) = delete;

    bool isReady() const;
    OpusRtpDecodeResult decodeRtp(const QByteArray& packet, quint32 expectedSsrc);
    OpusRtpDecodeResult decodeMissing();
    void reset();

private:
    struct State;
    std::unique_ptr<State> m_state;
};

} // namespace NereusSDR
