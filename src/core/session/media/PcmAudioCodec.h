#pragma once
// =================================================================
// src/core/session/media/PcmAudioCodec.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote daemon R3 lossless audio
// (R-R3-23): uncompressed 16-bit stereo over RTP (RFC 3551 section 4.5.11,
// L16), carried beside the Opus profile. It owns no audio device, jitter
// queue, transport, or session identity.
//
// =================================================================

#include "core/session/media/OpusAudioCodec.h"

#include <QByteArray>
#include <QList>
#include <QVector>

namespace NereusSDR {

/// The one lossless profile: 48 kHz, stereo, 16-bit big-endian samples,
/// interleaved left then right (RFC 3551 L16 network byte order).
struct PcmAudioCodecConfig {
    static constexpr int kSampleRate = 48'000;
    static constexpr int kChannels = 2;
    static constexpr int kBitsPerSample = 16;
    static constexpr int kBytesPerFrame = kChannels * kBitsPerSample / 8;
    /// Frames per packet: 4 ms. 192 stereo frames are 768 payload bytes and
    /// a 780-byte packet, under the shared 940-byte RTP cap.
    static constexpr int kPacketFrames = 192;
    static constexpr int kPayloadBytes = kPacketFrames * kBytesPerFrame;
    static constexpr int kRtpPacketBytes = OpusAudioCodecConfig::kRtpHeaderBytes + kPayloadBytes;
    /// The Core's capture block is the Opus frame, 1920 frames (40 ms); a
    /// block is exactly ten L16 packets.
    static constexpr int kBlockFrames = OpusAudioCodecConfig::kFrameSamples;
    static constexpr int kPacketsPerBlock = kBlockFrames / kPacketFrames;
    /// Dynamic RTP payload type (RFC 3551 section 3); 48 kHz stereo L16 has
    /// no static type. Named in the SDP rtpmap on the Opus m-line.
    static constexpr int kPayloadType = 96;

    static_assert(kRtpPacketBytes <= OpusAudioCodecConfig::kMaxRtpPacketBytes,
                  "an L16 packet must fit the shared RTP cap");
    static_assert(kPayloadBytes <= OpusAudioCodecConfig::kMaxPayloadBytes,
                  "an L16 payload must fit the shared payload cap");
    static_assert(kBlockFrames % kPacketFrames == 0,
                  "a capture block must be a whole number of L16 packets");
    static_assert(kPayloadType != OpusAudioCodecConfig::kPayloadType,
                  "L16 and Opus need distinct payload types");
};

/// Which of the two remote audio profiles the Core sends: Opus (the default,
/// OpusAudioCodec.h) or Lossless (L16, this file). RemoteAudioContext.h
/// carries the wire names "opus" and "lossless".
enum class RemoteAudioProfile { Opus, Lossless };

/// The SDP rtpmap encoding for this profile, "L16/48000/2".
const char* l16RtpMapEncoding();

/// The lossless profile the Core runs, as it reports it to a GUI.
struct PcmEncoderProfile {
    int sampleRate {0};    // RTP clock, Hz
    int channels {0};
    int frameSamples {0};  // frames per channel per packet
    int bitsPerSample {0};
    int payloadType {0};   // dynamic RTP payload type the packets carry
    friend bool operator==(const PcmEncoderProfile&, const PcmEncoderProfile&) = default;
};

/// The profile this build's packetiser produces.
PcmEncoderProfile l16EncoderProfile();

/// Quantises one float sample to 16 bits: clipped to +-1, scaled by 32768,
/// rounded to nearest, and held at 32767 at the top (+1.0 has no 16-bit
/// code). Decoding is sample / 32768, so every 16-bit code round-trips.
qint16 quantiseL16Sample(float sample);

struct PcmRtpEncodeResult {
    OpusAudioCodecStatus status {OpusAudioCodecStatus::InvalidInput};
    QByteArray packet;
};

struct PcmRtpDecodeResult {
    OpusAudioCodecStatus status {OpusAudioCodecStatus::InvalidInput};
    quint16 sequence {0};
    quint32 timestamp {0};
    QVector<float> pcmInterleaved; // kPacketFrames * kChannels samples
};

/// Stateless L16 packetiser. Caller owns sequence, timestamp and SSRC, as
/// with OpusAudioEncoder.
class PcmAudioPacketiser {
public:
    /// Always true: the profile needs no library. Kept so the Core's
    /// admission reads the same way for both profiles.
    bool isReady() const { return true; }
    PcmEncoderProfile profile() const { return l16EncoderProfile(); }

    /// One packet of exactly kPacketFrames interleaved stereo frames.
    /// InvalidInput for a wrong length or any non-finite sample.
    PcmRtpEncodeResult encode(const QVector<float>& pcmInterleaved, quint16 sequence,
                              quint32 timestamp, quint32 ssrc) const;

    /// One capture block (kBlockFrames interleaved stereo frames) as
    /// kPacketsPerBlock packets: sequence firstSequence + i (wrapping),
    /// timestamp firstTimestamp + i * kPacketFrames (wrapping). Empty for a
    /// wrong length or any non-finite sample.
    QList<QByteArray> packetiseBlock(const QVector<float>& pcmInterleaved,
                                     quint16 firstSequence, quint32 firstTimestamp,
                                     quint32 ssrc) const;
};

/// Decodes one L16 packet. MalformedRtp for a wrong payload type, a payload
/// that is not exactly kPayloadBytes, or any RTP framing fault; Oversized
/// above the 940-byte cap; UnexpectedSsrc for another SSRC.
PcmRtpDecodeResult decodeL16Rtp(const QByteArray& packet, quint32 expectedSsrc);

} // namespace NereusSDR
