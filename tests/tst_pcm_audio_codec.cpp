// =================================================================
// tests/tst_pcm_audio_codec.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. R-R3-23 lossless audio: the L16
// packetiser against the wire contract Task 6's receiver consumes.
//
// =================================================================

#include <QtTest>

#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/PcmAudioCodec.h"

#include <cmath>
#include <limits>

using namespace NereusSDR;

namespace {

constexpr quint32 kSsrc = 0x4c313621U;
constexpr int kPacketSamples = PcmAudioCodecConfig::kPacketFrames * PcmAudioCodecConfig::kChannels;
constexpr int kBlockSamples = PcmAudioCodecConfig::kBlockFrames * PcmAudioCodecConfig::kChannels;

// A deterministic block that covers the whole 16-bit range, both channels
// different, including values between codes.
QVector<float> rampBlock()
{
    QVector<float> pcm(kBlockSamples);
    for (int index = 0; index < kBlockSamples; ++index) {
        const double phase = static_cast<double>(index) / kBlockSamples;
        pcm[index] = static_cast<float>(std::sin(2.0 * 3.141592653589793 * 7.0 * phase)
                                        * ((index % 2) != 0 ? 0.999 : -0.75)
                                        + 1.0e-6 * (index % 13));
    }
    return pcm;
}

quint16 u16(const QByteArray& bytes, int offset)
{
    return static_cast<quint16>((static_cast<quint8>(bytes.at(offset)) << 8)
                                | static_cast<quint8>(bytes.at(offset + 1)));
}

quint32 u32(const QByteArray& bytes, int offset)
{
    return (static_cast<quint32>(u16(bytes, offset)) << 16) | u16(bytes, offset + 2);
}

} // namespace

class TstPcmAudioCodec final : public QObject {
    Q_OBJECT

private slots:
    void profileIsTheAgreedWireShape()
    {
        QCOMPARE(PcmAudioCodecConfig::kPacketFrames, 192);
        QCOMPARE(PcmAudioCodecConfig::kPayloadBytes, 768);
        QCOMPARE(PcmAudioCodecConfig::kRtpPacketBytes, 780);
        QCOMPARE(PcmAudioCodecConfig::kPacketsPerBlock, 10);
        QVERIFY(PcmAudioCodecConfig::kPayloadType >= 96
                && PcmAudioCodecConfig::kPayloadType <= 127);
        QVERIFY(PcmAudioCodecConfig::kPayloadType != OpusAudioCodecConfig::kPayloadType);
        QCOMPARE(QByteArray(l16RtpMapEncoding()), QByteArray("L16/48000/2"));
        const PcmEncoderProfile profile = PcmAudioPacketiser{}.profile();
        QCOMPARE(profile, (PcmEncoderProfile{48000, 2, 192, 16,
                                             PcmAudioCodecConfig::kPayloadType}));
    }

    // Ten packets per 1920-frame block, sequence +1 and timestamp +192 per
    // packet, both wrapping; every packet 780 bytes, under the 940-byte cap.
    void blockBecomesTenPacketsWithRtpClockAndWrap()
    {
        const PcmAudioPacketiser packetiser;
        const quint16 firstSequence = 65530;
        const quint32 firstTimestamp = 0xffffff00U;
        const QList<QByteArray> packets =
            packetiser.packetiseBlock(rampBlock(), firstSequence, firstTimestamp, kSsrc);
        QCOMPARE(packets.size(), 10);
        for (int index = 0; index < packets.size(); ++index) {
            const QByteArray& packet = packets.at(index);
            QCOMPARE(packet.size(), 780);
            QVERIFY(packet.size() <= OpusAudioCodecConfig::kMaxRtpPacketBytes);
            QCOMPARE(static_cast<quint8>(packet.at(0)), quint8(0x80));
            QCOMPARE(static_cast<quint8>(packet.at(1)),
                     quint8(PcmAudioCodecConfig::kPayloadType));
            QCOMPARE(audioRtpPayloadType(packet), PcmAudioCodecConfig::kPayloadType);
            QCOMPARE(u16(packet, 2), static_cast<quint16>(firstSequence + index));
            QCOMPARE(u32(packet, 4), static_cast<quint32>(firstTimestamp + 192U * index));
            QCOMPARE(u32(packet, 8), kSsrc);
        }
        // The last packet's successor would start where the next block does.
        QCOMPARE(static_cast<quint32>(u32(packets.last(), 4) + 192U),
                 static_cast<quint32>(firstTimestamp + 1920U));
    }

