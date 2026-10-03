// =================================================================
// src/core/session/media/OpusAudioCodec.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote daemon R3 Task 5.
// Profile values are pinned by
// docs/architecture/2026-09-20-remote-daemon-r3-verification/opus-profile-probe.c
// against Opus 940d4e5af64351ca8ba8390df3f555484c567fbb: 24 kbit/s codes
// wideband (8 kHz) and 48 kbit/s codes fullband (20 kHz), stereo, 40 ms.
//
// =================================================================

#include "core/session/media/OpusAudioCodec.h"

#include <opus.h>

#include <algorithm>
#include <cmath>
#include <memory>

namespace NereusSDR {
namespace {

int audioBandwidthHz(opus_int32 bandwidth)
{
    switch (bandwidth) {
    case OPUS_BANDWIDTH_NARROWBAND:
        return 4'000;
    case OPUS_BANDWIDTH_MEDIUMBAND:
        return 6'000;
    case OPUS_BANDWIDTH_WIDEBAND:
        return 8'000;
    case OPUS_BANDWIDTH_SUPERWIDEBAND:
        return 12'000;
    case OPUS_BANDWIDTH_FULLBAND:
        return 20'000;
    default:
        return 0;
    }
}

bool validConfig(const OpusAudioCodecConfig& config)
{
    return bandwidthForBitrate(config.bitrate) != 0;
}

bool finitePcm(const QVector<float>& pcm)
{
    return std::all_of(pcm.cbegin(), pcm.cend(), [](float sample) {
        return std::isfinite(sample);
    });
}

void appendU16(QByteArray& bytes, quint16 value)
{
    bytes.append(static_cast<char>(value >> 8));
    bytes.append(static_cast<char>(value));
}

void appendU32(QByteArray& bytes, quint32 value)
{
    bytes.append(static_cast<char>(value >> 24));
    bytes.append(static_cast<char>(value >> 16));
    bytes.append(static_cast<char>(value >> 8));
    bytes.append(static_cast<char>(value));
}

quint16 readU16(const QByteArray& bytes, int offset)
{
    return (static_cast<quint16>(static_cast<unsigned char>(bytes.at(offset))) << 8)
        | static_cast<unsigned char>(bytes.at(offset + 1));
}

quint32 readU32(const QByteArray& bytes, int offset)
{
    return (static_cast<quint32>(static_cast<unsigned char>(bytes.at(offset))) << 24)
        | (static_cast<quint32>(static_cast<unsigned char>(bytes.at(offset + 1))) << 16)
        | (static_cast<quint32>(static_cast<unsigned char>(bytes.at(offset + 2))) << 8)
        | static_cast<unsigned char>(bytes.at(offset + 3));
}

OpusPacketInfo packetInfo(const QByteArray& payload)
{
    OpusPacketInfo info;
    info.channels = opus_packet_get_nb_channels(
        reinterpret_cast<const unsigned char*>(payload.constData()));
    info.bandwidth = opus_packet_get_bandwidth(
        reinterpret_cast<const unsigned char*>(payload.constData()));
    info.samplesPerChannel = opus_packet_get_nb_samples(
        reinterpret_cast<const unsigned char*>(payload.constData()), payload.size(),
        OpusAudioCodecConfig::kSampleRate);
    return info;
}

bool validPacketInfo(const OpusPacketInfo& info)
{
    return info.channels == OpusAudioCodecConfig::kChannels
        && info.samplesPerChannel == OpusAudioCodecConfig::kFrameSamples
        && info.bandwidth > OPUS_BANDWIDTH_NARROWBAND;
}

} // namespace

// R-R3-23: the coded bandwidth each supported target forces, passed to
// OPUS_SET_BANDWIDTH and reported by OpusAudioEncoder::profile(). libopus
// has no getter for a forced bandwidth (OPUS_GET_BANDWIDTH reports the last
// encoded frame), so profile() reports this value and the codec tests check
// it against the TOC bandwidth of every packet the encoder produces.
int bandwidthForBitrate(int bitrate)
{
    switch (bitrate) {
    case 24'000:
        return OPUS_BANDWIDTH_WIDEBAND;
    case 48'000:
        return OPUS_BANDWIDTH_FULLBAND;
    default:
        return 0;
    }
}

int audioRtpPayloadType(const QByteArray& packet)
{
    if (packet.size() < OpusAudioCodecConfig::kRtpHeaderBytes
        || (static_cast<quint8>(packet.at(0)) >> 6) != 2) {
        return -1;
    }
    return static_cast<quint8>(packet.at(1)) & 0x7f;
}

OpusAudioCodecStatus parseAudioRtp(const QByteArray& packet, int payloadType,
                                   AudioRtpPacket& parsed)
{
    AudioRtpView view;
    const OpusAudioCodecStatus status = inspectAudioRtpHeader(packet, payloadType, view);
    if (status != OpusAudioCodecStatus::Accepted) {
        return status;
    }
    parsed.payloadType = view.payloadType;
    parsed.sequence = view.sequence;
    parsed.timestamp = view.timestamp;
    parsed.ssrc = view.ssrc;
    parsed.payload = packet.mid(view.payloadOffset, view.payloadBytes);
    return OpusAudioCodecStatus::Accepted;
}

OpusAudioCodecStatus inspectAudioRtpHeader(const QByteArray& packet, int payloadType,
                                           AudioRtpView& view)
{
    if (packet.size() > OpusAudioCodecConfig::kMaxRtpPacketBytes) {
        return OpusAudioCodecStatus::Oversized;
    }
    if (packet.size() < OpusAudioCodecConfig::kRtpHeaderBytes) {
        return OpusAudioCodecStatus::MalformedRtp;
    }
    const quint8 first = static_cast<unsigned char>(packet.at(0));
    const quint8 second = static_cast<unsigned char>(packet.at(1));
    if ((first >> 6) != 2 || (second & 0x7f) != payloadType) {
        return OpusAudioCodecStatus::MalformedRtp;
    }
    int headerBytes = OpusAudioCodecConfig::kRtpHeaderBytes + (first & 0x0f) * 4;
    if (headerBytes > packet.size()) {
        return OpusAudioCodecStatus::MalformedRtp;
    }
    if ((first & 0x10) != 0) {
        if (packet.size() - headerBytes < 4) {
            return OpusAudioCodecStatus::MalformedRtp;
        }
        const quint16 extensionWords = readU16(packet, headerBytes + 2);
        const int extensionBytes = 4 + static_cast<int>(extensionWords) * 4;
        if (extensionBytes > packet.size() - headerBytes) {
            return OpusAudioCodecStatus::MalformedRtp;
        }
        headerBytes += extensionBytes;
    }
    int payloadBytes = packet.size() - headerBytes;
    if ((first & 0x20) != 0) {
        if (payloadBytes == 0) {
            return OpusAudioCodecStatus::MalformedRtp;
        }
        const int padding = static_cast<unsigned char>(packet.back());
        if (padding <= 0 || padding > payloadBytes) {
            return OpusAudioCodecStatus::MalformedRtp;
        }
        payloadBytes -= padding;
    }
    if (payloadBytes <= 0 || payloadBytes > OpusAudioCodecConfig::kMaxPayloadBytes) {
        return OpusAudioCodecStatus::MalformedRtp;
    }
    view.payloadType = payloadType;
    view.sequence = readU16(packet, 2);
    view.timestamp = readU32(packet, 4);
    view.ssrc = readU32(packet, 8);
    view.payloadOffset = headerBytes;
    view.payloadBytes = payloadBytes;
    return OpusAudioCodecStatus::Accepted;
}

QByteArray buildAudioRtp(int payloadType, quint16 sequence, quint32 timestamp,
                         quint32 ssrc, const QByteArray& payload)
{
    QByteArray packet;
    packet.reserve(OpusAudioCodecConfig::kRtpHeaderBytes + payload.size());
    packet.append(static_cast<char>(0x80)); // V2, no CSRC/extension/padding
    packet.append(static_cast<char>(payloadType & 0x7f));
    appendU16(packet, sequence);
    appendU32(packet, timestamp);
    appendU32(packet, ssrc);
    packet.append(payload);
    return packet;
}

OpusRtpInspection inspectOpusRtp(const QByteArray& packet, quint32 expectedSsrc)
{
    OpusRtpInspection result;
    AudioRtpPacket parsed;
    result.status = parseAudioRtp(packet, OpusAudioCodecConfig::kPayloadType, parsed);
    if (result.status != OpusAudioCodecStatus::Accepted) { return result; }
    if (parsed.ssrc != expectedSsrc) {
        result.status = OpusAudioCodecStatus::UnexpectedSsrc;
        return result;
    }
    result.packetInfo = packetInfo(parsed.payload);
    if (!validPacketInfo(result.packetInfo)) {
        result.status = OpusAudioCodecStatus::MalformedRtp;
        return result;
    }
    result.sequence = parsed.sequence;
    result.timestamp = parsed.timestamp;
    result.payloadBytes = parsed.payload.size();
    return result;
}

struct OpusAudioEncoder::State {
    OpusEncoder* encoder {nullptr};
    OpusAudioCodecConfig config;
    opus_int32 bandwidth {0}; // forced OPUS_BANDWIDTH_* for config.bitrate

