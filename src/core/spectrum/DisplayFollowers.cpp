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
//   2026-10-10 - Waterfall AGC and NF-AGC the Thetis way (display.cs
//                 6190-6209, 6463-6478, 7640-7667): only the low edge
//                 follows, NF-AGC is an option under AGC on the display's
//                 noise floor, and the follower freezes while keyed, holds
//                 after an un-key and saves and restores its run level. The
//                 noise-floor fast attack takes a minimum length. J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-23 - The noise floor takes no estimate from a frame with no
//                 data (empty, or every pixel at the FFT's -200 dBm floor
//                 or not finite), and it seeds both estimates from the
//                 first frame that has data, at the level the estimate
//                 settles at on it, instead of climbing from -200 dBm. A display started or resumed
//                 drew ordinary noise hot for about eight seconds. J.J.
//                 Boyd (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-10-23 - The seed turns fast attack on, as Thetis does when it
//                 creates the display (display.cs:3698-3700). J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-23 - NF-AGC's run level takes nothing from rows before the
//                 floor has an estimate and starts at the floor's target on
//                 the row it first has one; a fast attack resumes after a
//                 hold for what its minimum had left. J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic Claude Code.
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
#include <iterator>

namespace NereusSDR {

// ── Noise floor ─────────────────────────────────────────────────────────

// From Thetis display.cs:917-927 [v2.10.3.13] FastAttackNoiseFloorRX1 setter.
// Also records the time — Thetis _fLastFastAttackEnabledTimeRX1
// (display.cs:4641) — so process()'s convergence-gated auto-clear
// (display.cs:5904-5908) can measure elapsed time since the last trigger.
bool NoiseFloorFollower::setFastAttack(bool on, qint64 nowMs, qint64 minMs)
{
    // From Thetis display.cs:928-933 [v2.10.3.15], the setter clears the
    // good flag on any set:
    //   m_bNoiseFloorGoodRX1 = false;
    //   if(value) _fLastFastAttackEnabledTimeRX1 = _high_perf_timer.ElapsedMsec;
    //   m_bFastAttackNoiseFloorRX1 = value;
    m_good = false;
    if (on) {
        // Stamp every trigger; consecutive band/freq/MOX events all reset
        // the elapsed clock so the gray window covers the full settling
        // period regardless of how many triggers fired.
        //
        // NereusSDR, based on Thetis display.cs:5912 [v2.10.3.15]: Thetis
        // has one settle time for every trigger. Here the trigger passes its
        // minimum (a MOX edge passes the un-key hold length), and a trigger
        // that would end before an earlier one still running does not
        // shorten it.
        const qint64 wanted = std::max<qint64>(0, minMs);
        const bool earlierRunsLonger =
            m_fastAttack && m_lastFastAttackMs + m_fastAttackMinMs > nowMs + wanted;
        if (!earlierRunsLonger) {
            m_lastFastAttackMs = nowMs;
            m_fastAttackMinMs = wanted;
        }
    }
    if (m_fastAttack == on) { return false; }
    m_fastAttack = on;
    return true;
}

bool NoiseFloorFollower::resumeFastAttackAfterHold(qint64 holdStartMs, qint64 nowMs)
{
    if (!m_fastAttack) { return false; }
    const qint64 ranBeforeHold = std::max<qint64>(0, holdStartMs - m_lastFastAttackMs);
    m_fastAttackMinMs = std::max<qint64>(0, m_fastAttackMinMs - ranBeforeHold);
    m_lastFastAttackMs = nowMs;
    m_good = false;
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

    // NereusSDR, not in Thetis: a pixel at or below -200 dBm (the floor
    // FFTEngine writes for a bin with no power) or not finite is no
    // measurement. A frame with no other pixel is a display that has no
    // data yet, or a stream that paused; it would pull the estimate down to
    // the bottom of the scale, so it is skipped.
    const auto isMeasured = [](float dB) { return std::isfinite(dB) && dB > kInitialDbm; };
    const int measuredCount = static_cast<int>(
        std::count_if(src.cbegin(), src.cend(), isMeasured));
    if (measuredCount == 0) { return false; }

    // NereusSDR, based on Thetis display.cs:4639-4642 [v2.10.3.15] (both
    // estimates start at -200): Thetis climbs from there, 1 dB a frame out
    // of fast attack, and the line lags it by the attack time, so a display
    // started or resumed reads far below the band for seconds and NF-AGC
    // maps ordinary noise hot. Here both estimates start from the first
    // frame that has data, at the level the estimate below settles at on
    // such a frame: it falls while at least requireSamples measured pixels
    // are below it and climbs while fewer are, so it holds where that many
    // are, the pixel with that many below it. On a noisy band the median
    // sits above that. The estimate runs from the next frame.
    if (!m_seeded) {
        QVector<float> measured;
        measured.reserve(measuredCount);
        std::copy_if(src.cbegin(), src.cend(), std::back_inserter(measured), isMeasured);
        const int quietNeeded = qBound(0, (measuredCount * m_sensitivity) / 20,
                                       measuredCount - 1);
        const auto settlesAt = measured.begin() + quietNeeded;
        std::nth_element(measured.begin(), settlesAt, measured.end());
        m_fftBinAverage = qBound(kInitialDbm, *settlesAt, 200.0f);
        m_lerpAverage = m_fftBinAverage;
        m_seeded = true;
        // From Thetis display.cs:3698-3700 [v2.10.3.15], when the display
        // is created:
        //   //[2.10.1.0] MW0LGE spectrum/bitmaps may be cleared or bad, so wait to settle
        //   FastAttackNoiseFloorRX1 = true;
        //   if(RX2Enabled) FastAttackNoiseFloorRX2 = true;
        // NereusSDR: here at the seed, the first frame with data, so a
        // display started, resumed or reset settles in fast attack from
        // there: 3 dB a frame up and no attack lag, and not settled (NF-AGC
        // and the grid wait) for at least the trigger's minimum. The setter
        // clears the good flag; a longer trigger already running is kept.
        return setFastAttack(true, nowMs);
    }

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
        if (!isMeasured(dB)) { continue; }
        if (dB < currentAverage) {
            averageSum += std::pow(10.0, static_cast<double>(dB) / 10.0);
            averageCount++;
        }
    }

