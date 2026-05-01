// =================================================================
// src/gui/widgets/ParametricEqWidget.cpp  (NereusSDR)
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

#include "ParametricEqWidget.h"

#include <QBrush>
#include <QCursor>
#include <QDateTime>
#include <QFontMetrics>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QRectF>
#include <QWheelEvent>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <limits>

namespace NereusSDR {

// From Thetis ucParametricEq.cs:254-274 [v2.10.3.13] -- _default_band_palette.
// 18 RGB triples, verbatim.  Ports byte-for-byte; do not reorder, recolor, or trim.
const QVector<QColor>& ParametricEqWidget::defaultBandPalette() {
    static const QVector<QColor> kPalette = {
        QColor(  0, 190, 255),
        QColor(  0, 220, 130),
        QColor(255, 210,   0),
        QColor(255, 140,   0),
        QColor(255,  80,  80),
        QColor(255,   0, 180),
        QColor(170,  90, 255),
        QColor( 70, 120, 255),
        QColor(  0, 200, 200),
        QColor(180, 255,  90),
        QColor(255, 105, 180),
        QColor(255, 215, 120),
        QColor(120, 255, 255),
        QColor(140, 200, 255),
        QColor(220, 160, 255),
        QColor(255, 120,  40),
        QColor(120, 255, 160),
        QColor(255,  60, 120),
    };
    return kPalette;
}

int ParametricEqWidget::defaultBandPaletteSize() {
    return defaultBandPalette().size();
}

QColor ParametricEqWidget::defaultBandPaletteAt(int index) {
    const auto& p = defaultBandPalette();
    if (index < 0 || index >= p.size()) {
        return QColor();
    }
    return p.at(index);
}

// From Thetis ucParametricEq.cs:2864-2871 [v2.10.3.13].
QColor ParametricEqWidget::getBandBaseColor(int index) {
    const auto& palette = defaultBandPalette();
    int n = palette.size();
    if (n <= 0) return QColor(200, 200, 200);
    int idx = index % n;
    if (idx < 0) idx = 0;
    return palette.at(idx);
}

// From Thetis ucParametricEq.cs:360-447 [v2.10.3.13] -- public ucParametricEq() ctor.
ParametricEqWidget::ParametricEqWidget(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);

    // Bar chart peak-hold timer -- 33 ms cadence (~30 fps), matches WinForms Timer { Interval = 33 }
    // at ucParametricEq.cs:428-430.  Slot tick body ports cs:2751-2767 (barChartPeakTimer_Tick).
    m_barChartPeakTimer = new QTimer(this);
    m_barChartPeakTimer->setInterval(33);

    // Mirror Thetis init of _bar_chart_peak_last_update_utc.  C# uses
    // DateTime.UtcNow lazily inside updateBarChartPeakTimerState, but the
    // first applyBarChartPeakDecay call needs a sane "previous tick" so the
    // initial elapsed_seconds delta isn't huge.  Ucparametriceq.cs:2851 also
    // re-initializes this when the timer enables -- we do the same plus this
    // ctor seed for safety.
    m_barChartPeakLastUpdateMs = QDateTime::currentMSecsSinceEpoch();

    connect(m_barChartPeakTimer, &QTimer::timeout, this, [this]() {
        // From Thetis ucParametricEq.cs:2751-2767 [v2.10.3.13] -- barChartPeakTimer_Tick.
        if (m_barChartData.isEmpty()) {
            updateBarChartPeakTimerState();
            return;
        }
        if (!m_barChartPeakHoldEnabled) {
            updateBarChartPeakTimerState();
            return;
        }
        applyBarChartPeakDecay(QDateTime::currentMSecsSinceEpoch());
        update();
    });

    resetPointsDefault();
}

ParametricEqWidget::~ParametricEqWidget() = default;

// =================================================================
// Axis math + ordering + clamping helpers (Phase 3M-3a-ii follow-up
// Batch 2).  All ports are line-faithful translations of
// ucParametricEq.cs [v2.10.3.13] -- each function carries a verbatim
// cite immediately above its definition.
// =================================================================