    ~State() { opus_encoder_destroy(encoder); }
};

struct OpusAudioDecoder::State {
    OpusDecoder* decoder {nullptr};
    OpusAudioCodecConfig config;

    ~State() { opus_decoder_destroy(decoder); }
};

OpusAudioEncoder::OpusAudioEncoder(const OpusAudioCodecConfig& config)
{
    if (!validConfig(config)) {
        return;
    }
    // R-R3-21: in-band FEC stays off (operator decision 2026-09-26), read
    // against the pinned Opus source (940d4e5):
    // - include/opus_defines.h, OPUS_SET_INBAND_FEC: FEC "is only applicable
    //   to the LPC layer" (SILK); value 1 makes Opus "switch to SILK even at
    //   high rates", value 2 does not switch for music and so carries none
    //   with OPUS_SIGNAL_MUSIC.
    // - src/opus_encoder.c, mode decision: with FEC on and the expected loss
    //   above (128 - voice_est) >> 4 (8 percent for music), every packet goes
    //   SILK at wideband and hybrid at fullband. Measured at 48 kbit/s, a
    //   15 kHz tone's energy against a 1 kHz tone fell from 0.99 to 0.005:
    //   most of what fullband adds above 8 kHz is gone.
    // - Hybrid frames stop at 20 ms, so a 40 ms packet is two frames and
    //   src/opus_decoder.c's decode_fec path rebuilds only the last 20 ms of a
    //   lost one. LBRR protects the one packet before, never a burst, which is
    //   how the WAN path loses them.
    // Opus DRED (deep redundancy, built for bursts) is the future option.
    int error = OPUS_OK;
    std::unique_ptr<State> state = std::make_unique<State>();
    state->config = config;
    state->bandwidth = bandwidthForBitrate(config.bitrate);
    state->encoder = opus_encoder_create(OpusAudioCodecConfig::kSampleRate,
                                         OpusAudioCodecConfig::kChannels,
                                         OPUS_APPLICATION_AUDIO, &error);
    if (state->encoder == nullptr || error != OPUS_OK
        || opus_encoder_ctl(state->encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC)) != OPUS_OK
        || opus_encoder_ctl(state->encoder, OPUS_SET_BANDWIDTH(state->bandwidth)) != OPUS_OK
        || opus_encoder_ctl(state->encoder, OPUS_SET_BITRATE(config.bitrate)) != OPUS_OK
        || opus_encoder_ctl(state->encoder, OPUS_SET_VBR(1)) != OPUS_OK
        || opus_encoder_ctl(state->encoder, OPUS_SET_VBR_CONSTRAINT(1)) != OPUS_OK
        || opus_encoder_ctl(state->encoder, OPUS_SET_COMPLEXITY(10)) != OPUS_OK
        || opus_encoder_ctl(state->encoder, OPUS_SET_INBAND_FEC(0)) != OPUS_OK
        || opus_encoder_ctl(state->encoder, OPUS_SET_DTX(0)) != OPUS_OK) {
        return;
    }
    m_state = std::move(state);
}

