// no-port-check: NereusSDR test of the port of Thetis's grid noise-floor
// tracking. Thetis moves the grid minimum to the display's own noise floor
// every 500 ms on tmrAutoAGC, not while transmitting, and only when the
// floor is "good" (not in fast attack):
//   console.cs:46136-46167 [v2.10.3.15] tmrAutoAGC_Tick
//   display.cs:5398-5404, 4670-4677, 925-934 [v2.10.3.15]
// The pan's own noise floor (NoiseFloorFollower) drives it, so it works with
// Clarity off and in a remote window; while Clarity feeds a pan its
// estimate, Clarity keeps driving that pan's grid (the add-on is kept).
//
// Modification history (NereusSDR):
//   2026-09-28  J.J. Boyd / KG4VCF  Created. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-10-23  J.J. Boyd / KG4VCF  A pan reads the band from its first
//                                    frame with data, also at Log
//                                    Recursive. AI-assisted via Anthropic
//                                    Claude Code.
//   2026-10-23  J.J. Boyd / KG4VCF  A remote pan at Log Recursive reads
//                                    the band from its first floor, new
//                                    and across a Core restart.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-23  J.J. Boyd / KG4VCF  A same context inside a new pan's
//                                    hold, a grid tick inside it, and a
//                                    band change whose hold outlasts its
//                                    fast attack; the remote pans run on
//                                    a display clock of the Core's frames.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-23  J.J. Boyd / KG4VCF  NF-AGC's low on an older Core's new
//                                    pan, and a MOX un-key's fast attack
//                                    across a hold. AI-assisted via
//                                    Anthropic Claude Code.
//   2026-10-23  J.J. Boyd / KG4VCF  A carried NSDC v2 trace inside a
//                                    remote pan's warm-up leaves the floor
//                                    and the start's share alone.
//                                    AI-assisted via Anthropic Claude Code.
//   2026-10-23  J.J. Boyd / KG4VCF  A carried first trace does not seed a
//                                    new pan with no averaging.
//                                    AI-assisted via Anthropic Claude Code.

#include <QtTest/QtTest>

#include "core/session/media/DisplayCodec.h"
#include "core/session/media/SpectrumEndpoint.h"
#include "core/spectrum/DisplayFollowers.h"
#include "gui/SpectrumWidget.h"

#include <cmath>
#include <optional>
#include <random>

using namespace NereusSDR;

class TestGridFollowsDisplayNoiseFloor : public QObject
{
    Q_OBJECT

private:
    static constexpr double kPanCentreHz = 14200000.0;
    static constexpr double kPanSpanHz   = 800.0;

    static void configure(SpectrumWidget& w)
    {
        w.setFrequencyRange(kPanCentreHz, kPanSpanHz);
        w.setDdcCenterFrequency(kPanCentreHz);
        w.setSampleRate(kPanSpanHz);
        w.setSpectrumDetector(SpectrumDetector::Peak);
        w.setSpectrumAveraging(SpectrumAveraging::None);
        w.setDbmRange(-150.0f, -40.0f);
        w.setNFOffsetGridFollow(-5);
        w.setMaintainNFAdjustDelta(false);
        w.setAdjustGridMinToNoiseFloor(true);
        // Out of fast attack, at a known floor.
        w.setNoiseFloorFastAttack(false);
        w.setMeasuredNoiseFloorForTest(-120.0f);
    }

    static void feed(SpectrumWidget& w)
    {
        const QVector<float> bins(4096, 1e-12f);   // a flat floor
        w.updateSpectrumLinear(0, bins, 2.0, -10.0);
    }

    static constexpr int kFps = 30;

    // A Core feeding a remote pan: a real endpoint at Log Recursive and its
    // encoder (both started again by start()), the pan's decoder, the
    // FFT's exponentially distributed noise, and the pan's display clock,
    // which moves on 34 ms a frame as the Core's frames do.
    struct RemoteCore {
        SpectrumEndpointSourceContext source;
        SpectrumEndpointRequest request;
        std::optional<SpectrumEndpoint> endpoint;
        std::optional<DisplayCodecEncoder> encoder;
        DisplayCodecDecoder decoder;
        std::mt19937 rng {628};
        std::exponential_distribution<float> noise {1.0f};
        qint64 producedAtNs {0};
        qint64 clockMs {0};
        float bandLinear {1.0e-12f};

