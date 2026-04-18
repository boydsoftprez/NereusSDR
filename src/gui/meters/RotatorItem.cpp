// =================================================================
// src/gui/meters/RotatorItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  MeterManager.cs

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

#include "RotatorItem.h"

// From Thetis clsRotatorItem (MeterManager.cs:15042+)
// renderRotator() (MeterManager.cs:35170-35569)

#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <QtMath>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// setBackgroundImagePath()
// Store path and load image from disk.
// ---------------------------------------------------------------------------
void RotatorItem::setBackgroundImagePath(const QString& path)
{
    m_bgImagePath = path;
    if (!path.isEmpty()) {
        m_bgImage.load(path);
    } else {
        m_bgImage = QImage{};
    }
}

// ---------------------------------------------------------------------------
// setValue()
// Azimuth smoothing — From Thetis clsRotatorItem::Update() (MeterManager.cs:15290-15312)
// Takes the shortest angular path (handles 359→1 wrap correctly).
// ---------------------------------------------------------------------------
void RotatorItem::setValue(double v)
{
    MeterItem::setValue(v);

    // From Thetis MeterManager.cs:15290-15312
    float normalizedValue = std::fmod(m_smoothedAz, 360.0f);
    if (normalizedValue < 0.0f) { normalizedValue += 360.0f; }

    float normalizedReading = std::fmod(static_cast<float>(v), 360.0f);
    if (normalizedReading < 0.0f) { normalizedReading += 360.0f; }

    float difference = normalizedReading - normalizedValue;
    if (difference > 180.0f) {
        difference -= 360.0f;
    } else if (difference < -180.0f) {
        difference += 360.0f;
    }

    float adjustmentSpeed = 0.2f * std::abs(difference);

    if (std::abs(difference) < adjustmentSpeed) {
        m_smoothedAz = static_cast<float>(v);
    } else {
        m_smoothedAz += (difference > 0.0f ? 1.0f : -1.0f) * adjustmentSpeed;
    }

    m_smoothedAz = std::fmod(m_smoothedAz, 360.0f);
    if (m_smoothedAz < 0.0f) { m_smoothedAz += 360.0f; }
}

// ---------------------------------------------------------------------------
// participatesIn()
// RotatorItem renders to OverlayStatic (compass face, ticks, labels)
// and OverlayDynamic (heading arrow, beam width arc, readout text).
// ---------------------------------------------------------------------------
bool RotatorItem::participatesIn(Layer layer) const
{
    return layer == Layer::OverlayStatic || layer == Layer::OverlayDynamic;
}

// ---------------------------------------------------------------------------
// squareRect()
// Compute a square centered within the item's pixel rect.
// Same pattern as DialItem / MagicEyeItem.
// ---------------------------------------------------------------------------
QRect RotatorItem::squareRect(int widgetW, int widgetH) const
{
    const QRect pr = pixelRect(widgetW, widgetH);
    const int side = std::min(pr.width(), pr.height());
    return QRect(pr.left() + (pr.width()  - side) / 2,
                 pr.top()  + (pr.height() - side) / 2,
                 side, side);
}

