#pragma once
// =================================================================
// src/core/session/media/DisplayExtras.h  (NereusSDR)
// =================================================================
// no-port-check: NereusSDR-original. iPhone app Task 20 (R-IOS-27).
//
// The display extras a spectrum subscription may ask the Core for, and the
// NSDX datagram that carries them beside each NSDC frame. The computations
// themselves are the desktop's own (core/spectrum: PeakBlobDetector,
// ActivePeakHoldTrace, DisplayFollowers, and ClarityController); this file
// only asks them, per endpoint, and puts their answers on the wire. The
// exact layout and the subscription fields are in
// docs/architecture/2026-09-23-display-extras-v1.md.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29  J.J. Boyd / KG4VCF  displayCostWithExtras, shared with a
//                                    remote window's planner. AI-assisted
//                                    via Anthropic Claude Code.
//   2026-09-24  J.J. Boyd / KG4VCF  Created for iPhone app Task 20.
//                                    AI-assisted via Anthropic Claude
//                                    Code.
//   2026-09-25  J.J. Boyd / KG4VCF  R-IOS-27, R-IOS-06: Clarity's
//                                    Re-tune for one endpoint
//                                    (displayExtrasVersion 2).
//                                    AI-assisted via Anthropic Claude Code.
//   2026-09-28  J.J. Boyd / KG4VCF  activePeakHold.onTx and the
//                                    endpoint's transmitting input: the
//                                    desktop's "Update during TX" and hold
//                                    time reach an app's peak hold
//                                    (displayExtrasVersion 3).
//                                    AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/session/media/DisplayBudget.h"
#include "core/session/media/DisplayCodec.h"
#include "core/spectrum/ActivePeakHoldTrace.h"
#include "core/spectrum/DisplayFollowers.h"
#include "core/spectrum/PeakBlobDetector.h"

#include <QByteArray>
#include <QJsonObject>
#include <QStringList>
#include <QVector>

#include <memory>
#include <optional>
#include <utility>

namespace NereusSDR {

class ClarityController;

// ── The subscription fields ─────────────────────────────────────────────

enum class WaterfallLevelMode {
    Manual,
    Agc,
    NoiseFloorAgc,
    Clarity,
};

struct DisplayExtrasRequest {
    /// `peakBlobs {count, holdMs, fallDbPerSec, insideOnly}`. holdMs 0 is
    /// the desktop's hold off; fallDbPerSec 0 is its hard cut after the
    /// hold (decay off).
    struct PeakBlobs {
        int count {3};
        int holdMs {500};
        double fallDbPerSec {6.0};
        bool insideOnly {false};
        bool operator==(const PeakBlobs&) const = default;
    };
    /// `activePeakHold {enabled, holdMs, fallDbPerSec[, onTx]}`. `onTx`
    /// (displayExtrasVersion 3) keeps the trace running while the
    /// endpoint's slice transmits; absent, it is true, which is how every
    /// older app's trace behaved.
    struct ActivePeakHold {
        bool enabled {false};
        int holdMs {2000};
        double fallDbPerSec {6.0};
        bool onTx {true};
        bool operator==(const ActivePeakHold&) const = default;
    };
    /// `noiseFloor {enabled, shiftDb[, fastAttack]}`. `fastAttack`
    /// (displayExtrasVersion 4, optional, false when absent) also asks for
    /// the noise floor state section: whether the floor is in fast attack.
    struct NoiseFloor {
        bool enabled {false};
        double shiftDb {0.0};
        bool fastAttack {false};
        bool operator==(const NoiseFloor&) const = default;
    };
    /// `waterfallLevels {mode, lowDbm, highDbm, offsetDb}`.
    struct WaterfallLevels {
        WaterfallLevelMode mode {WaterfallLevelMode::Manual};
        double lowDbm {-122.0};
        double highDbm {-62.0};
        int offsetDb {0};
        bool operator==(const WaterfallLevels&) const = default;
    };

