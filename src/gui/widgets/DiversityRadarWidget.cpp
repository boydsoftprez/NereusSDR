// =================================================================
// src/gui/widgets/DiversityRadarWidget.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/DiversityForm.cs [v2.10.3.15],
//   original licence from Thetis source is included below.
//
// Scope of port: see DiversityRadarWidget.h.  CalcVrms is the only
// piece of CalcVrms-equivalent math; the rest of the file is
// NereusSDR-original Qt6 paint structured around the picRadar_Paint
// background/axes shell from Thetis.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-27 — Reimplemented in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.  Phase 3F Sub-Epic G
//                 Task 5.
//   2026-09-28 - The lobe math and its sampling moved to
//                 core/DiversityPattern, which the Core also sends to the
//                 phone; the radar draws from it. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

//=================================================================
// DiversityForm.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
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

#include "gui/widgets/DiversityRadarWidget.h"

#include <QFont>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QRadialGradient>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

namespace {

constexpr double kTwoPi        = 2.0 * M_PI;
constexpr int    kLobeSamples  = DiversityPattern::kSamples;  // 360 / 3 degrees

} // namespace

DiversityRadarWidget::DiversityRadarWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(220, 220);
    setMouseTracking(false);
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void DiversityRadarWidget::setPhase(double radians)
{
    if (qFuzzyCompare(m_phase, radians)) { return; }
    m_phase = radians;
    update();
}

void DiversityRadarWidget::setGain(double ratio)
{
    if (qFuzzyCompare(m_gain, ratio)) { return; }
    m_gain = ratio;
    update();
}

void DiversityRadarWidget::setCrossFire(bool on)
{
    if (m_crossFire == on) { return; }
    m_crossFire = on;
    update();
}

void DiversityRadarWidget::setVfoFreqMhz(double mhz)
{
    if (qFuzzyCompare(m_vfoMhz, mhz)) { return; }
    m_vfoMhz = mhz;
    update();
}

void DiversityRadarWidget::setAntennaSpacingMeters(double m)
{
    if (qFuzzyCompare(m_antSpacingM, m)) { return; }
    m_antSpacingM = m;
    update();
}

// From Thetis DiversityForm.cs:2398-2440 [v2.10.3.15] (CalcVrms), now in
// DiversityPattern so the Core sends the pattern this radar draws.
DiversityPattern::Inputs DiversityRadarWidget::patternInputs() const
{
    DiversityPattern::Inputs in;
    in.vfoMhz = m_vfoMhz;
    in.phaseRad = m_phase;
    in.gainLinear = m_gain;
    in.crossFire = m_crossFire;
    in.spacingMeters = m_antSpacingM;
    return in;
}