// ---------------------------------------------------------------------------
// paintCompassFace()
// OverlayStatic — compass ring, tick dots, cardinals / degree labels.
// From Thetis renderRotator() (MeterManager.cs:35191-35303) — Primary (AZ) face.
// ---------------------------------------------------------------------------
void RotatorItem::paintCompassFace(QPainter& p, const QRect& compassRect)
{
    p.setRenderHint(QPainter::Antialiasing, true);

    // Step 1: fill background circle
    p.setBrush(m_backgroundColour);
    p.setPen(Qt::NoPen);
    p.drawEllipse(compassRect);

    // Step 2: draw background image if loaded
    if (!m_bgImage.isNull()) {
        p.save();
        QPainterPath clipPath;
        clipPath.addEllipse(compassRect);
        p.setClipPath(clipPath);
        p.drawImage(compassRect, m_bgImage);
        p.restore();
    }

    const float cx = static_cast<float>(compassRect.center().x());
    const float cy = static_cast<float>(compassRect.center().y());
    const float h  = static_cast<float>(compassRect.height());

    // From Thetis renderRotator() MeterManager.cs:35200-35203
    const float radius      = (h * 0.8f) / 2.0f;
    const float radius_text = (h * 0.92f) / 2.0f;
    const float dotBig      = h * 0.015f;   // big dot radius
    const float dotSmall    = h * 0.005f;   // small dot radius

    // Step 3: draw tick dots + labels
    // From Thetis renderRotator() MeterManager.cs:35215-35303
    if (m_showCardinals) {
        // Cardinal mode: small dots every 10° (non-45°), big dots every 45° + text
        for (int deg = 0; deg <= 350; deg += 10) {
            const float rad = qDegreesToRadians(static_cast<float>(deg) - 90.0f);
            const float px  = cx + radius * std::cos(rad);
            const float py  = cy + radius * std::sin(rad);

            if (deg % 45 != 0) {
                // From Thetis MeterManager.cs:35226-35229
                p.setBrush(m_smallBlobColour);
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPointF(px, py), dotSmall, dotSmall);
            }
        }

        // Big dots and cardinal text at 45° intervals
        // From Thetis MeterManager.cs:35232-35277
        for (int deg = 0; deg <= 315; deg += 45) {
            const float rad = qDegreesToRadians(static_cast<float>(deg) - 90.0f);
            const float px  = cx + radius * std::cos(rad);
            const float py  = cy + radius * std::sin(rad);

            p.setBrush(m_bigBlobColour);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(px, py), dotBig, dotBig);

            QString card;
            switch (deg) {
                case 0:   card = QStringLiteral("N");  break;
                case 45:  card = QStringLiteral("NE"); break;
                case 90:  card = QStringLiteral("E");  break;
                case 135: card = QStringLiteral("SE"); break;
                case 180: card = QStringLiteral("S");  break;
                case 225: card = QStringLiteral("SW"); break;
                case 270: card = QStringLiteral("W");  break;
                case 315: card = QStringLiteral("NW"); break;
                default:  break;
            }

            const float tx = cx + radius_text * std::cos(rad);
            const float ty = cy + radius_text * std::sin(rad);

            QFont font;
            font.setPixelSize(std::max(7, static_cast<int>(h * 0.06f)));
            font.setBold(deg == 0);
            p.setFont(font);
            // From Thetis MeterManager.cs:35275 — "N" drawn in red (bigBlobColour = red)
            p.setPen(deg == 0 ? m_bigBlobColour : m_outerTextColour);
            p.setBrush(Qt::NoBrush);

            const int fw = std::max(12, static_cast<int>(h * 0.12f));
            p.drawText(QRectF(tx - fw / 2.0f, ty - fw / 2.0f, fw, fw),
                       Qt::AlignCenter, card);
        }
    } else {
        // Non-cardinal mode: big dots + degree labels at 30°, small dots at 10°
        // From Thetis renderRotator() MeterManager.cs:35282-35303
        for (int deg = 0; deg <= 350; deg += 10) {
            const float rad = qDegreesToRadians(static_cast<float>(deg) - 90.0f);
            const float px  = cx + radius * std::cos(rad);
            const float py  = cy + radius * std::sin(rad);

            if (deg % 30 == 0) {
                p.setBrush(m_bigBlobColour);
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPointF(px, py), dotBig, dotBig);

                const float tx = cx + radius_text * std::cos(rad);
                const float ty = cy + radius_text * std::sin(rad);

                QFont font;
                font.setPixelSize(std::max(7, static_cast<int>(h * 0.06f)));
                p.setFont(font);
                p.setPen(m_outerTextColour);
                p.setBrush(Qt::NoBrush);

                const int fw = std::max(14, static_cast<int>(h * 0.14f));
                p.drawText(QRectF(tx - fw / 2.0f, ty - fw / 2.0f, fw, fw),
                           Qt::AlignCenter, QString::number(deg));
            } else {
                p.setBrush(m_smallBlobColour);
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPointF(px, py), dotSmall, dotSmall);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// paintElevationArc()
// OverlayStatic — 0-90° elevation arc dots and labels.
// From Thetis renderRotator() (MeterManager.cs:35476-35534) — Secondary (ELE) face.
// ---------------------------------------------------------------------------
void RotatorItem::paintElevationArc(QPainter& p, const QRect& eleRect)
{
    p.setRenderHint(QPainter::Antialiasing, true);

    const float h = static_cast<float>(eleRect.height());
    const float w = static_cast<float>(eleRect.width());

    float cx, cy, radius, radius_text;

    if (m_mode == RotatorMode::Ele) {
        // From Thetis MeterManager.cs:35496-35504 — ELE-only layout
        radius      = h * 0.84f;
        radius_text = h * 0.90f;
        cx = static_cast<float>(eleRect.left()) + (h / 2.0f) - (radius / 2.0f);
        cy = static_cast<float>(eleRect.bottom()) - (4.0f * (w * 0.0125f));
    } else {
        // BOTH mode — right half-circle
        // From Thetis MeterManager.cs:35506-35510
        const float xShift = 2.0f * (w * 0.0125f);
        radius      = (h * 0.8f) / 2.0f;
        radius_text = (h * 0.92f) / 2.0f;
        cx = static_cast<float>(eleRect.right()) - xShift - h / 2.0f;
        cy = static_cast<float>(eleRect.top()) + h / 2.0f;
    }

    const float dotBig   = h * 0.015f;
    const float dotSmall = h * 0.005f;

    // From Thetis MeterManager.cs:35512-35533 — elevation ticks 0-90°
    for (int deg = 0; deg <= 90; deg += 5) {
        const float rad = qDegreesToRadians(static_cast<float>(deg) - 90.0f);
        const float px  = cx + radius * std::cos(rad);
        const float py  = cy + radius * std::sin(rad);

        if (deg % 15 == 0) {
            p.setBrush(m_bigBlobColour);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(px, py), dotBig, dotBig);

            const float tx = cx + radius_text * std::cos(rad);
            const float ty = cy + radius_text * std::sin(rad);

            QFont font;
            font.setPixelSize(std::max(7, static_cast<int>(h * 0.06f)));
            p.setFont(font);
            p.setPen(m_outerTextColour);
            p.setBrush(Qt::NoBrush);

            // From Thetis MeterManager.cs:35526: plotText((90 - deg).ToString(), ...)
            const int fw = std::max(14, static_cast<int>(h * 0.14f));
            p.drawText(QRectF(tx - fw / 2.0f, ty - fw / 2.0f, fw, fw),
                       Qt::AlignCenter, QString::number(90 - deg));
        } else {
            p.setBrush(m_smallBlobColour);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(px, py), dotSmall, dotSmall);
        }
    }
}

