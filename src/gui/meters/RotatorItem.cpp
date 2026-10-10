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
//   2026-10-08 - Rotor control plan Task 5: drag to turn and the stop
//                 circle, ported from Thetis renderRotator() and
//                 clsRotatorItem.SendRotatorMessage/MouseUp/MouseDown, sent
//                 through RotorCommandSink instead of an MMIO template; the
//                 target marker in amber; elevation fed and drawn; "Both"
//                 laid out two faces wide as in Thetis. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
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

#include "core/LogCategories.h"
#include "gui/StyleConstants.h"
#include "models/RotorCommandSink.h"
#include "models/RotorModel.h"

#include <QLineF>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStringList>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace NereusSDR {

namespace {

// From Thetis MeterManager.cs:23490 [v2.10.3.15]: the rotator items read
// their reading every 100 ms (ri.UpdateInterval = 100).
constexpr int kFeedIntervalMs = 100;
// From Thetis MeterManager.cs:36919 [v2.10.3.15]: the arrow is at the
// destination within 3 degrees of it.
constexpr float kArrivedDeg = 3.0f;
// NereusSDR: the feed stops re-applying once the drawn heading is this
// close to the rotor's (Thetis re-reads forever).
constexpr float kSettledDeg = 0.01f;

// From Thetis MeterManager.cs:16610-16631 [v2.10.3.15]
// (clsRotatorItem::Update()): move toward the reading by the shortest way round.
float smoothToward(float current, float reading)
{
    float normalizedValue = std::fmod(current, 360.0f);
    if (normalizedValue < 0.0f) { normalizedValue += 360.0f; }

    float normalizedReading = std::fmod(reading, 360.0f);
    if (normalizedReading < 0.0f) { normalizedReading += 360.0f; }

    float difference = normalizedReading - normalizedValue;
    if (difference > 180.0f) {
        difference -= 360.0f;
    } else if (difference < -180.0f) {
        difference += 360.0f;
    }

    float adjustmentSpeed = 0.2f * std::abs(difference);

    float value = current;
    if (std::abs(difference) < adjustmentSpeed) {
        value = reading;
    } else {
        value += (difference > 0.0f ? 1.0f : -1.0f) * adjustmentSpeed;
    }

    value = std::fmod(value, 360.0f);
    if (value < 0.0f) { value += 360.0f; }
    return value;
}

// Smallest angle between two compass headings.
float headingGap(float a, float b)
{
    float gap = std::fmod(std::abs(a - b), 360.0f);
    return gap > 180.0f ? 360.0f - gap : gap;
}

float distance(const QPointF& a, const QPointF& b)
{
    return static_cast<float>(QLineF(a, b).length());
}

} // namespace

RotatorItem::RotatorItem(QObject* parent)
    : MeterItem(parent)
{
    m_feed.setInterval(kFeedIntervalMs);
    connect(&m_feed, &QTimer::timeout, this, &RotatorItem::feedTick);
}

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

    // From Thetis MeterManager.cs:16610-16631 [v2.10.3.15]
    m_smoothedAz = smoothToward(m_smoothedAz, static_cast<float>(v));
    clearArrivedTargets();
}

