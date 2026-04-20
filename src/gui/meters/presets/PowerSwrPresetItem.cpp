// =================================================================
// src/gui/meters/presets/PowerSwrPresetItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
//   (AddPWRBar, line 23854/23862; AddSWRBar, line 23990)
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
//                via Claude Opus 4.7. Attack/decay ratios and
//                HighPoint thresholds are byte-for-byte from Thetis
//                AddPWRBar / AddSWRBar @501e3f5.
// =================================================================

#include "gui/meters/presets/PowerSwrPresetItem.h"
#include "gui/meters/MeterPoller.h"  // MeterBinding::*

#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QPen>

namespace NereusSDR {

PowerSwrPresetItem::PowerSwrPresetItem(QObject* parent)
    : MeterItem(parent)
{
    // Power bar — Thetis AddPWRBar (MeterManager.cs:23862 onward).
    // Range 0..150W matches createPowerSwrPreset's pre-refactor
    // choice; Thetis addresses multiple power classes via commented-
    // out 200W / 500W paths (MeterManager.cs:23895-23934), with the
    // active path being the 100W default (5 -> 0.1875, 100 -> 0.75,
    // 120 -> 0.99 — MeterManager.cs:23910-23915). We keep the
    // NereusSDR 150W upper bound for visual parity with the existing
    // createPowerSwrPreset factory.
    m_bars[0] = {
        QStringLiteral("Power"),
        MeterBinding::TxPower,
        0.0,
        150.0,
        100.0,  // HighPoint at 100W — From Thetis MeterManager.cs:23914 [@501e3f5]
        QStringLiteral(" W")
    };
    // SWR bar — Thetis AddSWRBar (MeterManager.cs:23990).
    // Range 1..10 (createPowerSwrPreset uses 1..5; we widen to 10 to
    // match the AnanMM SWR calibration endpoint and capture severe
    // mismatches). HighPoint at 3:1 matches Thetis
    // MeterManager.cs:24017 (ElementAt(3).Value over 1/1.5/2/3/5).
    m_bars[1] = {
        QStringLiteral("SWR"),
        MeterBinding::TxSwr,
        1.0,
        10.0,
        3.0,  // HighPoint at SWR 3:1 — From Thetis MeterManager.cs:24017 [@501e3f5]
        QStringLiteral(":1")
    };
}

QString PowerSwrPresetItem::barName(int i) const
{
    if (i < 0 || i >= kBarCount) {
        return QString();
    }
    return m_bars[i].label;
}

int PowerSwrPresetItem::barBindingId(int i) const
{
    if (i < 0 || i >= kBarCount) {
        return -1;
    }
    return m_bars[i].bindingId;
}

double PowerSwrPresetItem::barMinVal(int i) const
{
    if (i < 0 || i >= kBarCount) {
        return 0.0;
    }
    return m_bars[i].minVal;
}

double PowerSwrPresetItem::barMaxVal(int i) const
{
    if (i < 0 || i >= kBarCount) {
        return 0.0;
    }
    return m_bars[i].maxVal;
}

void PowerSwrPresetItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (rect.width() <= 0 || rect.height() <= 0) {
        return;
    }

    p.save();
    p.fillRect(rect, m_backdropColor);

    // Two equal vertical halves: power on top, SWR on bottom.
    for (int i = 0; i < kBarCount; ++i) {
        const Bar& b = m_bars[i];
        const int halfH = rect.height() / 2;
        const QRect halfRect(rect.x(), rect.y() + i * halfH,
                             rect.width(), halfH);

        // Label strip (top ~25% of half) — NereusSDR label style,
        // matches createPowerSwrPreset's TextItem composition.
        const QRect labelRect(halfRect.x() + halfRect.width() * 2 / 100,
                              halfRect.y(),
                              halfRect.width() * 50 / 100,
                              halfRect.height() * 25 / 100);
        const QRect readoutRect(halfRect.x() + halfRect.width() * 50 / 100,
                                halfRect.y(),
                                halfRect.width() * 48 / 100,
                                halfRect.height() * 25 / 100);
        p.setPen(QColor(0x80, 0x90, 0xa0));
        p.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter, b.label);

        if (m_showReadout) {
            p.setPen(m_readoutColor);
            QFont rf = p.font();
            rf.setBold(true);
            p.setFont(rf);
            const double seed = b.minVal;
            p.drawText(readoutRect,
                       Qt::AlignRight | Qt::AlignVCenter,
                       QStringLiteral("%1%2").arg(seed, 0, 'f', 0).arg(b.suffix));
        }

        // Bar strip (mid ~45% of half).
        const QRect barRect(halfRect.x() + halfRect.width() * 2 / 100,
                            halfRect.y() + halfRect.height() * 25 / 100,
                            halfRect.width() * 96 / 100,
                            halfRect.height() * 45 / 100);
        p.fillRect(barRect, QColor(16, 16, 24));

        // Bar fill — pinned to the seed (minVal) in paint-smoke
        // context since there's no live data; real paint with
        // polled value will scale to range.
        const double seed = b.minVal;
        const double t = (b.maxVal - b.minVal) > 0
                           ? (seed - b.minVal) / (b.maxVal - b.minVal)
                           : 0.0;
        const int fillW = static_cast<int>(barRect.width() * t);
        const QColor fill = (seed >= b.redThreshold) ? m_barRedColor : m_barColor;
        p.fillRect(QRect(barRect.x(), barRect.y(), fillW, barRect.height()),
                   fill);

        // Scale strip (bottom ~30% of half) — tick marks.
        p.setPen(QColor(0xc8, 0xd8, 0xe8));
        const int ticks = 5;
        for (int k = 0; k <= ticks; ++k) {
            const int x = barRect.x() + barRect.width() * k / ticks;
            p.drawLine(x,
                       barRect.y() + barRect.height(),
                       x,
                       barRect.y() + barRect.height() + halfRect.height() * 10 / 100);
        }
    }
    p.restore();
}

QString PowerSwrPresetItem::serialize() const
{
    QJsonObject o;
    o.insert(QStringLiteral("kind"),        QStringLiteral("PowerSwrPreset"));
    o.insert(QStringLiteral("x"),           static_cast<double>(m_x));
    o.insert(QStringLiteral("y"),           static_cast<double>(m_y));
    o.insert(QStringLiteral("w"),           static_cast<double>(m_w));
    o.insert(QStringLiteral("h"),           static_cast<double>(m_h));
    o.insert(QStringLiteral("showReadout"), m_showReadout);
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool PowerSwrPresetItem::deserialize(const QString& data)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("kind")).toString() != QStringLiteral("PowerSwrPreset")) {
        return false;
    }
    setRect(static_cast<float>(o.value(QStringLiteral("x")).toDouble(m_x)),
            static_cast<float>(o.value(QStringLiteral("y")).toDouble(m_y)),
            static_cast<float>(o.value(QStringLiteral("w")).toDouble(m_w)),
            static_cast<float>(o.value(QStringLiteral("h")).toDouble(m_h)));
    m_showReadout = o.value(QStringLiteral("showReadout")).toBool(m_showReadout);
    return true;
}

} // namespace NereusSDR
