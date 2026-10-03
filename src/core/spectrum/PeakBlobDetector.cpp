// =================================================================
// src/core/spectrum/PeakBlobDetector.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/display.cs, original licence from
//   Thetis source is included below.
//
// Port covers:
//   display.cs:4395-4714  public API + state variables [v2.10.3.13]
//   display.cs:5453-5508  render path peak detection [v2.10.3.13]
//   display.cs:5483       decay math verbatim [v2.10.3.13]:
//       entry.max_dBm -= m_dBmPerSecondPeakBlobFall / (float)m_nFps;
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-01 — Created in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
//                 Ported from Thetis display.cs v2.10.3.13
//                 (commit 501e3f5).
//   2026-09-24 — Moved unchanged from src/gui/spectrum/ to src/core/spectrum/
//                 so the Core can run it for an app's display (iPhone app
//                 Task 20, R-IOS-27). J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-28 - The 500 ms display delay after clearMaximums(), ported
//                 from Thetis display.cs:841-877, 4219-4221, 4542-4544,
//                 5013 [v2.10.3.15]. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
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

#include "PeakBlobDetector.h"
#include <algorithm>

namespace NereusSDR {

// trigger_delta: the dB drop required to register a peak (and rise to
// register a trough).  From Thetis Display.cs:5217 [v2.10.3.13]:
//   float trigger_delta = 10; //db
// Hardcoded in Thetis; mirrored verbatim here.
static constexpr float kPeakBlobTriggerDeltaDb = 10.0f;

// Ellipse/circle radius for "near-X" slot occupancy.  From Thetis
// Display.cs:4441 [v2.10.3.13]:
//   if (entry.Enabled && p1 < 10) // 10 being the radius of the ellipse/circle
static constexpr int kPeakBlobOccupancyRadiusPx = 10;

// Disable threshold: a blob whose max_dBm has decayed at or below this
// value is dropped.  From Thetis Display.cs:5488-5491 [v2.10.3.13]:
//   else if (entry.max_dBm <= -200.0)
//     entry.Enabled = false;
//     entry.max_dBm = float.MinValue;
static constexpr float kPeakBlobDisableThresholdDb = -200.0f;

void PeakBlobDetector::ensureBlobsSized()
{
    if (m_blobs.size() == m_count) { return; }
    m_blobs.resize(m_count);
    // Newly-grown slots get the disabled / -200 default already from
    // PeakBlob's member initialisers; truncated slots are simply gone.
}

// From Thetis Display.cs:4536-4556 [v2.10.3.13] ResetBlobMaximums().
// Verbatim port of the bClear=false disable logic.  Per Thetis the
// per-frame entry point at Display.cs:5177 calls this BEFORE the
// per-pixel scan so the scan re-adds (or upgrades) blobs at the
// current frame's peaks; blobs that didn't survive the reset
// disappear cleanly.
//
// Three disable conditions (any of):
//   bClear:                 explicit clear (not used in the per-frame path)
//   !m_bBlobPeakHold:       hold is OFF -> blobs only live for one frame
//   m_bBlobPeakHold && !m_bBlobPeakHoldDrop &&
//       (now - blob.timeMs >= m_fBlobPeakHoldMS):
//       hold is ON, drop is OFF, hold time elapsed -> hard cut
//
// When hold ON + drop ON: blob is preserved here; tickFrame() handles
// the gradual decay.
void PeakBlobDetector::resetBlobMaximums()
{
    for (auto& entry : m_blobs) {
        const bool holdOff = !m_holdEnabled;
        const bool hardCutExpired = m_holdEnabled
            && !m_holdDrop
            && (m_currentTimeMs - entry.timeMs)
                   >= static_cast<qint64>(m_holdMs);
        if (holdOff || hardCutExpired) {
            entry.enabled = false;
            entry.max_dBm = kPeakBlobDisableThresholdDb;
        }
    }
}

// From Thetis Display.cs:4429-4448 [v2.10.3.13] isOccupied().
// Verbatim port: scan the m_count slots for an enabled blob whose X
// is within kPeakBlobOccupancyRadiusPx pixels of nX.
int PeakBlobDetector::isOccupied(int nX) const
{
    for (int n = 0; n < m_count && n < m_blobs.size(); ++n) {
        const PeakBlob& entry = m_blobs[n];
        if (!entry.enabled) { continue; }
        const int p1 = std::abs(nX - entry.binIndex);
        if (p1 < kPeakBlobOccupancyRadiusPx) {
            return n;
        }
    }
    return -1;
}

// From Thetis Display.cs:4456-4518 [v2.10.3.13] processMaximums().
// Verbatim port (with NereusSDR field names):
//   - If a slot is occupied near nX:
//       if dbm >= entry.max_dBm: update entry + bubble-up
//       (else: do nothing)
//   - Else (no nearby slot):
//       walk slots top-down; first slot where dbm > stored, push down
//       and insert.
void PeakBlobDetector::processMaximum(float dbm, int nX)
{
    ensureBlobsSized();

    const int nOccupiedIndex = isOccupied(nX);

    if (nOccupiedIndex >= 0) {
        PeakBlob& entry = m_blobs[nOccupiedIndex];
        if (dbm >= entry.max_dBm) {
            entry.enabled  = true;
            entry.max_dBm  = dbm;
            entry.binIndex = nX;
            entry.timeMs   = m_currentTimeMs;
            // Bubble up while higher than the entry above (descending sort).
            // From Thetis Display.cs:4476-4484 [v2.10.3.13].
            int pos = nOccupiedIndex;
            while (pos > 0 && m_blobs[pos].max_dBm > m_blobs[pos - 1].max_dBm) {
                std::swap(m_blobs[pos - 1], m_blobs[pos]);
                --pos;
            }
        }
        return;
    }

    // No nearby slot -- find the first position to insert.
    // From Thetis Display.cs:4489-4517 [v2.10.3.13].
    for (int n = 0; n < m_count; ++n) {
        if (dbm > m_blobs[n].max_dBm) {
            // Push remaining entries down by one.
            for (int nn = m_count - 1; nn > n; --nn) {
                m_blobs[nn] = m_blobs[nn - 1];
            }
            // Insert new at position n.
            m_blobs[n].enabled  = true;
            m_blobs[n].max_dBm  = dbm;
            m_blobs[n].binIndex = nX;
            m_blobs[n].timeMs   = m_currentTimeMs;
            break;
        }
    }
}

// Per-frame scan + processMaximum dispatch.  Mirrors the per-pixel
// state machine inside Thetis Display.cs:5245-5316 [v2.10.3.13]'s
// render loop -- specifically the trigger-delta peak detector that
// finds peaks AT 10 dB drop from the running max (and troughs at
// 10 dB rise from the running min).  This is fundamentally different
// from a "bins[i] > bins[i-1] && bins[i] > bins[i+1]" local-maxima
// scan: it produces the SAME peaks Thetis's UI shows because both
// use the same hysteresis state machine.
void PeakBlobDetector::clearMaximums()
{
    m_blobs.clear();
    // From Thetis display.cs:4542-4544 [v2.10.3.15]:
    //   static public void ResetBlobMaximums(int rx, bool bClear = false)
    //   {
    //       if (bClear) delayBlobsActivePeakDisplay(rx, true);
    // and display.cs:859-877 [v2.10.3.15] delayBlobsActivePeakDisplay:
    //   m_bDelayRX1Blobs = true;
    //   m_dPeakDelay = m_dElapsedFrameStart + 500;
    m_displayDelayed = true;
    m_displayDelayUntilMs = m_currentTimeMs + kDisplayDelayMs;
}

void PeakBlobDetector::update(const QVector<float>& bins,
                              int filterLowBin, int filterHighBin)
{
    // From Thetis display.cs:5013 [v2.10.3.15]:
    //   bPeakBlobs = m_bPeakBlobMaximums && !m_bDelayRX1Blobs;
    // While delayed there is no per-frame reset and no scan.
    if (!m_enabled || bins.isEmpty() || m_displayDelayed) {
        return;  // leave m_blobs untouched (cleared by setEnabled(false))
    }
    ensureBlobsSized();

    // Per-frame reset BEFORE the scan.  Mirrors Thetis Display.cs:5177
    // [v2.10.3.13] which calls ResetBlobMaximums(rx) at the top of the
    // per-pixel render loop in DrawPanadapterDX2D.  Disables blobs that
    // failed the hold/drop conditions (hard-cut when hold ON + drop OFF
    // + elapsed > holdMs; reset-every-frame when hold OFF).  Surviving
    // blobs get upgraded by processMaximum() during the scan that
    // follows.
    resetBlobMaximums();

    const int n = bins.size();

    // Filter-passband bounds (only used when m_insideOnly is true).
    const int filterLo = qBound(0, filterLowBin,  n - 1);
    const int filterHi = qBound(0, filterHighBin, n - 1);

    // State machine variables.  Names mirror Thetis's locals
    // (Display.cs:5234-5316 [v2.10.3.13]).
    float dbm_max          = -1e30f;
    int   dbm_max_xpos     = 0;
    float dbm_min          = +1e30f;
    bool  look_for_max     = true;

    for (int i = 0; i < n; ++i) {
        // Inside-filter gate: when on, ignore pixels outside the
        // filter passband.  Mirrors Thetis Display.cs:5272 [v2.10.3.13]:
        //   if (peaks_imds && (!m_bInsideFilterOnly ||
        //                      (point.X >= filter_left_x &&
        //                       point.X <= filter_right_x)))
        if (m_insideOnly && (i < filterLo || i > filterHi)) {
            continue;
        }

        const float v = bins[i];

        if (v > dbm_max) {
            dbm_max      = v;
            dbm_max_xpos = i;
        }
        if (v < dbm_min) {
            dbm_min = v;
        }

        if (look_for_max) {
            // Trigger: dBm has fallen kPeakBlobTriggerDeltaDb below the
            // running max.  Record the peak and switch to looking for
            // the next minimum.
            // From Thetis Display.cs:5287-5307 [v2.10.3.13].
            if (v < dbm_max - kPeakBlobTriggerDeltaDb) {
                processMaximum(dbm_max, dbm_max_xpos);
                dbm_min      = v;
                look_for_max = false;
            }
        } else {
            // Trigger: dBm has risen kPeakBlobTriggerDeltaDb above the
            // running min.  Record the trough boundary and resume
            // looking for the next max.
            // From Thetis Display.cs:5309-5315 [v2.10.3.13].
            if (v > dbm_min + kPeakBlobTriggerDeltaDb) {
                dbm_max      = v;
                dbm_max_xpos = i;
                look_for_max = true;
            }
        }
    }
}

// Per-frame decay + disable.  Mirrors Thetis Display.cs:5469-5493
// [v2.10.3.13] -- the per-blob block inside the render loop:
//
//   bool blob_drop = m_bBlobPeakHold && m_bBlobPeakHoldDrop;
//   for (int n = 0; n < maxblobs; n++) {
//     ref Maximums entry = ref maximums[n];
//     if (entry.Enabled) {
//       if (blob_drop) {
//         double dElapsed = local_frame_start - entry.Time;
//         if (entry.max_dBm > -200.0 && dElapsed > m_fBlobPeakHoldMS) {
//           entry.max_dBm -= m_dBmPerSecondPeakBlobFall / (float)m_nFps;
//         } else if (entry.max_dBm <= -200.0) {
//           entry.Enabled = false;
//           entry.max_dBm = float.MinValue;
//         }
//       }
//       ...
//     }
//   }
void PeakBlobDetector::tickFrame(int fps, int elapsedMs)
{
    const qint64 frameStartMs = m_currentTimeMs;
    m_currentTimeMs += elapsedMs;

    // At the end of each frame, as Thetis display.cs:4219-4221 [v2.10.3.15]:
    //   //MW0LGE_21k8
    //   processBlobsActivePeakDisplayDelay();
    //   //
    // with display.cs:847-858 [v2.10.3.15]:
    //   if (m_dElapsedFrameStart > m_dPeakDelay) { ... m_bDelayRX1Blobs = false; }
    // The next frame finds and draws blobs again.
    if (m_displayDelayed && frameStartMs > m_displayDelayUntilMs) {
        m_displayDelayed = false;
    }

    if (!m_enabled || fps <= 0) { return; }

    const bool blob_drop = m_holdEnabled && m_holdDrop;
    if (!blob_drop) { return; }

    for (auto& entry : m_blobs) {
        if (!entry.enabled) { continue; }

        const qint64 dElapsed = m_currentTimeMs - entry.timeMs;
        if (entry.max_dBm > kPeakBlobDisableThresholdDb
            && dElapsed > static_cast<qint64>(m_holdMs)) {
            entry.max_dBm -= static_cast<float>(m_fallDbPerSec
                                                / static_cast<double>(fps));
        } else if (entry.max_dBm <= kPeakBlobDisableThresholdDb) {
            entry.enabled = false;
            entry.max_dBm = kPeakBlobDisableThresholdDb;
        }
    }
}

}  // namespace NereusSDR