// From Thetis ucParametricEq.cs:2983-2988 [v2.10.3.13].
double ParametricEqWidget::clamp(double v, double lo, double hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// From Thetis ucParametricEq.cs:1013-1023 [v2.10.3.13].
double ParametricEqWidget::chooseDbStep(double span) const {
    double s = span;
    if (s <=  3.0) return  0.5;
    if (s <=  6.0) return  1.0;
    if (s <= 12.0) return  2.0;
    if (s <= 24.0) return  3.0;
    if (s <= 48.0) return  6.0;
    if (s <= 96.0) return 12.0;
    return 24.0;
}

// From Thetis ucParametricEq.cs:1007-1011 [v2.10.3.13].
double ParametricEqWidget::getYAxisStepDb() const {
    if (m_yAxisStepDb > 0.0) return m_yAxisStepDb;
    return chooseDbStep(m_dbMax - m_dbMin);
}

// From Thetis ucParametricEq.cs:2632-2643 [v2.10.3.13].
double ParametricEqWidget::chooseFrequencyStep(double span) const {
    double s = span;
    if (s <=   300.0) return   25.0;
    if (s <=   600.0) return   50.0;
    if (s <=  1200.0) return  100.0;
    if (s <=  2500.0) return  250.0;
    if (s <=  6000.0) return  500.0;
    if (s <= 12000.0) return 1000.0;
    if (s <= 24000.0) return 2000.0;
    return 5000.0;
}

// From Thetis ucParametricEq.cs:2995-3000 [v2.10.3.13].
double ParametricEqWidget::getLogFrequencyCentreHz(double minHz, double maxHz) const {
    double span = maxHz - minHz;
    if (span <= 0.0) return minHz;
    return minHz + (span * 0.125);
}

// From Thetis ucParametricEq.cs:3069-3078 [v2.10.3.13].
double ParametricEqWidget::getLogFrequencyShape(double centreRatio) const {
    double r = centreRatio;
    if (r <= 0.0 || r >= 1.0) return 0.0;
    if (qAbs(r - 0.5) < 0.0000001) return 0.0;

    double shape = (1.0 - (2.0 * r)) / (r * r);
    if (shape < 0.0) return 0.0;
    return shape;
}

// From Thetis ucParametricEq.cs:3012-3033 [v2.10.3.13].
double ParametricEqWidget::getNormalizedFrequencyPosition(
    double frequencyHz, double minHz, double maxHz, bool useLog) const {
    double span = maxHz - minHz;
    if (span <= 0.0) return 0.0;

    double f = clamp(frequencyHz, minHz, maxHz);
    double u = (f - minHz) / span;

    if (!useLog) {
        return u;
    }

    double centreRatio = (getLogFrequencyCentreHz(minHz, maxHz) - minHz) / span;
    double shape = getLogFrequencyShape(centreRatio);
    if (shape <= 0.0) {
        return u;
    }

    return std::log(1.0 + (shape * u)) / std::log(1.0 + shape);
}

// From Thetis ucParametricEq.cs:3002-3005 [v2.10.3.13].
double ParametricEqWidget::getNormalizedFrequencyPosition(double frequencyHz) const {
    return getNormalizedFrequencyPosition(frequencyHz, m_frequencyMinHz, m_frequencyMaxHz, m_logScale);
}

// From Thetis ucParametricEq.cs:3045-3067 [v2.10.3.13].
double ParametricEqWidget::frequencyFromNormalizedPosition(
    double t, double minHz, double maxHz, bool useLog) const {
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;

    double span = maxHz - minHz;
    if (span <= 0.0) return minHz;

    if (!useLog) {
        return minHz + (t * span);
    }

    double centreRatio = (getLogFrequencyCentreHz(minHz, maxHz) - minHz) / span;
    double shape = getLogFrequencyShape(centreRatio);
    if (shape <= 0.0) {
        return minHz + (t * span);
    }

    double u = (std::exp(t * std::log(1.0 + shape)) - 1.0) / shape;
    return minHz + (u * span);
}

// From Thetis ucParametricEq.cs:3035-3038 [v2.10.3.13].
double ParametricEqWidget::frequencyFromNormalizedPosition(double t) const {
    return frequencyFromNormalizedPosition(t, m_frequencyMinHz, m_frequencyMaxHz, m_logScale);
}

// From Thetis ucParametricEq.cs:2951-2955 [v2.10.3.13].
float ParametricEqWidget::xFromFreq(const QRect& plot, double frequencyHz) const {
    double t = getNormalizedFrequencyPosition(frequencyHz);
    return float(plot.left()) + float(t * plot.width());
}

// From Thetis ucParametricEq.cs:2957-2963 [v2.10.3.13].
double ParametricEqWidget::freqFromX(const QRect& plot, int x) const {
    double t = (double(x) - double(plot.left())) / double(plot.width());
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return frequencyFromNormalizedPosition(t);
}

// From Thetis ucParametricEq.cs:2965-2971 [v2.10.3.13].
float ParametricEqWidget::yFromDb(const QRect& plot, double db) const {
    double span = m_dbMax - m_dbMin;
    if (span <= 0.0) span = 1.0;
    double t = (db - m_dbMin) / span;
    return float(plot.bottom()) - float(t * plot.height());
}

// From Thetis ucParametricEq.cs:2973-2981 [v2.10.3.13].
double ParametricEqWidget::dbFromY(const QRect& plot, int y) const {
    double span = m_dbMax - m_dbMin;
    if (span <= 0.0) span = 1.0;
    double t = (double(plot.bottom()) - double(y)) / double(plot.height());
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return m_dbMin + t * span;
}

// From Thetis ucParametricEq.cs:2042-2059 [v2.10.3.13].
QRect ParametricEqWidget::computePlotRect() const {
    QRect r = rect();
    int left   = computedPlotMarginLeft();
    int right  = computedPlotMarginRight();
    int bottom = computedPlotMarginBottom();
    int x = r.x() + left;
    int y = r.y() + m_plotMarginTop;
    int w = r.width()  - left - right;
    int h = r.height() - m_plotMarginTop - bottom;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    return QRect(x, y, w, h);
}

// From Thetis ucParametricEq.cs:2002-2013 [v2.10.3.13].
// Now that Batch 3 has ported formatDbTick (cs:2601-2614) we use it directly
// for the real label-builder strings (e.g. "+24", "-12", "0") rather than
// the conservative "%.1f" placeholder used in Batch 2.  Addresses Batch 2
// code-review Minor #8 (overestimates margin for "+24" by ~3 px otherwise).
int ParametricEqWidget::axisLabelMaxWidth() const {
    QFontMetrics fm(font());
    double step = getYAxisStepDb();
    QString s1 = formatDbTick(m_dbMin, step);
    QString s2 = formatDbTick(m_dbMax, step);
    return std::max(fm.horizontalAdvance(s1), fm.horizontalAdvance(s2));
}

// From Thetis ucParametricEq.cs:2015-2028 [v2.10.3.13].
int ParametricEqWidget::computedPlotMarginLeft() const {
    int m = m_plotMarginLeft;
    if (m_showAxisScales) {
        int need = m_axisTickLength + 4 + axisLabelMaxWidth() + 8;
        if (need > m) m = need;
    }
    if (m < 10) m = 10;
    return m;
}

// From Thetis ucParametricEq.cs:2030-2040 [v2.10.3.13].
int ParametricEqWidget::computedPlotMarginRight() const {
    int m = m_plotMarginRight;
    int gainNeed = m_globalHandleXOffset + (m_globalHandleSize * 2) + m_globalHitExtra + 6;
    if (gainNeed > m) m = gainNeed;
    if (m < 10) m = 10;
    return m;
}

// From Thetis ucParametricEq.cs:3365-3382 [v2.10.3.13].
int ParametricEqWidget::computedPlotMarginBottom() const {
    if (m_showReadout) return m_plotMarginBottom;
    int m = 8;
    if (m_showAxisScales) {
        QFontMetrics fm(font());
        m += m_axisTickLength + 2 + fm.height() + 4;
    } else {
        m += 8;
    }
    if (m < 10) m = 10;
    return m;
}

// From Thetis ucParametricEq.cs:2910-2934 [v2.10.3.13].
int ParametricEqWidget::hitTestPoint(const QRect& plot, QPoint pt) const {
    int best = -1;
    double bestD2 = std::numeric_limits<double>::max();

    for (int i = 0; i < m_points.size(); ++i) {
        const auto& p = m_points.at(i);
        float x = xFromFreq(plot, p.frequencyHz);
        float y = yFromDb(plot, p.gainDb);
        double dx = double(pt.x()) - double(x);
        double dy = double(pt.y()) - double(y);
        double d2 = dx * dx + dy * dy;
        double r = double(m_hitRadius);
        if (d2 <= r * r && d2 < bestD2) {
            bestD2 = d2;
            best = i;
        }
    }
    return best;
}

// From Thetis ucParametricEq.cs:2936-2949 [v2.10.3.13].
bool ParametricEqWidget::hitTestGlobalGainHandle(const QRect& plot, QPoint pt) const {
    float y = yFromDb(plot, m_globalGainDb);
    int hx = plot.right() + m_globalHandleXOffset;
    int s  = m_globalHandleSize;
    QRect r(hx - m_globalHitExtra,
            int(std::round(y)) - (s + m_globalHitExtra),
            (s + m_globalHitExtra) * 2,
            (s + m_globalHitExtra) * 2);
    return r.contains(pt);
}

// From Thetis ucParametricEq.cs:3384-3387 [v2.10.3.13].
bool ParametricEqWidget::isFrequencyLockedIndex(int index) const {
    return !m_points.isEmpty()
        && (index == 0 || index == m_points.size() - 1);
}

// From Thetis ucParametricEq.cs:3389-3394 [v2.10.3.13].
double ParametricEqWidget::getLockedFrequencyForIndex(int index) const {
    if (index <= 0) return m_frequencyMinHz;
    if (index >= m_points.size() - 1) return m_frequencyMaxHz;
    return m_points.at(index).frequencyHz;
}

// From Thetis ucParametricEq.cs:1142-1150 [v2.10.3.13] -- GetIndexFromBandId.
int ParametricEqWidget::indexFromBandId(int bandId) const {
    for (int i = 0; i < m_points.size(); ++i) {
        if (m_points.at(i).bandId == bandId) return i;
    }
    return -1;
}

// From Thetis ucParametricEq.cs:3314-3323 [v2.10.3.13].
void ParametricEqWidget::clampAllGains() {
    for (auto& p : m_points) {
        p.gainDb = clamp(p.gainDb, m_dbMin, m_dbMax);
    }
    m_globalGainDb = clamp(m_globalGainDb, m_dbMin, m_dbMax);
}

// From Thetis ucParametricEq.cs:3325-3332 [v2.10.3.13].
void ParametricEqWidget::clampAllQ() {
    for (auto& p : m_points) {
        p.q = clamp(p.q, m_qMin, m_qMax);
    }
}

// From Thetis ucParametricEq.cs:3223-3312 [v2.10.3.13].
// Subtle bit: after the sort, the previously-selected and previously-dragged
// points may have shifted index.  We capture each point's bandId BEFORE the
// sort, then re-resolve by bandId AFTER, so the user's selection stays
// pinned to the same band rather than the old slot.  C# does this by
// holding object references (List<T>.IndexOf); we use bandId because
// EqPoint is a value type in C++.
void ParametricEqWidget::enforceOrdering(bool enforceSpacingAll) {
    if (m_points.isEmpty()) return;

    int oldSelected = m_selectedIndex;
    int oldDrag     = m_dragIndex;
    int selectedBandId = (oldSelected >= 0 && oldSelected < m_points.size())
                        ? m_points.at(oldSelected).bandId : -1;
    int dragBandId     = (oldDrag >= 0 && oldDrag < m_points.size())
                        ? m_points.at(oldDrag).bandId : -1;

    if (m_allowPointReorder && m_points.size() > 1) {
        std::stable_sort(m_points.begin(), m_points.end(),
            [](const EqPoint& a, const EqPoint& b) {
                if (a.frequencyHz != b.frequencyHz) return a.frequencyHz < b.frequencyHz;
                return a.bandId < b.bandId;
            });
    }

    // Re-resolve indices by bandId after sort.
    int newSelectedIndex = (selectedBandId >= 0) ? indexFromBandId(selectedBandId) : -1;
    bool selectedChanged = (newSelectedIndex != m_selectedIndex);
    m_selectedIndex = newSelectedIndex;
    m_dragIndex     = (dragBandId >= 0)     ? indexFromBandId(dragBandId)     : -1;

    // From Thetis ucParametricEq.cs:3249, 3257 [v2.10.3.13] -- emit when post-sort
    // selected index differs from the pre-sort value (the C# splits this into two
    // branches, one for "have selection" + one for "lost selection"; the boolean
    // collapse here is observationally identical because both branches always emit
    // when the index actually changes, and the combined predicate simply OR-s the
    // two sufficient conditions).
    if (selectedChanged) raiseSelectedIndexChanged(isDraggingNow());

    for (auto& p : m_points) {
        p.frequencyHz = clamp(p.frequencyHz, m_frequencyMinHz, m_frequencyMaxHz);
    }
    if (m_points.size() > 0) m_points.front().frequencyHz = m_frequencyMinHz;
    if (m_points.size() > 1) m_points.back().frequencyHz  = m_frequencyMaxHz;

    if (!enforceSpacingAll) return;
    if (m_points.size() < 3) return;

    double spacing    = m_minPointSpacingHz;
    double maxSpacing = (m_frequencyMaxHz - m_frequencyMinHz) / double(m_points.size() - 1);
    if (spacing > maxSpacing) spacing = maxSpacing;
    if (spacing < 0.0)        spacing = 0.0;

    for (int i = 1; i < m_points.size() - 1; ++i) {
        double minF = m_frequencyMinHz + (spacing * i);
        double maxF = m_frequencyMaxHz - (spacing * (m_points.size() - 1 - i));
        if (maxF < minF) maxF = minF;
        m_points[i].frequencyHz = clamp(m_points[i].frequencyHz, minF, maxF);
    }
    for (int i = 1; i < m_points.size() - 1; ++i) {
        double wantMin = m_points[i - 1].frequencyHz + spacing;
        if (m_points[i].frequencyHz < wantMin) m_points[i].frequencyHz = wantMin;
    }
    for (int i = m_points.size() - 2; i >= 1; --i) {
        double wantMax = m_points[i + 1].frequencyHz - spacing;
        if (m_points[i].frequencyHz > wantMax) m_points[i].frequencyHz = wantMax;
    }
    m_points.front().frequencyHz = m_frequencyMinHz;
    m_points.back().frequencyHz  = m_frequencyMaxHz;
}

// From Thetis ucParametricEq.cs:3163-3197 [v2.10.3.13].
void ParametricEqWidget::resetPointsDefault() {
    m_points.clear();
    int count = m_bandCount;
    if (count < 2) count = 2;

    m_selectedIndex      = -1;
    m_dragIndex          = -1;
    m_draggingPoint      = false;
    m_draggingGlobalGain = false;
    m_dragDirtyPoint         = false;
    m_dragDirtyGlobalGain    = false;
    m_dragDirtySelectedIndex = false;

    double span = m_frequencyMaxHz - m_frequencyMinHz;
    if (span <= 0.0) span = 1.0;

    for (int i = 0; i < count; ++i) {
        double t = double(i) / double(count - 1);
        double f = m_frequencyMinHz + t * span;
        QColor col = getBandBaseColor(i);
        m_points.append(EqPoint(i + 1, col, f, 0.0, 4.0));
    }

    enforceOrdering(true);
    clampAllGains();
    clampAllQ();
}

// From Thetis ucParametricEq.cs:3199-3221 [v2.10.3.13].
void ParametricEqWidget::rescaleFrequencies(
    double oldMin, double oldMax, double newMin, double newMax) {
    double oldSpan = oldMax - oldMin;
    double newSpan = newMax - newMin;
    if (oldSpan <= 0.0) oldSpan = 1.0;
    if (newSpan <= 0.0) newSpan = 1.0;

    for (auto& p : m_points) {
        double t = (p.frequencyHz - oldMin) / oldSpan;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        p.frequencyHz = newMin + t * newSpan;
    }
    if (m_points.size() > 0) m_points.front().frequencyHz = m_frequencyMinHz;
    if (m_points.size() > 1) m_points.back().frequencyHz  = m_frequencyMaxHz;
}

// From Thetis ucParametricEq.cs:3080-3161 [v2.10.3.13].
QVector<double> ParametricEqWidget::getLogFrequencyTicks(const QRect& plot) const {
    auto add = [this](QVector<double>& ticks, double f) {
        if (std::isnan(f) || std::isinf(f)) return;
        if (f < m_frequencyMinHz || f > m_frequencyMaxHz) return;
        ticks.append(f);
    };

    QVector<double> candidates;
    add(candidates, m_frequencyMinHz);
    add(candidates, getLogFrequencyCentreHz(m_frequencyMinHz, m_frequencyMaxHz));
    add(candidates, m_frequencyMaxHz);

    if (m_frequencyMinHz <= 0.0 && m_frequencyMaxHz >= 0.0) {
        add(candidates, 0.0);
    }

    double positiveMin = m_frequencyMinHz > 0.0 ? m_frequencyMinHz : 1.0;
    double positiveMax = m_frequencyMaxHz;
    if (positiveMax > 0.0) {
        int expMin = int(std::floor(std::log10(positiveMin)));
        int expMax = int(std::ceil (std::log10(positiveMax)));
        const double mults[] = {1.0, 2.0, 5.0};
        for (int e = expMin; e <= expMax; ++e) {
            double decade = std::pow(10.0, e);
            for (double m : mults) {
                add(candidates, m * decade);
            }
        }
    }
    std::sort(candidates.begin(), candidates.end());

    QVector<double> uniq;
    for (double f : candidates) {
        if (!uniq.isEmpty() && qAbs(uniq.last() - f) < 0.000001) continue;
        uniq.append(f);
    }
    if (uniq.size() <= 2) return uniq;

    QVector<double> filtered;
    constexpr double kMinSpacingPx = 28.0;
    for (int i = 0; i < uniq.size(); ++i) {
        double f = uniq.at(i);
        bool keep = (i == 0) || (i == uniq.size() - 1);
        if (!keep) {
            float x = xFromFreq(plot, f);
            bool farEnough = true;
            for (double k : filtered) {
                float kx = xFromFreq(plot, k);
                if (qAbs(x - kx) < kMinSpacingPx) {
                    farEnough = false;
                    break;
                }
            }
            keep = farEnough;
        }
        if (keep) filtered.append(f);
    }
    return filtered;
}

// =================================================================
// Paint helpers + bar chart timer + response-curve math
// (Phase 3M-3a-ii follow-up Batch 3).  Every helper is a line-faithful
// translation of ucParametricEq.cs [v2.10.3.13] -- each function carries
// a verbatim cite immediately above its definition.  WinForms ->
// Qt mapping notes:
//   - Graphics.SetClip(rect) -> QPainter::save()/setClipRect()/restore()
//   - GraphicsPath / DrawPath -> QPainterPath / QPainter::drawPath
//   - LinearGradientBrush -> QLinearGradient set on a QBrush
//   - SolidBrush -> stack QBrush (no IDisposable boilerplate)
//   - Color.FromArgb(a,r,g,b) -> QColor(r,g,b,a)  -- argument order differs!
//   - Math.Round -> std::round
//   - DateTime.UtcNow -> QDateTime::currentMSecsSinceEpoch()
//   - Qt QRect::right()/bottom() return x+width-1/y+height-1 (last pixel,
//     inclusive) whereas WinForms Rectangle.Right/Bottom return x+width
//     (exclusive).  Hit boxes + pixel placement are internally consistent
//     against Qt semantics, so user behavior is preserved -- but a side-by-
//     side screenshot vs Thetis on Windows will show a 1-px shift on the
//     right + bottom edges of the plot rect.  Not a bug; document only.
// =================================================================

// From Thetis ucParametricEq.cs:2887-2891 [v2.10.3.13].
QString ParametricEqWidget::formatHz(double hz) const {
    if (hz >= 1000.0) {
        return QStringLiteral("%1 kHz").arg(hz / 1000.0, 0, 'f', 3);
    }
    return QStringLiteral("%1 Hz").arg(hz, 0, 'f', 0);
}

// From Thetis ucParametricEq.cs:2893-2896 [v2.10.3.13].
QString ParametricEqWidget::formatDotReadingHz(double hz) const {
    return QStringLiteral("%1 Hz").arg(hz, 0, 'f', 0);
}

// From Thetis ucParametricEq.cs:2898-2902 [v2.10.3.13].
QString ParametricEqWidget::formatDb(double db) const {
    QString sign = db >= 0.0 ? QStringLiteral("+") : QString();
    return sign + QStringLiteral("%1 dB").arg(db, 0, 'f', 1);
}

// From Thetis ucParametricEq.cs:2904-2908 [v2.10.3.13].
QString ParametricEqWidget::formatDotReadingDb(double db) const {
    QString sign = db >= 0.0 ? QStringLiteral("+") : QString();
    return sign + QStringLiteral("%1 dB").arg(db, 0, 'f', 1);
}

// From Thetis ucParametricEq.cs:2601-2614 [v2.10.3.13].
// C# format strings:
//   "0"     -> integer, no decimal
//   "0.#"   -> 1 optional decimal (trailing zero suppressed)
//   "0.##"  -> 2 optional decimals (trailing zeros suppressed)
// Qt's QString::number / arg lacks "trailing-zero suppression" for 'f',
// so we strip them by hand to keep behavior identical.
QString ParametricEqWidget::formatDbTick(double db, double stepDb) const {
    auto fmtTrimmed = [](double v, int maxDecimals) {
        QString s = QString::number(v, 'f', maxDecimals);
        if (maxDecimals > 0 && s.contains(QLatin1Char('.'))) {
            int i = s.size() - 1;
            while (i > 0 && s.at(i) == QLatin1Char('0')) --i;
            if (s.at(i) == QLatin1Char('.')) --i;
            s.truncate(i + 1);
        }
        return s;
    };

    int maxDecimals = 0; // "0"
    double absStep = std::fabs(stepDb);
    if (absStep < 1.0) {
        maxDecimals = 2; // "0.##"
    } else if (std::fabs(absStep - std::round(absStep)) > 0.000001) {
        maxDecimals = 1; // "0.#"
    }

    if (std::fabs(db) < 0.000001) return QStringLiteral("0");
    if (db > 0.0) return QStringLiteral("+") + fmtTrimmed(db, maxDecimals);
    return fmtTrimmed(db, maxDecimals);
}

// From Thetis ucParametricEq.cs:2616-2630 [v2.10.3.13].
QString ParametricEqWidget::formatHzTick(double hz) const {
    auto fmtTrimmed = [](double v, int maxDecimals) {
        QString s = QString::number(v, 'f', maxDecimals);
        if (maxDecimals > 0 && s.contains(QLatin1Char('.'))) {
            int i = s.size() - 1;
            while (i > 0 && s.at(i) == QLatin1Char('0')) --i;
            if (s.at(i) == QLatin1Char('.')) --i;
            s.truncate(i + 1);
        }
        return s;
    };

    double absHz = std::fabs(hz);

    if (absHz >= 1000.0) {
        double khz = hz / 1000.0;
        if (std::fabs(khz) >= 10.0) {
            return fmtTrimmed(khz, 1) + QStringLiteral("k");
        }
        return fmtTrimmed(khz, 2) + QStringLiteral("k");
    }

    if (absHz >= 100.0) return fmtTrimmed(hz, 0);
    if (absHz >=  10.0) return fmtTrimmed(hz, 1);
    return fmtTrimmed(hz, 2);
}

// From Thetis ucParametricEq.cs:2873-2885 [v2.10.3.13].
// Note: C# uses (p.BandId - 1) on the palette index, so band IDs starting
// from 1 align with palette index 0.  Preserved verbatim.
QColor ParametricEqWidget::getPointDisplayColor(int index) const {
    const EqPoint& p = m_points.at(index);

    QColor baseCol = p.bandColor;
    if (!baseCol.isValid()) baseCol = getBandBaseColor(p.bandId - 1);

    QColor dotCol = m_usePerBandColours ? baseCol : QColor(90, 200, 255);

    if (index == m_selectedIndex) dotCol = QColor(255, 200, 80);

    return dotCol;
}

// From Thetis ucParametricEq.cs:2694-2748 [v2.10.3.13].
// THE core math.  Two branches:
//   - Graphic-EQ mode (!m_parametricEq): linear interpolation between
//     adjacent points, clamped to first/last gain at the edges.
//   - Parametric mode: Gaussian-weighted sum.  Each point is a Gaussian
//     centered at p.frequencyHz with FWHM = span / (q*3) (clamped to
//     min span/6000), sigma = FWHM / 2.3548200450309493 (the
//     2*sqrt(2*ln(2)) FWHM-to-sigma conversion).  Weight w = exp(-0.5*d^2)
//     where d = (f - p.frequencyHz)/sigma.  Per-point contribution is
//     p.gainDb * w; widget result is the unweighted sum.  This is the
//     authoritative response curve for paint and CFC dispatch.
double ParametricEqWidget::responseDbAtFrequency(double frequencyHz) const {
    if (!m_parametricEq) {
        if (m_points.isEmpty()) return 0.0;
        double f = frequencyHz;
        if (f <= m_points.first().frequencyHz) return m_points.first().gainDb;
        if (f >= m_points.last ().frequencyHz) return m_points.last ().gainDb;

        for (int i = 1; i < m_points.size(); ++i) {
            const auto& left  = m_points.at(i - 1);
            const auto& right = m_points.at(i);
            if (f <= right.frequencyHz) {
                double denom = right.frequencyHz - left.frequencyHz;
                if (denom <= 0.0000001) return right.gainDb;
                double t = (f - left.frequencyHz) / denom;
                if (t < 0.0) t = 0.0;
                if (t > 1.0) t = 1.0;
                return left.gainDb + ((right.gainDb - left.gainDb) * t);
            }
        }
        return m_points.last().gainDb;
    }

    double span = m_frequencyMaxHz - m_frequencyMinHz;
    if (span <= 0.0) span = 1.0;

    double sum = 0.0;
    for (const auto& p : m_points) {
        double q = clamp(p.q, m_qMin, m_qMax);
        double fwhm = span / (q * 3.0);
        double minFwhm = span / 6000.0;
        if (fwhm < minFwhm) fwhm = minFwhm;
        double sigma = fwhm / 2.3548200450309493;
        double d = (frequencyHz - p.frequencyHz) / sigma;
        double w = std::exp(-0.5 * d * d);
        sum += p.gainDb * w;
    }
    return sum;
}

// From Thetis ucParametricEq.cs:1575-1609 [v2.10.3.13].
void ParametricEqWidget::paintEvent(QPaintEvent* /*event*/) {
    QPainter g(this);
    g.setRenderHint(QPainter::Antialiasing);

    QRect client = rect();
    if (client.width() < 2 || client.height() < 2) return;

    g.fillRect(client, QColor(25, 25, 25));   // BackColor (ucParametricEq.cs:443)

    QRect plot = computePlotRect();
    if (plot.width() < 2 || plot.height() < 2) return;

    drawGrid(g, plot);

    g.save();
    g.setClipRect(plot);

    if (m_showBandShading) drawBandShading(g, plot);
    drawCurve (g, plot);
    drawPoints(g, plot);

    g.restore();

    if (m_showAxisScales) drawAxisScales(g, plot);

    drawGlobalGainHandle(g, plot);
    drawBorder           (g, plot);
    if (m_showReadout) drawReadout(g, plot);
}

// From Thetis ucParametricEq.cs:2061-2117 [v2.10.3.13].
void ParametricEqWidget::drawGrid(QPainter& g, const QRect& plot) {
    g.fillRect(plot, QColor(18, 18, 18));

    drawBarChart(g, plot);

    QPen gridPen(QColor(45, 45, 45), 1.0);
    g.setPen(gridPen);

    if (m_logScale) {
        QVector<double> ticks = getLogFrequencyTicks(plot);
        for (double t : ticks) {
            float x = xFromFreq(plot, t);
            g.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
    } else {
        constexpr int kVLines = 10;
        for (int i = 0; i <= kVLines; ++i) {
            float t = float(i) / float(kVLines);
            int x = plot.left() + int(std::round(double(t) * double(plot.width())));
            g.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        }
    }

    double stepDb = getYAxisStepDb();
    double start  = std::ceil(m_dbMin / stepDb) * stepDb;
    for (double db = start; db <= m_dbMax + 0.000001; db += stepDb) {
        float y = yFromDb(plot, db);
        g.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }

    // Zero-dB grid line: 1.5px width, brighter grey (75,75,75).
    QPen zeroPen(QColor(75, 75, 75), 1.5);
    g.setPen(zeroPen);
    float y0 = yFromDb(plot, 0.0);
    g.drawLine(QPointF(plot.left(), y0), QPointF(plot.right(), y0));

    if (m_globalGainIsHorizLine) {
        QPen globalGainPen(QColor(255, 255, 255, 180), 1.5);
        globalGainPen.setStyle(Qt::DashLine);
        g.setPen(globalGainPen);
        float y = yFromDb(plot, m_globalGainDb);
        g.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
    }
}

// From Thetis ucParametricEq.cs:2119-2190 [v2.10.3.13].
void ParametricEqWidget::drawBarChart(QPainter& g, const QRect& plot) {
    if (m_barChartData.isEmpty()) return;

    applyBarChartPeakDecay(QDateTime::currentMSecsSinceEpoch());

    QBrush barBrush(QColor(m_barChartFillColor.red(),
                           m_barChartFillColor.green(),
                           m_barChartFillColor.blue(),
                           m_barChartFillAlpha));
    QPen   peakPen(QColor(m_barChartPeakColor.red(),
                          m_barChartPeakColor.green(),
                          m_barChartPeakColor.blue(),
                          m_barChartPeakAlpha), 1.0);

    int count = m_barChartData.size();

    for (int i = 0; i < count; ++i) {
        int segLeft;
        int segRight;

        if (m_logScale) {
            double edgeLeftF  = frequencyFromNormalizedPosition(double(i)     / double(count));
            double edgeRightF = frequencyFromNormalizedPosition(double(i + 1) / double(count));

            segLeft  = int(std::round(double(xFromFreq(plot, edgeLeftF))));
            segRight = int(std::round(double(xFromFreq(plot, edgeRightF))));
        } else {
            segLeft  = plot.left() + int(std::round((double(i)     * double(plot.width())) / double(count)));
            segRight = plot.left() + int(std::round((double(i + 1) * double(plot.width())) / double(count)));
        }

        int segWidth = segRight - segLeft;
        if (segWidth <= 0) continue;

        int barWidth = segWidth > 1 ? segWidth - 1 : segWidth;
        if (barWidth < 1) barWidth = 1;

        double db  = clamp(m_barChartData.at(i), m_dbMin, m_dbMax);
        int    top = int(std::round(double(yFromDb(plot, db))));
        int    height = plot.bottom() - top;
        if (height < 1) height = 1;
        if (top < plot.top()) {
            height -= (plot.top() - top);
            top = plot.top();
        }
        if (top + height > plot.bottom()) {
            height = plot.bottom() - top;
        }

        if (height > 0) {
            g.fillRect(QRect(segLeft, top, barWidth, height), barBrush);
        }

        if (m_barChartPeakHoldEnabled
            && i < m_barChartPeakData.size()) {
            double peakDb = clamp(m_barChartPeakData.at(i), m_dbMin, m_dbMax);
            int peakY = int(std::round(double(yFromDb(plot, peakDb))));
            if (peakY < plot.top()) peakY = plot.top();
            if (peakY > plot.bottom() - 1) peakY = plot.bottom() - 1;

            int peakX2 = segLeft + barWidth - 1;
            if (peakX2 < segLeft) peakX2 = segLeft;

            g.setPen(peakPen);
            g.drawLine(QPointF(segLeft, peakY), QPointF(peakX2, peakY));
        }
    }
}

// From Thetis ucParametricEq.cs:2192-2342 [v2.10.3.13].
// Two branches:
//   - Parametric: per-band Gaussian-weighted polygon (samples+2 vertices,
//     baseline anchors at left/right plot edges) using fillPolygon.
//   - Graphic-EQ: per-segment 4-vertex trapezoid with horizontal linear
//     gradient between adjacent point colors.
void ParametricEqWidget::drawBandShading(QPainter& g, const QRect& plot) {
    if (m_points.isEmpty()) return;

    if (m_parametricEq) {
        int samples = plot.width();
        if (samples < 64) samples = 64;

        double span = m_frequencyMaxHz - m_frequencyMinHz;
        if (span <= 0.0) span = 1.0;

        float baselineY = yFromDb(plot, 0.0);

        for (int band = 0; band < m_points.size(); ++band) {
            const EqPoint& p = m_points.at(band);
            if (std::fabs(p.gainDb) < 0.000001) continue;

            double q = clamp(p.q, m_qMin, m_qMax);
            double fwhm = span / (q * 3.0);
            double minFwhm = span / 6000.0;
            if (fwhm < minFwhm) fwhm = minFwhm;
            double sigma = fwhm / 2.3548200450309493;

            QColor baseCol = p.bandColor;
            if (!baseCol.isValid()) baseCol = getBandBaseColor(band);

            QColor fillCol(baseCol.red(), baseCol.green(), baseCol.blue(), m_bandShadeAlpha);
            if (!m_usePerBandColours) {
                fillCol = QColor(m_bandShadeColor.red(),
                                 m_bandShadeColor.green(),
                                 m_bandShadeColor.blue(),
                                 m_bandShadeAlpha);
            }

            QPolygonF poly;
            poly.reserve(samples + 2);
            poly.append(QPointF(plot.left(), baselineY));

            for (int i = 0; i < samples; ++i) {
                double t = double(i) / double(samples - 1);
                double f = m_frequencyMinHz + t * span;

                double d = (f - p.frequencyHz) / sigma;
                double w = std::exp(-0.5 * d * d);

                double bandDb = 0.0;
                if (w >= m_bandShadeWeightCutoff) bandDb = p.gainDb * w;

                float x = m_logScale
                    ? xFromFreq(plot, f)
                    : float(double(plot.left()) + t * double(plot.width()));
                float y = yFromDb(plot, bandDb);
                poly.append(QPointF(x, y));
            }

            poly.append(QPointF(plot.right(), baselineY));

            g.setPen(Qt::NoPen);
            g.setBrush(QBrush(fillCol));
            g.drawPolygon(poly, Qt::WindingFill);
        }
        return;
    }

    // Graphic-EQ mode: per-segment trapezoids w/ horizontal linear gradients.
    float baseY = yFromDb(plot, 0.0);

    double prevF = m_frequencyMinHz;
    double prevDb = 0.0;
    QColor prevCol;

    if (!m_points.isEmpty()) {
        prevCol = m_points.first().bandColor;
        if (!prevCol.isValid()) prevCol = getBandBaseColor(0);
    } else {
        prevCol = m_bandShadeColor;
    }

    for (int i = 0; i <= m_points.size(); ++i) {
        double nextF;
        double nextDb;
        QColor nextCol;

        if (i < m_points.size()) {
            const EqPoint& p = m_points.at(i);
            nextF = p.frequencyHz;
            nextDb = p.gainDb;
            nextCol = p.bandColor;
            if (!nextCol.isValid()) nextCol = getBandBaseColor(i);
        } else {
            nextF = m_frequencyMaxHz;
            nextDb = 0.0;
            if (!m_points.isEmpty()) {
                nextCol = m_points.last().bandColor;
                if (!nextCol.isValid()) nextCol = getBandBaseColor(m_points.size() - 1);
            } else {
                nextCol = m_bandShadeColor;
            }
        }

        if (nextF < m_frequencyMinHz) nextF = m_frequencyMinHz;
        if (nextF > m_frequencyMaxHz) nextF = m_frequencyMaxHz;

        float x0 = xFromFreq(plot, prevF);
        float x1 = xFromFreq(plot, nextF);
        if (std::fabs(x1 - x0) < 0.5f) {
            prevF = nextF;
            prevDb = nextDb;
            prevCol = nextCol;
            continue;
        }

        float y0 = yFromDb(plot, prevDb);
        float y1 = yFromDb(plot, nextDb);

        QPolygonF poly;
        poly.reserve(4);
        poly.append(QPointF(x0, baseY));
        poly.append(QPointF(x0, y0));
        poly.append(QPointF(x1, y1));
        poly.append(QPointF(x1, baseY));

        QColor c0 = prevCol;
        QColor c1 = nextCol;
        if (!m_usePerBandColours) {
            c0 = m_bandShadeColor;
            c1 = m_bandShadeColor;
        }

        QColor a0(c0.red(), c0.green(), c0.blue(), m_bandShadeAlpha);
        QColor a1(c1.red(), c1.green(), c1.blue(), m_bandShadeAlpha);

        QLinearGradient grad(QPointF(x0, 0.0), QPointF(x1, 0.0));
        grad.setColorAt(0.0, a0);
        grad.setColorAt(1.0, a1);

        g.setPen(Qt::NoPen);
        g.setBrush(QBrush(grad));
        g.drawPolygon(poly, Qt::WindingFill);

        prevF = nextF;
        prevDb = nextDb;
        prevCol = nextCol;
    }
}

// From Thetis ucParametricEq.cs:2344-2370 [v2.10.3.13].
void ParametricEqWidget::drawCurve(QPainter& g, const QRect& plot) {
    if (m_points.isEmpty()) return;

    int samples = plot.width();
    if (samples < 64) samples = 64;

    QVector<QPointF> pts;
    pts.reserve(samples);

    for (int i = 0; i < samples; ++i) {
        double t = double(i) / double(samples - 1);
        double f = m_logScale
            ? frequencyFromNormalizedPosition(t)
            : (m_frequencyMinHz + t * (m_frequencyMaxHz - m_frequencyMinHz));

        double db = responseDbAtFrequency(f);
        if (!m_globalGainIsHorizLine) db = db + m_globalGainDb;

        float x = m_logScale
            ? xFromFreq(plot, f)
            : float(double(plot.left()) + t * double(plot.width()));
        float y = yFromDb(plot, db);
        pts.append(QPointF(x, y));
    }

    QPen curvePen(Qt::white, 2.0);
    g.setPen(curvePen);
    g.setBrush(Qt::NoBrush);
    g.drawPolyline(pts.constData(), pts.size());
}

// From Thetis ucParametricEq.cs:2372-2393 [v2.10.3.13].
void ParametricEqWidget::drawGlobalGainHandle(QPainter& g, const QRect& plot) {
    float y = yFromDb(plot, m_globalGainDb);

    int hx = plot.right() + m_globalHandleXOffset;
    int s  = m_globalHandleSize;

    QPolygon tri;
    tri << QPoint(hx,     int(std::round(y)))
        << QPoint(hx + s, int(std::round(y)) - s)
        << QPoint(hx + s, int(std::round(y)) + s);

    g.setBrush(QBrush(Qt::white));
    g.setPen(Qt::NoPen);
    g.drawPolygon(tri);

    QPen outline(QColor(40, 40, 40), 1.0);
    g.setPen(outline);
    g.setBrush(Qt::NoBrush);
    g.drawPolygon(tri);
}

// From Thetis ucParametricEq.cs:2395-2426 [v2.10.3.13].
void ParametricEqWidget::drawPoints(QPainter& g, const QRect& plot) {
    for (int i = 0; i < m_points.size(); ++i) {
        const EqPoint& p = m_points.at(i);

        float x = xFromFreq(plot, p.frequencyHz);
        float y = yFromDb  (plot, p.gainDb);

        bool selected = (i == m_selectedIndex);

        QColor dotCol = getPointDisplayColor(i);

        float r = float(m_pointRadius);
        if (selected) r = r + 1.0f;

        g.setBrush(QBrush(dotCol));
        g.setPen(Qt::NoPen);
        g.drawEllipse(QPointF(x, y), r, r);

        QPen outline(QColor(35, 35, 35), 1.0);
        g.setPen(outline);
        g.setBrush(Qt::NoBrush);
        g.drawEllipse(QPointF(x, y), r, r);

        if (m_showDotReadings && m_draggingPoint && i == m_dragIndex) {
            drawDotReading(g, plot, p, x, y, r);
        }
    }
}

// From Thetis ucParametricEq.cs:2428-2503 [v2.10.3.13].
// Includes inlined createRoundedRectPath (cs:2480-2503) since QPainterPath
// builds the same arcs natively without a helper.  See the QRectF::adjusted
// + arcMoveTo/arcTo block below.
void ParametricEqWidget::drawDotReading(QPainter& g, const QRect& plot,
                                        const EqPoint& p,
                                        float dotX, float dotY, float dotRadius) {
    QString text = QStringLiteral("F ") + formatDotReadingHz(p.frequencyHz)
                 + QStringLiteral("   ") + (m_showDotReadingsAsComp ? QStringLiteral("C") : QStringLiteral("G"))
                 + QStringLiteral(" ") + formatDotReadingDb(p.gainDb);
    if (m_parametricEq) {
        text += QStringLiteral("   Q ") + QString::number(p.q, 'f', 2);
    }

    QFontMetrics fm(font());
    QSize textSize(fm.horizontalAdvance(text), fm.height());

    int   padX     = 8;
    int   padY     = 5;
    qreal radius   = 8.0;
    float gap      = 8.0f;
    float sideGap  = 14.0f;
    float edgePad  = 2.0f;
    float plotCenterX = float(plot.left()) + (float(plot.width()) * 0.5f);
    bool  preferLeft  = dotX >= plotCenterX;

    QRectF panel(0.0, 0.0,
                 textSize.width() + (padX * 2),
                 textSize.height() + (padY * 2));

    panel.moveLeft(dotX - (panel.width() * 0.5));
    panel.moveTop (dotY - dotRadius - gap - panel.height());

    bool flipBelow = panel.top() < plot.top() + edgePad;
    if (flipBelow) {
        panel.moveTop(dotY + dotRadius + gap);
        panel.moveLeft(preferLeft ? (dotX - sideGap - panel.width())
                                  : (dotX + sideGap));
    }

    if (panel.bottom() > plot.bottom() - edgePad) {
        panel.moveTop(dotY - dotRadius - gap - panel.height());
        panel.moveLeft(dotX - (panel.width() * 0.5));
    }

    if (panel.left()  < plot.left() + edgePad) panel.moveLeft(plot.left() + edgePad);
    if (panel.right() > plot.right() - edgePad) panel.moveLeft(plot.right() - edgePad - panel.width());
    if (panel.top()   < plot.top() + edgePad)  panel.moveTop (plot.top() + edgePad);
    if (panel.bottom() > plot.bottom() - edgePad) panel.moveTop(plot.bottom() - edgePad - panel.height());

    // Inlined createRoundedRectPath (cs:2480-2503).
    QPainterPath path;
    {
        qreal d = radius * 2.0;
        if (d > panel.width())  d = panel.width();
        if (d > panel.height()) d = panel.height();
        if (d < 2.0) {
            path.addRect(panel);
        } else {
            path.addRoundedRect(panel, d * 0.5, d * 0.5);
        }
    }

    QBrush panelBrush(QColor(18, 18, 18, 150));
    QPen   panelPen  (QColor(255, 220, 120, 90), 1.0);
    QBrush textBrush (QColor(255, 235, 90));

    g.setBrush(panelBrush);
    g.setPen(Qt::NoPen);
    g.drawPath(path);

    g.setBrush(Qt::NoBrush);
    g.setPen(panelPen);
    g.drawPath(path);

    g.setPen(QPen(textBrush.color()));
    g.drawText(QPointF(panel.left() + padX,
                       panel.top()  + padY + fm.ascent()),
               text);
}

// From Thetis ucParametricEq.cs:2505-2599 [v2.10.3.13].
void ParametricEqWidget::drawAxisScales(QPainter& g, const QRect& plot) {
    QPen   tickPen(m_axisTickColor, 1.0);
    QBrush textBrush(m_axisTextColor);

    g.setPen(tickPen);

    QFontMetrics fm(font());

    double stepDb  = getYAxisStepDb();
    double startDb = std::ceil(m_dbMin / stepDb) * stepDb;

    bool drewMin = false;
    bool drewMax = false;

    auto drawDbLabel = [&](double db) {
        float y = yFromDb(plot, db);
        g.setPen(tickPen);
        g.drawLine(QPointF(plot.left() - m_axisTickLength, y),
                   QPointF(plot.left(), y));

        QString s = formatDbTick(db, stepDb);
        QSizeF  sz(fm.horizontalAdvance(s), fm.height());
        float tx = float(plot.left()) - float(m_axisTickLength) - 4.0f - float(sz.width());
        float ty = y - float(sz.height()) * 0.5f;
        g.setPen(QPen(textBrush.color()));
        g.drawText(QPointF(tx, ty + fm.ascent()), s);
    };

    for (double db = startDb; db <= m_dbMax + 0.000001; db += stepDb) {
        if (std::fabs(db - m_dbMin) < 0.000001) drewMin = true;
        if (std::fabs(db - m_dbMax) < 0.000001) drewMax = true;
        drawDbLabel(db);
    }

    if (!drewMin) drawDbLabel(m_dbMin);
    if (!drewMax) drawDbLabel(m_dbMax);

    double span = m_frequencyMaxHz - m_frequencyMinHz;
    if (span <= 0.0) span = 1.0;

    float tickTop    = float(plot.bottom());
    float tickBottom = float(plot.bottom() + m_axisTickLength);
    float labelsY    = float(plot.bottom() + m_axisTickLength + 2);

    auto drawHzLabel = [&](double f) {
        float x = xFromFreq(plot, f);
        g.setPen(tickPen);
        g.drawLine(QPointF(x, tickTop), QPointF(x, tickBottom));

        QString s = formatHzTick(f);
        QSizeF  sz(fm.horizontalAdvance(s), fm.height());
        float tx = x - float(sz.width()) * 0.5f;
        float ty = labelsY;
        g.setPen(QPen(textBrush.color()));
        g.drawText(QPointF(tx, ty + fm.ascent()), s);
    };

    if (m_logScale) {
        QVector<double> ticks = getLogFrequencyTicks(plot);
        for (double f : ticks) drawHzLabel(f);
    } else {
        double stepF = chooseFrequencyStep(span);
        double first = std::ceil(m_frequencyMinHz / stepF) * stepF;
        for (double f = first; f <= m_frequencyMaxHz + 0.000001; f += stepF) {
            drawHzLabel(f);
        }
    }
}

// From Thetis ucParametricEq.cs:2645-2651 [v2.10.3.13].
void ParametricEqWidget::drawBorder(QPainter& g, const QRect& plot) {
    QPen border(QColor(70, 70, 70), 1.0);
    g.setPen(border);
    g.setBrush(Qt::NoBrush);
    g.drawRect(plot);
}

// From Thetis ucParametricEq.cs:2653-2692 [v2.10.3.13].
void ParametricEqWidget::drawReadout(QPainter& g, const QRect& plot) {
    QFontMetrics fm(font());
    int readoutY = rect().bottom() - (fm.height() + 4);
    int x        = plot.left();

    QString s;

    if (m_selectedIndex >= 0 && m_selectedIndex < m_points.size()) {
        const EqPoint& p = m_points.at(m_selectedIndex);
        int bandId = p.bandId;

        if (m_parametricEq) {
            s = QStringLiteral("P%1  F %2  G %3  Q %4")
                    .arg(bandId)
                    .arg(formatHz(p.frequencyHz))
                    .arg(formatDb(p.gainDb))
                    .arg(p.q, 0, 'f', 2);
            s += QStringLiteral("     Global ") + formatDb(m_globalGainDb);
        } else {
            s = QStringLiteral("P%1  F %2  G %3")
                    .arg(bandId)
                    .arg(formatHz(p.frequencyHz))
                    .arg(formatDb(p.gainDb));
            s += QStringLiteral("     Global ") + formatDb(m_globalGainDb);
        }
    } else {
        s = QStringLiteral("Global ") + formatDb(m_globalGainDb);
    }

    g.setPen(QPen(palette().color(foregroundRole())));
    g.drawText(QPointF(x, readoutY + fm.ascent()), s);
}

// =================================================================
// Bar chart timer + decay (Phase 3M-3a-ii follow-up Batch 3).
// =================================================================

// From Thetis ucParametricEq.cs:1048-1105 [v2.10.3.13] -- public DrawBarChart slot.
void ParametricEqWidget::drawBarChartData(const QVector<double>& data) {
    if (data.isEmpty()) {
        m_barChartData.clear();
        m_barChartPeakData.clear();
        m_barChartPeakHoldUntilMs.clear();
        updateBarChartPeakTimerState();
        update();
        return;
    }

    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    applyBarChartPeakDecay(nowMs);

    bool resetPeaks = (m_barChartData.size() != data.size())
                   || (m_barChartPeakData.size() != data.size())
                   || (m_barChartPeakHoldUntilMs.size() != data.size());

    m_barChartData = data;

    if (resetPeaks) {
        m_barChartPeakData.fill(0.0, data.size());
        m_barChartPeakHoldUntilMs.fill(0, data.size());
    }

    qint64 holdUntil = nowMs + m_barChartPeakHoldMs;
    for (int i = 0; i < m_barChartData.size(); ++i) {
        double v = clamp(m_barChartData.at(i), m_dbMin, m_dbMax);
        m_barChartData[i] = v;
        if (resetPeaks || !m_barChartPeakHoldEnabled) {
            m_barChartPeakData[i]        = v;
            m_barChartPeakHoldUntilMs[i] = holdUntil;
        } else if (v >= m_barChartPeakData.at(i)) {
            m_barChartPeakData[i]        = v;
            m_barChartPeakHoldUntilMs[i] = holdUntil;
        } else if (m_barChartPeakData.at(i) < v) {
            m_barChartPeakData[i] = v;
        }
    }
    m_barChartPeakLastUpdateMs = nowMs;
    updateBarChartPeakTimerState();
    update();
}

// From Thetis ucParametricEq.cs:2769-2815 [v2.10.3.13].
// Decay rate: m_barChartPeakDecayDbPerSecond dB / second (Thetis default 20).
// Each tick computes elapsed seconds since the last applied tick, decays
// every peak that's past its hold deadline by (rate * elapsed) dB but
// never below the current bar value (the "floor").
void ParametricEqWidget::applyBarChartPeakDecay(qint64 nowMs) {
    if (m_barChartData.isEmpty()
        || m_barChartPeakData.isEmpty()
        || m_barChartPeakHoldUntilMs.isEmpty()) {
        m_barChartPeakLastUpdateMs = nowMs;
        return;
    }

    if (m_barChartPeakData.size() != m_barChartData.size()
        || m_barChartPeakHoldUntilMs.size() != m_barChartData.size()) {
        syncBarChartPeaksToData();
        m_barChartPeakLastUpdateMs = nowMs;
        return;
    }

    double elapsedSeconds = double(nowMs - m_barChartPeakLastUpdateMs) / 1000.0;
    if (elapsedSeconds < 0.0) elapsedSeconds = 0.0;
    m_barChartPeakLastUpdateMs = nowMs;

    if (!m_barChartPeakHoldEnabled) {
        syncBarChartPeaksToData();
        return;
    }

    if (elapsedSeconds <= 0.0) return;

    double decayDb = m_barChartPeakDecayDbPerSecond * elapsedSeconds;

    for (int i = 0; i < m_barChartData.size(); ++i) {
        double floorDb = clamp(m_barChartData.at(i), m_dbMin, m_dbMax);

        if (m_barChartPeakData.at(i) < floorDb) {
            m_barChartPeakData[i] = floorDb;
            continue;
        }

        if (nowMs <= m_barChartPeakHoldUntilMs.at(i)) continue;

        double newPeak = m_barChartPeakData.at(i) - decayDb;
        if (newPeak < floorDb) newPeak = floorDb;
        m_barChartPeakData[i] = newPeak;
    }
}

// From Thetis ucParametricEq.cs:2817-2843 [v2.10.3.13].
void ParametricEqWidget::syncBarChartPeaksToData() {
    if (m_barChartData.isEmpty()) {
        m_barChartPeakData.clear();
        m_barChartPeakHoldUntilMs.clear();
        return;
    }

    if (m_barChartPeakData.size() != m_barChartData.size()) {
        m_barChartPeakData.fill(0.0, m_barChartData.size());
    }
    if (m_barChartPeakHoldUntilMs.size() != m_barChartData.size()) {
        m_barChartPeakHoldUntilMs.fill(0, m_barChartData.size());
    }

    qint64 holdUntil = QDateTime::currentMSecsSinceEpoch() + m_barChartPeakHoldMs;

    for (int i = 0; i < m_barChartData.size(); ++i) {
        m_barChartPeakData[i]        = clamp(m_barChartData.at(i), m_dbMin, m_dbMax);
        m_barChartPeakHoldUntilMs[i] = holdUntil;
    }
}

// From Thetis ucParametricEq.cs:2845-2862 [v2.10.3.13].
void ParametricEqWidget::updateBarChartPeakTimerState() {
    bool shouldRun = !m_barChartData.isEmpty() && m_barChartPeakHoldEnabled;
    if (shouldRun) {
        if (!m_barChartPeakTimer->isActive()) {
            m_barChartPeakLastUpdateMs = QDateTime::currentMSecsSinceEpoch();
            m_barChartPeakTimer->start();
        }
    } else {
        if (m_barChartPeakTimer->isActive()) {
            m_barChartPeakTimer->stop();
        }
    }
}

// =================================================================
// Mouse + wheel interaction + raise* helpers + setters
// (Phase 3M-3a-ii follow-up Batch 4).  Each function is a line-faithful
// translation of ucParametricEq.cs [v2.10.3.13] -- cite is immediately
// above the definition.  WinForms -> Qt mapping notes:
//   - MouseEventArgs.Button == MouseButtons.Left -> event->button() == Qt::LeftButton
//   - Capture (Control.Capture flag) -> Qt grabs the mouse implicitly
//     when a press is accepted; mouseMove + mouseRelease arrive at the
//     pressed widget without further work.  We drop the explicit Capture
//     toggles -- they're a no-op in Qt.
//   - ModifierKeys & Keys.Control / Keys.Shift -> event->modifiers().testFlag(...)
//   - MouseEventArgs.Delta (-/+ 120 per click) -> event->angleDelta().y()
//     (Qt is in 1/8 degree units; standard wheel is 120 per detent.)
//   - MouseEventArgs.Location  -> event->pos()       (mouse) or
//                                  event->position().toPoint()  (wheel,
//                                  which uses QPointF in Qt6).
//   - Cursors.Hand / Cursors.Default -> setCursor(Qt::PointingHandCursor)
//                                       / unsetCursor()
// =================================================================

// From Thetis ucParametricEq.cs:1997-2000 [v2.10.3.13].
bool ParametricEqWidget::isDraggingNow() const {
    return m_draggingGlobalGain || m_draggingPoint;
}

// From Thetis ucParametricEq.cs:3334-3338 [v2.10.3.13].
void ParametricEqWidget::raisePointsChanged(bool isDragging) {
    emit pointsChanged(isDragging);
}

// From Thetis ucParametricEq.cs:3340-3344 [v2.10.3.13].
void ParametricEqWidget::raiseGlobalGainChanged(bool isDragging) {
    emit globalGainChanged(isDragging);
}

// From Thetis ucParametricEq.cs:3346-3352 [v2.10.3.13].
void ParametricEqWidget::raiseSelectedIndexChanged(bool isDragging) {
    if (isDragging) m_dragDirtySelectedIndex = true;
    emit selectedIndexChanged(isDragging);
}

// From Thetis ucParametricEq.cs:1025-1031 [v2.10.3.13].
void ParametricEqWidget::raisePointSelected(int index, const EqPoint& p) {
    emit pointSelected(index, p.bandId, p.frequencyHz, p.gainDb, p.q);
}

// From Thetis ucParametricEq.cs:1033-1039 [v2.10.3.13].
void ParametricEqWidget::raisePointUnselected(int index, const EqPoint& p) {
    emit pointUnselected(index, p.bandId, p.frequencyHz, p.gainDb, p.q);
}

// From Thetis ucParametricEq.cs:3354-3363 [v2.10.3.13].
// C# uses _points.IndexOf(p) to resolve the band index; we pass it
// through the call chain because EqPoint is a value type without an
// equality operator (and we don't want to add one).
void ParametricEqWidget::raisePointDataChanged(int index, const EqPoint& p, bool isDragging) {
    if (index < 0 || index >= m_points.size()) return;
    emit pointDataChanged(index, p.bandId, p.frequencyHz, p.gainDb, p.q, isDragging);
}

// From Thetis ucParametricEq.cs:704-720 [v2.10.3.13].
void ParametricEqWidget::setGlobalGainDb(double db) {
    double v = clamp(db, m_dbMin, m_dbMax);
    if (qAbs(v - m_globalGainDb) < 0.000001) return;

    m_globalGainDb = v;

    if (m_draggingGlobalGain) m_dragDirtyGlobalGain = true;

    raiseGlobalGainChanged(isDraggingNow());
    update();
}

// From Thetis ucParametricEq.cs:970-1005 [v2.10.3.13].
void ParametricEqWidget::setSelectedIndex(int index) {
    int v = index;
    if (v < -1) v = -1;
    if (v >= m_points.size()) v = m_points.size() - 1;
    if (v == m_selectedIndex) return;

    int oldIndex = m_selectedIndex;
    bool hadOld = (oldIndex >= 0 && oldIndex < m_points.size());
    EqPoint oldPoint;
    if (hadOld) oldPoint = m_points.at(oldIndex);

    m_selectedIndex = v;

    if (hadOld) {
        raisePointUnselected(oldIndex, oldPoint);
    }

    if (m_selectedIndex >= 0 && m_selectedIndex < m_points.size()) {
        raisePointSelected(m_selectedIndex, m_points.at(m_selectedIndex));
    }

    raiseSelectedIndexChanged(isDraggingNow());
    update();
}

// From Thetis ucParametricEq.cs:1625-1675 [v2.10.3.13].
void ParametricEqWidget::mousePressEvent(QMouseEvent* event) {
    setFocus();
    QRect plot = computePlotRect();

    if (event->button() != Qt::LeftButton) return;

    if (hitTestGlobalGainHandle(plot, event->pos())) {
        m_draggingGlobalGain = true;
        m_draggingPoint      = false;
        m_dragIndex          = -1;
        m_dragDirtyPoint         = false;
        m_dragDirtyGlobalGain    = false;
        m_dragDirtySelectedIndex = false;
        update();
        return;
    }

    if (plot.contains(event->pos())) {
        int idx = hitTestPoint(plot, event->pos());
        if (idx >= 0) {
            setSelectedIndex(idx);
            m_draggingPoint      = true;
            m_draggingGlobalGain = false;
            m_dragIndex          = idx;
            m_dragDirtyPoint         = false;
            m_dragDirtyGlobalGain    = false;
            m_dragDirtySelectedIndex = false;
            update();
            return;
        }
    }

    setSelectedIndex(-1);
    update();
}

// From Thetis ucParametricEq.cs:1677-1801 [v2.10.3.13].
// The OnMouseMove body branches on:
//   1. dragging the global gain handle      -> dbFromY -> GlobalGainDb setter
//   2. dragging a point                     -> compute (freq, gain), enforce
//                                              ordering / clamping, emit
//                                              pointsChanged(true) +
//                                              pointDataChanged(true)
//   3. neither                              -> hover: hand cursor on
//                                              hit-test, default otherwise
// `Capture` short-circuits in C# (return if capture lost) are no-ops in Qt
// because Qt does not deliver moves from a foreign press; we drop them.
void ParametricEqWidget::mouseMoveEvent(QMouseEvent* event) {
    QRect plot = computePlotRect();
    QPoint pt = event->pos();

    if (m_draggingGlobalGain) {
        double db = dbFromY(plot, pt.y());
        setGlobalGainDb(db);
        return;
    }

    if (m_draggingPoint) {
        if (m_dragIndex < 0 || m_dragIndex >= m_points.size()) return;

        EqPoint& p = m_points[m_dragIndex];

        double oldF = p.frequencyHz;
        double oldG = p.gainDb;
        double oldQ = p.q;

        double freq = p.frequencyHz;
        double gain = dbFromY(plot, pt.y());

        gain = clamp(gain, m_dbMin, m_dbMax);

        if (!isFrequencyLockedIndex(m_dragIndex)) {
            freq = freqFromX(plot, pt.x());
            freq = clamp(freq, m_frequencyMinHz, m_frequencyMaxHz);

            if (!m_allowPointReorder) {
                double minF;
                double maxF;

                if (m_dragIndex == 0) {
                    minF = m_frequencyMinHz;
                    maxF = m_points[1].frequencyHz - m_minPointSpacingHz;
                } else if (m_dragIndex == m_points.size() - 1) {
                    minF = m_points[m_points.size() - 2].frequencyHz + m_minPointSpacingHz;
                    maxF = m_frequencyMaxHz;
                } else {
                    minF = m_points[m_dragIndex - 1].frequencyHz + m_minPointSpacingHz;
                    maxF = m_points[m_dragIndex + 1].frequencyHz - m_minPointSpacingHz;
                }

                if (maxF < minF) maxF = minF;
                freq = clamp(freq, minF, maxF);
            }
        }

        bool changed = false;

        if (qAbs(p.frequencyHz - freq) > 0.000001) {
            p.frequencyHz = freq;
            changed = true;
        }

        if (qAbs(p.gainDb - gain) > 0.000001) {
            p.gainDb = gain;
            changed = true;
        }

        if (changed) {
            if (m_allowPointReorder && !isFrequencyLockedIndex(m_dragIndex)) {
                enforceOrdering(false);

                int idx = m_dragIndex;
                double minF = m_frequencyMinHz;
                double maxF = m_frequencyMaxHz;

                if (m_points.size() > 1) {
                    if (idx > 0) minF = m_points[idx - 1].frequencyHz + m_minPointSpacingHz;
                    if (idx < m_points.size() - 1) maxF = m_points[idx + 1].frequencyHz - m_minPointSpacingHz;
                    if (maxF < minF) maxF = minF;
                }

                if (idx >= 0 && idx < m_points.size()) {
                    double clampedFreq = clamp(m_points[idx].frequencyHz, minF, maxF);
                    if (qAbs(clampedFreq - m_points[idx].frequencyHz) > 0.000001) {
                        m_points[idx].frequencyHz = clampedFreq;
                    }
                }

                enforceOrdering(false);
            }

            m_dragDirtyPoint = true;

            // After enforceOrdering, m_dragIndex may have been re-resolved
            // by the bandId lookup; reread the point fresh from m_points
            // before raising the data signal.
            int curIdx = m_dragIndex;
            if (curIdx >= 0 && curIdx < m_points.size()) {
                raisePointsChanged(true);
                const EqPoint& curP = m_points.at(curIdx);
                if (qAbs(curP.frequencyHz - oldF) > 0.000001
                    || qAbs(curP.gainDb - oldG) > 0.000001
                    || qAbs(curP.q - oldQ) > 0.000001) {
                    raisePointDataChanged(curIdx, curP, true);
                }
            } else {
                raisePointsChanged(true);
            }
            update();
        }

        return;
    }

    bool wantHand = false;

    if (hitTestGlobalGainHandle(plot, pt)) {
        wantHand = true;
    } else if (plot.contains(pt)) {
        int idx = hitTestPoint(plot, pt);
        if (idx >= 0) wantHand = true;
    }

    if (wantHand) {
        setCursor(Qt::PointingHandCursor);
    } else {
        unsetCursor();
    }
}

// From Thetis ucParametricEq.cs:1803-1844 [v2.10.3.13].
void ParametricEqWidget::mouseReleaseEvent(QMouseEvent* /*event*/) {
    bool wasDraggingPoint    = m_draggingPoint;
    bool wasDraggingGlobal   = m_draggingGlobalGain;
    int dragIdx              = m_dragIndex;
    bool pointDirty          = m_dragDirtyPoint;
    bool globalDirty         = m_dragDirtyGlobalGain;
    bool selectedDirty       = m_dragDirtySelectedIndex;

    m_draggingGlobalGain     = false;
    m_draggingPoint          = false;
    m_dragIndex              = -1;
    m_dragDirtyPoint         = false;
    m_dragDirtyGlobalGain    = false;
    m_dragDirtySelectedIndex = false;

    // Use m_points.at(dragIdx) directly rather than a cached pointer.
    // m_dragIndex is the post-sort canonical slot from enforceOrdering's
    // bandId re-resolution; pointer-into-vector caching would dangle if
    // m_points were ever resized (e.g. Task 5 loadFromJson).
    if (wasDraggingPoint && pointDirty) {
        raisePointsChanged(false);
        if (dragIdx >= 0 && dragIdx < m_points.size()) {
            raisePointDataChanged(dragIdx, m_points.at(dragIdx), false);
        }
    }

    if (wasDraggingGlobal && globalDirty) {
        raiseGlobalGainChanged(false);
    }

    if (selectedDirty) {
        raiseSelectedIndexChanged(false);
    }

    update();
}

// From Thetis ucParametricEq.cs:1846-1995 [v2.10.3.13].
// The wheel handler has four behaviors, in order:
//   A. No selection -> wheel over the global handle adjusts global gain
//      by (steps * 0.5) dB.
//   B. Selected + Ctrl -> shift selected band frequency by
//      chooseFrequencyStep(span)/5 (min 1 Hz) per step.
//   C. Selected + (Shift OR !parametric) -> adjust selected band gain by
//      (steps * 0.5) dB.
//   D. Selected + (no modifier) + parametric -> multiply Q by 1.12^steps.
// Every branch raises pointsChanged(false-or-dragging) plus
// pointDataChanged when (f,g,q) actually moved.  Constants 1.12, 0.5,
// 120.0, 1.0 are verbatim from C#.
void ParametricEqWidget::wheelEvent(QWheelEvent* event) {
    bool dragging = isDraggingNow();

    QRect plot = computePlotRect();
    QPoint pt  = event->position().toPoint();

    double steps = double(event->angleDelta().y()) / 120.0;
    if (steps == 0.0) return;

    if (m_selectedIndex < 0 || m_selectedIndex >= m_points.size()) {
        if (hitTestGlobalGainHandle(plot, pt)) {
            setGlobalGainDb(clamp(m_globalGainDb + (steps * 0.5), m_dbMin, m_dbMax));
        }
        return;
    }

    if (!plot.contains(pt)) return;

    EqPoint& p = m_points[m_selectedIndex];

    double oldF = p.frequencyHz;
    double oldG = p.gainDb;
    double oldQ = p.q;

    bool ctrl  = event->modifiers().testFlag(Qt::ControlModifier);
    bool shift = event->modifiers().testFlag(Qt::ShiftModifier);

    if (ctrl) {
        if (isFrequencyLockedIndex(m_selectedIndex)) return;

        double stepHz = chooseFrequencyStep(m_frequencyMaxHz - m_frequencyMinHz) / 5.0;
        if (stepHz < 1.0) stepHz = 1.0;

        double freq = p.frequencyHz + (steps * stepHz);
        freq = clamp(freq, m_frequencyMinHz, m_frequencyMaxHz);

        if (!m_allowPointReorder) {
            double minF;
            double maxF;

            if (m_selectedIndex == 0) {
                minF = m_frequencyMinHz;
                maxF = m_points[1].frequencyHz - m_minPointSpacingHz;
            } else if (m_selectedIndex == m_points.size() - 1) {
                minF = m_points[m_points.size() - 2].frequencyHz + m_minPointSpacingHz;
                maxF = m_frequencyMaxHz;
            } else {
                minF = m_points[m_selectedIndex - 1].frequencyHz + m_minPointSpacingHz;
                maxF = m_points[m_selectedIndex + 1].frequencyHz - m_minPointSpacingHz;
            }

            if (maxF < minF) maxF = minF;
            freq = clamp(freq, minF, maxF);
        }

        if (qAbs(freq - p.frequencyHz) > 0.000001) {
            p.frequencyHz = freq;

            if (m_allowPointReorder) {
                enforceOrdering(false);

                int idx = m_selectedIndex;
                double minF = m_frequencyMinHz;
                double maxF = m_frequencyMaxHz;

                if (m_points.size() > 1) {
                    if (idx > 0) minF = m_points[idx - 1].frequencyHz + m_minPointSpacingHz;
                    if (idx < m_points.size() - 1) maxF = m_points[idx + 1].frequencyHz - m_minPointSpacingHz;
                    if (maxF < minF) maxF = minF;
                }

                if (idx >= 0 && idx < m_points.size()) {
                    double clampedFreq = clamp(m_points[idx].frequencyHz, minF, maxF);
                    if (qAbs(clampedFreq - m_points[idx].frequencyHz) > 0.000001) {
                        m_points[idx].frequencyHz = clampedFreq;
                    }
                }

                enforceOrdering(false);
            }

            // Re-resolve current point via m_selectedIndex (enforceOrdering
            // pinned selection by bandId, so this is the post-sort slot).
            int curIdx = m_selectedIndex;
            if (curIdx >= 0 && curIdx < m_points.size()) {
                const EqPoint& curP = m_points.at(curIdx);
                raisePointsChanged(false);
                if (qAbs(curP.frequencyHz - oldF) > 0.000001
                    || qAbs(curP.gainDb - oldG) > 0.000001
                    || qAbs(curP.q - oldQ) > 0.000001) {
                    raisePointDataChanged(curIdx, curP, dragging);
                }
            } else {
                raisePointsChanged(false);
            }
            update();
        }

        return;
    }

    if (!m_parametricEq) {
        double gain = clamp(p.gainDb + (steps * 0.5), m_dbMin, m_dbMax);
        if (qAbs(gain - p.gainDb) > 0.000001) {
            p.gainDb = gain;
            raisePointsChanged(false);
            if (qAbs(p.frequencyHz - oldF) > 0.000001
                || qAbs(p.gainDb - oldG) > 0.000001
                || qAbs(p.q - oldQ) > 0.000001) {
                raisePointDataChanged(m_selectedIndex, p, dragging);
            }
            update();
        }
        return;
    }

    if (shift) {
        double gain = clamp(p.gainDb + (steps * 0.5), m_dbMin, m_dbMax);
        if (qAbs(gain - p.gainDb) > 0.000001) {
            p.gainDb = gain;
            raisePointsChanged(false);
            if (qAbs(p.frequencyHz - oldF) > 0.000001
                || qAbs(p.gainDb - oldG) > 0.000001
                || qAbs(p.q - oldQ) > 0.000001) {
                raisePointDataChanged(m_selectedIndex, p, dragging);
            }
            update();
        }
        return;
    }

    double factor = std::pow(1.12, steps);
    double qv = clamp(p.q * factor, m_qMin, m_qMax);

    if (qAbs(qv - p.q) > 0.000001) {
        p.q = qv;
        raisePointsChanged(dragging);
        if (qAbs(p.frequencyHz - oldF) > 0.000001
            || qAbs(p.gainDb - oldG) > 0.000001
            || qAbs(p.q - oldQ) > 0.000001) {
            raisePointDataChanged(m_selectedIndex, p, dragging);
        }
        update();
    }
}

// =================================================================
// Section: public Q-style property setters (Batch 5)
//
// Each setter mirrors a [Category] property at ucParametricEq.cs:449-1005
// [v2.10.3.13].  The early-return + clamp + side-effect ordering is verbatim
// from the C# setter.  Public getters are inline in the header.
// =================================================================

// From Thetis ucParametricEq.cs:577-603 [v2.10.3.13] -- BandCount.
void ParametricEqWidget::setBandCount(int count) {
    int v = count;
    if (v < 2) v = 2;
    if (v > 256) v = 256;
    if (v == m_bandCount) return;

    m_bandCount = v;

    bool hadSelection = (m_selectedIndex != -1);
    resetPointsDefault();

    if (hadSelection) {
        m_selectedIndex = -1;
        raiseSelectedIndexChanged(false);
    }

    raisePointsChanged(false);
    update();
}

// From Thetis ucParametricEq.cs:605-624 [v2.10.3.13] -- FrequencyMinHz.
void ParametricEqWidget::setFrequencyMinHz(double hz) {
    double newMin = hz;
    if (std::isnan(newMin) || std::isinf(newMin)) return;
    if (newMin >= m_frequencyMaxHz) return;

    double oldMin = m_frequencyMinHz;
    double oldMax = m_frequencyMaxHz;

    m_frequencyMinHz = newMin;
    rescaleFrequencies(oldMin, oldMax, m_frequencyMinHz, m_frequencyMaxHz);
    enforceOrdering(true);
    raisePointsChanged(false);
    update();
}

// From Thetis ucParametricEq.cs:626-645 [v2.10.3.13] -- FrequencyMaxHz.
void ParametricEqWidget::setFrequencyMaxHz(double hz) {
    double newMax = hz;
    if (std::isnan(newMax) || std::isinf(newMax)) return;
    if (newMax <= m_frequencyMinHz) return;

    double oldMin = m_frequencyMinHz;
    double oldMax = m_frequencyMaxHz;

    m_frequencyMaxHz = newMax;
    rescaleFrequencies(oldMin, oldMax, m_frequencyMinHz, m_frequencyMaxHz);
    enforceOrdering(true);
    raisePointsChanged(false);
    update();
}

// From Thetis ucParametricEq.cs:647-660 [v2.10.3.13] -- LogScale.
void ParametricEqWidget::setLogScale(bool on) {
    if (on == m_logScale) return;
    m_logScale = on;
    update();
}

// From Thetis ucParametricEq.cs:662-681 [v2.10.3.13] -- DbMin.
void ParametricEqWidget::setDbMin(double db) {
    double v = db;
    if (std::isnan(v) || std::isinf(v)) return;
    if (v >= m_dbMax) return;
    m_dbMin = v;
    clampAllGains();
    if (!m_barChartData.isEmpty()) {
        syncBarChartPeaksToData();
    }
    raisePointsChanged(false);
    raiseGlobalGainChanged(false);
    update();
}

// From Thetis ucParametricEq.cs:683-702 [v2.10.3.13] -- DbMax.
void ParametricEqWidget::setDbMax(double db) {
    double v = db;
    if (std::isnan(v) || std::isinf(v)) return;
    if (v <= m_dbMin) return;
    m_dbMax = v;
    clampAllGains();
    if (!m_barChartData.isEmpty()) {
        syncBarChartPeaksToData();
    }
    raisePointsChanged(false);
    raiseGlobalGainChanged(false);
    update();
}

// From Thetis ucParametricEq.cs:722-735 [v2.10.3.13] -- GlobalGainIsHorizLine.
void ParametricEqWidget::setGlobalGainIsHorizLine(bool on) {
    if (on == m_globalGainIsHorizLine) return;
    m_globalGainIsHorizLine = on;
    update();
}

// From Thetis ucParametricEq.cs:737-746 [v2.10.3.13] -- ShowReadout.
// Note: C# unconditionally writes the field then Invalidate() (no early-
// return on equality). Match that behavior verbatim.
void ParametricEqWidget::setShowReadout(bool on) {
    m_showReadout = on;
    update();
}

// From Thetis ucParametricEq.cs:748-761 [v2.10.3.13] -- ShowDotReadings.
void ParametricEqWidget::setShowDotReadings(bool on) {
    if (on == m_showDotReadings) return;
    m_showDotReadings = on;
    update();
}

// From Thetis ucParametricEq.cs:763-776 [v2.10.3.13] -- ShowDotReadingsAsComp.
void ParametricEqWidget::setShowDotReadingsAsComp(bool on) {
    if (on == m_showDotReadingsAsComp) return;
    m_showDotReadingsAsComp = on;
    update();
}

// From Thetis ucParametricEq.cs:778-792 [v2.10.3.13] -- MinPointSpacingHz.
void ParametricEqWidget::setMinPointSpacingHz(double hz) {
    double v = hz;
    if (std::isnan(v) || std::isinf(v)) return;
    if (v < 0.0) v = 0.0;
    m_minPointSpacingHz = v;
    enforceOrdering(true);
    raisePointsChanged(false);
    update();
}

// From Thetis ucParametricEq.cs:794-807 [v2.10.3.13] -- AllowPointReorder.
void ParametricEqWidget::setAllowPointReorder(bool on) {
    if (on == m_allowPointReorder) return;
    m_allowPointReorder = on;
    enforceOrdering(true);
    raisePointsChanged(false);
    update();
}

// From Thetis ucParametricEq.cs:809-820 [v2.10.3.13] -- ParametricEQ.
void ParametricEqWidget::setParametricEq(bool on) {
    if (on == m_parametricEq) return;
    m_parametricEq = on;
    update();
}

// From Thetis ucParametricEq.cs:822-837 [v2.10.3.13] -- QMin.
void ParametricEqWidget::setQMin(double q) {
    double v = q;
    if (std::isnan(v) || std::isinf(v)) return;
    if (v <= 0.0) return;
    m_qMin = v;
    if (m_qMax < m_qMin) m_qMax = m_qMin;
    clampAllQ();
    raisePointsChanged(false);
    update();
}

// From Thetis ucParametricEq.cs:839-854 [v2.10.3.13] -- QMax.
void ParametricEqWidget::setQMax(double q) {
    double v = q;
    if (std::isnan(v) || std::isinf(v)) return;
    if (v <= 0.0) return;
    m_qMax = v;
    if (m_qMin > m_qMax) m_qMin = m_qMax;
    clampAllQ();
    raisePointsChanged(false);
    update();
}

// From Thetis ucParametricEq.cs:856-865 [v2.10.3.13] -- ShowBandShading.
// Note: C# unconditionally writes + Invalidate (no equality early-return).
void ParametricEqWidget::setShowBandShading(bool on) {
    m_showBandShading = on;
    update();
}

// From Thetis ucParametricEq.cs:867-876 [v2.10.3.13] -- UsePerBandColours.
// Note: C# unconditionally writes + Invalidate (no equality early-return).
void ParametricEqWidget::setUsePerBandColours(bool on) {
    m_usePerBandColours = on;
    update();
}

// From Thetis ucParametricEq.cs:878-887 [v2.10.3.13] -- BandShadeColor.
void ParametricEqWidget::setBandShadeColor(const QColor& c) {
    m_bandShadeColor = c;
    update();
}

// From Thetis ucParametricEq.cs:889-901 [v2.10.3.13] -- BandShadeAlpha.
void ParametricEqWidget::setBandShadeAlpha(int alpha) {
    int v = alpha;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    m_bandShadeAlpha = v;
    update();
}

// From Thetis ucParametricEq.cs:903-915 [v2.10.3.13] -- BandShadeWeightCutoff.
void ParametricEqWidget::setBandShadeWeightCutoff(double cutoff) {
    double v = cutoff;
    if (std::isnan(v) || std::isinf(v)) return;
    if (v < 0.0) v = 0.0;
    m_bandShadeWeightCutoff = v;
    update();
}

// From Thetis ucParametricEq.cs:917-926 [v2.10.3.13] -- ShowAxisScales.
// Note: C# unconditionally writes + Invalidate.
void ParametricEqWidget::setShowAxisScales(bool on) {
    m_showAxisScales = on;
    update();
}

// From Thetis ucParametricEq.cs:928-940 [v2.10.3.13] -- AxisTickLength.
void ParametricEqWidget::setAxisTickLength(int len) {
    int v = len;
    if (v < 2) v = 2;
    if (v > 20) v = 20;
    m_axisTickLength = v;
    update();
}

// From Thetis ucParametricEq.cs:942-951 [v2.10.3.13] -- AxisTextColor.
void ParametricEqWidget::setAxisTextColor(const QColor& c) {
    m_axisTextColor = c;
    update();
}

// From Thetis ucParametricEq.cs:953-962 [v2.10.3.13] -- AxisTickColor.
void ParametricEqWidget::setAxisTickColor(const QColor& c) {
    m_axisTickColor = c;
    update();
}

// From Thetis ucParametricEq.cs:559-575 [v2.10.3.13] -- YAxisStepDb.
void ParametricEqWidget::setYAxisStepDb(double step) {
    double v = step;
    if (std::isnan(v) || std::isinf(v)) return;
    if (v < 0.0) v = 0.0;
    if (v > 0.0 && v < 0.1) v = 0.1;
    if (qAbs(v - m_yAxisStepDb) < 0.000001) return;

    m_yAxisStepDb = v;
    update();
}

// From Thetis ucParametricEq.cs:449-470 [v2.10.3.13] -- BarChartPeakHoldEnabled.
void ParametricEqWidget::setBarChartPeakHoldEnabled(bool on) {
    if (on == m_barChartPeakHoldEnabled) return;

    m_barChartPeakHoldEnabled        = on;
    m_barChartPeakLastUpdateMs       = QDateTime::currentMSecsSinceEpoch();

    if (!m_barChartPeakHoldEnabled && !m_barChartData.isEmpty()) {
        syncBarChartPeaksToData();
    }

    updateBarChartPeakTimerState();
    update();
}

// From Thetis ucParametricEq.cs:472-485 [v2.10.3.13] -- BarChartPeakHoldMs.
void ParametricEqWidget::setBarChartPeakHoldMs(int ms) {
    int v = ms;
    if (v < 0) v = 0;
    if (v == m_barChartPeakHoldMs) return;
    m_barChartPeakHoldMs = v;
}

// From Thetis ucParametricEq.cs:487-501 [v2.10.3.13] -- BarChartPeakDecayDbPerSecond.
void ParametricEqWidget::setBarChartPeakDecayDbPerSecond(double dbPerSec) {
    double v = dbPerSec;
    if (std::isnan(v) || std::isinf(v)) return;
    if (v < 0.0) v = 0.0;
    if (qAbs(v - m_barChartPeakDecayDbPerSecond) < 0.000001) return;
    m_barChartPeakDecayDbPerSecond = v;
}

// From Thetis ucParametricEq.cs:503-512 [v2.10.3.13] -- BarChartFillColor.
void ParametricEqWidget::setBarChartFillColor(const QColor& c) {
    m_barChartFillColor = c;
    update();
}

// From Thetis ucParametricEq.cs:514-529 [v2.10.3.13] -- BarChartFillAlpha.
void ParametricEqWidget::setBarChartFillAlpha(int alpha) {
    int v = alpha;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    if (v == m_barChartFillAlpha) return;
    m_barChartFillAlpha = v;
    update();
}

// From Thetis ucParametricEq.cs:531-540 [v2.10.3.13] -- BarChartPeakColor.
void ParametricEqWidget::setBarChartPeakColor(const QColor& c) {
    m_barChartPeakColor = c;
    update();
}

// From Thetis ucParametricEq.cs:542-557 [v2.10.3.13] -- BarChartPeakAlpha.
void ParametricEqWidget::setBarChartPeakAlpha(int alpha) {
    int v = alpha;
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    if (v == m_barChartPeakAlpha) return;
    m_barChartPeakAlpha = v;
    update();
}

// =================================================================
// Section: point-edit public API (Batch 5)
//
// Mirrors ucParametricEq.cs:1134-1351 [v2.10.3.13].
// =================================================================

// From Thetis ucParametricEq.cs:1134-1140 [v2.10.3.13] -- SetPointHz.
bool ParametricEqWidget::setPointHz(int bandId, double frequencyHz, bool isDragging) {
    int idx = indexFromBandId(bandId);
    if (idx < 0) return false;

    return setPointHzInternal(idx, frequencyHz, isDragging);
}

// From Thetis ucParametricEq.cs:1162-1244 [v2.10.3.13] -- setPointHzInternal.
// C# keys on EqPoint reference identity; we resolve by index up-front
// because EqPoint is a value type in C++ and re-resolving by bandId after
// each enforceOrdering() call (which can reorder) is the safe equivalent.
bool ParametricEqWidget::setPointHzInternal(int index, double frequencyHz, bool isDragging) {
    if (index < 0 || index >= m_points.size()) return false;

    EqPoint& p = m_points[index];

    double oldF = p.frequencyHz;
    double oldG = p.gainDb;
    double oldQ = p.q;

    double freq;

    if (isFrequencyLockedIndex(index)) {
        freq = getLockedFrequencyForIndex(index);
    } else {
        freq = clamp(frequencyHz, m_frequencyMinHz, m_frequencyMaxHz);

        if (!m_allowPointReorder) {
            double minF;
            double maxF;

            if (index == 0) {
                minF = m_frequencyMinHz;
                maxF = m_points[1].frequencyHz - m_minPointSpacingHz;
            } else if (index == m_points.size() - 1) {
                minF = m_points[m_points.size() - 2].frequencyHz + m_minPointSpacingHz;
                maxF = m_frequencyMaxHz;
            } else {
                minF = m_points[index - 1].frequencyHz + m_minPointSpacingHz;
                maxF = m_points[index + 1].frequencyHz - m_minPointSpacingHz;
            }

            if (maxF < minF) maxF = minF;
            freq = clamp(freq, minF, maxF);
        }
    }

    if (qAbs(p.frequencyHz - freq) <= 0.000001) return true;

    // Capture bandId BEFORE writing -- enforceOrdering may reshuffle, so we
    // re-resolve the slot below and then re-bind p to the new location.
    int writeBandId = p.bandId;
    p.frequencyHz = freq;

    if (m_allowPointReorder && !isFrequencyLockedIndex(index)) {
        enforceOrdering(false);

        index = indexFromBandId(writeBandId);
        if (index < 0) return false;
        EqPoint& pAfter = m_points[index];

        double minF = m_frequencyMinHz;
        double maxF = m_frequencyMaxHz;

        if (m_points.size() > 1) {
            if (index > 0) minF = m_points[index - 1].frequencyHz + m_minPointSpacingHz;
            if (index < m_points.size() - 1) maxF = m_points[index + 1].frequencyHz - m_minPointSpacingHz;
            if (maxF < minF) maxF = minF;
        }

        double clampedFreq = clamp(pAfter.frequencyHz, minF, maxF);
        if (qAbs(clampedFreq - pAfter.frequencyHz) > 0.000001) pAfter.frequencyHz = clampedFreq;

        enforceOrdering(false);
    }

    raisePointsChanged(isDragging);

    // Re-resolve once more after the second enforceOrdering above; the band
    // may have shifted slot during the in-band clamp pass.
    int finalIdx = indexFromBandId(writeBandId);
    if (finalIdx >= 0 && finalIdx < m_points.size()) {
        const EqPoint& pFinal = m_points.at(finalIdx);
        if (qAbs(pFinal.frequencyHz - oldF) > 0.000001
            || qAbs(pFinal.gainDb - oldG) > 0.000001
            || qAbs(pFinal.q - oldQ) > 0.000001) {
            raisePointDataChanged(finalIdx, pFinal, isDragging);
        }
    }

    update();
    return true;
}

// From Thetis ucParametricEq.cs:1246-1258 [v2.10.3.13] -- GetPointData.
// Decimal precision: freq=3, gain=1, q=2 (Math.Round in C#).
void ParametricEqWidget::getPointData(int index, double& frequencyHz, double& gainDb, double& q) const {
    frequencyHz = 0.0;
    gainDb      = 0.0;
    q           = 0.0;

    if (index < 0 || index >= m_points.size()) return;

    const EqPoint& p = m_points.at(index);
    frequencyHz = qRound(p.frequencyHz * 1000.0) / 1000.0;
    gainDb      = qRound(p.gainDb * 10.0) / 10.0;
    q           = m_parametricEq ? (qRound(p.q * 100.0) / 100.0) : 0.0;
}

// From Thetis ucParametricEq.cs:1260-1288 [v2.10.3.13] -- SetPointData.
bool ParametricEqWidget::setPointData(int index, double frequencyHz, double gainDb, double q) {
    if (index < 0 || index >= m_points.size()) return false;

    EqPoint& p = m_points[index];

    double oldF = p.frequencyHz;
    double oldG = p.gainDb;
    double oldQ = p.q;
    int    bandId = p.bandId;

    if (isFrequencyLockedIndex(index)) {
        p.frequencyHz = getLockedFrequencyForIndex(index);
    } else {
        p.frequencyHz = clamp(frequencyHz, m_frequencyMinHz, m_frequencyMaxHz);
    }

    p.gainDb = clamp(gainDb, m_dbMin, m_dbMax);
    p.q      = clamp(q,      m_qMin, m_qMax);

    enforceOrdering(true);

    // After enforceOrdering, the slot may have shifted; re-resolve for
    // accurate change detection + signal payload.
    int newIdx = indexFromBandId(bandId);
    if (newIdx < 0) return true;

    const EqPoint& pAfter = m_points.at(newIdx);
    if (qAbs(pAfter.frequencyHz - oldF) > 0.000001
        || qAbs(pAfter.gainDb - oldG) > 0.000001
        || qAbs(pAfter.q - oldQ) > 0.000001) {
        raisePointsChanged(false);
        raisePointDataChanged(newIdx, pAfter, false);
        update();
    }

    return true;
}

// From Thetis ucParametricEq.cs:1290-1309 [v2.10.3.13] -- GetPointsData.
// 3 parallel arrays, NOT a single struct array, to match the Thetis signature
// (downstream MicProfileManager / TransmitModel call pattern).
void ParametricEqWidget::getPointsData(QVector<double>& frequencyHz,
                                       QVector<double>& gainDb,
                                       QVector<double>& q) const {
    int n = m_points.size();

    QVector<double> f(n);
    QVector<double> g(n);
    QVector<double> qq(n);

    for (int i = 0; i < n; ++i) {
        const EqPoint& p = m_points.at(i);
        f[i]  = p.frequencyHz;
        g[i]  = p.gainDb;
        qq[i] = p.q;
    }

    frequencyHz = f;
    gainDb      = g;
    q           = qq;
}

// From Thetis ucParametricEq.cs:1311-1351 [v2.10.3.13] -- SetPointsData.
// Bulk path: writes all 3 arrays atomically, fires ONE pointsChanged(false)
// at the end if anything moved (does NOT fire per-point pointDataChanged --
// callers wanting per-point notifications should use setPointData in a loop).
bool ParametricEqWidget::setPointsData(const QVector<double>& frequencyHz,
                                       const QVector<double>& gainDb,
                                       const QVector<double>& q) {
    if (frequencyHz.size() != m_points.size()) return false;
    if (gainDb.size()      != m_points.size()) return false;
    if (q.size()           != m_points.size()) return false;

    bool anyChanged = false;

    for (int i = 0; i < m_points.size(); ++i) {
        EqPoint& p = m_points[i];

        double oldF = p.frequencyHz;
        double oldG = p.gainDb;
        double oldQ = p.q;

        if (isFrequencyLockedIndex(i)) {
            p.frequencyHz = getLockedFrequencyForIndex(i);
        } else {
            p.frequencyHz = clamp(frequencyHz.at(i), m_frequencyMinHz, m_frequencyMaxHz);
        }

        p.gainDb = clamp(gainDb.at(i), m_dbMin, m_dbMax);
        p.q      = clamp(q.at(i),      m_qMin, m_qMax);

        if (qAbs(p.frequencyHz - oldF) > 0.000001
            || qAbs(p.gainDb - oldG) > 0.000001
            || qAbs(p.q - oldQ) > 0.000001) {
            anyChanged = true;
        }
    }

    enforceOrdering(true);

    if (anyChanged) {
        raisePointsChanged(false);
        update();
    }

    return true;
}

// =================================================================
// Section: JSON marshal (Batch 5)
//
// Mirrors Thetis Newtonsoft snake_case schema at ucParametricEq.cs:220-252,
// 1460-1573 [v2.10.3.13].  EqJsonPoint deliberately carries ONLY freq/gain/q
// -- bandId and bandColor are widget-local concerns and must NOT be
// serialized (round-trip compatibility with the Thetis-generated JSON
// blobs that ride in MicProfileManager and TransmitModel).
// Decimal precision verbatim: freq=3, gain=1, q=2.
// =================================================================

// From Thetis ucParametricEq.cs:1460-1486 [v2.10.3.13] -- SaveToJson.
QString ParametricEqWidget::saveToJson() const {
    QJsonObject root;
    root.insert(QStringLiteral("band_count"),       m_points.size());
    root.insert(QStringLiteral("parametric_eq"),    m_parametricEq);
    root.insert(QStringLiteral("global_gain_db"),   qRound(m_globalGainDb * 10.0) / 10.0);
    root.insert(QStringLiteral("frequency_min_hz"), qRound(m_frequencyMinHz * 1000.0) / 1000.0);
    root.insert(QStringLiteral("frequency_max_hz"), qRound(m_frequencyMaxHz * 1000.0) / 1000.0);

    QJsonArray pts;
    for (const EqPoint& p : m_points) {
        QJsonObject jp;
        jp.insert(QStringLiteral("frequency_hz"), qRound(p.frequencyHz * 1000.0) / 1000.0);
        jp.insert(QStringLiteral("gain_db"),      qRound(p.gainDb * 10.0) / 10.0);
        jp.insert(QStringLiteral("q"),            qRound(p.q * 100.0) / 100.0);
        pts.append(jp);
    }
    root.insert(QStringLiteral("points"), pts);

    return QString::fromUtf8(QJsonDocument(root).toJson(QJsonDocument::Indented));
}

// From Thetis ucParametricEq.cs:1488-1573 [v2.10.3.13] -- LoadFromJson.
// Drag-safety: Thetis source does NOT bail on an active drag; it reuses the
// existing enforceOrdering(true) path which normalizes ordering even mid-
// drag.  We match that behavior verbatim -- callers wanting drag-safe loads
// must gate at the call site.
bool ParametricEqWidget::loadFromJson(const QString& json) {
    if (json.trimmed().isEmpty()) return false;

    QJsonParseError perr;
    QJsonDocument   doc = QJsonDocument::fromJson(json.toUtf8(), &perr);
    if (perr.error != QJsonParseError::NoError) return false;
    if (!doc.isObject()) return false;

    QJsonObject root = doc.object();
    if (!root.contains(QStringLiteral("points"))) return false;
    QJsonValue ptsVal = root.value(QStringLiteral("points"));
    if (!ptsVal.isArray()) return false;
    QJsonArray pts = ptsVal.toArray();
    if (pts.size() < 2) return false;

    int    bandCount = root.value(QStringLiteral("band_count")).toInt(0);
    if (bandCount < 2) bandCount = pts.size();

    if (bandCount < 2)         return false;
    if (bandCount > 256)       return false;
    if (bandCount != pts.size()) return false;

    double newFreqMin = root.value(QStringLiteral("frequency_min_hz")).toDouble(
                         std::numeric_limits<double>::quiet_NaN());
    double newFreqMax = root.value(QStringLiteral("frequency_max_hz")).toDouble(
                         std::numeric_limits<double>::quiet_NaN());
    if (std::isnan(newFreqMin) || std::isinf(newFreqMin)) return false;
    if (std::isnan(newFreqMax) || std::isinf(newFreqMax)) return false;
    if (newFreqMax <= newFreqMin) return false;

    bool anyChanged = false;

    if (bandCount != m_points.size()) {
        anyChanged = true;
        m_bandCount = bandCount;
        resetPointsDefault();
    }

    bool   oldParam = m_parametricEq;
    double oldGlobal = m_globalGainDb;
    double oldFreqMin = m_frequencyMinHz;
    double oldFreqMax = m_frequencyMaxHz;

    m_parametricEq    = root.value(QStringLiteral("parametric_eq")).toBool(false);
    m_globalGainDb    = clamp(root.value(QStringLiteral("global_gain_db")).toDouble(0.0),
                              m_dbMin, m_dbMax);
    m_frequencyMinHz  = newFreqMin;
    m_frequencyMaxHz  = newFreqMax;

    if (oldParam != m_parametricEq)                         anyChanged = true;
    if (qAbs(oldGlobal - m_globalGainDb) > 0.000001)        anyChanged = true;
    if (qAbs(oldFreqMin - m_frequencyMinHz) > 0.000001)     anyChanged = true;
    if (qAbs(oldFreqMax - m_frequencyMaxHz) > 0.000001)     anyChanged = true;

    for (int i = 0; i < m_points.size(); ++i) {
        EqPoint& p = m_points[i];
        if (i >= pts.size()) break;
        if (!pts.at(i).isObject()) continue;
        QJsonObject jp = pts.at(i).toObject();

        double oldF = p.frequencyHz;
        double oldG = p.gainDb;
        double oldQ = p.q;

        double jpFreq = jp.value(QStringLiteral("frequency_hz")).toDouble(p.frequencyHz);
        double jpGain = jp.value(QStringLiteral("gain_db")).toDouble(p.gainDb);
        double jpQ    = jp.value(QStringLiteral("q")).toDouble(p.q);

        if (isFrequencyLockedIndex(i)) {
            p.frequencyHz = getLockedFrequencyForIndex(i);
        } else {
            p.frequencyHz = clamp(jpFreq, m_frequencyMinHz, m_frequencyMaxHz);
        }
        p.gainDb = clamp(jpGain, m_dbMin, m_dbMax);
        p.q      = clamp(jpQ,    m_qMin, m_qMax);

        if (qAbs(p.frequencyHz - oldF) > 0.000001
            || qAbs(p.gainDb - oldG) > 0.000001
            || qAbs(p.q - oldQ) > 0.000001) {
            anyChanged = true;
        }
    }

    enforceOrdering(true);

    if (anyChanged) {
        raisePointsChanged(false);
        raiseGlobalGainChanged(false);
        update();
    }

    return true;
}

} // namespace NereusSDR
