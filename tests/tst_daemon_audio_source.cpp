// no-port-check: NereusSDR-original test coverage for the bounded R3 audio
// source bridge.

#include <QtTest/QtTest>
#include "RealtimeTestLoad.h"
#include <QSemaphore>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/session/media/DaemonAudioSource.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <optional>
#include <thread>

using namespace NereusSDR;

namespace {

constexpr int kDspFrames = 64;

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
    AudioEngine* engine = nullptr;
    int sliceA = -1;
    int sliceB = -1;

    Harness()
    {
        radio.configureStreamPool(/*userDdcCount=*/5, /*maxSlices=*/5,
                                  /*defaultRateHz=*/192000);
        engine = radio.audioEngine();
        Q_ASSERT(engine != nullptr);
        // These tests assert bridge geometry and sums, not the established
        // anti-click fade, so make mixer gain deterministic from sample one.
        engine->masterMixForTest().setRampFrames(1);
        engine->masterMixForTest().setSlewUpFrames(0);
        sliceA = radio.addSlice();
        sliceB = radio.addSlice();
        Q_ASSERT(sliceA >= 0 && sliceB >= 0);
        // AF is the mixer level now; these tests measure unity gain.
        radio.sliceById(sliceA)->setAfGain(100);
        radio.sliceById(sliceB)->setAfGain(100);
        // Both slices share a DDC in this fixture, so state their mixer
        // membership directly rather than relying on receiver activation.
        engine->setSliceStreaming(sliceA, true);
        engine->setSliceStreaming(sliceB, true);
    }
};

void feedFrames(AudioEngine* engine, int sliceId, int frames,
                float left, float right)
{
    const QVector<float> block = stereoBlock(left, right);
    for (int delivered = 0; delivered < frames; delivered += kDspFrames) {
        engine->rxBlockReady(sliceId, block.constData(), kDspFrames);
    }
}

void feedMixedBlock(Harness& harness, float aLeft, float aRight,
                    float bLeft, float bRight)
{
    const QVector<float> a = stereoBlock(aLeft, aRight);
    const QVector<float> b = stereoBlock(bLeft, bRight);
    for (int delivered = 0; delivered < DaemonAudioSource::kBlockFrames;
         delivered += kDspFrames) {
        harness.engine->rxBlockReady(harness.sliceA, a.constData(), kDspFrames);
        harness.engine->rxBlockReady(harness.sliceB, b.constData(), kDspFrames);
    }
}

void primeMixedBarrier(Harness& harness, float aLeft, float aRight,
                       float bLeft, float bRight)
{
    // The master barrier's first A release may be A-only; leave an actual B
    // block queued while capture is stopped so the first captured A release
    // has both programs. This mirrors the steady state without hiding the
    // mixer's startup boundary.
    const QVector<float> a = stereoBlock(aLeft, aRight);
    const QVector<float> b = stereoBlock(bLeft, bRight);
    harness.engine->rxBlockReady(harness.sliceA, a.constData(), kDspFrames);
    harness.engine->rxBlockReady(harness.sliceB, b.constData(), kDspFrames);
    harness.engine->rxBlockReady(harness.sliceA, a.constData(), kDspFrames);
    harness.engine->rxBlockReady(harness.sliceB, b.constData(), kDspFrames);
}

void verifyStereoConstant(const DaemonAudioBlock& block, float left, float right)
{
    QCOMPARE(block.pcmInterleaved.size(), DaemonAudioSource::kBlockSamples);
    for (int sample = 0; sample < block.pcmInterleaved.size(); sample += 2) {
        if (std::abs(block.pcmInterleaved[sample] - left) >= 0.00001f
            || std::abs(block.pcmInterleaved[sample + 1] - right) >= 0.00001f) {
            QFAIL(qPrintable(QStringLiteral("stereo mismatch at frame %1: got (%2, %3), expected (%4, %5)")
                                  .arg(sample / 2)
                                  .arg(block.pcmInterleaved[sample], 0, 'g', 8)
                                  .arg(block.pcmInterleaved[sample + 1], 0, 'g', 8)
                                  .arg(left, 0, 'g', 8)
                                  .arg(right, 0, 'g', 8)));
        }
    }
}

