// =================================================================
// src/gui/meters/NeedleScalePwrItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - PR 677 review fix wave (settings M1): a saved power
//                 limit above 5 kW loads as saved. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Colours at their defaults follow the workspace look when
//                 drawn (ItemColourLook.h); saved colours are unchanged.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - NEEDLESCALEPWR building block: the power limit takes
//                 0.1 W to 5000 W (below 0.1 recovers to 100 W) and new
//                 items take the clsNeedleScalePwrItem defaults
//                 (kNeedleScalePwrThetisDefaults). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-10-03 — Responsive object text and measured role fitting by J.J. Boyd
//                 (KG4VCF), AI-assisted via OpenAI Codex.
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

// Modification history (NereusSDR):
//   2026-10-12 - Apply the approved workspace-only 11 px text minimum.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-26 - User text in the record (font family) is joined, not substituted
//                 by chained QString::arg, so a "%1" or "%20" in it survives.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.

#include "gui/styles/WorkspacePreview.h"
#include "NeedleScalePwrItem.h"

#include <cmath>
#include "ItemColourLook.h"
#include "ResponsiveText.h"

// From Thetis clsNeedleScalePwrItem (MeterManager.cs:14888+)
// From Thetis renderNeedleScale (MeterManager.cs:31645-31850)

#include <QPainter>
#include <QFont>
#include <QRect>
#include <QStringList>

#include <algorithm>

