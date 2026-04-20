// =================================================================
// src/gui/meters/presets/SMeterPresetItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
//   (AddSMeterBarSignal / addSMeterBar, line 21499 / 21523)
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
//                via Claude Opus 4.7. Calibration tuple and colours
//                are byte-for-byte from Thetis addSMeterBar @501e3f5.
// =================================================================

#include "gui/meters/presets/SMeterPresetItem.h"
#include "gui/meters/MeterPoller.h"  // MeterBinding::*

#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPen>

namespace NereusSDR {

SMeterPresetItem::SMeterPresetItem(QObject* parent)
    : MeterItem(parent)
{
    setBindingId(MeterBinding::SignalAvg);

    // 3-point non-linear calibration — byte-for-byte from Thetis
    // addSMeterBar.
    // From Thetis MeterManager.cs:21547 [@501e3f5] — -133 dBm -> 0.0
    // (S0 edge; S0 is actually -127 dBm but Thetis pins -133 as the
    // left edge).
    m_calibration.insert(-133.0,  0.00f);
    // From Thetis MeterManager.cs:21548 [@501e3f5] — -73 dBm (S9) -> 0.5
    m_calibration.insert( -73.0,  0.50f);
    // From Thetis MeterManager.cs:21549 [@501e3f5] — -13 dBm (S9+60) -> 0.99
    m_calibration.insert( -13.0,  0.99f);
}

bool SMeterPresetItem::hasCalibrationPoint(double dbm) const
{
    return m_calibration.contains(dbm);
}

float SMeterPresetItem::valueToNormalizedX(double dbm) const
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

void SMeterPresetItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (rect.width() <= 0 || rect.height() <= 0) {
        return;
    }

    // Layout: 45% title strip (scale ShowType), 55% bar + tick area.
    // Matches Thetis addSMeterBar where scale+bar share the rect and
    // the scale's ShowType centers the title above the bar.
    const QRect titleRect(rect.x(), rect.y(), rect.width(),
                          rect.height() * 45 / 100);
    const QRect barRect(rect.x() + rect.width() * 2 / 100,
                        rect.y() + rect.height() * 45 / 100,
                        rect.width() * 96 / 100,
                        rect.height() * 50 / 100);

    // Backdrop — Thetis Color.FromArgb(32,32,32) (MeterManager.cs:21571)
    p.save();
    p.fillRect(rect, m_backdropColor);

    // Title centered in top strip.
    p.setPen(m_titleColor);
    QFont f = p.font();
    f.setBold(true);
    p.setFont(f);
    p.drawText(titleRect, Qt::AlignCenter, QStringLiteral("S-Meter"));

    // Line-style bar (Thetis BarStyle::Line — baseline + marker).
    const double seed = -133.0;  // From Thetis addSMeterBar seed
                                  // (MeterManager.cs:21578)
    const float normX = valueToNormalizedX(seed);
    const int baseY = barRect.y() + barRect.height() / 2;
    QPen pen(m_barColor);
    pen.setWidthF(qMax(1.0, barRect.height() * 0.08));
    p.setPen(pen);
    p.drawLine(barRect.x(), baseY,
               barRect.x() + static_cast<int>(barRect.width() * normX), baseY);

    // Readout (right-aligned dBm display).
    if (m_showReadout) {
        p.setPen(m_readoutColor);
        QFont rf = p.font();
        rf.setPointSizeF(qMax(6.0, barRect.height() * 0.35));
        p.setFont(rf);
        p.drawText(barRect,
                   Qt::AlignRight | Qt::AlignVCenter,
                   QStringLiteral("%1 dBm").arg(seed, 0, 'f', 0));
    }
    p.restore();
}

QString SMeterPresetItem::serialize() const
{
    QJsonObject o;
    o.insert(QStringLiteral("kind"),        QStringLiteral("SMeterPreset"));
    o.insert(QStringLiteral("x"),           static_cast<double>(m_x));
    o.insert(QStringLiteral("y"),           static_cast<double>(m_y));
    o.insert(QStringLiteral("w"),           static_cast<double>(m_w));
    o.insert(QStringLiteral("h"),           static_cast<double>(m_h));
    o.insert(QStringLiteral("bindingId"),   bindingId());
    o.insert(QStringLiteral("showReadout"), m_showReadout);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool SMeterPresetItem::deserialize(const QString& data)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("kind")).toString() != QStringLiteral("SMeterPreset")) {
        return false;
    }
    setRect(static_cast<float>(o.value(QStringLiteral("x")).toDouble(m_x)),
            static_cast<float>(o.value(QStringLiteral("y")).toDouble(m_y)),
            static_cast<float>(o.value(QStringLiteral("w")).toDouble(m_w)),
            static_cast<float>(o.value(QStringLiteral("h")).toDouble(m_h)));
    setBindingId(o.value(QStringLiteral("bindingId")).toInt(bindingId()));
    m_showReadout = o.value(QStringLiteral("showReadout")).toBool(m_showReadout);
    return true;
}

} // namespace NereusSDR
