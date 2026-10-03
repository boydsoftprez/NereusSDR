// =================================================================
// tools/digital-mode-opus-bench/opus-bench-codec.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote daemon R3 receiver audio
// (R-R3-43, R-R3-23): the digital-mode decode bench's codec stage.
//
// Runs a file of 48 kHz mono float audio through the same encoder and
// decoder objects the Core and the app use for receiver audio, so the
// bench measures the shipped settings and nothing else:
//
//   opus24    OpusAudioEncoder / OpusAudioDecoder at 24000 bit/s
//             (wideband)
//   opus48    the same at 48000 bit/s (fullband; the Core's default
//             audio_bitrate since R-R3-21)
//   lossless  PcmAudioPacketiser / decodeL16Rtp (L16, 16-bit)
//
// Each profile goes through RTP exactly as on the wire: 1920-frame
// capture blocks, one Opus packet or ten L16 packets per block, decoded
// packet by packet. The mono input is sent on both channels, as a
// receiver's audio is (left equals right); the decoded left channel is
// written out. The Opus lookahead (opusCodecDelayFrames()) is trimmed so
// the output lines up with the input sample for sample. No packet is lost.
//
// Files only: raw little-endian float32, no audio device is opened.
//
// Usage: nereus-opus-bench-codec <opus24|opus48|lossless> <in.f32> <out.f32>
//        nereus-opus-bench-codec --describe
//
// =================================================================

#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/PcmAudioCodec.h"

#include <QByteArray>
#include <QList>
#include <QVector>

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

using namespace NereusSDR;

namespace {

constexpr quint32 kSsrc = 0x0A0B0C0Du;
constexpr int kBlock = OpusAudioCodecConfig::kFrameSamples;
constexpr int kChannels = OpusAudioCodecConfig::kChannels;

bool readF32(const char* path, std::vector<float>& samples)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) { return false; }
    const std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
    if (bytes.size() % sizeof(float) != 0) { return false; }
    samples.resize(bytes.size() / sizeof(float));
    std::memcpy(samples.data(), bytes.data(), bytes.size());
    return true;
}

bool writeF32(const char* path, const std::vector<float>& samples)
{
    std::ofstream out(path, std::ios::binary);
    if (!out) { return false; }
    out.write(reinterpret_cast<const char*>(samples.data()),
              static_cast<std::streamsize>(samples.size() * sizeof(float)));
    return bool(out);
}

// One capture block of the input (zero past its end), mono to L=R stereo.
QVector<float> stereoBlock(const std::vector<float>& mono, std::size_t first)
{
    QVector<float> block(kBlock * kChannels, 0.0f);
    for (int i = 0; i < kBlock; ++i) {
        const std::size_t at = first + static_cast<std::size_t>(i);
        const float v = at < mono.size() ? mono[at] : 0.0f;
        block[i * kChannels] = v;
        block[i * kChannels + 1] = v;
    }
    return block;
}

void appendLeft(std::vector<float>& out, const QVector<float>& interleaved)
{
    for (qsizetype i = 0; i < interleaved.size(); i += kChannels) {
        out.push_back(interleaved[i]);
    }
}

int runOpus(int bitrate, const std::vector<float>& mono, std::vector<float>& out)
{
    OpusAudioCodecConfig config;
    config.bitrate = bitrate;
    OpusAudioEncoder encoder(config);
    OpusAudioDecoder decoder(config);
    if (!encoder.isReady() || !decoder.isReady()) {
        std::fprintf(stderr, "opus codec refused bitrate %d\n", bitrate);
        return 3;
    }
    const int delay = opusCodecDelayFrames();
    // Enough blocks to carry the whole input plus the lookahead out.
    const std::size_t needed = mono.size() + static_cast<std::size_t>(delay);
    std::vector<float> decoded;
    decoded.reserve(needed + kBlock);
    quint16 sequence = 1;
    quint32 timestamp = 0;
    for (std::size_t first = 0; decoded.size() < needed; first += kBlock) {
        const OpusRtpEncodeResult packet
            = encoder.encode(stereoBlock(mono, first), sequence, timestamp, kSsrc);
        if (packet.status != OpusAudioCodecStatus::Accepted) {
            std::fprintf(stderr, "encode failed at block %zu\n", first / kBlock);
            return 4;
        }
        const OpusRtpDecodeResult audio = decoder.decodeRtp(packet.packet, kSsrc);
        if (audio.status != OpusAudioCodecStatus::Accepted) {
            std::fprintf(stderr, "decode failed at block %zu\n", first / kBlock);
            return 5;
        }
        appendLeft(decoded, audio.pcmInterleaved);
        ++sequence;
        timestamp += kBlock;
    }
    out.assign(decoded.begin() + delay, decoded.begin() + static_cast<std::ptrdiff_t>(needed));
    return 0;
}

