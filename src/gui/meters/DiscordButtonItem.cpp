// =================================================================
// src/gui/meters/DiscordButtonItem.cpp  (NereusSDR)
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

#include "DiscordButtonItem.h"
namespace NereusSDR {
static const char* const kDiscordLabels[] = {
    "Active", "Away", "Double", "CQ", "QRV", "QRT",
    "QSY", "QRX", "DX", "Version", "Net Start", "Net Fin"
};

DiscordButtonItem::DiscordButtonItem(QObject* parent) : ButtonBoxItem(parent)
{
    setButtonCount(kDiscordButtonCount);
    setColumns(4);
    setCornerRadius(3.0f);
    for (int i = 0; i < kDiscordButtonCount; ++i) {
        setupButton(i, QString::fromLatin1(kDiscordLabels[i]));
        button(i).onColour = QColor(0x58, 0x65, 0xf2); // Discord blurple
    }
    connect(this, &ButtonBoxItem::buttonClicked, this, &DiscordButtonItem::onButtonClicked);
}

void DiscordButtonItem::onButtonClicked(int index, Qt::MouseButton btn)
{ if (btn == Qt::LeftButton) { emit discordAction(index); } }

QString DiscordButtonItem::serialize() const
{
    return QStringLiteral("DISCORDBTNS|%1|%2|%3|%4|%5|%6|%7")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h).arg(m_bindingId).arg(m_zOrder).arg(columns());
}

bool DiscordButtonItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("DISCORDBTNS")) { return false; }
    m_x = parts[1].toFloat(); m_y = parts[2].toFloat();
    m_w = parts[3].toFloat(); m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt(); m_zOrder = parts[6].toInt();
    if (parts.size() > 7) { setColumns(parts[7].toInt()); }
    return true;
}
} // namespace NereusSDR