        explicit RemoteCore(int averageTimeMs)
        {
            source.source = {2, FftTier::Wide};
            source.sourceGeneration = 11;
            source.fftBins = 1024;
            source.centreHz = kPanCentreHz;
            source.sampleRateHz = 192'000.0;
            source.contextGeneration = 4;
            request.endpointId = 91;
            request.source = source.source;
            request.centreHz = source.centreHz;
            request.spanHz = 96'000.0;
            request.pixels = 128;
            request.targetFps = kFps;
            request.framesPerLine = 1;
            // The window a pan with a -150 to -50 dBm scale asks for.
            request.minDbm = -150.0f;
            request.maxDbm = -50.0f;
            request.trace.detector = SpectrumDetectorMode::Peak;
            request.trace.averageMode = 3;
            request.trace.averageAlpha = averageAlphaForTimeMs(averageTimeMs, kFps);
            request.waterfall.detector = SpectrumDetectorMode::Peak;
            request.waterfall.averageMode = 0;
        }

        void start()
        {
            endpoint.emplace();
            encoder.emplace();
            QVERIFY(endpoint->configure(request, source));
        }

        // The pan takes the endpoint's context, with the Core's picture
        // word if it sent one.
        void attach(SpectrumWidget& w, std::optional<bool> coreCarriedOn = std::nullopt)
        {
            w.setDisplayClockForTest([this] { return clockMs; });
            w.setRemoteSpectrumContext(endpoint->context(), source.centreHz,
                                       source.sampleRateHz, 0, coreCarriedOn);
        }

        // The Core's next frame, averaged and reduced.
        std::optional<DisplayCodecFrame> next()
        {
            DaemonSpectrumFrame input;
            input.source = source.source;
            input.generation = source.sourceGeneration;
            input.centreHz = source.centreHz;
            input.sampleRateHz = source.sampleRateHz;
            producedAtNs += 34'000'000;
            input.producedAtNs = producedAtNs;
            input.windowEnb = 1.0;
            input.dbmOffset = 0.0;
            input.binsLinear.resize(source.fftBins);
            for (float& bin : input.binsLinear) { bin = bandLinear * noise(rng); }
            input.binsLinear[300] = 1.0e-5f;
            clockMs += 34;
            return endpoint->consume(input, 0.0);
        }

        // One frame from the Core into the window, through the codec.
        void deliver(SpectrumWidget& w)
        {
            const std::optional<DisplayCodecFrame> reduced = next();
            QVERIFY(reduced.has_value());
            const QByteArray packet = encoder->encode(*reduced);
            QVERIFY(!packet.isEmpty());
            const DisplayCodecDecodeResult decoded = decoder.decode(packet);
            QVERIFY(decoded.disposition == DisplayCodecDisposition::Accepted);
            QVERIFY(w.updateRemoteSpectrum(decoded.frame));
        }

        // A frame the Core averaged and the pan never drew.
        void skip()
        {
            QVERIFY(next().has_value());
        }
    };

    static void configureRemote(SpectrumWidget& w, int averageTimeMs)
    {
        w.setSpectrumDetector(SpectrumDetector::Peak);
        w.setSpectrumAveraging(SpectrumAveraging::LogRecursive);
        w.setSpectrumAverageTimeMs(averageTimeMs);
    }

    // The mean of the floors over the last 3 s of a 10 s run.
    static double settledOf(const QVector<float>& floors)
    {
        double settled = 0.0;
        for (int frame = 7 * kFps; frame < 10 * kFps; ++frame) { settled += floors.at(frame); }
        return settled / (3 * kFps);
    }

