// =================================================================
// tests/tst_spectrum_endpoint.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote daemon R3 Task 2.
//
// =================================================================

#include <QtTest>

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <random>

#include "core/WidebandFftEngine.h"
#include "core/session/media/DssWideRow.h"
#include "core/session/media/SpectrumEndpoint.h"
#include "core/session/media/SpectrumSampleClock.h"
#include "core/spectrum/ExtendedSpectrumReducer.h"
#include "core/spectrum/WidebandDisplayReference.h"

using namespace NereusSDR;

namespace {

SpectrumEndpointSourceContext sourceContext(quint64 generation = 11,
                                            quint32 contextGeneration = 4)
{
    SpectrumEndpointSourceContext source;
    source.source = {2, FftTier::Wide};
    source.sourceGeneration = generation;
    source.fftBins = 1024;
    source.centreHz = 14'200'000.0;
    source.sampleRateHz = 192'000.0;
    source.contextGeneration = contextGeneration;
    return source;
}

SpectrumEndpointRequest request()
{
    SpectrumEndpointRequest endpoint;
    endpoint.endpointId = 91;
    endpoint.source = {2, FftTier::Wide};
    endpoint.centreHz = 14'200'000.0;
    endpoint.spanHz = 96'000.0;
    endpoint.pixels = 64;
    endpoint.targetFps = 30;
    endpoint.framesPerLine = 2;
    endpoint.trace.detector = SpectrumDetectorMode::Peak;
    endpoint.trace.averageMode = 0;
    endpoint.waterfall.detector = SpectrumDetectorMode::Average;
    endpoint.waterfall.averageMode = 0;
    endpoint.requestedWideSpanFactor = 2.0;
    return endpoint;
}

DaemonSpectrumFrame frame(const SpectrumEndpointSourceContext& source,
                          qint64 producedAtNs)
{
    DaemonSpectrumFrame frame;
    frame.source = source.source;
    frame.generation = source.sourceGeneration;
    frame.centreHz = source.centreHz;
    frame.sampleRateHz = source.sampleRateHz;
    frame.producedAtNs = producedAtNs;
    frame.windowEnb = 1.0;
    frame.dbmOffset = 0.0;
    frame.binsLinear.fill(1.0e-12f, source.fftBins);
    frame.binsLinear[300] = 1.0e-5f;
    frame.binsLinear[800] = 1.0e-4f;
    return frame;
}

SpectrumEndpointSourceContext extendedSourceContext(int adc = 0,
                                                    quint32 widebandGeneration = 31,
                                                    quint64 generation = 11,
                                                    quint32 contextGeneration = 4)
{
    SpectrumEndpointSourceContext source = sourceContext(generation, contextGeneration);
    source.centreHz = 500'000.0;
    source.sampleRateHz = 1'000'000.0;
    source.wideband.available = true;
    source.wideband.active = true;
    source.wideband.physicalAdcIndex = adc;
    source.wideband.filterChainIndex = 0;
    source.wideband.sourceGeneration = widebandGeneration;
    source.wideband.adcRateHz = 4'000'000.0;
    return source;
}

SpectrumEndpointRequest extendedRequest()
{
    SpectrumEndpointRequest endpoint = request();
    endpoint.centreHz = 500'000.0;
    endpoint.spanHz = 2'000'000.0;
    endpoint.pixels = 100;
    endpoint.extendedView = true;
    return endpoint;
}

WidebandSpectrumFrame widebandFrame(const SpectrumEndpointSourceContext& source,
                                    float value = -140.0f)
{
    WidebandSpectrumFrame frame;
    frame.source.physicalAdcIndex = source.wideband.physicalAdcIndex;
    frame.source.sourceGeneration = source.wideband.sourceGeneration;
    frame.source.adcRateHz = source.wideband.adcRateHz;
    frame.producedAtNs = 1;
    frame.rawDbBins.fill(value, WidebandFftEngine::kOutputBins);
    return frame;
}

WidebandSpectrumFrame widebandAtDisplayLevel(const SpectrumEndpointSourceContext& source,
                                             float levelDbm)
{
    // The cache holds unnormalised FFT dB, not display dBm. These integration
    // fixtures choose a known display level through the existing reference;
    // separate reference/reducer tests characterize the normalization itself.
    const float reference = widebandRelativeReferenceDb(source.wideband.adcRateHz,
        source.sampleRateHz / source.fftBins, 1.0, SpectrumDetectorMode::Peak);
    return widebandFrame(source, levelDbm - reference);
}

bool allNear(const QVector<float>& values, float expected, float tolerance = 0.01f)
{
    return std::all_of(values.cbegin(), values.cend(), [expected, tolerance](float value) {
        return std::abs(value - expected) <= tolerance;
    });
}

} // namespace

class TstSpectrumEndpoint : public QObject
{
    Q_OBJECT

private slots:
    void acceptedPlaneWindowDepthReachesTheReducersAcrossCadenceChanges()
    {
        SpectrumEndpoint endpoint;
        SpectrumEndpointRequest req = request();
        req.framesPerLine = 1;
        req.trace.averageMode = 2;
        req.trace.averageFrames = 4;
        req.waterfall.averageMode = 2;
        req.waterfall.averageFrames = 6;
        SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(endpoint.configure(req, source));
        qint64 at = 1'000'000'000;
        std::optional<DisplayCodecFrame> output;
        for (float power : {1.0f, 3.0f, 5.0f}) {
            DaemonSpectrumFrame input = frame(source, at);
            input.binsLinear.fill(power);
            output = endpoint.consume(input);
            QVERIFY(output.has_value());
            at += 40'000'000;
        }
        QVERIFY(std::abs(output->traceDbm[32] - 4.7712125f) < 0.0001f);
        QVERIFY(std::abs(output->waterfallDbm[32] - 4.7712125f) < 0.0001f);
        req.targetFps = 15;
        req.trace.averageFrames = 2;
        ++source.contextGeneration;
        QVERIFY(endpoint.configure(req, source));
        QCOMPARE(endpoint.context().targetFps, 15);
        DaemonSpectrumFrame input = frame(source, at);
        input.binsLinear.fill(7.0f);
        output = endpoint.consume(input);
        QVERIFY(output.has_value());
        QVERIFY(std::abs(output->traceDbm[32] - 8.4509802f) < 0.0001f);
        // Unchanged waterfall depth keeps its existing window on renewal.
        QVERIFY(std::abs(output->waterfallDbm[32] - 6.0206000f) < 0.0001f);
    }

