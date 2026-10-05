// =================================================================
// tests/tst_display_budget.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original tests for bounded display-codec accounting.
//
// Modification history (NereusSDR):
//   2026-10-04: Pace byte-only display extras with the shared spectrum
//               budget; retain ordinary spectrum sample validation.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//
// =================================================================

#include <QtTest>

#include "core/session/Ps3DisplayCodec.h"
#include "core/session/media/DisplayBudget.h"
#include "core/session/media/DisplayExtras.h"
#include "core/session/media/IMediaTransport.h"

#include <QRandomGenerator>

#include <limits>

using namespace NereusSDR;

namespace {

DisplayBudgetLimits limits(quint64 bytes, quint64 samples, quint32 generation = 1)
{
    return {bytes, samples, generation};
}

DisplayCodecFrame maximumSpectrumFrame()
{
    DisplayCodecFrame frame;
    frame.context.endpointId = 1;
    frame.context.contextGeneration = 1;
    frame.context.minDbm = -160.0f;
    frame.context.maxDbm = 0.0f;
    frame.context.traceSamples = DisplayCodecEncoder::kMaxSamplesPerPlane;
    frame.context.waterfallSamples = DisplayCodecEncoder::kMaxSamplesPerPlane;
    frame.context.wideSamples = SpectrumEndpoint::kMaxWideSamples;
    frame.encoderSequence = 1;
    frame.producerTimestamp = 1;
    frame.traceDbm.fill(-100.0f, frame.context.traceSamples);
    frame.waterfallDbm.fill(-100.0f, frame.context.waterfallSamples);
    frame.wideDbm.fill(-100.0f, frame.context.wideSamples);
    return frame;
}

Ps3Snapshot maximumPs3Snapshot()
{
    Ps3Snapshot snapshot;
    snapshot.channelId = 1;
    snapshot.sessionGeneration = 1;
    snapshot.sequence = 1;
    snapshot.sampleCount = Ps3Snapshot::kMaxSampleCount;
    snapshot.correctionCount = Ps3Snapshot::kMaxCorrectionCount;
    const auto fill = [](std::vector<double>& values, int count) {
        values.assign(static_cast<std::size_t>(count), 0.0);
    };
    fill(snapshot.x, snapshot.sampleCount);
    fill(snapshot.ym, snapshot.sampleCount);
    fill(snapshot.yc, snapshot.sampleCount);
    fill(snapshot.ys, snapshot.sampleCount);
    fill(snapshot.xmCorrection, snapshot.correctionCount);
    fill(snapshot.ymCorrection, snapshot.correctionCount);
    fill(snapshot.xaCorrection, snapshot.correctionCount);
    fill(snapshot.yaCorrection, snapshot.correctionCount);
    return snapshot;
}

DisplayCodecFrame spectrumFrame(int pixels, int wide)
{
    DisplayCodecFrame frame;
    frame.context.endpointId = 3;
    frame.context.contextGeneration = 1;
    frame.context.minDbm = -160.0f;
    frame.context.maxDbm = 0.0f;
    frame.context.traceSamples = static_cast<quint16>(pixels);
    frame.context.waterfallSamples = static_cast<quint16>(pixels);
    frame.context.wideSamples = static_cast<quint16>(wide);
    frame.traceDbm.resize(pixels);
    frame.waterfallDbm.resize(pixels);
    frame.wideDbm.resize(wide);
    return frame;
}

// Rows of three characters: full-range noise, a slow drift, a flat floor.
void fillRow(QVector<float>& row, QRandomGenerator& random, int character)
{
    float level = -100.0f;
    for (float& value : row) {
        switch (character) {
        case 0:
            value = -170.0f + 180.0f * static_cast<float>(random.generateDouble());
            break;
        case 1:
            level += -1.5f + 3.0f * static_cast<float>(random.generateDouble());
            value = level;
            break;
        default:
            value = -120.0f;
            break;
        }
    }
}

SpectrumDisplayCost cost(int pixels, int fps, bool wide)
{
    const auto result = spectrumDisplayCost(pixels, fps, wide);
    Q_ASSERT(result);
    return *result;
}

} // namespace

