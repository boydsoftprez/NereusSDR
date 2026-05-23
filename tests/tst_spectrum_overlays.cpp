// =================================================================
// tests/tst_spectrum_overlays.cpp  (NereusSDR)
// =================================================================
//
// Task 2.3 — Spectrum Defaults overlay group + decimation wire-up.
// Tests for SpectrumWidget overlay setters and FFTEngine decimation.
//
// Ported from Thetis source:
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-01 — Ported in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

//=================================================================
// display.cs / console.cs
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
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                   //
//============================================================================================//

// Tests use QApplication (SpectrumWidget is a QWidget subclass).

#include <QtTest/QtTest>
#include <QApplication>

#include "gui/SpectrumWidget.h"
#include "core/FFTEngine.h"

using namespace NereusSDR;

// ---------------------------------------------------------------------------
// TestSpectrumOverlays
// ---------------------------------------------------------------------------
class TestSpectrumOverlays : public QObject
{
    Q_OBJECT

private slots:

    // ── formatCursorFreq ───────────────────────────────────────────────────
    //
    // (Earlier `showMHzOnCursor` toggle retired by main commit 4651e51 —
    // formatCursorFreq always returns MHz format now.  The visibility-only
    // toggle moved to `m_showCursorFreq` driven from the SpectrumOverlayPanel
    // button + Setup → Display → Spectrum Defaults checkbox.  See
    // SpectrumWidget.h formatCursorFreq comment.)

    void format_cursor_freq_always_returns_mhz()
    {
        SpectrumWidget w;
        const QString s = w.formatCursorFreq(7173500.0);
        // Format must include "MHz" and the 4-decimal MHz value.
        QVERIFY(s.contains(QStringLiteral("MHz")));
        QVERIFY(s.contains(QStringLiteral("7.1735")));
    }

    // ── ShowBinWidth / binWidthHz ───────────────────────────────────────────

    // From Thetis setup.cs:7061 [v2.10.3.13] lblDisplayBinWidth.
    void bin_width_hz_default_is_nonzero()
    {
        // SpectrumWidget default: m_sampleRateHz=768000, m_fullLinearBins
        // is empty so binWidthHz uses a fallback FFT size of 4096.
        SpectrumWidget w;
        // With no FFT data, binWidthHz returns 768000 / 4096 ≈ 187.5 Hz.
        const double bw = w.binWidthHz();
        QVERIFY(bw > 0.0);
    }

    void show_bin_width_roundtrip()
    {
        SpectrumWidget w;
        QCOMPARE(w.showBinWidth(), false);
        w.setShowBinWidth(true);
        QCOMPARE(w.showBinWidth(), true);
        w.setShowBinWidth(false);
        QCOMPARE(w.showBinWidth(), false);
    }

    // ── ShowNoiseFloor ─────────────────────────────────────────────────────

    // From Thetis display.cs:2304 [v2.10.3.13] m_bShowNoiseFloorDBM.
    void show_noise_floor_roundtrip()
    {
        SpectrumWidget w;
        QCOMPARE(w.showNoiseFloor(), false);
        w.setShowNoiseFloor(true);
        QCOMPARE(w.showNoiseFloor(), true);
    }

    void noise_floor_position_roundtrip()
    {
        SpectrumWidget w;
        w.setShowNoiseFloorPosition(SpectrumWidget::OverlayPosition::TopLeft);
        QCOMPARE(w.showNoiseFloorPosition(), SpectrumWidget::OverlayPosition::TopLeft);
        w.setShowNoiseFloorPosition(SpectrumWidget::OverlayPosition::BottomRight);
        QCOMPARE(w.showNoiseFloorPosition(), SpectrumWidget::OverlayPosition::BottomRight);
    }

    // ── DispNormalize ──────────────────────────────────────────────────────

    // From Thetis specHPSDR.cs:325 [v2.10.3.13] NormOneHzPan.
    void disp_normalize_roundtrip()
    {
        SpectrumWidget w;
        QCOMPARE(w.dispNormalize(), false);
        w.setDispNormalize(true);
        QCOMPARE(w.dispNormalize(), true);
        w.setDispNormalize(false);
        QCOMPARE(w.dispNormalize(), false);
    }

    // ── ShowPeakValueOverlay ────────────────────────────────────────────────

    // From Thetis console.cs:20073 [v2.10.3.13] peak_text_delay default=500.
    // Upstream tags preserved: //MW0LGE (from cited console.cs:20070) [v2.10.3.15]
    void peak_value_overlay_roundtrip()
    {
        SpectrumWidget w;
        QCOMPARE(w.showPeakValueOverlay(), false);
        w.setShowPeakValueOverlay(true);
        QCOMPARE(w.showPeakValueOverlay(), true);
        w.setShowPeakValueOverlay(false);
        QCOMPARE(w.showPeakValueOverlay(), false);
    }

    void peak_text_delay_default_is_500ms()
    {
        // From Thetis console.cs:20073 [v2.10.3.13]: peak_text_delay = 500.
        // Upstream tags preserved: //MW0LGE (from cited console.cs:20070) [v2.10.3.15]
        SpectrumWidget w;
        QCOMPARE(w.peakTextDelayMs(), 500);
    }

    void peak_text_delay_clamped_to_bounds()
    {
        SpectrumWidget w;
        w.setPeakTextDelayMs(0);     // below min (50)
        QCOMPARE(w.peakTextDelayMs(), 50);
        w.setPeakTextDelayMs(99999); // above max (10000)
        QCOMPARE(w.peakTextDelayMs(), 10000);
    }

    void peak_value_color_default_is_dodgerblue()
    {
        // From Thetis console.cs:20278 [v2.10.3.13]: Color.DodgerBlue = #1E90FF.
        SpectrumWidget w;
        QCOMPARE(w.peakValueColor(), QColor(0x1E, 0x90, 0xFF));
    }

    void peak_value_position_roundtrip()
    {
        SpectrumWidget w;
        w.setPeakValuePosition(SpectrumWidget::OverlayPosition::BottomLeft);
        QCOMPARE(w.peakValuePosition(), SpectrumWidget::OverlayPosition::BottomLeft);
    }

    // ── FFTEngine decimation ────────────────────────────────────────────────

    void fft_engine_decimation_default_is_one()
    {
        FFTEngine e(0);
        QCOMPARE(e.decimation(), 1);
    }

    void fft_engine_decimation_roundtrip()
    {
        FFTEngine e(0);
        e.setDecimation(4);
        QCOMPARE(e.decimation(), 4);
        e.setDecimation(1);
        QCOMPARE(e.decimation(), 1);
    }

    void fft_engine_decimation_rejects_out_of_range()
    {
        FFTEngine e(0);
        e.setDecimation(4);
        e.setDecimation(0);   // below min
        QCOMPARE(e.decimation(), 4);   // unchanged
        e.setDecimation(33);  // above max
        QCOMPARE(e.decimation(), 4);   // unchanged
    }

    void fft_engine_decimation_boundary_values()
    {
        FFTEngine e(0);
        e.setDecimation(1);
        QCOMPARE(e.decimation(), 1);
        e.setDecimation(32);
        QCOMPARE(e.decimation(), 32);
    }
};

// QApplication is required for SpectrumWidget (a QWidget subclass).
QTEST_MAIN(TestSpectrumOverlays)
#include "tst_spectrum_overlays.moc"
