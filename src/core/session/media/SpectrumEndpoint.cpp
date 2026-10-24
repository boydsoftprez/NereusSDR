// =================================================================
// src/core/session/media/SpectrumEndpoint.cpp  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote daemon R3 Task 2.
//
// Modification history (NereusSDR):
//   2026-10-23: A frame carries the row its noise floor measures
//               (measurementDbm) while the trace's average warms up or a
//               wing holds no data. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-21: Even row gaps: a frame fills its slot when it is nearer the
//               slot than the source's next frame (earlyToleranceNs), not
//               only within a twentieth of a period. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-15: The wide row is cropped and peak-reduced as powers and only
//               the values sent are converted to dBm (wideRowDbm), in place
//               of converting every FFT bin first. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-10-11: R-DISP-RETUNE (third wave): configure records the context
//               generation at which the planes now running were started
//               (pictureGeneration). J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//   2026-10-11: R-DISP-RETUNE: configure tells a picture moved in
//               frequency from a new one and keeps its averaged planes;
//               awaitMovedSource. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
//
// =================================================================

#include "core/session/media/SpectrumEndpoint.h"
#include "core/FFTEngine.h"

#include "core/session/media/DssWideRow.h"
#include "core/WidebandFftEngine.h"
#include "core/spectrum/ExtendedSpectrumReducer.h"
#include "core/spectrum/SpectrumReducer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>