// Feeds `callbacks` 64-frame DSP periods whose left channel carries the
// absolute source frame index and whose right channel carries its negation,
// so any lost, repeated or reordered frame is visible at the exact boundary.
void feedRamp(AudioEngine* engine, int sliceId, quint64& nextFrame, int callbacks)
{
    QVector<float> block(kDspFrames * 2);
    for (int callback = 0; callback < callbacks; ++callback) {
        for (int frame = 0; frame < kDspFrames; ++frame) {
            const float value = static_cast<float>(nextFrame + static_cast<quint64>(frame));
            block[frame * 2] = value;
            block[frame * 2 + 1] = -value;
        }
        engine->rxBlockReady(sliceId, block.constData(), kDspFrames);
        nextFrame += kDspFrames;
    }
}

// Returns an empty string when `block` is the ramp starting at firstFrame,
// otherwise a description of the first mismatching frame.
QString rampMismatch(const DaemonAudioBlock& block, quint64 firstFrame)
{
    if (block.pcmInterleaved.size() != DaemonAudioSource::kBlockSamples) {
        return QStringLiteral("block has %1 samples").arg(block.pcmInterleaved.size());
    }
    for (int frame = 0; frame < DaemonAudioSource::kBlockFrames; ++frame) {
        const float expected = static_cast<float>(firstFrame + static_cast<quint64>(frame));
        if (block.pcmInterleaved[frame * 2] != expected
            || block.pcmInterleaved[frame * 2 + 1] != -expected) {
            return QStringLiteral("ramp break at source frame %1: got (%2, %3)")
                .arg(firstFrame + static_cast<quint64>(frame))
                .arg(block.pcmInterleaved[frame * 2], 0, 'f', 1)
                .arg(block.pcmInterleaved[frame * 2 + 1], 0, 'f', 1);
        }
    }
    return {};
}

class BlockingTap final : public MasterMixAudioTap {
public:
    void consume(const float*, int, int) noexcept override
    {
        entered.release();
        release.acquire();
    }

    QSemaphore entered;
    QSemaphore release;
};

class BlockingSliceTap final : public SliceAudioTap {
public:
    void consume(const float*, int, int) noexcept override
    {
        entered.release();
        release.acquire();
    }
    void skip(int, int) noexcept override {}

    QSemaphore entered;
    QSemaphore release;
};

class IdleSliceTap final : public SliceAudioTap {
public:
    void consume(const float*, int, int) noexcept override {}
    void skip(int, int) noexcept override {}
};

} // namespace

class TstDaemonAudioSource : public QObject {
    Q_OBJECT

private slots:
    // The load when a real-time case failed (R-R3-21, R-R3-40).
    void cleanup() { NereusSDR::RealtimeTestLoad::printLoadAverageIfFailed(); }

    void capturesTheBarrierMixedStereoBeforeLocalMasterControls()
    {
        Harness harness;
        DaemonAudioSource source;
        source.setAudioEngine(harness.engine);
        primeMixedBarrier(harness, 0.20f, -0.25f, 0.30f, 0.50f);
        source.start();

        // R-R3-06: the source is upstream of local speaker volume/mute.
        harness.engine->setVolume(0.0f);
        harness.engine->setMasterMuted(true);
        feedMixedBlock(harness, 0.20f, -0.25f, 0.30f, 0.50f);

        const auto block = source.takeBlock();
        QVERIFY(block.has_value());
        QCOMPARE(block->samplePosition, quint64{0});
        verifyStereoConstant(*block, 0.50f, 0.25f);
        QVERIFY(!source.takeBlock().has_value());
        QCOMPARE(source.dropCount(), std::uint64_t{0});
    }

    void preservesPerSliceMuteInTheMixedProgram()
    {
        Harness harness;
        DaemonAudioSource source;
        source.setAudioEngine(harness.engine);
        primeMixedBarrier(harness, 0.20f, -0.25f, 0.30f, 0.50f);
        source.start();

        feedMixedBlock(harness, 0.20f, -0.25f, 0.30f, 0.50f);
        const auto beforeMute = source.takeBlock();
        QVERIFY(beforeMute.has_value());
        QCOMPARE(beforeMute->samplePosition, quint64{0});
        verifyStereoConstant(*beforeMute, 0.50f, 0.25f);

        // The just-finished period has one pre-mute B block queued. Drain it
        // while stopped, then queue one muted B block before a fresh source
        // epoch so the asserted capture begins at the mute boundary.
        source.stop();
        harness.radio.sliceById(harness.sliceB)->setMuted(true);
        const QVector<float> a = stereoBlock(0.20f, -0.25f);
        const QVector<float> b = stereoBlock(0.30f, 0.50f);
        harness.engine->rxBlockReady(harness.sliceA, a.constData(), kDspFrames);
        harness.engine->rxBlockReady(harness.sliceB, b.constData(), kDspFrames);
        source.start();
        feedMixedBlock(harness, 0.20f, -0.25f, 0.30f, 0.50f);
        const auto afterMute = source.takeBlock();
        QVERIFY(afterMute.has_value());
        QCOMPARE(afterMute->samplePosition, quint64{0});
        verifyStereoConstant(*afterMute, 0.20f, -0.25f);
    }

