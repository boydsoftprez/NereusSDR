// =================================================================
// tests/tst_daemon_audio_sender.cpp  (NereusSDR)
// =================================================================
// DaemonAudioSender owns only capture-to-Opus packetisation.  These tests
// drive the real AudioEngine/MasterMixer path and decode the emitted RTP.
// =================================================================

#include <QtTest>

#include <algorithm>

#include "core/AudioEngine.h"
#include "core/session/media/DaemonAudioSender.h"
#include "core/session/media/DaemonAudioSource.h"
#include "core/session/media/OpusAudioCodec.h"
#include "core/session/media/PcmAudioCodec.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <cmath>

using namespace NereusSDR;

namespace {

constexpr int kDspFrames = 64;
constexpr quint32 kSsrc = 0x6e657265U;

QVector<float> stereoBlock(float left, float right)
{
    QVector<float> block(kDspFrames * 2);
    for (int frame = 0; frame < kDspFrames; ++frame) {
        block[frame * 2] = left;
        block[frame * 2 + 1] = right;
    }
    return block;
}

struct Harness {
    RadioModel radio;
    AudioEngine* engine{nullptr};
    int sliceA{-1};
    int sliceB{-1};

    Harness()
    {
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        engine = radio.audioEngine();
        Q_ASSERT(engine != nullptr);
        engine->masterMixForTest().setRampFrames(1);
        engine->masterMixForTest().setSlewUpFrames(0);
        sliceA = radio.addSlice();
        sliceB = radio.addSlice();
        Q_ASSERT(sliceA >= 0 && sliceB >= 0);
        engine->setSliceStreaming(sliceA, true);
        engine->setSliceStreaming(sliceB, true);
    }

    void feedMixed(int frames, float aLeft, float aRight, float bLeft, float bRight)
    {
        const QVector<float> a = stereoBlock(aLeft, aRight);
        const QVector<float> b = stereoBlock(bLeft, bRight);
        for (int delivered = 0; delivered < frames; delivered += kDspFrames) {
            engine->rxBlockReady(sliceA, a.constData(), kDspFrames);
            engine->rxBlockReady(sliceB, b.constData(), kDspFrames);
        }
    }

    void primeBarrier()
    {
        feedMixed(kDspFrames * 2, 0.40f, 0.40f, 0.40f, 0.40f);
    }
};

double energy(const QVector<float>& pcm, int channel)
{
    double total = 0.0;
    for (int frame = 0; frame < OpusAudioCodecConfig::kFrameSamples; ++frame) {
        const double sample = pcm.at(frame * 2 + channel);
        total += sample * sample;
    }
    return total;
}

QByteArray packetAt(const QSignalSpy& packets, int index)
{
    return packets.at(index).at(0).toByteArray();
}

// The lossless pacing clock a test drives: ticks advance it explicitly.
struct PacingClock {
    qint64 nowNs = 1'000'000'000;
    void attach(DaemonAudioSender& sender)
    {
        sender.setPacingClockForTest([this] { return nowNs; });
    }
};

constexpr qint64 kTickNs = 10'000'000; // the sender's nominal 10 ms tick

// Drains one tick of `tickNs` at a time until `expected` packets have been
// emitted (or a tick emits nothing), checking that no tick emits more than
// the lossless cap. Returns the ticks used, or -1 if a tick broke the cap.
int drainPaced(DaemonAudioSender& sender, PacingClock& clock, const QSignalSpy& packets,
               int expected, qint64 tickNs = kTickNs, QList<int>* perTick = nullptr)
{
    int ticks = 0;
    while (packets.count() < expected && ticks < 1000) {
        const int before = packets.count();
        clock.nowNs += tickNs;
        sender.drain();
        ++ticks;
        const int emitted = packets.count() - before;
        if (perTick) { perTick->append(emitted); }
        if (emitted > DaemonAudioSender::kMaxLosslessPacketsPerDrain) { return -1; }
        if (emitted == 0) { break; }
    }
    return ticks;
}

} // namespace