    // Where a new remote pan's floor settles on the RemoteCore's band.
    static double settledRemoteFloor(int averageTimeMs)
    {
        RemoteCore core(averageTimeMs);
        SpectrumWidget w;
        configureRemote(w, averageTimeMs);
        core.start();
        core.attach(w);
        QVector<float> floors;
        for (int frame = 0; frame < 10 * kFps; ++frame) {
            core.deliver(w);
            floors.append(w.nfLerpAverageForTest());
        }
        return settledOf(floors);
    }

private slots:
    void gridMinFollowsTheDisplayNoiseFloorWithoutClarity()
    {
        SpectrumWidget w;
        configure(w);
        feed(w);
        const int expected = qRound(w.nfLerpAverageForTest() + w.nfShiftDbm() - 5.0f);
        QVERIFY(std::abs(expected - w.gridMin()) >= 2);
        QTRY_COMPARE_WITH_TIMEOUT(w.gridMin(), expected, 2000);
        QCOMPARE(w.gridMax(), -40);   // Maintain grid range is off
    }

    void maintainGridRangeMovesTheMaxToo()
    {
        SpectrumWidget w;
        configure(w);
        w.setMaintainNFAdjustDelta(true);
        feed(w);
        const int expected = qRound(w.nfLerpAverageForTest() + w.nfShiftDbm() - 5.0f);
        QTRY_COMPARE_WITH_TIMEOUT(w.gridMin(), expected, 2000);
        QCOMPARE(w.gridMax() - w.gridMin(), 110);
    }

    void holdsWhileTransmittingAndInFastAttack()
    {
        SpectrumWidget keyed;
        configure(keyed);
        keyed.setMoxOverlay(true);
        feed(keyed);
        SpectrumWidget attacking;
        configure(attacking);
        attacking.setNoiseFloorFastAttack(true);
        feed(attacking);
        QTest::qWait(1200);
        QCOMPARE(attacking.gridMin(), -150);
        keyed.setMoxOverlay(false);
        // The receive grid the pan keeps while keyed.
        QCOMPARE(keyed.gridMin(), -150);
    }

    void aPanClarityFeedsKeepsClaritysGrid()
    {
        SpectrumWidget w;
        configure(w);
        w.testApplyNoiseFloor(-100.0f);   // Clarity's estimate
        QCOMPARE(w.gridMin(), -105);
        feed(w);
        QTest::qWait(700);
        QCOMPARE(w.gridMin(), -105);
    }

    // JJ 2026-10-23: after a reconnect the window drew the waterfall bright
    // for seconds while its floor climbed from -200 dBm. A pan that starts,
    // or that drew frames with nothing in them while its stream was down,
    // reads the band from its first frame with data.
    void aPanReadsTheBandFromItsFirstFrameWithData()
    {
        SpectrumWidget w;
        w.setFrequencyRange(kPanCentreHz, kPanSpanHz);
        w.setDdcCenterFrequency(kPanCentreHz);
        w.setSampleRate(kPanSpanHz);
        w.setSpectrumDetector(SpectrumDetector::Peak);
        w.setSpectrumAveraging(SpectrumAveraging::None);
        const QVector<float> nothing(4096, 0.0f);
        for (int frame = 0; frame < 5; ++frame) {
            w.updateSpectrumLinear(0, nothing, 2.0, -10.0);
        }
        QCOMPARE(w.nfLerpAverageForTest(), -200.0f);
        feed(w);
        const float first = w.nfLerpAverageForTest();
        for (int frame = 0; frame < 300; ++frame) {
            feed(w);
        }
        const float settled = w.nfLerpAverageForTest();
        QVERIFY2(settled > -150.0f, qPrintable(QString::number(settled)));
        QVERIFY2(std::abs(first - settled) < 1.0f,
                 qPrintable(QStringLiteral("first %1, settled %2").arg(first).arg(settled)));
        // The stream drops: frames with nothing leave the floor alone.
        for (int frame = 0; frame < 100; ++frame) {
            w.updateSpectrumLinear(0, nothing, 2.0, -10.0);
        }
        QCOMPARE(w.nfLerpAverageForTest(), settled);
    }

    // PR 628 review: at Log Recursive (the default) the average starts at
    // -160 dBm, so a fresh pan's first rows read below the band. The floor
    // is within 3 dB of where it settles from the first frame, at 30 ms and
    // at 650 ms.
    void aPanAtLogRecursiveReadsTheBandFromItsFirstFrame_data()
    {
        QTest::addColumn<int>("averageTimeMs");
        QTest::newRow("30 ms") << 30;
        QTest::newRow("650 ms") << 650;
    }