    void assemblesExactFortyMillisecondBlocks()
    {
        Harness harness;
        DaemonAudioSource source;
        source.setAudioEngine(harness.engine);
        source.start();

        // A single streaming slice produces one mixer release for each 64
        // frame DSP period.  The source must not expose a partial Opus frame.
        harness.engine->setSliceStreaming(harness.sliceB, false);
        feedFrames(harness.engine, harness.sliceA,
                   DaemonAudioSource::kBlockFrames - kDspFrames,
                   -0.40f, 0.70f);
        QVERIFY(!source.takeBlock().has_value());

        feedFrames(harness.engine, harness.sliceA, kDspFrames, -0.40f, 0.70f);
        const auto block = source.takeBlock();
        QVERIFY(block.has_value());
        QCOMPARE(block->samplePosition, quint64{0});
        verifyStereoConstant(*block, -0.40f, 0.70f);
    }

    void partialIngressLossResumesOnOriginalPacketGrid()
    {
        Harness harness;
        DaemonAudioSource source;
        source.setAudioEngine(harness.engine);
        source.start();
        harness.engine->setSliceStreaming(harness.sliceB, false);

        // Establish a partial packet, then model a 64-frame master-mix
        // callback that lost the bridge's try-lock. The next complete packet
        // must begin at source frame 1,920, not at the first post-loss frame
        // (128). That makes the receiver see one integral PLC-sized gap.
        feedFrames(harness.engine, harness.sliceA, kDspFrames, 0.25f, -0.50f);
        source.dropIngressForTest(kDspFrames);
        feedFrames(harness.engine, harness.sliceA,
                   DaemonAudioSource::kBlockFrames * 3, 0.25f, -0.50f);

        const auto block = source.takeBlock();
        QVERIFY(block.has_value());
        QCOMPARE(block->samplePosition,
                 quint64{DaemonAudioSource::kBlockFrames});
        verifyStereoConstant(*block, 0.25f, -0.50f);

        const auto followingBlock = source.takeBlock();
        QVERIFY(followingBlock.has_value());
        QCOMPARE(followingBlock->samplePosition,
                 quint64{DaemonAudioSource::kBlockFrames * 2});
        verifyStereoConstant(*followingBlock, 0.25f, -0.50f);
        QCOMPARE(source.dropCount(), std::uint64_t{1});
        QCOMPARE(source.telemetry().invalidIngressDrops, std::uint64_t{1});
    }

    void boundedQueueDropsNewestCompletedBlock()
    {
        Harness harness;
        DaemonAudioSource source;
        source.setAudioEngine(harness.engine);
        source.start();
        harness.engine->setSliceStreaming(harness.sliceB, false);

        for (int marker = 1; marker <= DaemonAudioSource::kQueueBlocks + 1; ++marker) {
            feedFrames(harness.engine, harness.sliceA,
                       DaemonAudioSource::kBlockFrames,
                       static_cast<float>(marker), static_cast<float>(-marker));
        }

        QCOMPARE(source.dropCount(), std::uint64_t{1});
        for (int marker = 1; marker <= DaemonAudioSource::kQueueBlocks; ++marker) {
            const auto block = source.takeBlock();
            QVERIFY(block.has_value());
            QCOMPARE(block->samplePosition,
                     static_cast<quint64>(marker - 1)
                         * DaemonAudioSource::kBlockFrames);
            verifyStereoConstant(*block, static_cast<float>(marker),
                                 static_cast<float>(-marker));
        }
        QVERIFY(!source.takeBlock().has_value());

        // The discarded fifth block still consumes 1,920 source frames. The
        // next retained block exposes its true position rather than closing
        // that loss gap in the RTP clock.
        feedFrames(harness.engine, harness.sliceA,
                   DaemonAudioSource::kBlockFrames, 6.0f, -6.0f);
        const auto afterLoss = source.takeBlock();
        QVERIFY(afterLoss.has_value());
        QCOMPARE(afterLoss->samplePosition,
                 quint64{DaemonAudioSource::kQueueBlocks + 1}
                     * DaemonAudioSource::kBlockFrames);
        verifyStereoConstant(*afterLoss, 6.0f, -6.0f);

        // This is capture ingress, rather than a packet-loss percentage:
        // the discarded completed block still advanced the source frame
        // position. The snapshot remains useful after capture stops and is
        // cleared only by the next source epoch.
        const auto active = source.telemetry();
        QCOMPARE(active.capturedValidRateFrames,
                 std::uint64_t{DaemonAudioSource::kQueueBlocks + 2}
                     * DaemonAudioSource::kBlockFrames);
        QCOMPARE(active.sourceDropEvents, std::uint64_t{1});
        QCOMPARE(active.ringFullDrops, std::uint64_t{1});
        QCOMPARE(active.contentionLosses, std::uint64_t{0});
        source.stop();
        QCOMPARE(source.telemetry().capturedValidRateFrames,
                 active.capturedValidRateFrames);
        QCOMPARE(source.telemetry().sourceDropEvents, active.sourceDropEvents);
        source.start();
        QCOMPARE(source.telemetry().capturedValidRateFrames, std::uint64_t{0});
        QCOMPARE(source.telemetry().sourceDropEvents, std::uint64_t{0});
    }

