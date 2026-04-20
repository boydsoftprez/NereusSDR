// =================================================================
// src/gui/meters/presets/SignalTextPresetItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs (AddSMeterBarText, line 21678)
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
//                via Claude Opus 4.7. Colour / default binding /
//                backdrop are byte-for-byte from Thetis
//                AddSMeterBarText @501e3f5.
// =================================================================

#include "gui/meters/presets/SignalTextPresetItem.h"
#include "gui/meters/MeterPoller.h"  // MeterBinding::*

#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>

#include <cmath>

namespace NereusSDR {

SignalTextPresetItem::SignalTextPresetItem(QObject* parent)
    : MeterItem(parent)
{
    // Default binding — Thetis AddSMeterBarText:
    //   cst.ReadingSource = Reading.AVG_SIGNAL_STRENGTH;
    // From Thetis MeterManager.cs:21696 [@501e3f5]
    setBindingId(MeterBinding::SignalAvg);
}

QString SignalTextPresetItem::formatReadout(double dbm) const
{
    // S-unit mapping: S0 = -127 dBm, S9 = -73 dBm, 6 dB / S-unit
    // (standard amateur S-meter convention, matches Thetis
    // SignalTextItem formatting).
    constexpr double kS0Dbm   = -127.0;
    constexpr double kS9Dbm   = -73.0;
    constexpr double kDbPerS  = 6.0;

    int    sUnit;
    double overrange = 0.0;
    if (dbm <= kS0Dbm) {
        sUnit = 0;
    } else if (dbm >= kS9Dbm) {
        sUnit = 9;
        overrange = dbm - kS9Dbm;
    } else {
        const double sFrac = (dbm - kS0Dbm) / kDbPerS;
        sUnit = static_cast<int>(std::floor(sFrac));
        if (sUnit < 0) { sUnit = 0; }
        if (sUnit > 9) { sUnit = 9; }
    }

    QString sPart = QStringLiteral("S%1").arg(sUnit);
    if (sUnit >= 9 && overrange >= 1.0) {
        const int over = static_cast<int>(std::round(overrange));
        sPart = QStringLiteral("S9 +%1").arg(over);
    }
    return QStringLiteral("%1  %2 dBm")
        .arg(sPart)
        .arg(static_cast<int>(std::round(dbm)));
}

void SignalTextPresetItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (rect.width() <= 0 || rect.height() <= 0) {
        return;
    }

    p.save();
    p.fillRect(rect, m_backdropColor);

    QFont f = p.font();
    // Scale font to fit the rect — Thetis ships 56pt in the default
    // container (MeterManager.cs:21704) but fit-to-rect avoids
    // overflow in narrow NereusSDR containers.
    f.setPointSizeF(qMax(10.0f, qMin(m_fontPoint, rect.height() * 0.6f)));
    f.setBold(true);
    p.setFont(f);
    p.setPen(m_textColor);

    // Idle seed = -140 dBm (below S0) — paint an "S0 -140 dBm" placeholder
    // until the poller sends a live value.
    const double seed = -140.0;
    p.drawText(rect, Qt::AlignCenter, formatReadout(seed));
    p.restore();
}

QString SignalTextPresetItem::serialize() const
{
    QJsonObject o;
    o.insert(QStringLiteral("kind"),      QStringLiteral("SignalTextPreset"));
    o.insert(QStringLiteral("x"),         static_cast<double>(m_x));
    o.insert(QStringLiteral("y"),         static_cast<double>(m_y));
    o.insert(QStringLiteral("w"),         static_cast<double>(m_w));
    o.insert(QStringLiteral("h"),         static_cast<double>(m_h));
    o.insert(QStringLiteral("bindingId"), bindingId());
    o.insert(QStringLiteral("textColor"), m_textColor.name(QColor::HexArgb));
    o.insert(QStringLiteral("fontPoint"), static_cast<double>(m_fontPoint));
    return QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
}

bool SignalTextPresetItem::deserialize(const QString& data)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }
    const QJsonObject o = doc.object();
    if (o.value(QStringLiteral("kind")).toString() != QStringLiteral("SignalTextPreset")) {
        return false;
    }
    setRect(static_cast<float>(o.value(QStringLiteral("x")).toDouble(m_x)),
            static_cast<float>(o.value(QStringLiteral("y")).toDouble(m_y)),
            static_cast<float>(o.value(QStringLiteral("w")).toDouble(m_w)),
            static_cast<float>(o.value(QStringLiteral("h")).toDouble(m_h)));
    setBindingId(o.value(QStringLiteral("bindingId")).toInt(bindingId()));
    const QString col = o.value(QStringLiteral("textColor")).toString();
    if (!col.isEmpty()) {
        m_textColor = QColor(col);
    }
    m_fontPoint = static_cast<float>(o.value(QStringLiteral("fontPoint")).toDouble(40.0));
    return true;
}

} // namespace NereusSDR