class TstDaemonAudioSender final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase()
    {
        OpusAudioEncoder encoder;
        if (!encoder.isReady()) {
            QSKIP("Opus encoder is unavailable in this build");
        }
    }

    void encodesRealMasterMixWithPerSliceGainPanAndMute()
    {
        Harness h;
        // A supplies left only; B supplies right only.  Both are real slice
        // programs, mixed before DaemonAudioSource captures the station bus.
        h.engine->masterMixForTest().setSliceGain(h.sliceA, 0.5f, -1.0f);
        h.engine->masterMixForTest().setSliceGain(h.sliceB, 0.5f, 1.0f);
        h.primeBarrier();

        DaemonAudioSender sender(h.engine);
        QSignalSpy packets(&sender, &DaemonAudioSender::packetReady);
        QVERIFY(sender.start(kSsrc, 9, 10'000));
        h.feedMixed(DaemonAudioSource::kBlockFrames, 0.40f, 0.40f, 0.40f, 0.40f);
        sender.drain();
        QCOMPARE(packets.count(), 1);

        OpusAudioDecoder decoder;
        const auto mixed = decoder.decodeRtp(packetAt(packets, 0), kSsrc);
        QCOMPARE(mixed.status, OpusAudioCodecStatus::Accepted);
        QVERIFY(energy(mixed.pcmInterleaved, 0) > 1.0);
        QVERIFY(energy(mixed.pcmInterleaved, 1) > 1.0);

        // Keep B in the barrier while muted: mute belongs in the mixer and
        // must not cause the station sender to fall back to B's old program.
        sender.stop();
        h.radio.sliceById(h.sliceB)->setMuted(true);
        h.feedMixed(kDspFrames, 0.40f, 0.40f, 0.40f, 0.40f);
        QVERIFY(sender.start(kSsrc, 10, 11'920));
        h.feedMixed(DaemonAudioSource::kBlockFrames, 0.40f, 0.40f, 0.40f, 0.40f);
        sender.drain();
        QCOMPARE(packets.count(), 2);
        const auto muted = decoder.decodeRtp(packetAt(packets, 1), kSsrc);
        QCOMPARE(muted.status, OpusAudioCodecStatus::Accepted);
        QVERIFY(energy(muted.pcmInterleaved, 0) > energy(muted.pcmInterleaved, 1) * 8.0);
    }

    void tracksSequenceTimestampAndWrapFromCapturePositions()
    {
        Harness h;
        h.engine->setSliceStreaming(h.sliceB, false);
        DaemonAudioSender sender(h.engine);
        QSignalSpy packets(&sender, &DaemonAudioSender::packetReady);
        constexpr quint32 firstTimestamp = 0xfffffff0U;
        QVERIFY(sender.start(kSsrc, 65535, firstTimestamp));
        h.feedMixed(DaemonAudioSource::kBlockFrames * 2, 0.25f, -0.25f, 0.0f, 0.0f);
        sender.drain();
        QCOMPARE(packets.count(), 2);

        OpusAudioDecoder decoder;
        const auto first = decoder.decodeRtp(packetAt(packets, 0), kSsrc);
        const auto second = decoder.decodeRtp(packetAt(packets, 1), kSsrc);
        QCOMPARE(first.status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(second.status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(first.sequence, quint16(65535));
        QCOMPARE(second.sequence, quint16(0));
        QCOMPARE(first.timestamp, firstTimestamp);
        QCOMPARE(second.timestamp,
                 static_cast<quint32>(firstTimestamp + DaemonAudioSource::kBlockFrames));
        QCOMPARE(sender.nextSequence(), quint16(1));
        QCOMPARE(sender.nextTimestamp(), static_cast<quint32>(
            firstTimestamp + 2 * DaemonAudioSource::kBlockFrames));
    }

    void telemetryTracksSourceEncodingAndRestart()
    {
        Harness h;
        h.engine->setSliceStreaming(h.sliceB, false);
        DaemonAudioSender sender(h.engine);
        QSignalSpy packets(&sender, &DaemonAudioSender::packetReady);
        QVERIFY(sender.start(kSsrc, 11, 2'000));
        h.feedMixed(DaemonAudioSource::kBlockFrames * 2,
                    0.25f, -0.25f, 0.0f, 0.0f);
        sender.drain();
        QCOMPARE(packets.count(), 2);

        const auto active = sender.telemetry();
        QCOMPARE(active.source.capturedValidRateFrames,
                 std::uint64_t{2 * DaemonAudioSource::kBlockFrames});
        QCOMPARE(active.source.sourceDropEvents, std::uint64_t{0});
        QCOMPARE(active.consumedBlocks, std::uint64_t{2});
        QCOMPARE(active.encodedPackets, std::uint64_t{2});
        QCOMPARE(active.encodeFailures, std::uint64_t{0});
        QVERIFY(active.hasLastEmittedPacket);
        QCOMPARE(active.lastEmittedSequence, quint16{12});
        QCOMPARE(active.lastEmittedTimestamp,
                 quint32{2'000 + DaemonAudioSource::kBlockFrames});

        sender.stop();
        const auto stopped = sender.telemetry();
        QCOMPARE(stopped.consumedBlocks, active.consumedBlocks);
        QCOMPARE(stopped.encodedPackets, active.encodedPackets);
        QCOMPARE(stopped.lastEmittedSequence, active.lastEmittedSequence);
        QCOMPARE(stopped.lastEmittedTimestamp, active.lastEmittedTimestamp);

        QVERIFY(sender.start(kSsrc, 99, 99'000));
        const auto restarted = sender.telemetry();
        QCOMPARE(restarted.source.capturedValidRateFrames, std::uint64_t{0});
        QCOMPARE(restarted.source.sourceDropEvents, std::uint64_t{0});
        QCOMPARE(restarted.consumedBlocks, std::uint64_t{0});
        QCOMPARE(restarted.encodedPackets, std::uint64_t{0});
        QCOMPARE(restarted.encodeFailures, std::uint64_t{0});
        QVERIFY(!restarted.hasLastEmittedPacket);
    }

    void stopRestartFlushesCaptureAndUsesTheNewCallerBases()
    {
        Harness h;
        h.engine->setSliceStreaming(h.sliceB, false);
        DaemonAudioSender sender(h.engine);
        QSignalSpy packets(&sender, &DaemonAudioSender::packetReady);
        QVERIFY(sender.start(kSsrc, 1, 100));
        h.feedMixed(DaemonAudioSource::kBlockFrames, 0.10f, 0.10f, 0.0f, 0.0f);
        sender.stop();
        QVERIFY(!sender.isRunning());

        // This post-stop PCM cannot survive the new source epoch.
        h.feedMixed(DaemonAudioSource::kBlockFrames, 0.20f, 0.20f, 0.0f, 0.0f);
        QVERIFY(sender.start(kSsrc, 77, 50'000));
        h.feedMixed(DaemonAudioSource::kBlockFrames, 0.30f, 0.30f, 0.0f, 0.0f);
        sender.drain();
        QCOMPARE(packets.count(), 1);

        OpusAudioDecoder decoder;
        const auto packet = decoder.decodeRtp(packetAt(packets, 0), kSsrc);
        QCOMPARE(packet.status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(packet.sequence, quint16(77));
        QCOMPARE(packet.timestamp, quint32(50'000));
    }

    void packetReadyLifecycleChangeCannotContinueTheOldDrain()
    {
        Harness h;
        h.engine->setSliceStreaming(h.sliceB, false);
        DaemonAudioSender sender(h.engine);
        QSignalSpy packets(&sender, &DaemonAudioSender::packetReady);
        QVERIFY(sender.start(kSsrc, 4, 400));
        h.feedMixed(DaemonAudioSource::kBlockFrames * 2, 0.20f, 0.20f, 0.0f, 0.0f);

        bool restarted = false;
        bool restartAccepted = false;
        connect(&sender, &DaemonAudioSender::packetReady, &sender,
                [&sender, &restarted, &restartAccepted](const QByteArray&) {
            if (!restarted) {
                restarted = true;
                restartAccepted = sender.start(kSsrc + 1, 900, 90'000);
            }
        });
        sender.drain();
        QCOMPARE(packets.count(), 1);
        QVERIFY(restarted);
        QVERIFY(restartAccepted);

        // The second old block was flushed by start() in the recipient. A
        // newly captured block must use only the recipient's supplied bases.
        h.feedMixed(DaemonAudioSource::kBlockFrames, 0.30f, 0.30f, 0.0f, 0.0f);
        sender.drain();
        QCOMPARE(packets.count(), 2);
        OpusAudioDecoder decoder;
        const auto first = decoder.decodeRtp(packetAt(packets, 0), kSsrc);
        const auto restartedPacket = decoder.decodeRtp(packetAt(packets, 1), kSsrc + 1);
        QCOMPARE(first.status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(restartedPacket.status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(restartedPacket.sequence, quint16(900));
        QCOMPARE(restartedPacket.timestamp, quint32(90'000));
    }

    void packetReadyStopCannotEmitAnotherQueuedOldPacket()
    {
        Harness h;
        h.engine->setSliceStreaming(h.sliceB, false);
        DaemonAudioSender sender(h.engine);
        QSignalSpy packets(&sender, &DaemonAudioSender::packetReady);
        QVERIFY(sender.start(kSsrc, 4, 400));
        h.feedMixed(DaemonAudioSource::kBlockFrames * 2, 0.20f, 0.20f, 0.0f, 0.0f);

        connect(&sender, &DaemonAudioSender::packetReady, &sender,
                [&sender](const QByteArray&) { sender.stop(); });
        sender.drain();
        QCOMPARE(packets.count(), 1);
        QVERIFY(!sender.isRunning());
    }

    // R-R3-23: the Core's audio_bitrate reaches the encoder. The encoder's
    // own profile (read back from libopus) is the evidence, and a sender
    // built at 24000 still produces packets the default decoder accepts.
    // R-R3-21: 48000 (fullband) is the default.
    void configuredBitrateIsTheEncoderTarget()
    {
        const DaemonAudioSender defaultSender(nullptr);
        QVERIFY(defaultSender.encoderProfile().has_value());
        QCOMPARE(defaultSender.encoderProfile()->targetBitrate, 48'000);

        OpusAudioCodecConfig low;
        low.bitrate = 24'000;
        Harness h;
        h.engine->setSliceStreaming(h.sliceB, false);
        DaemonAudioSender sender(h.engine, low);
        const std::optional<OpusEncoderProfile> profile = sender.encoderProfile();
        QVERIFY(profile.has_value());
        QCOMPARE(profile->targetBitrate, 24'000);
        // Only the bitrate and the bandwidth it forces move: 24 kbit/s codes
        // wideband, sound up to 8 kHz; the rest of the profile is the default's.
        OpusEncoderProfile expected = *defaultSender.encoderProfile();
        QCOMPARE(expected.audioBandwidthHz, 20'000);
        expected.targetBitrate = 24'000;
        expected.audioBandwidthHz = 8'000;
        QCOMPARE(*profile, expected);

        QSignalSpy packets(&sender, &DaemonAudioSender::packetReady);
        QVERIFY(sender.start(kSsrc, 1, 0));
        h.feedMixed(DaemonAudioSource::kBlockFrames, 0.30f, 0.30f, 0.0f, 0.0f);
        sender.drain();
        QCOMPARE(packets.count(), 1);
        // reset() on start keeps the configured target.
        QCOMPARE(sender.encoderProfile()->targetBitrate, 24'000);
        OpusAudioDecoder decoder;
        QCOMPARE(decoder.decodeRtp(packetAt(packets, 0), kSsrc).status,
                 OpusAudioCodecStatus::Accepted);

        OpusAudioCodecConfig unsupported;
        unsupported.bitrate = 32'000;
        DaemonAudioSender refused(h.engine, unsupported);
        QVERIFY(!refused.encoderProfile().has_value());
        QVERIFY(!refused.start(kSsrc, 1, 0));
    }

    // R-R3-23 lossless: each 1920-frame capture block leaves as ten L16
    // packets, sequence +1 and timestamp +192 per packet, continuing into
    // the next block; the audio is the real master mix, 16-bit exact.
    void losslessProfileSendsTenL16PacketsPerBlock()
    {
        Harness h;
        h.engine->setSliceStreaming(h.sliceB, false);
        DaemonAudioSender sender(h.engine);
        QCOMPARE(sender.profile(), RemoteAudioProfile::Opus);
        QVERIFY(sender.setProfile(RemoteAudioProfile::Lossless));
        QCOMPARE(sender.losslessProfile(), l16EncoderProfile());
        // The Opus encoder stays built for an instant return to Opus.
        QVERIFY(sender.encoderProfile().has_value());
        QSignalSpy packets(&sender, &DaemonAudioSender::packetReady);
        PacingClock clock;
        clock.attach(sender);
        constexpr quint16 firstSequence = 65530;
        constexpr quint32 firstTimestamp = 0xfffffc00U;
        QVERIFY(sender.start(kSsrc, firstSequence, firstTimestamp));
        h.feedMixed(DaemonAudioSource::kBlockFrames * 2, 0.25f, -0.25f, 0.0f, 0.0f);
        // Paced: three a 10 ms tick, so twenty packets take seven ticks.
        QCOMPARE(drainPaced(sender, clock, packets, 20), 7);
        QCOMPARE(packets.count(), 20);
        QCOMPARE(sender.pendingLosslessPackets(), 0);

        for (int index = 0; index < packets.count(); ++index) {
            const QByteArray packet = packetAt(packets, index);
            QCOMPARE(packet.size(), PcmAudioCodecConfig::kRtpPacketBytes);
            QVERIFY(packet.size() <= OpusAudioCodecConfig::kMaxRtpPacketBytes);
            const PcmRtpDecodeResult decoded = decodeL16Rtp(packet, kSsrc);
            QCOMPARE(decoded.status, OpusAudioCodecStatus::Accepted);
            QCOMPARE(decoded.sequence, static_cast<quint16>(firstSequence + index));
            QCOMPARE(decoded.timestamp, static_cast<quint32>(firstTimestamp + 192U * index));
            if (index >= 10) {
                // The mixer is past its ramp by the second block: steady
                // left and right levels, each exactly a 16-bit code.
                for (int frame = 0; frame < PcmAudioCodecConfig::kPacketFrames; ++frame) {
                    const float left = decoded.pcmInterleaved.at(frame * 2);
                    const float right = decoded.pcmInterleaved.at(frame * 2 + 1);
                    QVERIFY(left != 0.0f && right != 0.0f);
                    QCOMPARE(left * 32768.0f, std::round(left * 32768.0f));
                }
            }
        }
        QCOMPARE(sender.nextSequence(), static_cast<quint16>(firstSequence + 20));
        QCOMPARE(sender.nextTimestamp(),
                 static_cast<quint32>(firstTimestamp + 2 * DaemonAudioSource::kBlockFrames));
        const auto telemetry = sender.telemetry();
        QCOMPARE(telemetry.consumedBlocks, std::uint64_t{2});
        QCOMPARE(telemetry.encodedPackets, std::uint64_t{20});
        QCOMPARE(telemetry.encodeFailures, std::uint64_t{0});
        QCOMPARE(telemetry.lastEmittedSequence, static_cast<quint16>(firstSequence + 19));
        QCOMPARE(telemetry.lastEmittedTimestamp,
                 static_cast<quint32>(firstTimestamp + 192U * 19));
    }

    // A profile change is a new capture epoch: refused while running; after
    // stop, set and start, queued audio is flushed and the new profile begins
    // at the caller's block boundary. Opus comes back without a new encoder.
    void profileChangesOnlyBetweenEpochsAndFlushesQueuedAudio()
    {
        Harness h;
        h.engine->setSliceStreaming(h.sliceB, false);
        DaemonAudioSender sender(h.engine);
        QSignalSpy packets(&sender, &DaemonAudioSender::packetReady);
        PacingClock clock;
        clock.attach(sender);
        QVERIFY(sender.start(kSsrc, 1, 0));
        QVERIFY(!sender.setProfile(RemoteAudioProfile::Lossless));
        QCOMPARE(sender.profile(), RemoteAudioProfile::Opus);
        h.feedMixed(DaemonAudioSource::kBlockFrames, 0.20f, 0.20f, 0.0f, 0.0f);
        sender.drain();
        QCOMPARE(packets.count(), 1);

        // Queued, never drained: stop() flushes it.
        h.feedMixed(DaemonAudioSource::kBlockFrames, 0.20f, 0.20f, 0.0f, 0.0f);
        const quint16 sequence = sender.nextSequence();
        const quint32 timestamp = sender.nextTimestamp();
        QCOMPARE(timestamp, quint32(1920));
        sender.stop();
        QVERIFY(sender.setProfile(RemoteAudioProfile::Lossless));
        QVERIFY(sender.start(kSsrc, sequence, timestamp));
        sender.drain();
        QCOMPARE(packets.count(), 1);
        h.feedMixed(DaemonAudioSource::kBlockFrames, 0.20f, 0.20f, 0.0f, 0.0f);
        QCOMPARE(drainPaced(sender, clock, packets, 11), 4);
        QCOMPARE(packets.count(), 11);
        const PcmRtpDecodeResult first = decodeL16Rtp(packetAt(packets, 1), kSsrc);
        QCOMPARE(first.status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(first.sequence, sequence);
        QCOMPARE(first.timestamp, timestamp);

        sender.stop();
        QVERIFY(sender.setProfile(RemoteAudioProfile::Opus));
        QVERIFY(sender.start(kSsrc, sender.nextSequence(), sender.nextTimestamp()));
        h.feedMixed(DaemonAudioSource::kBlockFrames, 0.20f, 0.20f, 0.0f, 0.0f);
        sender.drain();
        QCOMPARE(packets.count(), 12);
        OpusAudioDecoder decoder;
        const auto opus = decoder.decodeRtp(packetAt(packets, 11), kSsrc);
        QCOMPARE(opus.status, OpusAudioCodecStatus::Accepted);
        QCOMPARE(opus.sequence, static_cast<quint16>(sequence + 10));
        QCOMPARE(opus.timestamp, timestamp + 1920U);
    }

    // A recipient that stops the sender mid-block ends that block: the rest
    // of its ten packets are never emitted, not even those already built
    // and waiting for a later tick.
    void losslessStopFromPacketReadyEndsTheBlock()
    {
        Harness h;
        h.engine->setSliceStreaming(h.sliceB, false);
        DaemonAudioSender sender(h.engine);
        QVERIFY(sender.setProfile(RemoteAudioProfile::Lossless));
        QSignalSpy packets(&sender, &DaemonAudioSender::packetReady);
        PacingClock clock;
        clock.attach(sender);
        QVERIFY(sender.start(kSsrc, 4, 400));
        h.feedMixed(DaemonAudioSource::kBlockFrames * 2, 0.20f, 0.20f, 0.0f, 0.0f);
        connect(&sender, &DaemonAudioSender::packetReady, &sender,
                [&sender, &packets](const QByteArray&) {
            if (packets.count() == 2) { sender.stop(); }
        });
        clock.nowNs += kTickNs;
        sender.drain();
        QCOMPARE(packets.count(), 2);
        QVERIFY(!sender.isRunning());
        QCOMPARE(sender.pendingLosslessPackets(), 0);
        sender.drain();
        QCOMPARE(packets.count(), 2);
    }

    // Carried finding (Task 5 review): after a capture stall four blocks
    // wait in the capture queue, which an unpaced lossless tick released as
    // forty packets at once. Paced, sending follows the clock at three
    // packets per 10 ms (300 packets/s against the 250 the audio needs) and
    // never more than six in a tick, however late: the rest follow on later
    // ticks, all forty, in order, none lost. Opus, one packet per block,
    // still drains the four blocks in one tick.
    void losslessStallIsPacedWithoutLoss_data()
    {
        QTest::addColumn<qint64>("tickNs");
        QTest::addColumn<int>("firstTick");
        QTest::addColumn<int>("maxTicks");
        QTest::addColumn<bool>("capped");
        QTest::newRow("10 ms ticks") << qint64(10'000'000) << 3 << 14 << false;
        // A busy Core's timer: 16 ms ticks earn 4.8 packets each, and the
        // fraction carries, so pacing still keeps ahead of the audio.
        QTest::newRow("late 16 ms ticks") << qint64(16'000'000) << 4 << 9 << false;
        // One tick a second late: the cap, six, and no more. Every such
        // tick that sends the whole cap is counted (fix wave minor 3).
        QTest::newRow("one 1 s tick") << qint64(1'000'000'000) << 6 << 7 << true;
    }
    void losslessStallIsPacedWithoutLoss()
    {
        QFETCH(qint64, tickNs);
        QFETCH(int, firstTick);
        QFETCH(int, maxTicks);
        QFETCH(bool, capped);
        Harness h;
        h.engine->setSliceStreaming(h.sliceB, false);
        DaemonAudioSender sender(h.engine);
        QVERIFY(sender.setProfile(RemoteAudioProfile::Lossless));
        QSignalSpy packets(&sender, &DaemonAudioSender::packetReady);
        PacingClock clock;
        clock.attach(sender);
        constexpr quint16 firstSequence = 100;
        constexpr quint32 firstTimestamp = 5000;
        QVERIFY(sender.start(kSsrc, firstSequence, firstTimestamp));
        // The stall: four blocks captured, no tick has run.
        h.feedMixed(DaemonAudioSource::kBlockFrames * DaemonAudioSource::kQueueBlocks,
                    0.20f, -0.20f, 0.0f, 0.0f);
        constexpr int kStallPackets =
            DaemonAudioSource::kQueueBlocks * PcmAudioCodecConfig::kPacketsPerBlock;
        static_assert(kStallPackets == 40);
        clock.nowNs += tickNs;
        sender.drain();
        QCOMPARE(packets.count(), firstTick);
        // The block being sent waits in part; the other three stay captured.
        QCOMPARE(sender.pendingLosslessPackets(), PcmAudioCodecConfig::kPacketsPerBlock - firstTick);
        QCOMPARE(sender.telemetry().consumedBlocks, std::uint64_t{1});
        QList<int> perTick{firstTick};
        const int ticks = drainPaced(sender, clock, packets, kStallPackets, tickNs, &perTick);
        QVERIFY(ticks > 0);
        QCOMPARE(packets.count(), kStallPackets);
        QVERIFY2(1 + ticks <= maxTicks, qPrintable(QString::number(1 + ticks)));
        for (int emitted : perTick) {
            QVERIFY(emitted <= DaemonAudioSender::kMaxLosslessPacketsPerDrain);
        }
        // Never behind the audio: at least 250 packets a second of clock
        // while the backlog drains (the final tick may be partial).
        const double seconds = double(tickNs) * double(ticks) / 1e9;
        QVERIFY2(double(kStallPackets - firstTick) >= 250.0 * seconds * 0.95
                     || tickNs >= 100'000'000,
                 qPrintable(QString::number(seconds)));
        for (int index = 0; index < kStallPackets; ++index) {
            const PcmRtpDecodeResult decoded = decodeL16Rtp(packetAt(packets, index), kSsrc);
            QCOMPARE(decoded.status, OpusAudioCodecStatus::Accepted);
            QCOMPARE(decoded.sequence, static_cast<quint16>(firstSequence + index));
            QCOMPARE(decoded.timestamp, firstTimestamp + 192U * quint32(index));
        }
        const auto telemetry = sender.telemetry();
        QCOMPARE(telemetry.consumedBlocks, std::uint64_t{DaemonAudioSource::kQueueBlocks});
        QCOMPARE(telemetry.encodedPackets, std::uint64_t{kStallPackets});
        QCOMPARE(telemetry.source.ringFullDrops, std::uint64_t{0});
        QCOMPARE(telemetry.source.sourceDropEvents, std::uint64_t{0});
        QCOMPARE(telemetry.lastEmittedSequence, static_cast<quint16>(firstSequence + kStallPackets - 1));
        QCOMPARE(sender.pendingLosslessPackets(), 0);
        // Only ticks that sent a full six count as held to the cap; on-time
        // and moderately late ticks never do.
        const auto fullTicks = std::uint64_t(std::count(perTick.cbegin(), perTick.cend(),
            DaemonAudioSender::kMaxLosslessPacketsPerDrain));
        QCOMPARE(telemetry.losslessCappedTicks, capped ? fullTicks : std::uint64_t{0});
        if (capped) { QVERIFY(telemetry.losslessCappedTicks >= 6); }
        clock.nowNs += tickNs;
        sender.drain();
        QCOMPARE(packets.count(), kStallPackets);

        // Opus pacing is unchanged: the same stall leaves in one tick.
        sender.stop();
        QVERIFY(sender.setProfile(RemoteAudioProfile::Opus));
        QVERIFY(sender.start(kSsrc, 1, 0));
        h.feedMixed(DaemonAudioSource::kBlockFrames * DaemonAudioSource::kQueueBlocks,
                    0.20f, -0.20f, 0.0f, 0.0f);
        sender.drain();
        QCOMPARE(packets.count(), kStallPackets + DaemonAudioSource::kQueueBlocks);
    }
};

QTEST_MAIN(TstDaemonAudioSender)
#include "tst_daemon_audio_sender.moc"
