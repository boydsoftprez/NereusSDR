#pragma once
// =================================================================
// src/core/spectrum/DisplayFollowers.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/display.cs, original licence from Thetis
//   source is included below
//   Project Files/Source/Console/HPSDR/specHPSDR.cs, original licence from
//   Thetis source is included below
//
// The display computations the desktop's SpectrumWidget runs on each
// reduced spectrum and waterfall row, moved here unchanged so the Core runs
// the same code for an app's display (iPhone app Task 20, R-IOS-27): the
// noise-floor line (display.cs processNoiseFloor), the waterfall's
// automatic levels, the 1 Hz normalise shift (specHPSDR.cs NormOneHzPan),
// the averaging constant (specHPSDR.cs AvTau / AvTauWF), the calibration
// offset's range and the peak-blob passband in pixels.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-24 — Moved from src/gui/SpectrumWidget.cpp for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code. The logic is unchanged; state that
//                 lived in SpectrumWidget members lives in the classes
//                 below, and the clock is passed in.
//   2026-09-24 — NoiseFloorFastAttackTrigger: the desktop's fast-attack
//                 triggers (a band change, a 0.5 MHz jump, a MOX edge) for
//                 the Core's display extras (iPhone app Task 20). J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-28 - normalizeAppliesToDetector (specHPSDR.cs updateNormalizePan):
//                 the 1 Hz normalise follows the pan detector. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
// =================================================================

// --- From display.cs ---
//=================================================================
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
// 02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to:
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2025 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//


