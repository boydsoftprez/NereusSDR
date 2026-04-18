// =================================================================
// src/gui/meters/MagicEyeItem.cpp  (NereusSDR)
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

#include "MagicEyeItem.h"

// From Thetis clsMagicEyeItem (MeterManager.cs:15855+)

#include <QPainter>
#include <QRadialGradient>
#include <QStringList>

#include <algorithm>
#include <cmath>

namespace NereusSDR {

// ---------------------------------------------------------------------------
// setBezelImagePath()
// Store path and load image from disk.
// ---------------------------------------------------------------------------
void MagicEyeItem::setBezelImagePath(const QString& path)
{
    m_bezelPath = path;
    m_bezelImage.load(path);
}

// ---------------------------------------------------------------------------
// setValue()
// From Thetis clsMagicEyeItem — exponential smoothing toward incoming dBm.
// ---------------------------------------------------------------------------
void MagicEyeItem::setValue(double v)
{
    MeterItem::setValue(v);
    m_smoothedDbm += kSmoothAlpha * (static_cast<float>(v) - m_smoothedDbm);
    m_smoothedDbm = std::clamp(m_smoothedDbm, kS0Dbm, kMaxDbm);
}

// ---------------------------------------------------------------------------
// dbmToFraction()
// Returns 0.0 at S0 (-127 dBm), 1.0 at S9+60 (-13 dBm).
// From Thetis clsMagicEyeItem (MeterManager.cs:15855+)
// ---------------------------------------------------------------------------
float MagicEyeItem::dbmToFraction(float dbm) const
{
    return std::clamp((dbm - kS0Dbm) / (kMaxDbm - kS0Dbm), 0.0f, 1.0f);
}

// ---------------------------------------------------------------------------
// paint()
// From Thetis clsMagicEyeItem (MeterManager.cs:15855+)
// Renders a green phosphor disc with dark shadow wedge (EM84/6E5 tuning eye).
// Shadow opens wide at no signal, closes to a sliver at full signal.
// ---------------------------------------------------------------------------
void MagicEyeItem::paint(QPainter& p, int widgetW, int widgetH)
{
    // Step 1: compute square eye rect centered within item pixel rect
    QRect pr = pixelRect(widgetW, widgetH);
    int side = std::min(pr.width(), pr.height());
    QRect eyeRect(pr.left() + (pr.width() - side) / 2,
                  pr.top()  + (pr.height() - side) / 2,
                  side, side);

    // Step 2: enable antialiasing
    p.setRenderHint(QPainter::Antialiasing, true);

    // Step 3: dark background circle
    p.setBrush(QColor(0x0a, 0x0a, 0x0a));
    p.setPen(Qt::NoPen);
    p.drawEllipse(eyeRect);

    // Step 4: green phosphor glow via radial gradient
    QRadialGradient grad(eyeRect.center(), side / 2.0);
    grad.setColorAt(0.0, m_glowColor);
    grad.setColorAt(0.6, m_glowColor.darker(200));
    grad.setColorAt(1.0, QColor(0x00, 0x44, 0x00, 0x40));
    p.setBrush(grad);
    p.drawEllipse(eyeRect);

    // Step 5: shadow wedge — dark pie slice, symmetric about 12 o'clock
    // Shadow narrows as signal strength increases (fraction → 1.0)
    // From Thetis clsMagicEyeItem (MeterManager.cs:15855+)
    float fraction  = dbmToFraction(m_smoothedDbm);
    float shadowDeg = kMaxShadowDeg - fraction * (kMaxShadowDeg - kMinShadowDeg);
    float halfShadow = shadowDeg / 2.0f;

    // QPainter angles: 0° = 3 o'clock, counter-clockwise positive.
    // Top (12 o'clock) = 90°. drawPie uses 1/16th-degree units.
    int startAngle = static_cast<int>((90.0f - halfShadow) * 16.0f);
    int spanAngle  = static_cast<int>(shadowDeg * 16.0f);

    p.setBrush(QColor(0x0a, 0x0a, 0x0a, 0xe0));
    p.setPen(Qt::NoPen);
    p.drawPie(eyeRect, startAngle, spanAngle);

    // Step 6: center dot (anode)
    int dotR = side / 12;
    p.setBrush(QColor(0x20, 0x20, 0x20));
    p.drawEllipse(eyeRect.center(), dotR, dotR);

    // Step 7: optional bezel overlay image
    if (!m_bezelImage.isNull()) {
        p.drawImage(eyeRect, m_bezelImage);
    }

    // Step 8: disable antialiasing
    p.setRenderHint(QPainter::Antialiasing, false);
}

// ---------------------------------------------------------------------------
// serialize()
// Format: MAGICEYE|x|y|w|h|bindingId|zOrder|glowColor|bezelPath
// ---------------------------------------------------------------------------
QString MagicEyeItem::serialize() const
{
    return QStringLiteral("MAGICEYE|%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(static_cast<double>(m_x))
        .arg(static_cast<double>(m_y))
        .arg(static_cast<double>(m_w))
        .arg(static_cast<double>(m_h))
        .arg(m_bindingId)
        .arg(m_zOrder)
        .arg(m_glowColor.name(QColor::HexArgb))
        .arg(m_bezelPath);
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected parts:
// [0]=MAGICEYE [1-6]=base fields [7]=glowColor [8]=bezelPath
// ---------------------------------------------------------------------------
bool MagicEyeItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 9 || parts[0] != QLatin1String("MAGICEYE")) {
        return false;
    }

    // Parse base fields (indices 1..6)
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    if (!MeterItem::deserialize(base)) {
        return false;
    }

    const QColor glowColor(parts[7]);
    if (!glowColor.isValid()) {
        return false;
    }

    m_glowColor = glowColor;
    setBezelImagePath(parts[8]);  // empty string is valid (no bezel)

    return true;
}

} // namespace NereusSDR
