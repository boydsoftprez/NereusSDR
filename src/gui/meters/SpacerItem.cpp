// =================================================================
// src/gui/meters/SpacerItem.cpp  (NereusSDR)
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

#include "SpacerItem.h"

// From Thetis MeterManager.cs:16116 — clsSpacerItem / renderSpacer (line 35010)

#include <QPainter>
#include <QLinearGradient>
#include <QStringList>

namespace NereusSDR {

// From Thetis MeterManager.cs:16121
SpacerItem::SpacerItem(QObject* parent)
    : MeterItem(parent)
{
}

// ---------------------------------------------------------------------------
// paint()
// From Thetis MeterManager.cs:35010 — renderSpacer()
// Thetis renders colour1 (RX) or colour2 (TX) depending on MOX state,
// with a half-second cross-fade when MOX toggles.
// NereusSDR simplifies: if colours differ, show vertical linear gradient
// (colour1 top, colour2 bottom); otherwise solid fill with colour1.
// The padding field is preserved for serialization parity with Thetis
// (Thetis uses it for spacing in automatic layout, not for rendering).
// ---------------------------------------------------------------------------
void SpacerItem::paint(QPainter& p, int widgetW, int widgetH)
{
    const QRect rect = pixelRect(widgetW, widgetH);
    if (m_colour1 == m_colour2) {
        p.fillRect(rect, m_colour1);
    } else {
        QLinearGradient grad(rect.topLeft(), rect.bottomLeft());
        grad.setColorAt(0.0, m_colour1);
        grad.setColorAt(1.0, m_colour2);
        p.fillRect(rect, grad);
    }
}

// ---------------------------------------------------------------------------
// serialize()
// Format: SPACER|x|y|w|h|bindingId|zOrder|padding|colour1|colour2
// ---------------------------------------------------------------------------

// Local helper mirroring the static baseFields() in MeterItem.cpp
static QString spacerBaseFields(const MeterItem& item)
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6")
        .arg(static_cast<double>(item.x()))
        .arg(static_cast<double>(item.y()))
        .arg(static_cast<double>(item.itemWidth()))
        .arg(static_cast<double>(item.itemHeight()))
        .arg(item.bindingId())
        .arg(item.zOrder());
}

// Local helper mirroring the static parseBaseFields() in MeterItem.cpp
static bool spacerParseBaseFields(MeterItem& item, const QStringList& parts)
{
    if (parts.size() < 7) {
        return false;
    }
    const QString base = QStringList(parts.mid(1, 6)).join(QLatin1Char('|'));
    return item.MeterItem::deserialize(base);
}

QString SpacerItem::serialize() const
{
    return QStringLiteral("SPACER|%1|%2|%3|%4")
        .arg(spacerBaseFields(*this))
        .arg(static_cast<double>(m_padding))
        .arg(m_colour1.name(QColor::HexArgb))
        .arg(m_colour2.name(QColor::HexArgb));
}

// ---------------------------------------------------------------------------
// deserialize()
// Expected: SPACER|x|y|w|h|bindingId|zOrder|padding|colour1|colour2
//           [0]    [1-6]             [7]     [8]     [9]
// ---------------------------------------------------------------------------
bool SpacerItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 10 || parts[0] != QLatin1String("SPACER")) {
        return false;
    }
    if (!spacerParseBaseFields(*this, parts)) {
        return false;
    }

    bool ok = true;
    const float padding = parts[7].toFloat(&ok);
    if (!ok) {
        return false;
    }

    const QColor colour1(parts[8]);
    if (!colour1.isValid()) {
        return false;
    }

    const QColor colour2(parts[9]);
    if (!colour2.isValid()) {
        return false;
    }

    m_padding = padding;
    m_colour1 = colour1;
    m_colour2 = colour2;
    return true;
}

} // namespace NereusSDR