void DiversityRadarWidget::paintEvent(QPaintEvent* /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int side  = std::min(width(), height());
    const QPointF c(width() * 0.5, height() * 0.5);
    const double  r = (side - 4) * 0.5;

    // Backdrop radial gradient (NereusSDR-original chrome).
    QRadialGradient bg(c, r);
    bg.setColorAt(0.0, QColor(0, 60, 100));
    bg.setColorAt(1.0, QColor(0, 20, 40));
    p.setBrush(bg);
    p.setPen(QPen(QColor(0, 255, 255), 1.0));
    p.drawEllipse(c, r, r);

    // Range rings at 30 percent and 50 percent (dashed).
    QPen ringPen(QColor(0, 200, 220, 140), 0.8, Qt::DashLine);
    p.setPen(ringPen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(c, r * 0.3, r * 0.3);
    p.drawEllipse(c, r * 0.5, r * 0.5);

    // X and Y axis cross.
    // From Thetis DiversityForm.cs:1511-1512 [v2.10.3.15] (picRadar_Paint).
    QPen axisPen(QColor(0, 255, 255, 160), 0.8);
    p.setPen(axisPen);
    p.drawLine(QPointF(c.x() - r, c.y()), QPointF(c.x() + r, c.y()));
    p.drawLine(QPointF(c.x(), c.y() - r), QPointF(c.x(), c.y() + r));

    // Compass labels (NereusSDR-original chrome).
    QFont labelFont = p.font();
    labelFont.setPointSizeF(labelFont.pointSizeF() * 0.85);
    p.setFont(labelFont);
    p.setPen(QColor(200, 220, 230));
    const QString labels[4] = {QStringLiteral("N"), QStringLiteral("E"),
                               QStringLiteral("S"), QStringLiteral("W")};
    const QPointF offsets[4] = {
        QPointF(c.x() - 5,  c.y() - r + 12),
        QPointF(c.x() + r - 12, c.y() + 4),
        QPointF(c.x() - 5,  c.y() + r - 4),
        QPointF(c.x() - r + 4, c.y() + 4),
    };
    for (int i = 0; i < 4; ++i) {
        p.drawText(offsets[i], labels[i]);
    }

    // The sensitivity lobe: DiversityPattern's samples (bearing 0 = N,
    // clockwise, each divided by the peak), drawn at 0.85 * r so a peak
    // never blows out the radar window. The Core sends these samples
    // (SliceModel::diversityPattern), so the phone draws the same lobe.
    const QList<double> samples = DiversityPattern::normalizedSamples(patternInputs());
    QPolygonF lobe;
    lobe.reserve(kLobeSamples + 1);
    const double scale = 0.85 * r;
    for (int i = 0; i <= kLobeSamples; ++i) {
        const double theta = (kTwoPi * i) / static_cast<double>(kLobeSamples);
        const double rho   = samples.at(i % kLobeSamples) * scale;
        // Map azimuth 0 = N (up).  Convert to screen-space.
        const double sx = c.x() + rho * std::sin(theta);
        const double sy = c.y() - rho * std::cos(theta);
        lobe << QPointF(sx, sy);
    }

    QPen lobePen(QColor(0, 220, 255), 1.6);
    p.setPen(lobePen);
    p.setBrush(QColor(0, 220, 255, 60));
    p.drawPolygon(lobe);

    // Centre dot (white) + steering vector to the on-lobe focal point.
    p.setPen(QPen(QColor(255, 255, 255), 1.2));
    p.setBrush(Qt::white);
    p.drawEllipse(c, 4.0, 4.0);

    // The "control handle" sits at the steering angle on a unit ring.
    // From Thetis DiversityForm.cs:1610-1632 [v2.10.3.15]
    // (getControlHandlePoint).  We use 0.85 * r so it lines up with
    // the lobe scale.
    const double handleR = 0.85 * r;
    const double hx = c.x() + handleR * std::sin(m_phase);
    const double hy = c.y() - handleR * std::cos(m_phase);
    p.setPen(QPen(QColor(255, 255, 255, 200), 1.0, Qt::DashLine));
    p.drawLine(c, QPointF(hx, hy));
    p.setBrush(QColor(255, 220, 80));
    p.setPen(QPen(QColor(255, 255, 255), 1.0));
    p.drawEllipse(QPointF(hx, hy), 5.0, 5.0);
}

void DiversityRadarWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_dragging = true;
    mouseMoveEvent(event);
}

void DiversityRadarWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_dragging) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const QPointF c(width() * 0.5, height() * 0.5);
    const QPointF p = event->position();
    const double  dx = p.x() - c.x();
    const double  dy = -(p.y() - c.y());  // up is positive
    // Compute azimuth (0 = N).  atan2(dx, dy) gives angle measured
    // clockwise from N.
    const double theta = std::atan2(dx, dy);
    // Normalise 0..2 PI
    const double norm = (theta < 0.0) ? (theta + kTwoPi) : theta;
    emit phaseAdjusted(norm);
}

void DiversityRadarWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    m_dragging = false;
}

} // namespace NereusSDR