    void consumerCriticalSectionDoesNotLoseIngress()
    {
        Harness harness;
        DaemonAudioSource source;
        source.setAudioEngine(harness.engine);
        source.start();
        harness.engine->setSliceStreaming(harness.sliceB, false);

        constexpr int kCallbacksPerBlock = DaemonAudioSource::kBlockFrames / kDspFrames;
        quint64 nextFrame = 0;
        feedRamp(harness.engine, harness.sliceA, nextFrame, kCallbacksPerBlock);

        // Hold a consumer inside takeBlock()'s locked section while the DSP
        // thread keeps producing. The producer must never wait, and it must
        // not lose audio merely because the consumer owned the lock.
        QSemaphore entered;
        QSemaphore release;
        source.setTakeBlockLockedHookForTest([&] {
            entered.release();
            release.acquire();
        });
        std::optional<DaemonAudioBlock> first;
        std::thread consumer([&] { first = source.takeBlock(); });
        QVERIFY2(entered.tryAcquire(1, 1000),
                 "the consumer did not enter takeBlock's locked section");

        // A whole packet completes during the hold, plus part of the next.
        feedRamp(harness.engine, harness.sliceA, nextFrame, kCallbacksPerBlock + 10);
        release.release();
        consumer.join();
        // Clear only after the consumer has left the hook it was running.
        source.setTakeBlockLockedHookForTest({});

        feedRamp(harness.engine, harness.sliceA, nextFrame, kCallbacksPerBlock - 10);

        QVERIFY(first.has_value());
        QCOMPARE(first->samplePosition, quint64{0});
        QVERIFY2(rampMismatch(*first, 0).isEmpty(), qPrintable(rampMismatch(*first, 0)));
        for (quint64 expected : {quint64{DaemonAudioSource::kBlockFrames},
                                 quint64{DaemonAudioSource::kBlockFrames * 2}}) {
            const auto block = source.takeBlock();
            QVERIFY2(block.has_value(), qPrintable(QStringLiteral(
                "missing block at source frame %1").arg(expected)));
            QCOMPARE(block->samplePosition, expected);
            const QString mismatch = rampMismatch(*block, expected);
            QVERIFY2(mismatch.isEmpty(), qPrintable(mismatch));
        }
        QVERIFY(!source.takeBlock().has_value());
        QCOMPARE(source.dropCount(), std::uint64_t{0});
        const DaemonAudioSourceTelemetry telemetry = source.telemetry();
        QVERIFY(telemetry.contentionRetries >= 1);
        QCOMPARE(telemetry.contentionLosses, std::uint64_t{0});
        QCOMPARE(telemetry.ringFullDrops, std::uint64_t{0});
        QCOMPARE(telemetry.invalidIngressDrops, std::uint64_t{0});
        QCOMPARE(telemetry.sourceDropEvents, std::uint64_t{0});
    }