    // Big-endian, interleaved left then right, exactly as RFC 3551 L16.
    void payloadIsBigEndianInterleavedStereo()
    {
        QVector<float> pcm(kPacketSamples, 0.0f);
        pcm[0] = 0.5f;          // left, frame 0: 16384 = 0x4000
        pcm[1] = -0.5f;         // right, frame 0: -16384 = 0xc000
        pcm[2] = 1.0f / 32768;  // left, frame 1: 1
        pcm[3] = -1.0f;         // right, frame 1: -32768 = 0x8000
        const PcmRtpEncodeResult encoded = PcmAudioPacketiser{}.encode(pcm, 5, 6, kSsrc);
        QCOMPARE(encoded.status, OpusAudioCodecStatus::Accepted);
        const QByteArray payload = encoded.packet.mid(12);
        QCOMPARE(payload.size(), 768);
        QCOMPARE(payload.left(8), QByteArray::fromHex("4000c00000018000"));
    }

    // Anything beyond +-1 is clipped; +1.0 takes the top code, 32767.
    void clipsToPlusMinusOne()
    {
        QCOMPARE(quantiseL16Sample(1.0f), qint16(32767));
        QCOMPARE(quantiseL16Sample(1.5f), qint16(32767));
        QCOMPARE(quantiseL16Sample(std::numeric_limits<float>::max()), qint16(32767));
        QCOMPARE(quantiseL16Sample(-1.0f), qint16(-32768));
        QCOMPARE(quantiseL16Sample(-7.0f), qint16(-32768));
        QCOMPARE(quantiseL16Sample(0.0f), qint16(0));
        QCOMPARE(quantiseL16Sample(0.4f / 32768), qint16(0));
        QCOMPARE(quantiseL16Sample(0.6f / 32768), qint16(1));
    }

    // After 16-bit quantisation the round trip is exact: decode gives
    // code / 32768, and encoding that again gives the same bytes.
    void roundTripIsBitExactAfterQuantisation()
    {
        const PcmAudioPacketiser packetiser;
        const QVector<float> block = rampBlock();
        const QList<QByteArray> packets = packetiser.packetiseBlock(block, 100, 2000, kSsrc);
        QCOMPARE(packets.size(), 10);
        for (int index = 0; index < packets.size(); ++index) {
            const PcmRtpDecodeResult decoded = decodeL16Rtp(packets.at(index), kSsrc);
            QCOMPARE(decoded.status, OpusAudioCodecStatus::Accepted);
            QCOMPARE(decoded.sequence, quint16(100 + index));
            QCOMPARE(decoded.timestamp, quint32(2000 + 192 * index));
            QCOMPARE(decoded.pcmInterleaved.size(), kPacketSamples);
            for (int sample = 0; sample < kPacketSamples; ++sample) {
                const float input = block.at(index * kPacketSamples + sample);
                const float expected =
                    static_cast<float>(quantiseL16Sample(input)) / 32768.0f;
                QCOMPARE(decoded.pcmInterleaved.at(sample), expected);
                QVERIFY(std::abs(decoded.pcmInterleaved.at(sample) - input) <= 1.0f / 65536
                        || std::abs(input) >= 32767.0f / 32768);
            }
            const PcmRtpEncodeResult again = packetiser.encode(
                decoded.pcmInterleaved, decoded.sequence, decoded.timestamp, kSsrc);
            QCOMPARE(again.status, OpusAudioCodecStatus::Accepted);
            QCOMPARE(again.packet, packets.at(index));
        }

        // Every 16-bit code survives decode then encode.
        QVector<float> every(kPacketSamples);
        for (int start = -32768; start <= 32767; start += kPacketSamples) {
            for (int sample = 0; sample < kPacketSamples; ++sample) {
                const int code = std::min(start + sample, 32767);
                every[sample] = static_cast<float>(code) / 32768.0f;
            }
            const PcmRtpEncodeResult encoded = packetiser.encode(every, 1, 1, kSsrc);
            const PcmRtpDecodeResult decoded = decodeL16Rtp(encoded.packet, kSsrc);
            QCOMPARE(decoded.status, OpusAudioCodecStatus::Accepted);
            QCOMPARE(decoded.pcmInterleaved, every);
        }
    }

