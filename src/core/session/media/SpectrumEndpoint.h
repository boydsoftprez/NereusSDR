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
// Modification history (NereusSDR):
//   2026-10-21: An endpoint slower than its source takes the source frame
//               nearest each of its slots (earlyToleranceNs), so its rows
//               are evenly spaced. J.J. Boyd (KG4VCF), AI-assisted via
//               Anthropic Claude Code.
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
    int averageFrames {SpectrumAvenger::kMinWindowFrames};
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

/// R-DISP-HOLD, R-DISP-RETUNE: what an accepted configure made of the picture
/// the endpoint already held. The order is from least kept to most.
enum class SpectrumPictureKind {
    /// Anything else: the reduction history starts again.
    New,
    /// The same shape in another place: the stream center, the crop center
    /// or both moved, and nothing else. The averaged state carries on (it is
    /// not shifted to follow the frequency and not cleared).
    Moved,
    /// Nothing about the geometry changed. Everything is kept.
    Same,
};

class SpectrumEndpoint {
public:
    static constexpr int kMaxPixels = 4096;
    static constexpr int kMaxWideSamples = 768;

    /// The trace and waterfall sample count a request receives from a source
    /// of `fftBins` bins: min(requested, visible bins, kMaxPixels), or
    /// min(requested, kMaxPixels) for an active extended view, whose ADC
    /// wings own every pixel. Zero means the crop covers no source bin.
    /// The visible bins are those the span holds wherever it lies over them
    /// (ceil(span in bins) + 1), not the one more it touches at most
    /// alignments, so the count is the same before and after a tune that
    /// moves the crop a fraction of a bin (R-DISP-RETUNE).
    static int grantedPixels(const SpectrumEndpointRequest& request, int fftBins,
                             double sourceCentreHz, double sourceSampleRateHz,
                             bool extendedView);

    /// Validates and accepts a request against an active source. The context,
    /// the sequence and the output cadence always restart. A reduction history
    /// restarts too unless its plane draws the same picture as before
    /// (R-DISP-HOLD): same source and source generation, same FFT size, source
    /// centre and sample rate, same ADC descriptor for an extended view, same
    /// crop centre, span and pixel count, same detector and averaging mode.
    /// The dBm range, frame rate, frames per line and averaging constant may
    /// differ; a kept plane averages its next frame with the new constant.
    ///
    /// R-DISP-RETUNE: a plane whose picture only moved keeps its history
    /// too. That is the same source continued through a retune
    /// (awaitMovedSource), the same FFT size and sample rate, the same ADC
    /// descriptor for an extended view, the same span and pixel count, the
    /// same detector and averaging mode, with the source center, the crop
    /// center or both at another frequency. The plane takes the new crop;
    /// what it has averaged per pixel is neither shifted nor cleared, as on
    /// the local display. tracePicture() and waterfallPicture() say which of
    /// the three each plane was.
    /// A failed request preserves the previous accepted endpoint state.
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
    /// newer than the last frame taken, or early). `sourceFrameGapNs` is
    /// the frame's DaemonSpectrumFrame::sourceFrameGapNs.
    std::optional<qint64> outputDeadlineNs(qint64 producedAtNs,
                                           qint64 sourceFrameGapNs = 0) const;

    /// How early a frame may be against its slot and still fill it: half
    /// the source's frame spacing, so the frame nearest the slot fills it
    /// and the one before it does not, bounded by half this endpoint's
    /// period, and never less than a twentieth of the period (the rule for
    /// a frame whose spacing is unknown, 0).
    ///
    /// A twentieth alone took a frame only when it came no more than that
    /// early. With a source at twice the endpoint's rate, a frame early by
    /// more (its publish time jittered) was refused and the next one, a
    /// source frame later, filled the slot late; the slot after came on
    /// time. Rows then alternated 67 and 33 ms at 15 fps from 30.
    static qint64 earlyToleranceNs(qint64 periodNs, qint64 sourceFrameGapNs);

    /// Whether the last accepted configure found that plane drawing the same
    /// picture as before (R-DISP-HOLD): true only for SpectrumPictureKind::
    /// Same. A moved plane keeps its averaged state too and reads false here.
    bool tracePlaneKept() const { return m_tracePicture == SpectrumPictureKind::Same; }
    bool waterfallPlaneKept() const { return m_waterfallPicture == SpectrumPictureKind::Same; }

    /// What the last accepted configure made of each plane, and of the
    /// endpoint: the lesser of its two planes, so the picture is the same
    /// only when both are, and new as soon as either is.
    SpectrumPictureKind tracePicture() const { return m_tracePicture; }
    SpectrumPictureKind waterfallPicture() const { return m_waterfallPicture; }
    SpectrumPictureKind picture() const
    {
        return m_tracePicture < m_waterfallPicture ? m_tracePicture : m_waterfallPicture;
    }

    /// R-DISP-RETUNE: which picture the accepted context's frames continue,
    /// named by the context generation at which the planes now running were
    /// started. configure() sets it where it makes a plane anew and leaves
    /// it where it carries both planes on, so it is the record of what was
    /// done to the planes: unchanged through SpectrumPictureKind::Same and
    /// Moved, the new context's generation for New. A client compares it
    /// with the picture it shows instead of working the verdict out again
    /// from the fields of the context.
    quint32 pictureGeneration() const { return m_pictureGeneration; }

    /// Ends the accepted context and keeps the reduction histories for the
    /// next configure to judge (R-DISP-HOLD). Until that configure the
    /// endpoint is not configured and produces nothing, exactly as after
    /// reset().
    void awaitContext();

    /// R-DISP-RETUNE: as awaitContext(), for a source that was relabeled
    /// through a retune (DaemonSpectrumSourceUpdate::relabeled). Histories
    /// held from `fromGeneration` are from then on histories of
    /// `toGeneration`, so the next configure can keep them as a moved
    /// picture. Histories of any other generation are left as they are, and
    /// the next configure starts a new picture for them: a generation the
    /// endpoint was not told continues its own is a restarted source.
    void awaitMovedSource(quint64 fromGeneration, quint64 toGeneration);

    /// Ends the accepted context and drops both reduction histories: the
    /// next configure starts a new picture.
    void reset();

private:
    static bool validRequest(const SpectrumEndpointRequest& request);
    static bool validSourceContext(const SpectrumEndpointSourceContext& sourceContext);

    bool m_configured {false};
    bool m_hasOverlap {false};
    bool m_extendedView {false};
    bool m_planesHeld {false};
    SpectrumPictureKind m_tracePicture {SpectrumPictureKind::New};
    SpectrumPictureKind m_waterfallPicture {SpectrumPictureKind::New};
    quint32 m_pictureGeneration {0};
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
