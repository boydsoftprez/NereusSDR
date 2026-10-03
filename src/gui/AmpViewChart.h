// no-port-check: NereusSDR-original Qt chart.  The presentation semantics
// follow Thetis AmpView.cs [v2.10.3.13]; PS3 curve data comes from the pinned
// WDSP 2.10 display adapter rather than reconstructing the old PS2 splines.
// Thetis uses System.Windows.Forms.DataVisualization.Charting; this custom
// QPainter renderer follows phase3m-4-puresignal-design.md section 15 #14.
//
// =================================================================
// src/gui/AmpViewChart.h  (NereusSDR)
// =================================================================
// Modification history (NereusSDR):
//   2026-05-06 — Phase 3M-4 Task 9 created by J.J. Boyd (KG4VCF),
//                 with AI-assisted transformation via Anthropic
//                 Claude Code.  NereusSDR-original Qt6 widget.
//   2026-09-22 — Accept owning PS3 curves with independent coordinates and
//                 series visibility; J.J. Boyd (KG4VCF), OpenAI Codex.
// =================================================================

#pragma once

#include "core/dsp/Ps3Snapshot.h"

#include <QStringList>
#include <QWidget>

#include <array>

class QPaintEvent;

namespace NereusSDR {

class AmpViewChart final : public QWidget {
    Q_OBJECT

public:
    enum class Series : int {
        Reference = 0,
        MeasuredMagnitude,
        MeasuredPhase,
        CorrectionMagnitude,
        CorrectionPhase,
        Count
    };

    explicit AmpViewChart(QWidget* parent = nullptr);
    ~AmpViewChart() override;

    // Ps3PlotPoint owns each series' x coordinate.  In PS3 the magnitude and
    // phase correction curves have different x arrays (xm_cor and xa_cor), so
    // they must never be projected through a shared or synthesized axis.
    void setPlotData(const Ps3PlotData& data);
    void clearData();
    const Ps3PlotData& plotData() const noexcept { return m_plot; }

    void setShowGain(bool on);
    bool showGain() const noexcept { return m_showGain; }
    void setPhaseZoom(bool on);
    bool phaseZoom() const noexcept { return m_phaseZoom; }
    void setLowRes(bool on);
    bool lowRes() const noexcept { return m_lowRes; }

    void setSeriesVisible(Series series, bool visible);
    bool seriesVisible(Series series) const noexcept;
    int seriesCount() const noexcept { return static_cast<int>(Series::Count); }
    QStringList seriesNames() const;

    double magYMin() const noexcept { return 0.0; }
    double magYMax() const noexcept { return m_showGain ? 2.0 : 1.0; }
    double phaseYMax() const noexcept { return m_phaseZoom ? 45.0 : 180.0; }
    double phaseYMin() const noexcept { return -phaseYMax(); }
    int lowResStride() const noexcept { return m_lowRes ? 4 : 1; }

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize sizeHint() const override { return QSize(564, 340); }

private:
    Ps3PlotData m_plot;
    std::array<bool, static_cast<std::size_t>(Series::Count)> m_visible{
        true, true, true, true, true};
    bool m_showGain{false};
    bool m_phaseZoom{false};
    bool m_lowRes{true};
};

} // namespace NereusSDR
