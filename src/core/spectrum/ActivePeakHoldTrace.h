#pragma once

// =================================================================
// src/core/spectrum/ActivePeakHoldTrace.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/display.cs
//   original licence from Thetis source is included below
//
// NereusSDR-original class structure. Constants and logic reference
// Thetis display.cs: m_bActivePeakHold and associated peak-hold
// rendering.  Thetis stores the trace as a per-bin max array
// (display.cs:~4750) and decays it in the display timer; NereusSDR
// mirrors that pattern here with an explicit tickFrame() step.
//
// Design decision Q14.1 (locked): rendered as a separate pass on
// SpectrumWidget, not composited with the main trace, for
// architectural flexibility.
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
//                 display.cs [v2.10.3.15]: a refreshed peak holds for the
//                 hold time before it falls, and while this receiver
//                 transmits without "Also in TX" the trace neither updates,
//                 decays nor draws. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-28 - The 500 ms display delay after a reset ported from Thetis
//                 display.cs:841-877, 4219-4221 [v2.10.3.15]
//                 (delayBlobsActivePeakDisplay / processBlobsActivePeakDisplayDelay).
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
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

#include <QVector>
#include <limits>

namespace NereusSDR {

/// Per-bin peak-hold trace with configurable decay and TX-state gating.
///
/// State machine (Thetis display.cs:5011, 5333-5364 [v2.10.3.15]):
///   - active(): enabled && (!txActive || onTx); an inactive trace neither
///     updates, decays nor draws (bSpectralPeakHold false)
///   - update(currentBins): where current[i] >= peak[i], peak[i] = current[i]
///     and the bin's time is the current frame's
///   - tickFrame(fps): a bin whose time is more than durationMs old falls by
///     dropDbPerSec / fps, then the frame clock advances one frame
///   - clear(): reset peaks to -∞ and start the 500 ms display delay, during
///     which the trace is inactive (display.cs:4527-4530, 859-877)
///
/// Rendered as a separate trace on SpectrumWidget (Q14.1: separate pass for
/// architectural flexibility, not composited with main spectrum trace).
///
/// From Thetis display.cs m_bActivePeakHold [v2.10.3.13] — per-bin maximum
/// tracking with decay, mirrored to C++20/Qt6.
class ActivePeakHoldTrace {
public:
    explicit ActivePeakHoldTrace(int nBins = 0);
    void resize(int nBins);

    // ---- Configuration ----

    void setEnabled(bool e)             { m_enabled = e; }
    // Hold duration before decay begins (ms): a bin refreshed at frame time
    // T falls only once the frame clock is more than this past T.
    // From Thetis display.cs:746-750, 5360-5363 [v2.10.3.15]
    // (SpectralPeakHoldDelayRX1; dElapsed > dSpectralPeakHoldDelay).
    void setDurationMs(int ms)          { m_durationMs = ms; }
    // From Thetis Display.cs:4697 [v2.10.3.13] m_dBmPerSecondPeakBlobFall = 6.0f
    // — reused as the default decay rate for the Active Peak Hold trace.
    void setDropDbPerSec(double r)      { m_dropDbPerSec = r; }
    void setFill(bool f)                { m_fill = f; }
    // When true ("Also in TX"), the trace keeps running while this receiver
    // transmits. When false (default), the trace is off while transmitting:
    // no update, no decay, nothing drawn. From Thetis display.cs:4941-4945,
    // 5011 [v2.10.3.15]:
    //   bSpectralPeakHold = (!local_mox || _activePeakInTxRX1) && m_bSpectralPeakHoldRX1 && ...
    void setOnTx(bool o)                { m_onTx = o; }
    // This receiver is transmitting (Thetis local_mox).
    void setTxActive(bool t)            { m_txActive = t; }
    bool txActive() const               { return m_txActive; }
    bool onTx() const                   { return m_onTx; }
    /// Enabled, past the display delay after a reset, and not switched off
    /// by transmitting (Thetis display.cs:5011 [v2.10.3.15]:
    /// ... && m_bSpectralPeakHoldRX1 && !m_bDelayRX1SpectrumPeaks).
    bool active() const                 {
        return m_enabled && !m_displayDelayed && (!m_txActive || m_onTx);
    }
    /// Inside the display delay that follows a reset.
    bool displayDelayed() const         { return m_displayDelayed; }
    /// From Thetis display.cs:876 [v2.10.3.15]:
    ///   m_dPeakDelay = m_dElapsedFrameStart + 500;
    static constexpr double kDisplayDelayMs = 500.0;

    // ---- Per-frame operations ----

    /// Raise each peak bin to max(peak[i], currentBins[i]) and stamp the
    /// raised bins with the current frame's time. No-op when !active().
    void update(const QVector<float>& currentBins);

    /// When active(), decay the finite peaks older than the hold time by
    /// (dropDbPerSec / fps) dB. Then mark a display delay whose time has
    /// passed as over from the next frame's update(), and advance the frame
    /// clock by one frame (both run inactive too, as Thetis's frame clock
    /// does). No-op when fps <= 0.
    void tickFrame(int fps);

    /// Reset all peaks to -infinity and start the display delay
    /// (Thetis ResetSpectrumPeaks).
    void clear();

    // ---- Accessors ----

    bool   enabled() const              { return m_enabled; }
    bool   fill() const                 { return m_fill; }
    int    durationMs() const           { return m_durationMs; }
    double dropDbPerSec() const         { return m_dropDbPerSec; }
    float  peak(int bin) const          {
        return (bin >= 0 && bin < m_peaks.size())
               ? m_peaks[bin]
               : -std::numeric_limits<float>::infinity();
    }
    const QVector<float>& peaks() const { return m_peaks; }
    int    size() const                 { return m_peaks.size(); }

private:
    bool   m_enabled       = false;
    // From Thetis Display.cs:4599 [v2.10.3.13] m_fBlobPeakHoldMS = 500 — similar
    // hold pattern; Active Peak Hold defaults to 2000 ms per SpectrumPeaksPage.
    int    m_durationMs    = 2000;
    // From Thetis Display.cs:4697 [v2.10.3.13] m_dBmPerSecondPeakBlobFall = 6.0f
    double m_dropDbPerSec  = 6.0;
    bool   m_fill          = false;
    bool   m_onTx          = false;
    bool   m_txActive      = false;
    QVector<float> m_peaks;
    // Frame time each bin was last raised (Thetis Maximums.Time), and the
    // frame clock (Thetis local_frame_start), in ms.
    QVector<double> m_peakTimesMs;
    double m_frameMs       = 0.0;
    // Thetis m_bDelayRX1SpectrumPeaks / m_dPeakDelay (display.cs:843-845).
    bool   m_displayDelayed = false;
    bool   m_displayDelayEnded = false;   // set at a frame's end, applied at the next
    double m_displayDelayUntilMs = 0.0;
};

}  // namespace NereusSDR
