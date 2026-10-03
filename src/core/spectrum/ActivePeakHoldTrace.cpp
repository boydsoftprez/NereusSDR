// =================================================================
// src/core/spectrum/ActivePeakHoldTrace.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/display.cs
//   original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-01 — Created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//   2026-09-24 — Moved unchanged from src/gui/spectrum/ to src/core/spectrum/
//                 so the Core can run it for an app's display (iPhone app
//                 Task 20, R-IOS-27). J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-28 - Hold time and the transmit gate ported from Thetis
//                 display.cs:5011, 5333-5364 [v2.10.3.15]. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
// =================================================================

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
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include "ActivePeakHoldTrace.h"

#include <algorithm>
#include <cmath>

namespace NereusSDR {

ActivePeakHoldTrace::ActivePeakHoldTrace(int nBins)
{
    // Sized, not reset: no display delay for a trace that has shown nothing.
    if (nBins > 0) {
        m_peaks.fill(-std::numeric_limits<float>::infinity(), nBins);
        m_peakTimesMs.fill(0.0, nBins);
    }
}

void ActivePeakHoldTrace::resize(int nBins)
{
    m_peaks.resize(nBins);
    m_peakTimesMs.resize(nBins);
    clear();
}

void ActivePeakHoldTrace::clear()
{
    // Reset all peaks to -infinity so the first update fills them with live data.
    // From Thetis display.cs:4527-4541 [v2.10.3.15] ResetSpectrumPeaks:
    //   maximums[i].max_dBm = float.MinValue;
    std::fill(m_peaks.begin(), m_peaks.end(),
              -std::numeric_limits<float>::infinity());
    std::fill(m_peakTimesMs.begin(), m_peakTimesMs.end(), m_frameMs);

    // From Thetis display.cs:4527-4530 [v2.10.3.15]:
    //   static public void ResetSpectrumPeaks(int rx)
    //   {
    //       delayBlobsActivePeakDisplay(rx, false);
    // and display.cs:859-877 [v2.10.3.15] delayBlobsActivePeakDisplay:
    //   m_bDelayRX1SpectrumPeaks = true;
    //   m_dPeakDelay = m_dElapsedFrameStart + 500;
    m_displayDelayed = true;
    m_displayDelayEnded = false;
    m_displayDelayUntilMs = m_frameMs + kDisplayDelayMs;
}

void ActivePeakHoldTrace::update(const QVector<float>& bins)
{
    // A display delay that ended with the last frame (tickFrame) is over
    // from this frame on.
    if (m_displayDelayEnded) {
        m_displayDelayed = false;
        m_displayDelayEnded = false;
    }

    // From Thetis display.cs:5011 [v2.10.3.15]:
    //   bSpectralPeakHold = (!local_mox || _activePeakInTxRX1) && m_bSpectralPeakHoldRX1 && ...
    // Off while transmitting without "Also in TX": nothing is raised.
    if (!active()) {
        return;
    }

    // From Thetis display.cs:5333-5341 [v2.10.3.15]:
    //   if (max >= peak.max_dBm)
    //   {
    //       peak.max_dBm = max;
    //       peak.Time = local_frame_start;
    //   }
    const int n = std::min(bins.size(), m_peaks.size());
    for (int i = 0; i < n; ++i) {
        if (!std::isfinite(m_peaks[i]) || bins[i] >= m_peaks[i]) {
            m_peaks[i] = bins[i];
            m_peakTimesMs[i] = m_frameMs;
        }
    }
}

void ActivePeakHoldTrace::tickFrame(int fps)
{
    if (fps <= 0) {
        return;
    }

    // From Thetis display.cs:5107 [v2.10.3.15]:
    //   dBmSpectralPeakFall /= (float)m_nFps;
    // and display.cs:5359-5363 [v2.10.3.15], per bin after its update:
    //   double dElapsed = local_frame_start - peak.Time;
    //   if (dElapsed > dSpectralPeakHoldDelay)
    //   {
    //       peak.max_dBm -= dBmSpectralPeakFall;
    //   }
    if (active()) {
        const float dropPerFrame = static_cast<float>(m_dropDbPerSec / fps);
        for (int i = 0; i < m_peaks.size(); ++i) {
            float& p = m_peaks[i];
            if (std::isfinite(p) && m_frameMs - m_peakTimesMs[i] > m_durationMs) {
                p -= dropPerFrame;
            }
        }
    }

    // At the end of each frame, as Thetis display.cs:4219-4221 [v2.10.3.15]:
    //   //MW0LGE_21k8
    //   processBlobsActivePeakDisplayDelay();
    //   //
    // with display.cs:847-858 [v2.10.3.15]:
    //   if (m_dElapsedFrameStart > m_dPeakDelay) { ... m_bDelayRX1SpectrumPeaks = false; }
    // The flag clears for the NEXT frame: this one was drawn without the
    // trace, and update() at the next frame's start lets it run again.
    if (m_displayDelayed && m_frameMs > m_displayDelayUntilMs) {
        m_displayDelayEnded = true;
    }

    // The next frame starts one frame later (Thetis m_dElapsedFrameStart).
    m_frameMs += 1000.0 / fps;
}

}  // namespace NereusSDR
