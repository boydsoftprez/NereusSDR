// =================================================================
// src/gui/meters/DialItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

#include "DialItem.h"

// From Thetis clsDialDisplay (MeterManager.cs:15399+)
// renderDialDisplay() (MeterManager.cs:33750-33899)

#include <QPainter>
#include <QPainterPath>
#include <QStringList>

#include <algorithm>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// participatesIn()
// DialItem renders to OverlayStatic (main circle, dashed ring, quadrant labels)
// and OverlayDynamic (active quadrant highlight, speed indicator dot).
// ---------------------------------------------------------------------------
bool DialItem::participatesIn(Layer layer) const
{
    return layer == Layer::OverlayStatic || layer == Layer::OverlayDynamic;
}

// ---------------------------------------------------------------------------
// squareRect()
// Compute a square centered within the item's pixel rect.
// Same pattern as MagicEyeItem.
// ---------------------------------------------------------------------------
QRect DialItem::squareRect(int widgetW, int widgetH) const
{
    const QRect pr = pixelRect(widgetW, widgetH);
    const int side = std::min(pr.width(), pr.height());
    return QRect(pr.left() + (pr.width()  - side) / 2,
                 pr.top()  + (pr.height() - side) / 2,
                 side, side);
}

// ---------------------------------------------------------------------------
// paintStatic()
// From Thetis renderDialDisplay() (MeterManager.cs:33750-33899)
// Draws: main circle fill, dashed ring, quadrant dividers, quadrant labels.
// ---------------------------------------------------------------------------
void DialItem::paintStatic(QPainter& p, const QRect& dialRect)
{
    p.setRenderHint(QPainter::Antialiasing, true);

    // Step 1: fill main circle
    p.setBrush(m_circleColour);
    p.setPen(Qt::NoPen);
    p.drawEllipse(dialRect);

    // Step 2: dashed ring inset by 4px
    // From Thetis clsDialDisplay (MeterManager.cs:15399+)
    QPen ringPen(m_ringColour, 2, Qt::DashLine);
    p.setPen(ringPen);
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(dialRect.adjusted(4, 4, -4, -4));

    // Step 3: cross lines dividing circle into four quadrants
    // Thin lines through center in ring colour
    const QPoint center = dialRect.center();
    QPen linePen(m_ringColour, 1);
    p.setPen(linePen);
    p.drawLine(center.x(), dialRect.top(), center.x(), dialRect.bottom());
    p.drawLine(dialRect.left(), center.y(), dialRect.right(), center.y());

    // Step 4: quadrant labels
    // From Thetis clsDialDisplay (MeterManager.cs:15814-15836)
    // Quadrants: 0=top-left(VFOA), 1=top-right(VFOB),
    //            2=bottom-left(ACCEL), 3=bottom-right(LOCK)
    QFont labelFont;
    labelFont.setPixelSize(9);
    p.setFont(labelFont);
    p.setPen(m_textColour);
    p.setBrush(Qt::NoBrush);

    const int hw = dialRect.width()  / 2;
    const int hh = dialRect.height() / 2;
    const int l  = dialRect.left();
    const int t  = dialRect.top();

    // Top-left quadrant — "VFOA"
    const QRect tlRect(l, t, hw, hh);
    p.drawText(tlRect, Qt::AlignCenter, QStringLiteral("VFOA"));

    // Top-right quadrant — "VFOB"
    const QRect trRect(l + hw, t, hw, hh);
    p.drawText(trRect, Qt::AlignCenter, QStringLiteral("VFOB"));

    // Bottom-left quadrant — "ACCEL"
    const QRect blRect(l, t + hh, hw, hh);
    p.drawText(blRect, Qt::AlignCenter, QStringLiteral("ACCEL"));

    // Bottom-right quadrant — "LOCK"
    const QRect brRect(l + hw, t + hh, hw, hh);
    p.drawText(brRect, Qt::AlignCenter, QStringLiteral("LOCK"));
}