    void secondPacketDuringHeldLockIsAContentionLossOnTheGrid()
    {
        Harness harness;
        DaemonAudioSource source;
        source.setAudioEngine(harness.engine);
        source.start();
        harness.engine->setSliceStreaming(harness.sliceB, false);

        constexpr int kCallbacksPerBlock = DaemonAudioSource::kBlockFrames / kDspFrames;
        quint64 nextFrame = 0;
        feedRamp(harness.engine, harness.sliceA, nextFrame, kCallbacksPerBlock);

        QSemaphore entered;
        QSemaphore release;
        source.setTakeBlockLockedHookForTest([&] {
            entered.release();
            release.acquire();
        });
        std::optional<DaemonAudioBlock> first;
        std::thread consumer([&] { first = source.takeBlock(); });
        QVERIFY2(entered.tryAcquire(1, 1000),
                 "the consumer did not enter takeBlock's locked section");

        // Two whole packets complete while the consumer holds the lock for
        // more than 40 ms. The first stays pending; the second is the one
        // audio loss, and it still consumes its 1,920 grid frames.
        feedRamp(harness.engine, harness.sliceA, nextFrame, kCallbacksPerBlock * 2);
        release.release();
        consumer.join();
        // Clear only after the consumer has left the hook it was running.
        source.setTakeBlockLockedHookForTest({});
        feedRamp(harness.engine, harness.sliceA, nextFrame, kCallbacksPerBlock);

        QVERIFY(first.has_value());
        QCOMPARE(first->samplePosition, quint64{0});
        for (quint64 expected : {quint64{DaemonAudioSource::kBlockFrames},
                                 quint64{DaemonAudioSource::kBlockFrames * 3}}) {
            const auto block = source.takeBlock();
            QVERIFY(block.has_value());
            QCOMPARE(block->samplePosition, expected);
            const QString mismatch = rampMismatch(*block, expected);
            QVERIFY2(mismatch.isEmpty(), qPrintable(mismatch));
        }
        QVERIFY(!source.takeBlock().has_value());
        const DaemonAudioSourceTelemetry telemetry = source.telemetry();
        QCOMPARE(telemetry.contentionLosses, std::uint64_t{1});
        QCOMPARE(telemetry.ringFullDrops, std::uint64_t{0});
        QCOMPARE(telemetry.invalidIngressDrops, std::uint64_t{0});
        QCOMPARE(telemetry.sourceDropEvents, std::uint64_t{1});
        QCOMPARE(source.dropCount(), std::uint64_t{1});
    }

    void pacedProducerAgainstTightConsumerLosesNothing()
    {
        // Real-time soak: a DSP-cadence producer thread (64 frames every
        // 1.333 ms, the ANAN-G2 P2 192 kHz period) against a consumer that
        // polls takeBlock() as fast as it can. Short by default for CI; set
        // NEREUS_AUDIO_SOURCE_SOAK_MS for a longer run.
        bool envOk = false;
        int soakMs = qEnvironmentVariableIntValue("NEREUS_AUDIO_SOURCE_SOAK_MS", &envOk);
        if (!envOk || soakMs <= 0) {
            soakMs = 2000;
        }
        constexpr int kCallbacksPerBlock = DaemonAudioSource::kBlockFrames / kDspFrames;
        const auto period = std::chrono::nanoseconds(1'333'333);
        const int blocks = std::max(
            1, static_cast<int>(std::chrono::milliseconds(soakMs) / period)
                   / kCallbacksPerBlock);

        Harness harness;
        DaemonAudioSource source;
        source.setAudioEngine(harness.engine);
        source.start();
        harness.engine->setSliceStreaming(harness.sliceB, false);

        std::atomic<bool> consumerDone{false};
        std::thread producer([&] {
            quint64 nextFrame = 0;
            auto deadline = std::chrono::steady_clock::now();
            for (int callback = 0; callback < blocks * kCallbacksPerBlock; ++callback) {
                std::this_thread::sleep_until(deadline);
                deadline += period;
                feedRamp(harness.engine, harness.sliceA, nextFrame, 1);
            }
            // A finished packet may still be pending hand-over; it is retried
            // on the next callback, so keep the DSP cadence going (short of
            // completing another packet) until the consumer has everything.
            for (int extra = 0; extra < kCallbacksPerBlock - 1
                 && !consumerDone.load(std::memory_order_acquire); ++extra) {
                std::this_thread::sleep_until(deadline);
                deadline += period;
                feedRamp(harness.engine, harness.sliceA, nextFrame, 1);
            }
        });

        QString failure;
        int received = 0;
        const auto giveUp = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(soakMs) + std::chrono::seconds(5);
        while (received < blocks && std::chrono::steady_clock::now() < giveUp) {
            const auto block = source.takeBlock();
            if (!block.has_value()) {
                std::this_thread::yield();
                continue;
            }
            const quint64 expected =
                static_cast<quint64>(received) * DaemonAudioSource::kBlockFrames;
            if (failure.isEmpty()) {
                if (block->samplePosition != expected) {
                    failure = QStringLiteral("block %1 at source frame %2, expected %3")
                                  .arg(received).arg(block->samplePosition).arg(expected);
                } else {
                    failure = rampMismatch(*block, expected);
                }
            }
            ++received;
        }
        consumerDone.store(true, std::memory_order_release);
        producer.join();

        const DaemonAudioSourceTelemetry telemetry = source.telemetry();
        qInfo("soak %d ms: %d/%d packets, contentionRetries=%llu contentionLosses=%llu "
              "ringFullDrops=%llu invalidIngressDrops=%llu",
              soakMs, received, blocks,
              static_cast<unsigned long long>(telemetry.contentionRetries),
              static_cast<unsigned long long>(telemetry.contentionLosses),
              static_cast<unsigned long long>(telemetry.ringFullDrops),
              static_cast<unsigned long long>(telemetry.invalidIngressDrops));
        QVERIFY2(failure.isEmpty(), qPrintable(failure));
        QCOMPARE(received, blocks);
        QCOMPARE(telemetry.contentionLosses, std::uint64_t{0});
        QCOMPARE(telemetry.sourceDropEvents, std::uint64_t{0});
    }