    std::optional<PeakBlobs> peakBlobs;
    std::optional<ActivePeakHold> activePeakHold;
    std::optional<NoiseFloor> noiseFloor;
    std::optional<WaterfallLevels> waterfallLevels;
    std::optional<bool> normalize;
    std::optional<double> calibrationOffsetDb;
    /// The spectrum's averaging time; the waterfall's too unless
    /// `waterfallAverageTimeMs` is present.
    std::optional<int> averageTimeMs;
    /// The waterfall's own averaging time, in the same range.
    std::optional<int> waterfallAverageTimeMs;

    /// No display extras field was present.
    bool empty() const;
    /// The NSDX sections these fields ask for (kDisplayExtras* bits).
    quint8 sections() const;
    bool operator==(const DisplayExtrasRequest&) const = default;
};

/// The subscribe keys display extras v1 adds, in the document's order.
const QStringList& displayExtrasSubscribeKeys();

/// Reads the display extras keys present in a subscribe operation. False,
/// with `request` untouched, when one of them has the wrong shape, an
/// unknown member or a value outside its range.
bool parseDisplayExtrasRequest(const QJsonObject& subscribe, DisplayExtrasRequest& request);

// ── The NSDX datagram ───────────────────────────────────────────────────

inline constexpr quint8 kDisplayExtrasPeakBlobs = 0x01;
inline constexpr quint8 kDisplayExtrasPeakHold = 0x02;
inline constexpr quint8 kDisplayExtrasNoiseFloor = 0x04;
inline constexpr quint8 kDisplayExtrasWaterfallLevels = 0x08;
/// Version 4: one byte, bit 0 set while the noise floor is in fast attack.
inline constexpr quint8 kDisplayExtrasNoiseFloorState = 0x10;
inline constexpr quint8 kDisplayExtrasNoiseFloorFastAttack = 0x01;
inline constexpr quint8 kDisplayExtrasKnownSections = 0x1F;
inline constexpr quint8 kDisplayExtrasVersion = 1;
inline constexpr quint16 kDisplayExtrasHeaderBytes = 20;
/// The most blobs a section carries (the desktop's Setup allows 1 to 20).
inline constexpr int kDisplayExtrasMaxBlobs = 20;
/// One datagram is at most what one NSDC frame may be.
inline constexpr int kDisplayExtrasMaxBytes = DisplayCodecEncoder::kMaxEncodedBytes;

struct DisplayExtrasBlob {
    /// The trace sample (display pixel) the peak is at.
    quint16 pixel {0};
    float dbm {0.0f};
    bool operator==(const DisplayExtrasBlob&) const = default;
};

struct DisplayExtrasFrame {
    quint32 endpointId {0};
    quint32 contextGeneration {0};
    /// The encoder sequence of the NSDC frame this goes beside.
    quint32 encoderSequence {0};
    std::optional<QVector<DisplayExtrasBlob>> peakBlobs;
    std::optional<QVector<float>> peakHoldDbm;
    std::optional<float> noiseFloorDbm;
    std::optional<std::pair<float, float>> waterfallLevelsDbm;
    /// Section 0x10 (version 4): the noise floor is in fast attack.
    std::optional<bool> noiseFloorFastAttack;