    void invalidInputIsRefused()
    {
        const PcmAudioPacketiser packetiser;
        QCOMPARE(packetiser.encode(QVector<float>(kPacketSamples - 2), 1, 1, kSsrc).status,
                 OpusAudioCodecStatus::InvalidInput);
        QVector<float> nan(kPacketSamples, 0.0f);
        nan[17] = std::numeric_limits<float>::quiet_NaN();
        QCOMPARE(packetiser.encode(nan, 1, 1, kSsrc).status, OpusAudioCodecStatus::InvalidInput);
        QVector<float> infinite = rampBlock();
        infinite[1900] = std::numeric_limits<float>::infinity();
        QVERIFY(packetiser.packetiseBlock(infinite, 1, 1, kSsrc).isEmpty());
        QVERIFY(packetiser.packetiseBlock(QVector<float>(kBlockSamples - 2), 1, 1, kSsrc)
                    .isEmpty());
    }

    // The decoder shares the Opus RTP boundary and dispatch is by payload
    // type: an Opus packet is not L16 and an L16 packet is not Opus.
    void decoderEnforcesPayloadTypeSsrcSizeAndCap()
    {
        const QByteArray good =
            PcmAudioPacketiser{}.encode(QVector<float>(kPacketSamples, 0.25f), 9, 90, kSsrc)
                .packet;
        QCOMPARE(decodeL16Rtp(good, kSsrc).status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(decodeL16Rtp(good, kSsrc + 1).status, OpusAudioCodecStatus::UnexpectedSsrc);

        QByteArray opusType = good;
        opusType[1] = static_cast<char>(OpusAudioCodecConfig::kPayloadType);
        QCOMPARE(decodeL16Rtp(opusType, kSsrc).status, OpusAudioCodecStatus::MalformedRtp);
        QCOMPARE(inspectOpusRtp(good, kSsrc).status, OpusAudioCodecStatus::MalformedRtp);

        QCOMPARE(decodeL16Rtp(good.left(good.size() - 2), kSsrc).status,
                 OpusAudioCodecStatus::MalformedRtp);
        QCOMPARE(decodeL16Rtp(good.left(11), kSsrc).status, OpusAudioCodecStatus::MalformedRtp);
        QByteArray oversized = good;
        oversized.append(QByteArray(OpusAudioCodecConfig::kMaxRtpPacketBytes, '\0'));
        QCOMPARE(decodeL16Rtp(oversized, kSsrc).status, OpusAudioCodecStatus::Oversized);
        QByteArray version1 = good;
        version1[0] = static_cast<char>(0x40);
        QCOMPARE(audioRtpPayloadType(version1), -1);
        QCOMPARE(decodeL16Rtp(version1, kSsrc).status, OpusAudioCodecStatus::MalformedRtp);
        QCOMPARE(audioRtpPayloadType(good.left(11)), -1);
    }

    // The seam the Opus codec now shares builds the same header it always
    // wrote, so Opus packets are unchanged.
    void sharedRtpFramingKeepsOpusPacketsUnchanged()
    {
        OpusAudioEncoder encoder;
        if (!encoder.isReady()) {
            QSKIP("Opus encoder is unavailable in this build");
        }
        const QVector<float> silence(OpusAudioCodecConfig::kFrameSamples * 2, 0.0f);
        const OpusRtpEncodeResult encoded = encoder.encode(silence, 0x1234, 0x89abcdefU, kSsrc);
        QCOMPARE(encoded.status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(encoded.packet.left(12), QByteArray::fromHex("806f123489abcdef4c313621"));
        AudioRtpPacket parsed;
        QCOMPARE(parseAudioRtp(encoded.packet, OpusAudioCodecConfig::kPayloadType, parsed),
                 OpusAudioCodecStatus::Accepted);
        QCOMPARE(parsed.payload, encoded.packet.mid(12));
        QCOMPARE(buildAudioRtp(OpusAudioCodecConfig::kPayloadType, 0x1234, 0x89abcdefU, kSsrc,
                               parsed.payload),
                 encoded.packet);
    }
};

QTEST_GUILESS_MAIN(TstPcmAudioCodec)
#include "tst_pcm_audio_codec.moc"
