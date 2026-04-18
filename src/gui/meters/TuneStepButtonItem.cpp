// =================================================================
// src/gui/meters/TuneStepButtonItem.cpp  (NereusSDR)
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

#include "TuneStepButtonItem.h"

namespace NereusSDR {

// From Thetis TuneStepList with "Hz" stripped (MeterManager.cs:7999+)
static const char* const kStepLabels[] = {
    "1", "10", "100", "1k", "10k", "100k", "1M"
};

TuneStepButtonItem::TuneStepButtonItem(QObject* parent)
    : ButtonBoxItem(parent)
{
    setButtonCount(kStepCount);
    setColumns(4);
    setCornerRadius(3.0f);

    for (int i = 0; i < kStepCount; ++i) {
        setupButton(i, QString::fromLatin1(kStepLabels[i]));
        button(i).onColour = QColor(0x00, 0x70, 0xc0);
    }

    connect(this, &ButtonBoxItem::buttonClicked, this, &TuneStepButtonItem::onButtonClicked);
}

void TuneStepButtonItem::setActiveStep(int index)
{
    if (m_activeStep == index) { return; }
    if (m_activeStep >= 0 && m_activeStep < buttonCount()) { button(m_activeStep).on = false; }
    m_activeStep = index;
    if (m_activeStep >= 0 && m_activeStep < buttonCount()) { button(m_activeStep).on = true; }
}

void TuneStepButtonItem::onButtonClicked(int index, Qt::MouseButton btn)
{
    if (btn == Qt::LeftButton) { emit tuneStepSelected(index); }
}

QString TuneStepButtonItem::serialize() const
{
    return QStringLiteral("TUNESTEPBTNS|%1|%2|%3|%4|%5|%6|%7|%8|%9")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h)
        .arg(m_bindingId).arg(m_zOrder)
        .arg(columns()).arg(m_activeStep).arg(visibleBits());
}

bool TuneStepButtonItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("TUNESTEPBTNS")) { return false; }
    m_x = parts[1].toFloat(); m_y = parts[2].toFloat();
    m_w = parts[3].toFloat(); m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt(); m_zOrder = parts[6].toInt();
    if (parts.size() > 7) { setColumns(parts[7].toInt()); }
    if (parts.size() > 8) { setActiveStep(parts[8].toInt()); }
    if (parts.size() > 9) { setVisibleBits(parts[9].toUInt()); }
    return true;
}

} // namespace NereusSDR
