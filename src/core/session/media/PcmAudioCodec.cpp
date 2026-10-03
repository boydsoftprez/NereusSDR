// =================================================================
// src/core/session/media/PcmAudioCodec.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. See PcmAudioCodec.h.
//
// =================================================================

#include "core/session/media/PcmAudioCodec.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {
namespace {

bool finitePcm(const QVector<float>& pcm, qsizetype offset, qsizetype count)
{
    return std::all_of(pcm.cbegin() + offset, pcm.cbegin() + offset + count,
                       [](float sample) { return std::isfinite(sample); });
}

QByteArray l16Payload(const QVector<float>& pcm, qsizetype offset)
{
    constexpr qsizetype samples =
        PcmAudioCodecConfig::kPacketFrames * PcmAudioCodecConfig::kChannels;
    QByteArray payload(PcmAudioCodecConfig::kPayloadBytes, Qt::Uninitialized);
    for (qsizetype index = 0; index < samples; ++index) {
        const quint16 code = static_cast<quint16>(quantiseL16Sample(pcm.at(offset + index)));
        payload[index * 2] = static_cast<char>(code >> 8);
        payload[index * 2 + 1] = static_cast<char>(code & 0xff);
    }
    return payload;
}

} // namespace

const char* l16RtpMapEncoding()
{
    return "L16/48000/2";
}

PcmEncoderProfile l16EncoderProfile()
{
    return {PcmAudioCodecConfig::kSampleRate, PcmAudioCodecConfig::kChannels,
            PcmAudioCodecConfig::kPacketFrames, PcmAudioCodecConfig::kBitsPerSample,
            PcmAudioCodecConfig::kPayloadType};
}

qint16 quantiseL16Sample(float sample)
{
    const float clipped = std::clamp(sample, -1.0f, 1.0f);
    const long scaled = std::lround(static_cast<double>(clipped) * 32768.0);
    return static_cast<qint16>(std::clamp(scaled, -32768L, 32767L));
}

PcmRtpEncodeResult PcmAudioPacketiser::encode(const QVector<float>& pcmInterleaved,
                                              quint16 sequence, quint32 timestamp,
                                              quint32 ssrc) const
{
    constexpr qsizetype samples =
        PcmAudioCodecConfig::kPacketFrames * PcmAudioCodecConfig::kChannels;
    PcmRtpEncodeResult result;
    if (pcmInterleaved.size() != samples || !finitePcm(pcmInterleaved, 0, samples)) {
        return result;
    }
    result.packet = buildAudioRtp(PcmAudioCodecConfig::kPayloadType, sequence, timestamp,
                                  ssrc, l16Payload(pcmInterleaved, 0));
    result.status = OpusAudioCodecStatus::Accepted;
    return result;
}

QList<QByteArray> PcmAudioPacketiser::packetiseBlock(const QVector<float>& pcmInterleaved,
                                                     quint16 firstSequence,
                                                     quint32 firstTimestamp,
                                                     quint32 ssrc) const
{
    constexpr qsizetype blockSamples =
        PcmAudioCodecConfig::kBlockFrames * PcmAudioCodecConfig::kChannels;
    constexpr qsizetype packetSamples =
        PcmAudioCodecConfig::kPacketFrames * PcmAudioCodecConfig::kChannels;
    if (pcmInterleaved.size() != blockSamples
        || !finitePcm(pcmInterleaved, 0, blockSamples)) {
        return {};
    }
    QList<QByteArray> packets;
    packets.reserve(PcmAudioCodecConfig::kPacketsPerBlock);
    for (int index = 0; index < PcmAudioCodecConfig::kPacketsPerBlock; ++index) {
        // Unsigned arithmetic supplies normal RTP sequence/timestamp wrap.
        const quint16 sequence = static_cast<quint16>(firstSequence + index);
        const quint32 timestamp = firstTimestamp
            + static_cast<quint32>(index * PcmAudioCodecConfig::kPacketFrames);
        packets.append(buildAudioRtp(PcmAudioCodecConfig::kPayloadType, sequence, timestamp,
                                     ssrc, l16Payload(pcmInterleaved, index * packetSamples)));
    }
    return packets;
}

PcmRtpDecodeResult decodeL16Rtp(const QByteArray& packet, quint32 expectedSsrc)
{
    PcmRtpDecodeResult result;
    AudioRtpPacket parsed;
    result.status = parseAudioRtp(packet, PcmAudioCodecConfig::kPayloadType, parsed);
    if (result.status != OpusAudioCodecStatus::Accepted) {
        return result;
    }
    if (parsed.ssrc != expectedSsrc) {
        result.status = OpusAudioCodecStatus::UnexpectedSsrc;
        return result;
    }
    if (parsed.payload.size() != PcmAudioCodecConfig::kPayloadBytes) {
        result.status = OpusAudioCodecStatus::MalformedRtp;
        return result;
    }
    constexpr qsizetype samples =
        PcmAudioCodecConfig::kPacketFrames * PcmAudioCodecConfig::kChannels;
    result.pcmInterleaved.resize(samples);
    for (qsizetype index = 0; index < samples; ++index) {
        const quint16 code = static_cast<quint16>(
            (static_cast<quint16>(static_cast<quint8>(parsed.payload.at(index * 2))) << 8)
            | static_cast<quint8>(parsed.payload.at(index * 2 + 1)));
        result.pcmInterleaved[index] = static_cast<float>(static_cast<qint16>(code)) / 32768.0f;
    }
    result.sequence = parsed.sequence;
    result.timestamp = parsed.timestamp;
    result.status = OpusAudioCodecStatus::Accepted;
    return result;
}

} // namespace NereusSDR