OpusAudioEncoder::~OpusAudioEncoder() = default;

bool OpusAudioEncoder::isReady() const
{
    return m_state && m_state->encoder;
}

std::optional<OpusEncoderProfile> OpusAudioEncoder::profile() const
{
    if (!isReady()) {
        return std::nullopt;
    }
    opus_int32 sampleRate = 0;
    opus_int32 bitrate = 0;
    if (opus_encoder_ctl(m_state->encoder, OPUS_GET_SAMPLE_RATE(&sampleRate)) != OPUS_OK
        || opus_encoder_ctl(m_state->encoder, OPUS_GET_BITRATE(&bitrate)) != OPUS_OK) {
        return std::nullopt;
    }
    OpusEncoderProfile profile;
    profile.sampleRate = sampleRate;
    profile.channels = OpusAudioCodecConfig::kChannels;
    profile.frameSamples = OpusAudioCodecConfig::kFrameSamples;
    profile.targetBitrate = bitrate;
    profile.audioBandwidthHz = audioBandwidthHz(m_state->bandwidth);
    return profile;
}

int OpusAudioEncoder::lookaheadFrames() const
{
    opus_int32 lookahead = 0;
    if (!isReady()
        || opus_encoder_ctl(m_state->encoder, OPUS_GET_LOOKAHEAD(&lookahead)) != OPUS_OK) {
        return 0;
    }
    return std::max<opus_int32>(0, lookahead);
}

int opusCodecDelayFrames()
{
    // The same application, rate and channels as every encoder here;
    // libopus's lookahead is Fs/400 plus its delay compensation for
    // OPUS_APPLICATION_AUDIO (312 frames at 48 kHz in the pinned source).
    static const int frames = OpusAudioEncoder{}.lookaheadFrames();
    return frames;
}

