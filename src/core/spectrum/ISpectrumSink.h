#pragma once
// =================================================================
// src/core/spectrum/ISpectrumSink.h  (NereusSDR)
// =================================================================
//
// no-port-check: NereusSDR-original. Abstract sink that RadioModel holds
// in place of a concrete SpectrumWidget*, so src/core and src/models
// contain no reference to src/gui and nereusd can link NereusCore alone.
// SpectrumWidget implements it; the daemon supplies its own implementation
// in a later phase.
//
// Method list is exactly the set RadioModel called on the widget as of
// 2026-08-02, found by grepping every m_spectrumWidget-> call site plus
// every call reached indirectly through the spectrumWidget() accessor
// (RadioModel::applyClaritySmoothDefaults stores the accessor's result in
// a local SpectrumWidget* and calls seven setters on it, which a plain
// "m_spectrumWidget->" grep does not see). Do not add methods
// speculatively: find a real call site first.
//
// WfColorScheme and AverageMode are relocated here verbatim from
// gui/SpectrumWidget.h (same "no-port-check: extraction, not a new port"
// treatment R1 Task 2 used for SpectrumDetectorMode): two of the eight
// interface methods above pass one of these enums by value, and passing
// an enum by value needs its full enumerator list at both the
// declaration and every call site, not just the forward-declared type.
// RadioModel::applyClaritySmoothDefaults names specific enumerators
// (WfColorScheme::ClarityBlue, AverageMode::Logarithmic), so once
// RadioModel.cpp drops its "gui/" include, gui/SpectrumWidget.h is no
// longer available to supply them. Kept in namespace NereusSDR under
// their original names (no rename, unlike SpectrumDetector ->
// SpectrumDetectorMode), so gui/SpectrumWidget.h needs no compatibility
// alias: it simply stops defining them and picks them up from this
// header's include instead. Original Thetis citation comments preserved
// verbatim below, per the inline-comment-preservation rule.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-02  J.J. Boyd / KG4VCF  Remote daemon R1, extraction 1 of 9.
//                                    AI-assisted transformation via
//                                    Anthropic Claude Code.
// =================================================================

#include <QColor>

namespace NereusSDR {

// Waterfall color scheme presets.
// Default matches AetherSDR/SmartSDR style.
// From Thetis enums.cs:68-79 (ColorScheme enum). Expanded 4 → 7 in
// Phase 3G-8 commit 5 (plan §7 W17 waterfall colour schemes expansion).
enum class WfColorScheme : int {
    Default = 0,    // AetherSDR: black → blue → cyan → green → yellow → red
    Enhanced,       // Thetis enhanced (9-band progression)
    Spectran,       // SPECTRAN
    BlackWhite,     // Grayscale
    LinLog,         // Linear in low, log in high — Thetis LinLog
    LinRad,         // Linradiance-style cool → hot
    Custom,         // User-defined custom stops (reads from AppSettings)
    ClarityBlue,    // Phase 3G-9b: narrow-band monochrome (80% navy noise floor,
                    // top 20% cyan→white signals). AetherSDR-style readability.
    Count
};

// Spectrum averaging mode. Ported from Thetis comboDispPanAveraging
// (setup.designer.cs:34835, target console.specRX.GetSpecRX(0).AverageMode).
// Thetis options: None / Recursive / Time Window / Log Recursive.
// NereusSDR names: None / Weighted / TimeWindow / Logarithmic — the
// previous single smoothing behavior (kSmoothAlpha * new + (1-a) * prev)
// corresponds to Weighted.
enum class AverageMode : int {
    None = 0,        // pass frame through unchanged
    Weighted,        // kSmoothAlpha exponential (current NereusSDR behavior)
    Logarithmic,     // log-domain exponential (matches Thetis Log Recursive)
    TimeWindow,      // approximated as slower exponential for now
    Count
};

/// Non-owning display sink. Implementations must tolerate being called from
/// the thread RadioModel runs on (the main thread today).
class ISpectrumSink {
public:
    virtual ~ISpectrumSink() = default;

    // SwrProtectionController::highSwrChanged / windBackLatchedChanged ->
    // spectrum overlay push. RadioModel.cpp, SwrProtectionController wiring.
    virtual void setHighSwrOverlay(bool active, bool foldback) = 0;

    // Phase 3G-9b smooth-default recipe, RadioModel::applyClaritySmoothDefaults.
    virtual void setWfColorScheme(WfColorScheme scheme) = 0;
    virtual void setAverageMode(AverageMode mode) = 0;
    virtual void setAverageAlpha(float alpha) = 0;
    virtual void setFillColor(const QColor& color) = 0;
    virtual void setPanFillEnabled(bool on) = 0;
    virtual void setWfAgcEnabled(bool on) = 0;
    virtual void setWfUpdatePeriodMs(int ms) = 0;
};

} // namespace NereusSDR