    void logAveragingDoesNotRestartAtZeroDbAfterRetune()
    {
        SpectrumEndpoint endpoint;
        auto req = request();
        req.trace.averageMode = 3;
        req.trace.averageAlpha = 0.5;
        auto source = sourceContext();
        QVERIFY(endpoint.configure(req, source));
        auto input = frame(source, 1'000'000'000);
        input.binsLinear.fill(1.0e-12f);
        auto output = endpoint.consume(input);
        QVERIFY(output);
        QVERIFY(std::abs(output->traceDbm[32] - (-140.0f)) < 0.001f);
        input.producedAtNs += 40'000'000;
        output = endpoint.consume(input);
        QVERIFY(output);
        QVERIFY(std::abs(output->traceDbm[32] - (-130.0f)) < 0.001f);

        ++source.sourceGeneration;
        ++source.contextGeneration;
        source.centreHz += 100;
        QVERIFY(endpoint.configure(req, source));
        QVERIFY(!endpoint.consume(input)); // Old source frames remain inadmissible.
        input = frame(source, 1'100'000'000);
        input.binsLinear.fill(1.0e-12f);
        output = endpoint.consume(input);
        QVERIFY(output);
        QVERIFY(std::abs(output->traceDbm[32] - (-140.0f)) < 0.001f);
    }

    void independentPlanesClampAndWideCoverage()
    {
        SpectrumEndpoint endpoint;
        const SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(endpoint.configure(request(), source));
        const SpectrumEndpointContext context = endpoint.context();
        QCOMPARE(context.codec.traceSamples, quint16(64));
        QCOMPARE(context.codec.waterfallSamples, quint16(64));
        QCOMPARE(context.codec.wideSamples, quint16(768));
        QVERIFY(context.exactSpanHz > 0.0);
        QVERIFY(context.wideSpanHz > context.exactSpanHz);

        const std::optional<DisplayCodecFrame> output = endpoint.consume(frame(source, 1'000'000'000));
        QVERIFY(output.has_value());
        QCOMPARE(output->traceDbm.size(), 64);
        QCOMPARE(output->waterfallDbm.size(), 64);
        QCOMPARE(output->wideDbm.size(), 768);
        QVERIFY(output->waterfallAdvance);
        QCOMPARE(output->encoderSequence, quint32(0));

        // Peak and average reducers cover the same crop but are independent:
        // the single-bin carrier is preserved by Peak and diluted by Average.
        const int exactPeak = std::distance(output->traceDbm.cbegin(),
                                            std::max_element(output->traceDbm.cbegin(),
                                                             output->traceDbm.cend()));
        QVERIFY(output->traceDbm.at(exactPeak) > output->waterfallDbm.at(exactPeak));
        const int widePeak = std::distance(output->wideDbm.cbegin(),
                                           std::max_element(output->wideDbm.cbegin(), output->wideDbm.cend()));
        QVERIFY(widePeak >= 0 && widePeak < output->wideDbm.size());
        QVERIFY(output->wideDbm.at(widePeak) > -80.0f);

        SpectrumEndpoint clamped;
        SpectrumEndpointRequest large = request();
        large.pixels = 8'000; // source bins then protocol cap
        QVERIFY(clamped.configure(large, source));
        // A half-DDC crop includes both inclusive boundary bins under the
        // existing floor/ceil reducer contract, therefore 513, not 1024.
        QCOMPARE(clamped.context().codec.traceSamples, quint16(513));
        // Core computes the same grant before any frame exists.
        QCOMPARE(SpectrumEndpoint::grantedPixels(large, source.fftBins, source.centreHz,
                                                 source.sampleRateHz, false), 513);
        QCOMPARE(SpectrumEndpoint::grantedPixels(large, source.fftBins, source.centreHz,
                                                 source.sampleRateHz, true),
                 SpectrumEndpoint::kMaxPixels);
        SpectrumEndpointRequest beside = large;
        beside.centreHz = source.centreHz + source.sampleRateHz * 4.0;
        QCOMPARE(SpectrumEndpoint::grantedPixels(beside, source.fftBins, source.centreHz,
                                                 source.sampleRateHz, false), 0);
    }

    // The wide row is the one a conversion of every FFT bin gives: each bin to
    // dBm, then the crop, then the peak reduction. The endpoint reduces the
    // powers first and converts what is left (at most kMaxWideSamples), which
    // gives the same row because the conversion rises with the power.
    void wideRowIsTheRowOfTheFullConversion_data()
    {
        QTest::addColumn<int>("fftBins");
        QTest::addColumn<double>("spanHz");
        QTest::addColumn<double>("centreOffsetHz");
        QTest::addColumn<int>("wideSamples");
        QTest::newRow("1024 bins, whole source reduced") << 1024 << 96'000.0 << 0.0 << 768;
        QTest::newRow("16384 bins, half the source reduced") << 16384 << 48'000.0 << 0.0 << 768;
        QTest::newRow("16384 bins, crop at the source edge") << 16384 << 48'000.0 << 70'000.0 << 768;
        QTest::newRow("16384 bins, fewer bins than samples") << 16384 << 3'000.0 << -20'000.0 << 512;
    }

    void wideRowIsTheRowOfTheFullConversion()
    {
        QFETCH(int, fftBins);
        QFETCH(double, spanHz);
        QFETCH(double, centreOffsetHz);
        QFETCH(int, wideSamples);

        SpectrumEndpointSourceContext source = sourceContext();
        source.fftBins = fftBins;
        SpectrumEndpointRequest asked = request();
        asked.spanHz = spanHz;
        asked.centreHz = source.centreHz + centreOffsetHz;
        SpectrumEndpoint endpoint;
        QVERIFY(endpoint.configure(asked, source));
        QCOMPARE(int(endpoint.context().codec.wideSamples), wideSamples);

        DaemonSpectrumFrame input = frame(source, 1'000'000'000);
        input.dbmOffset = -23.75;
        const double stationOffsetDb = 4.5;
        // Powers over 14 decades, a zero and a run of equal values, from a
        // fixed sequence.
        quint32 state = 0x2545F491u;
        for (int index = 0; index < fftBins; ++index) {
            state = state * 1664525u + 1013904223u;
            const double unit = (state >> 8) / 16777216.0;
            input.binsLinear[index] = static_cast<float>(std::pow(10.0, -16.0 + 14.0 * unit));
        }
        input.binsLinear[fftBins / 2] = 0.0f;
        for (int index = fftBins / 2 + 40; index < fftBins / 2 + 60; ++index) {
            input.binsLinear[index] = 3.0e-9f;
        }

        const double scale = std::pow(10.0, (input.dbmOffset + stationOffsetDb) / 10.0);
        QVector<float> fullDbm(fftBins);
        for (int index = 0; index < fftBins; ++index) {
            fullDbm[index] = static_cast<float>(
                10.0 * std::log10(scale * input.binsLinear.at(index) + 1.0e-60));
        }
        const DssWideRow crop = cropDssWideRow(fullDbm, {
            asked.centreHz, asked.spanHz, source.centreHz, source.sampleRateHz,
            asked.requestedWideSpanFactor});
        const QVector<float> expected =
            peakReduceDssWideRow(crop.binsDbm, SpectrumEndpoint::kMaxWideSamples);
        QCOMPARE(int(expected.size()), wideSamples);

        const std::optional<DisplayCodecFrame> output = endpoint.consume(input, stationOffsetDb);
        QVERIFY(output.has_value());
        QCOMPARE(output->wideDbm, expected);
    }

    // A bin that does not convert refuses the whole frame, wherever the bin
    // sits: here outside the view and outside the wide row.
    void wideRowRefusesAFrameWithABinThatDoesNotConvert()
    {
        SpectrumEndpointSourceContext source = sourceContext();
        SpectrumEndpointRequest asked = request();
        asked.spanHz = 24'000.0;   // wide row: 48 kHz of the 192 kHz source
        SpectrumEndpoint endpoint;
        QVERIFY(endpoint.configure(asked, source));
        QVERIFY(endpoint.context().codec.wideSamples != 0);

        DaemonSpectrumFrame input = frame(source, 1'000'000'000);
        input.dbmOffset = 3000.0;              // a scale of 1e300
        input.binsLinear.fill(1.0e-30f);
        QVERIFY(endpoint.consume(input).has_value());

        input.producedAtNs = 2'000'000'000;
        input.binsLinear[10] = 1.0e10f;        // 1e310: past a double
        QVERIFY(!endpoint.consume(input).has_value());
    }

    void cadenceAndGenerationReset()
    {
        SpectrumEndpoint endpoint;
        SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(endpoint.configure(request(), source));
        QVERIFY(endpoint.consume(frame(source, 1'000'000'000)).has_value());
        QVERIFY(!endpoint.consume(frame(source, 1'010'000'000)).has_value());
        const std::optional<DisplayCodecFrame> second = endpoint.consume(frame(source, 1'040'000'000));
        QVERIFY(second.has_value());
        QVERIFY(!second->waterfallAdvance);
        QCOMPARE(second->encoderSequence, quint32(1));

        source.sourceGeneration = 12;
        source.contextGeneration = 5;
        QVERIFY(endpoint.configure(request(), source));
        const std::optional<DisplayCodecFrame> reset = endpoint.consume(frame(source, 2'000'000'000));
        QVERIFY(reset.has_value());
        QCOMPARE(reset->context.contextGeneration, quint32(5));
        QCOMPARE(reset->encoderSequence, quint32(0));
        QVERIFY(reset->waterfallAdvance);
    }

    // R-R3-08, R-R3-37: the sender orders endpoints by this deadline. It
    // reads the schedule consume() keeps and never moves it.
    void outputDeadlineReadsTheScheduleWithoutMovingIt()
    {
        SpectrumEndpoint endpoint;
        SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(!endpoint.outputDeadlineNs(1'000'000'000).has_value()); // Not configured.
        QVERIFY(endpoint.configure(request(), source));
        constexpr qint64 kPeriodNs = 1'000'000'000LL / 30;
        // A first frame fills the period that starts with it.
        QCOMPARE(endpoint.outputDeadlineNs(1'000'000'000),
                 std::optional<qint64>(1'000'000'000 + kPeriodNs));
        QCOMPARE(endpoint.outputDeadlineNs(1'000'000'000),
                 std::optional<qint64>(1'000'000'000 + kPeriodNs));
        QVERIFY(endpoint.consume(frame(source, 1'000'000'000)).has_value());
        // Early and not newer: consume() would take nothing, so no deadline.
        QVERIFY(!endpoint.outputDeadlineNs(1'010'000'000).has_value());
        QVERIFY(!endpoint.outputDeadlineNs(1'000'000'000).has_value());
        // On the calendar: the end of the next period.
        QCOMPARE(endpoint.outputDeadlineNs(1'040'000'000),
                 std::optional<qint64>(1'000'000'000 + 2 * kPeriodNs));
        // A frame that already missed a whole slot starts a new one, as
        // consume() would resynchronize it.
        QCOMPARE(endpoint.outputDeadlineNs(1'100'000'000),
                 std::optional<qint64>(1'100'000'000 + kPeriodNs));
        // None of these reads moved the schedule.
        QVERIFY(!endpoint.consume(frame(source, 1'010'000'000)).has_value());
        QVERIFY(endpoint.consume(frame(source, 1'040'000'000)).has_value());
        QCOMPARE(endpoint.outputDeadlineNs(1'080'000'000),
                 std::optional<qint64>(1'000'000'000 + 3 * kPeriodNs));
    }

    void cadenceScheduleToleratesNominalJitterAndPreservesWaterfallCadence()
    {
        SpectrumEndpoint endpoint;
        const SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(endpoint.configure(request(), source));

        qint64 producedAtNs = 1'000'000'000;
        int waterfallAdvances = 0;
        constexpr int kFrames = 120;
        for (int index = 0; index < kFrames; ++index) {
            const auto output = endpoint.consume(frame(source, producedAtNs));
            QVERIFY2(output.has_value(),
                     "32 ms / 34.666667 ms source jitter must retain the 30 FPS schedule");
            QCOMPARE(output->encoderSequence, static_cast<quint32>(index));
            if (output->waterfallAdvance) {
                ++waterfallAdvances;
            }
            producedAtNs += (index % 2 == 0) ? 32'000'000 : 34'666'667;
        }
        QCOMPARE(waterfallAdvances, kFrames / request().framesPerLine);
    }

    void fasterProducerStaysCappedAndStallDoesNotCatchUp()
    {
        SpectrumEndpoint endpoint;
        const SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(endpoint.configure(request(), source));

        int emitted = 0;
        for (qint64 offsetNs = 0; offsetNs < 1'000'000'000; offsetNs += 10'000'000) {
            if (endpoint.consume(frame(source, 1'000'000'000 + offsetNs)).has_value()) {
                ++emitted;
            }
        }
        // A 100 FPS shared FFT producer must not make this 30 FPS endpoint
        // exceed its own budget. Inclusive time endpoints allow 30 frames.
        QVERIFY(emitted <= 30);
        QVERIFY(emitted >= 29);

        SpectrumEndpoint afterStall;
        QVERIFY(afterStall.configure(request(), source));
        QVERIFY(afterStall.consume(frame(source, 1'000'000'000)).has_value());
        QVERIFY(afterStall.consume(frame(source, 2'000'000'000)).has_value());
        for (qint64 offsetNs = 1'000'000; offsetNs < 20'000'000;
             offsetNs += 1'000'000) {
            QVERIFY(!afterStall.consume(frame(source, 2'000'000'000 + offsetNs)).has_value());
        }
    }

    // How early a frame may come and still fill its slot: half the
    // source's spacing, within a twentieth and a half of the period.
    void earlyToleranceIsHalfTheSourceSpacing()
    {
        constexpr qint64 kPeriodNs = 1'000'000'000LL / 15;
        QCOMPARE(SpectrumEndpoint::earlyToleranceNs(kPeriodNs, 0), kPeriodNs / 20);
        QCOMPARE(SpectrumEndpoint::earlyToleranceNs(kPeriodNs, -1), kPeriodNs / 20);
        QCOMPARE(SpectrumEndpoint::earlyToleranceNs(kPeriodNs, 1'000'000), kPeriodNs / 20);
        QCOMPARE(SpectrumEndpoint::earlyToleranceNs(kPeriodNs, 33'333'333),
                 qint64(16'666'666));
        QCOMPARE(SpectrumEndpoint::earlyToleranceNs(kPeriodNs, 200'000'000), kPeriodNs / 2);
    }

    // A 15 fps pan on a 30 fps source whose stamps jitter by up to 8 ms
    // (the FFT publish times, as stamped before SpectrumSampleClock): every
    // row gap is near 67 ms. Without the source spacing (the old twentieth
    // of a period alone) the same frames gave 33 and 100 ms gaps.
    void halfRatePanTakesEvenlySpacedRows()
    {
        const auto rowGaps = [](bool withSourceSpacing) {
            SpectrumEndpoint endpoint;
            const SpectrumEndpointSourceContext source = sourceContext();
            SpectrumEndpointRequest pan = request();
            pan.targetFps = 15;
            pan.framesPerLine = 1;
            if (!endpoint.configure(pan, source)) {
                return QVector<qint64>{};
            }
            std::mt19937 random(598);
            std::uniform_int_distribution<qint64> jitter(0, 8'000'000);
            QVector<qint64> gaps;
            qint64 previousStampNs = 0;
            qint64 previousRowNs = 0;
            for (int index = 0; index < 600; ++index) {
                const qint64 stampNs =
                    1'000'000'000 + index * 33'333'333LL + jitter(random);
                DaemonSpectrumFrame next = frame(source, stampNs);
                next.sourceFrameGapNs =
                    withSourceSpacing && index > 0 ? stampNs - previousStampNs : 0;
                previousStampNs = stampNs;
                if (endpoint.consume(next).has_value()) {
                    if (previousRowNs != 0) {
                        gaps.append(stampNs - previousRowNs);
                    }
                    previousRowNs = stampNs;
                }
            }
            return gaps;
        };

        const QVector<qint64> gaps = rowGaps(true);
        QVERIFY(gaps.size() >= 295);
        for (const qint64 gapNs : gaps) {
            QVERIFY2(gapNs >= 58'000'000 && gapNs <= 75'500'000,
                     qPrintable(QStringLiteral("row gap %1 ns").arg(gapNs)));
        }
        // The old rule on the same frames: the check above can fail.
        const QVector<qint64> oldGaps = rowGaps(false);
        QVERIFY(std::any_of(oldGaps.cbegin(), oldGaps.cend(), [](qint64 gapNs) {
            return gapNs < 45'000'000 || gapNs > 90'000'000;
        }));
    }

    // The same pan on stamps from the source's sample clock: the rows are
    // even to within the clock's settling, against 8 ms of publish jitter.
    void halfRatePanOnSampleClockStampsIsEven()
    {
        SpectrumEndpoint endpoint;
        const SpectrumEndpointSourceContext source = sourceContext();
        SpectrumEndpointRequest pan = request();
        pan.targetFps = 15;
        pan.framesPerLine = 1;
        QVERIFY(endpoint.configure(pan, source));
        SpectrumSampleClock clock;
        std::mt19937 random(598);
        std::uniform_int_distribution<qint64> jitter(0, 8'000'000);
        constexpr qint64 kPairsPerFrame = 6400; // 30 fps at 192 kHz
        qint64 previousStampNs = 0;
        qint64 previousRowNs = 0;
        int rows = 0;
        for (int index = 0; index < 600; ++index) {
            const qint64 position = (index + 1) * kPairsPerFrame;
            const qint64 publishedAtNs =
                1'000'000'000 + index * 33'333'333LL + jitter(random);
            const qint64 stampNs = clock.stamp(position, source.sampleRateHz, publishedAtNs);
            DaemonSpectrumFrame next = frame(source, stampNs);
            next.sourceFrameGapNs = index > 0 ? stampNs - previousStampNs : 0;
            previousStampNs = stampNs;
            if (endpoint.consume(next).has_value()) {
                if (previousRowNs != 0 && index >= 60) {
                    const qint64 gapNs = stampNs - previousRowNs;
                    QVERIFY2(std::abs(gapNs - 66'666'667) <= 3'000'000,
                             qPrintable(QStringLiteral("row %1 gap %2 ns")
                                            .arg(index).arg(gapNs)));
                }
                previousRowNs = stampNs;
                ++rows;
            }
        }
        QVERIFY(rows >= 295);
    }

    // The nearest-frame rule does not let a fast source past the pan's
    // frame rate: a 100 fps source on a 30 fps pan is still capped.
    void fasterProducerWithItsSpacingStaysCapped()
    {
        SpectrumEndpoint endpoint;
        const SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(endpoint.configure(request(), source));
        int emitted = 0;
        for (qint64 offsetNs = 0; offsetNs < 1'000'000'000; offsetNs += 10'000'000) {
            DaemonSpectrumFrame next = frame(source, 1'000'000'000 + offsetNs);
            next.sourceFrameGapNs = offsetNs > 0 ? 10'000'000 : 0;
            if (endpoint.consume(next).has_value()) {
                ++emitted;
            }
        }
        QVERIFY(emitted <= 30);
        QVERIFY(emitted >= 29);
    }

    void backwardAndDuplicateProducerTimestampsCannotEmitTwice()
    {
        SpectrumEndpoint endpoint;
        const SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(endpoint.configure(request(), source));

        const auto first = endpoint.consume(frame(source, 1'000'000'000));
        QVERIFY(first.has_value());
        QCOMPARE(first->encoderSequence, quint32(0));
        QVERIFY(!endpoint.consume(frame(source, 1'000'000'000)).has_value());
        QVERIFY(!endpoint.consume(frame(source, 999'999'999)).has_value());

        const auto next = endpoint.consume(frame(source, 1'040'000'000));
        QVERIFY(next.has_value());
        QCOMPARE(next->encoderSequence, quint32(1));
    }

    void deepCropNeverClaimsMorePixelsThanItsBins()
    {
        SpectrumEndpoint endpoint;
        const SpectrumEndpointSourceContext source = sourceContext();
        SpectrumEndpointRequest deep = request();
        deep.spanHz = source.sampleRateHz / source.fftBins;
        deep.pixels = 512;
        QVERIFY(endpoint.configure(deep, source));
        const SpectrumEndpointContext context = endpoint.context();
        const int coveredBins = qRound(context.exactSpanHz
                                       / (source.sampleRateHz / source.fftBins));
        QVERIFY(coveredBins > 0);
        // One bin of span touches two bins with its low edge on a bin edge
        // and three anywhere else, as here. It is granted the two it has at
        // every alignment (R-DISP-RETUNE).
        QCOMPARE(coveredBins, 3);
        QCOMPARE(context.codec.traceSamples, quint16(2));
        QCOMPARE(context.codec.waterfallSamples, quint16(2));
        const std::optional<DisplayCodecFrame> output = endpoint.consume(frame(source, 1'000'000'000));
        QVERIFY(output.has_value());
        QCOMPARE(output->traceDbm.size(), 2);
    }

    // R-DISP-RETUNE: a crop of fewer bins than the points asked is granted
    // its bins. The bins a crop touches are one more at some alignments
    // than at others, and the points granted are the same at every one, so
    // a tuning step moves the picture and never starts a new one. This is
    // the mini display's 20 kHz beside a pan whose FFT it shares.
    void aCropOfFewerBinsThanItsPointsKeepsItsPointCountAsItMoves()
    {
        SpectrumEndpointSourceContext source = sourceContext();
        source.fftBins = 8192;
        const double binHz = source.sampleRateHz / source.fftBins;
        SpectrumEndpointRequest req = request();
        req.spanHz = 20'000.0; // 853.33 bins
        req.pixels = 1024;

        SpectrumEndpoint endpoint;
        QVERIFY(endpoint.configure(req, source));
        const int points = endpoint.context().codec.traceSamples;
        QVERIFY2(points > 800 && points < req.pixels, qPrintable(QString::number(points)));
        qint64 at = 1'000'000'000;
        QVERIFY(endpoint.consume(frame(source, at)).has_value());

        QSet<int> binsTouched;
        SpectrumEndpointRequest next = req;
        for (int step = 0; step < 16; ++step) {
            next.centreHz += 3'005.0; // no whole number of bins
            ++source.contextGeneration;
            endpoint.awaitContext();
            QVERIFY(endpoint.configure(next, source));
            const int touched = qRound(endpoint.context().exactSpanHz / binHz);
            binsTouched.insert(touched);
            const QByteArray where = QStringLiteral("step %1, %2 bins touched")
                                         .arg(step).arg(touched).toUtf8();
            QVERIFY2(endpoint.context().codec.traceSamples == points, where.constData());
            QVERIFY2(endpoint.context().codec.waterfallSamples == points, where.constData());
            QVERIFY2(SpectrumEndpoint::grantedPixels(next, source.fftBins, source.centreHz,
                                                     source.sampleRateHz, false) == points,
                     where.constData());
            QVERIFY2(endpoint.picture() == SpectrumPictureKind::Moved, where.constData());
            // Never more points than bins.
            QVERIFY2(points <= touched, where.constData());
            at += 40'000'000;
            const std::optional<DisplayCodecFrame> output = endpoint.consume(frame(source, at));
            QVERIFY2(output.has_value(), where.constData());
            QVERIFY2(output->traceDbm.size() == points, where.constData());
        }
        // The case under test did happen: the crop touched a bin more at
        // some alignments than at others.
        QCOMPARE(binsTouched.size(), 2);
    }

    void outOfRangeAndStaleSourceNeverYieldPriorFrame()
    {
        SpectrumEndpoint endpoint;
        SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(endpoint.configure(request(), source));
        QVERIFY(endpoint.consume(frame(source, 1'000'000'000)).has_value());

        // R-R3-09: a crop beside the source is refused, and the refusal
        // keeps the accepted endpoint painting.
        const double acceptedSpan = endpoint.context().exactSpanHz;
        SpectrumEndpointRequest outside = request();
        outside.centreHz = source.centreHz + source.sampleRateHz * 4.0;
        QVERIFY(!endpoint.configure(outside, source));
        QCOMPARE(endpoint.context().exactSpanHz, acceptedSpan);
        QVERIFY(endpoint.consume(frame(source, 2'000'000'000)).has_value());

        QVERIFY(endpoint.configure(request(), source));
        DaemonSpectrumFrame stale = frame(source, 3'000'000'000);
        stale.generation = source.sourceGeneration - 1;
        QVERIFY(!endpoint.consume(stale).has_value());
    }

    void validatesOwnershipAndInput()
    {
        SpectrumEndpoint endpoint;
        SpectrumEndpointRequest bad = request();
        bad.endpointId = 0;
        QVERIFY(!endpoint.configure(bad, sourceContext()));
        bad = request();
        bad.source.streamIndex = 3;
        QVERIFY(!endpoint.configure(bad, sourceContext()));
        bad = request();
        bad.trace.averageMode = 4;
        QVERIFY(!endpoint.configure(bad, sourceContext()));
        bad = request();
        bad.targetFps = 61;
        QVERIFY(!endpoint.configure(bad, sourceContext()));
        bad = request();
        bad.framesPerLine = kMaxFramesPerLine + 1;
        QVERIFY(!endpoint.configure(bad, sourceContext()));
        bad = request();
        bad.minDbm = static_cast<float>(kMinDbmLimit) - 1.0f;
        QVERIFY(!endpoint.configure(bad, sourceContext()));
        bad = request();
        bad.maxDbm = static_cast<float>(kMaxDbmLimit) + 1.0f;
        QVERIFY(!endpoint.configure(bad, sourceContext()));
        bad = request();
        bad.framesPerLine = kMaxFramesPerLine;
        bad.minDbm = static_cast<float>(kMinDbmLimit);
        bad.maxDbm = static_cast<float>(kMaxDbmLimit);
        QVERIFY(endpoint.configure(bad, sourceContext()));
        endpoint.reset();
        bad = request();
        bad.centreHz = std::numeric_limits<double>::max();
        bad.spanHz = std::numeric_limits<double>::max();
        QVERIFY(!endpoint.configure(bad, sourceContext()));
        SpectrumEndpointSourceContext oversized = sourceContext();
        oversized.fftBins = std::numeric_limits<int>::max();
        QVERIFY(!endpoint.configure(request(), oversized));

        SpectrumEndpoint configured;
        const SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(configured.configure(request(), source));
        DaemonSpectrumFrame nonfinite = frame(source, 1'000'000'000);
        nonfinite.binsLinear[0] = std::numeric_limits<float>::infinity();
        QVERIFY(!configured.consume(nonfinite).has_value());
    }

    void extendedCompositeMatchesCoreReducerForEveryDetector()
    {
        const SpectrumEndpointSourceContext source = extendedSourceContext();
        DaemonSpectrumFrame ddc = frame(source, 1'000'000'000);
        ddc.binsLinear.fill(1.0e-12f);
        ddc.binsLinear[512] = 1.0e-4f;
        WidebandSpectrumFrame adc = widebandFrame(source);
        adc.rawDbBins[24'576] = -80.0f;

        for (SpectrumDetectorMode detector : {SpectrumDetectorMode::Peak,
                                               SpectrumDetectorMode::Rosenfell,
                                               SpectrumDetectorMode::Average,
                                               SpectrumDetectorMode::Sample,
                                               SpectrumDetectorMode::RMS}) {
            SpectrumEndpointRequest req = extendedRequest();
            req.trace.detector = detector;
            req.waterfall.detector = detector;
            SpectrumEndpoint endpoint;
            QVERIFY(endpoint.configure(req, source));

            ExtendedSpectrumReducer expectedTrace;
            ExtendedSpectrumReducer expectedWaterfall;
            ReducerConfig traceConfig;
            traceConfig.pixels = req.pixels;
            traceConfig.centreHz = req.centreHz;
            traceConfig.spanHz = req.spanHz;
            traceConfig.streamCentreHz = source.centreHz;
            traceConfig.sampleRateHz = source.sampleRateHz;
            traceConfig.detector = detector;
            traceConfig.averageMode = req.trace.averageMode;
            traceConfig.averageAlpha = req.trace.averageAlpha;
            ReducerConfig waterfallConfig = traceConfig;
            waterfallConfig.averageMode = req.waterfall.averageMode;
            waterfallConfig.averageAlpha = req.waterfall.averageAlpha;
            expectedTrace.setConfig(traceConfig);
            expectedWaterfall.setConfig(waterfallConfig);
            QVector<float> expectedTraceRow;
            QVector<float> expectedWaterfallRow;
            QVERIFY(expectedTrace.reduce(ddc.binsLinear, ddc.windowEnb, ddc.dbmOffset,
                                         adc.rawDbBins, source.wideband.adcRateHz, 3.5,
                                         req.minDbm, expectedTraceRow));
            QVERIFY(expectedWaterfall.reduce(ddc.binsLinear, ddc.windowEnb, ddc.dbmOffset,
                                             adc.rawDbBins, source.wideband.adcRateHz, 3.5,
                                             req.minDbm, expectedWaterfallRow));

            const auto actual = endpoint.consume(ddc, 3.5, adc);
            QVERIFY(actual.has_value());
            QCOMPARE(actual->traceDbm, expectedTraceRow);
            QCOMPARE(actual->waterfallDbm, expectedWaterfallRow);
        }
    }

    void extendedPermissionAndActivationRemainSeparate()
    {
        SpectrumEndpointRequest unavailablePermission = request();
        unavailablePermission.extendedView = true;
        SpectrumEndpoint endpoint;
        // Permission alone does not activate or require an ADC source.
        const SpectrumEndpointSourceContext ddcOnly = sourceContext();
        QVERIFY(endpoint.configure(unavailablePermission, ddcOnly));
        QVERIFY(!endpoint.context().wideband.available);
        QVERIFY(!endpoint.context().wideband.active);
        QVERIFY(endpoint.consume(frame(ddcOnly, 1'000'000'000)).has_value());

        SpectrumEndpointRequest req = extendedRequest();
        SpectrumEndpointSourceContext inactive = extendedSourceContext();
        inactive.wideband.active = false;
        inactive.wideband.sourceGeneration = 0;
        QVERIFY(endpoint.configure(req, inactive));
        QVERIFY(!endpoint.context().wideband.active);
        QCOMPARE(endpoint.context().wideband.sourceGeneration, quint32(0));
        QVERIFY(endpoint.context().wideband.available);
        QVERIFY(endpoint.context().exactSpanHz < req.spanHz);
        QVERIFY(endpoint.consume(frame(inactive, 1'000'000'000)).has_value());

        // An active ADC cannot compose a row without the negotiated permission.
        const SpectrumEndpointContext ddcContext = endpoint.context();
        req.extendedView = false;
        const SpectrumEndpointSourceContext active = extendedSourceContext();
        QVERIFY(!endpoint.configure(req, active));
        QCOMPARE(endpoint.context().codec.contextGeneration,
                 ddcContext.codec.contextGeneration);
        QCOMPARE(endpoint.context().wideband, ddcContext.wideband);

        SpectrumEndpointSourceContext malformed = inactive;
        malformed.wideband.sourceGeneration = 99;
        QVERIFY(!endpoint.configure(extendedRequest(), malformed));
    }

    void extendedStationScalarAndAdcIdentityAreAppliedExactlyOnce()
    {
        for (int adcIndex : {0, 1}) {
            const SpectrumEndpointSourceContext source = extendedSourceContext(adcIndex);
            SpectrumEndpointRequest req = extendedRequest();
            req.centreHz = 1'500'000.0; // DDC is absent; ADC owns every pixel.
            req.spanHz = 500'000.0;
            SpectrumEndpoint endpoint;
            QVERIFY(endpoint.configure(req, source));
            const auto base = endpoint.consume(frame(source, 1'000'000'000), 0.0,
                                               widebandFrame(source, -120.0f));
            QVERIFY(base.has_value());

            SpectrumEndpoint shifted;
            QVERIFY(shifted.configure(req, source));
            const auto plusSeven = shifted.consume(frame(source, 1'000'000'000), 7.0,
                                                   widebandFrame(source, -120.0f));
            QVERIFY(plusSeven.has_value());
            QVERIFY(std::abs((plusSeven->traceDbm[50] - base->traceDbm[50]) - 7.0f) < 0.01f);
            QVERIFY(std::abs((plusSeven->waterfallDbm[50] - base->waterfallDbm[50]) - 7.0f) < 0.01f);
        }
    }

    void extendedMissingAndMismatchedAdcRowsPaintFloor()
    {
        const SpectrumEndpointSourceContext source = extendedSourceContext();
        SpectrumEndpointRequest req = extendedRequest();
        req.centreHz = 1'500'000.0; // no DDC island
        req.spanHz = 500'000.0;
        SpectrumEndpoint endpoint;
        QVERIFY(endpoint.configure(req, source));
        const auto missing = endpoint.consume(frame(source, 1'000'000'000));
        QVERIFY(missing.has_value());
        QVERIFY(allNear(missing->traceDbm, req.minDbm));
        QVERIFY(allNear(missing->waterfallDbm, req.minDbm));

        for (const auto mismatch : {0, 1, 2}) {
            SpectrumEndpoint wrongSource;
            QVERIFY(wrongSource.configure(req, source));
            WidebandSpectrumFrame stale = widebandFrame(source, -20.0f);
            if (mismatch == 0) {
                ++stale.source.physicalAdcIndex;
            } else if (mismatch == 1) {
                stale.source.adcRateHz *= 0.5;
            } else {
                ++stale.source.sourceGeneration;
            }
            const auto masked = wrongSource.consume(frame(source, 1'000'000'000), 0.0, stale);
            QVERIFY(masked.has_value());
            QVERIFY(allNear(masked->traceDbm, req.minDbm));
            QVERIFY(allNear(masked->waterfallDbm, req.minDbm));
        }
    }

    void extendedAllowsNoDdcIslandAndFloorsOutsideAdcGeometry()
    {
        const SpectrumEndpointSourceContext source = extendedSourceContext();
        WidebandSpectrumFrame adc = widebandAtDisplayLevel(source, -90.0f);

        SpectrumEndpoint noIsland;
        SpectrumEndpointRequest wingOnly = extendedRequest();
        wingOnly.centreHz = 1'500'000.0;
        wingOnly.spanHz = 500'000.0;
        QVERIFY(noIsland.configure(wingOnly, source));
        const auto wingRow = noIsland.consume(frame(source, 1'000'000'000), 0.0, adc);
        QVERIFY(wingRow.has_value());
        QVERIFY(wingRow->traceDbm[50] > wingOnly.minDbm + 10.0f);

        SpectrumEndpoint belowDc;
        SpectrumEndpointRequest negative = extendedRequest();
        negative.centreHz = -250'000.0;
        negative.spanHz = 500'000.0;
        QVERIFY(belowDc.configure(negative, source));
        const auto below = belowDc.consume(frame(source, 1'000'000'000), 0.0, adc);
        QVERIFY(below.has_value());
        QVERIFY(allNear(below->traceDbm, negative.minDbm));

        SpectrumEndpoint beyondNyquist;
        SpectrumEndpointRequest high = extendedRequest();
        high.centreHz = 2'250'000.0;
        high.spanHz = 1'000'000.0;
        QVERIFY(beyondNyquist.configure(high, source));
        const auto above = beyondNyquist.consume(frame(source, 1'000'000'000), 0.0, adc);
        QVERIFY(above.has_value());
        QVERIFY(above->traceDbm.first() > high.minDbm + 10.0f);
        QVERIFY(std::abs(above->traceDbm.last() - high.minDbm) < 0.01f);
    }

    void extendedHistoriesResetOnSourceChangeAndKeepDdcWidePlane()
    {
        SpectrumEndpointSourceContext source = extendedSourceContext();
        SpectrumEndpointRequest req = extendedRequest();
        req.centreHz = 1'500'000.0;
        req.spanHz = 500'000.0;
        req.trace.averageMode = 1;
        req.trace.averageAlpha = 0.5;
        req.waterfall.averageMode = 0;
        SpectrumEndpoint endpoint;
        QVERIFY(endpoint.configure(req, source));
        QVERIFY(endpoint.consume(frame(source, 1'000'000'000), 0.0,
                                  widebandAtDisplayLevel(source, -100.0f)).has_value());
        const auto loud = endpoint.consume(frame(source, 1'040'000'000), 0.0,
                                           widebandAtDisplayLevel(source, -40.0f));
        QVERIFY(loud.has_value());
        QVERIFY(loud->traceDbm[50] < loud->waterfallDbm[50] - 1.0f);

        ++source.sourceGeneration;
        ++source.contextGeneration;
        ++source.wideband.sourceGeneration;
        QVERIFY(endpoint.configure(req, source));
        const DaemonSpectrumFrame fresh = frame(source, 1'040'000'000);
        const WidebandSpectrumFrame quiet = widebandAtDisplayLevel(source, -100.0f);
        const auto reset = endpoint.consume(fresh, 0.0, quiet);
        QVERIFY(reset.has_value());

        SpectrumEndpoint freshEndpoint;
        QVERIFY(freshEndpoint.configure(req, source));
        const auto expected = freshEndpoint.consume(fresh, 0.0, quiet);
        QVERIFY(expected.has_value());
        QCOMPARE(reset->traceDbm, expected->traceDbm);
        QCOMPARE(reset->waterfallDbm, expected->waterfallDbm);

        SpectrumEndpointRequest ddcWide = extendedRequest();
        // A half-DDC view crossing its upper edge needs ADC wings while
        // still leaving off-screen DDC coverage for the optional 3D row.
        // A full-DDC view correctly has no additional DDC history to send.
        ddcWide.centreHz = source.centreHz + source.sampleRateHz * 0.4;
        ddcWide.spanHz = source.sampleRateHz * 0.5;
        ddcWide.requestedWideSpanFactor = 2.0;
        SpectrumEndpoint wideEndpoint;
        QVERIFY(wideEndpoint.configure(ddcWide, source));
        DaemonSpectrumFrame ddcNoise = frame(source, 2'000'000'000);
        ddcNoise.binsLinear.fill(1.0e-12f);
        const auto output = wideEndpoint.consume(ddcNoise, 5.0, widebandFrame(source, -10.0f));
        QVERIFY(output.has_value());
        QVERIFY(!output->wideDbm.isEmpty());
        // The ADC's -10 dB row is intentionally absent from the legacy
        // DDC-only 3D history, whose DDC noise floor is -120 dB plus scalar.
        QVERIFY(*std::max_element(output->wideDbm.cbegin(), output->wideDbm.cend()) < -100.0f);
    }

    // R-DISP-HOLD: a second configure that changes only the dBm range is the
    // same picture. The averaged trace is in linear power per output pixel
    // and does not depend on the range it is coded in, so it carries on. The
    // context, the sequence and the pacing still restart.
    void aDbmRangeChangeKeepsTheAveragedPlanes()
    {
        SpectrumEndpoint endpoint;
        SpectrumEndpointRequest req = request();
        req.trace.averageMode = 3;
        req.trace.averageAlpha = 0.5;
        req.waterfall.averageMode = 3;
        req.waterfall.averageAlpha = 0.5;
        SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(endpoint.configure(req, source));
        qint64 at = 1'000'000'000;
        std::optional<DisplayCodecFrame> output;
        for (int index = 0; index < 40; ++index, at += 40'000'000) {
            DaemonSpectrumFrame input = frame(source, at);
            input.binsLinear.fill(1.0e-12f);
            output = endpoint.consume(input);
            QVERIFY(output.has_value());
        }
        const QVector<float> convergedTrace = output->traceDbm;
        const QVector<float> convergedWaterfall = output->waterfallDbm;
        QVERIFY(allNear(convergedTrace, -120.0f));
        QVERIFY(allNear(convergedWaterfall, -120.0f));
        QVERIFY(output->encoderSequence > 0);

        SpectrumEndpointRequest moved = req;
        moved.minDbm = -150.0f;
        moved.maxDbm = -20.0f;
        ++source.contextGeneration;
        QVERIFY(endpoint.configure(moved, source));
        QCOMPARE(endpoint.context().codec.minDbm, -150.0f);
        QCOMPARE(endpoint.context().codec.maxDbm, -20.0f);
        QCOMPARE(endpoint.context().codec.contextGeneration, source.contextGeneration);
        DaemonSpectrumFrame input = frame(source, at);
        input.binsLinear.fill(1.0e-12f);
        output = endpoint.consume(input);
        QVERIFY(output.has_value());
        // The context is new: its sequence and its waterfall pacing restart.
        QCOMPARE(output->encoderSequence, quint32(0));
        QVERIFY(output->waterfallAdvance);
        // One codec step of the new range.
        const float step = (moved.maxDbm - moved.minDbm) / 255.0f;
        QCOMPARE(output->traceDbm.size(), convergedTrace.size());
        for (int index = 0; index < convergedTrace.size(); ++index) {
            QVERIFY2(std::abs(output->traceDbm.at(index) - convergedTrace.at(index)) <= step,
                     qPrintable(QStringLiteral("trace pixel %1 restarted at %2 dBm")
                                    .arg(index).arg(output->traceDbm.at(index))));
            QVERIFY2(std::abs(output->waterfallDbm.at(index) - convergedWaterfall.at(index))
                         <= step,
                     qPrintable(QStringLiteral("waterfall pixel %1 restarted at %2 dBm")
                                    .arg(index).arg(output->waterfallDbm.at(index))));
        }
    }

    // R-DISP-HOLD: frame rate, frames per line and the averaging constant
    // that follows the frame rate may differ in the same picture. The state
    // is kept and the next frame is averaged with the new constant.
    void aFrameRateChangeKeepsTheStateAndTakesTheNewConstant()
    {
        SpectrumEndpoint endpoint;
        SpectrumEndpointRequest req = request();
        req.trace.averageMode = 3;
        req.trace.averageAlpha = 0.5;
        SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(endpoint.configure(req, source));
        qint64 at = 1'000'000'000;
        for (int index = 0; index < 40; ++index, at += 40'000'000) {
            DaemonSpectrumFrame input = frame(source, at);
            input.binsLinear.fill(1.0e-12f);
            QVERIFY(endpoint.consume(input).has_value());
        }

        SpectrumEndpointRequest slower = req;
        slower.targetFps = 15;
        slower.framesPerLine = 1;
        slower.trace.averageAlpha = 0.9;
        ++source.contextGeneration;
        QVERIFY(endpoint.configure(slower, source));
        QCOMPARE(endpoint.context().targetFps, 15);
        QCOMPARE(endpoint.context().framesPerLine, 1);
        DaemonSpectrumFrame louder = frame(source, at);
        louder.binsLinear.fill(1.0e-10f);
        const std::optional<DisplayCodecFrame> output = endpoint.consume(louder);
        QVERIFY(output.has_value());
        // Recursive log averaging: 0.9 * -120 + 0.1 * -100.
        QVERIFY2(std::abs(output->traceDbm[32] - (-118.0f)) < 0.01f,
                 qPrintable(QString::number(output->traceDbm[32])));
    }

    void aNewPictureStillRestartsTheAveragedPlanes_data()
    {
        QTest::addColumn<QString>("change");
        // R-DISP-RETUNE: the "center" row that stood here pinned that a moved
        // crop restarts the averaging. A moved crop is now a moved picture
        // and keeps it (aMovedViewCenterKeepsTheAveragedEnergy). Every row
        // below is still a new picture, alone or together with a move.
        QTest::newRow("span") << QStringLiteral("span");
        QTest::newRow("span and center") << QStringLiteral("spanAndCenter");
        QTest::newRow("pixel count") << QStringLiteral("pixels");
        QTest::newRow("averaging mode") << QStringLiteral("averageMode");
        QTest::newRow("detector") << QStringLiteral("detector");
        QTest::newRow("sample rate") << QStringLiteral("sampleRate");
        QTest::newRow("FFT size") << QStringLiteral("fftSize");
        QTest::newRow("source generation") << QStringLiteral("sourceGeneration");
        QTest::newRow("restarted source at a moved center")
            << QStringLiteral("untoldGenerationMoved");
        QTest::newRow("moved source of another generation")
            << QStringLiteral("otherGenerationMoved");
        QTest::newRow("after reset") << QStringLiteral("reset");
        // R-DISP-RETUNE: average mode -1 holds each pixel's maximum and
        // never lets it go, so a maximum carried through a move would stay
        // on the screen at a frequency it never came from.
        QTest::newRow("held maximum at a moved view center")
            << QStringLiteral("heldMaximumMovedView");
        QTest::newRow("held maximum at a moved stream center")
            << QStringLiteral("heldMaximumMovedStream");
    }

    // R-DISP-HOLD: whatever makes the picture a different one restarts the
    // averaging exactly as a first configure does.
    void aNewPictureStillRestartsTheAveragedPlanes()
    {
        QFETCH(QString, change);
        SpectrumEndpoint endpoint;
        SpectrumEndpointRequest req = request();
        req.trace.averageMode = 3;
        req.trace.averageAlpha = 0.5;
        req.waterfall.averageMode = 1;
        req.waterfall.averageAlpha = 0.5;
        const bool heldMaximum = change.startsWith(QLatin1String("heldMaximum"));
        // The first frame of the next picture. For a held maximum it is the
        // weaker one, so a maximum kept from before would show in it.
        float nextLevel = 1.0e-10f;
        if (heldMaximum) {
            req.trace.averageMode = -1;
            req.waterfall.averageMode = -1;
            nextLevel = 1.0e-14f;
        }
        SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(endpoint.configure(req, source));
        qint64 at = 1'000'000'000;
        for (int index = 0; index < 40; ++index, at += 40'000'000) {
            DaemonSpectrumFrame input = frame(source, at);
            input.binsLinear.fill(1.0e-12f);
            QVERIFY(endpoint.consume(input).has_value());
        }

        SpectrumEndpointRequest next = req;
        ++source.contextGeneration;
        if (change == QLatin1String("heldMaximumMovedView")) {
            next.centreHz += 12'000.0;
        } else if (change == QLatin1String("heldMaximumMovedStream")) {
            // A relabeled source, told to the endpoint: for any other
            // averaging this is a moved picture.
            endpoint.awaitMovedSource(source.sourceGeneration,
                                      source.sourceGeneration + 1);
            ++source.sourceGeneration;
            source.centreHz += 12'000.0;
            next.centreHz += 12'000.0;
        } else if (change == QLatin1String("span")) {
            next.spanHz = 48'000.0;
        } else if (change == QLatin1String("spanAndCenter")) {
            next.spanHz = 48'000.0;
            next.centreHz += 12'000.0;
        } else if (change == QLatin1String("sampleRate")) {
            source.sampleRateHz = 384'000.0;
        } else if (change == QLatin1String("fftSize")) {
            source.fftBins = 2048;
        } else if (change == QLatin1String("untoldGenerationMoved")) {
            // The source restarted (another window, say) and came back at
            // another center. Nobody told the endpoint that the new
            // generation continues the old one, so it does not.
            ++source.sourceGeneration;
            source.centreHz += 100.0;
            endpoint.awaitContext();
        } else if (change == QLatin1String("otherGenerationMoved")) {
            // A relabel of some other generation is not this endpoint's.
            endpoint.awaitMovedSource(source.sourceGeneration + 5,
                                      source.sourceGeneration + 6);
            source.sourceGeneration += 6;
            source.centreHz += 100.0;
        } else if (change == QLatin1String("pixels")) {
            next.pixels = 48;
        } else if (change == QLatin1String("averageMode")) {
            next.trace.averageMode = 1;
            next.waterfall.averageMode = 3;
        } else if (change == QLatin1String("detector")) {
            next.trace.detector = SpectrumDetectorMode::Average;
            next.waterfall.detector = SpectrumDetectorMode::Peak;
        } else if (change == QLatin1String("sourceGeneration")) {
            ++source.sourceGeneration;
        } else {
            endpoint.reset();
        }
        QVERIFY(endpoint.configure(next, source));
        DaemonSpectrumFrame input = frame(source, at);
        input.binsLinear.fill(nextLevel);
        const std::optional<DisplayCodecFrame> restarted = endpoint.consume(input);
        QVERIFY(restarted.has_value());

        SpectrumEndpoint fresh;
        QVERIFY(fresh.configure(next, source));
        const std::optional<DisplayCodecFrame> expected = fresh.consume(input);
        QVERIFY(expected.has_value());
        QCOMPARE(restarted->traceDbm, expected->traceDbm);
        QCOMPARE(restarted->waterfallDbm, expected->waterfallDbm);
        QCOMPARE(endpoint.picture(), SpectrumPictureKind::New);
        QVERIFY(!endpoint.tracePlaneKept());
        QVERIFY(!endpoint.waterfallPlaneKept());
    }

    // R-DISP-RETUNE: the stream center moved under the same view. The source
    // was relabeled, not restarted, and the endpoint was told so. Its
    // converged trace and waterfall carry on: the first frame of the moved
    // picture is within one codec step of the frame before the move. A
    // restarted average reads 20 dB low on that frame (see
    // logAveragingDoesNotRestartAtZeroDbAfterRetune).
    void aMovedStreamCenterKeepsTheConvergedTrace()
    {
        SpectrumEndpoint endpoint;
        SpectrumEndpointRequest req = request();
        req.trace.averageMode = 3;
        req.trace.averageAlpha = 0.5;
        req.waterfall.averageMode = 3;
        req.waterfall.averageAlpha = 0.5;
        SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(endpoint.configure(req, source));
        QCOMPARE(endpoint.picture(), SpectrumPictureKind::New);
        qint64 at = 1'000'000'000;
        std::optional<DisplayCodecFrame> output;
        for (int index = 0; index < 40; ++index, at += 40'000'000) {
            DaemonSpectrumFrame input = frame(source, at);
            input.binsLinear.fill(1.0e-12f);
            output = endpoint.consume(input);
            QVERIFY(output.has_value());
        }
        const QVector<float> convergedTrace = output->traceDbm;
        const QVector<float> convergedWaterfall = output->waterfallDbm;
        QVERIFY(allNear(convergedTrace, -120.0f));
        QVERIFY(allNear(convergedWaterfall, -120.0f));

        const SpectrumEndpointSourceContext before = source;
        endpoint.awaitMovedSource(source.sourceGeneration, source.sourceGeneration + 1);
        QVERIFY(!endpoint.configured());
        ++source.sourceGeneration;
        ++source.contextGeneration;
        source.centreHz += 100.0;
        QVERIFY(endpoint.configure(req, source));
        QCOMPARE(endpoint.tracePicture(), SpectrumPictureKind::Moved);
        QCOMPARE(endpoint.waterfallPicture(), SpectrumPictureKind::Moved);
        QCOMPARE(endpoint.picture(), SpectrumPictureKind::Moved);
        // "Kept" still means the same picture, which this is not.
        QVERIFY(!endpoint.tracePlaneKept());
        QVERIFY(!endpoint.waterfallPlaneKept());
        QCOMPARE(endpoint.context().sourceGeneration, source.sourceGeneration);
        QCOMPARE(endpoint.context().codec.contextGeneration, source.contextGeneration);

        // A frame from before the move is still refused.
        DaemonSpectrumFrame stale = frame(before, at);
        stale.binsLinear.fill(1.0e-12f);
        QVERIFY(!endpoint.consume(stale).has_value());

        DaemonSpectrumFrame input = frame(source, at);
        input.binsLinear.fill(1.0e-12f);
        output = endpoint.consume(input);
        QVERIFY(output.has_value());
        // The context is new: its sequence and its waterfall pacing restart.
        QCOMPARE(output->encoderSequence, quint32(0));
        QVERIFY(output->waterfallAdvance);
        const float step = (req.maxDbm - req.minDbm) / 255.0f;
        QCOMPARE(output->traceDbm.size(), convergedTrace.size());
        for (int index = 0; index < convergedTrace.size(); ++index) {
            QVERIFY2(std::abs(output->traceDbm.at(index) - convergedTrace.at(index)) <= step,
                     qPrintable(QStringLiteral("trace pixel %1 restarted at %2 dBm")
                                    .arg(index).arg(output->traceDbm.at(index))));
            QVERIFY2(std::abs(output->waterfallDbm.at(index) - convergedWaterfall.at(index))
                         <= step,
                     qPrintable(QStringLiteral("waterfall pixel %1 restarted at %2 dBm")
                                    .arg(index).arg(output->waterfallDbm.at(index))));
        }

        // A second move straight after the first carries on the same way.
        endpoint.awaitMovedSource(source.sourceGeneration, source.sourceGeneration + 1);
        ++source.sourceGeneration;
        ++source.contextGeneration;
        source.centreHz += 100.0;
        QVERIFY(endpoint.configure(req, source));
        QCOMPARE(endpoint.picture(), SpectrumPictureKind::Moved);
        at += 40'000'000;
        DaemonSpectrumFrame louder = frame(source, at);
        louder.binsLinear.fill(1.0e-10f);
        output = endpoint.consume(louder);
        QVERIFY(output.has_value());
        // Recursive log averaging went on from the state: 0.5 * -120 + 0.5 * -100.
        QVERIFY2(std::abs(output->traceDbm[32] - (-110.0f)) < 0.01f,
                 qPrintable(QString::number(output->traceDbm[32])));
    }

    // R-DISP-RETUNE: the view center moved over the same source (a pan
    // dragged, or a re-ask after a tune). The averaged energy stays where it
    // is on the screen and goes on averaging, as on the local display.
    void aMovedViewCenterKeepsTheAveragedEnergy()
    {
        SpectrumEndpoint endpoint;
        SpectrumEndpointRequest req = request();
        req.trace.averageMode = 3;
        req.trace.averageAlpha = 0.5;
        req.waterfall.averageMode = 1;
        req.waterfall.averageAlpha = 0.5;
        SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(endpoint.configure(req, source));
        qint64 at = 1'000'000'000;
        for (int index = 0; index < 40; ++index, at += 40'000'000) {
            DaemonSpectrumFrame input = frame(source, at);
            input.binsLinear.fill(1.0e-12f);
            QVERIFY(endpoint.consume(input).has_value());
        }

        SpectrumEndpointRequest next = req;
        next.centreHz += 12'000.0;
        ++source.contextGeneration;
        endpoint.awaitContext();
        QVERIFY(endpoint.configure(next, source));
        QCOMPARE(endpoint.tracePicture(), SpectrumPictureKind::Moved);
        QCOMPARE(endpoint.waterfallPicture(), SpectrumPictureKind::Moved);
        QVERIFY(!endpoint.tracePlaneKept());
        // The plane took the new crop.
        QVERIFY(std::abs(endpoint.context().exactCentreHz - next.centreHz) < 200.0);

        DaemonSpectrumFrame louder = frame(source, at);
        louder.binsLinear.fill(1.0e-10f);
        const std::optional<DisplayCodecFrame> output = endpoint.consume(louder);
        QVERIFY(output.has_value());
        // Trace, recursive log: 0.5 * -120 + 0.5 * -100.
        QVERIFY2(std::abs(output->traceDbm[32] - (-110.0f)) < 0.01f,
                 qPrintable(QString::number(output->traceDbm[32])));
        // Waterfall, recursive linear power: half way from 1e-12 to 1e-10.
        const float expectedWaterfall =
            10.0f * std::log10(0.5f * 1.0e-12f + 0.5f * 1.0e-10f);
        QVERIFY2(std::abs(output->waterfallDbm[32] - expectedWaterfall) < 0.01f,
                 qPrintable(QString::number(output->waterfallDbm[32])));

        // A restart reads otherwise, so the check above can fail.
        SpectrumEndpoint fresh;
        QVERIFY(fresh.configure(next, source));
        const std::optional<DisplayCodecFrame> restarted = fresh.consume(louder);
        QVERIFY(restarted.has_value());
        QVERIFY(std::abs(restarted->traceDbm[32] - output->traceDbm[32]) > 1.0f);

        // Stream and view moved together, as when a pan follows the tune.
        endpoint.awaitMovedSource(source.sourceGeneration, source.sourceGeneration + 1);
        ++source.sourceGeneration;
        ++source.contextGeneration;
        source.centreHz += 5'000.0;
        next.centreHz += 5'000.0;
        QVERIFY(endpoint.configure(next, source));
        QCOMPARE(endpoint.picture(), SpectrumPictureKind::Moved);
        at += 40'000'000;
        DaemonSpectrumFrame same = frame(source, at);
        same.binsLinear.fill(1.0e-10f);
        const std::optional<DisplayCodecFrame> followed = endpoint.consume(same);
        QVERIFY(followed.has_value());
        // 0.5 * -110 + 0.5 * -100.
        QVERIFY2(std::abs(followed->traceDbm[32] - (-105.0f)) < 0.01f,
                 qPrintable(QString::number(followed->traceDbm[32])));
    }

    // R-DISP-RETUNE: one plane can be a moved picture while the other is a
    // new one, and the endpoint's picture is then new.
    void aMovedPlaneAndANewPlaneMakeANewPicture()
    {
        SpectrumEndpoint endpoint;
        SpectrumEndpointRequest req = request();
        req.trace.averageMode = 3;
        req.trace.averageAlpha = 0.5;
        req.waterfall.averageMode = 3;
        req.waterfall.averageAlpha = 0.5;
        SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(endpoint.configure(req, source));
        qint64 at = 1'000'000'000;
        for (int index = 0; index < 40; ++index, at += 40'000'000) {
            DaemonSpectrumFrame input = frame(source, at);
            input.binsLinear.fill(1.0e-12f);
            QVERIFY(endpoint.consume(input).has_value());
        }
        SpectrumEndpointRequest next = req;
        next.centreHz += 12'000.0;
        next.waterfall.averageMode = 1;
        ++source.contextGeneration;
        QVERIFY(endpoint.configure(next, source));
        QCOMPARE(endpoint.tracePicture(), SpectrumPictureKind::Moved);
        QCOMPARE(endpoint.waterfallPicture(), SpectrumPictureKind::New);
        QCOMPARE(endpoint.picture(), SpectrumPictureKind::New);
        DaemonSpectrumFrame input = frame(source, at);
        input.binsLinear.fill(1.0e-10f);
        const std::optional<DisplayCodecFrame> output = endpoint.consume(input);
        QVERIFY(output.has_value());
        SpectrumEndpoint fresh;
        QVERIFY(fresh.configure(next, source));
        const std::optional<DisplayCodecFrame> expected = fresh.consume(input);
        QVERIFY(expected.has_value());
        QVERIFY2(std::abs(output->traceDbm[32] - (-110.0f)) < 0.01f,
                 qPrintable(QString::number(output->traceDbm[32])));
        QCOMPARE(output->waterfallDbm, expected->waterfallDbm);
    }

    // R-DISP-RETUNE: the extended (ADC wings) planes follow the same rule.
    void anExtendedMovedViewKeepsTheAveragedPlanes()
    {
        SpectrumEndpointSourceContext source = extendedSourceContext();
        SpectrumEndpointRequest req = extendedRequest();
        req.centreHz = 1'500'000.0;
        req.spanHz = 500'000.0;
        req.trace.averageMode = 1;
        req.trace.averageAlpha = 0.5;
        SpectrumEndpoint endpoint;
        QVERIFY(endpoint.configure(req, source));
        qint64 at = 1'000'000'000;
        std::optional<DisplayCodecFrame> output;
        for (int index = 0; index < 40; ++index, at += 40'000'000) {
            output = endpoint.consume(frame(source, at), 0.0,
                                      widebandAtDisplayLevel(source, -40.0f));
            QVERIFY(output.has_value());
        }
        const float converged = output->traceDbm[50];

        SpectrumEndpointRequest moved = req;
        moved.centreHz += 10'000.0;
        endpoint.awaitMovedSource(source.sourceGeneration, source.sourceGeneration + 1);
        ++source.sourceGeneration;
        ++source.contextGeneration;
        source.centreHz += 10'000.0;
        QVERIFY(endpoint.configure(moved, source));
        QCOMPARE(endpoint.tracePicture(), SpectrumPictureKind::Moved);
        output = endpoint.consume(frame(source, at), 0.0,
                                  widebandAtDisplayLevel(source, -100.0f));
        QVERIFY(output.has_value());
        // Kept: half way in linear power from the -40 dBm state towards
        // -100 dBm is 3 dB down. Restarted, it would read -100 dBm.
        QVERIFY2(std::abs(output->traceDbm[50] - (converged - 3.01f)) < 0.05f,
                 qPrintable(QStringLiteral("%1 after %2").arg(output->traceDbm[50])
                                .arg(converged)));
    }

    // R-DISP-HOLD: the two planes are judged apart. A changed waterfall
    // averaging mode restarts the waterfall plane and leaves the trace plane.
    void aChangedPlaneRestartsAloneAndTheOtherPlaneIsKept()
    {
        SpectrumEndpoint endpoint;
        SpectrumEndpointRequest req = request();
        req.trace.averageMode = 3;
        req.trace.averageAlpha = 0.5;
        req.waterfall.averageMode = 3;
        req.waterfall.averageAlpha = 0.5;
        SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(endpoint.configure(req, source));
        qint64 at = 1'000'000'000;
        for (int index = 0; index < 40; ++index, at += 40'000'000) {
            DaemonSpectrumFrame input = frame(source, at);
            input.binsLinear.fill(1.0e-12f);
            QVERIFY(endpoint.consume(input).has_value());
        }
        SpectrumEndpointRequest next = req;
        next.waterfall.averageMode = 1;
        ++source.contextGeneration;
        QVERIFY(endpoint.configure(next, source));
        DaemonSpectrumFrame input = frame(source, at);
        input.binsLinear.fill(1.0e-10f);
        const std::optional<DisplayCodecFrame> output = endpoint.consume(input);
        QVERIFY(output.has_value());

        SpectrumEndpoint fresh;
        QVERIFY(fresh.configure(next, source));
        const std::optional<DisplayCodecFrame> expected = fresh.consume(input);
        QVERIFY(expected.has_value());
        // Kept trace: 0.5 * -120 + 0.5 * -100. Restarted waterfall: as fresh.
        QVERIFY2(std::abs(output->traceDbm[32] - (-110.0f)) < 0.01f,
                 qPrintable(QString::number(output->traceDbm[32])));
        QCOMPARE(output->waterfallDbm, expected->waterfallDbm);
    }

    // R-DISP-HOLD: the extended (ADC wings) planes follow the same rule.
    void anExtendedDbmRangeChangeKeepsTheAveragedPlanes()
    {
        SpectrumEndpointSourceContext source = extendedSourceContext();
        SpectrumEndpointRequest req = extendedRequest();
        req.centreHz = 1'500'000.0;
        req.spanHz = 500'000.0;
        req.trace.averageMode = 1;
        req.trace.averageAlpha = 0.5;
        SpectrumEndpoint endpoint;
        QVERIFY(endpoint.configure(req, source));
        qint64 at = 1'000'000'000;
        std::optional<DisplayCodecFrame> output;
        for (int index = 0; index < 40; ++index, at += 40'000'000) {
            output = endpoint.consume(frame(source, at), 0.0,
                                      widebandAtDisplayLevel(source, -40.0f));
            QVERIFY(output.has_value());
        }
        const float converged = output->traceDbm[50];

        SpectrumEndpointRequest moved = req;
        moved.minDbm = -150.0f;
        moved.maxDbm = -20.0f;
        ++source.contextGeneration;
        QVERIFY(endpoint.configure(moved, source));
        output = endpoint.consume(frame(source, at), 0.0,
                                  widebandAtDisplayLevel(source, -100.0f));
        QVERIFY(output.has_value());
        // Kept: half way in linear power from the -40 dBm state towards
        // -100 dBm is 3 dB down. Restarted, it would read -100 dBm.
        QVERIFY2(std::abs(output->traceDbm[50] - (converged - 3.01f)) < 0.05f,
                 qPrintable(QStringLiteral("%1 after %2").arg(output->traceDbm[50])
                                .arg(converged)));

        // A changed ADC source is a new picture.
        ++source.contextGeneration;
        ++source.wideband.sourceGeneration;
        QVERIFY(endpoint.configure(moved, source));
        const WidebandSpectrumFrame quiet = widebandAtDisplayLevel(source, -100.0f);
        const auto restarted = endpoint.consume(frame(source, at), 0.0, quiet);
        SpectrumEndpoint fresh;
        QVERIFY(fresh.configure(moved, source));
        const auto expected = fresh.consume(frame(source, at), 0.0, quiet);
        QVERIFY(restarted.has_value());
        QVERIFY(expected.has_value());
        QCOMPARE(restarted->traceDbm, expected->traceDbm);
    }

    // R-DISP-RETUNE (third wave): the endpoint names the picture its frames
    // continue: the context generation at which the planes now running were
    // started. It stays through a context of the same picture and of that
    // picture moved, and takes the new generation whenever a plane starts
    // again.
    void thePictureIsNamedByTheGenerationItsPlanesStartedAt()
    {
        SpectrumEndpoint endpoint;
        SpectrumEndpointRequest req = request();
        req.trace.averageMode = 3;
        req.trace.averageAlpha = 0.5;
        req.waterfall.averageMode = 3;
        req.waterfall.averageAlpha = 0.5;
        SpectrumEndpointSourceContext source = sourceContext();
        // A first configure starts the planes.
        QVERIFY(endpoint.configure(req, source));
        QCOMPARE(endpoint.picture(), SpectrumPictureKind::New);
        QCOMPARE(endpoint.pictureGeneration(), quint32(4));

        // Same: a new dBm range.
        req.minDbm = -150.0f;
        ++source.contextGeneration;
        endpoint.awaitContext();
        QVERIFY(endpoint.configure(req, source));
        QCOMPARE(endpoint.picture(), SpectrumPictureKind::Same);
        QCOMPARE(endpoint.context().codec.contextGeneration, quint32(5));
        QCOMPARE(endpoint.pictureGeneration(), quint32(4));

        // Moved: the view center.
        req.centreHz += 12'000.0;
        ++source.contextGeneration;
        endpoint.awaitContext();
        QVERIFY(endpoint.configure(req, source));
        QCOMPARE(endpoint.picture(), SpectrumPictureKind::Moved);
        QCOMPARE(endpoint.pictureGeneration(), quint32(4));

        // Moved: the source, relabeled through a retune.
        endpoint.awaitMovedSource(source.sourceGeneration, source.sourceGeneration + 1);
        ++source.sourceGeneration;
        ++source.contextGeneration;
        source.centreHz += 100.0;
        QVERIFY(endpoint.configure(req, source));
        QCOMPARE(endpoint.picture(), SpectrumPictureKind::Moved);
        QCOMPARE(endpoint.context().codec.contextGeneration, quint32(7));
        QCOMPARE(endpoint.pictureGeneration(), quint32(4));

        // A configure that is refused changes nothing.
        SpectrumEndpointRequest bad = req;
        bad.trace.averageMode = 4;
        ++source.contextGeneration;
        QVERIFY(!endpoint.configure(bad, source));
        QCOMPARE(endpoint.pictureGeneration(), quint32(4));

        // New: one plane started again (another averaging mode), the other
        // was kept. The picture is new as soon as either plane is.
        req.waterfall.averageMode = 1;
        ++source.contextGeneration;
        QVERIFY(endpoint.configure(req, source));
        QCOMPARE(endpoint.tracePicture(), SpectrumPictureKind::Same);
        QCOMPARE(endpoint.waterfallPicture(), SpectrumPictureKind::New);
        QCOMPARE(endpoint.pictureGeneration(), quint32(9));

        // New: the source restarted (a generation nobody said continues the
        // one held), at the same center, size and rate.
        endpoint.awaitContext();
        ++source.sourceGeneration;
        ++source.contextGeneration;
        QVERIFY(endpoint.configure(req, source));
        QCOMPARE(endpoint.picture(), SpectrumPictureKind::New);
        QCOMPARE(endpoint.pictureGeneration(), quint32(10));

        // New: the planes were dropped.
        endpoint.reset();
        ++source.contextGeneration;
        QVERIFY(endpoint.configure(req, source));
        QCOMPARE(endpoint.pictureGeneration(), quint32(11));
    }

    // Average mode -1 holds a maximum per pixel, and a pixel shows another
    // frequency after a move: that plane starts again, so the picture is new.
    void aHeldMaximumPlaneMovedStartsANewPicture()
    {
        SpectrumEndpoint endpoint;
        SpectrumEndpointRequest req = request();
        req.trace.averageMode = -1;
        req.waterfall.averageMode = 3;
        req.waterfall.averageAlpha = 0.5;
        SpectrumEndpointSourceContext source = sourceContext();
        QVERIFY(endpoint.configure(req, source));
        QCOMPARE(endpoint.pictureGeneration(), quint32(4));
        // Its dBm range alone: the same picture.
        req.maxDbm = -10.0f;
        ++source.contextGeneration;
        QVERIFY(endpoint.configure(req, source));
        QCOMPARE(endpoint.picture(), SpectrumPictureKind::Same);
        QCOMPARE(endpoint.pictureGeneration(), quint32(4));
        // Moved.
        req.centreHz += 12'000.0;
        ++source.contextGeneration;
        QVERIFY(endpoint.configure(req, source));
        QCOMPARE(endpoint.tracePicture(), SpectrumPictureKind::New);
        QCOMPARE(endpoint.waterfallPicture(), SpectrumPictureKind::Moved);
        QCOMPARE(endpoint.pictureGeneration(), quint32(6));
    }
};

QTEST_MAIN(TstSpectrumEndpoint)
#include "tst_spectrum_endpoint.moc"