    void stopAndRestartDiscardOldAndStoppedAudio()
    {
        Harness harness;
        DaemonAudioSource source;
        source.setAudioEngine(harness.engine);
        source.start();
        harness.engine->setSliceStreaming(harness.sliceB, false);

        feedFrames(harness.engine, harness.sliceA,
                   DaemonAudioSource::kBlockFrames, 1.0f, -1.0f);
        source.stop();
        QVERIFY(!source.takeBlock().has_value());

        // An engine callback after stop has no installed bridge and cannot
        // leave a block to be picked up after the next session starts.
        feedFrames(harness.engine, harness.sliceA,
                   DaemonAudioSource::kBlockFrames, 2.0f, -2.0f);
        source.start();
        feedFrames(harness.engine, harness.sliceA,
                   DaemonAudioSource::kBlockFrames, 3.0f, -3.0f);

        const auto block = source.takeBlock();
        QVERIFY(block.has_value());
        QCOMPARE(block->samplePosition, quint64{0});
        verifyStereoConstant(*block, 3.0f, -3.0f);
        QCOMPARE(source.dropCount(), std::uint64_t{0});
    }

    // R-R3-43: a slice source captures that receiver's own audio in the
    // same 1920-frame blocks on the same grid, with nothing of the other
    // slice and nothing of its mute.
    void sliceSourceCapturesOnlyItsReceiverInWholeBlocks()
    {
        Harness harness;
        DaemonAudioSource source;
        source.setAudioEngine(harness.engine);
        QCOMPARE(source.sliceSource(), DaemonAudioSource::kMasterMix);
        QVERIFY(source.setSliceSource(harness.sliceB));
        QCOMPARE(source.sliceSource(), harness.sliceB);
        source.start();
        QVERIFY(source.isRunning());
        QCOMPARE(harness.engine->sliceAudioTapCount(), 1);
        harness.radio.sliceById(harness.sliceB)->setMuted(true);

        quint64 nextFrame = 0;
        const QVector<float> other = stereoBlock(0.9f, 0.9f);
        const int callbacks = 2 * DaemonAudioSource::kBlockFrames / kDspFrames;
        for (int callback = 0; callback < callbacks; ++callback) {
            harness.engine->rxBlockReady(harness.sliceA, other.constData(), kDspFrames);
            feedRamp(harness.engine, harness.sliceB, nextFrame, 1);
        }
        for (quint64 first : {quint64{0}, quint64{DaemonAudioSource::kBlockFrames}}) {
            const auto block = source.takeBlock();
            QVERIFY(block.has_value());
            QCOMPARE(block->samplePosition, first);
            const QString mismatch = rampMismatch(*block, first);
            QVERIFY2(mismatch.isEmpty(), qPrintable(mismatch));
        }
        QVERIFY(!source.takeBlock().has_value());
        QCOMPARE(source.dropCount(), std::uint64_t{0});
        source.stop();
        QCOMPARE(harness.engine->sliceAudioTapCount(), 0);
    }

