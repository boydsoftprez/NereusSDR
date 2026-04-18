// =================================================================
// src/gui/meters/OtherButtonItem.cpp  (NereusSDR)
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

#include "OtherButtonItem.h"

namespace NereusSDR {

// From Thetis clsOtherButtons (MeterManager.cs:8225+)
static const char* const kCoreLabels[] = {
    "PWR", "RX2", "MON", "TUN", "MOX", "2TON", "DUP", "PS",
    "PLAY", "REC", "ANF", "SNB", "MNF", "AVG", "PEAK", "CTUN",
    "VAC1", "VAC2", "MUTE", "BIN", "SUB", "SWAP", "XPA",
    "SPEC", "PAN", "SCP", "SCP2", "PHS",
    "WF", "HIST", "PANF", "PANS", "SPCS", "OFF"
};

OtherButtonItem::OtherButtonItem(QObject* parent)
    : ButtonBoxItem(parent)
{
    const int total = kCoreButtonCount + kMacroCount;
    setButtonCount(total);
    setColumns(6);
    setCornerRadius(3.0f);

    m_buttonMap.resize(total);
    for (int i = 0; i < total; ++i) { m_buttonMap[i] = i; }

    for (int i = 0; i < kCoreButtonCount; ++i) {
        setupButton(i, QString::fromLatin1(kCoreLabels[i]));
        button(i).onColour = QColor(0x00, 0x60, 0x40);
    }

    m_macroSettings.resize(kMacroCount);
    for (int i = 0; i < kMacroCount; ++i) {
        const int idx = kCoreButtonCount + i;
        setupButton(idx, QStringLiteral("M%1").arg(i));
        button(idx).visible = false;
        button(idx).onColour = QColor(0x00, 0x70, 0xc0);
    }

    connect(this, &ButtonBoxItem::buttonClicked, this, &OtherButtonItem::onButtonClicked);
}

void OtherButtonItem::setButtonState(ButtonId id, bool on)
{
    const int idVal = static_cast<int>(id);
    for (int i = 0; i < m_buttonMap.size(); ++i) {
        if (m_buttonMap[i] == idVal && i < buttonCount()) {
            button(i).on = on;
            return;
        }
    }
}

OtherButtonItem::MacroSettings& OtherButtonItem::macroSettings(int macroIndex)
{
    return m_macroSettings[macroIndex];
}

void OtherButtonItem::onButtonClicked(int index, Qt::MouseButton btn)
{
    if (btn != Qt::LeftButton) { return; }
    const int mapped = (index < m_buttonMap.size()) ? m_buttonMap[index] : index;
    if (mapped >= kCoreButtonCount) {
        const int macroIdx = mapped - kCoreButtonCount;
        if (macroIdx >= 0 && macroIdx < kMacroCount) { emit macroTriggered(macroIdx); }
    } else {
        emit otherButtonClicked(mapped);
    }
}

QString OtherButtonItem::serialize() const
{
    return QStringLiteral("OTHERBTNS|%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h)
        .arg(m_bindingId).arg(m_zOrder)
        .arg(columns()).arg(visibleBits());
}

bool OtherButtonItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("OTHERBTNS")) { return false; }
    m_x = parts[1].toFloat(); m_y = parts[2].toFloat();
    m_w = parts[3].toFloat(); m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt(); m_zOrder = parts[6].toInt();
    if (parts.size() > 7) { setColumns(parts[7].toInt()); }
    if (parts.size() > 8) { setVisibleBits(parts[8].toUInt()); }
    return true;
}

} // namespace NereusSDR
