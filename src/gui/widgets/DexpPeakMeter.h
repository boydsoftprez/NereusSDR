// =================================================================
// src/gui/widgets/DexpPeakMeter.h  (NereusSDR)
// =================================================================
//
// NereusSDR-native widget — DEXP / VOX live peak-meter strip.
// Mirrors Thetis picVOX_Paint / picNoiseGate_Paint draw behavior
// (console.cs:28949-28960 / :28972-28981 [v2.10.3.13]) but is a
// fresh Qt6/QPainter implementation (Thetis uses
// System.Drawing.Graphics, no direct port possible).
//
// Drawn as a thin horizontal strip (default 4 px tall) directly
// under a threshold slider in PhoneCwApplet.  Shows live mic peak
// in lime-green, with a red threshold marker line + red overlay
// when the signal exceeds the threshold (the "above-threshold"
// segment from threshold -> signal is repainted red, matching
// picVOX_Paint:28958-28959 verbatim).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-03 — Phase 3M-3a-iii Task 13: Created by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via
//                 Anthropic Claude Code.
// =================================================================

// no-port-check: NereusSDR-original Qt6 widget.  Thetis paints with
// System.Drawing.Graphics (no direct port possible); only the visual
// recipe is mirrored, and the cite-to-Thetis lines above are
// commentary, not ported code.

#pragma once

#include <QWidget>

namespace NereusSDR {

class DexpPeakMeter : public QWidget {
    Q_OBJECT
public:
    explicit DexpPeakMeter(QWidget* parent = nullptr);

    /// Set the live signal level (normalized 0.0..1.0).  Caller is
    /// responsible for mapping their domain (linear amplitude or dB)
    /// to this scale.  Out-of-range values are clamped.
    void setSignalLevel(double normalized01);

    /// Set the threshold marker position (normalized 0.0..1.0).
    /// Out-of-range values are clamped.
    void setThresholdMarker(double normalized01);

    QSize sizeHint() const override        { return QSize(100, 4); }
    QSize minimumSizeHint() const override { return QSize(40, 4); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    double m_signal01    = 0.0;
    double m_threshold01 = 0.5;
};

} // namespace NereusSDR