    // R-R3-45: with B on the headphones, a speakers-mix source captures A
    // alone and a headphones-mix source B alone, side by side on their own
    // taps; the headphones source leaves no receiver tap slot taken.
    void speakersAndHeadphonesSourcesSplitTheMix()
    {
        Harness harness;
        const auto route = qScopeGuard([&harness] {
            harness.radio.sliceById(harness.sliceB)->setOutputRoute(
                SliceModel::OutputRoute::Speakers);
            AppSettings::instance().remove(
                QStringLiteral("Slice%1/OutputRoute").arg(harness.sliceB));
        });
        harness.radio.sliceById(harness.sliceB)->setOutputRoute(
            SliceModel::OutputRoute::Headphones);
        DaemonAudioSource speakers;
        DaemonAudioSource headphones;
        speakers.setAudioEngine(harness.engine);
        headphones.setAudioEngine(harness.engine);
        QVERIFY(speakers.setSliceSource(DaemonAudioSource::kSpeakersMix));
        QVERIFY(headphones.setSliceSource(DaemonAudioSource::kHeadphonesMix));
        speakers.start();
        headphones.start();
        QVERIFY(speakers.isRunning() && headphones.isRunning());
        QCOMPARE(harness.engine->sliceAudioTapCount(), 0);

        for (int block = 0; block < 2; ++block) {
            feedMixedBlock(harness, 0.4f, 0.4f, -0.4f, -0.4f);
        }
        int speakerBlocks = 0;
        while (const auto block = speakers.takeBlock()) {
            ++speakerBlocks;
            float peak = 0.0f;
            for (float sample : block->pcmInterleaved) {
                QVERIFY2(sample >= 0.0f, "slice B leaked onto the speakers' mix");
                peak = std::max(peak, sample);
            }
            QVERIFY(peak > 0.05f);
        }
        int headphoneBlocks = 0;
        while (const auto block = headphones.takeBlock()) {
            ++headphoneBlocks;
            float trough = 0.0f;
            for (float sample : block->pcmInterleaved) {
                QVERIFY2(sample <= 0.0f, "slice A leaked onto the headphones mix");
                trough = std::min(trough, sample);
            }
            QVERIFY(trough < -0.05f);
        }
        QVERIFY(speakerBlocks >= 1);
        QCOMPARE(headphoneBlocks, speakerBlocks);
        headphones.stop();
        speakers.stop();
    }

    // Frames the MOX gate withholds advance the slice source's position
    // without samples: the next block starts on the original grid, and the
    // withheld audio is not counted as a loss.
    void sliceSourceMoxGapKeepsThePacketGrid()
    {
        Harness harness;
        SliceModel* const txSlice = harness.radio.txBoundSlice();
        QVERIFY(txSlice != nullptr);
        const int gated = txSlice->sliceIndex();
        DaemonAudioSource source;
        source.setAudioEngine(harness.engine);
        QVERIFY(source.setSliceSource(gated));
        source.start();

        quint64 nextFrame = 0;
        feedRamp(harness.engine, gated, nextFrame, 1);
        harness.engine->setMoxStateForTest(true);
        feedRamp(harness.engine, gated, nextFrame, 1);
        harness.engine->setMoxStateForTest(false);
        feedRamp(harness.engine, gated, nextFrame,
                 2 * DaemonAudioSource::kBlockFrames / kDspFrames);

        const auto block = source.takeBlock();
        QVERIFY(block.has_value());
        QCOMPARE(block->samplePosition, quint64{DaemonAudioSource::kBlockFrames});
        const QString mismatch = rampMismatch(*block, DaemonAudioSource::kBlockFrames);
        QVERIFY2(mismatch.isEmpty(), qPrintable(mismatch));
        QCOMPARE(source.dropCount(), std::uint64_t{0});
    }

    // The source kind is fixed while running; a slice source that finds
    // every receiver tap slot taken stays stopped.
    void sliceSourceRefusalsAndFullSlots()
    {
        Harness harness;
        DaemonAudioSource source;
        source.setAudioEngine(harness.engine);
        QVERIFY(!source.setSliceSource(-4));
        // R-R3-45: -2 and -3 are the speakers' and headphones mixes.
        QCOMPARE(DaemonAudioSource::kSpeakersMix, -2);
        QCOMPARE(DaemonAudioSource::kHeadphonesMix, -3);
        QVERIFY(source.setSliceSource(harness.sliceA));
        source.start();
        QVERIFY(source.isRunning());
        QVERIFY(!source.setSliceSource(harness.sliceB));
        QVERIFY(!source.setSliceSource(DaemonAudioSource::kMasterMix));
        source.stop();
        QVERIFY(source.setSliceSource(harness.sliceB));

        IdleSliceTap others[AudioEngine::kMaxSliceAudioTaps];
        for (IdleSliceTap& tap : others) {
            QVERIFY(harness.engine->setSliceAudioTap(harness.sliceA, &tap));
        }
        source.start();
        QVERIFY(!source.isRunning());
        QVERIFY(!source.takeBlock().has_value());
        harness.engine->clearSliceAudioTap(&others[0]);
        source.start();
        QVERIFY(source.isRunning());
        source.stop();
        for (IdleSliceTap& tap : others) {
            harness.engine->clearSliceAudioTap(&tap);
        }
        QCOMPARE(harness.engine->sliceAudioTapCount(), 0);
    }