OpusRtpEncodeResult OpusAudioEncoder::encode(const QVector<float>& pcmInterleaved,
                                              quint16 sequence, quint32 timestamp,
                                              quint32 ssrc)
{
    OpusRtpEncodeResult result;
    if (!isReady() || pcmInterleaved.size()
        != OpusAudioCodecConfig::kFrameSamples * OpusAudioCodecConfig::kChannels
        || !finitePcm(pcmInterleaved)) {
        return result;
    }
    QByteArray payload(OpusAudioCodecConfig::kMaxPayloadBytes, '\0');
    const int encoded = opus_encode_float(m_state->encoder, pcmInterleaved.constData(),
                                          OpusAudioCodecConfig::kFrameSamples,
                                          reinterpret_cast<unsigned char*>(payload.data()),
                                          payload.size());
    if (encoded <= 0 || encoded > OpusAudioCodecConfig::kMaxPayloadBytes) {
        result.status = OpusAudioCodecStatus::EncodeFailed;
        return result;
    }
    payload.truncate(encoded);
    result.packetInfo = packetInfo(payload);
    if (!validPacketInfo(result.packetInfo)) {
        result.status = OpusAudioCodecStatus::EncodeFailed;
        return result;
    }
    result.packet = buildAudioRtp(OpusAudioCodecConfig::kPayloadType, sequence,
                                  timestamp, ssrc, payload);
    result.status = OpusAudioCodecStatus::Accepted;
    return result;
}

void OpusAudioEncoder::reset()
{
    if (isReady()) {
        opus_encoder_ctl(m_state->encoder, OPUS_RESET_STATE);
    }
}

OpusAudioDecoder::OpusAudioDecoder(const OpusAudioCodecConfig& config)
{
    if (!validConfig(config)) {
        return;
    }
    int error = OPUS_OK;
    std::unique_ptr<State> state = std::make_unique<State>();
    state->config = config;
    state->decoder = opus_decoder_create(OpusAudioCodecConfig::kSampleRate,
                                         OpusAudioCodecConfig::kChannels, &error);
    if (state->decoder == nullptr || error != OPUS_OK) {
        return;
    }
    m_state = std::move(state);
}

OpusAudioDecoder::~OpusAudioDecoder() = default;

bool OpusAudioDecoder::isReady() const
{
    return m_state && m_state->decoder;
}

OpusRtpDecodeResult OpusAudioDecoder::decodeRtp(const QByteArray& packet, quint32 expectedSsrc)
{
    OpusRtpDecodeResult result;
    if (!isReady()) {
        return result;
    }
    AudioRtpPacket parsed;
    result.status = parseAudioRtp(packet, OpusAudioCodecConfig::kPayloadType, parsed);
    if (result.status != OpusAudioCodecStatus::Accepted) {
        return result;
    }
    if (parsed.ssrc != expectedSsrc) {
        result.status = OpusAudioCodecStatus::UnexpectedSsrc;
        return result;
    }
    result.packetInfo = packetInfo(parsed.payload);
    if (!validPacketInfo(result.packetInfo)) {
        result.status = OpusAudioCodecStatus::MalformedRtp;
        return result;
    }
    result.pcmInterleaved.resize(OpusAudioCodecConfig::kFrameSamples
                                 * OpusAudioCodecConfig::kChannels);
    const int decoded = opus_decode_float(m_state->decoder,
                                          reinterpret_cast<const unsigned char*>(parsed.payload.constData()),
                                          parsed.payload.size(), result.pcmInterleaved.data(),
                                          OpusAudioCodecConfig::kFrameSamples, 0);
    if (decoded != OpusAudioCodecConfig::kFrameSamples || !finitePcm(result.pcmInterleaved)) {
        result.pcmInterleaved.clear();
        result.status = OpusAudioCodecStatus::DecodeFailed;
        return result;
    }
    result.sequence = parsed.sequence;
    result.timestamp = parsed.timestamp;
    result.status = OpusAudioCodecStatus::Accepted;
    return result;
}

OpusRtpDecodeResult OpusAudioDecoder::decodeMissing()
{
    OpusRtpDecodeResult result;
    if (!isReady()) {
        return result;
    }
    result.pcmInterleaved.resize(OpusAudioCodecConfig::kFrameSamples
                                 * OpusAudioCodecConfig::kChannels);
    const int decoded = opus_decode_float(m_state->decoder, nullptr, 0,
                                          result.pcmInterleaved.data(),
                                          OpusAudioCodecConfig::kFrameSamples, 0);
    if (decoded != OpusAudioCodecConfig::kFrameSamples || !finitePcm(result.pcmInterleaved)) {
        result.pcmInterleaved.clear();
        result.status = OpusAudioCodecStatus::DecodeFailed;
        return result;
    }
    result.status = OpusAudioCodecStatus::Concealed;
    return result;
}

void OpusAudioDecoder::reset()
{
    if (isReady()) {
        opus_decoder_ctl(m_state->decoder, OPUS_RESET_STATE);
    }
}

} // namespace NereusSDR