    void aPanAtLogRecursiveReadsTheBandFromItsFirstFrame()
    {
        QFETCH(int, averageTimeMs);
        const auto fresh = [averageTimeMs](SpectrumWidget& w) {
            w.setFrequencyRange(kPanCentreHz, kPanSpanHz);
            w.setDdcCenterFrequency(kPanCentreHz);
            w.setSampleRate(kPanSpanHz);
            w.setSpectrumDetector(SpectrumDetector::Peak);
            w.setSpectrumAveraging(SpectrumAveraging::LogRecursive);
            w.setSpectrumAverageTimeMs(averageTimeMs);
        };
        SpectrumWidget settledPan;
        fresh(settledPan);
        for (int frame = 0; frame < 600; ++frame) {
            feed(settledPan);
        }
        const float settled = settledPan.nfLerpAverageForTest();
        QVERIFY2(settled > -150.0f, qPrintable(QString::number(settled)));

        SpectrumWidget w;
        fresh(w);
        for (int frame = 0; frame < 120; ++frame) {
            feed(w);
            QVERIFY2(std::abs(w.nfLerpAverageForTest() - settled) < 3.0f,
                     qPrintable(QStringLiteral("frame %1: %2, settled %3")
                                    .arg(frame).arg(w.nfLerpAverageForTest()).arg(settled)));
        }
    }

    // PR 628 re-review: a remote window's own floor line reads the Core's
    // averaged trace, which at Log Recursive starts again from -160 dBm on
    // a new picture. A real endpoint at Log Recursive, through the codec,
    // into a window: a brand-new pan, and a running window across a Core
    // restart, read within 3 dB of the settled floor on every frame they
    // have one, at 30 ms and 650 ms.
    void aRemotePanAtLogRecursiveReadsTheBandFromTheStart_data()
    {
        QTest::addColumn<int>("averageTimeMs");
        QTest::addColumn<bool>("coreRestarts");
        QTest::newRow("new pan, 30 ms") << 30 << false;
        QTest::newRow("new pan, 650 ms") << 650 << false;
        QTest::newRow("Core restart, 30 ms") << 30 << true;
        QTest::newRow("Core restart, 650 ms") << 650 << true;
    }

    void aRemotePanAtLogRecursiveReadsTheBandFromTheStart()
    {
        QFETCH(int, averageTimeMs);
        QFETCH(bool, coreRestarts);
        RemoteCore core(averageTimeMs);
        SpectrumWidget w;
        configureRemote(w, averageTimeMs);
        core.start();
        QVERIFY(!QTest::currentTestFailed());
        core.attach(w);

        // Before a restart, or the reference for a new pan: 10 s, the last
        // 3 s of which is the settled floor.
        QVector<float> floors;
        for (int frame = 0; frame < 10 * kFps; ++frame) {
            core.deliver(w);
            QVERIFY(!QTest::currentTestFailed());
            floors.append(w.nfLerpAverageForTest());
        }
        const double settled = settledOf(floors);
        QVERIFY2(settled > -140.0 && settled < -110.0, qPrintable(QString::number(settled)));

        if (coreRestarts) {
            // The Core restarts: a new endpoint and encoder for the same
            // pan, a new picture, and frames from sequence 0 again.
            core.source.sourceGeneration = 1;
            core.source.contextGeneration = 1;
            core.start();
            core.decoder.reset();
            QVERIFY(!QTest::currentTestFailed());
            core.attach(w, false);
            floors.clear();
            for (int frame = 0; frame < 6 * kFps; ++frame) {
                core.deliver(w);
                QVERIFY(!QTest::currentTestFailed());
                floors.append(w.nfLerpAverageForTest());
            }
        }
        // A new pan has no floor until it has a frame it can read; it has
        // one within half a second.
        int first = 0;
        while (first < floors.size() && floors.at(first) <= -199.0f) { ++first; }
        double worst = 0.0;
        for (int frame = first; frame < floors.size(); ++frame) {
            worst = std::max(worst, std::abs(floors.at(frame) - settled));
        }
        qInfo("%s: settled %.2f dBm, first floor on frame %d, worst %.2f dB off",
              QTest::currentDataTag(), settled, first, worst);
        QVERIFY2(first < kFps / 2, qPrintable(QString::number(first)));
        for (int frame = first; frame < floors.size(); ++frame) {
            QVERIFY2(std::abs(floors.at(frame) - settled) < 3.0,
                     qPrintable(QStringLiteral("frame %1: %2, settled %3")
                                    .arg(frame).arg(floors.at(frame)).arg(settled)));
        }
    }