    // Starting and stopping a receiver source never costs the master-mix
    // source a frame: its blocks stay contiguous and nothing is dropped.
    void receiverSourceChurnLeavesTheMasterSourceContinuous()
    {
        Harness harness;
        harness.engine->setSliceStreaming(harness.sliceB, false);
        DaemonAudioSource master;
        master.setAudioEngine(harness.engine);
        master.start();
        DaemonAudioSource receiver;
        receiver.setAudioEngine(harness.engine);
        QVERIFY(receiver.setSliceSource(harness.sliceA));

        quint64 nextFrame = 0;
        const int callbacks = 3 * DaemonAudioSource::kBlockFrames / kDspFrames;
        for (int callback = 0; callback < callbacks; ++callback) {
            if (callback % 5 == 0) {
                receiver.start();
                QVERIFY(receiver.isRunning());
            } else if (callback % 5 == 3) {
                receiver.stop();
            }
            feedRamp(harness.engine, harness.sliceA, nextFrame, 1);
        }
        receiver.stop();
        for (int index = 0; index < 3; ++index) {
            const quint64 first = quint64(index) * DaemonAudioSource::kBlockFrames;
            const auto block = master.takeBlock();
            QVERIFY(block.has_value());
            QCOMPARE(block->samplePosition, first);
            const QString mismatch = rampMismatch(*block, first);
            QVERIFY2(mismatch.isEmpty(), qPrintable(mismatch));
        }
        QCOMPARE(master.dropCount(), std::uint64_t{0});
    }

    void sliceTapRetirementWaitsForAnAdmittedDspCallback()
    {
        Harness harness;
        BlockingSliceTap tap;
        QVERIFY(harness.engine->setSliceAudioTap(harness.sliceA, &tap));

        const QVector<float> samples = stereoBlock(0.25f, -0.25f);
        std::thread producer([&] {
            harness.engine->rxBlockReady(harness.sliceA, samples.constData(), kDspFrames);
        });
        QVERIFY2(tap.entered.tryAcquire(1, 1000),
                 "the DSP callback did not enter the installed slice tap");

        std::atomic<bool> clearReturned{false};
        std::thread retirement([&] {
            harness.engine->clearSliceAudioTap(&tap);
            clearReturned.store(true, std::memory_order_release);
        });

        QTest::qWait(25);
        QVERIFY(!clearReturned.load(std::memory_order_acquire));
        tap.release.release();
        producer.join();
        retirement.join();
        QVERIFY(clearReturned.load(std::memory_order_acquire));
        QCOMPARE(harness.engine->sliceAudioTapCount(), 0);
    }

    void tapRetirementWaitsForAnAdmittedDspCallback()
    {
        Harness harness;
        harness.engine->setSliceStreaming(harness.sliceB, false);
        BlockingTap tap;
        harness.engine->setMasterMixAudioTap(&tap);

        const QVector<float> samples = stereoBlock(0.25f, -0.25f);
        std::thread producer([&] {
            harness.engine->rxBlockReady(harness.sliceA, samples.constData(), kDspFrames);
        });
        QVERIFY2(tap.entered.tryAcquire(1, 1000),
                 "the DSP callback did not enter the installed tap");

        std::atomic<bool> clearReturned{false};
        std::thread retirement([&] {
            harness.engine->clearMasterMixAudioTap(&tap);
            clearReturned.store(true, std::memory_order_release);
        });

        QTest::qWait(25);
        QVERIFY(!clearReturned.load(std::memory_order_acquire));
        tap.release.release();
        producer.join();
        retirement.join();
        QVERIFY(clearReturned.load(std::memory_order_acquire));
    }
};

QTEST_MAIN(TstDaemonAudioSource)
#include "tst_daemon_audio_source.moc"
