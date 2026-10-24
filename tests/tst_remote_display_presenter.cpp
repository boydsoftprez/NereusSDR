// no-port-check: NereusSDR-original.
// =================================================================
// tests/tst_remote_display_presenter.cpp  (NereusSDR)
// =================================================================
//
// R-R3-21 / R-R3-08: a remote window's display presented on its audio's
// playout clock, and ridden through a stalling link. Pure arithmetic on
// injected times: the presentation map from one audio measurement, how it
// follows the audio's delay, and the presenter's queue, gap rows and
// repeats.
//
// Modification history (NereusSDR):
//   2026-10-22: NSDC v2: frames encoded by encodeDisplayCodecV2, lost and
//               swapped under the codec's loss model and decoded, draw one
//               row a slot with one blended row a lost row; push() keeps
//               the measurementCurrent it is given. J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
//   2026-10-21: the rows each push() queues, for the early hand-off.
//               J.J. Boyd (KG4VCF), AI-assisted via Claude Code.
//   2026-09-26: created for R-R3-21 / R-R3-08. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
// =================================================================

#include "gui/RemoteDisplayPresenter.h"
#include "gui/SpectrumWidget.h"
#include "core/session/media/DisplayBudget.h"
#include "core/session/media/IMediaTransport.h"
#include "core/session/media/SpectrumEndpoint.h"

#include "DisplayCodecV2Vectors.h"

#include <QSignalSpy>
#include <QtTest>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

using namespace NereusSDR;
namespace Vectors = NereusSDR::Test::DisplayCodecV2Vectors;

namespace {

constexpr qint64 kMs = 1'000'000;
// 30 frames a second, every frame a waterfall row (framesPerLine 1).
constexpr qint64 kPeriodNs = 1'000'000'000 / 30;
// One display tick: the waterfall ticker's period at 30 frames a second.
constexpr qint64 kTickNs = 33 * kMs;

DisplayCodecFrame frameAt(int index, qint64 producerNs, float level)
{
    DisplayCodecFrame frame;
    frame.context = {1, 1, -180, 0, 8, 8, 0};
    frame.encoderSequence = quint32(index + 1);
    frame.producerTimestamp = quint64(producerNs);
    frame.waterfallAdvance = true;
    frame.traceDbm = QVector<float>(8, level);
    frame.waterfallDbm = QVector<float>(8, level);
    return frame;
}

struct Presented {
    RemoteDisplayPresenter::Item item;
    qint64 atNs;
};

// Steps a local clock 1 ms at a time from `fromNs` to `toNs`, taking what
// falls due, as the controller's precise timer would.
void run(RemoteDisplayPresenter& presenter, qint64 fromNs, qint64 toNs,
         std::optional<qint64> map, QList<Presented>& out)
{
    for (qint64 now = fromNs; now <= toNs; now += kMs) {
        for (auto& item : presenter.takeDue(now, map)) {
            out.append({std::move(item), now});
        }
    }
}

// A Core whose clock reads kOffsetNs ahead of this computer's, a clock
// estimate 12 ms off (half an asymmetric round trip), and an audio
// anchor: RTP time kAnchorRtp captured at Core time kAnchorCoreNs.
constexpr qint64 kOffsetNs = 5'000'000'000;
constexpr qint64 kOffsetErrorNs = 12 * kMs;
constexpr quint32 kAnchorRtp = 96'000;
constexpr qint64 kAnchorCoreNs = 70'000'000'000;
constexpr int kPacketFrames = 1920; // 40 ms at 48 kHz

// The audio inputs when packet `n` after the anchor plays `delayNs` after
// the Core captured it (true delay, on the true clocks).
AudioDelayInputs audioPlaying(int n, qint64 delayNs)
{
    AudioDelayInputs inputs;
    inputs.offset = AudioClockOffset{kOffsetNs + kOffsetErrorNs, 30 * kMs, 0, 30 * kMs};
    inputs.capture = AudioCaptureAnchor{7, kAnchorRtp, kAnchorCoreNs};
    inputs.playingGeneration = 7;
    RemoteAudioPlayoutPoint playout;
    playout.rtpTimestamp = kAnchorRtp + quint32(n * kPacketFrames);
    const qint64 capturedCoreNs = kAnchorCoreNs + qint64(n) * 40 * kMs;
    // Nothing queued, no device latency: the sample at rtpTimestamp plays
    // at measuredNs, on this computer's clock.
    playout.measuredNs = capturedCoreNs - kOffsetNs + delayNs;
    inputs.playout = playout;
    return inputs;
}

qint64 audioPlayNs(int n, qint64 delayNs)
{
    return kAnchorCoreNs + qint64(n) * 40 * kMs - kOffsetNs + delayNs;
}

// NSDC v2: the display codec's loss model (tst_display_codec_v2's
// runLoss): each message is dropped by datagram (ceil(bytes / 876)
// datagrams), by Gilbert-Elliott bursts of mean length 3 or at random, both
// at 2 percent; then adjacent delivered messages swap with probability 0.1.
std::vector<QByteArray> deliveredUnderLoss(const std::vector<std::vector<QByteArray>>& messages,
                                           bool bursts, quint64 seed)
{
    constexpr double kLossRate = 0.02;
    constexpr double kGoodFromBad = 1.0 / 3.0;
    constexpr double kBadFromGood = kGoodFromBad * kLossRate / (1.0 - kLossRate);
    constexpr double kSwapProbability = 0.1;
    constexpr int kDatagramBytes = IMediaTransport::kSctpDataPayloadBytes;
    Vectors::SplitMix64 rng(seed);
    bool bad = false;
    std::vector<QByteArray> delivered;
    for (const std::vector<QByteArray>& frame : messages) {
        for (const QByteArray& message : frame) {
            const int datagrams = int((message.size() + kDatagramBytes - 1) / kDatagramBytes);
            bool lost = false;
            for (int d = 0; d < datagrams; ++d) {
                if (bursts) {
                    bad = bad ? rng.uniform() >= kGoodFromBad : rng.uniform() < kBadFromGood;
                    lost = lost || bad;
                } else if (rng.uniform() < kLossRate) {
                    lost = true;
                }
            }
            if (!lost) { delivered.push_back(message); }
        }
    }
    for (size_t i = 0; i + 1 < delivered.size();) {
        if (rng.uniform() < kSwapProbability) {
            std::swap(delivered[i], delivered[i + 1]);
            i += 2;
        } else {
            ++i;
        }
    }
    return delivered;
}

} // namespace

