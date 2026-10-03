// no-port-check: NereusSDR-original owning value type for the bounded PS3
// display transport boundary.  Field names mirror the pinned WDSP ABI.

#pragma once

#include "../../../third_party/wdsp/src/ps3_abi.h"

#include <array>
#include <cstdint>
#include <vector>

namespace NereusSDR {

struct Ps3Snapshot {
    // Pinned TAPR WDSP 2.10 display geometry.  The vendor import exposes the
    // matching PS3_MAX_DISPLAY_SAMPLES and PS3_DISPLAY_CORRECTION_POINTS
    // constants and compile-time guards its internal storage against them.
    static constexpr int kMaxSampleCount = PS3_MAX_DISPLAY_SAMPLES;
    static constexpr int kMaxCorrectionCount = PS3_DISPLAY_CORRECTION_POINTS;

    static_assert(kMaxSampleCount == 4096);
    static_assert(kMaxCorrectionCount == 512);

    int channelId{-1};
    std::uint64_t sessionGeneration{0};
    std::uint64_t sequence{0};
    std::int64_t capturedAtUnixMilliseconds{0};
    int sampleCount{0};
    int correctionCount{0};

    std::vector<double> x;
    std::vector<double> ym;
    std::vector<double> yc;
    std::vector<double> ys;

    std::vector<double> xmCorrection;
    std::vector<double> ymCorrection;
    std::vector<double> xaCorrection;
    std::vector<double> yaCorrection;

    double phaseReferenceDegrees{0.0};
};

struct Ps3PlotPoint {
    double x{0.0};
    double y{0.0};
};

struct Ps3PlotData {
    std::vector<Ps3PlotPoint> measuredMagnitude;
    std::vector<Ps3PlotPoint> measuredGain;
    std::vector<Ps3PlotPoint> measuredPhase;
    std::vector<Ps3PlotPoint> correctionMagnitude;
    std::vector<Ps3PlotPoint> correctionGain;
    std::vector<Ps3PlotPoint> correctionPhase;
};

// Current Core-owned packet route; no GUI access to the pump or sample buffers.
struct Ps3RoutingSnapshot {
    int txMonitorDdc{-1};
    int feedbackDdc{-1};
    int feedbackChannelId{-1};
    bool pumpActive{false};
    std::uint64_t pairedBlocks{0};
};

struct Ps3StatusSnapshot {
    int channelId{-1};
    std::uint64_t sessionGeneration{0};
    std::uint64_t sequence{0};
    std::int64_t capturedAtUnixMilliseconds{0};
    std::array<int, 16> raw{};

    int feedbackLevel{0};             // info[4]
    int successfulCalibrations{0};    // info[5]
    int solutionStatusBits{0};        // info[6]
    int attemptedCalibrations{0};     // info[7]
    int fileStatusBits{0};            // info[12]
    int dogCount{0};                  // info[13]
    bool correctionsApplied{false};   // info[14] == 1
    int engineState{0};               // info[15]

    bool solutionComparisonFailed{false}; // info[6] bit 0
    bool overdriveOrBucketFillFailure{false}; // info[6] bit 1
    bool saveFailed{false};           // info[12] bit 0
    bool restoreFailed{false};        // info[12] bit 1

    double requestedTxDelayNs{0.0};
    double appliedTxDelayNs{0.0};
    double hardwarePeak{0.0};
    double maxTx{0.0};
    int feedbackRateHz{0};
    int txMonitorDdc{-1};
    int feedbackDdc{-1};
    int feedbackChannelId{-1};
    bool pumpActive{false};
    std::uint64_t pairedBlocks{0};
    bool psEnabled{false};
    bool mox{false};
    bool runCalibrationProcessing{false};
    bool correctionRun{false};
    bool correctionBusy{false};

    std::uint64_t saveGeneration{0};
    bool savePending{false};
    int saveResult{0};
    std::uint64_t restoreGeneration{0};
    bool restorePending{false};
    int restoreResult{0};
};

enum class Ps3FileOperationKind : int {
    Save = 0,
    Restore = 1,
};

enum class Ps3FileOperationResult : int {
    Success = 0,
    Failure = 1,
    Cancelled = 2,
};

struct Ps3FileOperationStatus {
    std::uint64_t generation{0};
    bool pending{false};
    Ps3FileOperationResult result{Ps3FileOperationResult::Success};
};

struct Ps3FileOperationToken {
    Ps3FileOperationKind kind{Ps3FileOperationKind::Save};
    std::uint64_t sessionGeneration{0};
    std::uint64_t nativeCompletionGeneration{0};
};

struct Ps3CorrectionState {
    bool run{false};
    bool busy{false};
};

} // namespace NereusSDR