int runLossless(const std::vector<float>& mono, std::vector<float>& out)
{
    const PcmAudioPacketiser packetiser;
    std::vector<float> decoded;
    decoded.reserve(mono.size() + kBlock);
    quint16 sequence = 1;
    quint32 timestamp = 0;
    for (std::size_t first = 0; decoded.size() < mono.size(); first += kBlock) {
        const QList<QByteArray> packets
            = packetiser.packetiseBlock(stereoBlock(mono, first), sequence, timestamp, kSsrc);
        if (packets.size() != PcmAudioCodecConfig::kPacketsPerBlock) {
            std::fprintf(stderr, "packetise failed at block %zu\n", first / kBlock);
            return 4;
        }
        for (const QByteArray& packet : packets) {
            const PcmRtpDecodeResult audio = decodeL16Rtp(packet, kSsrc);
            if (audio.status != OpusAudioCodecStatus::Accepted) {
                std::fprintf(stderr, "L16 decode failed at block %zu\n", first / kBlock);
                return 5;
            }
            appendLeft(decoded, audio.pcmInterleaved);
        }
        sequence = static_cast<quint16>(sequence + PcmAudioCodecConfig::kPacketsPerBlock);
        timestamp += kBlock;
    }
    out.assign(decoded.begin(), decoded.begin() + static_cast<std::ptrdiff_t>(mono.size()));
    return 0;
}

void describe()
{
    for (const int bitrate : {24'000, 48'000}) {
        OpusAudioCodecConfig config;
        config.bitrate = bitrate;
        const OpusAudioEncoder encoder(config);
        const std::optional<OpusEncoderProfile> profile = encoder.profile();
        if (!profile) {
            std::printf("opus%d unavailable\n", bitrate / 1000);
            continue;
        }
        std::printf("opus%d rate=%d channels=%d frame=%d target_bitrate=%d "
                    "audio_bandwidth_hz=%d lookahead_frames=%d\n",
                    bitrate / 1000, profile->sampleRate, profile->channels,
                    profile->frameSamples, profile->targetBitrate,
                    profile->audioBandwidthHz, encoder.lookaheadFrames());
    }
    const PcmEncoderProfile l16 = l16EncoderProfile();
    std::printf("lossless rate=%d channels=%d frame=%d bits=%d payload_type=%d\n",
                l16.sampleRate, l16.channels, l16.frameSamples, l16.bitsPerSample,
                l16.payloadType);
}

} // namespace

int main(int argc, char** argv)
{
    if (argc == 2 && std::string(argv[1]) == "--describe") {
        describe();
        return 0;
    }
    if (argc != 4) {
        std::fprintf(stderr,
                     "usage: %s <opus24|opus48|lossless> <in.f32> <out.f32>\n"
                     "       %s --describe\n",
                     argv[0], argv[0]);
        return 2;
    }
    const std::string profile = argv[1];
    std::vector<float> mono;
    if (!readF32(argv[2], mono) || mono.empty()) {
        std::fprintf(stderr, "cannot read %s\n", argv[2]);
        return 2;
    }
    std::vector<float> out;
    int status = 2;
    if (profile == "opus24") {
        status = runOpus(24'000, mono, out);
    } else if (profile == "opus48") {
        status = runOpus(48'000, mono, out);
    } else if (profile == "lossless") {
        status = runLossless(mono, out);
    } else {
        std::fprintf(stderr, "unknown profile %s\n", profile.c_str());
        return 2;
    }
    if (status != 0) { return status; }
    if (!writeF32(argv[3], out)) {
        std::fprintf(stderr, "cannot write %s\n", argv[3]);
        return 2;
    }
    return 0;
}