class TstRemoteDisplayPresenter : public QObject {
    Q_OBJECT

private slots:
    void queuedFrameAfterLossKeepsPictureButNotCurrentImd()
    {
        SpectrumWidget widget;
        widget.resize(420, 240);
        widget.setDisplayDuplex(true);
        widget.setMoxOverlay(true);
        widget.setTestingIMD(true);
        widget.setShowIMDMeasurements(true);
        SpectrumEndpointContext context;
        context.codec = {43, 1, -180.0f, 0.0f, 400, 400, 0};
        context.exactCentreHz = 14200000.0;
        context.exactSpanHz = 48000.0;
        widget.setRemoteSpectrumContext(context, 14200000.0, 192000.0);
        const RemoteSpectrumCapture capture{context, 14200000.0, 192000.0};
        const auto frame = [&](quint32 sequence, qint64 producerNs, float level) {
            DisplayCodecFrame result;
            result.context = context.codec;
            result.encoderSequence = sequence;
            result.producerTimestamp = quint64(producerNs);
            result.waterfallAdvance = true;
            result.traceDbm = QVector<float>(400, level);
            result.waterfallDbm = QVector<float>(400, level);
            return result;
        };
        RemoteDisplayPresenter presenter;
        presenter.setRowPeriodNs(kPeriodNs);
        presenter.push(frame(1, kPeriodNs, -90.0f), capture);
        auto due = presenter.takeDue(kPeriodNs, qint64(0));
        QCOMPARE(due.size(), size_t(1));
        QVERIFY(widget.updateRemoteSpectrum(due.front().frame, due.front().capture,
                                            due.front().measurementCurrent));
        QVERIFY(widget.imdMeasurementFrame().context.sourceEligible);
        QSignalSpy measurementChanges(&widget, &SpectrumWidget::imdMeasurementChanged);

        presenter.push(frame(2, 2 * kPeriodNs, -95.0f), capture);
        QVERIFY(presenter.takeDue(kPeriodNs, qint64(0)).empty());
        presenter.noteLoss();
        widget.noteImdReceiverInputLost();
        QCOMPARE(widget.imdMeasurementFrame().status, ImdStatus::WaitingForSpectrum);
        QCOMPARE(measurementChanges.count(), 1);
        const int rowsBefore = widget.remoteRowQueueDepthForTest();
        due = presenter.takeDue(2 * kPeriodNs, qint64(0));
        QCOMPARE(due.size(), size_t(1));
        QCOMPARE(due.front().kind, RemoteDisplayPresenter::Kind::Frame);
        QVERIFY(!due.front().measurementCurrent);
        QVERIFY(presenter.waitingForKeyframe());
        QVERIFY(widget.updateRemoteSpectrum(due.front().frame, due.front().capture,
                                            due.front().measurementCurrent));
        QCOMPARE(widget.renderedPixels().at(0), -95.0f);
        QCOMPARE(widget.remoteRowQueueDepthForTest(), rowsBefore + 1);
        QCOMPARE(widget.imdMeasurementFrame().status, ImdStatus::WaitingForSpectrum);
        QVERIFY(!widget.imdMeasurementFrame().context.sourceEligible);
        QCOMPARE(measurementChanges.count(), 1);

        presenter.push(frame(3, 3 * kPeriodNs, -85.0f), capture); // recovery keyframe
        due = presenter.takeDue(3 * kPeriodNs, qint64(0));
        QCOMPARE(due.size(), size_t(1));
        QVERIFY(!presenter.waitingForKeyframe());
        QVERIFY(due.front().measurementCurrent);
        QVERIFY(widget.updateRemoteSpectrum(due.front().frame, due.front().capture,
                                            due.front().measurementCurrent));
        QVERIFY(widget.imdMeasurementFrame().context.sourceEligible);
        QCOMPARE(measurementChanges.count(), 2);
    }

    void captureSnapshot_survivesDelayBlendRepeatAndRenewal()
    {
        RemoteDisplayPresenter presenter;
        presenter.setRowPeriodNs(kPeriodNs);
        auto first = frameAt(0, kPeriodNs, -100);
        first.context.wideSamples = 8;
        first.wideDbm = QVector<float>(8, -90);
        SpectrumEndpointContext context;
        context.codec = first.context;
        context.source = {7, FftTier::Fine};
        context.sourceGeneration = 0; // Unavailable on the existing wire.
        context.exactCentreHz = 14e6;
        context.exactSpanHz = 24000;
        context.wideCentreHz = 14.001e6;
        context.wideSpanHz = 96000;
        context.wideband = {true, true, 1, 2, 17, 122880000};
        const RemoteSpectrumCapture accepted{context, 14.002e6, 192000};
        presenter.push(first, accepted);
        context.wideCentreHz += 5000; // The next context cannot mutate a queued capture.
        QVERIFY(presenter.takeDue(kPeriodNs, kPeriodNs).empty());
        auto due = presenter.takeDue(2 * kPeriodNs, kPeriodNs);
        QCOMPARE(due.size(), size_t(1));
        QVERIFY(due.front().capture == accepted);
        presenter.noteLoss();
        due = presenter.takeDue(3 * kPeriodNs, kPeriodNs);
        QCOMPARE(due.size(), size_t(1));
        QCOMPARE(due.front().kind, RemoteDisplayPresenter::Kind::Repeated);
        QVERIFY(due.front().capture == accepted);
        auto next = first;
        next.producerTimestamp = 5 * kPeriodNs;
        next.waterfallDbm.fill(-60);
        next.wideDbm.fill(-50);
        presenter.push(next, accepted);
        due = presenter.takeDue(6 * kPeriodNs, kPeriodNs);
        QCOMPARE(due.size(), size_t(3)); // Two remaining gap slots plus the frame.
        QCOMPARE(due.front().kind, RemoteDisplayPresenter::Kind::Blended);
        for (const auto& item : due) { QVERIFY(item.capture == accepted); }
        presenter.push(next, accepted);
        presenter.restartChain(); // Renewal retains every queued immutable capture.
        due = presenter.takeDue(6 * kPeriodNs, kPeriodNs);
        QCOMPARE(due.size(), size_t(1));
        QVERIFY(due.front().capture == accepted);
        presenter.reset();
        QCOMPARE(presenter.queued(), 0);
    }

