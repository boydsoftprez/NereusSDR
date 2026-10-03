// =================================================================
// src/core/spectrum/DisplayFollowers.cpp  (NereusSDR)
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
//   2026-09-28 - normalizeAppliesToDetector: the 1 Hz normalise applies
//                 only to the Average, Sample and RMS detectors, as
//                 specHPSDR.cs updateNormalizePan does (R-IOS-18). J.J. Boyd
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

#include "core/spectrum/DisplayFollowers.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

// ── Noise floor ─────────────────────────────────────────────────────────

// From Thetis display.cs:917-927 [v2.10.3.13] FastAttackNoiseFloorRX1 setter.
// Also records the time — Thetis _fLastFastAttackEnabledTimeRX1
// (display.cs:4641) — so process()'s convergence-gated auto-clear
// (display.cs:5904-5908) can measure elapsed time since the last trigger.
bool NoiseFloorFollower::setFastAttack(bool on, qint64 nowMs)
{
    if (on) {
        // Stamp every trigger; consecutive band/freq/MOX events all reset
        // the elapsed clock so the gray window covers the full settling
        // period regardless of how many triggers fired.
        m_lastFastAttackMs = nowMs;
    }
    if (m_fastAttack == on) { return false; }
    m_fastAttack = on;
    return true;
}

// Source-first port of Thetis processNoiseFloor — display.cs:5866-5912 [v2.10.3.13].
// Iterates the display pixels to count + linear-sum bins below the previous
// frame's estimate, then updates the per-frame average and the smoothed
// average.  Mirrors the per-pixel accumulator that lives in Thetis's render
// loop at display.cs:5253-5258 (averageSum += 10^(dB/10), averageCount++ when
// max_copy < currentAverage), folded into one helper because NereusSDR's
// renderer doesn't share the averaging loop.
bool NoiseFloorFollower::process(const QVector<float>& src, int fps, qint64 nowMs)
{
    const int width = src.size();
    if (width <= 0) { return false; }

    // Per-pixel accumulator — Thetis display.cs:5253-5258 [v2.10.3.13]:
    //   if (!mox && max_copy < currentAverage) {
    //       averageSum += fastPow10Raw(max_copy);
    //       averageCount++;
    //   }
    // currentAverage = previous-frame fftBinAverage (the running estimate);
    // bins below it are the "quiet" ones we want to characterise.
    const float currentAverage = m_fftBinAverage;
    double averageSum = 0.0;
    int    averageCount = 0;
    for (int i = 0; i < width; ++i) {
        const float dB = src[i];
        if (dB < currentAverage) {
            averageSum += std::pow(10.0, static_cast<double>(dB) / 10.0);
            averageCount++;
        }
    }

    // Thetis default _NFsensitivity=3, clamp [0,19] (display.cs:5775+5783).
    // Match the formula exactly: int truncation of (width * sens / 20).
    const int requireSamples = qMax(1,
        (width * m_sensitivity) / 20);

    // Per-frame fftBinAverage update — display.cs:5883-5896.
    if (averageCount >= requireSamples) {
        const float linearAverage =
            static_cast<float>(averageSum / static_cast<double>(averageCount));
        const float oldLinear = std::pow(10.0f, m_fftBinAverage / 10.0f);
        const float newLinear = (linearAverage + oldLinear) * 0.5f;
        m_fftBinAverage = 10.0f *
            std::log10(static_cast<double>(newLinear) + 1e-60);
    } else {
        // Not enough quiet bins (signal-dense band, or estimate stuck below
        // the true floor).  Drift up by 1 dB/frame (3 dB in fast-attack to
        // re-acquire faster).  display.cs:5893.
        m_fftBinAverage += m_fastAttack ? 3.0f : 1.0f;
    }
    m_fftBinAverage = qBound(-200.0f, m_fftBinAverage, 200.0f);

    // Lerp smoothing — display.cs:5898-5902.
    int framesInAttack = m_fastAttack ? 0 :
        static_cast<int>((static_cast<float>(fps) / 1000.0f) * m_attackTimeMs);
    framesInAttack += 1;
    const float difference = m_lerpAverage - m_fftBinAverage;
    m_lerpAverage -= difference / static_cast<float>(framesInAttack);

    // Fast-attack convergence-gated auto-clear — display.cs:5904-5908:
    //   if (fastAttack && abs(fft - lerp) < 1.0) {
    //       float tmpDelay = Math.Max(1000, fftFillTime + ...);
    //       if (elapsed > tmpDelay) fastAttack = false;
    //   }
    // Clears the gray flag once the smoothed estimate has caught up to the
    // per-frame estimate AND at least 1 second has passed since the trigger.
    if (m_fastAttack &&
        std::abs(m_fftBinAverage - m_lerpAverage) < 1.0f)
    {
        const qint64 elapsed = nowMs - m_lastFastAttackMs;
        if (elapsed > kFastAttackMinMs) {
            m_fastAttack = false;
            return true;
        }
    }
    return false;
}

bool NoiseFloorFastAttackTrigger::observe(double frequencyHz, int band, bool mox)
{
    if (!m_primed) {
        m_primed = true;
        m_lastHz = frequencyHz;
        m_band = band;
        m_mox = mox;
        return false;
    }
    // A band change, a jump of more than 0.5 MHz, and either MOX edge,
    // as MainWindow connects PanadapterModel::bandChanged,
    // SliceModel::frequencyChanged and TransmitModel::moxChanged.
    const bool fire = band != m_band
        || std::abs(m_lastHz - frequencyHz) > kFrequencyJumpHz
        || mox != m_mox;
    m_lastHz = frequencyHz;
    m_band = band;
    m_mox = mox;
    return fire;
}

