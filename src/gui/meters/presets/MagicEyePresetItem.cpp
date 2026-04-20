// =================================================================
// src/gui/meters/presets/MagicEyePresetItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs (AddMagicEye, line 22249)
//   verbatim upstream header reproduced below.
//
// =================================================================

// --- From MeterManager.cs ---
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

// =================================================================
// Modification history (NereusSDR):
//   2026-04-19 — Reimplemented in C++20/Qt6 for NereusSDR by
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Claude Opus 4.7. 3-point calibration and leaf
//                colour are byte-for-byte from Thetis AddMagicEye
//                @501e3f5.
// =================================================================

#include "gui/meters/presets/MagicEyePresetItem.h"
#include "gui/meters/MeterPoller.h"  // MeterBinding::*

#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

namespace NereusSDR {

MagicEyePresetItem::MagicEyePresetItem(QObject* parent)
    : MeterItem(parent)
{
    // Default binding — Thetis AddMagicEye:
    //   me.ReadingSource = Reading.AVG_SIGNAL_STRENGTH;
    // From Thetis MeterManager.cs:22266 [@501e3f5]
    setBindingId(MeterBinding::SignalAvg);

    // Default leaf colour — Thetis AddMagicEye:
    //   me.Colour = System.Drawing.Color.Lime;
    // Qt's Qt::green is #00FF00 — the same 24-bit RGB as
    // System.Drawing.Color.Lime (per GDI colour table).
    // From Thetis MeterManager.cs:22265 [@501e3f5]
    m_leafColor = QColor(Qt::green);

    // 3-point calibration — byte-for-byte from Thetis AddMagicEye.
    // From Thetis MeterManager.cs:22267 [@501e3f5] — -127 dBm -> 0.0
    m_calibration.insert(-127.0, 0.00f);
    // From Thetis MeterManager.cs:22268 [@501e3f5] — -73 dBm (S9) -> 0.85
    m_calibration.insert( -73.0, 0.85f);
    // From Thetis MeterManager.cs:22269 [@501e3f5] — -13 dBm (S9+60) -> 1.0
    m_calibration.insert( -13.0, 1.00f);

    // Load the bullseye bezel image — registered in resources.qrc
    // (corresponds to Thetis "eye-bezel" ImageName, MeterManager.cs:22279).
    m_background.load(QStringLiteral(":/meters/eye-bezel.png"));
}

float MagicEyePresetItem::apertureFraction(double dbm) const
{
    if (m_calibration.isEmpty()) {
        return 0.0f;
    }
    auto first = m_calibration.constBegin();
    auto last  = std::prev(m_calibration.constEnd());
    if (dbm <= first.key()) {
        return first.value();
    }
    if (dbm >= last.key()) {
        return last.value();
    }
    auto upper = m_calibration.lowerBound(dbm);
    auto lower = std::prev(upper);
    const double a = lower.key();
    const double b = upper.key();
    const float  t = static_cast<float>((dbm - a) / (b - a));
    return lower.value() + t * (upper.value() - lower.value());
}

void MagicEyePresetItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (rect.width() <= 0 || rect.height() <= 0) {
        return;
    }

    p.save();

    // Draw the circular leaf first (so bezel paints over its edges).
    // Leaf geometry: a filled sector at the centre whose aperture
    // sweep angle grows with apertureFraction().
    const int cx = rect.x() + rect.width()  / 2;
    const int cy = rect.y() + rect.height() / 2;
    const int r  = qMin(rect.width(), rect.height()) / 2;
    const QRect eyeRect(cx - r, cy - r, 2 * r, 2 * r);

    // Paint base eye disk (dark).
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 30, 20));
    p.drawEllipse(eyeRect);

    // Paint the leaf pie-slice. Edit-container refactor Task 20:
    // prefer the live value the poller pushed via setValue(); fall
    // back to -127 dBm (no signal) when no data has arrived yet.
    const double liveValue = value();
    const double seed = (liveValue <= -139.9) ? -127.0 : liveValue;
    const float frac = apertureFraction(seed);
    // Thetis draws two leaves, each opening from the top — we render
    // a single pair of opposing fan sectors for the same visual.
    const int sweepDeg = static_cast<int>(180.0f * frac);
    p.setBrush(m_leafColor);
    // Qt drawPie takes start and span angles in 1/16 degree units.
    p.drawPie(eyeRect,
              (90 - sweepDeg / 2) * 16,
              sweepDeg * 16);

    // Bullseye bezel image on top (hides the leaf edges).
    // Smooth transform + antialiasing keeps the circular bezel crisp
    // at non-native container sizes (nearest-neighbour default produces
    // visibly jagged circle edges).
    if (!m_background.isNull()) {
        p.save();
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.drawImage(eyeRect, m_background);
        p.restore();
    }

    p.restore();
}

QString MagicEyePresetItem::serialize() const
{
    QJsonObject o;
    o.insert(QStringLiteral("kind"),     QStringLiteral("MagicEyePreset"));
    o.insert(QStringLiteral("x"),        static_cast<double>(m_x));
    o.insert(QStringLiteral("y"),        static_cast<double>(m_y));
    o.insert(QStringLiteral("w"),        static_cast<double>(m_w));
    o.insert(QStringLiteral("h"),        static_cast<double>(m_h));
    o.insert(QStringLiteral("bindingId"), bindingId());
    o.insert(QStringLiteral("leafColor"), m_leafColor.name(QColor::HexArgb));
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool MagicEyePresetItem::deserialize(const QString& data)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("kind")).toString() != QStringLiteral("MagicEyePreset")) {
        return false;
    }
    setRect(static_cast<float>(o.value(QStringLiteral("x")).toDouble(m_x)),
            static_cast<float>(o.value(QStringLiteral("y")).toDouble(m_y)),
            static_cast<float>(o.value(QStringLiteral("w")).toDouble(m_w)),
            static_cast<float>(o.value(QStringLiteral("h")).toDouble(m_h)));
    setBindingId(o.value(QStringLiteral("bindingId")).toInt(bindingId()));
    const QString col = o.value(QStringLiteral("leafColor")).toString();
    if (!col.isEmpty()) {
        m_leafColor = QColor(col);
    }
    return true;
}

} // namespace NereusSDR