    void changedCaptureIdentity_neverBlends_data()
    {
        QTest::addColumn<int>("change");
        for (int i = 0; i < 7; ++i) { QTest::newRow(qPrintable(QString::number(i))) << i; }
    }
    void changedCaptureIdentity_neverBlends()
    {
        QFETCH(int, change);
        RemoteDisplayPresenter presenter;
        presenter.setRowPeriodNs(kPeriodNs);
        auto frame = frameAt(0, kPeriodNs, -40);
        SpectrumEndpointContext context;
        context.codec = frame.context;
        context.exactCentreHz = 14e6;
        context.exactSpanHz = 24000;
        context.wideCentreHz = 14e6;
        context.wideSpanHz = 96000;
        double sourceCentreHz = 14e6;
        presenter.push(frame, RemoteSpectrumCapture{context, sourceCentreHz, 192000});
        presenter.takeDue(kPeriodNs, std::nullopt);
        switch (change) {
        case 0: ++context.source.streamIndex; break;
        case 1: context.source.tier = FftTier::Fine; break;
        case 2: ++context.sourceGeneration; break;
        case 3: context.wideCentreHz += 1000; break;
        case 4: context.wideSpanHz *= 2; break;
        case 5: sourceCentreHz += 1000; break;
        case 6: ++context.wideband.sourceGeneration; break;
        }
        frame.producerTimestamp = 4 * kPeriodNs;
        const RemoteSpectrumCapture capture{context, sourceCentreHz, 192000};
        presenter.push(frame, capture);
        const auto due = presenter.takeDue(4 * kPeriodNs, std::nullopt);
        QCOMPARE(due.size(), size_t(1));
        QVERIFY(due.front().capture == capture);
    }

    void changedCodecOrWideGeometry_neverBlendsEqualSizedExactRows_data()
    {
        QTest::addColumn<bool>("codecChange");
        QTest::newRow("generation") << true;
        QTest::newRow("wide-count") << false;
    }
    void changedCodecOrWideGeometry_neverBlendsEqualSizedExactRows()
    {
        QFETCH(bool, codecChange);
        RemoteDisplayPresenter presenter;
        presenter.setRowPeriodNs(kPeriodNs);
        auto first = frameAt(0, kPeriodNs, -40);
        first.context.wideSamples = 8;
        first.wideDbm = QVector<float>(8, -30);
        presenter.push(first, 14e6, 24000);
        QCOMPARE(presenter.takeDue(kPeriodNs, std::nullopt).size(), size_t(1));
        auto next = first;
        next.producerTimestamp = 4 * kPeriodNs;
        if (codecChange) { ++next.context.contextGeneration; }
        else { next.context.wideSamples = 4; next.wideDbm.resize(4); }
        presenter.push(next, 14e6, 24000);
        const auto due = presenter.takeDue(4 * kPeriodNs, std::nullopt);
        QCOMPARE(due.size(), size_t(1));
        QCOMPARE(due.front().kind, RemoteDisplayPresenter::Kind::Frame);
        QCOMPARE(presenter.counters().rowsBlended, quint64(0));
    }

    // A display frame stamped at the same Core time as an audio sample is
    // presented when that sample plays, whatever the clock estimate's error.
    void mapPresentsWithTheAudioAndCancelsTheOffsetError()
    {
        const qint64 delayNs = 180 * kMs;
        const std::optional<qint64> map = audioPresentationMapNs(audioPlaying(25, delayNs));
        QVERIFY(map.has_value());
        const qint64 capturedCoreNs = kAnchorCoreNs + 25 * 40 * kMs;
        QVERIFY2(std::llabs(capturedCoreNs + *map - audioPlayNs(25, delayNs)) <= 1,
                 qPrintable(QString::number(capturedCoreNs + *map - audioPlayNs(25, delayNs))));
        // Another context's capture, or no offset: nothing.
        AudioDelayInputs other = audioPlaying(25, delayNs);
        other.playingGeneration = 8;
        QVERIFY(!audioPresentationMapNs(other));
        other = audioPlaying(25, delayNs);
        other.offset.reset();
        QVERIFY(!audioPresentationMapNs(other));
    }

    // Sync on a clean link, after a stall deepens the audio's hold by
    // 300 ms, and as it sheds back 40 ms at a time: a display frame and the
    // audio with the same capture time are presented within one tick.
    void displayStaysWithTheAudioThroughAStallAndItsEasing()
    {
        DisplayDelayFollower follower;
        RemoteDisplayPresenter presenter;
        presenter.setRowPeriodNs(kPeriodNs);
        // Audio delay over time: 180 ms clean, 480 ms after the stall at
        // packet 50, then shed 40 ms every 25 packets (1 s) back to 180,
        // with +-4 ms of playout-reading jitter throughout.
        // The jitter hold is the step: the delay less a constant 100 ms.
        const auto holdAt = [](int n) -> qint64 {
            qint64 delay = 180 * kMs;
            if (n >= 50) {
                const int sheds = std::max(0, (n - 100) / 25 + 1);
                delay = std::max<qint64>(180 * kMs, 480 * kMs - qint64(sheds) * 40 * kMs);
            }
            return delay - 100 * kMs;
        };
        const auto delayAt = [&](int n) -> qint64 {
            return holdAt(n) + 100 * kMs + ((n % 3) - 1) * 4 * kMs;
        };
        qint64 worstNs = 0;
        int checked = 0;
        for (int n = 0; n < 500; ++n) {
            const qint64 delayNs = delayAt(n);
            const AudioDelayInputs inputs = audioPlaying(n, delayNs);
            const qint64 nowNs = inputs.playout->measuredNs;
            const std::optional<qint64> measured = audioPresentationMapNs(inputs);
            QVERIFY(measured);
            follower.observe(*measured, nowNs, 4 * kMs, holdAt(n));
            // The display frame captured with this audio packet.
            const qint64 producerNs = kAnchorCoreNs + qint64(n) * 40 * kMs;
            const qint64 presentNs = producerNs + *follower.mapNs();
            const qint64 errorNs = std::llabs(presentNs - audioPlayNs(n, delayNs));
            worstNs = std::max(worstNs, errorNs);
            ++checked;
        }
        QCOMPARE(checked, 500);
        QVERIFY2(worstNs < kTickNs,
                 qPrintable(QStringLiteral("worst %1 ms").arg(double(worstNs) / kMs)));
    }

