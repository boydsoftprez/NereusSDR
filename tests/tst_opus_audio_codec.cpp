// =================================================================
// tests/tst_opus_audio_codec.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote daemon R3 Task 5.
//
// =================================================================

#include <QtTest>

#include <opus.h>

#include <cmath>
#include <limits>

#include "core/session/media/OpusAudioCodec.h"

using namespace NereusSDR;

namespace NereusSDR {

// Readable QCOMPARE failures for the profile value type (found by ADL).
char* toString(const OpusEncoderProfile& profile)
{
    return QTest::toString(QStringLiteral("{%1 Hz, %2 ch, %3 samples, %4 bit/s, %5 Hz}")
                               .arg(profile.sampleRate)
                               .arg(profile.channels)
                               .arg(profile.frameSamples)
                               .arg(profile.targetBitrate)
                               .arg(profile.audioBandwidthHz));
}

} // namespace NereusSDR

namespace {

constexpr quint32 kSsrc = 0x6e657265U;

// One 40 ms frame of a phase-continuous stereo signal, frame `frameIndex` of
// a longer stream, so consecutive frames form one uninterrupted program.
QVector<float> continuousStereoTones(int frameIndex, double leftHz, double rightHz)
{
    QVector<float> pcm(OpusAudioCodecConfig::kFrameSamples * OpusAudioCodecConfig::kChannels);
    for (int sample = 0; sample < OpusAudioCodecConfig::kFrameSamples; ++sample) {
        const double time = static_cast<double>(
            frameIndex * OpusAudioCodecConfig::kFrameSamples + sample)
            / OpusAudioCodecConfig::kSampleRate;
        pcm[sample * 2] = static_cast<float>(0.3 * std::sin(2.0 * M_PI * leftHz * time));
        pcm[sample * 2 + 1] = static_cast<float>(0.3 * std::sin(2.0 * M_PI * rightHz * time));
    }
    return pcm;
}

// One 40 ms frame of a phase-continuous two-tone program, the same `lowHz` +
// `highHz` pair on both channels.
QVector<float> continuousTonePair(int frameIndex, double lowHz, double highHz)
{
    QVector<float> pcm(OpusAudioCodecConfig::kFrameSamples * OpusAudioCodecConfig::kChannels);
    for (int sample = 0; sample < OpusAudioCodecConfig::kFrameSamples; ++sample) {
        const double time = static_cast<double>(
            frameIndex * OpusAudioCodecConfig::kFrameSamples + sample)
            / OpusAudioCodecConfig::kSampleRate;
        const float value = static_cast<float>(0.25 * std::sin(2.0 * M_PI * lowHz * time)
                                               + 0.25 * std::sin(2.0 * M_PI * highHz * time));
        pcm[sample * 2] = value;
        pcm[sample * 2 + 1] = value;
    }
    return pcm;
}

QVector<float> stereoTones(float leftHz = 700.0f, float rightHz = 1700.0f)
{
    QVector<float> pcm(OpusAudioCodecConfig::kFrameSamples * OpusAudioCodecConfig::kChannels);
    for (int frame = 0; frame < OpusAudioCodecConfig::kFrameSamples; ++frame) {
        const float time = static_cast<float>(frame) / OpusAudioCodecConfig::kSampleRate;
        pcm[frame * 2] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * leftHz * time);
        pcm[frame * 2 + 1] = 0.3f * std::sin(2.0f * static_cast<float>(M_PI) * rightHz * time);
    }
    return pcm;
}

double frequencyEnergy(const QVector<float>& pcm, int channel, float hz)
{
    double cosine = 0.0;
    double sine = 0.0;
    for (int frame = 0; frame < OpusAudioCodecConfig::kFrameSamples; ++frame) {
        const double angle = 2.0 * M_PI * hz * frame / OpusAudioCodecConfig::kSampleRate;
        const double sample = pcm.at(frame * 2 + channel);
        cosine += sample * std::cos(angle);
        sine += sample * std::sin(angle);
    }
    return cosine * cosine + sine * sine;
}