// --- From specHPSDR.cs ---
/*
*
* Copyright (C) 2010-2018  Doug Wigley 
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <QVector>
#include <QtGlobal>

#include <utility>

namespace NereusSDR {

// ── Noise floor ─────────────────────────────────────────────────────────

/// Per-frame and smoothed noise-floor estimates of one display.
/// Source-first port of Thetis processNoiseFloor, display.cs:5866-5912
/// [v2.10.3.13], with its per-pixel accumulator (display.cs:5253-5258)
/// folded in, as SpectrumWidget ran it before the move.
class NoiseFloorFollower {
public:
    // From Thetis display.cs:4633-4636 [v2.10.3.13] — both estimates start
    // at the bottom of the scale.
    static constexpr float kInitialDbm = -200.0f;
    // From Thetis display.cs:4638 [v2.10.3.13] m_fAttackTimeInMSForRX1=2000.
    static constexpr float kDefaultAttackTimeMs = 2000.0f;
    // From Thetis display.cs:5775 [v2.10.3.13] _NFsensitivity = 3,
    // clamped to [0, 19] in its setter.
    static constexpr int kDefaultSensitivity = 3;
    // display.cs:5906 Math.Max(1000, ...): fast attack clears no sooner.
    static constexpr qint64 kFastAttackMinMs = 1000;
    // From Thetis display.cs:5763-5773 [v2.10.3.13] _fNFshiftDBM setter:
    //   if (t < -12f) t = -12f;
    //   if (t > 12f) t = 12f;
    static constexpr float kShiftMinDb = -12.0f;
    static constexpr float kShiftMaxDb = 12.0f;

    static float clampShiftDb(float db) { return qBound(kShiftMinDb, db, kShiftMaxDb); }

    /// From Thetis display.cs:917-927 [v2.10.3.13] FastAttackNoiseFloorRX1
    /// setter. Turning it on stamps nowMs (Thetis
    /// _fLastFastAttackEnabledTimeRX1, display.cs:4641) every time, so
    /// consecutive triggers each restart the settling window. Returns
    /// whether the flag changed.
    bool setFastAttack(bool on, qint64 nowMs);
    bool fastAttack() const { return m_fastAttack; }

    /// One frame of display pixels (the measurement row: undented).
    /// Returns true when fast attack cleared itself on this frame
    /// (display.cs:5904-5908), so a caller that draws the flag repaints.
    bool process(const QVector<float>& pixels, int fps, qint64 nowMs);

    /// Thetis m_fFFTBinAverageRX1: this frame's estimate.
    float fftBinAverage() const { return m_fftBinAverage; }
    /// Thetis m_fLerpAverageRX1: the smoothed estimate the line is drawn at.
    float lerpAverage() const { return m_lerpAverage; }
    /// Seeds the smoothed estimate (SpectrumWidget's 3D floor test seam).
    void setLerpAverage(float dbm) { m_lerpAverage = dbm; }

private:
    float m_fftBinAverage {kInitialDbm};
    float m_lerpAverage {kInitialDbm};
    float m_attackTimeMs {kDefaultAttackTimeMs};
    int m_sensitivity {kDefaultSensitivity};
    bool m_fastAttack {false};
    qint64 m_lastFastAttackMs {0};
};

/// When a display's noise floor goes to fast attack, as the desktop's
/// MainWindow wires it for its own pan: a band change, a tune of more than
/// half a megahertz, and either edge of MOX. The Core runs it for an app's
/// display from the slice's frequency and the MOX state it samples.
class NoiseFloorFastAttackTrigger {
public:
    // From Thetis display.cs:910 [v2.10.3.15] OnCentreFrequencyChanged:
    //   if (Math.Abs(oldFreq - newFreq) > 0.5) FastAttackNoiseFloorRX1 = true;
    // (MHz there; the desktop's kFastAttackFreqJumpHz).
    static constexpr double kFrequencyJumpHz = 500000.0;

    /// Returns true when this observation fires fast attack. The first one
    /// only records the state.
    bool observe(double frequencyHz, int band, bool mox);

private:
    bool m_primed {false};
    double m_lastHz {0.0};
    int m_band {0};
    bool m_mox {false};
};

// ── Waterfall levels ────────────────────────────────────────────────────

/// What sets the waterfall's low and high levels for one row.
struct WaterfallLevelSettings {
    /// The operator's own levels (Thetis waterfall_low/high_threshold);
    /// SpectrumWidget's defaults.
    float lowDbm {-122.0f};
    float highDbm {-62.0f};
    /// The one-pole follower on each row's minimum and maximum.
    bool agc {false};
    /// Levels from the row's 10th-percentile floor plus an offset.
    bool noiseFloorAgc {false};
    int noiseFloorAgcOffsetDb {0};
    /// Clarity writes the levels itself between rows; nothing here moves
    /// them while it does.
    bool clarityActive {false};
};

/// The waterfall's automatic levels, as SpectrumWidget composed them for
/// each receive row (composeWaterfallActiveThresholds, issue #230).
/// Thetis-faithful seed per display.cs:6575-6594 [v2.10.3.13].
class WaterfallLevelFollower {
public:
    // Phase 3G-9c follower pole and Phase 3G-9b palette margin.
    static constexpr float kAgcAlpha = 0.05f;
    static constexpr float kAgcMarginDb = 12.0f;
    // Task 2.8: NF-AGC span above its low level, and its offset range.
    static constexpr float kNoiseFloorAgcSpanDb = 60.0f;
    static constexpr int kNoiseFloorAgcOffsetMinDb = -60;
    static constexpr int kNoiseFloorAgcOffsetMaxDb = 60;

    static int clampNoiseFloorAgcOffsetDb(int db)
    {
        return qBound(kNoiseFloorAgcOffsetMinDb, db, kNoiseFloorAgcOffsetMaxDb);
    }

    /// Forget the follower's running envelope; the next AGC row primes it.
    void resetAgc() { m_agcPrimed = false; }
    bool agcPrimed() const { return m_agcPrimed; }

    /// Composes one row's levels into activeLow / activeHigh, which hold the
    /// levels in force (Clarity's included) on entry.
    void compose(const QVector<float>& rowDbm, const WaterfallLevelSettings& settings,
                 float& activeLowDbm, float& activeHighDbm);

private:
    float m_agcRunMin {0.0f};
    float m_agcRunMax {0.0f};
    bool m_agcPrimed {false};
};

// ── Normalise, averaging, calibration, passband ─────────────────────────

/// The 1 Hz normalise shift: -10*log10(binWidthHz) when on, 0 otherwise,
/// or 0 for a bin width that is not positive. Mirrors Thetis
/// SetDisplayNormOneHz (specHPSDR.cs:325 NormOneHzPan) at render time.
float normalizeShiftDb(bool enabled, double binWidthHz);

/// Whether the 1 Hz normalise applies with this spectrum detector (0 Peak,
/// 1 Rosenfell, 2 Average, 3 Sample, 4 RMS): only 2, 3 and 4. Thetis
/// specHPSDR.cs updateNormalizePan passes the flag to WDSP for those three
/// and turns it off for the others.
bool normalizeAppliesToDetector(int detector);

/// The per-frame averaging constant for a time constant: α =
/// exp(-1 / (fps × τ)), τ in seconds, clamped to [0, 1]. Thetis AvTau /
/// AvTauWF, specHPSDR.cs:351-380 [v2.10.3.13]. A time under 1 ms counts as
/// 1 ms.
float averageAlphaForTimeMs(int timeMs, int fps);

/// The average-time range the desktop's Setup offers, in ms.
inline constexpr int kAverageTimeMinMs = 10;
inline constexpr int kAverageTimeMaxMs = 9999;
inline int clampAverageTimeMs(int ms) { return qBound(kAverageTimeMinMs, ms, kAverageTimeMaxMs); }

/// The display calibration offset's range in dB (Thetis
/// Display.RX1DisplayCalOffset, display.cs:1372).
inline constexpr float kCalibrationOffsetMinDb = -30.0f;
inline constexpr float kCalibrationOffsetMaxDb = 30.0f;
inline float clampCalibrationOffsetDb(float db)
{
    return qBound(kCalibrationOffsetMinDb, db, kCalibrationOffsetMaxDb);
}

/// The inclusive pixel range of a filter passband [loHz, hiHz] on a row of
/// `pixels` pixels spanning centreHz ± spanHz/2, clamped to the row: the
/// peak blobs' "inside the filter only" range (display.cs:5453-5508
/// [v2.10.3.13], in pixel space). The whole row for a span that is not
/// positive or an empty row.
std::pair<int, int> passbandPixels(int pixels, double centreHz, double spanHz,
                                   double loHz, double hiHz);

} // namespace NereusSDR
