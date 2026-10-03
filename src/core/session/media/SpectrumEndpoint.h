#pragma once
// =================================================================
// src/core/session/media/SpectrumEndpoint.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Remote daemon R3 Task 2.
//
// A session-owned, GUI-free consumer of one daemon FFT source. It owns the
// independent trace/waterfall reducer histories and produces codec-ready dBm
// rows; transport and subscription messages remain outside this class.
//
// =================================================================

#include "core/session/media/DaemonSpectrumSource.h"
#include "core/session/media/DisplayCodec.h"
#include "core/session/media/WidebandDisplayContext.h"
#include "core/spectrum/ExtendedSpectrumReducer.h"
#include "core/spectrum/SpectrumReducer.h"
#include "core/spectrum/WidebandSpectrumCache.h"

#include <memory>
#include <optional>

namespace NereusSDR {

/// Accepted-context limits shared by Core validation and the GUI's context
/// parser, so a subscription Core accepts always yields a context the GUI
/// accepts (R-R3-09).
inline constexpr int kMaxFramesPerLine = 10000;
inline constexpr double kMinDbmLimit = -400.0;
inline constexpr double kMaxDbmLimit = 100.0;

/// Reasons Core sends when it retires a spectrum endpoint because of an
/// operator change rather than refusing it (R-R3-01, R-R3-37): the slice was
/// removed, its stream binding changed, or the slice retuned so the source no
/// longer covers the requested crop. DaemonMediaController sends these and the
/// window's refusal filter matches them, so one wording serves both ends.
/// The first two are compared as they are by windows already in use, so they
/// keep their wording; the third is in operator words (R-IOS-01).
inline constexpr char kRetireReasonSliceRemoved[] = "slice removed";
inline constexpr char kRetireReasonStreamBindingChanged[] = "slice stream binding changed";
inline constexpr char kRetireReasonSourceRetune[] = "The receiver was retuned away from this view.";

/// Why Core granted less than a spectrum subscription asked for.
enum class SpectrumLimitReason {
    None,
    /// The requested FFT size is above the largest size the engine supports.
    LargestSize,
    /// Another endpoint uses the (stream, tier) engine, so its size stands.
    /// Parity Task 17 follow-up: also when the engine runs at another
    /// decimation than this endpoint asked for, for the same reason.
    SharedEngine,
    /// The source has fewer visible bins than the requested pixels.
    SourceBins,
};

/// What Core actually granted one spectrum endpoint (R-R3-01, R-R3-08).
/// `reason` names the root cause of a reduction: an FFT size limit outranks
/// the pixel rule, because a smaller engine is what leaves fewer bins.
struct SpectrumGrant {
    int requestedFftSize {0};
    int grantedFftSize {0};
    FftTier grantedTier {FftTier::Wide};
    int requestedPixels {0};
    int grantedPixels {0};
    /// Parity Task 17 follow-up (R-R3-01): the decimation asked for (1
    /// without the subscribe's `decimation`) and the engine's. Kept by the
    /// Core; the wire reports a held decimation as limit "shared".
    int requestedDecimation {1};
    int grantedDecimation {1};
    SpectrumLimitReason reason {SpectrumLimitReason::None};
};

struct SpectrumPlaneRequest {
    SpectrumDetectorMode detector {SpectrumDetectorMode::Peak};
    int averageMode {0};
    double averageAlpha {0.12};
};

struct SpectrumEndpointRequest {
    quint32 endpointId {0};
    MediaSourceKey source;
    double centreHz {0.0};
    double spanHz {0.0};
    int pixels {0};
    int targetFps {0};
    int framesPerLine {0};
    SpectrumPlaneRequest trace;
    SpectrumPlaneRequest waterfall;
    float minDbm {-160.0f};
    float maxDbm {0.0f};
    /// GUI supplies its maximum 3D shape factor. Zero disables wide output.
    double requestedWideSpanFactor {0.0};
    /// Negotiated permission to compose the slice's authorized ADC wings.
    bool extendedView {false};
};

struct SpectrumEndpointSourceContext {
    MediaSourceKey source;
    quint64 sourceGeneration {0};
    int fftBins {0};
    double centreHz {0.0};
    double sampleRateHz {0.0};
    quint32 contextGeneration {0};
    /// ADC metadata is a separate identity from the DDC FFT source above.
    WidebandDisplayContext wideband;
};

struct SpectrumEndpointContext {
    DisplayCodecContext codec;
    MediaSourceKey source;
    quint64 sourceGeneration {0};
    double exactCentreHz {0.0};
    double exactSpanHz {0.0};
    double wideCentreHz {0.0};
    double wideSpanHz {0.0};
    int targetFps {0};
    int framesPerLine {0};
    /// Accepted physical-ADC descriptor for an extended composite row.
    WidebandDisplayContext wideband;
};

class SpectrumEndpoint {
public:
    static constexpr int kMaxPixels = 4096;
    static constexpr int kMaxWideSamples = 768;