    // PR 628 round two: a context for the same picture while a new pan
    // waits for the Core's average to warm. The Core averaged frames the
    // pan never drew, so the start's share the pan carried is too large and
    // taking it out would put the floor high; the pan holds instead. At
    // 650 ms with 1 to 3 Core frames never drawn, the floor is never more
    // than 3 dB above where it settles, and within 3 dB once it has one.
    void aSameContextInTheHoldKeepsTheFloorDown_data()
    {
        QTest::addColumn<int>("framesNeverDrawn");
        QTest::newRow("1 frame never drawn") << 1;
        QTest::newRow("2 frames never drawn") << 2;
        QTest::newRow("3 frames never drawn") << 3;
    }

    void aSameContextInTheHoldKeepsTheFloorDown()
    {
        QFETCH(int, framesNeverDrawn);
        constexpr int kAverageTimeMs = 650;
        const double settled = settledRemoteFloor(kAverageTimeMs);
        QVERIFY(!QTest::currentTestFailed());

        RemoteCore core(kAverageTimeMs);
        SpectrumWidget w;
        configureRemote(w, kAverageTimeMs);
        core.start();
        QVERIFY(!QTest::currentTestFailed());
        core.attach(w);
        // Two frames drawn, inside the hold.
        core.deliver(w);
        core.deliver(w);
        QVERIFY(!QTest::currentTestFailed());
        QCOMPARE(w.nfLerpAverageForTest(), -200.0f);
        // Frames the Core averaged and the pan never drew, then a context
        // for the same picture, whose encoder sequence starts again.
        for (int frame = 0; frame < framesNeverDrawn; ++frame) {
            core.skip();
            QVERIFY(!QTest::currentTestFailed());
        }
        core.encoder.emplace();
        core.decoder.reset();
        core.attach(w, true);
        QVERIFY(w.remoteContextKeptPicture());

        QVector<float> floors;
        for (int frame = 0; frame < 6 * kFps; ++frame) {
            core.deliver(w);
            QVERIFY(!QTest::currentTestFailed());
            floors.append(w.nfLerpAverageForTest());
        }
        int first = 0;
        while (first < floors.size() && floors.at(first) <= -199.0f) { ++first; }
        double highest = -1.0e9;
        for (int frame = first; frame < floors.size(); ++frame) {
            highest = std::max(highest, floors.at(frame) - settled);
        }
        qInfo("%s: settled %.2f dBm, first floor on frame %d, at most %.2f dB above",
              QTest::currentDataTag(), settled, first, highest);
        QVERIFY2(first < 4 * kFps, qPrintable(QString::number(first)));
        for (int frame = first; frame < floors.size(); ++frame) {
            QVERIFY2(std::abs(floors.at(frame) - settled) < 3.0,
                     qPrintable(QStringLiteral("frame %1: %2, settled %3")
                                    .arg(frame).arg(floors.at(frame)).arg(settled)));
        }
    }