    void followerTakesStepsAtOnceAndSmoothsJitter()
    {
        DisplayDelayFollower follower;
        QVERIFY(!follower.mapNs());
        follower.observe(200 * kMs, 0);
        QCOMPARE(*follower.mapNs(), 200 * kMs);
        // Jitter below the step moves it a little, not all the way.
        follower.observe(205 * kMs, 50 * kMs);
        QVERIFY(*follower.mapNs() > 200 * kMs && *follower.mapNs() < 202 * kMs);
        // A deepened hold is taken at once; so is a shed.
        follower.observe(201 * kMs, 60 * kMs, 5 * kMs, 80 * kMs);
        follower.observe(500 * kMs, 100 * kMs, 5 * kMs, 380 * kMs);
        QCOMPARE(*follower.mapNs(), 500 * kMs);
        follower.observe(460 * kMs, 150 * kMs, 5 * kMs, 340 * kMs);
        QCOMPARE(*follower.mapNs(), 460 * kMs);
        // Without a hold change one far reading waits for the next to agree.
        follower.observe(520 * kMs, 200 * kMs, 5 * kMs, 340 * kMs);
        QCOMPARE(*follower.mapNs(), 460 * kMs);
        follower.observe(521 * kMs, 250 * kMs, 5 * kMs, 340 * kMs);
        QCOMPARE(*follower.mapNs(), 521 * kMs);
        // A far reading the next one does not confirm is noise.
        follower.observe(600 * kMs, 300 * kMs, 5 * kMs, 340 * kMs);
        follower.observe(522 * kMs, 350 * kMs, 5 * kMs, 340 * kMs);
        QVERIFY(*follower.mapNs() >= 521 * kMs && *follower.mapNs() < 523 * kMs);
        follower.reset();
        QVERIFY(!follower.mapNs());
    }

    // Review Important 2: a 1024-frame device quantum (PipeWire's default at
    // 48 kHz, 21 ms) or a Bluetooth sink makes the playout reading swing
    // +-11 ms from one reading to the next with no change of delay. The rows
    // still come out one a period: none bunch into pairs, none skip a tick.
    void playoutReadingJitterDoesNotBunchRows()
    {
        DisplayDelayFollower follower;
        RemoteDisplayPresenter presenter;
        presenter.setRowPeriodNs(kPeriodNs);
        const qint64 map = 200 * kMs;
        const qint64 accuracy = 11 * kMs; // half a 1024-frame callback
        const qint64 hold = 80 * kMs;
        QList<Presented> shown;
        for (int i = 0; i < 90; ++i) {
            presenter.push(frameAt(i, i * kPeriodNs, -100.0f), 14.0e6, 24000.0);
        }
        int reading = 0;
        for (qint64 now = 0; now <= 90 * kPeriodNs + map + 50 * kMs; now += kMs) {
            if (now % (20 * kMs) == 0) {
                // A reading every 20 ms, alternating 11 ms early and late.
                const qint64 noise = (reading++ % 2 == 0) ? accuracy : -accuracy;
                follower.observe(map + noise, now, accuracy, hold);
            }
            for (auto& item : presenter.takeDue(now, follower.mapNs())) {
                shown.append({std::move(item), now});
            }
        }
        QCOMPARE(shown.size(), 90);
        qint64 shortest = std::numeric_limits<qint64>::max();
        qint64 longest = 0;
        for (int i = 11; i < shown.size(); ++i) {
            const qint64 interval = shown.at(i).atNs - shown.at(i - 1).atNs;
            shortest = std::min(shortest, interval);
            longest = std::max(longest, interval);
        }
        QVERIFY2(shortest >= kPeriodNs - 5 * kMs && longest <= kPeriodNs + 5 * kMs,
                 qPrintable(QStringLiteral("row intervals %1..%2 ms")
                                .arg(double(shortest) / kMs).arg(double(longest) / kMs)));
    }

    // Review Minor 1: a keyframe that comes after its own slot, the slots
    // repeated meanwhile standing in for it and the next rows: the waterfall
    // still gets exactly one row per slot.
    void lateKeyframeKeepsOneRowPerSlot()
    {
        RemoteDisplayPresenter presenter;
        presenter.setRowPeriodNs(kPeriodNs);
        const qint64 map = 100 * kMs;
        QList<Presented> shown;
        for (int i = 0; i < 5; ++i) {
            presenter.push(frameAt(i, i * kPeriodNs, -120.0f), 14.0e6, 24000.0);
        }
        run(presenter, 0, 4 * kPeriodNs + map + kMs, map, shown);
        presenter.noteLoss();
        // Frame 10 is the keyframe; it arrives 300 ms after its slot was due.
        const qint64 keyArrivalNs = 10 * kPeriodNs + map + 300 * kMs;
        run(presenter, 4 * kPeriodNs + map + 2 * kMs, keyArrivalNs, map, shown);
        QVERIFY(presenter.counters().rowsRepeated > 5);
        for (int i = 10; i < 30; ++i) {
            presenter.push(frameAt(i, i * kPeriodNs, -60.0f), 14.0e6, 24000.0);
        }
        run(presenter, keyArrivalNs + kMs, 30 * kPeriodNs + map + 50 * kMs, map, shown);
        int rows = 0;
        for (const Presented& p : shown) {
            if (p.item.frame.waterfallAdvance) {
                ++rows;
            }
        }
        // Frames 0..4 and 10..29 plus the 5 gap slots: 30 slots, 30 rows.
        QCOMPARE(rows, 30);
    }

