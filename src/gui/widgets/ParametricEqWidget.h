#pragma once

// =================================================================
// src/gui/widgets/ParametricEqWidget.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/ucParametricEq.cs [v2.10.3.13],
//   original licence from Thetis source is included below.
//   Sole author: Richard Samphire (MW0LGE) — GPLv2-or-later with
//   Samphire dual-licensing.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — Reimplemented in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.  Phase 3M-3a-ii follow-up
//                 sub-PR Batch 1: skeleton + EqPoint/EqJsonState classes
//                 + default 18-color palette + ctor with verbatim
//                 ucParametricEq.cs:360-447 defaults.
// =================================================================

/*  ucParametricEq.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

#include <QColor>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>

class QPaintEvent;
class QPainter;
class QMouseEvent;
class QWheelEvent;

// Forward declarations for tester classes that need access to private
// axis/ordering helpers via `friend`. Each tester lives in its own
// translation unit (tst_parametric_eq_widget_*.cpp) and exposes the
// protected `using ParametricEqWidget::xFromFreq` etc. so the test
// `QObject` can call them.  Future batches add their own friends in
// the same block — keep them grouped.
class ParametricEqAxisTester;          // Batch 2
class ParametricEqPaintTester;         // Batch 3 (this batch)
class ParametricEqInteractionTester;   // Batch 4
class ParametricEqJsonTester;          // Batch 5

namespace NereusSDR {

class ParametricEqWidget : public QWidget {
    Q_OBJECT
    friend class ::ParametricEqAxisTester;        // for tst_parametric_eq_widget_axis
    friend class ::ParametricEqPaintTester;       // for tst_parametric_eq_widget_paint
    friend class ::ParametricEqInteractionTester; // Task 4
    friend class ::ParametricEqJsonTester;        // Task 5
public:
    // -- Public types (mirror C# ucParametricEq.EqPoint / EqJsonState / EqJsonPoint) --

    // From Thetis ucParametricEq.cs:54-105 [v2.10.3.13] -- sealed class EqPoint.
    struct EqPoint {
        int     bandId       = 0;
        QColor  bandColor;          // QColor() means "use palette default" (Color.Empty in C#)
        double  frequencyHz  = 0.0;
        double  gainDb       = 0.0;
        double  q            = 4.0;

        EqPoint() = default;
        EqPoint(int id, QColor color, double f, double g, double qVal)
            : bandId(id), bandColor(std::move(color)),
              frequencyHz(f), gainDb(g), q(qVal) {}
    };

    // From Thetis ucParametricEq.cs:220-240 [v2.10.3.13] -- EqJsonState.
    struct EqJsonState {
        int                bandCount     = 10;
        bool               parametricEq  = true;
        double             globalGainDb  = 0.0;
        double             frequencyMinHz = 0.0;
        double             frequencyMaxHz = 4000.0;
        QVector<EqPoint>   points;        // FrequencyHz / GainDb / Q only -- ignore bandId/color
    };

    explicit ParametricEqWidget(QWidget* parent = nullptr);
    ~ParametricEqWidget() override;

    // Test-friendly accessors for the palette (still private data; just exposed read-only).
    static int    defaultBandPaletteSize();
    static QColor defaultBandPaletteAt(int index);

    // -- Public setters (mirror C# property setters at ucParametricEq.cs:704-720
    //    GlobalGainDb and cs:970-1005 SelectedIndex). --
    // From Thetis ucParametricEq.cs:704-720 [v2.10.3.13].
    void setGlobalGainDb(double db);
    // From Thetis ucParametricEq.cs:970-1005 [v2.10.3.13].
    void setSelectedIndex(int index);

public slots:
    // From Thetis ucParametricEq.cs:1048-1105 [v2.10.3.13] -- public DrawBarChart slot.
    void drawBarChartData(const QVector<double>& data);

signals:
    // From Thetis ucParametricEq.cs:353-358 [v2.10.3.13] -- public events.
    // PointsChanged / GlobalGainChanged / SelectedIndexChanged carry an
    // is_dragging bool so downstream consumers can throttle expensive
    // reactions (e.g. a CFC re-dispatch) until the user releases the mouse.
    // PointDataChanged carries the resolved point payload.  PointSelected /
    // PointUnselected fire when the highlight moves between bands.
    void pointsChanged          (bool isDragging);
    void globalGainChanged      (bool isDragging);
    void selectedIndexChanged   (bool isDragging);
    void pointDataChanged       (int index, int bandId,
                                 double frequencyHz, double gainDb, double q,
                                 bool isDragging);
    void pointSelected          (int index, int bandId,
                                 double frequencyHz, double gainDb, double q);
    void pointUnselected        (int index, int bandId,
                                 double frequencyHz, double gainDb, double q);

protected:
    // Paint orchestration -- From Thetis ucParametricEq.cs:1575-1609 [v2.10.3.13].
    void paintEvent(QPaintEvent* event) override;

    // Interaction handlers -- From Thetis ucParametricEq.cs:1625-1995 [v2.10.3.13].
    void mousePressEvent  (QMouseEvent* event) override;
    void mouseMoveEvent   (QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent       (QWheelEvent* event) override;

private:
    // Draw helpers -- From Thetis ucParametricEq.cs:1575-2748 [v2.10.3.13].
    void drawGrid           (QPainter& g, const QRect& plot);
    void drawBarChart       (QPainter& g, const QRect& plot);
    void drawBandShading    (QPainter& g, const QRect& plot);
    void drawCurve          (QPainter& g, const QRect& plot);
    void drawGlobalGainHandle(QPainter& g, const QRect& plot);
    void drawPoints         (QPainter& g, const QRect& plot);
    void drawDotReading     (QPainter& g, const QRect& plot, const EqPoint& p,
                             float dotX, float dotY, float dotRadius);
    void drawAxisScales     (QPainter& g, const QRect& plot);
    void drawBorder         (QPainter& g, const QRect& plot);
    void drawReadout        (QPainter& g, const QRect& plot);

    // Math: response curve at a given frequency.
    double responseDbAtFrequency(double frequencyHz) const;

    // Bar chart helpers -- From Thetis ucParametricEq.cs:2751-2862 [v2.10.3.13].
    void   applyBarChartPeakDecay (qint64 nowMs);
    void   syncBarChartPeaksToData();
    void   updateBarChartPeakTimerState();
    QColor getPointDisplayColor(int index) const;

    // Tick / readout formatting.
    QString formatDbTick(double db, double stepDb) const;
    QString formatHzTick(double hz)               const;
    QString formatDotReadingHz(double hz)         const;
    QString formatDotReadingDb(double db)         const;
    QString formatHz(double hz)                   const;
    QString formatDb(double db)                   const;

private:
    // From Thetis ucParametricEq.cs:254-274 [v2.10.3.13] -- _default_band_palette.
    static const QVector<QColor>& defaultBandPalette();

    // Band colour resolution -- From Thetis ucParametricEq.cs:2864-2871 [v2.10.3.13].
    static QColor getBandBaseColor(int index);

    // Axis math -- From Thetis ucParametricEq.cs:2951-3078 [v2.10.3.13].
    QRect  computePlotRect()                                   const;
    int    computedPlotMarginLeft()                            const;
    int    computedPlotMarginRight()                           const;
    int    computedPlotMarginBottom()                          const;
    int    axisLabelMaxWidth()                                 const;
    float  xFromFreq(const QRect& plot, double frequencyHz)    const;
    float  yFromDb(const QRect& plot, double db)               const;
    double freqFromX(const QRect& plot, int x)                 const;
    double dbFromY(const QRect& plot, int y)                   const;
    double getNormalizedFrequencyPosition(double freqHz)       const;
    double getNormalizedFrequencyPosition(double freqHz, double minHz, double maxHz, bool useLog) const;
    double frequencyFromNormalizedPosition(double t)           const;
    double frequencyFromNormalizedPosition(double t, double minHz, double maxHz, bool useLog) const;
    double getLogFrequencyCentreHz(double minHz, double maxHz) const;
    double getLogFrequencyShape(double centreRatio)            const;
    QVector<double> getLogFrequencyTicks(const QRect& plot)    const;
    double chooseFrequencyStep(double span)                    const;
    double chooseDbStep(double span)                           const;
    double getYAxisStepDb()                                    const;

    // Hit-test -- From Thetis ucParametricEq.cs:2910-2949 [v2.10.3.13].
    int    hitTestPoint(const QRect& plot, QPoint pt)          const;
    bool   hitTestGlobalGainHandle(const QRect& plot, QPoint pt) const;

    // Ordering / clamping -- From Thetis ucParametricEq.cs:3163-3332 [v2.10.3.13].
    void   resetPointsDefault();
    void   rescaleFrequencies(double oldMin, double oldMax, double newMin, double newMax);
    void   enforceOrdering(bool enforceSpacingAll);
    void   clampAllGains();
    void   clampAllQ();

    // Locked endpoints -- From Thetis ucParametricEq.cs:3384-3394 [v2.10.3.13].
    bool   isFrequencyLockedIndex(int index)                   const;
    double getLockedFrequencyForIndex(int index)               const;

    // Helper -- From Thetis ucParametricEq.cs:1142-1150 [v2.10.3.13] -- band lookup.
    int      indexFromBandId(int bandId)                       const;

    // From Thetis ucParametricEq.cs:1997-2000 [v2.10.3.13].
    bool     isDraggingNow()                                   const;

    // From Thetis ucParametricEq.cs:1025-1039, 3334-3363 [v2.10.3.13] -- raise*
    // helpers.  These are private signal forwarders so the call sites stay
    // 1:1 with the C# source.  The `raisePointDataChanged` overload takes
    // the index explicitly (rather than calling QVector::indexOf and
    // requiring EqPoint::operator==): EqPoint is a value type without a
    // natural equality, so the caller threads the resolved index through.
    void     raisePointsChanged       (bool isDragging);
    void     raiseGlobalGainChanged   (bool isDragging);
    void     raiseSelectedIndexChanged(bool isDragging);
    void     raisePointSelected       (int index, const EqPoint& p);
    void     raisePointUnselected     (int index, const EqPoint& p);
    void     raisePointDataChanged    (int index, const EqPoint& p, bool isDragging);

    static double clamp(double v, double lo, double hi);

    // -- Member state (mirrors private fields ucParametricEq.cs:276-351) --
    QVector<EqPoint> m_points;
    int              m_bandCount               = 10;

    double           m_frequencyMinHz          = 0.0;
    double           m_frequencyMaxHz          = 4000.0;

    double           m_dbMin                   = -24.0;
    double           m_dbMax                   =  24.0;

    double           m_globalGainDb            = 0.0;
    bool             m_globalGainIsHorizLine   = false;

    int              m_selectedIndex           = -1;
    int              m_dragIndex               = -1;
    bool             m_draggingGlobalGain      = false;
    bool             m_draggingPoint           = false;

    // Margins, hit radii, axis ticks, palette flags, etc. -- defaults below match
    // ucParametricEq.cs:367-447 verbatim.
    int              m_plotMarginLeft          = 30;
    int              m_plotMarginRight         = 28;
    int              m_plotMarginTop           = 14;
    int              m_plotMarginBottom        = 62;
    double           m_yAxisStepDb             = 0.0;
    int              m_pointRadius             = 5;
    int              m_hitRadius               = 11;
    double           m_qMin                    = 0.2;
    double           m_qMax                    = 30.0;
    double           m_minPointSpacingHz       = 5.0;
    bool             m_allowPointReorder       = true;
    bool             m_parametricEq            = true;
    bool             m_showReadout             = true;
    bool             m_showDotReadings         = false;
    bool             m_showDotReadingsAsComp   = false;
    int              m_globalHandleXOffset     = 6;
    int              m_globalHandleSize        = 10;
    int              m_globalHitExtra          = 6;
    bool             m_showBandShading         = true;
    bool             m_usePerBandColours       = true;
    QColor           m_bandShadeColor          {200, 200, 200};
    int              m_bandShadeAlpha          = 70;
    double           m_bandShadeWeightCutoff   = 0.002;
    bool             m_showAxisScales          = true;
    QColor           m_axisTextColor           {170, 170, 170};
    QColor           m_axisTickColor           {80,  80,  80};
    int              m_axisTickLength          = 6;
    bool             m_logScale                = false;

    // Bar chart state (ucParametricEq.cs:340-351).
    QVector<double>  m_barChartData;
    QVector<double>  m_barChartPeakData;
    QVector<qint64>  m_barChartPeakHoldUntilMs;
    qint64           m_barChartPeakLastUpdateMs = 0;
    QTimer*          m_barChartPeakTimer       = nullptr;
    QColor           m_barChartFillColor       {0, 120, 255};
    int              m_barChartFillAlpha       = 120;
    QColor           m_barChartPeakColor       {160, 210, 255};
    int              m_barChartPeakAlpha       = 230;
    bool             m_barChartPeakHoldEnabled = true;
    int              m_barChartPeakHoldMs      = 1000;
    double           m_barChartPeakDecayDbPerSecond = 20.0;

    // Drag-commit dirty flags (ucParametricEq.cs:296-299).
    EqPoint*         m_dragPointRef            = nullptr;
    bool             m_dragDirtyPoint          = false;
    bool             m_dragDirtyGlobalGain     = false;
    bool             m_dragDirtySelectedIndex  = false;
};

} // namespace NereusSDR