// ---------------------------------------------------------------------------
// paintHeading()
// OverlayDynamic — beam-width arc, heading arrow, value readout.
// From Thetis renderRotator() (MeterManager.cs:35306-35382, 35535-35569)
// ---------------------------------------------------------------------------
void RotatorItem::paintHeading(QPainter& p, const QRect& compassRect)
{
    p.setRenderHint(QPainter::Antialiasing, true);

    const float h = static_cast<float>(compassRect.height());
    const float w = static_cast<float>(compassRect.width());

    // ---- AZ heading (Primary face) ----
    // From Thetis MeterManager.cs:35191-35382
    if (m_mode != RotatorMode::Ele) {
        // From Thetis MeterManager.cs:35187 — xShift for BOTH mode
        const float xShift = (m_mode == RotatorMode::Both) ? 2.0f * (w * 0.0125f) : 0.0f;
        const float cx = xShift + static_cast<float>(compassRect.left()) + h / 2.0f;
        const float cy = static_cast<float>(compassRect.top()) + h / 2.0f;

        // From Thetis MeterManager.cs:35200-35203
        const float radius_inner_arrow = (h * 0.75f) / 2.0f;
        const float radius_tip_arrow   = (h * 0.78f) / 2.0f;

        const float degrees_az = std::fmod(std::abs(m_smoothedAz), 360.0f);

        // Step 1: beam width arc (pie slice)
        // From Thetis MeterManager.cs:35306-35339
        if (m_showBeamWidth) {
            const float half_bw   = m_beamWidth / 2.0f;
            const float rad1      = qDegreesToRadians(degrees_az - 90.0f - half_bw);
            const float rad2      = qDegreesToRadians(degrees_az - 90.0f + half_bw);
            const float ax1       = cx + radius_tip_arrow * std::cos(rad1);
            const float ay1       = cy + radius_tip_arrow * std::sin(rad1);
            const float ax2       = cx + radius_tip_arrow * std::cos(rad2);
            const float ay2       = cy + radius_tip_arrow * std::sin(rad2);

            Q_UNUSED(ax2); Q_UNUSED(ay2);

            // Qt arcTo: startAngle from 3-o'clock, CCW positive
            // Thetis uses CW from top (deg-90 in math radians)
            // Convert: Qt angle = -(deg-90) for our orientation
            const float startAngle = -(degrees_az - 90.0f - half_bw);
            const float spanAngle  = -m_beamWidth;

            QPainterPath beamPath;
            beamPath.moveTo(cx, cy);
            beamPath.lineTo(ax1, ay1);
            beamPath.arcTo(cx - radius_tip_arrow,
                           cy - radius_tip_arrow,
                           radius_tip_arrow * 2.0f,
                           radius_tip_arrow * 2.0f,
                           startAngle, spanAngle);
            beamPath.closeSubpath();

            QColor bwColor = m_beamWidthColour;
            bwColor.setAlphaF(static_cast<double>(m_beamWidthAlpha));
            p.setBrush(bwColor);
            p.setPen(Qt::NoPen);
            p.drawPath(beamPath);
        }

        // Step 2: arrow shaft — center to tip
        // From Thetis MeterManager.cs:35342-35346
        const float arrowRad = qDegreesToRadians(degrees_az - 90.0f);
        const float tipX     = cx + radius_tip_arrow   * std::cos(arrowRad);
        const float tipY     = cy + radius_tip_arrow   * std::sin(arrowRad);

        QPen arrowPen(m_arrowColour, std::max(1.0f, h * 0.01f));
        p.setPen(arrowPen);
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(cx, cy), QPointF(tipX, tipY));

        // Step 3: arrow winglets (arrowhead sides)
        // From Thetis MeterManager.cs:35348-35359
        {
            const float radL = qDegreesToRadians(degrees_az - 90.0f - 3.0f);
            const float sxL  = cx + radius_inner_arrow * std::cos(radL);
            const float syL  = cy + radius_inner_arrow * std::sin(radL);
            p.drawLine(QPointF(tipX, tipY), QPointF(sxL, syL));
        }
        {
            const float radR = qDegreesToRadians(degrees_az - 90.0f + 3.0f);
            const float sxR  = cx + radius_inner_arrow * std::cos(radR);
            const float syR  = cy + radius_inner_arrow * std::sin(radR);
            p.drawLine(QPointF(tipX, tipY), QPointF(sxR, syR));
        }

        // Step 4: AZ text readout
        // From Thetis MeterManager.cs:35362-35382
        if (m_showValue) {
            const int fontSize = std::max(7, static_cast<int>(h * 0.07f));
            QFont font;
            font.setPixelSize(fontSize);
            font.setBold(true);
            p.setFont(font);
            p.setPen(m_outerTextColour);
            p.setBrush(Qt::NoBrush);

            const int labelSize  = std::max(7, static_cast<int>(fontSize * 0.65f));
            const int fwLarge    = std::max(20, static_cast<int>(w * 0.22f));
            const int fhLarge    = std::max(10, static_cast<int>(h * 0.09f));

            if (m_mode == RotatorMode::Both) {
                // From Thetis MeterManager.cs:35364-35371
                const float tx  = static_cast<float>(compassRect.left()) + w * 0.75f;
                const float ty1 = static_cast<float>(compassRect.top())  + h * 0.575f;
                const float ty2 = static_cast<float>(compassRect.top())  + h * 0.7f;
                p.drawText(QRectF(tx, ty1, fwLarge, fhLarge),
                           Qt::AlignLeft | Qt::AlignVCenter,
                           QStringLiteral("%1°").arg(degrees_az, 0, 'f', 1));
                QFont labelFont;
                labelFont.setPixelSize(labelSize);
                p.setFont(labelFont);
                p.drawText(QRectF(tx, ty2, fwLarge, fhLarge),
                           Qt::AlignLeft | Qt::AlignVCenter,
                           QStringLiteral("azimuth"));
            } else {
                // From Thetis MeterManager.cs:35375-35381
                const float tx  = static_cast<float>(compassRect.left()) + w * 0.5f;
                const float ty1 = static_cast<float>(compassRect.top())  + h * 0.525f;
                const float ty2 = static_cast<float>(compassRect.top())  + h * 0.65f;
                p.drawText(QRectF(tx - fwLarge / 2.0f, ty1, fwLarge, fhLarge),
                           Qt::AlignCenter,
                           QStringLiteral("%1°").arg(degrees_az, 0, 'f', 1));
                QFont labelFont;
                labelFont.setPixelSize(labelSize);
                p.setFont(labelFont);
                p.drawText(QRectF(tx - fwLarge / 2.0f, ty2, fwLarge, fhLarge),
                           Qt::AlignCenter,
                           QStringLiteral("azimuth"));
            }
        }
    }

    // ---- ELE heading (Secondary face) ----
    // From Thetis MeterManager.cs:35476-35569
    if (m_mode != RotatorMode::Az) {
        float cx_ele, cy_ele, radius_ele, radius_inner_ele, radius_tip_ele;

        if (m_mode == RotatorMode::Ele) {
            // From Thetis MeterManager.cs:35496-35504
            radius_ele       = h * 0.84f;
            radius_inner_ele = h * 0.75f;
            radius_tip_ele   = h * 0.78f;
            cx_ele = static_cast<float>(compassRect.left()) + (h / 2.0f) - (radius_ele / 2.0f);
            cy_ele = static_cast<float>(compassRect.bottom()) - (4.0f * (w * 0.0125f));
        } else {
            // BOTH mode — right half-circle
            // From Thetis MeterManager.cs:35506-35510
            const float xShift   = 2.0f * (w * 0.0125f);
            radius_ele       = (h * 0.8f) / 2.0f;
            radius_inner_ele = (h * 0.75f) / 2.0f;
            radius_tip_ele   = (h * 0.78f) / 2.0f;
            cx_ele = static_cast<float>(compassRect.right()) - xShift - h / 2.0f;
            cy_ele = static_cast<float>(compassRect.top()) + h / 2.0f;
        }

        const float degrees_ele = std::fmod(std::abs(m_smoothedEle), 90.0f);

        // Arrow — elevation: 0°=right, 90°=top (negated angle)
        // From Thetis MeterManager.cs:35536-35554
        const float arrowRad = qDegreesToRadians(-degrees_ele);
        const float tipX     = cx_ele + radius_tip_ele   * std::cos(arrowRad);
        const float tipY     = cy_ele + radius_tip_ele   * std::sin(arrowRad);

        QPen arrowPen(m_arrowColour, std::max(1.0f, h * 0.01f));
        p.setPen(arrowPen);
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(cx_ele, cy_ele), QPointF(tipX, tipY));

        {
            const float radL = qDegreesToRadians(-degrees_ele - 3.0f);
            const float sxL  = cx_ele + radius_inner_ele * std::cos(radL);
            const float syL  = cy_ele + radius_inner_ele * std::sin(radL);
            p.drawLine(QPointF(tipX, tipY), QPointF(sxL, syL));
        }
        {
            const float radR = qDegreesToRadians(-degrees_ele + 3.0f);
            const float sxR  = cx_ele + radius_inner_ele * std::cos(radR);
            const float syR  = cy_ele + radius_inner_ele * std::sin(radR);
            p.drawLine(QPointF(tipX, tipY), QPointF(sxR, syR));
        }

        // ELE text readout
        // From Thetis MeterManager.cs:35558-35569
        if (m_showValue) {
            const int fontSize  = std::max(7, static_cast<int>(h * 0.07f));
            const int labelSize = std::max(7, static_cast<int>(fontSize * 0.65f));
            const int fw        = std::max(20, static_cast<int>(w * 0.22f));
            const int fh        = std::max(10, static_cast<int>(h * 0.09f));

            QFont font;
            font.setPixelSize(fontSize);
            font.setBold(true);
            p.setFont(font);
            p.setPen(m_outerTextColour);
            p.setBrush(Qt::NoBrush);

            if (m_mode == RotatorMode::Ele) {
                // From Thetis MeterManager.cs:35560-35564
                const float tx  = static_cast<float>(compassRect.left()) + w * 0.4f;
                const float ty1 = static_cast<float>(compassRect.top())  + h * 0.75f;
                const float ty2 = ty1 + fh;
                p.drawText(QRectF(tx, ty1, fw, fh),
                           Qt::AlignCenter,
                           QStringLiteral("%1°").arg(degrees_ele, 0, 'f', 1));
                QFont labelFont;
                labelFont.setPixelSize(labelSize);
                p.setFont(labelFont);
                p.drawText(QRectF(tx, ty2, fw, fh),
                           Qt::AlignCenter,
                           QStringLiteral("elevation"));
            } else {
                // BOTH mode — From Thetis MeterManager.cs:35567-35569
                const float tx  = static_cast<float>(compassRect.left()) + w * 0.75f;
                const float ty1 = static_cast<float>(compassRect.top())  + h * 0.825f;
                const float ty2 = ty1 + fh;
                p.drawText(QRectF(tx, ty1, fw, fh),
                           Qt::AlignLeft | Qt::AlignVCenter,
                           QStringLiteral("%1°").arg(degrees_ele, 0, 'f', 1));
                QFont labelFont;
                labelFont.setPixelSize(labelSize);
                p.setFont(labelFont);
                p.drawText(QRectF(tx, ty2, fw, fh),
                           Qt::AlignLeft | Qt::AlignVCenter,
                           QStringLiteral("elevation"));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// paintForLayer()
// Dispatch to static or dynamic paint based on pipeline layer.
// ---------------------------------------------------------------------------
void RotatorItem::paintForLayer(QPainter& p, int widgetW, int widgetH, Layer layer)
{
    const QRect cr = squareRect(widgetW, widgetH);
    if (layer == Layer::OverlayStatic) {
        if (m_mode != RotatorMode::Ele) {
            paintCompassFace(p, cr);
        }
        if (m_mode != RotatorMode::Az) {
            paintElevationArc(p, cr);
        }
    } else if (layer == Layer::OverlayDynamic) {
        paintHeading(p, cr);
    }
}

// ---------------------------------------------------------------------------
// paint()
// CPU fallback: render both layers in sequence.
// ---------------------------------------------------------------------------
void RotatorItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect cr = squareRect(widgetW, widgetH);
    if (m_mode != RotatorMode::Ele) {
        paintCompassFace(p, cr);
    }
    if (m_mode != RotatorMode::Az) {
        paintElevationArc(p, cr);
    }
    paintHeading(p, cr);
}

// ---------------------------------------------------------------------------
// serialize()
// Format: ROTATOR|x|y|w|h|bindingId|zOrder|mode|showValue|showCardinals|
//         showBeamWidth|beamWidth|beamWidthAlpha|darkMode|padding|
//         bigBlobColour|smallBlobColour|outerTextColour|arrowColour|
//         beamWidthColour|backgroundColour|bgImagePath
// ---------------------------------------------------------------------------
QString RotatorItem::serialize() const
{
    return QStringLiteral(
        "ROTATOR|%1|%2|%3|%4|%5|%6|%7|%8|%9|%10|%11|%12|%13|%14|%15|%16|%17|%18|%19|%20|%21")
        .arg(static_cast<double>(m_x))
        .arg(static_cast<double>(m_y))
        .arg(static_cast<double>(m_w))
        .arg(static_cast<double>(m_h))
        .arg(m_bindingId)
        .arg(m_zOrder)
        .arg(static_cast<int>(m_mode))
        .arg(m_showValue    ? 1 : 0)
        .arg(m_showCardinals? 1 : 0)
        .arg(m_showBeamWidth? 1 : 0)
        .arg(static_cast<double>(m_beamWidth))
        .arg(static_cast<double>(m_beamWidthAlpha))
        .arg(m_darkMode     ? 1 : 0)
        .arg(static_cast<double>(m_padding))
        .arg(m_bigBlobColour.name(QColor::HexArgb))
        .arg(m_smallBlobColour.name(QColor::HexArgb))
        .arg(m_outerTextColour.name(QColor::HexArgb))
        .arg(m_arrowColour.name(QColor::HexArgb))
        .arg(m_beamWidthColour.name(QColor::HexArgb))
        .arg(m_backgroundColour.name(QColor::HexArgb))
        .arg(m_bgImagePath);
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected parts (22 total):
// [0]=ROTATOR [1-6]=base fields [7-21]=type-specific fields
// ---------------------------------------------------------------------------
bool RotatorItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 22 || parts[0] != QLatin1String("ROTATOR")) {
        return false;
    }

    // Parse base fields (indices 1..6) via MeterItem::deserialize
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    if (!MeterItem::deserialize(base)) {
        return false;
    }

    bool ok = true;

    const int modeInt = parts[7].toInt(&ok);    if (!ok) { return false; }
    const int showVal = parts[8].toInt(&ok);    if (!ok) { return false; }
    const int showCard= parts[9].toInt(&ok);    if (!ok) { return false; }
    const int showBw  = parts[10].toInt(&ok);   if (!ok) { return false; }
    const float bw    = parts[11].toFloat(&ok); if (!ok) { return false; }
    const float bwAlp = parts[12].toFloat(&ok); if (!ok) { return false; }
    const int dark    = parts[13].toInt(&ok);   if (!ok) { return false; }
    const float pad   = parts[14].toFloat(&ok); if (!ok) { return false; }

    const QColor bigBlob    (parts[15]);
    const QColor smallBlob  (parts[16]);
    const QColor outerText  (parts[17]);
    const QColor arrow      (parts[18]);
    const QColor beamWid    (parts[19]);
    const QColor background (parts[20]);

    if (!bigBlob.isValid()    || !smallBlob.isValid() ||
        !outerText.isValid()  || !arrow.isValid()     ||
        !beamWid.isValid()    || !background.isValid()) {
        return false;
    }

    if (modeInt < 0 || modeInt > 2) { return false; }

    m_mode             = static_cast<RotatorMode>(modeInt);
    m_showValue        = (showVal  != 0);
    m_showCardinals    = (showCard != 0);
    m_showBeamWidth    = (showBw   != 0);
    m_beamWidth        = bw;
    m_beamWidthAlpha   = bwAlp;
    m_darkMode         = (dark     != 0);
    m_padding          = pad;
    m_bigBlobColour    = bigBlob;
    m_smallBlobColour  = smallBlob;
    m_outerTextColour  = outerText;
    m_arrowColour      = arrow;
    m_beamWidthColour  = beamWid;
    m_backgroundColour = background;

    // bgImagePath — trailing optional field (index 21)
    const QString imgPath = (parts.size() > 21) ? parts[21] : QString{};
    setBackgroundImagePath(imgPath);

    return true;
}

} // namespace NereusSDR