    // Thetis default _NFsensitivity=3, clamp [0,19] (display.cs:5775+5783).
    // Match the formula exactly: int truncation of (width * sens / 20).
    // NereusSDR: the width is the measured pixels, since pixels with no
    // data cannot be quiet ones.
    const int requireSamples = qMax(1,
        (measuredCount * m_sensitivity) / 20);

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
    // per-frame estimate AND at least the trigger's minimum (1 second
    // unless it passed its own) has passed since the trigger.
    bool cleared = false;
    if (m_fastAttack &&
        std::abs(m_fftBinAverage - m_lerpAverage) < 1.0f)
    {
        const qint64 elapsed = nowMs - m_lastFastAttackMs;
        if (elapsed > m_fastAttackMinMs) {
            m_fastAttack = false;
            cleared = true;
        }
    }
    // From Thetis display.cs:5400-5404 [v2.10.3.15], after the estimate on
    // each frame that is not in fast attack:
    //   if (!m_bFastAttackNoiseFloorRX1 && !bPreviousRX1)
    //   {
    //       m_fNoiseFloorRX1 = m_fLerpAverageRX1 + _fNFshiftDBM;
    //       m_bNoiseFloorGoodRX1 = true;
    //   }
    if (!m_fastAttack) {
        m_good = true;
    }
    return cleared;
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

void WaterfallLevelFollower::beginHold(qint64 lengthMs, qint64 nowMs)
{
    // NereusSDR, based on Thetis display.cs:2856-2858 [v2.10.3.15]:
    //   //delay waterfall agc
    //   _rx1_no_agc_duration = _high_perf_timer.ElapsedMsec + _fft_fill_timeRX1 + ((m_nFps / 1000f) * 2); // 2 extra frames
    //   _ignore_waterfall_rx1_agc = true;
    // Thetis delays the AGC for one fill time. NereusSDR's caller passes
    // the one un-key hold length that Clarity and the noise-floor fast
    // attack also use (moxHoldLengthMs, which says why the "2 extra frames"
    // term is not ported).
    m_holdActive = true;
    m_holdEndMs = nowMs + std::max<qint64>(0, lengthMs);
}

void WaterfallLevelFollower::restoreAgcRunLevel(std::optional<float> saved)
{
    // From Thetis display.cs:2810-2820 [v2.10.3.15]
    // getWaterfallCachedPreviousMinOrFloor: the level cached for the band,
    // or
    //   return WATERFALL_AGC_RESTART_FLOOR_DBM;
    m_agcRunMin = saved.value_or(kAgcRestartFloorDbm);
    m_agcPrimed = true;
    // The low level kept for a key belonged to the run level this replaces.
    m_hasAgcLow = false;
}

void WaterfallLevelFollower::compose(const QVector<float>& wfPixelsDbm,
                                     const WaterfallLevelSettings& settings,
                                     float& activeLowDbm, float& activeHighDbm,
                                     qint64 nowMs)
{
    if (wfPixelsDbm.isEmpty()) { return; }

    const int n = wfPixelsDbm.size();

    // Seed from persistent user values unless Clarity is the live
    // driver — Clarity writes the active levels directly and they must
    // survive between rows.  Matches the Thetis
    // "high_threshold = waterfall_high_threshold" seed at display.cs:6575
    // [v2.10.3.13].
    //
    // The high level is never moved after this: with AGC on, only the low
    // edge follows.
    // From Thetis display.cs:6463 [v2.10.3.15]:
    //   high_threshold = waterfall_high_threshold;
    if (!settings.clarityActive) {
        activeLowDbm  = settings.lowDbm;
        activeHighDbm = settings.highDbm;
    }

    // AGC, with NF-AGC as an option under it. AGC off leaves the
    // operator's Low and High, whatever NF-AGC holds.
    // From Thetis display.cs:6464-6479 [v2.10.3.15]:
    //   if (rx1_waterfall_agc && !m_bRX1_spectrum_thresholds)
    //   {
    //       if (useNoiseFloorCompensation)
    //       {
    //           low_threshold = useSettledNoiseFloorCompensation ? noiseFloorCompensationTarget : _RX1waterfallPreviousMinValue;
    //       }
    //       else
    //       {
    //           low_threshold = _RX1waterfallPreviousMinValue;
    //           low_threshold -= m_fWaterfallAGCOffsetRX1;
    //       }
    //   }
    //   else
    //   {
    //       low_threshold = waterfall_low_threshold;
    //   }
    // Skipped while Clarity is the driver.
    if (!settings.agc || settings.clarityActive) { return; }

    // While keyed nothing is estimated: the rows of a key draw with the low
    // level last produced (setKeyed says how this differs from Thetis).
    if (m_keyed && m_hasAgcLow) {
        activeLowDbm = m_agcLow;
        return;
    }

    // NereusSDR, based on Thetis display.cs:6207 [v2.10.3.15]:
    //   return rx == 1 ? m_fLerpAverageRX1 - m_fWaterfallAGCOffsetRX1
    // Thetis subtracts its offset. NereusSDR adds its stored offset, so the
    // values already stored, the phone, and older windows and Cores keep
    // their meaning: a negative offset puts the low level under the floor.
    const float noiseFloorTarget = settings.noiseFloorDbm + settings.noiseFloorAgcOffsetDb;

    // From Thetis display.cs:7640-7651 [v2.10.3.15]:
    //   bool bIgnoreAgc = (rx == 1 && _ignore_waterfall_rx1_agc && (_high_perf_timer.ElapsedMsec < _rx1_no_agc_duration)) ||
    //   ...
    //   if (!local_mox && !bIgnoreAgc)
    const bool ignoreAgc = m_keyed || inHold(nowMs);
    if (m_holdActive && nowMs >= m_holdEndMs) {
        m_holdActive = false;
    }

    if (ignoreAgc) {
        // No row of a key or a hold may prime the run level: with nothing
        // saved it starts from the restart floor, as restoreAgcRunLevel.
        if (!m_agcPrimed) {
            m_agcRunMin = kAgcRestartFloorDbm;
            m_agcPrimed = true;
        }
    } else if (settings.noiseFloorAgc) {
        if (!m_agcPrimed) {
            // From Thetis display.cs:3071 [v2.10.3.15]:
            //   private static float _RX1waterfallPreviousMinValue = WATERFALL_AGC_RESTART_FLOOR_DBM;
            m_agcRunMin = kAgcRestartFloorDbm;
            m_agcPrimed = true;
        }
        // NereusSDR, not in Thetis (PR 628): a floor with no estimate yet
        // reads -200 dBm, and the fixed restart floor ignores the band.
        // The seed's fast attack keeps the floor unsettled for a second, so
        // the run level is the low level for those rows. The run level
        // takes nothing from a row before the floor has an estimate, and
        // starts at the floor's target on the row it first has one.
        if (settings.noiseFloorSeeded && !m_floorSeenSeeded) {
            m_agcRunMin = noiseFloorTarget;
        }
        // From Thetis display.cs:7653-7667 [v2.10.3.15]:
        //   //if (rx == 1)
        //   //    _RX1waterfallPreviousMinValue = (((_RX1waterfallPreviousMinValue * 8) + (waterfall_minimum * 2)) / 10) + 1; //wfagc
        //   //else
        //   //    _RX2waterfallPreviousMinValue = (((_RX2waterfallPreviousMinValue * 8) + (waterfall_minimum * 2)) / 10) + 1; //wfagc
        //   ...
        //   if (rx1_waterfall_agc && !m_bRX1_spectrum_thresholds && useNoiseFloorCompensation)
        //   {
        //       _RX1waterfallPreviousMinValue = (_RX1waterfallPreviousMinValue * 0.6f) + (noiseFloorCompensationTarget * 0.4f);
        //   }
        if (settings.noiseFloorSeeded) {
            m_agcRunMin = m_agcRunMin * kNoiseFloorAgcKeepWeight
                + noiseFloorTarget * kNoiseFloorAgcTargetWeight;
        }
        m_floorSeenSeeded = settings.noiseFloorSeeded;
    } else {
        // AGC without NF-AGC keeps NereusSDR's own follower on the row's
        // minimum (Phase 3G-9c). NereusSDR, based on Thetis display.cs:7666
        // [v2.10.3.15]:
        //   _RX1waterfallPreviousMinValue = (_RX1waterfallPreviousMinValue * 0.6f) + (waterfall_minimum * 0.4f);
        // The slower pole, the first row's prime and the margin are kept:
        // AGC without NF-AGC changes only in leaving the high level alone.
        float mn = wfPixelsDbm[0];
        for (int i = 1; i < n; ++i) {
            const float v = wfPixelsDbm[i];
            if (v < mn) { mn = v; }
        }
        if (!m_agcPrimed) {
            m_agcRunMin = mn;
            m_agcPrimed = true;
        } else {
            m_agcRunMin = kAgcAlpha * mn + (1.0f - kAgcAlpha) * m_agcRunMin;
        }
        // A run level from the rows: NF-AGC turned on later carries on from it.
        m_floorSeenSeeded = settings.noiseFloorSeeded;
    }

    if (settings.noiseFloorAgc) {
        // From Thetis display.cs:6468 [v2.10.3.15] (quoted above): the
        // settled noise floor, or the run level until there is one. A floor
        // read while keyed is not a receive floor (Thetis: !local_mox,
        // display.cs:6407 [v2.10.3.15]).
        activeLowDbm = (settings.noiseFloorSettled && !m_keyed) ? noiseFloorTarget : m_agcRunMin;
    } else {
        // Phase 3G-9b: 12 dB margin for palette breathing room.
        activeLowDbm = m_agcRunMin - kAgcMarginDb;
    }
    m_agcLow = activeLowDbm;
    m_hasAgcLow = true;
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