    // PR 628 with NSDC v2 (PR 631): a v2 frame whose trace part was lost
    // carries the last decoded trace (traceCarried). Inside a new pan's
    // warm-up that trace is not the Core's average at this frame's
    // sequence, so taking the start's share out of it measures nothing
    // real. A carried, stale, hot trace leaves the floor and the start's
    // share where the fresh frame before it put them.
    void aCarriedTraceInTheWarmUpLeavesTheFloorAlone()
    {
        constexpr int kAverageTimeMs = 650;
        RemoteCore core(kAverageTimeMs);
        SpectrumWidget w;
        configureRemote(w, kAverageTimeMs);
        core.start();
        QVERIFY(!QTest::currentTestFailed());
        core.attach(w);
        // Fresh frames until the pan reads a floor, still inside the warm-up.
        int fresh = 0;
        while (w.nfLerpAverageForTest() <= -199.0f && fresh < kFps) {
            core.deliver(w);
            QVERIFY(!QTest::currentTestFailed());
            ++fresh;
        }
        core.deliver(w);
        QVERIFY(!QTest::currentTestFailed());
        const float floor = w.nfLerpAverageForTest();
        const double share = w.remoteTraceStartShareForTest();
        QVERIFY2(floor > -199.0f, qPrintable(QString::number(floor)));
        QVERIFY2(share > 0.0, qPrintable(QString::number(share)));

        // The Core's next frame reaches the pan with its trace part lost:
        // the decoder carries an older trace, here a hot one.
        std::optional<DisplayCodecFrame> carried = core.next();
        QVERIFY(carried.has_value());
        carried->traceDbm.fill(-130.0f);
        carried->traceCarried = true;
        QVERIFY(w.updateRemoteSpectrum(*carried));
        QCOMPARE(w.nfLerpAverageForTest(), floor);
        QCOMPARE(w.remoteTraceStartShareForTest(), share);
    }

    // PR 628 re-review round five: a v2 context clears the decoder's fill,
    // so a first frame whose trace part was lost carries the bottom of the
    // codec range across the width. A new pan with no averaging must not
    // seed its floor there; it seeds from its first fresh frame, as a pan
    // that never saw the carried frame does.
    void aCarriedFirstTraceDoesNotSeedANewPan()
    {
        RemoteCore core(30);
        core.request.trace.averageMode = 0;
        SpectrumWidget w;
        SpectrumWidget twin;
        for (SpectrumWidget* pan : {&w, &twin}) {
            pan->setSpectrumDetector(SpectrumDetector::Peak);
            pan->setSpectrumAveraging(SpectrumAveraging::None);
        }
        core.start();
        QVERIFY(!QTest::currentTestFailed());
        core.attach(w);
        core.attach(twin);

        std::optional<DisplayCodecFrame> carried = core.next();
        QVERIFY(carried.has_value());
        carried->traceDbm.fill(core.request.minDbm);
        carried->traceCarried = true;
        QVERIFY(w.updateRemoteSpectrum(*carried));
        QCOMPARE(w.nfLerpAverageForTest(), -200.0f);

        std::optional<DisplayCodecFrame> fresh = core.next();
        QVERIFY(fresh.has_value());
        QVERIFY(w.updateRemoteSpectrum(*fresh));
        QVERIFY(twin.updateRemoteSpectrum(*fresh));
        QVERIFY2(w.nfLerpAverageForTest() > -140.0f,
                 qPrintable(QString::number(w.nfLerpAverageForTest())));
        QCOMPARE(w.nfLerpAverageForTest(), twin.nfLerpAverageForTest());
    }

    // PR 628 round two: while a new remote pan holds for the Core's
    // average, its follower has no estimate (it reads -200 dBm). The grid
    // does not follow it there, and the 3D floor stands at the bottom of
    // the dBm range instead.
    void aGridTickInsideTheHoldLeavesTheGridAlone()
    {
        constexpr int kAverageTimeMs = 650;
        RemoteCore core(kAverageTimeMs);
        SpectrumWidget w;
        configureRemote(w, kAverageTimeMs);
        w.setDbmRange(-150.0f, -40.0f);
        w.setNFOffsetGridFollow(-5);
        w.setMaintainNFAdjustDelta(false);
        w.setAdjustGridMinToNoiseFloor(true);
        core.start();
        QVERIFY(!QTest::currentTestFailed());
        core.attach(w);
        core.deliver(w);
        QVERIFY(!QTest::currentTestFailed());
        QCOMPARE(w.nfLerpAverageForTest(), -200.0f);
        w.gridFollowTickForTest();
        QCOMPARE(w.gridMin(), -150);
        QCOMPARE(w.gridMax(), -40);
        QCOMPARE(w.dssFloorDbm(), -150.0f - static_cast<float>(w.dssFloorDepth()));
    }