    quint8 sections() const;
};

/// The largest datagram these sections can make for a trace of
/// `traceSamples` samples.
quint32 displayExtrasWorstCaseBytes(quint8 sections, int traceSamples);

/// The display budget charge of an endpoint of `pixels` at `fps` that asks
/// for the extras `sections` (kDisplayExtras* bits): spectrumDisplayCost's,
/// plus, for any section, one message a frame and at most
/// displayExtrasWorstCaseBytes a frame, and the peak hold row's samples a
/// frame. The Core charges this; a window plans with the same number.
std::optional<SpectrumDisplayCost> displayCostWithExtras(int pixels, int fps,
                                                         bool includeWidePlane,
                                                         quint8 sections);

/// Encodes a frame for the endpoint context it belongs to: the peak hold
/// row is quantised on that context's interval. Empty when the frame has no
/// section, does not match the context, or holds a value that is not finite.
QByteArray encodeDisplayExtras(const DisplayExtrasFrame& frame,
                               const DisplayCodecContext& context);

enum class DisplayExtrasReason {
    None,
    BadMagic,
    UnsupportedVersion,
    UnknownSections,
    Truncated,
    Oversized,
    Malformed,
    ContextMismatch,
};

struct DisplayExtrasDecodeResult {
    bool accepted {false};
    DisplayExtrasReason reason {DisplayExtrasReason::Malformed};
    DisplayExtrasFrame frame;
};

/// Decodes one datagram against the receiver's accepted context for its
/// endpoint. Stateless: a datagram stands on its own.
DisplayExtrasDecodeResult decodeDisplayExtras(const QByteArray& bytes,
                                              const DisplayCodecContext& context);

// ── The Core's computation, one per endpoint ────────────────────────────

struct DisplayExtrasInputs {
    /// The endpoint's frame rate and the Core's clock.
    int fps {30};
    qint64 nowMs {0};
    /// The endpoint's crop, for the blobs' passband.
    double centreHz {0.0};
    double spanHz {0.0};
    /// The endpoint's slice: its frequency and filter edges.
    double sliceHz {0.0};
    double filterLowHz {0.0};
    double filterHighHz {0.0};
    int band {0};
    bool mox {false};
    /// The endpoint's slice is the one transmitting (MOX on and the slice
    /// is the transmit slice): Thetis's local_mox for that receiver.
    bool transmitting {false};
    /// The source's bin width (sample rate / FFT size), for normalise.
    double binWidthHz {0.0};
    /// The endpoint's trace detector: normalise applies only with Average,
    /// Sample or RMS (normalizeAppliesToDetector), as on the desktop.
    int traceDetector {0};
};

class DisplayExtrasProcessor {
public:
    explicit DisplayExtrasProcessor(const DisplayExtrasRequest& request);
    ~DisplayExtrasProcessor();
    DisplayExtrasProcessor(const DisplayExtrasProcessor&) = delete;
    DisplayExtrasProcessor& operator=(const DisplayExtrasProcessor&) = delete;

    const DisplayExtrasRequest& request() const { return m_request; }

    /// A new endpoint context: the blobs and the peak hold start again, as a
    /// desktop remote window's do on a new context.
    void newContext();

    /// Clarity's input: the full-source noise floor the Core already
    /// estimates for the `noise-floor` operation, before any shift.
    void feedNoiseFloor(float floorDbm, qint64 nowMs);

    /// R-IOS-27, R-IOS-06 (displayExtrasVersion 2): Clarity's Re-tune, what
    /// the desktop's Re-tune button does for its pan
    /// (ClarityController::retuneNow). False, and nothing changes, when this
    /// endpoint's waterfall levels are not in Clarity mode.
    bool retuneClarity();

    /// What the Core adds to every dBm it sends for this endpoint:
    /// calibrationOffsetDb plus the normalise shift, which applies only with
    /// the Average, Sample and RMS trace detectors (2, 3, 4), as Thetis
    /// updateNormalizePan and the desktop do.
    float displayShiftDb(double binWidthHz, int traceDetector) const;

    /// Runs the computations on one reduced frame as the Core produced it
    /// (before the shift) and returns the sections asked for, shifted, for
    /// that frame's endpoint, generation and sequence. The waterfall levels
    /// move only on a frame that adds a waterfall line.
    DisplayExtrasFrame process(const DisplayCodecFrame& frame, const DisplayExtrasInputs& inputs);

    // Read-only views for tests.
    const PeakBlobDetector& peakBlobs() const { return m_blobs; }
    const ActivePeakHoldTrace& activePeakHold() const { return m_peakHold; }
    const NoiseFloorFollower& noiseFloor() const { return m_noiseFloor; }
    std::pair<float, float> waterfallLevels() const { return {m_activeLow, m_activeHigh}; }
    /// This endpoint's Clarity controller, or nullptr outside Clarity mode.
    const ClarityController* clarity() const { return m_clarity.get(); }

private:
    DisplayExtrasRequest m_request;
    PeakBlobDetector m_blobs;
    ActivePeakHoldTrace m_peakHold;
    NoiseFloorFollower m_noiseFloor;
    NoiseFloorFastAttackTrigger m_fastAttackTrigger;
    WaterfallLevelFollower m_levels;
    std::unique_ptr<ClarityController> m_clarity;
    float m_activeLow {-122.0f};
    float m_activeHigh {-62.0f};
    bool m_mox {false};
};

} // namespace NereusSDR