namespace NereusSDR {

void NeedleScalePwrItem::setMaxPower(float p)
{
    // From Thetis MeterManager.cs:7132-7139 [v2.10.3.15]: below 0.1 recovers
    // (Thetis takes CurrentPowerRating; NereusSDR takes the setup default).
    // [original inline comment from MeterManager.cs:7137] // incase of 0 recovery from db
    if (!(static_cast<double>(p) >= kMinBarPowerLimitW)) {
        m_maxPower = static_cast<float>(kDefaultBarPowerLimitW);
        return;
    }
    m_maxPower = static_cast<float>(std::min(static_cast<double>(p), kMaxBarPowerLimitW));
}

void NeedleScalePwrItem::applyNewItemDefaults()
{
    MeterItem::applyNewItemDefaults();
    const NeedleScalePwrItemThetisDefaults& d = kNeedleScalePwrThetisDefaults;
    m_lowColour    = QColor::fromRgb(d.lowColour);
    m_highColour   = QColor::fromRgb(d.highColour);
    m_fontFamily   = QString::fromLatin1(d.fontFamily);
    m_fontBold     = d.fontBold;
    m_fontSize     = d.fontSize;
    m_showMarkers  = d.showMarkers;
    m_darkMode     = d.darkMode;
    m_lengthFactor = d.lengthFactor;
    m_direction    = d.direction;
    setMaxPower(static_cast<float>(d.maxPower));
}

// ---------------------------------------------------------------------------
// tidyPower()
// From Thetis renderNeedleScale (MeterManager.cs:31822-31823)
// Formats a watt value as a human-readable string.
// Callers append "W" or "mW" suffix on the final label only.
// ---------------------------------------------------------------------------
QString NeedleScalePwrItem::tidyPower(float watts, bool useMilliwatts)
{
    if (useMilliwatts) {
        return QString::number(static_cast<double>(watts * 1000.0f), 'f', 0);
    }
    if (watts < 8.0f) {
        return QString::number(static_cast<double>(watts), 'f', 1);
    }
    return QString::number(static_cast<double>(watts), 'f', 0);
}

// ---------------------------------------------------------------------------
// paint()
// From Thetis renderNeedleScale (MeterManager.cs:31645-31850)
// Renders text labels at calibration points around the needle arc.
// ---------------------------------------------------------------------------
void NeedleScalePwrItem::paint(QPainter& p, int widgetW, int widgetH)
{
    // From Thetis renderNeedleScale (MeterManager.cs:31645)
    if (m_calibration.isEmpty() || !m_showMarkers) {
        return;
    }

    QRect rect = pixelRect(widgetW, widgetH);

    // From Thetis renderNeedleScale (MeterManager.cs:31660+) — font size scaled to widget
    float fontSizeEm = (m_fontSize / 16.0f) * (static_cast<float>(rect.width()) / 52.0f);

    QFont font;
    font.setFamily(m_fontFamily);
    font.setBold(m_fontBold);
    font.setPointSizeF(static_cast<double>(fontSizeEm)); font = WorkspacePreview::readableFont(font, p.device());
    p.setFont(font);

    // Determine if using milliwatts (max power ≤ 1 W)
    // From Thetis renderNeedleScale (MeterManager.cs:31670)
    bool useMw = (m_maxPower <= 1.0f);

    // Select which calibration points to label — evenly space m_marks indices
    // across the full sorted key list.
    // From Thetis renderNeedleScale (MeterManager.cs:31680+)
    QList<float> keys = m_calibration.keys();
    int total = keys.size();

    if (total == 0 || m_marks <= 0) {
        return;
    }

    int effectiveMarks = qMin(m_marks, total);

    p.setRenderHint(QPainter::Antialiasing, true);

    for (int i = 0; i < effectiveMarks; ++i) {
        int idx = 0;
        if (effectiveMarks > 1) {
            idx = i * (total - 1) / (effectiveMarks - 1);
        }
        idx = qBound(0, idx, total - 1);

        float val    = keys[idx];
        QPointF npos = m_calibration[val];

        // Convert normalized position to pixel coordinates
        int px = rect.left() + static_cast<int>(npos.x() * static_cast<float>(rect.width()));
        int py = rect.top()  + static_cast<int>(npos.y() * static_cast<float>(rect.height()));

        // Compute actual watt value from normalized (0-100 scale)
        // From Thetis renderNeedleScale (MeterManager.cs:31700)
        float watts = val * m_maxPower / 100.0f;

        // Build label text — append unit only on last label
        // From Thetis renderNeedleScale (MeterManager.cs:31822-31823)
        bool isLast = (i == effectiveMarks - 1);
        QString text = tidyPower(watts, useMw);
        if (isLast) {
            text += (useMw ? QStringLiteral("mW") : QStringLiteral("W"));
        }

        // From Thetis renderNeedleScale (MeterManager.cs:31710) — use low colour for labels
        p.setPen(ItemColourLook::effective("PowerScale.lowColour", m_lowColour, this));

        const double scale=objectTextScale(rect,{260,120});
        QRectF label(px-30*scale,py-10*scale,60*scale,20*scale);
        if(label.left()<rect.left()) { label.moveLeft(rect.left()); }
        if(label.right()>rect.right()) { label.moveRight(rect.right()); }
        if(label.top()<rect.top()) { label.moveTop(rect.top()); }
        if(label.bottom()>rect.bottom()) { label.moveBottom(rect.bottom()); }
        p.setFont(fitObjectText(font,fontSizeEm,text,label,true,Qt::TextSingleLine,p.device()));
        p.drawText(label,Qt::AlignCenter,text);
    }

    p.setRenderHint(QPainter::Antialiasing, false);
}

// ---------------------------------------------------------------------------
// serialize()
// Format: NEEDLESCALEPWR|x|y|w|h|bindingId|zOrder|lowColour|highColour|
//         fontFamily|fontSize|fontBold|marks|maxPower|darkMode|
//         calCount|v1:x1:y1|v2:x2:y2|...
// ---------------------------------------------------------------------------
QString NeedleScalePwrItem::serialize() const
{
    // The font family is joined, not substituted: chained .arg() would
    // read a "%1" in it as a placeholder.
    QString s = QStringLiteral("NEEDLESCALEPWR|%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(static_cast<double>(m_x))
        .arg(static_cast<double>(m_y))
        .arg(static_cast<double>(m_w))
        .arg(static_cast<double>(m_h))
        .arg(m_bindingId)
        .arg(m_zOrder)
        .arg(m_lowColour.name(QColor::HexArgb))
        .arg(m_highColour.name(QColor::HexArgb))
        + QLatin1Char('|') + m_fontFamily
        + QStringLiteral("|%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(m_fontSize))
        .arg(m_fontBold ? 1 : 0)
        .arg(m_marks)
        .arg(static_cast<double>(m_maxPower))
        .arg(m_darkMode ? 1 : 0)
        .arg(m_calibration.size());

    // Append calibration points as value:x:y
    for (auto it = m_calibration.constBegin(); it != m_calibration.constEnd(); ++it) {
        s += QStringLiteral("|%1:%2:%3")
            .arg(static_cast<double>(it.key()))
            .arg(it.value().x())
            .arg(it.value().y());
    }

    // Phase 3G-6 block 1: append filter fields after calibration points.
    // Older state that lacks them still deserializes (read is optional).
    s += QStringLiteral("|%1|%2|%3")
        .arg(m_onlyWhenRx ? 1 : 0)
        .arg(m_onlyWhenTx ? 1 : 0)
        .arg(m_displayGroup);

    return s;
}

// ---------------------------------------------------------------------------
// deserialize()
// Parses the format written by serialize().
// Fixed fields at indices 0-15, calibration points starting at index 16.
// ---------------------------------------------------------------------------
bool NeedleScalePwrItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    // Minimum: tag + 6 base fields + 9 type fields + calCount = 16 fields
    if (parts.size() < 16 || parts[0] != QLatin1String("NEEDLESCALEPWR")) {
        return false;
    }

    // Parse base fields (indices 1-6) via MeterItem::deserialize
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    if (!MeterItem::deserialize(base)) {
        return false;
    }

    bool ok = true;

    const QColor lowColour(parts[7]);
    if (!lowColour.isValid()) { return false; }

    const QColor highColour(parts[8]);
    if (!highColour.isValid()) { return false; }

    const QString fontFamily = parts[9];

    const float fontSize = parts[10].toFloat(&ok);
    if (!ok) { return false; }

    const bool fontBold = (parts[11].toInt(&ok) != 0);
    if (!ok) { return false; }

    const int marks = parts[12].toInt(&ok);
    if (!ok) { return false; }

    const float maxPower = parts[13].toFloat(&ok);
    if (!ok) { return false; }

    const bool darkMode = (parts[14].toInt(&ok) != 0);
    if (!ok) { return false; }

    const int calCount = parts[15].toInt(&ok);
    if (!ok) { return false; }

    // Validate we have enough parts for calibration points
    if (parts.size() < 16 + calCount) {
        return false;
    }

    QMap<float, QPointF> calibration;
    for (int i = 0; i < calCount; ++i) {
        const QStringList cal = parts[16 + i].split(QLatin1Char(':'));
        if (cal.size() < 3) { return false; }

        float v  = cal[0].toFloat(&ok);  if (!ok) { return false; }
        float nx = cal[1].toFloat(&ok);  if (!ok) { return false; }
        float ny = cal[2].toFloat(&ok);  if (!ok) { return false; }

        calibration.insert(v, QPointF(nx, ny));
    }

    // Commit all parsed values
    m_lowColour   = lowColour;
    m_highColour  = highColour;
    m_fontFamily  = fontFamily;
    m_fontSize    = fontSize;
    m_fontBold    = fontBold;
    m_marks       = marks;
    // A saved rating keeps its value (an earlier build saved up to 10 kW);
    // only the below-0.1 recovery applies on load.
    if (std::isfinite(maxPower) && static_cast<double>(maxPower) >= kMinBarPowerLimitW) {
        m_maxPower = maxPower;
    } else {
        setMaxPower(maxPower);
    }
    m_darkMode    = darkMode;
    m_calibration = calibration;

    // Phase 3G-6 block 1: optional trailing filter fields.
    const int filterStart = 16 + calCount;
    if (parts.size() >= filterStart + 3) {
        m_onlyWhenRx   = (parts[filterStart].toInt() != 0);
        m_onlyWhenTx   = (parts[filterStart + 1].toInt() != 0);
        m_displayGroup = parts[filterStart + 2].toInt();
    }

    return true;
}

} // namespace NereusSDR