    // PR 628 round two: a band change on a Core that sends no picture word
    // is a new picture, and the pan holds (its follower has an estimate)
    // until the Core's average is warm, longer than the band change's
    // fast attack. The fast attack runs its time again from the end of
    // the hold, so the floor reaches the new band within ten frames of it.
    void aBandChangeHoldDoesNotUseUpTheFastAttack()
    {
        constexpr int kAverageTimeMs = 650;
        RemoteCore core(kAverageTimeMs);
        SpectrumWidget w;
        configureRemote(w, kAverageTimeMs);
        core.start();
        QVERIFY(!QTest::currentTestFailed());
        core.attach(w);
        QVector<float> floors;
        for (int frame = 0; frame < 10 * kFps; ++frame) {
            core.deliver(w);
            QVERIFY(!QTest::currentTestFailed());
            floors.append(w.nfLerpAverageForTest());
        }
        // The new band is 20 dB hotter, so it settles 20 dB higher.
        const double settled = settledOf(floors) + 20.0;

        // The band change: a new centre with no picture word, and the fast
        // attack the desktop's trigger turns on for it.
        core.source.centreHz += 1'000'000.0;
        core.source.sourceGeneration = 12;
        core.source.contextGeneration = 5;
        core.request.centreHz = core.source.centreHz;
        core.bandLinear = 1.0e-10f;
        core.start();
        core.decoder.reset();
        QVERIFY(!QTest::currentTestFailed());
        core.attach(w);
        QVERIFY(!w.remoteContextKeptPicture());
        w.setNoiseFloorFastAttack(true);

        const float held = w.nfLerpAverageForTest();
        floors.clear();
        for (int frame = 0; frame < 8 * kFps; ++frame) {
            core.deliver(w);
            QVERIFY(!QTest::currentTestFailed());
            floors.append(w.nfLerpAverageForTest());
        }
        int holdEnd = 0;
        while (holdEnd < floors.size() && floors.at(holdEnd) == held) { ++holdEnd; }
        qInfo("hold of %d frames; ten frames after it %.2f dB off the new band",
              holdEnd, double(floors.value(holdEnd + 10)) - settled);
        // The hold outlasts the band change's fast attack.
        QVERIFY2(holdEnd * 34 > 1000 && holdEnd < 4 * kFps, qPrintable(QString::number(holdEnd)));
        for (int frame = holdEnd + 10; frame < floors.size(); ++frame) {
            QVERIFY2(std::abs(floors.at(frame) - settled) < 3.0,
                     qPrintable(QStringLiteral("frame %1: %2, new band %3")
                                    .arg(frame).arg(floors.at(frame)).arg(settled)));
        }
    }

    // PR 628 round three: a new pan on an older Core (no picture word) at
    // 2000 ms holds while the Core's average warms, and its floor reads
    // -200 dBm until then. NF-AGC's run level takes nothing from those rows
    // and starts at the seed's target, so the waterfall's low level is
    // within 3 dB of where it settles from the pan's first floor (it was
    // 44 to 48 dB under, dragged toward -205 by the held rows).
    void nfAgcOnAnOlderCoresNewPanStartsAtTheBand()
    {
        constexpr int kAverageTimeMs = 2000;
        RemoteCore core(kAverageTimeMs);
        SpectrumWidget w;
        configureRemote(w, kAverageTimeMs);
        w.setWfAgcEnabled(true);
        w.setWaterfallNFAGCEnabled(true);
        core.start();
        QVERIFY(!QTest::currentTestFailed());
        core.attach(w);
        // NF-AGC's low level does not read the row.
        const QVector<float> row(128, -120.0f);
        QVector<float> floors;
        QVector<float> lows;
        for (int frame = 0; frame < 10 * kFps; ++frame) {
            core.deliver(w);
            QVERIFY(!QTest::currentTestFailed());
            w.composeWaterfallActiveThresholds(row);
            floors.append(w.nfLerpAverageForTest());
            lows.append(w.wfActiveLowThreshold());
        }
        const double settledLow = settledOf(lows);
        int first = 0;
        while (first < floors.size() && floors.at(first) <= -199.0f) { ++first; }
        double worst = 0.0;
        int over = 0;
        for (int frame = first; frame < lows.size(); ++frame) {
            const double off = std::abs(lows.at(frame) - settledLow);
            worst = std::max(worst, off);
            if (off >= 3.0) { ++over; }
        }
        qInfo("settled low %.2f dBm; held %d frames; first low %+.2f dB; worst %.2f dB;"
              " %d frames 3 dB or more off", settledLow, first,
              double(lows.value(first)) - settledLow, worst, over);
        // The held rows this is about.
        QVERIFY2(first > 5 && first < kFps, qPrintable(QString::number(first)));
        for (int frame = first; frame < lows.size(); ++frame) {
            QVERIFY2(std::abs(lows.at(frame) - settledLow) < 3.0,
                     qPrintable(QStringLiteral("frame %1: low %2, settled %3")
                                    .arg(frame).arg(lows.at(frame)).arg(settledLow)));
        }
    }