// ---------------------------------------------------------------------------
// paintDynamic()
// From Thetis renderDialDisplay() (MeterManager.cs:33750-33899)
// Draws: active quadrant highlight, accel/lock tints, speed indicator dot.
// ---------------------------------------------------------------------------
void DialItem::paintDynamic(QPainter& p, const QRect& dialRect)
{
    p.setRenderHint(QPainter::Antialiasing, true);

    const QPoint center = dialRect.center();
    const int hw = dialRect.width()  / 2;
    const int hh = dialRect.height() / 2;
    const int l  = dialRect.left();
    const int t  = dialRect.top();

    // Helper lambda: fill a quadrant-rect clipped to the circle with given color+alpha
    auto fillQuadrant = [&](const QRect& qRect, const QColor& baseColor, int alpha) {
        QPainterPath circlePath;
        circlePath.addEllipse(dialRect);

        QPainterPath quadPath;
        quadPath.addRect(qRect);

        const QPainterPath clipped = circlePath.intersected(quadPath);

        QColor fillColor = baseColor;
        fillColor.setAlpha(alpha);
        p.setBrush(fillColor);
        p.setPen(Qt::NoPen);
        p.drawPath(clipped);
    };

    // Quadrant rect lookup
    // 0=top-left(VFOA), 1=top-right(VFOB), 2=bottom-left(ACCEL), 3=bottom-right(LOCK)
    const QRect quadRects[4] = {
        QRect(l,      t,      hw, hh),   // 0: VFOA top-left
        QRect(l + hw, t,      hw, hh),   // 1: VFOB top-right
        QRect(l,      t + hh, hw, hh),   // 2: ACCEL bottom-left
        QRect(l + hw, t + hh, hw, hh),   // 3: LOCK bottom-right
    };

    // Step 1: highlight active quadrant at alpha 0.3 (≈76/255)
    // From Thetis renderDialDisplay() (MeterManager.cs:33750-33899)
    if (m_activeQuadrant >= 0 && m_activeQuadrant <= 3) {
        fillQuadrant(quadRects[m_activeQuadrant], m_btnOnColour, 76);
    }

    // Step 2: accel tint (bottom-left) at alpha 0.2 (≈51/255)
    if (m_accelEnabled) {
        fillQuadrant(quadRects[2], m_btnOnColour, 51);
    }

    // Step 3: lock tint (bottom-right) at alpha 0.2 (≈51/255)
    if (m_lockEnabled) {
        fillQuadrant(quadRects[3], m_btnOnColour, 51);
    }

    // Step 4: speed indicator dot at center
    // From Thetis renderDialDisplay() (MeterManager.cs:33750-33899)
    QColor speedColor;
    switch (m_speedState) {
        case SpeedState::Slow: speedColor = m_slowColour; break;
        case SpeedState::Hold: speedColor = m_holdColour; break;
        case SpeedState::Fast: speedColor = m_fastColour; break;
    }

    const int dotR = dialRect.width() / 10;
    p.setBrush(speedColor);
    p.setPen(Qt::NoPen);
    p.drawEllipse(center, dotR, dotR);
}

// ---------------------------------------------------------------------------
// paintForLayer()
// Dispatch to static or dynamic paint based on pipeline layer.
// ---------------------------------------------------------------------------
void DialItem::paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer)
{
    const QRect dr = squareRect(widgetW, widgetH);
    if (layer == Layer::OverlayStatic) {
        paintStatic(p, dr);
    } else if (layer == Layer::OverlayDynamic) {
        paintDynamic(p, dr);
    }
}

// ---------------------------------------------------------------------------
// paint()
// CPU fallback: render both layers in sequence.
// ---------------------------------------------------------------------------
void DialItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect dr = squareRect(widgetW, widgetH);
    paintStatic(p, dr);
    paintDynamic(p, dr);
}

// ---------------------------------------------------------------------------
// serialize()
// Format: DIAL|x|y|w|h|bindingId|zOrder|textColour|circleColour|padColour|
//         ringColour|btnOnColour|btnOffColour|btnHighlight|slowColour|holdColour|fastColour
// ---------------------------------------------------------------------------
QString DialItem::serialize() const
{
    return QStringLiteral("DIAL|%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11|%12|%13|%14|%15|%16")
        .arg(static_cast<double>(m_x))
        .arg(static_cast<double>(m_y))
        .arg(static_cast<double>(m_w))
        .arg(static_cast<double>(m_h))
        .arg(m_bindingId)
        .arg(m_zOrder)
        .arg(m_textColour.name(QColor::HexArgb))
        .arg(m_circleColour.name(QColor::HexArgb))
        .arg(m_padColour.name(QColor::HexArgb))
        .arg(m_ringColour.name(QColor::HexArgb))
        .arg(m_btnOnColour.name(QColor::HexArgb))
        .arg(m_btnOffColour.name(QColor::HexArgb))
        .arg(m_btnHighlight.name(QColor::HexArgb))
        .arg(m_slowColour.name(QColor::HexArgb))
        .arg(m_holdColour.name(QColor::HexArgb))
        .arg(m_fastColour.name(QColor::HexArgb));
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected parts (17 total):
// [0]=DIAL [1-6]=base fields [7-16]=colour fields
// ---------------------------------------------------------------------------
bool DialItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 17 || parts[0] != QLatin1String("DIAL")) {
        return false;
    }

    // Parse base fields (indices 1..6)
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    if (!MeterItem::deserialize(base)) {
        return false;
    }

    // Parse colour fields
    const QColor textColour(parts[7]);
    const QColor circleColour(parts[8]);
    const QColor padColour(parts[9]);
    const QColor ringColour(parts[10]);
    const QColor btnOnColour(parts[11]);
    const QColor btnOffColour(parts[12]);
    const QColor btnHighlight(parts[13]);
    const QColor slowColour(parts[14]);
    const QColor holdColour(parts[15]);
    const QColor fastColour(parts[16]);

    if (!textColour.isValid()   || !circleColour.isValid() ||
        !padColour.isValid()    || !ringColour.isValid()   ||
        !btnOnColour.isValid()  || !btnOffColour.isValid() ||
        !btnHighlight.isValid() || !slowColour.isValid()   ||
        !holdColour.isValid()   || !fastColour.isValid()) {
        return false;
    }

    m_textColour   = textColour;
    m_circleColour = circleColour;
    m_padColour    = padColour;
    m_ringColour   = ringColour;
    m_btnOnColour  = btnOnColour;
    m_btnOffColour = btnOffColour;
    m_btnHighlight = btnHighlight;
    m_slowColour   = slowColour;
    m_holdColour   = holdColour;
    m_fastColour   = fastColour;

    return true;
}

} // namespace NereusSDR
