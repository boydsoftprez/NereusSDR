// no-port-check: NereusSDR-original configuration value and validation contract.
// Defaults/domains are source-backed by TAPR WDSP 2.10 RXA.c/create_nnr,
// nnet.c/create_dfhead, NNET_TAU_DEFAULT and NNET_GMAX_DB, with the operator
// ranges approved in 2026-09-21-wdsp210-nnr-ps3-design.md section 6.
// Modification history (NereusSDR):
//   2026-09-21 — J.J. Boyd (KG4VCF), with OpenAI Codex assistance.
//   2026-09-23 : NnrLimit, nnrLimitExplanation and the applied-limit
//                readback (R-R3-40) by J.J. Boyd (KG4VCF), with Anthropic
//                Claude Code assistance. NnrLimitSite (the Core wording for
//                a remote window) added the same day.
//   2026-09-27 : the defaults and ranges read from ControlRanges.h (the
//                table NnrControls and the Core's catalogue read; values
//                unchanged; R-IOS-06, R-IOS-27) by J.J. Boyd (KG4VCF), with
//                Anthropic Claude Code assistance.
#pragma once

#include "core/ControlRanges.h"
#include "core/WdspTypes.h"

#include <array>
#include <cmath>
#include <QString>

namespace NereusSDR {

struct NnrSettings {
    // Defaults and ranges from ControlRanges.h, the table NnrControls and
    // the Core's catalogue read.
    int modelSlot{static_cast<int>(ControlRanges::kNnrModel.defaultValue)};
    double maskFloorDb{ControlRanges::kNnrMaskFloor.defaultValue};
    NrPosition position{static_cast<NrPosition>(ControlRanges::kNnrPosition.defaultValue)};
    double alpha{ControlRanges::kNnrAlpha.defaultValue};
    double alphaKneeDb{ControlRanges::kNnrAlphaKnee.defaultValue};
    double tauSeconds{ControlRanges::kNnrTau.defaultValue};
    double maxGainDb{ControlRanges::kNnrMaxGain.defaultValue};
    double attackMs{ControlRanges::kNnrAttack.defaultValue};
    double releaseMs{ControlRanges::kNnrRelease.defaultValue};

    bool operator==(const NnrSettings&) const = default;

    [[nodiscard]] bool isValid() const noexcept
    {
        const auto within = [](double value, const ControlRanges::NrControl& control) {
            return std::isfinite(value) && value >= control.min && value <= control.max;
        };
        return (modelSlot == 0 || modelSlot == 1)
            && (position == NrPosition::PreAgc || position == NrPosition::PostAgc)
            && within(maskFloorDb, ControlRanges::kNnrMaskFloor)
            && within(alpha, ControlRanges::kNnrAlpha)
            && within(alphaKneeDb, ControlRanges::kNnrAlphaKnee)
            && within(tauSeconds, ControlRanges::kNnrTau)
            && within(maxGainDb, ControlRanges::kNnrMaxGain)
            && within(attackMs, ControlRanges::kNnrAttack)
            && within(releaseMs, ControlRanges::kNnrRelease);
    }
};

enum class NnrModelSource : int { Unavailable = 0, Bundled = 1, File = 2 };

// R-R3-40: a runtime limit the Core sets when a receiver cannot keep up with
// NNR. It never changes the saved choice; the operator can clear it.
enum class NnrLimit : int { None = 0, StandardOnly = 1, Off = 2 };

[[nodiscard]] inline bool isValidNnrLimit(int value) noexcept
{
    return value >= static_cast<int>(NnrLimit::None) && value <= static_cast<int>(NnrLimit::Off);
}

// Which computer the operator is told could not keep up: the one running
// this window, or, in a remote window, the Core computer it is connected to.
enum class NnrLimitSite : int { ThisComputer = 0, CoreComputer = 1 };

// What the operator reads while a limit is in force; empty when there is none.
[[nodiscard]] inline QString nnrLimitExplanation(
    int limit, NnrLimitSite site = NnrLimitSite::ThisComputer)
{
    const bool core = site == NnrLimitSite::CoreComputer;
    switch (limit) {
    case static_cast<int>(NnrLimit::StandardOnly):
        return core ? QStringLiteral("Noise reduction is using the Standard model. "
                                     "The Core computer could not keep up with Premium.")
                    : QStringLiteral("Noise reduction is using the Standard model. "
                                     "This computer could not keep up with Premium.");
    case static_cast<int>(NnrLimit::Off):
        return core ? QStringLiteral("Noise reduction was turned off. "
                                     "The Core computer could not keep up.")
                    : QStringLiteral("Noise reduction was turned off. "
                                     "This computer could not keep up.");
    default:
        return {};
    }
}

// Readback only. Diagnostic processing modes intentionally have no normal
// save/load representation; they reset when the station session is replaced.
struct NnrDiagnostics {
    bool operator==(const NnrDiagnostics&) const = default;
    bool available{false};
    bool ready{false};
    bool running{false};
    bool rateSupported{false};
    int actualModelSlot{-1};
    std::array<bool, 2> modelAvailable{};
    std::array<NnrModelSource, 2> modelSources{};
    int dspRateHz{0};
    int networkRateHz{0};
    int delaySamples{0};
    double latencyMs{0.0};
    int testMode{0};
    int outputMode{1};
    bool profilingAvailable{false};
    QString explanation;
    // R-R3-40: the runtime limit WDSP has applied (NnrLimit) and whether NNR
    // is requested on. Local readback only; not mirrored.
    int appliedLimit{0};
    bool requestedRun{false};
};

} // namespace NereusSDR
