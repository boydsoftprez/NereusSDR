// =================================================================
// src/gui/meters/TuneStepButtonItem.cpp  (NereusSDR)
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
//   2026-10-26 - Thetis's 26 tuning steps (kTuneStepList), labels without
//                 "Hz", first ten shown on a new grid; grids saved with the
//                 old seven steps keep their visible and active steps
//                 through an appended marker. J.J. Boyd (KG4VCF),
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

#include "TuneStepButtonItem.h"

#include "models/SliceModel.h"

#include <algorithm>
#include <array>

namespace NereusSDR {

static_assert(TuneStepButtonItem::kStepCount == kTuneStepListSize);

namespace {
// From Thetis MeterManager.cs:8041 [v2.10.3.15]:
//   VisibleBits = (1 << 10) - 1; //1023, 10 bits
constexpr uint32_t kNewGridVisibleBits = (1u << 10) - 1u;

// Grids saved before the 26 steps had seven buttons: 1, 10, 100, 1k, 10k,
// 100k and 1M Hz. A record with the 26 steps ends with this marker after
// the grid style fields; one without it is in the old seven-step order.
constexpr int kStepListMarkerField = 16;
constexpr const char* kStepListMarker = "STEPLIST";
constexpr int kOldStepCount = 7;
constexpr std::array<int, kOldStepCount> kOldStepHz{1, 10, 100, 1000, 10000, 100000, 1000000};
// The kTuneStepList button of each old button: 0, 2, 5, 8, 14, 21, 24.
constexpr std::array<int, kOldStepCount> kOldToNewIndex{
    tuneStepIndexForHz(kOldStepHz[0]), tuneStepIndexForHz(kOldStepHz[1]),
    tuneStepIndexForHz(kOldStepHz[2]), tuneStepIndexForHz(kOldStepHz[3]),
    tuneStepIndexForHz(kOldStepHz[4]), tuneStepIndexForHz(kOldStepHz[5]),
    tuneStepIndexForHz(kOldStepHz[6])};
static_assert(kOldToNewIndex[0] == 0 && kOldToNewIndex[1] == 2 && kOldToNewIndex[2] == 5
              && kOldToNewIndex[3] == 8 && kOldToNewIndex[4] == 14 && kOldToNewIndex[5] == 21
              && kOldToNewIndex[6] == 24);

// An old active step that was not one of the seven buttons lights nothing.
int remapOldIndex(int index)
{
    return index >= 0 && index < kOldStepCount ? kOldToNewIndex[static_cast<size_t>(index)] : -1;
}

// Moves each old button's visible bit to its kTuneStepList place. The steps
// the old grid did not have start hidden; bits past the 26 buttons are kept
// as saved.
uint32_t remapOldBits(uint32_t bits)
{
    uint32_t mapped = bits & ~((1u << TuneStepButtonItem::kStepCount) - 1u);
    for (int i = 0; i < kOldStepCount; ++i) {
        if ((bits >> i) & 1u) { mapped |= 1u << kOldToNewIndex[static_cast<size_t>(i)]; }
    }
    return mapped;
}
} // namespace

TuneStepButtonItem::TuneStepButtonItem(QObject* parent)
    : ButtonBoxItem(parent)
{
    // From Thetis MeterManager.cs:8040 [v2.10.3.15]:
    //   base.Buttons = _console.TuneStepList.Count;
    setButtonCount(kStepCount);
    setColumns(4);
    setCornerRadius(3.0f);

    for (int i = 0; i < kStepCount; ++i) {
        setupButton(i, stepLabel(i));
        button(i).onColour = QColor(0x00, 0x70, 0xc0);
    }
    setVisibleBits(kNewGridVisibleBits);

    connect(this, &ButtonBoxItem::buttonClicked, this, &TuneStepButtonItem::onButtonClicked);
}

int TuneStepButtonItem::stepHz(int index)
{
    if (index < 0 || index >= kStepCount) { return 0; }
    return kTuneStepList[index].stepHz;
}

// From Thetis MeterManager.cs:8066 [v2.10.3.15] (clsTunestepButtons.setupButtons):
//   SetText(1, i, _console.TuneStepList[i].Name.Replace("Hz", ""));//GetText(0, i));
QString TuneStepButtonItem::stepLabel(int index)
{
    if (index < 0 || index >= kStepCount) { return QString(); }
    return QString::fromLatin1(kTuneStepList[index].name).remove(QStringLiteral("Hz"));
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
        .arg(columns()).arg(m_activeStep).arg(visibleBits())
        + serializeGridStyle()
        + QLatin1Char('|') + QLatin1String(kStepListMarker);
}

bool TuneStepButtonItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("TUNESTEPBTNS")) { return false; }
    m_x = parts[1].toFloat(); m_y = parts[2].toFloat();
    m_w = parts[3].toFloat(); m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt(); m_zOrder = parts[6].toInt();
    // A record without the marker has the old seven steps: its active step
    // and visible steps move to their kTuneStepList places.
    const bool stepList = parts.size() > kStepListMarkerField
        && parts[kStepListMarkerField] == QLatin1String(kStepListMarker);
    if (parts.size() > 7) { setColumns(parts[7].toInt()); }
    if (parts.size() > 8) {
        const int saved = parts[8].toInt();
        setActiveStep(stepList ? saved : remapOldIndex(saved));
    }
    if (parts.size() > 9) {
        const uint32_t saved = parts[9].toUInt();
        setVisibleBits(stepList ? saved : remapOldBits(saved));
    } else if (!stepList) {
        // The old grid showed all seven steps.
        setVisibleBits(remapOldBits((1u << kOldStepCount) - 1u));
    }
    deserializeGridStyle(parts, 10);
    return true;
}

namespace {
// From Thetis setup.cs:25841-25843 [v2.10.3.15]: Math.Max(1, max_buttons)
constexpr int kTuneStepGridMinColumns = 1;
} // namespace

// From Thetis setup.cs:25841-25843 [v2.10.3.15] (TUNESTEP_BUTTONS): up to
// one column per checked step, at least one.
std::pair<int, int> TuneStepButtonItem::columnRange() const
{
    return {kTuneStepGridMinColumns, std::max(kTuneStepGridMinColumns, checkedButtonCount())};
}

} // namespace NereusSDR
