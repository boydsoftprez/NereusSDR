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

#include "core/WidebandFftEngine.h"
#include "core/session/media/SpectrumEndpoint.h"
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
        QCOMPARE(context.codec.traceSamples, static_cast<quint16>(coveredBins));
        QCOMPARE(context.codec.waterfallSamples, static_cast<quint16>(coveredBins));
        const std::optional<DisplayCodecFrame> output = endpoint.consume(frame(source, 1'000'000'000));
        QVERIFY(output.has_value());
        QCOMPARE(output->traceDbm.size(), coveredBins);
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
};

QTEST_MAIN(TstSpectrumEndpoint)
#include "tst_spectrum_endpoint.moc"