OpusRtpEncodeResult encode(OpusAudioEncoder& encoder, quint16 sequence = 1,
                           quint32 timestamp = 10'000, quint32 ssrc = kSsrc)
{
    return encoder.encode(stereoTones(), sequence, timestamp, ssrc);
}

} // namespace

class TstOpusAudioCodec : public QObject
{
    Q_OBJECT

private slots:
    void profilePacketAndStereoSeparation()
    {
        OpusAudioEncoder encoder;
        OpusAudioDecoder decoder;
        QVERIFY(encoder.isReady());
        QVERIFY(decoder.isReady());
        const OpusRtpEncodeResult encoded = encode(encoder);
        QCOMPARE(encoded.status, OpusAudioCodecStatus::Accepted);
        QVERIFY(encoded.packet.size() <= OpusAudioCodecConfig::kMaxRtpPacketBytes);
        QCOMPARE(encoded.packetInfo.channels, 2);
        // R-R3-21: the default codes fullband.
        QCOMPARE(encoded.packetInfo.bandwidth, OPUS_BANDWIDTH_FULLBAND);
        QCOMPARE(encoded.packetInfo.samplesPerChannel, 1920);
        const OpusRtpDecodeResult decoded = decoder.decodeRtp(encoded.packet, kSsrc);
        QCOMPARE(decoded.status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(decoded.pcmInterleaved.size(), 3840);
        QVERIFY(frequencyEnergy(decoded.pcmInterleaved, 0, 700.0f)
                > frequencyEnergy(decoded.pcmInterleaved, 0, 1700.0f));
        QVERIFY(frequencyEnergy(decoded.pcmInterleaved, 1, 1700.0f)
                > frequencyEnergy(decoded.pcmInterleaved, 1, 700.0f));
    }

    void profileIsCorrectBeforeAnyEncodeAndAfterReset()
    {
        // R-R3-21: the default is 48 kbit/s fullband, sound up to 20 kHz.
        const OpusEncoderProfile expected{48'000, 2, 1'920, 48'000, 20'000};
        QCOMPARE(OpusAudioCodecConfig{}.bitrate, 48'000);
        OpusAudioEncoder encoder;
        QVERIFY(encoder.isReady());
        // Core announces an audio context straight after construction and
        // straight after reset(), before the first frame is encoded.
        std::optional<OpusEncoderProfile> profile = encoder.profile();
        QVERIFY(profile.has_value());
        QCOMPARE(*profile, expected);

        QCOMPARE(encode(encoder).status, OpusAudioCodecStatus::Accepted);
        profile = encoder.profile();
        QVERIFY(profile.has_value());
        QCOMPARE(*profile, expected);

        encoder.reset();
        profile = encoder.profile();
        QVERIFY(profile.has_value());
        QCOMPARE(*profile, expected);
    }

    void alternateBitrateProfileAndUnreadyEncoder()
    {
        OpusAudioCodecConfig config;
        config.bitrate = 24'000;
        OpusAudioEncoder alternate(config);
        // R-R3-21: 24 kbit/s stays accepted and codes wideband, sound up to 8 kHz.
        const OpusEncoderProfile expected{48'000, 2, 1'920, 24'000, 8'000};
        QVERIFY(alternate.profile().has_value());
        QCOMPARE(*alternate.profile(), expected);
        alternate.reset();
        QVERIFY(alternate.profile().has_value());
        QCOMPARE(*alternate.profile(), expected);

        OpusAudioCodecConfig unsupported;
        unsupported.bitrate = 32'000;
        const OpusAudioEncoder unready(unsupported);
        QVERIFY(!unready.isReady());
        QVERIFY(!unready.profile().has_value());
    }

    // R-R3-23: the two supported targets and the bandwidth each forces.
    void bandwidthFollowsTheBitrate()
    {
        QCOMPARE(bandwidthForBitrate(24'000), OPUS_BANDWIDTH_WIDEBAND);
        QCOMPARE(bandwidthForBitrate(48'000), OPUS_BANDWIDTH_FULLBAND);
        QCOMPARE(bandwidthForBitrate(32'000), 0);
        QCOMPARE(bandwidthForBitrate(0), 0);
    }

    void everyPacketMatchesTheReportedProfile_data()
    {
        QTest::addColumn<int>("bitrate");
        QTest::addColumn<int>("bandwidth");
        QTest::addColumn<int>("bandwidthHz");
        QTest::newRow("24 kbit/s wideband") << 24'000 << int(OPUS_BANDWIDTH_WIDEBAND) << 8'000;
        QTest::newRow("48 kbit/s fullband") << 48'000 << int(OPUS_BANDWIDTH_FULLBAND) << 20'000;
    }

    void everyPacketMatchesTheReportedProfile()
    {
        QFETCH(int, bitrate);
        QFETCH(int, bandwidth);
        QFETCH(int, bandwidthHz);
        OpusAudioCodecConfig config;
        config.bitrate = bitrate;
        OpusAudioEncoder encoder(config);
        const std::optional<OpusEncoderProfile> profile = encoder.profile();
        QVERIFY(profile.has_value());
        QCOMPARE(profile->targetBitrate, bitrate);
        QCOMPARE(profile->audioBandwidthHz, bandwidthHz);
        // Two seconds of a 997/1703 Hz stereo program per context, with the
        // reset Core performs between contexts.
        constexpr int kFramesPerContext = 50;
        for (int context = 0; context < 2; ++context) {
            for (int frame = 0; frame < kFramesPerContext; ++frame) {
                const OpusRtpEncodeResult encoded = encoder.encode(
                    continuousStereoTones(frame, 997.0, 1703.0), static_cast<quint16>(frame),
                    static_cast<quint32>(frame * OpusAudioCodecConfig::kFrameSamples), kSsrc);
                QCOMPARE(encoded.status, OpusAudioCodecStatus::Accepted);
                const OpusRtpInspection inspected = inspectOpusRtp(encoded.packet, kSsrc);
                QCOMPARE(inspected.status, OpusAudioCodecStatus::Accepted);
                QCOMPARE(inspected.packetInfo.channels, profile->channels);
                QCOMPARE(inspected.packetInfo.samplesPerChannel, profile->frameSamples);
                QCOMPARE(inspected.packetInfo.bandwidth, bandwidth);
            }
            encoder.reset();
            QVERIFY(encoder.profile().has_value());
            QCOMPARE(*encoder.profile(), *profile);
        }
    }

    void measuredAlternateBitrateAndPlc()
    {
        OpusAudioCodecConfig config;
        config.bitrate = 48'000;
        OpusAudioEncoder encoder(config);
        OpusAudioDecoder decoder(config);
        const OpusRtpEncodeResult encoded = encode(encoder);
        QCOMPARE(encoded.status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(encoded.packetInfo.channels, 2);
        QCOMPARE(encoded.packetInfo.bandwidth, OPUS_BANDWIDTH_FULLBAND);
        QCOMPARE(decoder.decodeRtp(encoded.packet, kSsrc).status, OpusAudioCodecStatus::Accepted);
        const OpusRtpDecodeResult concealed = decoder.decodeMissing();
        QCOMPARE(concealed.status, OpusAudioCodecStatus::Concealed);
        QCOMPARE(concealed.pcmInterleaved.size(), 3840);
        for (float sample : concealed.pcmInterleaved) { QVERIFY(std::isfinite(sample)); }
    }

    // R-R3-23: a 15 kHz tone above a 1 kHz tone survives encode and decode at
    // 48 kbit/s (fullband) and is cut at 24 kbit/s (wideband, 8 kHz).
    void highToneSurvivesOnlyAtFullband_data()
    {
        QTest::addColumn<int>("bitrate");
        QTest::addColumn<bool>("keepsHighTone");
        QTest::newRow("24 kbit/s") << 24'000 << false;
        QTest::newRow("48 kbit/s") << 48'000 << true;
    }

    void highToneSurvivesOnlyAtFullband()
    {
        QFETCH(int, bitrate);
        QFETCH(bool, keepsHighTone);
        OpusAudioCodecConfig config;
        config.bitrate = bitrate;
        OpusAudioEncoder encoder(config);
        OpusAudioDecoder decoder(config);
        QVERIFY(encoder.isReady());
        QVERIFY(decoder.isReady());
        constexpr double kLowHz = 1'000.0;
        constexpr double kHighHz = 15'000.0;
        // One second of program, so the encoder has settled; measure the
        // last decoded frame. Both tones fit a whole number of cycles in a
        // frame, so frequencyEnergy sees no leakage between them.
        constexpr int kFrames = 25;
        QVector<float> decodedPcm;
        for (int frame = 0; frame < kFrames; ++frame) {
            const OpusRtpEncodeResult encoded = encoder.encode(
                continuousTonePair(frame, kLowHz, kHighHz), static_cast<quint16>(frame),
                static_cast<quint32>(frame * OpusAudioCodecConfig::kFrameSamples), kSsrc);
            QCOMPARE(encoded.status, OpusAudioCodecStatus::Accepted);
            const OpusRtpDecodeResult decoded = decoder.decodeRtp(encoded.packet, kSsrc);
            QCOMPARE(decoded.status, OpusAudioCodecStatus::Accepted);
            decodedPcm = decoded.pcmInterleaved;
        }
        for (int channel = 0; channel < OpusAudioCodecConfig::kChannels; ++channel) {
            const double low = frequencyEnergy(decodedPcm, channel, static_cast<float>(kLowHz));
            const double high = frequencyEnergy(decodedPcm, channel, static_cast<float>(kHighHz));
            QVERIFY(low > 0.0);
            const double ratio = high / low;
            if (keepsHighTone) {
                QVERIFY2(ratio > 0.25, qPrintable(QStringLiteral("15/1 kHz energy ratio %1").arg(ratio)));
            } else {
                QVERIFY2(ratio < 0.001, qPrintable(QStringLiteral("15/1 kHz energy ratio %1").arg(ratio)));
            }
        }
    }

    void sequenceAndTimestampWrap()
    {
        OpusAudioEncoder encoder;
        OpusAudioDecoder decoder;
        const OpusRtpEncodeResult nearWrap = encode(encoder, 65535, 0xfffffff0U);
        QCOMPARE(nearWrap.status, OpusAudioCodecStatus::Accepted);
        const OpusRtpDecodeResult decodedNearWrap = decoder.decodeRtp(nearWrap.packet, kSsrc);
        QCOMPARE(decodedNearWrap.status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(decodedNearWrap.sequence, quint16(65535));
        QCOMPARE(decodedNearWrap.timestamp, quint32(0xfffffff0U));
        const OpusRtpEncodeResult wrapped = encode(encoder, 0, 0x00000770U);
        QCOMPARE(wrapped.status, OpusAudioCodecStatus::Accepted);
        const OpusRtpDecodeResult decodedWrapped = decoder.decodeRtp(wrapped.packet, kSsrc);
        QCOMPARE(decodedWrapped.status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(decodedWrapped.sequence, quint16(0));
        QCOMPARE(decodedWrapped.timestamp, quint32(0x00000770U));
    }

    void rejectsMalformedAndUnexpectedPackets()
    {
        OpusAudioEncoder encoder;
        OpusAudioDecoder decoder;
        const OpusRtpEncodeResult encoded = encode(encoder);
        QCOMPARE(encoded.status, OpusAudioCodecStatus::Accepted);
        const QByteArray packet = encoded.packet;
        QVERIFY(packet.size() > 12);
        QCOMPARE(decoder.decodeRtp(packet.left(11), kSsrc).status, OpusAudioCodecStatus::MalformedRtp);

        QByteArray wrongVersion = packet;
        wrongVersion[0] = static_cast<char>(0x40);
        QCOMPARE(decoder.decodeRtp(wrongVersion, kSsrc).status, OpusAudioCodecStatus::MalformedRtp);
        QByteArray wrongPayloadType = packet;
        wrongPayloadType[1] = static_cast<char>(112);
        QCOMPARE(decoder.decodeRtp(wrongPayloadType, kSsrc).status, OpusAudioCodecStatus::MalformedRtp);
        QByteArray missingCsrc = packet.left(12);
        missingCsrc[0] = static_cast<char>(0x81);
        QCOMPARE(decoder.decodeRtp(missingCsrc, kSsrc).status, OpusAudioCodecStatus::MalformedRtp);
        QByteArray badExtension = packet.left(12);
        badExtension[0] = static_cast<char>(0x90);
        QCOMPARE(decoder.decodeRtp(badExtension, kSsrc).status, OpusAudioCodecStatus::MalformedRtp);
        QByteArray badPadding = packet;
        badPadding[0] = static_cast<char>(0xa0);
        badPadding[badPadding.size() - 1] = static_cast<char>(0xff);
        QCOMPARE(decoder.decodeRtp(badPadding, kSsrc).status, OpusAudioCodecStatus::MalformedRtp);
        QCOMPARE(decoder.decodeRtp(packet, kSsrc + 1).status, OpusAudioCodecStatus::UnexpectedSsrc);
        QCOMPARE(decoder.decodeRtp(QByteArray(941, '\0'), kSsrc).status, OpusAudioCodecStatus::Oversized);
    }

    void inspectionReportsPayloadAfterCsrcExtensionAndPadding()
    {
        OpusAudioEncoder encoder;
        const OpusRtpEncodeResult encoded = encode(encoder);
        QCOMPARE(encoded.status, OpusAudioCodecStatus::Accepted);

        // Wrap a real profile-valid Opus payload in every variable RTP
        // header component. The observer reports only the encoded payload,
        // excluding the CSRC, extension and trailing padding bytes.
        QByteArray decorated;
        decorated.reserve(encoded.packet.size() + 15);
        decorated.append(static_cast<char>(0xb1)); // V2, P, X, one CSRC
        decorated.append(encoded.packet.at(1));
        decorated.append(encoded.packet.mid(2, 10));
        decorated.append("\x01\x02\x03\x04", 4); // one CSRC
        decorated.append("\xbe\xde\x00\x01", 4); // one extension word
        decorated.append("\x00\x00\x00\x00", 4);
        decorated.append(encoded.packet.mid(OpusAudioCodecConfig::kRtpHeaderBytes));
        decorated.append("\x00\x00\x03", 3); // three padding bytes

        const OpusRtpInspection inspected = inspectOpusRtp(decorated, kSsrc);
        QCOMPARE(inspected.status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(inspected.payloadBytes,
                 qsizetype(encoded.packet.size() - OpusAudioCodecConfig::kRtpHeaderBytes));

        // The copy-free view (minor 2) finds the same payload in place.
        AudioRtpView view;
        QCOMPARE(inspectAudioRtpHeader(decorated, OpusAudioCodecConfig::kPayloadType, view),
                 OpusAudioCodecStatus::Accepted);
        QCOMPARE(view.payloadOffset, qsizetype(OpusAudioCodecConfig::kRtpHeaderBytes + 4 + 8));
        QCOMPARE(view.payloadBytes, inspected.payloadBytes);
        QCOMPARE(view.ssrc, kSsrc);
        QCOMPARE(decorated.mid(view.payloadOffset, view.payloadBytes),
                 encoded.packet.mid(OpusAudioCodecConfig::kRtpHeaderBytes));
        QCOMPARE(inspectAudioRtpHeader(decorated, 96, view), OpusAudioCodecStatus::MalformedRtp);
    }

    // R-R3-35 (I1): the codec delay the measured audio delay counts is the
    // real one. A 200 Hz cosine that starts at full height mid-packet
    // crosses half height lookaheadFrames() later after encode and decode.
    void lookaheadIsTheDecodedDelay()
    {
        OpusAudioEncoder encoder;
        OpusAudioDecoder decoder;
        QVERIFY(encoder.isReady());
        QVERIFY(decoder.isReady());
        // Fs/400 + Fs/250 at 48 kHz for OPUS_APPLICATION_AUDIO.
        QCOMPARE(encoder.lookaheadFrames(), 312);
        QCOMPARE(opusCodecDelayFrames(), encoder.lookaheadFrames());
        constexpr int kPackets = 10;
        constexpr int kOnset = 5 * OpusAudioCodecConfig::kFrameSamples + 777;
        constexpr double kAmplitude = 0.5;
        QVector<float> decoded;
        for (int packet = 0; packet < kPackets; ++packet) {
            QVector<float> pcm(OpusAudioCodecConfig::kFrameSamples * OpusAudioCodecConfig::kChannels,
                               0.0f);
            for (int sample = 0; sample < OpusAudioCodecConfig::kFrameSamples; ++sample) {
                const int frame = packet * OpusAudioCodecConfig::kFrameSamples + sample;
                if (frame >= kOnset) {
                    const double time = double(frame - kOnset) / OpusAudioCodecConfig::kSampleRate;
                    pcm[sample * 2] = pcm[sample * 2 + 1] =
                        float(kAmplitude * std::cos(2.0 * M_PI * 200.0 * time));
                }
            }
            const auto encoded = encoder.encode(pcm, quint16(packet),
                quint32(packet * OpusAudioCodecConfig::kFrameSamples), kSsrc);
            QCOMPARE(encoded.status, OpusAudioCodecStatus::Accepted);
            const auto audio = decoder.decodeRtp(encoded.packet, kSsrc);
            QCOMPARE(audio.status, OpusAudioCodecStatus::Accepted);
            decoded += audio.pcmInterleaved;
        }
        int crossing = -1;
        for (int frame = 0; frame < decoded.size() / 2; ++frame) {
            if (decoded.at(frame * 2) >= kAmplitude / 2.0) {
                crossing = frame;
                break;
            }
        }
        QVERIFY(crossing > 0);
        QVERIFY2(std::abs(crossing - kOnset - encoder.lookaheadFrames()) <= 4,
                 qPrintable(QString::number(crossing - kOnset)));
    }

    void resetAndInputValidation()
    {
        OpusAudioEncoder encoder;
        OpusAudioDecoder decoder;
        QVector<float> bad = stereoTones();
        bad[0] = std::numeric_limits<float>::quiet_NaN();
        QCOMPARE(encoder.encode(bad, 1, 0, kSsrc).status, OpusAudioCodecStatus::InvalidInput);
        OpusAudioCodecConfig unsupported;
        unsupported.bitrate = 32'000;
        QVERIFY(!OpusAudioEncoder(unsupported).isReady());

        const OpusRtpEncodeResult before = encode(encoder);
        QCOMPARE(before.status, OpusAudioCodecStatus::Accepted);
        encoder.reset();
        decoder.reset();
        const OpusRtpEncodeResult after = encode(encoder, 2, 1920);
        QCOMPARE(after.status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(decoder.decodeRtp(after.packet, kSsrc).status, OpusAudioCodecStatus::Accepted);
        QVERIFY(!before.packet.isEmpty());
    }
};

QTEST_MAIN(TstOpusAudioCodec)
#include "tst_opus_audio_codec.moc"