    // The diagnosis's pure-delay burst: 40 frames at 33 ms, frames 10 to 23
    // held 460 ms and released back to back. With the audio's delay deep
    // enough to ride the stall, all 40 rows play at their timestamps' pace;
    // with a shallower one they still all play, in order, as soon as due.
    void pureDelayBurstPlaysEveryRowAtItsTime()
    {
        const qint64 startNs = 1'000 * kMs;
        const qint64 transitNs = 20 * kMs;
        for (const qint64 map : {500 * kMs, 150 * kMs}) {
            RemoteDisplayPresenter presenter;
            presenter.setRowPeriodNs(kPeriodNs);
            QList<Presented> shown;
            qint64 clock = startNs;
            const qint64 releaseNs = startNs + 10 * kPeriodNs + 460 * kMs;
            for (int i = 0; i < 40; ++i) {
                const qint64 producerNs = startNs + i * kPeriodNs;
                qint64 arrivalNs = producerNs + transitNs;
                if (i >= 10 && i <= 23) {
                    arrivalNs = std::max(arrivalNs, releaseNs);
                }
                run(presenter, clock, arrivalNs, map, shown);
                clock = arrivalNs + kMs;
                presenter.push(frameAt(i, producerNs, float(-100 - i)), 14.0e6, 24000.0);
                run(presenter, arrivalNs, arrivalNs, map, shown);
            }
            run(presenter, clock, clock + 2'000 * kMs, map, shown);
            QCOMPARE(shown.size(), 40);
            QCOMPARE(presenter.counters().keyframeWaits, quint64(0));
            QCOMPARE(presenter.counters().rowsBlended, quint64(0));
            QCOMPARE(presenter.counters().rowsRepeated, quint64(0));
            for (int i = 0; i < 40; ++i) {
                QCOMPARE(shown.at(i).item.kind, RemoteDisplayPresenter::Kind::Frame);
                QCOMPARE(shown.at(i).item.frame.waterfallDbm.first(), float(-100 - i));
                const qint64 dueNs = startNs + i * kPeriodNs + map;
                if (map == 500 * kMs) {
                    // At its timestamp's pace, to the millisecond step.
                    QVERIFY2(shown.at(i).atNs >= dueNs && shown.at(i).atNs - dueNs < kMs,
                             qPrintable(QStringLiteral("row %1 at +%2 ms").arg(i)
                                            .arg(double(shown.at(i).atNs - dueNs) / kMs)));
                } else {
                    QVERIFY(shown.at(i).atNs >= dueNs);
                }
            }
        }
    }

    // One lost delta mid-burst: the decoder refuses the chain until the
    // keyframe it asks for. The rows up to the keyframe blend from the last
    // good row to the keyframe's, each in its own slot; normal after.
    void lostDeltaBlendsUpToTheKeyframe()
    {
        RemoteDisplayPresenter presenter;
        presenter.setRowPeriodNs(kPeriodNs);
        const qint64 startNs = 1'000 * kMs;
        const qint64 map = 300 * kMs; // presentation runs behind arrival
        QList<Presented> shown;
        qint64 clock = startNs;
        // Frame 15 lost; 16..21 refused (need keyframe); 22 is the keyframe.
        for (int i = 0; i < 30; ++i) {
            if (i >= 15 && i <= 21) {
                if (i == 16) {
                    presenter.noteLoss();
                    presenter.noteLoss(); // a second refusal is the same wait
                }
                continue;
            }
            const qint64 producerNs = startNs + i * kPeriodNs;
            const qint64 arrivalNs = producerNs + 20 * kMs;
            run(presenter, clock, arrivalNs, map, shown);
            clock = arrivalNs + kMs;
            const float level = i < 22 ? -120.0f : -60.0f;
            presenter.push(frameAt(i, producerNs, level), 14.0e6, 24000.0);
        }
        run(presenter, clock, clock + 2'000 * kMs, map, shown);
        QCOMPARE(presenter.counters().keyframeWaits, quint64(1));
        QCOMPARE(presenter.counters().rowsBlended, quint64(7));
        QCOMPARE(presenter.counters().rowsRepeated, quint64(0));
        QCOMPARE(shown.size(), 30);
        for (int i = 0; i < 30; ++i) {
            const auto& item = shown.at(i).item;
            const qint64 slotNs = startNs + i * kPeriodNs;
            QVERIFY2(std::llabs(item.producerNs - slotNs) <= 1,
                     qPrintable(QStringLiteral("row %1").arg(i)));
            if (i >= 15 && i <= 21) {
                QCOMPARE(item.kind, RemoteDisplayPresenter::Kind::Blended);
                // -120 to -60 over 8 steps: slot k of 7 is 7.5 dB a step.
                const float expected = -120.0f + 60.0f * float(i - 14) / 8.0f;
                QVERIFY(std::abs(item.frame.waterfallDbm.first() - expected) < 0.01f);
                QVERIFY(item.frame.traceDbm.isEmpty());
            } else {
                QCOMPARE(item.kind, RemoteDisplayPresenter::Kind::Frame);
            }
        }
        QVERIFY(!presenter.waitingForKeyframe());
    }

    // The keyframe is later than the gap's presentation: each slot that
    // passes repeats the last good row, and the rest blend in when the
    // keyframe comes. The time axis stays true: one row per slot.
    void lateKeyframeRepeatsThenBlends()
    {
        RemoteDisplayPresenter presenter;
        presenter.setRowPeriodNs(kPeriodNs);
        const qint64 startNs = 1'000 * kMs;
        const qint64 map = 100 * kMs;
        QList<Presented> shown;
        for (int i = 0; i < 5; ++i) {
            presenter.push(frameAt(i, startNs + i * kPeriodNs, -120.0f), 14.0e6, 24000.0);
        }
        run(presenter, startNs, startNs + 4 * kPeriodNs + map + kMs, map, shown);
        QCOMPARE(shown.size(), 5);
        presenter.noteLoss();
        // The keyframe is frame 20, arriving 20 ms after the Core made it:
        // after most of the gap's slots were due.
        const qint64 keyArrivalNs = startNs + 20 * kPeriodNs + 20 * kMs;
        run(presenter, startNs + 4 * kPeriodNs + map + 2 * kMs, keyArrivalNs, map, shown);
        const int repeated = int(presenter.counters().rowsRepeated);
        QVERIFY2(repeated >= 10 && repeated < 15, qPrintable(QString::number(repeated)));
        presenter.push(frameAt(20, startNs + 20 * kPeriodNs, -60.0f), 14.0e6, 24000.0);
        run(presenter, keyArrivalNs + kMs, keyArrivalNs + 1'000 * kMs, map, shown);
        QCOMPARE(presenter.counters().keyframeWaits, quint64(1));
        // Slots 5..19 are each filled once: repeats first, then blends.
        QCOMPARE(int(presenter.counters().rowsRepeated + presenter.counters().rowsBlended), 15);
        QCOMPARE(shown.size(), 21);
        for (int i = 5; i < 5 + repeated; ++i) {
            QCOMPARE(shown.at(i).item.kind, RemoteDisplayPresenter::Kind::Repeated);
            QCOMPARE(shown.at(i).item.frame.waterfallDbm.first(), -120.0f);
        }
        for (int i = 5 + repeated; i < 20; ++i) {
            QCOMPARE(shown.at(i).item.kind, RemoteDisplayPresenter::Kind::Blended);
            QVERIFY(shown.at(i).item.frame.waterfallDbm.first() > -120.0f);
            QVERIFY(shown.at(i).item.frame.waterfallDbm.first() < -60.0f);
        }
        QCOMPARE(shown.last().item.kind, RemoteDisplayPresenter::Kind::Frame);
    }

    // Rows the Core never sent (a timestamp gap, no loss) blend too; a pause
    // longer than kMaxGapRows, a new chain or a tune is not filled.
    void timestampGapBlendsButPausesAndTunesDoNot()
    {
        RemoteDisplayPresenter presenter;
        presenter.setRowPeriodNs(kPeriodNs);
        presenter.push(frameAt(0, 0, -100.0f), 14.0e6, 24000.0);
        presenter.push(frameAt(1, 3 * kPeriodNs, -100.0f), 14.0e6, 24000.0);
        QCOMPARE(presenter.counters().rowsBlended, quint64(2));
        presenter.push(frameAt(2, 3 * kPeriodNs + 100 * kPeriodNs, -100.0f), 14.0e6, 24000.0);
        QCOMPARE(presenter.counters().rowsBlended, quint64(2));
        presenter.push(frameAt(3, 106 * kPeriodNs, -100.0f), 14.1e6, 24000.0);
        QCOMPARE(presenter.counters().rowsBlended, quint64(2));
        presenter.restartChain();
        presenter.push(frameAt(4, 110 * kPeriodNs, -100.0f), 14.1e6, 24000.0);
        QCOMPARE(presenter.counters().rowsBlended, quint64(2));
        // Normal jitter in the Core's frame times is not a gap.
        presenter.push(frameAt(5, 110 * kPeriodNs + kPeriodNs * 14 / 10, -100.0f), 14.1e6, 24000.0);
        QCOMPARE(presenter.counters().rowsBlended, quint64(2));
        QCOMPARE(presenter.queued(), 8);
    }

    // Without a map (an older Core, or before the first echo) everything
    // is due at once; a map that would hold an item longer than kMaxHoldNs
    // is not trusted.
    void noMapIsInstantAndAnAbsurdMapIsNotTrusted()
    {
        RemoteDisplayPresenter presenter;
        presenter.push(frameAt(0, 5'000 * kMs, -100.0f), 14.0e6, 24000.0);
        QCOMPARE(presenter.nextDueNs(0, std::nullopt).value_or(-1), 0);
        QCOMPARE(int(presenter.takeDue(0, std::nullopt).size()), 1);
        presenter.push(frameAt(1, 5'000 * kMs, -100.0f), 14.0e6, 24000.0);
        QCOMPARE(int(presenter.takeDue(0, 10'000 * kMs).size()), 1);
        presenter.push(frameAt(2, 5'000 * kMs, -100.0f), 14.0e6, 24000.0);
        QCOMPARE(int(presenter.takeDue(0, -4'000 * kMs).size()), 0);
        QCOMPARE(presenter.nextDueNs(0, -4'000 * kMs).value_or(-1), 1'000 * kMs);
        presenter.reset();
        QCOMPARE(presenter.queued(), 0);
    }

    // A pan's widget is handed each waterfall row as the presenter queues
    // it, with its due time, so its row clock can slide the row in by then.
    // NSDC v2 (display codec v2 document, section 11 item 2): v2 frames
    // from encodeDisplayCodecV2, through a DisplayCodecDecoder, into the
    // presenter, under the codec's loss model at a 33 ms row period. Every
    // row the decoder releases is drawn once at its own slot, each lost row
    // is one blended row at its slot (no burst is longer than kMaxGapRows),
    // nothing repeats and nothing waits for a keyframe; a frame whose trace
    // was carried is drawn, as no current measurement.
    void codecV2LossDrawsOneRowPerSlot_data()
    {
        QTest::addColumn<bool>("bursts");
        QTest::addColumn<quint64>("seed");
        QTest::newRow("Gilbert-Elliott") << true << quint64(0x5EED0783ULL);
        QTest::newRow("random") << false << quint64(0x5EED1783ULL);
    }

    void codecV2LossDrawsOneRowPerSlot()
    {
        QFETCH(bool, bursts);
        QFETCH(quint64, seed);
        constexpr qint64 kRowNs = 33 * kMs;
        constexpr int kFrames = 1500;
        Vectors::SceneRun source(Vectors::Scene::PeakBins167);
        const int pixels = source.pixels();
        const std::optional<SpectrumDisplayCost> cost = spectrumDisplayCost(pixels, 30, false);
        QVERIFY(cost);
        std::vector<std::vector<QByteArray>> messages;
        int splitFrames = 0;
        QVector<float> trace;
        QVector<float> waterfall;
        for (int f = 0; f < kFrames; ++f) {
            source.next(trace, waterfall);
            DisplayCodecFrame frame;
            frame.context = {7, 1, Vectors::kMinDbm, Vectors::kMaxDbm, quint16(pixels),
                             quint16(pixels), 0};
            frame.encoderSequence = quint32(f + 1);
            frame.producerTimestamp = quint64(kRowNs * (f + 1));
            frame.waterfallAdvance = true;
            frame.traceDbm = trace;
            frame.waterfallDbm = waterfall;
            DisplayCodecV2Encoding encoding = encodeDisplayCodecV2(frame, cost->maximumFrameBytes);
            QVERIFY(!encoding.messages.empty());
            if (encoding.split()) { ++splitFrames; }
            messages.push_back(std::move(encoding.messages));
        }
        QVERIFY(splitFrames > 0);

        SpectrumEndpointContext endpoint;
        endpoint.codec = {7, 1, Vectors::kMinDbm, Vectors::kMaxDbm, quint16(pixels),
                          quint16(pixels), 0};
        endpoint.exactCentreHz = 14.0e6;
        endpoint.exactSpanHz = 24000.0;
        const RemoteSpectrumCapture capture{endpoint, 0.0, 0.0};
        RemoteDisplayPresenter presenter;
        presenter.setRowPeriodNs(kRowNs);
        DisplayCodecDecoder decoder;
        std::vector<RemoteDisplayPresenter::Item> shown;
        quint32 firstSequence = 0;
        quint32 lastSequence = 0;
        std::optional<quint32> firstRowSequence;
        quint32 lastRowSequence = 0;
        quint64 rowsReleased = 0;
        quint64 traceOnlyFrames = 0;
        quint64 carriedTraces = 0;
        for (const QByteArray& message : deliveredUnderLoss(messages, bursts, seed)) {
            const DisplayCodecDecodeResult decoded = decoder.decode(message);
            if (decoded.disposition != DisplayCodecDisposition::Accepted) {
                // Held parts wait for their partner; a swapped message may
                // come too late. Neither is a keyframe wait in v2.
                QVERIFY(decoded.disposition == DisplayCodecDisposition::Held
                        || decoded.reason == DisplayCodecReason::StaleSequence);
                continue;
            }
            for (int k = 0; k < decoded.frameCount; ++k) {
                const DisplayCodecFrame& frame = k == 0 ? decoded.frame : decoded.secondFrame;
                if (firstSequence == 0) { firstSequence = frame.encoderSequence; }
                lastSequence = frame.encoderSequence;
                if (frame.waterfallAdvance) {
                    if (!firstRowSequence) { firstRowSequence = frame.encoderSequence; }
                    lastRowSequence = frame.encoderSequence;
                    ++rowsReleased;
                } else {
                    ++traceOnlyFrames;
                }
                if (frame.traceCarried) { ++carriedTraces; }
                presenter.push(frame, capture, !frame.traceCarried);
            }
            // Without a map everything queued is due at once.
            for (auto& item : presenter.takeDue(0, std::nullopt)) {
                shown.push_back(std::move(item));
            }
        }
        QVERIFY(firstRowSequence);
        QVERIFY(carriedTraces > 0);
        QVERIFY(traceOnlyFrames > 0);

        // Rows sent over the run of released rows, and those lost: the
        // decoder's lost frames and trace-only partials, less those outside
        // the first and last released row.
        const quint64 rowsSent = lastRowSequence - *firstRowSequence + 1;
        const quint64 rowsLost = rowsSent - rowsReleased;
        QVERIFY(rowsLost > 0);
        QCOMPARE(decoder.counters().framesLost + traceOnlyFrames
                     - (*firstRowSequence - firstSequence) - (lastSequence - lastRowSequence),
                 rowsLost);

        // One row a slot, from the first released row to the last.
        quint64 realRows = 0;
        quint64 blendedRows = 0;
        qint64 slotNs = qint64(*firstRowSequence) * kRowNs;
        qint64 lastRealNs = slotNs;
        int longestGap = 0;
        for (const RemoteDisplayPresenter::Item& item : shown) {
            if (item.kind == RemoteDisplayPresenter::Kind::Frame) {
                QCOMPARE(item.measurementCurrent, !item.frame.traceCarried);
            }
            const bool row = item.kind == RemoteDisplayPresenter::Kind::Blended
                || (item.kind == RemoteDisplayPresenter::Kind::Frame && item.frame.waterfallAdvance);
            if (!row) { continue; }
            QVERIFY2(item.producerNs == slotNs,
                     qPrintable(QStringLiteral("row at %1, slot %2").arg(item.producerNs).arg(slotNs)));
            slotNs += kRowNs;
            if (item.kind == RemoteDisplayPresenter::Kind::Frame) {
                ++realRows;
                longestGap = std::max(longestGap, int((item.producerNs - lastRealNs) / kRowNs) - 1);
                lastRealNs = item.producerNs;
            } else {
                QCOMPARE(item.kind, RemoteDisplayPresenter::Kind::Blended);
                ++blendedRows;
            }
        }
        QVERIFY(longestGap <= RemoteDisplayPresenter::kMaxGapRows);
        QCOMPARE(realRows, rowsReleased);
        QCOMPARE(realRows, rowsSent - rowsLost);
        QCOMPARE(blendedRows, rowsLost);
        QCOMPARE(presenter.counters().rowsBlended, rowsLost);
        QCOMPARE(presenter.counters().rowsRepeated, quint64(0));
        QCOMPARE(presenter.counters().keyframeWaits, quint64(0));
        QCOMPARE(presenter.counters().itemsDropped, quint64(0));
        QVERIFY(!presenter.waitingForKeyframe());
        qInfo().noquote() << QStringLiteral(
            "%1: %2 frames, %3 split, %4 rows sent, %5 lost, %6 carried traces, longest gap %7")
                                 .arg(bursts ? QStringLiteral("Gilbert-Elliott")
                                             : QStringLiteral("random"))
                                 .arg(kFrames).arg(splitFrames).arg(rowsSent).arg(rowsLost)
                                 .arg(carriedTraces).arg(longestGap);
    }

    // NSDC v2: push() keeps the measurementCurrent it is given; the drawing
    // is unchanged.
    void pushKeepsTheFramesMeasurementCurrent()
    {
        RemoteDisplayPresenter presenter;
        presenter.setRowPeriodNs(kPeriodNs);
        SpectrumEndpointContext endpoint;
        endpoint.codec = {1, 1, -180, 0, 8, 8, 0};
        endpoint.exactCentreHz = 14.0e6;
        endpoint.exactSpanHz = 24000.0;
        const RemoteSpectrumCapture capture{endpoint, 0.0, 0.0};
        DisplayCodecFrame carried = frameAt(0, kPeriodNs, -100.0f);
        carried.traceCarried = true;
        presenter.push(carried, capture, false);
        presenter.push(frameAt(1, 2 * kPeriodNs, -90.0f), capture, true);
        const std::vector<RemoteDisplayPresenter::Item> due = presenter.takeDue(0, std::nullopt);
        QCOMPARE(due.size(), size_t(2));
        QCOMPARE(due.at(0).kind, RemoteDisplayPresenter::Kind::Frame);
        QVERIFY(!due.at(0).measurementCurrent);
        QCOMPARE(due.at(0).frame.waterfallDbm.first(), -100.0f);
        QVERIFY(due.at(0).frame.waterfallAdvance);
        QCOMPARE(due.at(1).kind, RemoteDisplayPresenter::Kind::Frame);
        QVERIFY(due.at(1).measurementCurrent);
        QCOMPARE(presenter.counters().keyframeWaits, quint64(0));
        // The default is current, as before.
        presenter.push(frameAt(2, 3 * kPeriodNs, -80.0f), capture);
        QVERIFY(presenter.takeDue(0, std::nullopt).front().measurementCurrent);
    }

    void pushRecordsTheRowsItQueues()
    {
        RemoteDisplayPresenter presenter;
        presenter.setRowPeriodNs(kPeriodNs);
        const qint64 map = 100 * kMs;
        presenter.push(frameAt(0, 0, -100.0f), 14.0e6, 24000.0);
        std::vector<RemoteDisplayPresenter::Item> rows = presenter.takeQueuedRows();
        QCOMPARE(int(rows.size()), 1);
        QCOMPARE(rows[0].kind, RemoteDisplayPresenter::Kind::Frame);
        QCOMPARE(rows[0].producerNs, qint64(0));
        QCOMPARE(presenter.dueNs(rows[0], 0, map), map);
        // Taken once; the items stay queued for takeDue().
        QVERIFY(presenter.takeQueuedRows().empty());
        QCOMPARE(presenter.queued(), 1);

        // After a gap of two rows: the two blended rows, then the frame's
        // own, in order, with their producer times.
        presenter.push(frameAt(1, 3 * kPeriodNs, -40.0f), 14.0e6, 24000.0);
        rows = presenter.takeQueuedRows();
        QCOMPARE(int(rows.size()), 3);
        QCOMPARE(rows[0].kind, RemoteDisplayPresenter::Kind::Blended);
        QCOMPARE(rows[0].producerNs, kPeriodNs);
        QCOMPARE(rows[1].kind, RemoteDisplayPresenter::Kind::Blended);
        QCOMPARE(rows[1].producerNs, 2 * kPeriodNs);
        QCOMPARE(rows[2].kind, RemoteDisplayPresenter::Kind::Frame);
        QCOMPARE(rows[2].producerNs, 3 * kPeriodNs);
        QCOMPARE(rows[2].frame.encoderSequence, quint32(2));
        QVERIFY(std::abs(rows[0].frame.waterfallDbm.at(0) + 80.0f) < 0.01f);
        QCOMPARE(presenter.dueNs(rows[1], 0, map), 2 * kPeriodNs + map);
        // A frame with no row (its trace only) queues none.
        DisplayCodecFrame traceOnly = frameAt(2, 4 * kPeriodNs, -100.0f);
        traceOnly.waterfallAdvance = false;
        presenter.push(traceOnly, 14.0e6, 24000.0);
        QVERIFY(presenter.takeQueuedRows().empty());

        // Repeats stand in for rows while a keyframe is awaited; a keyframe
        // later than them owes its row back: that row is not handed over.
        RemoteDisplayPresenter owed;
        owed.setRowPeriodNs(kPeriodNs);
        owed.push(frameAt(0, 0, -100.0f), 14.0e6, 24000.0);
        owed.takeQueuedRows();
        QCOMPARE(int(owed.takeDue(map, map).size()), 1);
        owed.noteLoss();
        int repeats = 0;
        for (int slot = 1; slot <= 3; ++slot) {
            repeats += int(owed.takeDue(map + slot * kPeriodNs, map).size());
        }
        QCOMPARE(repeats, 3);
        owed.push(frameAt(1, kPeriodNs, -100.0f), 14.0e6, 24000.0);
        QVERIFY(owed.takeQueuedRows().empty());
        owed.push(frameAt(2, 2 * kPeriodNs, -100.0f), 14.0e6, 24000.0);
        QVERIFY(owed.takeQueuedRows().empty());
        owed.push(frameAt(3, 3 * kPeriodNs, -100.0f), 14.0e6, 24000.0);
        QVERIFY(owed.takeQueuedRows().empty());
        owed.push(frameAt(4, 4 * kPeriodNs, -100.0f), 14.0e6, 24000.0);
        QCOMPARE(int(owed.takeQueuedRows().size()), 1);

        // A reset forgets them with the queue.
        presenter.push(frameAt(3, 5 * kPeriodNs, -100.0f), 14.0e6, 24000.0);
        presenter.reset();
        QVERIFY(presenter.takeQueuedRows().empty());
    }
};

QTEST_MAIN(TstRemoteDisplayPresenter)
#include "tst_remote_display_presenter.moc"