    /// The trace and waterfall sample count a request receives from a source
    /// of `fftBins` bins: min(requested, visible bins, kMaxPixels), or
    /// min(requested, kMaxPixels) for an active extended view, whose ADC
    /// wings own every pixel. Zero means the crop covers no source bin.
    static int grantedPixels(const SpectrumEndpointRequest& request, int fftBins,
                             double sourceCentreHz, double sourceSampleRateHz,
                             bool extendedView);

    /// Validates and accepts a request against an active source. This clears
    /// both reduction histories and output cadence. A failed request preserves
    /// the previous accepted endpoint state.
    bool configure(const SpectrumEndpointRequest& request,
                   const SpectrumEndpointSourceContext& sourceContext);

    bool configured() const { return m_configured; }
    const SpectrumEndpointContext& context() const { return m_context; }

    /// Reduces one matching current-generation FFT frame. Returns no frame
    /// when source/context do not match, input is invalid, timestamps are
    /// non-monotonic, or cadence has not reached the requested interval. The
    /// optional ADC row is used only by an accepted extended context; a missing
    /// or mismatched row paints its wings at the requested display floor.
    std::optional<DisplayCodecFrame> consume(
        const DaemonSpectrumFrame& frame,
        double stationOffsetDb = 0.0,
        const std::optional<WidebandSpectrumFrame>& widebandFrame = std::nullopt);

    /// When a frame produced at `producedAtNs` must be sent by, in the
    /// producer's clock, before this endpoint loses an output slot: the end
    /// of the output period that frame would fill (R-R3-08, R-R3-37). For
    /// an endpoint that takes every source frame this is the next source
    /// frame. Read-only: only consume() advances the schedule. Empty when
    /// consume() would take nothing from such a frame (not configured, not
    /// newer than the last frame taken, or early).
    std::optional<qint64> outputDeadlineNs(qint64 producedAtNs) const;

    void reset();

private:
    static bool validRequest(const SpectrumEndpointRequest& request);
    static bool validSourceContext(const SpectrumEndpointSourceContext& sourceContext);

    bool m_configured {false};
    bool m_hasOverlap {false};
    bool m_extendedView {false};
    SpectrumEndpointRequest m_request;
    SpectrumEndpointSourceContext m_sourceContext;
    SpectrumEndpointContext m_context;
    std::unique_ptr<SpectrumReducer> m_traceReducer;
    std::unique_ptr<SpectrumReducer> m_waterfallReducer;
    std::unique_ptr<ExtendedSpectrumReducer> m_extendedTraceReducer;
    std::unique_ptr<ExtendedSpectrumReducer> m_extendedWaterfallReducer;
    bool m_hasProducerTimestamp {false};
    qint64 m_lastProducerTimestampNs {0};
    qint64 m_nextOutputDueNs {0};
    quint64 m_emittedFrames {0};
    quint32 m_nextEncoderSequence {0};
};

} // namespace NereusSDR
