// =================================================================
// src/gui/meters/ModeButtonItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs (column range), original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-10-25 - Saves and restores the grid styling (border, margin,
//                 corner radius, height ratio, fade on RX/TX) after the
//                 older fields. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - Column range from Thetis setup.cs (columnRange), which
//                 the editor clamps to. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - Twelve modes in DSPMode order (modeForIndex), as Thetis's
//                 clsModeButtonBox. A record in that order ends with an
//                 order marker; one without it (the earlier ten-button
//                 order) is remapped on load. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
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

// --- From setup.cs ---

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
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

#include "ModeButtonItem.h"

#include <array>

namespace NereusSDR {

// From Thetis MeterManager.cs:10140-10152 [v2.10.3.15] (clsModeButtonBox
// setupButtons): button i shows DSPMode (LSB + i) by its name, LSB to DRM.
static const char* const kModeLabels[] = {
    "LSB", "USB", "DSB", "CWL", "CWU", "FM", "AM", "DIGU", "SPEC", "DIGL", "SAM", "DRM"
};
static_assert(std::size(kModeLabels) == ModeButtonItem::kModeCount);

namespace {
// Grids saved before the DSPMode order had ten buttons: LSB, USB, DSB, CWL,
// CWU, FM, AM, SAM, DIGL, DIGU. A record in DSPMode order ends with this
// marker after the grid style fields; one without it is in the old order.
constexpr int kOrderMarkerField = 16;
constexpr const char* kDspModeOrderMarker = "DSPORDER";
constexpr int kOldModeCount = 10;
// The DSPMode-order button of each old button: SAM 7 -> 10, DIGL 8 -> 9,
// DIGU 9 -> 7; the first seven keep their place.
constexpr std::array<int, kOldModeCount> kOldToNewIndex{0, 1, 2, 3, 4, 5, 6, 10, 9, 7};
constexpr uint32_t kSpecBit = 1u << static_cast<int>(DSPMode::SPEC);
constexpr uint32_t kDrmBit = 1u << static_cast<int>(DSPMode::DRM);

int remapOldIndex(int index)
{
    return index >= 0 && index < kOldModeCount ? kOldToNewIndex[index] : index;
}

// Moves each old button's visible bit to its DSPMode place. SPEC and DRM
// were not on the old grid, so they start hidden; bits past the twelve
// buttons are kept as saved.
uint32_t remapOldBits(uint32_t bits)
{
    uint32_t mapped = bits & ~((1u << ModeButtonItem::kModeCount) - 1u);
    for (int i = 0; i < kOldModeCount; ++i) {
        if ((bits >> i) & 1u) { mapped |= 1u << kOldToNewIndex[i]; }
    }
    return mapped;
}
} // namespace

ModeButtonItem::ModeButtonItem(QObject* parent)
    : ButtonBoxItem(parent)
{
    setButtonCount(kModeCount);
    setColumns(5);
    setCornerRadius(3.0f);

    for (int i = 0; i < kModeCount; ++i) {
        setupButton(i, QString::fromLatin1(kModeLabels[i]));
        button(i).onColour = QColor(0x00, 0x70, 0xc0);
    }

    connect(this, &ButtonBoxItem::buttonClicked,
            this, &ModeButtonItem::onButtonClicked);
}

QString ModeButtonItem::modeLabel(int index)
{
    if (index < 0 || index >= kModeCount) { return QString(); }
    return QString::fromLatin1(kModeLabels[index]);
}

// From Thetis MeterManager.cs:10258 [v2.10.3.15] (clsModeButtonBox.MouseUp):
//   DSPMode m = (DSPMode)((int)DSPMode.LSB + index);
DSPMode ModeButtonItem::modeForIndex(int index)
{
    if (index < 0 || index >= kModeCount) { return DSPMode::LSB; }
    return static_cast<DSPMode>(static_cast<int>(DSPMode::LSB) + index);
}

void ModeButtonItem::setActiveMode(int index)
{
    if (m_activeMode == index) { return; }
    if (m_activeMode >= 0 && m_activeMode < buttonCount()) {
        button(m_activeMode).on = false;
    }
    m_activeMode = index;
    if (m_activeMode >= 0 && m_activeMode < buttonCount()) {
        button(m_activeMode).on = true;
    }
}

void ModeButtonItem::onButtonClicked(int index, Qt::MouseButton btn)
{
    if (btn == Qt::LeftButton) {
        emit modeClicked(index);
    }
}

QString ModeButtonItem::serialize() const
{
    return QStringLiteral("MODEBTNS|%1|%2|%3|%4|%5|%6|%7|%8|%9")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h)
        .arg(m_bindingId).arg(m_zOrder)
        .arg(columns()).arg(m_activeMode).arg(visibleBits())
        + serializeGridStyle()
        + QLatin1Char('|') + QLatin1String(kDspModeOrderMarker);
}

bool ModeButtonItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("MODEBTNS")) { return false; }
    m_x = parts[1].toFloat();
    m_y = parts[2].toFloat();
    m_w = parts[3].toFloat();
    m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt();
    m_zOrder = parts[6].toInt();
    // A record without the order marker is in the old ten-button order:
    // its active button and visible buttons move to their DSPMode places.
    const bool dspModeOrder = parts.size() > kOrderMarkerField
        && parts[kOrderMarkerField] == QLatin1String(kDspModeOrderMarker);
    if (parts.size() > 7) { setColumns(parts[7].toInt()); }
    if (parts.size() > 8) {
        const int saved = parts[8].toInt();
        setActiveMode(dspModeOrder ? saved : remapOldIndex(saved));
    }
    if (parts.size() > 9) {
        const uint32_t saved = parts[9].toUInt();
        setVisibleBits(dspModeOrder ? saved : remapOldBits(saved));
    } else if (!dspModeOrder) {
        setVisibleBits(visibleBits() & ~(kSpecBit | kDrmBit));
    }
    deserializeGridStyle(parts, 10);
    return true;
}

namespace {
// From Thetis setup.cs:25822 [v2.10.3.15]
constexpr int kModeGridMinColumns = 1;
constexpr int kModeGridMaxColumns = 12;
} // namespace

// From Thetis setup.cs:25821-25825 [v2.10.3.15] (MODE_BUTTONS: 1 to 12 columns).
std::pair<int, int> ModeButtonItem::columnRange() const
{
    return {kModeGridMinColumns, kModeGridMaxColumns};
}

} // namespace NereusSDR