namespace NereusSDR {
namespace {

bool finite(double value)
{
    return std::isfinite(value);
}

// A span that is a whole number of bins computes a hair to either side of
// it. This much of a bin, far under any step a control makes, reads such a
// span as the whole number it is.
constexpr double kWholeBinSlack = 1.0e-6;

bool validPlane(const SpectrumPlaneRequest& plane)
{
    return static_cast<int>(plane.detector) >= static_cast<int>(SpectrumDetectorMode::Peak)
        && static_cast<int>(plane.detector) < static_cast<int>(SpectrumDetectorMode::Count)
        && plane.averageMode >= -1 && plane.averageMode <= 3
        && finite(plane.averageAlpha) && plane.averageAlpha >= 0.0 && plane.averageAlpha <= 1.0;
}

ReducerConfig reducerConfig(const SpectrumEndpointRequest& request,
                            const SpectrumEndpointSourceContext& source,
                            const SpectrumPlaneRequest& plane,
                            int pixels)
{
    ReducerConfig config;
    config.pixels = pixels;
    config.centreHz = request.centreHz;
    config.spanHz = request.spanHz;
    config.streamCentreHz = source.centreHz;
    config.sampleRateHz = source.sampleRateHz;
    config.detector = plane.detector;
    config.averageMode = plane.averageMode;
    config.averageAlpha = plane.averageAlpha;
    config.averageFrames = plane.averageFrames;
    return config;
}

bool overlapsSource(const SpectrumEndpointRequest& request,
                    const SpectrumEndpointSourceContext& source)
{
    const double sourceLow = source.centreHz - source.sampleRateHz * 0.5;
    const double sourceHigh = source.centreHz + source.sampleRateHz * 0.5;
    const double requestLow = request.centreHz - request.spanHz * 0.5;
    const double requestHigh = request.centreHz + request.spanHz * 0.5;
    return requestHigh > sourceLow && requestLow < sourceHigh;
}

// R-DISP-HOLD: two reducer configurations draw the same picture when only
// the averaging constant differs. The constant follows the frame rate and
// applies to the next frame; it is not part of the averaged state.
bool samePlanePicture(const ReducerConfig& held, const ReducerConfig& next)
{
    return held.pixels == next.pixels && held.centreHz == next.centreHz
        && held.spanHz == next.spanHz && held.streamCentreHz == next.streamCentreHz
        && held.sampleRateHz == next.sampleRateHz && held.detector == next.detector
        && held.averageMode == next.averageMode;
}

// R-DISP-RETUNE: the same picture in another place. Everything that shapes
// the averaged state is equal (how many pixels, how wide, how each pixel is
// detected and averaged); only where the stream or the crop is centered
// differs. Each pixel then goes on averaging what arrives at its place on
// the screen, which is what the local display does through a retune.
//
// Average mode -1 is the exception. It holds each pixel's maximum and never
// lets it go (the "peak-hold" case in SpectrumAvenger.cpp), so a maximum carried through a move
// would stay for good at a place on the screen it never came from. A plane
// in that mode starts again on any move.
bool movedPlanePicture(const ReducerConfig& held, const ReducerConfig& next)
{
    constexpr int kHeldMaximumAverageMode = -1;
    if (next.averageMode == kHeldMaximumAverageMode) {
        return false;
    }
    return held.pixels == next.pixels && held.spanHz == next.spanHz
        && held.sampleRateHz == next.sampleRateHz && held.detector == next.detector
        && held.averageMode == next.averageMode
        && (held.centreHz != next.centreHz || held.streamCentreHz != next.streamCentreHz);
}

SpectrumPictureKind planePicture(bool sameSource, const ReducerConfig& held,
                                 const ReducerConfig& next)
{
    if (!sameSource) {
        return SpectrumPictureKind::New;
    }
    if (samePlanePicture(held, next)) {
        return SpectrumPictureKind::Same;
    }
    return movedPlanePicture(held, next) ? SpectrumPictureKind::Moved
                                         : SpectrumPictureKind::New;
}

bool finiteNonnegativeBins(const QVector<float>& bins)
{
    return std::all_of(bins.cbegin(), bins.cend(), [](float value) {
        return std::isfinite(value) && value >= 0.0f;
    });
}

bool finiteRow(const QVector<float>& row)
{
    return std::all_of(row.cbegin(), row.cend(), [](float value) {
        return std::isfinite(value);
    });
}

// The wide row in dBm, or an empty row for a frame that does not convert.
//
// Cropped and peak-reduced as powers, then converted. The conversion rises
// with the power, so the largest power of a group gives the largest dBm of
// that group: the row is the one a conversion of every bin gives
// (tst_spectrum_endpoint holds the two equal). Converting every FFT bin
// first cost a log10 a bin where at most maximumSamples are sent. On the
// Rock 5C on 2026-10-10, eight pans' worth from two windows, that was a
// fifth of the Core's main thread, which was using 91% of its core.
QVector<float> wideRowDbm(const DaemonSpectrumFrame& frame, double offsetDb,
                          const DssWideRowRequest& request, int maximumSamples)
{
    const double scale = std::pow(10.0, offsetDb / 10.0);
    if (!finite(scale) || scale <= 0.0) {
        return {};
    }
    const auto dbm = [scale](float linear) {
        return static_cast<float>(10.0 * std::log10(scale * linear + 1.0e-60));
    };
    // A frame with a bin that does not convert is refused whole, wherever
    // the bin sits. Every bin converts if the largest does. consume has
    // already refused a frame with a bin that is not finite or is negative
    // (finiteNonnegativeBins), so only the conversion is checked here.
    const float largest = frame.binsLinear.isEmpty()
        ? 0.0f
        : *std::max_element(frame.binsLinear.cbegin(), frame.binsLinear.cend());
    if (!std::isfinite(dbm(largest))) {
        return {};
    }
    const DssWideRow wide = cropDssWideRow(frame.binsLinear, request);
    QVector<float> row = peakReduceDssWideRow(wide.binsDbm, maximumSamples);
    for (float& value : row) {
        value = dbm(value);
    }
    return row;
}

bool matchesWidebandSource(const WidebandSpectrumFrame& frame,
                           const WidebandDisplayContext& context)
{
    if (frame.source.physicalAdcIndex != context.physicalAdcIndex
        || frame.source.sourceGeneration != context.sourceGeneration
        || frame.source.adcRateHz != context.adcRateHz
        || frame.producedAtNs < 0
        || frame.rawDbBins.size() != WidebandFftEngine::kOutputBins) {
        return false;
    }
    return finiteRow(frame.rawDbBins);
}

qint64 saturatingAdd(qint64 value, qint64 increment)
{
    if (value > std::numeric_limits<qint64>::max() - increment) {
        return std::numeric_limits<qint64>::max();
    }
    return value + increment;
}

} // namespace

bool SpectrumEndpoint::validRequest(const SpectrumEndpointRequest& request)
{
    return request.endpointId != 0 && request.source.streamIndex >= 0
        && finite(request.centreHz) && finite(request.spanHz) && request.spanHz > 0.0
        && finite(request.centreHz - request.spanHz * 0.5)
        && finite(request.centreHz + request.spanHz * 0.5)
        && request.pixels > 0 && request.targetFps >= 1 && request.targetFps <= 60
        && request.framesPerLine > 0 && request.framesPerLine <= kMaxFramesPerLine
        && validPlane(request.trace) && validPlane(request.waterfall)
        && std::isfinite(request.minDbm) && std::isfinite(request.maxDbm)
        && request.minDbm >= kMinDbmLimit && request.maxDbm <= kMaxDbmLimit
        && request.maxDbm > request.minDbm
        && finite(request.requestedWideSpanFactor)
        && finite(request.requestedWideSpanFactor * request.spanHz)
        && (request.requestedWideSpanFactor == 0.0 || request.requestedWideSpanFactor > 1.0);
}

int SpectrumEndpoint::grantedPixels(const SpectrumEndpointRequest& request, int fftBins,
                                    double sourceCentreHz, double sourceSampleRateHz,
                                    bool extendedView)
{
    if (request.pixels <= 0) {
        return 0;
    }
    // The extended path uses the full requested RF geometry, where an empty
    // DDC island is valid because the physical ADC owns every pixel.
    if (extendedView) {
        return std::min(request.pixels, kMaxPixels);
    }
    // A DDC-only crop cannot claim more independent samples than source bins.
    if (fftBins <= 0 || !finite(sourceCentreHz) || !finite(sourceSampleRateHz)
        || sourceSampleRateHz <= 0.0) {
        return 0;
    }
    SpectrumEndpointSourceContext source;
    source.fftBins = fftBins;
    source.centreHz = sourceCentreHz;
    source.sampleRateHz = sourceSampleRateHz;
    if (!overlapsSource(request, source)) {
        return 0;
    }
    const ReducerConfig geometry = reducerConfig(request, source, request.trace, 1);
    const std::pair<int, int> bins = SpectrumReducer::visibleBinRange(fftBins, geometry);
    const int count = bins.second - bins.first + 1;
    if (count <= 0) {
        return 0;
    }
    // R-DISP-RETUNE: the bins a crop touches depend on where it lies over
    // them. The reducer takes the floor at the low edge and the ceiling at
    // the high one, so a span of n bins touches ceil(n) + 1 of them with
    // its low edge on a bin edge and one more anywhere else. A grant that
    // followed that would change as a tuning step moved the crop a fraction
    // of a bin, and a changed sample count is a new picture: the display
    // would blank where it only moved. So a crop limited by its bins is
    // granted the count it has at every alignment, and the reducer lays the
    // bins it touches over that many samples. A crop that runs past the
    // source's edge has fewer bins still, and is granted those.
    const double spanBins = request.spanHz * fftBins / sourceSampleRateHz;
    const int everyAlignment = static_cast<int>(std::clamp(
        std::ceil(spanBins - kWholeBinSlack) + 1.0, 1.0, static_cast<double>(kMaxPixels)));
    return std::min({request.pixels, count, everyAlignment, kMaxPixels});
}

bool SpectrumEndpoint::validSourceContext(const SpectrumEndpointSourceContext& sourceContext)
{
    return sourceContext.source.streamIndex >= 0 && sourceContext.sourceGeneration != 0
        && sourceContext.fftBins > 0 && sourceContext.fftBins <= FFTEngine::maximumFftSize()
        && finite(sourceContext.centreHz)
        && finite(sourceContext.sampleRateHz) && sourceContext.sampleRateHz > 0.0
        && sourceContext.contextGeneration != 0
        && finite(sourceContext.centreHz - sourceContext.sampleRateHz * 0.5)
        && finite(sourceContext.centreHz + sourceContext.sampleRateHz * 0.5);
}

bool SpectrumEndpoint::configure(const SpectrumEndpointRequest& request,
                                 const SpectrumEndpointSourceContext& sourceContext)
{
    if (!validRequest(request) || !validSourceContext(sourceContext)
        || !(request.source == sourceContext.source)) {
        return false;
    }

    // Permission is distinct from activation. An available inactive ADC is
    // advertised with the normal DDC row until Core activates its demand;
    // only an active ADC changes the row's geometry and reducer ownership.
    if (!sourceContext.wideband.valid()
        || (sourceContext.wideband.active && !request.extendedView)) {
        return false;
    }
    const bool extendedView = sourceContext.wideband.active;

    // A DDC-only crop must overlap its source: a crop beside the DDC would
    // otherwise be accepted with zero span and never paint (R-R3-09).
    const bool hasOverlap = overlapsSource(request, sourceContext);
    if (!extendedView && !hasOverlap) {
        return false;
    }
    const ReducerConfig geometry = reducerConfig(request, sourceContext, request.trace, 1);
    const std::pair<int, int> bins = SpectrumReducer::visibleBinRange(sourceContext.fftBins, geometry);
    const int count = bins.second - bins.first + 1;
    const int pixels = grantedPixels(request, sourceContext.fftBins, sourceContext.centreHz,
                                     sourceContext.sampleRateHz, extendedView);
    if (pixels <= 0) {
        return false;
    }
    const ReducerConfig traceConfig = reducerConfig(request, sourceContext, request.trace, pixels);
    const double binHz = sourceContext.sampleRateHz / sourceContext.fftBins;
    SpectrumEndpointContext accepted;
    accepted.codec.endpointId = request.endpointId;
    accepted.codec.contextGeneration = sourceContext.contextGeneration;
    accepted.codec.minDbm = request.minDbm;
    accepted.codec.maxDbm = request.maxDbm;
    accepted.codec.traceSamples = static_cast<quint16>(pixels);
    accepted.codec.waterfallSamples = static_cast<quint16>(pixels);
    accepted.source = request.source;
    accepted.sourceGeneration = sourceContext.sourceGeneration;
    if (extendedView) {
        accepted.exactCentreHz = request.centreHz;
        accepted.exactSpanHz = request.spanHz;
        accepted.wideband = sourceContext.wideband;
    } else if (hasOverlap) {
        accepted.exactCentreHz = sourceContext.centreHz - sourceContext.sampleRateHz * 0.5
            + (bins.first + bins.second + 1) * binHz * 0.5;
        accepted.exactSpanHz = count * binHz;
    }
    if (!extendedView) {
        accepted.wideband = sourceContext.wideband;
        accepted.wideband.active = false;
        accepted.wideband.sourceGeneration = 0;
    }
    accepted.targetFps = request.targetFps;
    accepted.framesPerLine = request.framesPerLine;

    if (hasOverlap && request.requestedWideSpanFactor > 1.0) {
        const QVector<float> emptyBins(sourceContext.fftBins, -200.0f);
        const DssWideRow wide = cropDssWideRow(emptyBins, {
            request.centreHz, request.spanHz, sourceContext.centreHz,
            sourceContext.sampleRateHz, request.requestedWideSpanFactor});
        if (!wide.binsDbm.isEmpty()) {
            accepted.codec.wideSamples = static_cast<quint16>(
                std::min(static_cast<int>(wide.binsDbm.size()), kMaxWideSamples));
            accepted.wideCentreHz = wide.centreHz;
            accepted.wideSpanHz = wide.spanHz;
        }
    }

    // R-DISP-HOLD: a plane whose picture is unchanged keeps its reducer and
    // the averaged state in it. The state is linear power per output pixel,
    // so it depends on the source geometry, the crop, the pixel count, the
    // detector and the averaging mode, and on nothing else: not on the dBm
    // range, the frame rate, the frames per line or the averaging constant.
    // Each plane is judged apart. Everything else restarts as on a first
    // configure.
    //
    // R-DISP-RETUNE: the source center is no longer part of "same source".
    // A source relabeled through a retune is the same source at another
    // center (awaitMovedSource carried its generation over), and the center
    // is judged with the crop, per plane: equal is the same picture, moved
    // is a moved picture, and both keep the reducer. The generation is what
    // tells a continued source from a restarted one, so a source that
    // restarted with the same FFT size and sample rate (another window or
    // decimation) still starts a new picture.
    const bool sameSource = m_planesHeld && m_extendedView == extendedView
        && m_sourceContext.source == sourceContext.source
        && m_sourceContext.sourceGeneration == sourceContext.sourceGeneration
        && m_sourceContext.fftBins == sourceContext.fftBins
        && m_sourceContext.sampleRateHz == sourceContext.sampleRateHz
        && (!extendedView || !(m_sourceContext.wideband != sourceContext.wideband));
    const ReducerConfig waterfallConfig =
        reducerConfig(request, sourceContext, request.waterfall, pixels);
    std::unique_ptr<SpectrumReducer> trace;
    std::unique_ptr<SpectrumReducer> waterfall;
    std::unique_ptr<ExtendedSpectrumReducer> extendedTrace;
    std::unique_ptr<ExtendedSpectrumReducer> extendedWaterfall;
    SpectrumPictureKind tracePicture = SpectrumPictureKind::New;
    SpectrumPictureKind waterfallPicture = SpectrumPictureKind::New;
    if (extendedView) {
        if (m_extendedTraceReducer) {
            tracePicture = planePicture(sameSource, m_extendedTraceReducer->config(),
                                        traceConfig);
        }
        if (m_extendedWaterfallReducer) {
            waterfallPicture = planePicture(sameSource, m_extendedWaterfallReducer->config(),
                                            waterfallConfig);
        }
    } else {
        if (m_traceReducer) {
            tracePicture = planePicture(sameSource, m_traceReducer->config(), traceConfig);
        }
        if (m_waterfallReducer) {
            waterfallPicture = planePicture(sameSource, m_waterfallReducer->config(),
                                            waterfallConfig);
        }
    }
    const bool traceKept = tracePicture != SpectrumPictureKind::New;
    const bool waterfallKept = waterfallPicture != SpectrumPictureKind::New;
    // R-DISP-RETUNE: the picture these planes draw is named by the context
    // generation at which they were started. Each branch below that makes a
    // plane anew notes it here, so the name is the record of what was done
    // to the planes and not a second reading of the two configs.
    bool planeStarted = false;
    if (extendedView) {
        if (traceKept) {
            extendedTrace = std::move(m_extendedTraceReducer);
            extendedTrace->setConfig(traceConfig);
        } else {
            extendedTrace = std::make_unique<ExtendedSpectrumReducer>();
            extendedTrace->setConfig(traceConfig);
            extendedTrace->clearAveraging();
            planeStarted = true;
        }
        if (waterfallKept) {
            extendedWaterfall = std::move(m_extendedWaterfallReducer);
            extendedWaterfall->setConfig(waterfallConfig);
        } else {
            extendedWaterfall = std::make_unique<ExtendedSpectrumReducer>();
            extendedWaterfall->setConfig(waterfallConfig);
            extendedWaterfall->clearAveraging();
            planeStarted = true;
        }
    } else {
        if (traceKept) {
            trace = std::move(m_traceReducer);
            trace->setConfig(traceConfig);
        } else {
            trace = std::make_unique<SpectrumReducer>();
            trace->setConfig(traceConfig);
            trace->clearAveraging();
            planeStarted = true;
        }
        if (waterfallKept) {
            waterfall = std::move(m_waterfallReducer);
            waterfall->setConfig(waterfallConfig);
        } else {
            waterfall = std::make_unique<SpectrumReducer>();
            waterfall->setConfig(waterfallConfig);
            waterfall->clearAveraging();
            planeStarted = true;
        }
    }

    m_request = request;
    m_sourceContext = sourceContext;
    m_context = accepted;
    m_traceReducer = std::move(trace);
    m_waterfallReducer = std::move(waterfall);
    m_extendedTraceReducer = std::move(extendedTrace);
    m_extendedWaterfallReducer = std::move(extendedWaterfall);
    m_hasProducerTimestamp = false;
    m_lastProducerTimestampNs = 0;
    m_nextOutputDueNs = 0;
    m_emittedFrames = 0;
    m_nextEncoderSequence = 0;
    m_hasOverlap = hasOverlap;
    m_extendedView = extendedView;
    m_tracePicture = tracePicture;
    m_waterfallPicture = waterfallPicture;
    if (planeStarted) {
        m_pictureGeneration = accepted.codec.contextGeneration;
    }
    m_planesHeld = true;
    m_configured = true;
    return true;
}

std::optional<DisplayCodecFrame> SpectrumEndpoint::consume(
    const DaemonSpectrumFrame& frame,
    double stationOffsetDb,
    const std::optional<WidebandSpectrumFrame>& widebandFrame)
{
    if (!m_configured || !(frame.source == m_sourceContext.source)
        || frame.generation != m_sourceContext.sourceGeneration
        || frame.binsLinear.size() != m_sourceContext.fftBins
        || frame.centreHz != m_sourceContext.centreHz
        || frame.sampleRateHz != m_sourceContext.sampleRateHz
        || !finite(frame.windowEnb) || frame.windowEnb <= 0.0 || !finite(frame.dbmOffset)
        || !finite(stationOffsetDb) || !finite(frame.dbmOffset + stationOffsetDb)
        || frame.producedAtNs < 0 || (!m_extendedView && !m_hasOverlap)
        || !finiteNonnegativeBins(frame.binsLinear)) {
        return std::nullopt;
    }
    if (m_hasProducerTimestamp
        && frame.producedAtNs <= m_lastProducerTimestampNs) {
        return std::nullopt;
    }
    m_hasProducerTimestamp = true;
    m_lastProducerTimestampNs = frame.producedAtNs;

    const qint64 periodNs = 1'000'000'000LL / m_context.targetFps;
    // Permit early arrival against an advancing schedule: at least 5%, and
    // up to half the source's frame spacing (earlyToleranceNs). The
    // schedule itself advances by a full period, so a persistently fast
    // producer eventually spends its allowance and cannot exceed target FPS.
    const qint64 toleranceNs = earlyToleranceNs(periodNs, frame.sourceFrameGapNs);
    if (m_nextOutputDueNs != 0
        && frame.producedAtNs < m_nextOutputDueNs - toleranceNs) {
        return std::nullopt;
    }

    QVector<float> trace;
    QVector<float> waterfall;
    QVector<float> measurement;
    if (m_extendedView) {
        const QVector<float> emptyAdcRawDb;
        const QVector<float>& adcRawDb =
            widebandFrame && matchesWidebandSource(*widebandFrame, m_context.wideband)
            ? widebandFrame->rawDbBins
            : emptyAdcRawDb;
        if (!m_extendedTraceReducer->reduce(frame.binsLinear, frame.windowEnb, frame.dbmOffset,
                                            adcRawDb, m_context.wideband.adcRateHz,
                                            stationOffsetDb, m_request.minDbm, trace,
                                            &measurement)
            || !m_extendedWaterfallReducer->reduce(frame.binsLinear, frame.windowEnb,
                                                    frame.dbmOffset, adcRawDb,
                                                    m_context.wideband.adcRateHz,
                                                    stationOffsetDb, m_request.minDbm,
                                                    waterfall)) {
            return std::nullopt;
        }
    } else {
        m_traceReducer->reduce(frame.binsLinear, frame.windowEnb,
                               frame.dbmOffset + stationOffsetDb, trace, &measurement);
        m_waterfallReducer->reduce(frame.binsLinear, frame.windowEnb,
                                   frame.dbmOffset + stationOffsetDb, waterfall);
    }
    if (trace.size() != m_context.codec.traceSamples
        || waterfall.size() != m_context.codec.waterfallSamples
        || !finiteRow(trace) || !finiteRow(waterfall)) {
        return std::nullopt;
    }

    DisplayCodecFrame output;
    output.context = m_context.codec;
    output.encoderSequence = m_nextEncoderSequence++;
    output.producerTimestamp = static_cast<quint64>(frame.producedAtNs);
    output.waterfallAdvance = (m_emittedFrames % static_cast<quint64>(m_context.framesPerLine)) == 0;
    output.traceDbm = std::move(trace);
    output.waterfallDbm = std::move(waterfall);
    if (measurement.size() == output.traceDbm.size()) {
        output.measurementDbm = std::move(measurement);
    }
    if (m_context.codec.wideSamples != 0) {
        output.wideDbm = wideRowDbm(frame, frame.dbmOffset + stationOffsetDb, {
            m_request.centreHz, m_request.spanHz, m_sourceContext.centreHz,
            m_sourceContext.sampleRateHz, m_request.requestedWideSpanFactor},
            kMaxWideSamples);
        if (output.wideDbm.size() != m_context.codec.wideSamples) {
            return std::nullopt;
        }
    }
    // Keep the advancing calendar for a frame that is merely late within its
    // slot; shared sources then retain the requested average FPS. Only a
    // frame that missed at least one entire slot resynchronizes to itself,
    // preventing a post-stall catch-up burst.
    const qint64 followingScheduledDue =
        saturatingAdd(m_nextOutputDueNs, periodNs);
    m_nextOutputDueNs = frame.producedAtNs >= followingScheduledDue
        ? saturatingAdd(frame.producedAtNs, periodNs)
        : followingScheduledDue;
    ++m_emittedFrames;
    return output;
}

qint64 SpectrumEndpoint::earlyToleranceNs(qint64 periodNs, qint64 sourceFrameGapNs)
{
    constexpr qint64 kEarlyToleranceDivisor = 20;
    const qint64 floorNs = periodNs / kEarlyToleranceDivisor;
    if (sourceFrameGapNs <= 0) {
        return floorNs;
    }
    // A frame earlier than its slot by less than half the source's spacing
    // is nearer the slot than the source frame after it. Half a period at
    // most: a slower source fills every slot anyway, and a slot never takes
    // a frame nearer the slot before it.
    return std::max(floorNs, std::min(sourceFrameGapNs / 2, periodNs / 2));
}

std::optional<qint64> SpectrumEndpoint::outputDeadlineNs(qint64 producedAtNs,
                                                         qint64 sourceFrameGapNs) const
{
    if (!m_configured || producedAtNs < 0 || m_context.targetFps <= 0
        || (m_hasProducerTimestamp && producedAtNs <= m_lastProducerTimestampNs)) {
        return std::nullopt;
    }
    // The same period and early tolerance as consume().
    const qint64 periodNs = 1'000'000'000LL / m_context.targetFps;
    const qint64 toleranceNs = earlyToleranceNs(periodNs, sourceFrameGapNs);
    if (m_nextOutputDueNs != 0 && producedAtNs < m_nextOutputDueNs - toleranceNs) {
        return std::nullopt;
    }
    // The slot this frame fills, as consume() would place it: on the
    // calendar, or starting at the frame itself for a first frame or one
    // that already missed a whole slot (consume() resynchronizes those, so
    // an endpoint that has fallen behind is not urgent forever).
    const bool onCalendar = m_nextOutputDueNs != 0
        && producedAtNs < saturatingAdd(m_nextOutputDueNs, periodNs);
    return saturatingAdd(onCalendar ? m_nextOutputDueNs : producedAtNs, periodNs);
}

void SpectrumEndpoint::awaitContext()
{
    // As reset() for everything a context owns; the reducers stay so that
    // the next configure can judge whether the picture is the same.
    m_configured = false;
    m_context = {};
    m_hasProducerTimestamp = false;
    m_lastProducerTimestampNs = 0;
    m_nextOutputDueNs = 0;
    m_emittedFrames = 0;
    m_nextEncoderSequence = 0;
}

void SpectrumEndpoint::awaitMovedSource(quint64 fromGeneration, quint64 toGeneration)
{
    awaitContext();
    // The held center stays as it was: the next configure compares it with
    // the new one to tell a moved picture from the same one.
    if (m_planesHeld && m_sourceContext.sourceGeneration == fromGeneration) {
        m_sourceContext.sourceGeneration = toGeneration;
    }
}

void SpectrumEndpoint::reset()
{
    m_configured = false;
    m_planesHeld = false;
    m_tracePicture = SpectrumPictureKind::New;
    m_waterfallPicture = SpectrumPictureKind::New;
    m_hasOverlap = false;
    m_extendedView = false;
    m_traceReducer.reset();
    m_waterfallReducer.reset();
    m_extendedTraceReducer.reset();
    m_extendedWaterfallReducer.reset();
    m_hasProducerTimestamp = false;
    m_lastProducerTimestampNs = 0;
    m_nextOutputDueNs = 0;
    m_emittedFrames = 0;
    m_nextEncoderSequence = 0;
}

} // namespace NereusSDR