    // PR 628 round three: a MOX edge's fast attack runs for the pan's hold
    // length (3 s here). A new picture straight after it holds the pan while
    // the Core's average warms; from the end of that hold the fast attack
    // runs for what it had left, not a flat second.
    void aMoxEdgesFastAttackOutlastsAHoldAfterIt()
    {
        constexpr int kAverageTimeMs = 650;
        constexpr qint64 kHoldLengthMs = 3000;
        RemoteCore core(kAverageTimeMs);
        SpectrumWidget w;
        configureRemote(w, kAverageTimeMs);
        core.start();
        QVERIFY(!QTest::currentTestFailed());
        core.attach(w);
        for (int frame = 0; frame < 10 * kFps; ++frame) {
            core.deliver(w);
            QVERIFY(!QTest::currentTestFailed());
        }
        QVERIFY(!w.noiseFloorFastAttack());

        w.setDisplayKeyed(true, kHoldLengthMs, 0);
        w.setDisplayKeyed(false, kHoldLengthMs, 0);
        QVERIFY(w.noiseFloorFastAttack());
        // A new picture: a new endpoint on another centre, with no picture
        // word (the band's noise is the same).
        core.source.centreHz += 1'000'000.0;
        core.request.centreHz = core.source.centreHz;
        core.source.sourceGeneration = 12;
        core.source.contextGeneration = 5;
        core.start();
        core.decoder.reset();
        QVERIFY(!QTest::currentTestFailed());
        core.attach(w);
        QVERIFY(!w.remoteContextKeptPicture());

        const float held = w.nfLerpAverageForTest();
        QVector<float> floors;
        QVector<bool> fastAttacks;
        for (int frame = 0; frame < 8 * kFps; ++frame) {
            core.deliver(w);
            QVERIFY(!QTest::currentTestFailed());
            floors.append(w.nfLerpAverageForTest());
            fastAttacks.append(w.noiseFloorFastAttack());
        }
        int holdEnd = 0;
        while (holdEnd < floors.size() && floors.at(holdEnd) == held) { ++holdEnd; }
        int fastAttackEnd = holdEnd;
        while (fastAttackEnd < fastAttacks.size() && fastAttacks.at(fastAttackEnd)) {
            ++fastAttackEnd;
        }
        qInfo("hold of %d frames; fast attack for %d frames (%lld ms) after it",
              holdEnd, fastAttackEnd - holdEnd,
              static_cast<long long>(fastAttackEnd - holdEnd) * 34);
        QVERIFY2(holdEnd > 10 && holdEnd < 4 * kFps, qPrintable(QString::number(holdEnd)));
        // It began one frame before the hold: all but 34 ms of it is left.
        QVERIFY2((fastAttackEnd - holdEnd) * 34 >= kHoldLengthMs - 2 * 34,
                 qPrintable(QString::number(fastAttackEnd - holdEnd)));
        QVERIFY2(fastAttackEnd < fastAttacks.size(), qPrintable(QString::number(fastAttackEnd)));
    }

    void offDoesNothing()
    {
        SpectrumWidget w;
        configure(w);
        w.setAdjustGridMinToNoiseFloor(false);
        feed(w);
        QTest::qWait(1200);
        QCOMPARE(w.gridMin(), -150);
    }
};

QTEST_MAIN(TestGridFollowsDisplayNoiseFloor)
#include "tst_grid_follows_display_noise_floor.moc"