// ── Waterfall levels ────────────────────────────────────────────────────

void WaterfallLevelFollower::compose(const QVector<float>& wfPixelsDbm,
                                     const WaterfallLevelSettings& settings,
                                     float& activeLowDbm, float& activeHighDbm)
{
    if (wfPixelsDbm.isEmpty()) { return; }

    const int n = wfPixelsDbm.size();

    // Seed from persistent user values unless Clarity is the live
    // driver — Clarity writes the active levels directly and they must
    // survive between rows.  Matches the Thetis
    // "high_threshold = waterfall_high_threshold" seed at display.cs:6575
    // [v2.10.3.13].
    if (!settings.clarityActive) {
        activeLowDbm  = settings.lowDbm;
        activeHighDbm = settings.highDbm;
    }

    // AGC: one-pole follower on display-pixel min/max biases the
    // effective thresholds.  Skipped while Clarity is the driver.
    // Phase 3G-9c.
    if (settings.agc && !settings.clarityActive) {
        float mn = wfPixelsDbm[0];
        float mx = mn;
        for (int i = 1; i < n; ++i) {
            const float v = wfPixelsDbm[i];
            if (v < mn) { mn = v; }
            if (v > mx) { mx = v; }
        }
        if (!m_agcPrimed) {
            m_agcRunMin = mn;
            m_agcRunMax = mx;
            m_agcPrimed = true;
        } else {
            m_agcRunMin = kAgcAlpha * mn + (1.0f - kAgcAlpha) * m_agcRunMin;
            m_agcRunMax = kAgcAlpha * mx + (1.0f - kAgcAlpha) * m_agcRunMax;
        }
        // Phase 3G-9b: 12 dB margin for palette breathing room.
        activeLowDbm  = m_agcRunMin - kAgcMarginDb;
        activeHighDbm = m_agcRunMax + kAgcMarginDb;
    }

    // Task 2.8: NF-AGC -- override thresholds from 10th-percentile
    // noise floor + configured offset.  Runs after AGC so it wins on
    // tie; defers to Clarity.
    if (settings.noiseFloorAgc && !settings.clarityActive) {
        QVector<float> sorted = wfPixelsDbm;
        std::sort(sorted.begin(), sorted.end());
        const float nf = sorted[qBound(0, sorted.size() / 10, sorted.size() - 1)];
        const float offsetF = static_cast<float>(settings.noiseFloorAgcOffsetDb);
        activeLowDbm  = nf + offsetF;
        activeHighDbm = activeLowDbm + kNoiseFloorAgcSpanDb;
    }
}

// ── Normalise, averaging, passband ──────────────────────────────────────

// From Thetis specHPSDR.cs:325 [v2.10.3.13] NormOneHzPan, applied at the
// rendering stage so the toggle is instantly reversible without a WDSP
// channel rebuild.
// From Thetis specHPSDR.cs:288-294 [v2.10.3.15] updateNormalizePan:
//     if (norm_oneHz_pan && (det_type_pan == 2 || det_type_pan == 3 || det_type_pan == 4))
//         SpecHPSDRDLL.SetDisplayNormOneHz(disp, 0, true);
//     else
//         SpecHPSDRDLL.SetDisplayNormOneHz(disp, 0, false);
// Detector 2, 3 and 4 are Average, Sample and RMS in the pan detector list.
bool normalizeAppliesToDetector(int detector)
{
    return detector == 2 || detector == 3 || detector == 4;
}

float normalizeShiftDb(bool enabled, double binWidthHz)
{
    if (!enabled) { return 0.0f; }
    if (binWidthHz <= 0.0) { return 0.0f; }
    return -10.0f * std::log10(static_cast<float>(binWidthHz));
}

// From Thetis specHPSDR.cs:351-380 [v2.10.3.13] — AvTau / AvTauWF setters
// compute the per-side back-multiplier α via Math.Exp(-1.0 / (frame_rate * tau)).
// We mirror that exactly: τ in seconds, fps from the display's frame rate.
float averageAlphaForTimeMs(int timeMs, int fps)
{
    const double tauSec = qMax(timeMs, 1) / 1000.0;
    // α = exp(-1 / (fps × τ)). Matches Thetis specHPSDR.cs:358 / :374.
    const double a = std::exp(-1.0 / (static_cast<double>(fps) * tauSec));
    return static_cast<float>(qBound(0.0, a, 1.0));
}

// Filter-passband math in pixel space: the visible window is
// centreHz +/- spanHz/2 mapped across `n` pixels.
// From Thetis Display.cs:5453-5508 [v2.10.3.13].
std::pair<int, int> passbandPixels(int n, double centreHz, double spanHz,
                                   double loHz, double hiHz)
{
    int filterLowPx  = 0;
    int filterHighPx = n - 1;
    if (n <= 0 || spanHz <= 0.0) {
        return {filterLowPx, filterHighPx};
    }
    const double leftHz  = centreHz - spanHz / 2.0;
    const double pxWidth = spanHz / static_cast<double>(n);
    filterLowPx  = qBound(0,
        static_cast<int>(std::floor((loHz - leftHz) / pxWidth)),
        n - 1);
    filterHighPx = qBound(0,
        static_cast<int>(std::ceil((hiHz - leftHz) / pxWidth)),
        n - 1);
    return {filterLowPx, filterHighPx};
}

} // namespace NereusSDR