// ---------------------------------------------------------------------------
// setElevation()
// Rotor control plan Task 5: nothing smoothed the elevation before, so the
// "Both" and "Ele" faces always drew 0. Thetis feeds elevation through the
// second rotator item's Update() (ReadingSource Reading.ELE, AddRotator
// MeterManager.cs:23493-23503): the reading is held to 0..90, then smoothed
// exactly as azimuth.
// ---------------------------------------------------------------------------
void RotatorItem::setElevation(float ele)
{
    m_elevation = ele;
    // From Thetis MeterManager.cs:16604-16608 [v2.10.3.15]
    float reading = ele;
    if (reading < 0.0f) { reading = 0.0f; }
    if (reading > 90.0f) { reading = 90.0f; }
    m_smoothedEle = smoothToward(m_smoothedEle, reading);
    clearArrivedTargets();
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
// faceRect()
// Az and Ele draw in a square. Both draws the compass and the elevation
// quarter side by side, so its rect is twice as wide as it is high.
// ---------------------------------------------------------------------------
QRect RotatorItem::faceRect(int widgetW, int widgetH) const
{
    if (m_mode != RotatorMode::Both) { return squareRect(widgetW, widgetH); }
    // From Thetis MeterManager.cs:23474-23486 [v2.10.3.15]: in BOTH the
    // rotator is one wide and half as high (ri.Size = new SizeF(1f, fSize),
    // fSize 0.5). The text positions (x + w * 0.75f) assume it.
    const QRect pr = pixelRect(widgetW, widgetH);
    const int h = std::min(pr.height(), pr.width() / 2);
    const int w = 2 * h;
    return QRect(pr.left() + (pr.width() - w) / 2,
                 pr.top() + (pr.height() - h) / 2, w, h);
}

// ---------------------------------------------------------------------------
// faces()
// Where each face's centre is and how far its pointer and hit ring reach.
// From Thetis MeterManager.cs:36721-36738, 37013-37043 [v2.10.3.15] (renderRotator()).
// ---------------------------------------------------------------------------
RotatorItem::Faces RotatorItem::faces(const QRect& rect) const
{
    Faces f;
    f.w = static_cast<float>(rect.width());
    f.h = static_cast<float>(rect.height());
    const float w = f.w;
    const float h = f.h;

    // From Thetis MeterManager.cs:36721 [v2.10.3.15]
    const float xShift = m_mode == RotatorMode::Both ? 2.0f * (w * 0.0125f) : 0.0f;
    // From Thetis MeterManager.cs:36722 [v2.10.3.15]
    f.radiusStop = m_mode == RotatorMode::Ele ? (h * 0.09f) / 2.0f : (h * 0.15f) / 2.0f; // to include the numbers when clicking to move the rotator

    // From Thetis MeterManager.cs:36729-36738 [v2.10.3.15]
    f.az = QPointF(xShift + static_cast<float>(rect.left()) + h / 2.0f,
                   static_cast<float>(rect.top()) + h / 2.0f);
    f.radiusTipAz = (h * 0.78f) / 2.0f;
    f.radiusExtraAz = (h * 0.98f) / 2.0f; // to include the numbers when clicking to move the rotator

    if (m_mode == RotatorMode::Ele) {
        // From Thetis MeterManager.cs:37030-37039 [v2.10.3.15]
        const float radius = h * 0.84f;
        f.radiusTipEle = (h * 0.78f);
        f.radiusExtraEle = (h * 0.98f);
        f.ele = QPointF(static_cast<float>(rect.left()) + (h / 2.0f) - (radius / 2.0f),
                        static_cast<float>(rect.bottom()) - (4.0f * (w * 0.0125f)));
    } else {
        // From Thetis MeterManager.cs:37042-37043 [v2.10.3.15]. Thetis
        // takes the item's width as the right edge (w - xShift - h / 2f),
        // which holds only for an item at x = 0; ours uses the rect's right
        // edge so the elevation face follows the item.
        f.radiusTipEle = (h * 0.78f) / 2.0f;
        f.radiusExtraEle = (h * 0.98f) / 2.0f;
        f.ele = QPointF(static_cast<float>(rect.right()) - xShift - h / 2.0f,
                        static_cast<float>(rect.top()) + h / 2.0f);
    }
    return f;
}

QPointF RotatorItem::azimuthCentre(int widgetW, int widgetH) const
{
    return faces(faceRect(widgetW, widgetH)).az;
}

QPointF RotatorItem::elevationCentre(int widgetW, int widgetH) const
{
    return faces(faceRect(widgetW, widgetH)).ele;
}

float RotatorItem::stopCircleRadius(int widgetW, int widgetH) const
{
    return faces(faceRect(widgetW, widgetH)).radiusStop;
}

float RotatorItem::pointerRadius(int widgetW, int widgetH) const
{
    const Faces f = faces(faceRect(widgetW, widgetH));
    return m_mode == RotatorMode::Ele ? f.radiusTipEle : f.radiusTipAz;
}

// ---------------------------------------------------------------------------
// paintCompassFace()
// OverlayStatic — compass ring, tick dots, cardinals / degree labels.
// From Thetis renderRotator() (MeterManager.cs:35191-35303) — Primary (AZ) face.
// ---------------------------------------------------------------------------
void RotatorItem::paintCompassFace(QPainter& p, const QRect& compassRect)
{
    p.setRenderHint(QPainter::Antialiasing, true);

    // Rotor control plan Task 5: the compass sits on the azimuth face's
    // centre, which in Both is the left half of a two-face rect.
    const Faces f = faces(compassRect);
    const float cx = static_cast<float>(f.az.x());
    const float cy = static_cast<float>(f.az.y());
    const float h  = f.h;
    const QRectF disc(cx - h / 2.0f, cy - h / 2.0f, h, h);

    // Step 1: fill background circle
    p.setBrush(m_backgroundColour);
    p.setPen(Qt::NoPen);
    p.drawEllipse(disc);

    // Step 2: draw background image if loaded
    if (!m_bgImage.isNull()) {
        p.save();
        QPainterPath clipPath;
        clipPath.addEllipse(disc);
        p.setClipPath(clipPath);
        p.drawImage(disc, m_bgImage);
        p.restore();
    }

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

    float radius, radius_text;
    Q_UNUSED(w);

    // Rotor control plan Task 5: the centre from faces(), shared with the
    // mouse handling.
    const Faces f = faces(eleRect);
    const float cx = static_cast<float>(f.ele.x());
    const float cy = static_cast<float>(f.ele.y());

    if (m_mode == RotatorMode::Ele) {
        // From Thetis MeterManager.cs:35496-35504 — ELE-only layout
        radius      = h * 0.84f;
        radius_text = h * 0.90f;
    } else {
        // BOTH mode — right half-circle
        // From Thetis MeterManager.cs:35506-35510
        radius      = (h * 0.8f) / 2.0f;
        radius_text = (h * 0.92f) / 2.0f;
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
    // Upstream inline attribution preserved verbatim:
    //   :35220  //[2.10.3.5]MW0LGE note these are reverse RGB, we normally expect BGRA #289
    if (m_mode != RotatorMode::Ele) {
        // From Thetis MeterManager.cs:35187 — xShift for BOTH mode
        // (Rotor control plan Task 5: from faces(), shared with the mouse.)
        const Faces f = faces(compassRect);
        const float cx = static_cast<float>(f.az.x());
        const float cy = static_cast<float>(f.az.y());

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
        float radius_inner_ele, radius_tip_ele;
        // Rotor control plan Task 5: the centre from faces(), shared with
        // the mouse handling.
        const Faces f = faces(compassRect);
        const float cx_ele = static_cast<float>(f.ele.x());
        const float cy_ele = static_cast<float>(f.ele.y());

        if (m_mode == RotatorMode::Ele) {
            // From Thetis MeterManager.cs:35496-35504
            radius_inner_ele = h * 0.75f;
            radius_tip_ele   = h * 0.78f;
        } else {
            // BOTH mode — right half-circle
            // From Thetis MeterManager.cs:35506-35510
            radius_inner_ele = (h * 0.75f) / 2.0f;
            radius_tip_ele   = (h * 0.78f) / 2.0f;
        }

        // From Thetis MeterManager.cs:37069 [v2.10.3.15]: Thetis draws
        // Math.Abs(rotator.Value) % 90f, which shows a rotor at 90 degrees
        // (straight up) as 0. Ours holds the smoothed value to 0..90
        // instead, so 90 reads 90.
        const float degrees_ele = std::clamp(m_smoothedEle, 0.0f, 90.0f);

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
    const QRect cr = faceRect(widgetW, widgetH);
    if (layer == Layer::OverlayStatic) {
        if (m_mode != RotatorMode::Ele) {
            paintCompassFace(p, cr);
        }
        if (m_mode != RotatorMode::Az) {
            paintElevationArc(p, cr);
        }
    } else if (layer == Layer::OverlayDynamic) {
        paintHeading(p, cr);
        paintControl(p, cr);
    }
}

// ---------------------------------------------------------------------------
// paint()
// CPU fallback: render both layers in sequence.
// ---------------------------------------------------------------------------
void RotatorItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect cr = faceRect(widgetW, widgetH);
    if (m_mode != RotatorMode::Ele) {
        paintCompassFace(p, cr);
    }
    if (m_mode != RotatorMode::Az) {
        paintElevationArc(p, cr);
    }
    paintHeading(p, cr);
    paintControl(p, cr);
}

// ---------------------------------------------------------------------------
// Rotor control plan Task 5: the rotor this dial shows and turns.
// ---------------------------------------------------------------------------
void RotatorItem::setRotor(RotorLink::RotorModel* state, RotorCommandSink* commands)
{
    QObject::disconnect(m_rotorState);
    QObject::disconnect(m_rotorPosition);
    cancelControl();
    m_rotor = state;
    m_commands = state ? commands : nullptr;
    if (!m_rotor) {
        m_feed.stop();
        return;
    }
    m_rotorState = connect(m_rotor.data(), &RotorLink::RotorModel::stateChanged,
                           this, &RotatorItem::onRotorChanged);
    m_rotorPosition = connect(m_rotor.data(), &RotorLink::RotorModel::positionChanged,
                              this, &RotatorItem::onRotorChanged);
    onRotorChanged();
}

bool RotatorItem::rotorControllable() const
{
    return m_rotor && m_commands
        && m_rotor->driver() != RotorLink::RotorModel::Driver::None
        && m_rotor->connectionPhase() == TunerModel::ConnectionPhase::Connected;
}

bool RotatorItem::elevationControllable() const
{
    return rotorControllable()
        && m_rotor->axes() == RotorLink::RotorModel::Axes::AzimuthElevation;
}

void RotatorItem::onRotorChanged()
{
    if (!m_rotor) { return; }
    if (!rotorControllable()) {
        // With no rotor to turn the dial does nothing and shows no target.
        cancelControl();
    } else if (!elevationControllable() && m_dragFace == DragFace::Ele) {
        cancelControl();
    }
    // -1 is "unknown" on the rotor object: the dial keeps what it showed.
    m_rotorAz = static_cast<float>(m_rotor->azimuthDeg());
    m_rotorEle = static_cast<float>(m_rotor->elevationDeg());
    feedTick();
    if ((m_rotorAz >= 0.0f || m_rotorEle >= 0.0f) && !m_feed.isActive()) {
        m_feed.start();
    }
    emit repaintRequested();
}

void RotatorItem::feedTick()
{
    bool settled = true;
    if (m_rotorAz >= 0.0f) {
        setValue(m_rotorAz);
        settled = settled && headingGap(m_smoothedAz, m_rotorAz) < kSettledDeg;
    }
    if (m_rotorEle >= 0.0f) {
        setElevation(m_rotorEle);
        settled = settled && std::abs(m_smoothedEle - std::clamp(m_rotorEle, 0.0f, 90.0f)) < kSettledDeg;
    }
    if (settled) { m_feed.stop(); }
    emit repaintRequested();
}

void RotatorItem::cancelControl()
{
    m_pressed = false;
    m_overStop = false;
    m_dragFace = DragFace::None;
    m_showingEleDrag = false;
    m_dragEle = false;
    m_dragDegrees = kNoAngle;
    m_targetAz = kNoAngle;
    m_targetEle = kNoAngle;
}

void RotatorItem::clearArrivedTargets()
{
    // check if arrow at desination  [original inline comment from MeterManager.cs:36918]
    // From Thetis MeterManager.cs:36919 [v2.10.3.15]. Thetis compares
    // degrees_az >= target - 3 && <= target + 3, so a target at 359 is never
    // reached from 1; ours measures the gap the short way round.
    if (m_targetAz != kNoAngle && headingGap(m_smoothedAz, m_targetAz) <= kArrivedDeg) {
        m_targetAz = kNoAngle;
    }
    // From Thetis MeterManager.cs:37108 [v2.10.3.15]
    if (m_targetEle != kNoAngle && std::abs(m_smoothedEle - m_targetEle) <= kArrivedDeg) {
        m_targetEle = kNoAngle;
    }
}

bool RotatorItem::hitTest(const QPointF& pos, int widgetW, int widgetH) const
{
    // A held press keeps the mouse, so a drag let go outside the item
    // still sends, as Thetis's MouseUp does.
    if (m_pressed) { return true; }
    return MeterItem::hitTest(pos, widgetW, widgetH);
}

void RotatorItem::updateStopHover(const QPointF& pos, const Faces& f)
{
    // draw red circle at centre for stop command  [original inline comment from MeterManager.cs:36925]
    // From Thetis MeterManager.cs:36926-36937 [v2.10.3.15]
    bool over = false;
    if (m_mode != RotatorMode::Ele && distance(f.az, pos) <= f.radiusStop) { over = true; }
    // draw red circle at pointer origin for stop command  [original inline comment from MeterManager.cs:37114]
    // From Thetis MeterManager.cs:37115-37126 [v2.10.3.15]
    if (m_mode != RotatorMode::Az && distance(f.ele, pos) <= f.radiusStop) { over = true; }
    m_overStop = over;
    m_lastPos = pos;
}

void RotatorItem::updateDrag(const QPointF& pos, const Faces& f)
{
    if (m_overStop) {
        // From Thetis MeterManager.cs:36937 [v2.10.3.15]: over the stop
        // circle there is no angle to send.
        m_dragDegrees = kNoAngle;
        return;
    }
    if (m_dragFace == DragFace::Az) {
        // From Thetis MeterManager.cs:36966-36983 [v2.10.3.15]
        //get the angle through the mouse using atan2, radians
        const float deltaX = static_cast<float>(pos.x() - f.az.x());
        const float deltaY = static_cast<float>(pos.y() - f.az.y());
        const float rad = std::atan2(deltaY, deltaX);
        float temp_degrees = rad * (180.0f / std::numbers::pi_v<float>); // the angle we need to send
        temp_degrees = std::fmod(temp_degrees + 90.0f, 360.0f);
        if (temp_degrees < 0.0f) {
            temp_degrees += 360.0f;
        }
        m_dragDegrees = temp_degrees;
        m_dragEle = false;
    } else if (m_dragFace == DragFace::Ele) {
        // From Thetis MeterManager.cs:37147-37175 [v2.10.3.15]
        const float deltaX = static_cast<float>(pos.x() - f.ele.x());
        const float deltaY = static_cast<float>(pos.y() - f.ele.y());
        const float rad = std::atan2(deltaY, deltaX);
        float temp_degrees = -rad * (180.0f / std::numbers::pi_v<float>);
        if ((temp_degrees >= 0.0f && temp_degrees <= 90.0f) || m_showingEleDrag) {
            //get the angle through the mouse using atan2, radians
            if (temp_degrees > 90.0f) { temp_degrees = 90.0f; }
            if (temp_degrees < 0.0f) { temp_degrees = 0.0f; }
            m_showingEleDrag = true;
            m_dragDegrees = temp_degrees;
            m_dragEle = true;
        } else {
            m_showingEleDrag = false;
        }
    }
}

bool RotatorItem::handleMousePress(QMouseEvent* event, int widgetW, int widgetH)
{
    if (!event || event->button() != Qt::LeftButton) { return false; }
    if (!rotorControllable()) { return false; }
    const Faces f = faces(faceRect(widgetW, widgetH));
    const QPointF pos = event->position();
    updateStopHover(pos, f);
    if (m_overStop) {
        // From Thetis MeterManager.cs:16711-16717 [v2.10.3.15]
        // (clsRotatorItem.MouseDown): a press on the stop circle stops.
        m_pressed = true;
        m_dragFace = DragFace::None;
        m_dragDegrees = kNoAngle;
        sendStop();
        return true;
    }
    // find outer edge  [original inline comment from MeterManager.cs:36952]
    // From Thetis MeterManager.cs:36952-36959, 37139-37145 [v2.10.3.15]:
    // a drag starts only from a press inside the ring of numbers.
    DragFace face = DragFace::None;
    if (m_mode != RotatorMode::Ele && distance(f.az, pos) <= f.radiusExtraAz) {
        face = DragFace::Az;
    } else if (m_mode != RotatorMode::Az && elevationControllable()
               && distance(f.ele, pos) <= f.radiusExtraEle) {
        const float deg = -std::atan2(static_cast<float>(pos.y() - f.ele.y()),
                                      static_cast<float>(pos.x() - f.ele.x()))
            * (180.0f / std::numbers::pi_v<float>);
        if (deg >= 0.0f && deg <= 90.0f) { face = DragFace::Ele; }
    }
    if (face == DragFace::None) { return false; }
    m_pressed = true;
    m_dragFace = face;
    m_showingEleDrag = false;
    m_dragDegrees = kNoAngle;
    updateDrag(pos, f);
    return true;
}

bool RotatorItem::handleMouseMove(QMouseEvent* event, int widgetW, int widgetH)
{
    if (!event) { return false; }
    if (!rotorControllable()) {
        const bool showing = m_pressed || m_overStop;
        cancelControl();
        return showing;
    }
    const Faces f = faces(faceRect(widgetW, widgetH));
    const bool wasOverStop = m_overStop;
    updateStopHover(event->position(), f);
    if (m_pressed) {
        // Nothing is sent while dragging; the line shows where a release
        // would turn the rotor.
        updateDrag(event->position(), f);
        return true;
    }
    return wasOverStop != m_overStop;
}

bool RotatorItem::handleMouseRelease(QMouseEvent* event, int widgetW, int widgetH)
{
    if (!event || !m_pressed) { return false; }
    if (rotorControllable()) {
        const Faces f = faces(faceRect(widgetW, widgetH));
        updateStopHover(event->position(), f);
        if (m_overStop) {
            // From Thetis MeterManager.cs:16704-16709 [v2.10.3.15]
            // (clsRotatorItem.MouseUp): let go on the stop circle stops.
            sendStop();
        } else if (m_dragFace != DragFace::None && m_dragDegrees >= 0.0f) {
            //send rotator position message  [original inline comment from MeterManager.cs:37204]
            // From Thetis MeterManager.cs:37205-37217 [v2.10.3.15]
            sendTarget();
        }
    }
    m_pressed = false;
    m_dragFace = DragFace::None;
    m_showingEleDrag = false;
    m_dragEle = false;
    m_dragDegrees = kNoAngle;
    return true;
}

void RotatorItem::sendStop()
{
    if (!rotorControllable()) { return; }
    // From Thetis MeterManager.cs:16486-16488 [v2.10.3.15]
    // (clsRotatorItem.SendRotatorMessage): stop sends the stop command (the
    // rotor's own here, not an MMIO template).
    QString why;
    if (!m_commands->requestStopRotor(&why)) {
        qCWarning(lcMeter) << "The rotor did not take Stop:" << why;
    }
}

void RotatorItem::sendTarget()
{
    if (!rotorControllable()) { return; }
    // From Thetis MeterManager.cs:16494, 16498 [v2.10.3.15]: the heading
    // goes as whole degrees ((int)dragging_rotator_degrees).
    const double degrees = std::trunc(static_cast<double>(m_dragDegrees));
    QString why;
    if (m_dragEle) {
        if (!elevationControllable()) { return; }
        // The rotor commands always carry an azimuth: keep the one the rotor
        // is turning to, else where it points.
        const double azimuth = m_rotor->targetAzimuthDeg() >= 0.0
            ? m_rotor->targetAzimuthDeg() : m_rotor->azimuthDeg();
        if (azimuth < 0.0) {
            qCWarning(lcMeter) << "The rotor's heading is not known yet; elevation not sent";
            return;
        }
        if (m_commands->requestRotorTarget(azimuth, degrees, &why)) {
            // From Thetis MeterManager.cs:37211-37212 [v2.10.3.15]
            m_targetEle = static_cast<float>(degrees);
            clearArrivedTargets();
            return;
        }
    } else {
        if (m_commands->requestRotorTarget(degrees, -1.0, &why)) {
            // From Thetis MeterManager.cs:37213-37214 [v2.10.3.15]
            m_targetAz = static_cast<float>(degrees);
            clearArrivedTargets();
            return;
        }
    }
    qCWarning(lcMeter) << "The rotor did not take the turn:" << why;
}

// ---------------------------------------------------------------------------
// paintControl()
// OverlayDynamic: the drag line, the target marker and the stop circle.
// From Thetis MeterManager.cs:36921-37008, 37110-37201 [v2.10.3.15] (renderRotator()).
// Thetis draws these in the control colour (LimeGreen); the plan has them
// amber.
// ---------------------------------------------------------------------------
void RotatorItem::paintControl(QPainter& p, const QRect& rect)
{
    if (!rotorControllable()) { return; }
    const Faces f = faces(rect);
    const float h = f.h;
    const QColor control(Style::kAmberText);
    QPen controlPen(control, std::max(1.0f, h * 0.01f));
    controlPen.setCapStyle(Qt::RoundCap);
    p.setRenderHint(QPainter::Antialiasing, true);

    const auto azTip = [&](float degrees) {
        // Convert degrees to radians, and -90 to top
        const double rad = qDegreesToRadians(static_cast<double>(degrees) - 90.0);
        const double r = f.radiusTipAz;
        return QPointF(f.az.x() + r * std::cos(rad),
                       f.az.y() + r * std::sin(rad));
    };
    const auto eleTip = [&](float degrees) {
        const double rad = qDegreesToRadians(-static_cast<double>(degrees));
        const double r = f.radiusTipEle;
        return QPointF(f.ele.x() + r * std::cos(rad),
                       f.ele.y() + r * std::sin(rad));
    };

    // The would-be target while dragging.
    // From Thetis MeterManager.cs:36960-36971, 37159-37167 [v2.10.3.15]
    if (m_pressed && m_dragFace != DragFace::None && m_dragDegrees >= 0.0f && !m_overStop) {
        const QPointF centre = m_dragEle ? f.ele : f.az;
        p.setPen(Qt::NoPen);
        p.setBrush(m_bigBlobColour);
        p.drawEllipse(centre, h * 0.015f, h * 0.015f);
        p.setPen(controlPen);
        p.setBrush(Qt::NoBrush);
        p.drawLine(centre, m_dragEle ? eleTip(m_dragDegrees) : azTip(m_dragDegrees));
    }

    //draw to angle
    //set to -999 when pointer gets close
    // From Thetis MeterManager.cs:36988-36998, 37181-37191 [v2.10.3.15]
    p.setPen(controlPen);
    p.setBrush(Qt::NoBrush);
    if (m_targetAz != kNoAngle && m_mode != RotatorMode::Ele) {
        p.drawLine(f.az, azTip(m_targetAz));
    }
    if (m_targetEle != kNoAngle && m_mode != RotatorMode::Az) {
        p.drawLine(f.ele, eleTip(m_targetEle));
    }

    //draw over the top again
    // From Thetis MeterManager.cs:36930-36937, 37000-37006 [v2.10.3.15]
    if (m_overStop) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(Qt::red));
        if (m_mode != RotatorMode::Ele && distance(f.az, m_lastPos) <= f.radiusStop) {
            p.drawEllipse(f.az, f.radiusStop, f.radiusStop);
        }
        if (m_mode != RotatorMode::Az && distance(f.ele, m_lastPos) <= f.radiusStop) {
            p.drawEllipse(f.ele, f.radiusStop, f.radiusStop);
        }
    }
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