class TestDisplayBudget : public QObject {
    Q_OBJECT

private slots:
    void calculatorUsesActualCodecBounds()
    {
        const SpectrumDisplayCost maximum = cost(DisplayCodecEncoder::kMaxSamplesPerPlane,
                                                  60, true);
        QCOMPARE(maximum.maximumFrameBytes, kMaximumSpectrumDisplayFrameBytes);
        QCOMPARE(maximum.maximumFrameBytes, quint32{9'361});
        QCOMPARE(maximum.maximumFrameSampleUnits, kMaximumSpectrumDisplayFrameSampleUnits);
        QCOMPARE(maximum.maximumFrameSampleUnits, quint32{8'960});
        QCOMPARE(maximum.charge.applicationBytesPerSecond, quint64{9'361 * 60});
        QCOMPARE(maximum.charge.spectrumSampleUnitsPerSecond, quint64{8'960 * 60});
        QCOMPARE(maximum.charge.messagesPerSecond, quint32{60});

        DisplayCodecEncoder encoder;
        const QByteArray keyframe = encoder.encode(maximumSpectrumFrame(), true);
        QCOMPARE(keyframe.size(), static_cast<int>(maximum.maximumFrameBytes));
        QCOMPARE(keyframe.size(), static_cast<int>(kMaximumSpectrumDisplayFrameBytes));

        QVERIFY(!spectrumDisplayCost(0, 1, false));
        QVERIFY(!spectrumDisplayCost(1, 0, false));
        QVERIFY(!spectrumDisplayCost(DisplayCodecEncoder::kMaxSamplesPerPlane + 1, 1, false));
        QVERIFY(!spectrumDisplayCost(1, 61, false));

        QString error;
        const QList<QByteArray> chunks = Ps3DisplayCodec::encode(maximumPs3Snapshot(), &error);
        QVERIFY2(!chunks.isEmpty(), qPrintable(error));
        QCOMPARE(chunks.size(), 3);
        quint64 encodedBytes = 0;
        for (const QByteArray& chunk : chunks) {
            QVERIFY(chunk.size() > 0);
            QVERIFY(chunk.size() <= static_cast<int>(kMaximumPs3DisplayChunkBytes));
            encodedBytes += static_cast<quint64>(chunk.size());
        }
        QCOMPARE(encodedBytes, quint64{147'648});

        const DisplayBudgetCharge ps3 = ps3DisplayCharge();
        QCOMPARE(ps3.applicationBytesPerSecond,
                 encodedBytes * (1'000 / kPs3DisplayPollIntervalMs));
        QCOMPARE(ps3.spectrumSampleUnitsPerSecond, quint64{0});
        QCOMPARE(ps3.messagesPerSecond,
                 static_cast<quint32>(chunks.size() * (1'000 / kPs3DisplayPollIntervalMs)));
        QCOMPARE(ps3.messagesPerSecond, quint32{30});
    }

    // R-R3-03: the frame sizes the Core logs are the codec's worst case. A
    // keyframe is exactly 42 + 2*A(pixels) + A(wide), with
    // A(n) = 3 + 5*ceil(n/128) + n, whatever the row holds; no delta is
    // larger. At 876 bytes per SCTP fragment, 1024/1024/768 is 2,977 bytes
    // in 4 fragments and 4096/4096/768 is 9,361 bytes in 11.
    void keyframesMatchTheSizeRuleAndDeltasNeverExceedThem()
    {
        QCOMPARE(kDisplayCodecHeaderBytes + 2 * displayCodecWorstCasePlaneBytes(1024)
                     + displayCodecWorstCasePlaneBytes(768),
                 quint32{2'977});
        QCOMPARE(IMediaTransport::sctpFragmentCount(2'977), quint64{4});
        QCOMPARE(kDisplayCodecHeaderBytes + 2 * displayCodecWorstCasePlaneBytes(4096)
                     + displayCodecWorstCasePlaneBytes(768),
                 quint32{9'361});
        QCOMPARE(IMediaTransport::sctpFragmentCount(9'361), quint64{11});

        QRandomGenerator random(0x52523303u);
        const QList<int> pixelCounts{1, 2, 16, 17, 127, 128, 129, 255, 256, 700,
                                     1024, 2048, 3001, 4095, 4096};
        const QList<int> wideCounts{0, 1, 128, 129, 500, 768};
        int shapes = 0;
        for (int pixels : pixelCounts) {
            for (int wide : wideCounts) {
                const quint32 keyframeBytes = kDisplayCodecHeaderBytes
                    + 2 * displayCodecWorstCasePlaneBytes(quint32(pixels))
                    + displayCodecWorstCasePlaneBytes(quint32(wide));
                for (int character = 0; character < 3; ++character) {
                    DisplayCodecEncoder encoder;
                    DisplayCodecFrame frame = spectrumFrame(pixels, wide);
                    for (quint32 sequence = 1; sequence <= 8; ++sequence) {
                        frame.encoderSequence = sequence;
                        frame.producerTimestamp = sequence;
                        frame.waterfallAdvance = (sequence % 2) == 0;
                        // Later frames change character, so deltas see both
                        // small residuals and full-range jumps.
                        const int rowCharacter = (character + int(sequence / 3)) % 3;
                        fillRow(frame.traceDbm, random, rowCharacter);
                        fillRow(frame.waterfallDbm, random, rowCharacter);
                        fillRow(frame.wideDbm, random, rowCharacter);
                        const bool keyframe = sequence == 1 || sequence == 6;
                        const QByteArray encoded = encoder.encode(frame, keyframe);
                        QVERIFY(!encoded.isEmpty());
                        QCOMPARE((static_cast<quint8>(encoded.at(5)) & 0x01) != 0, keyframe);
                        if (keyframe) {
                            QCOMPARE(quint32(encoded.size()), keyframeBytes);
                        } else {
                            QVERIFY2(quint32(encoded.size()) <= keyframeBytes,
                                     qPrintable(QStringLiteral("%1/%2 delta %3 > %4")
                                                    .arg(pixels).arg(wide)
                                                    .arg(encoded.size()).arg(keyframeBytes)));
                        }
                    }
                }
                ++shapes;
            }
        }
        QCOMPARE(shapes, int(pixelCounts.size() * wideCounts.size()));
    }

    void checkedLedgerAndDescriptorValidation()
    {
        QVERIFY(!limits(0, 1).isValid());
        QVERIFY(!limits(1, 0).isValid());
        QVERIFY(!limits(1, 1, 0).isValid());
        QVERIFY(!limits(kDisplayBudgetJsonSafePositiveLimit + 1, 1).isValid());
        QVERIFY(limits(kDisplayBudgetJsonSafePositiveLimit,
                       kDisplayBudgetJsonSafePositiveLimit).isValid());

        const auto summed = sumDisplayCharges({{1, 2, 3}, {4, 5, 6}});
        QVERIFY(summed);
        QCOMPARE(*summed, (DisplayBudgetCharge{5, 7, 9}));
        QVERIFY(!sumDisplayCharges({{std::numeric_limits<quint64>::max(), 0, 0},
                                    {1, 0, 0}}));
        QVERIFY(!sumDisplayCharges({{0, 0, std::numeric_limits<quint32>::max()},
                                    {0, 0, 1}}));

        const DisplayBudgetLimits cap = limits(100, 200);
        QVERIFY(displayChargeFits(cap, {100, 200, kDisplaySenderMessagesPerSecond}));
        QVERIFY(!displayChargeFits(cap, {101, 200, 1}));
        QVERIFY(!displayChargeFits(cap, {100, 201, 1}));
        QVERIFY(!displayChargeFits(cap, {100, 200, kDisplaySenderMessagesPerSecond + 1}));
    }

    void pacerEnforcesFixedBurstAndMonotonicRefill()
    {
        const SpectrumDisplayCost maximum = cost(DisplayCodecEncoder::kMaxSamplesPerPlane,
                                                  1, true);
        const DisplayBudgetLimits cap = limits(maximum.charge.applicationBytesPerSecond,
                                               maximum.charge.spectrumSampleUnitsPerSecond);
        DisplayBudgetPacer pacer;
        QVERIFY(pacer.beginSession(7, cap, 0));
        QVERIFY(pacer.update(cap, maximum.charge, false, 0));

        QVERIFY(pacer.spendSpectrum(maximum.maximumFrameBytes,
                                    maximum.maximumFrameSampleUnits, 0));
        // A failed transport call after the debit is deliberately not a refund.
        QVERIFY(!pacer.canSpendSpectrum(maximum.maximumFrameBytes,
                                        maximum.maximumFrameSampleUnits, 0));
        QVERIFY(!pacer.canSpendSpectrum(maximum.maximumFrameBytes,
                                        maximum.maximumFrameSampleUnits, -1));
        QVERIFY(!pacer.canSpendSpectrum(maximum.maximumFrameBytes,
                                        maximum.maximumFrameSampleUnits, 999'999'999));
        QVERIFY(pacer.canSpendSpectrum(maximum.maximumFrameBytes,
                                       maximum.maximumFrameSampleUnits, 1'000'000'000));
        QVERIFY(pacer.spendSpectrum(maximum.maximumFrameBytes,
                                    maximum.maximumFrameSampleUnits, 1'000'000'000));

        // The elapsed time is capped at a fixed one-message burst, no matter
        // how long the input has been idle.
        QVERIFY(pacer.canSpendSpectrum(maximum.maximumFrameBytes,
                                       maximum.maximumFrameSampleUnits,
                                       std::numeric_limits<qint64>::max()));
        QVERIFY(pacer.spendSpectrum(maximum.maximumFrameBytes,
                                    maximum.maximumFrameSampleUnits,
                                    std::numeric_limits<qint64>::max()));
        QVERIFY(!pacer.canSpendSpectrum(maximum.maximumFrameBytes,
                                        maximum.maximumFrameSampleUnits,
                                        std::numeric_limits<qint64>::max()));
    }

    void updatesAccrueOldRatesAndNeverCreateCredit()
    {
        const SpectrumDisplayCost oldCost = cost(1, 1, false);
        const SpectrumDisplayCost newCost = cost(2, 1, false);
        const DisplayBudgetLimits oldLimits = limits(oldCost.charge.applicationBytesPerSecond,
                                                     oldCost.charge.spectrumSampleUnitsPerSecond);
        const DisplayBudgetLimits newLimits = limits(newCost.charge.applicationBytesPerSecond,
                                                     newCost.charge.spectrumSampleUnitsPerSecond,
                                                     2);
        DisplayBudgetPacer pacer;
        QVERIFY(pacer.beginSession(9, oldLimits, 0));
        QVERIFY(pacer.update(oldLimits, oldCost.charge, false, 0));
        QVERIFY(pacer.spendSpectrum(kMaximumSpectrumDisplayFrameBytes,
                                    kMaximumSpectrumDisplayFrameSampleUnits, 0));

        // At this update boundary exactly half of the old 60-byte / 2-sample
        // rate has accrued. The next half second uses the new 62-byte / 4-sample rate.
        QVERIFY(pacer.update(newLimits, newCost.charge, false, 500'000'000));
        QVERIFY(pacer.spendSpectrum(30, 1, 500'000'000));
        QVERIFY(pacer.canSpendSpectrum(31, 2, 1'000'000'000));
        QVERIFY(pacer.spendSpectrum(31, 2, 1'000'000'000));

        // Repeated updates with unchanged subscriptions preserve the drained
        // buckets; they never install a fresh burst.
        QVERIFY(pacer.update(newLimits, newCost.charge, false, 1'000'000'000));
        QVERIFY(!pacer.canSpendSpectrum(kMaximumSpectrumDisplayFrameBytes,
                                        kMaximumSpectrumDisplayFrameSampleUnits,
                                        1'000'000'000));
    }

    void runtimeSampleCapBindsExistingSpectrumReservation()
    {
        const SpectrumDisplayCost maximum = cost(DisplayCodecEncoder::kMaxSamplesPerPlane,
                                                  1, true);
        const DisplayBudgetLimits initial = limits(maximum.charge.applicationBytesPerSecond,
                                                   maximum.charge.spectrumSampleUnitsPerSecond);
        const DisplayBudgetLimits reduced = limits(maximum.charge.applicationBytesPerSecond,
                                                   100, 2);
        DisplayBudgetPacer pacer;
        QVERIFY(pacer.beginSession(10, initial, 0));
        QVERIFY(pacer.update(initial, maximum.charge, false, 0));
        QVERIFY(pacer.spendSpectrum(kMaximumSpectrumDisplayFrameBytes,
                                    kMaximumSpectrumDisplayFrameSampleUnits, 0));

        // The update earns 4,480 historical units at the old rate. Drain
        // those first, then prove each later second earns only the new 100/s
        // effective cap even though the retained reservation remains 8,960/s.
        QVERIFY(pacer.update(reduced, maximum.charge, false, 500'000'000));
        QVERIFY(pacer.spendSpectrum(1, 4'480, 500'000'000));
        QVERIFY(!pacer.canSpendSpectrum(101, 101, 1'500'000'000));
        QVERIFY(pacer.spendSpectrum(100, 100, 1'500'000'000));
        qInfo().nospace() << "DisplayBudget reduced sample cap: reservation="
                          << maximum.charge.spectrumSampleUnitsPerSecond
                          << " effective=100 earned=100";
    }

    // R-R3-37 (final review, the ceiling run): under a budget with room to
    // spare, spectrum is paced to the budget, not to the admitted charge. The
    // app's plan for eight wide 4096-pixel pans under the Core's computed
    // ceiling is 200 frames a second, one per 5 ms sender tick; ticks that
    // come 0.5 ms early or late must still each carry a frame, as they do
    // without a budget.
    void spectrumIsPacedToTheBudgetNotTheAdmittedCharge()
    {
        const SpectrumDisplayCost active = cost(DisplayCodecEncoder::kMaxSamplesPerPlane, 60, true);
        const SpectrumDisplayCost background
            = cost(DisplayCodecEncoder::kMaxSamplesPerPlane, 20, true);
        QList<DisplayBudgetCharge> plan{active.charge};
        QList<DisplayBudgetCharge> ceilingCharges{ps3DisplayCharge()};
        for (int pan = 0; pan < 8; ++pan) {
            if (pan > 0) { plan.append(background.charge); }
            ceilingCharges.append(active.charge);
        }
        const DisplayBudgetCharge charge = *sumDisplayCharges(plan);
        QCOMPARE(charge.messagesPerSecond, kDisplaySenderMessagesPerSecond);
        const DisplayBudgetCharge ceilingCharge = *sumDisplayCharges(ceilingCharges);
        const DisplayBudgetLimits ceiling = limits(ceilingCharge.applicationBytesPerSecond,
                                                   ceilingCharge.spectrumSampleUnitsPerSecond);
        QVERIFY(displayChargeFits(ceiling, charge));

        const auto sendsInOneSecond = [&](const DisplayBudgetLimits& budget) {
            DisplayBudgetPacer pacer;
            if (!pacer.beginSession(11, budget, 0) || !pacer.update(budget, charge, false, 0)) {
                return -1;
            }
            int sent = 0;
            qint64 now = 0;
            for (int tick = 0; tick < 200; ++tick) {
                now += tick % 2 == 0 ? 4'500'000 : 5'500'000;
                if (pacer.spendSpectrum(active.maximumFrameBytes,
                                        active.maximumFrameSampleUnits, now)) {
                    ++sent;
                }
            }
            return sent;
        };
        // Every tick carries a frame under the ceiling.
        QCOMPARE(sendsInOneSecond(ceiling), 200);
        // A budget equal to the charge still binds: an early tick has not
        // earned a whole frame yet.
        const int atCharge = sendsInOneSecond(
            limits(charge.applicationBytesPerSecond, charge.spectrumSampleUnitsPerSecond));
        QVERIFY2(atCharge < 200, qPrintable(QString::number(atCharge)));

        // PureSignal's display keeps its share: spectrum may use the budget
        // less PureSignal's bytes, and no more.
        const SpectrumDisplayCost small = cost(128, 10, false);
        const quint64 room = 4 * quint64(kMaximumSpectrumDisplayFrameBytes);
        const DisplayBudgetLimits withPs3
            = limits(ps3DisplayCharge().applicationBytesPerSecond + room,
                     kDisplayBudgetJsonSafePositiveLimit);
        DisplayBudgetPacer pacer;
        QVERIFY(pacer.beginSession(12, withPs3, 0));
        QVERIFY(pacer.update(withPs3, small.charge, true, 0));
        quint64 spent = 0;
        for (qint64 now = 0; now <= 1'000'000'000; now += 1'000'000) {
            if (pacer.spendSpectrum(kMaximumSpectrumDisplayFrameBytes, 1, now)) {
                spent += kMaximumSpectrumDisplayFrameBytes;
            }
        }
        // One burst plus one second at the room left beside PureSignal.
        QVERIFY(spent > small.charge.applicationBytesPerSecond);
        QVERIFY(spent <= room + kMaximumSpectrumDisplayFrameBytes);
    }

    void longIdleOverflowSaturatesPortablyAtExactBoundary()
    {
        constexpr quint64 kHighRate = 2'000'000'001ULL;
        // The spectrum bucket was emptied by the 9,361-byte frame; global
        // credit still has room. At this rate 4,680 ns earns only 9,360 bytes.
        constexpr qint64 kFullBurstAtNs = 4'681;
        // This duration gives (kHighRate * elapsed / 1e9) == 2^64 exactly.
        // A narrowing implementation wraps the quotient to zero instead of
        // filling the fixed burst bucket.
        constexpr qint64 kOverflowIdleNs = 9'223'372'032'243'089'792LL;
        const DisplayBudgetLimits cap = limits(kHighRate, kHighRate);
        const DisplayBudgetCharge charge{kHighRate, kHighRate, 1};
        DisplayBudgetPacer pacer;
        QVERIFY(pacer.beginSession(10, cap, 0));
        QVERIFY(pacer.update(cap, charge, false, 0));
        QVERIFY(pacer.spendSpectrum(kMaximumSpectrumDisplayFrameBytes,
                                    kMaximumSpectrumDisplayFrameSampleUnits, 0));

        QVERIFY(!pacer.canSpendSpectrum(kMaximumSpectrumDisplayFrameBytes,
                                        kMaximumSpectrumDisplayFrameSampleUnits,
                                        kFullBurstAtNs - 1));
        QVERIFY(pacer.canSpendSpectrum(kMaximumSpectrumDisplayFrameBytes,
                                       kMaximumSpectrumDisplayFrameSampleUnits,
                                       kFullBurstAtNs));
        QVERIFY(pacer.spendSpectrum(kMaximumSpectrumDisplayFrameBytes,
                                    kMaximumSpectrumDisplayFrameSampleUnits,
                                    kFullBurstAtNs));

        DisplayBudgetPacer overflowPacer;
        QVERIFY(overflowPacer.beginSession(11, cap, 0));
        QVERIFY(overflowPacer.update(cap, charge, false, 0));
        QVERIFY(overflowPacer.spendSpectrum(kMaximumSpectrumDisplayFrameBytes,
                                            kMaximumSpectrumDisplayFrameSampleUnits, 0));
        QVERIFY(overflowPacer.canSpendSpectrum(kMaximumSpectrumDisplayFrameBytes,
                                               kMaximumSpectrumDisplayFrameSampleUnits,
                                               kOverflowIdleNs));
        qInfo().nospace() << "DisplayBudget portable long-idle saturation: rate="
                          << kHighRate << " boundaryNs=" << kFullBurstAtNs
                          << " overflowIdleNs=" << kOverflowIdleNs;
    }

    void classGatesEpochsAndInvalidCallsPreserveAccounting()
    {
        const SpectrumDisplayCost small = cost(1, 1, false);
        const DisplayBudgetLimits smallLimits = limits(small.charge.applicationBytesPerSecond,
                                                        small.charge.spectrumSampleUnitsPerSecond);
        DisplayBudgetPacer pacer;
        QVERIFY(pacer.beginSession(11, smallLimits, 0));
        QVERIFY(!pacer.canSpendSpectrum(1, 1, 0));
        QVERIFY(!pacer.canSpendPs3(1, 0));
        QVERIFY(!pacer.update(smallLimits, {1, 0, 1}, false, 0));
        QVERIFY(!pacer.update(smallLimits,
                              {kDisplayBudgetJsonSafePositiveLimit + 1, 1, 1}, false, 0));
        QVERIFY(pacer.update(smallLimits, small.charge, false, 0));
        QVERIFY(pacer.spendSpectrum(kMaximumSpectrumDisplayFrameBytes,
                                    kMaximumSpectrumDisplayFrameSampleUnits, 0));

        // Invalid costs neither earn time nor alter a partially drained bucket.
        QVERIFY(!pacer.spendSpectrum(0, 1, 1'000'000'000));
        QVERIFY(!pacer.canSpendSpectrum(31, 1, 500'000'000));
        QVERIFY(pacer.canSpendSpectrum(30, 1, 500'000'000));

        pacer.endSession();
        QVERIFY(!pacer.canSpendSpectrum(1, 1, 1'000'000'000));
        QVERIFY(!pacer.beginSession(11, smallLimits, 1'000'000'000));
        QVERIFY(pacer.beginSession(12, smallLimits, 1'000'000'000));
        QVERIFY(!pacer.canSpendSpectrum(1, 1, 1'000'000'000));
    }

    // R-R3-08/37: PureSignal's own bucket holds one whole worst-case
    // snapshot, so its chunks go out on consecutive sender ticks; one chunk
    // of room lost credit each poll and dropped about one snapshot in
    // twenty. A roomy global budget keeps the global bucket out of the way.
    void ps3BurstIsOneWholeSnapshot()
    {
        const DisplayBudgetCharge ps3 = ps3DisplayCharge();
        const quint64 snapshotBytes = ps3.applicationBytesPerSecond
            / (1'000 / kPs3DisplayPollIntervalMs);
        QVERIFY(snapshotBytes > 2 * quint64{kMaximumPs3DisplayChunkBytes});
        const DisplayBudgetLimits roomy = limits(100 * ps3.applicationBytesPerSecond, 1);
        DisplayBudgetPacer pacer;
        QVERIFY(pacer.beginSession(14, roomy, 0));
        QVERIFY(pacer.update(roomy, {}, true, 0));
        // Three chunks on three 5 ms ticks: the whole snapshot.
        QVERIFY(pacer.spendPs3(kMaximumPs3DisplayChunkBytes, 0));
        QVERIFY(pacer.spendPs3(kMaximumPs3DisplayChunkBytes, 5'000'000));
        QVERIFY(pacer.spendPs3(snapshotBytes - 2 * quint64{kMaximumPs3DisplayChunkBytes},
                               10'000'000));
        // Then nothing more than the charge accrues: the next worst-case
        // snapshot's first chunk waits.
        QVERIFY(!pacer.canSpendPs3(kMaximumPs3DisplayChunkBytes, 15'000'000));
        // A full poll interval later the whole next snapshot has room again.
        QVERIFY(pacer.spendPs3(kMaximumPs3DisplayChunkBytes, 110'000'000));
        QVERIFY(pacer.spendPs3(kMaximumPs3DisplayChunkBytes, 115'000'000));
    }

    void ps3DisableReenableRetainsItsDrainedBucket()
    {
        const DisplayBudgetCharge ps3 = ps3DisplayCharge();
        // A roomy global budget, so only PureSignal's own bucket is drained.
        const DisplayBudgetLimits cap = limits(100 * ps3.applicationBytesPerSecond, 1);
        const quint64 snapshotBytes = ps3.applicationBytesPerSecond
            / (1'000 / kPs3DisplayPollIntervalMs);
        DisplayBudgetPacer pacer;
        QVERIFY(pacer.beginSession(13, cap, 0));
        QVERIFY(pacer.update(cap, {}, true, 0));
        // Drain the whole snapshot burst.
        QVERIFY(pacer.spendPs3(kMaximumPs3DisplayChunkBytes, 0));
        QVERIFY(pacer.spendPs3(kMaximumPs3DisplayChunkBytes, 5'000'000));
        QVERIFY(pacer.spendPs3(snapshotBytes - 2 * quint64{kMaximumPs3DisplayChunkBytes},
                               10'000'000));
        QVERIFY(pacer.update(cap, {}, false, 10'000'000));
        QVERIFY(pacer.update(cap, {}, true, 1'000'000'000));
        QVERIFY(!pacer.canSpendPs3(kMaximumPs3DisplayChunkBytes, 1'000'000'000));
        QVERIFY(pacer.canSpendPs3(kMaximumPs3DisplayChunkBytes, 2'000'000'000));

        // Structural sender admission is separate from byte pacing.
        QVERIFY(!pacer.update(cap, {1, 1, kDisplaySenderMessagesPerSecond - 29},
                              true, 2'000'000'000));
    }

    void displayExtrasShareByteCreditWithoutInventingSamples()
    {
        const SpectrumDisplayCost maximum = cost(DisplayCodecEncoder::kMaxSamplesPerPlane, 1, true);
        const DisplayBudgetLimits cap = limits(maximum.charge.applicationBytesPerSecond,
                                               maximum.charge.spectrumSampleUnitsPerSecond);
        constexpr quint64 kScalarBytes = kDisplayExtrasHeaderBytes + 2 * sizeof(float);
        DisplayBudgetPacer spectrum;
        QVERIFY(spectrum.beginSession(16, cap, 0));
        QVERIFY(spectrum.update(cap, maximum.charge, false, 0));
        QVERIFY(!spectrum.spendSpectrum(kScalarBytes, 0, 0));
        QVERIFY(!spectrum.canSpendSpectrum(kScalarBytes, 0, 0));
        QVERIFY(spectrum.spendDisplayExtras(kScalarBytes, 0, 0));
        // Extras consume the same spectrum-byte burst, but leave all sample credit.
        QVERIFY(!spectrum.canSpendSpectrum(maximum.maximumFrameBytes,
                                           maximum.maximumFrameSampleUnits, 0));
        QVERIFY(spectrum.spendSpectrum(maximum.maximumFrameBytes - kScalarBytes,
                                       maximum.maximumFrameSampleUnits, 0));
        QVERIFY(!spectrum.spendDisplayExtras(1, 0, 0));

        const DisplayBudgetCharge ps3 = ps3DisplayCharge();
        DisplayBudgetPacer global;
        const DisplayBudgetLimits roomy = limits(100 * ps3.applicationBytesPerSecond,
                                                  maximum.charge.spectrumSampleUnitsPerSecond);
        QVERIFY(global.beginSession(17, roomy, 0));
        QVERIFY(global.update(roomy, maximum.charge, true, 0));
        QVERIFY(global.spendDisplayExtras(kScalarBytes, 0, 0));
        QVERIFY(global.spendPs3(kMaximumDisplayMessageBytes - kScalarBytes, 0));
        // Spectrum still has byte/sample room; the common global bucket is empty.
        QVERIFY(!global.spendDisplayExtras(1, 0, 0));
    }

    void displayExtrasDebitRealPeakHoldSamples()
    {
        const SpectrumDisplayCost maximum = cost(DisplayCodecEncoder::kMaxSamplesPerPlane, 1, true);
        const DisplayBudgetLimits cap = limits(maximum.charge.applicationBytesPerSecond,
                                               maximum.charge.spectrumSampleUnitsPerSecond);
        DisplayBudgetPacer pacer;
        QVERIFY(pacer.beginSession(18, cap, 0));
        QVERIFY(pacer.update(cap, maximum.charge, false, 0));
        constexpr quint64 kHoldSamples = 128;
        const quint64 holdBytes = displayExtrasWorstCaseBytes(kDisplayExtrasPeakHold, kHoldSamples);
        QVERIFY(pacer.spendDisplayExtras(holdBytes, kHoldSamples, 0));
        QVERIFY(!pacer.canSpendSpectrum(1, maximum.maximumFrameSampleUnits - kHoldSamples + 1, 0));
        QVERIFY(pacer.spendSpectrum(1, maximum.maximumFrameSampleUnits - kHoldSamples, 0));
        QVERIFY(!pacer.spendDisplayExtras(1, 1, 0));
        // A scalar-only message remains valid even when sample credit is exhausted.
        QVERIFY(pacer.spendDisplayExtras(1, 0, 0));
    }

    void displayExtrasPreserveBoundsEpochsAndInvalidCallAccounting()
    {
        const SpectrumDisplayCost maximum = cost(DisplayCodecEncoder::kMaxSamplesPerPlane, 1, true);
        const DisplayBudgetLimits cap = limits(maximum.charge.applicationBytesPerSecond,
                                               maximum.charge.spectrumSampleUnitsPerSecond);
        DisplayBudgetPacer pacer;
        QVERIFY(!pacer.spendDisplayExtras(1, 0, 0));
        QVERIFY(pacer.beginSession(19, cap, 0));
        QVERIFY(!pacer.spendDisplayExtras(1, 0, 0));
        QVERIFY(pacer.update(cap, maximum.charge, false, 0));
        QVERIFY(!pacer.spendDisplayExtras(1, 0, -1));
        QVERIFY(!pacer.spendDisplayExtras(0, 0, 0));
        QVERIFY(!pacer.spendDisplayExtras(quint64{kMaximumSpectrumDisplayFrameBytes} + 1, 0, 0));
        QVERIFY(!pacer.spendDisplayExtras(1, quint64{kMaximumSpectrumDisplayFrameSampleUnits} + 1, 0));
        QVERIFY(pacer.spendDisplayExtras(kMaximumSpectrumDisplayFrameBytes, 0, 0));
        // Invalid calls must not earn future time or refill the drained bucket.
        QVERIFY(!pacer.spendDisplayExtras(0, 0, 1'000'000'000));
        QVERIFY(!pacer.spendSpectrum(1, 0, 1'000'000'000));
        const quint64 half = maximum.maximumFrameBytes / 2;
        QVERIFY(!pacer.spendDisplayExtras(half + 1, 0, 500'000'000));
        QVERIFY(pacer.spendDisplayExtras(half, 0, 500'000'000));
        QVERIFY(!pacer.spendDisplayExtras(1, 0, 400'000'000));
        // Long idle saturates at the same fixed burst, never beyond it.
        QVERIFY(pacer.spendDisplayExtras(kMaximumSpectrumDisplayFrameBytes,
                                         kMaximumSpectrumDisplayFrameSampleUnits,
                                         std::numeric_limits<qint64>::max()));
        QVERIFY(!pacer.spendDisplayExtras(1, 0, std::numeric_limits<qint64>::max()));
        pacer.endSession();
        QVERIFY(!pacer.spendDisplayExtras(1, 0, std::numeric_limits<qint64>::max()));
        QVERIFY(!pacer.beginSession(19, cap, 0));
        QVERIFY(pacer.beginSession(20, cap, 0));
        QVERIFY(!pacer.spendDisplayExtras(1, 0, 0));
        QVERIFY(pacer.update(cap, maximum.charge, false, 0));
        QVERIFY(pacer.update(cap, {}, false, 0));
        QVERIFY(!pacer.spendDisplayExtras(1, 0, 0));
    }

    void rawIqDebitsGlobalBytesWithFixedBurst()
    {
        constexpr quint64 iqRate = 192000ULL * 8 + 24ULL * ((192000 + 1023) / 1024);
        const DisplayBudgetLimits cap = limits(iqRate, 1);
        DisplayBudgetPacer pacer;
        QVERIFY(pacer.beginSession(15, cap, 0));
        QVERIFY(pacer.update(cap, {}, false, 0, iqRate));
        constexpr quint64 frame = 24 + 1024 * 8;
        QVERIFY(pacer.spendIq(frame, 0));
        QVERIFY(pacer.spendIq(frame, 0));
        QVERIFY(pacer.spendIq(frame, 0));
        QVERIFY(!pacer.canSpendIq(frame, 0));
        QVERIFY(pacer.update(cap, {}, false, 0, iqRate));
        QVERIFY(!pacer.canSpendIq(frame, 0));
        QVERIFY(pacer.spendIq(frame, 1'000'000'000));
        QVERIFY(!pacer.canSpendSpectrum(1, 1, 1'000'000'000));
    }
};

QTEST_APPLESS_MAIN(TestDisplayBudget)
#include "tst_display_budget.moc"
