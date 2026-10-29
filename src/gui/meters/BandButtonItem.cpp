// =================================================================
// src/gui/meters/BandButtonItem.cpp  (NereusSDR)
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
//   2026-10-26 - Screenshot fix: a blank in a band group is a disabled
//                 placeholder whose click says why, never a blank button
//                 that takes clicks. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-28 - 2 m button at index 14 (R-IOS-26, R-R3-49). J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-10-25 - Saves and restores the grid styling (border, margin,
//                 corner radius, height ratio, fade on RX/TX) after the
//                 older fields. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - Column range from Thetis setup.cs (columnRange), which
//                 the editor clamps to. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-26 - The Thetis band groups (BandGroups.h): a group per slice,
//                 jump buttons, clicks by band number, visibility bits kept
//                 by meaning with a layout marker. J.J. Boyd (KG4VCF),
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

#include "BandButtonItem.h"
#include "gui/UnbuiltFeatures.h"

namespace NereusSDR {
namespace {

// Visibility bits after the per-band state slots (Band.h, 0-14): the SWL
// bands, then the jumps. Appended, never renumbered.
constexpr int kFirstSwlBit = kPerBandStateCount;
constexpr int kJumpGenBit = kFirstSwlBit + kSwlBandCount;
constexpr int kJumpHfBit = kJumpGenBit + 1;
constexpr int kJumpVhfBit = kJumpGenBit + 2;

QString labelFor(const BandGroupSlot& slot)
{
    switch (slot.kind) {
    case BandGroupSlot::Band: return bandLabel(slot.band);
    // From Thetis MeterManager.cs:12972, :13014 [v2.10.3.15]
    case BandGroupSlot::JumpHf: return QStringLiteral("HF");
    // From Thetis MeterManager.cs:12991 [v2.10.3.15]
    case BandGroupSlot::JumpVhf: return QStringLiteral("VHF");
    // From Thetis MeterManager.cs:13003 [v2.10.3.15]
    case BandGroupSlot::JumpGen: return QStringLiteral("SWL");
    case BandGroupSlot::Blank: break;
    }
    return QString();
}

} // namespace

BandButtonItem::BandButtonItem(QObject* parent)
    : ButtonBoxItem(parent)
{
    setButtonCount(kBandGroupButtons);
    setColumns(4);
    setCornerRadius(3.0f);

    for (int i = 0; i < kBandGroupButtons; ++i) {
        button(i).onColour = QColor(0x00, 0x70, 0xc0);
    }
    rebuildButtons();

    connect(this, &ButtonBoxItem::buttonClicked,
            this, &BandButtonItem::onButtonClicked);
}

int BandButtonItem::visibleBitFor(const BandGroupSlot& slot)
{
    switch (slot.kind) {
    case BandGroupSlot::Band: {
        const int stateSlot = perBandStateSlot(slot.band);
        if (stateSlot >= 0) { return stateSlot; }
        const int swl = static_cast<int>(slot.band) - static_cast<int>(Band::SwlFirst);
        return swl >= 0 && swl < kSwlBandCount ? kFirstSwlBit + swl : -1;
    }
    case BandGroupSlot::JumpGen: return kJumpGenBit;
    case BandGroupSlot::JumpHf: return kJumpHfBit;
    case BandGroupSlot::JumpVhf: return kJumpVhfBit;
    case BandGroupSlot::Blank: break;
    }
    return -1;
}

void BandButtonItem::setVisibleBits(uint32_t bits)
{
    // Kept by meaning: the buttons read their bit as the group lays them out.
    // Set now, every bit has its meaning (the record gets the marker).
    m_legacyVisibility = false;
    setVisibleBitsValue(bits);
    rebuildButtons();
}

void BandButtonItem::setActiveBand(int index)
{
    if (index < 0) {
        clearSliceBand();
        return;
    }
    setSliceBand(m_shownSlice, bandFromUiIndex(index));
}

void BandButtonItem::setSliceBand(int sliceIndex, Band band)
{
    m_shownSlice = sliceIndex;
    // From Thetis MeterManager.cs:12876-12890 [v2.10.3.15] (BandChanged): the
    // group moves only when the band moves to another group, so a group the
    // user jumped to stays while the band stays in its own group.
    const BandGroup bandGroup = bandGroupForBand(band);
    const auto last = m_lastBands.constFind(sliceIndex);
    if (last == m_lastBands.cend()) {
        // A slice seen for the first time shows its band's group
        // (Initialise, MeterManager.cs:12836), unless one was chosen for it.
        if (!m_groups.contains(sliceIndex)) { m_groups.insert(sliceIndex, bandGroup); }
    } else if (bandGroupForBand(*last) != bandGroup) {
        m_groups.insert(sliceIndex, bandGroup);
    }
    // just set the band
    // [original inline comment from MeterManager.cs:12892]
    m_lastBands.insert(sliceIndex, band);
    m_hasLitBand = true;
    m_litBand = band;
    m_activeBand = uiIndexFromBand(band);
    rebuildButtons();
}

void BandButtonItem::clearSliceBand()
{
    m_hasLitBand = false;
    m_activeBand = -1;
    rebuildButtons();
}

void BandButtonItem::setGroup(int sliceIndex, BandGroup group)
{
    m_groups.insert(sliceIndex, group);
    if (sliceIndex == m_shownSlice) { rebuildButtons(); }
}

BandGroup BandButtonItem::group(int sliceIndex) const
{
    return m_groups.value(sliceIndex, BandGroup::Hf);
}

BandGroupSlot BandButtonItem::slotAt(int index) const
{
    const QVector<BandGroupSlot>& layout = bandGroupLayout(shownGroup());
    if (index < 0 || index >= layout.size()) { return {Band::GEN, BandGroupSlot::Blank}; }
    return layout[index];
}

int BandButtonItem::buttonForBand(Band band) const
{
    return bandGroupPosition(shownGroup(), band);
}

void BandButtonItem::setBandUnavailableReason(Band band, const QString& reason)
{
    if (reason.isEmpty()) {
        m_bandReasons.remove(static_cast<int>(band));
    } else {
        m_bandReasons.insert(static_cast<int>(band), reason);
    }
    applyBandAvailability();
}

// From Thetis MeterManager.cs:12930-13066 [v2.10.3.15] (setupButtons): each
// button takes the group's band or jump. NereusSDR keeps one set of button
// colours (no per-group text colours), so only the text, the lit band and
// the visibility change with the group.
void BandButtonItem::rebuildButtons()
{
    const QVector<BandGroupSlot>& layout = bandGroupLayout(shownGroup());
    // R-R3-49: XVTR goes with the transverters; not drawn until they are
    // built (the saved visibility is untouched).
    const bool transverters = UnbuiltFeatures::isBuilt(UnbuiltFeature::Transverters);
    const auto savedVisible = [this](const BandGroupSlot& slot) {
        const int bit = visibleBitFor(slot);
        if (bit < 0) { return false; }
        // A record saved before the groups has no bit for a button added
        // with them: shown.
        if (m_legacyVisibility && bit >= kFirstSwlBit) { return true; }
        return ((visibleBits() >> bit) & 1u) != 0;
    };
    // A blank holds its place in the Thetis layout. Once a button of the
    // group is hidden the shown buttons close up and the places are gone,
    // so the blanks go too.
    bool wholeGroup = true;
    for (const BandGroupSlot& slot : layout) {
        if (slot.kind != BandGroupSlot::Blank && !savedVisible(slot)) { wholeGroup = false; }
    }
    for (int i = 0; i < buttonCount() && i < layout.size(); ++i) {
        const BandGroupSlot& slot = layout[i];
        ButtonState& face = button(i);
        face.text = labelFor(slot);
        face.visible = slot.kind == BandGroupSlot::Blank ? wholeGroup : savedVisible(slot);
        // From Thetis MeterManager.cs:13064 [v2.10.3.15]: SetOn(1, i, b == _band)
        // for the group's bands. WWV and the jumps are never lit
        // (MeterManager.cs:12997-13008, and :12913-12914 if (newBand != Band.WWV)).
        face.on = slot.kind == BandGroupSlot::Band && slot.band != Band::WWV
            && m_hasLitBand && slot.band == m_litBand;
        face.hiddenUntilBuilt = slot.kind == BandGroupSlot::Band && slot.band == Band::XVTR
            && !transverters;
        // NereusSDR: a blank only keeps its place; it is drawn disabled and
        // a click says why (Thetis returns from the click, :13192).
        face.placeholderReason = slot.kind == BandGroupSlot::Blank ? QString::fromLatin1(kBlankButtonReason)
                                                                   : QString();
    }
    invalidateShownLayout();
    applyBandAvailability();
}

void BandButtonItem::applyBandAvailability()
{
    // A button a band reason gated is free again, unless something else
    // has made it unavailable since.
    for (auto it = m_gatedButtons.cbegin(); it != m_gatedButtons.cend(); ++it) {
        const int index = it.key();
        if (index < buttonCount() && !isButtonAvailable(index)
            && buttonUnavailableReason(index) == it.value()) {
            setButtonAvailable(index, true);
        }
    }
    m_gatedButtons.clear();
    // A grid that is unavailable as a whole (no slice, a preview) keeps its
    // own reason on every button.
    for (int i = 0; i < buttonCount(); ++i) {
        // A blank (placeholder) is never available; it says nothing about
        // the grid.
        if (!button(i).enabled) { return; }
    }
    const QVector<BandGroupSlot>& layout = bandGroupLayout(shownGroup());
    for (int i = 0; i < buttonCount() && i < layout.size(); ++i) {
        if (layout[i].kind != BandGroupSlot::Band) { continue; }
        const auto reason = m_bandReasons.constFind(static_cast<int>(layout[i].band));
        if (reason == m_bandReasons.cend()) { continue; }
        setButtonAvailable(i, false, *reason);
        m_gatedButtons.insert(i, *reason);
    }
}

// From Thetis MeterManager.cs:13146-13218 [v2.10.3.15] (MouseUp). A button
// that is unavailable never gets here (ButtonBoxItem reports its reason):
// that is how a grid without its slice changes nothing.
//   if (_owningmeter.RX == 2 && !_owningmeter.RX2Enabled) return; // prevent any change when rx2 is disabled, but this container is for rx2
//   [original inline comment from MeterManager.cs:13154]
// Thetis also sets the console's band panel on a jump (SetBandPanel);
// NereusSDR has no band panels.
void BandButtonItem::onButtonClicked(int index, Qt::MouseButton btn)
{
    const BandGroupSlot slot = slotAt(index);
    switch (slot.kind) {
    case BandGroupSlot::JumpHf:
        // HF button
        // [original inline comment from MeterManager.cs:13165, :13198]
        setGroup(m_shownSlice, BandGroup::Hf);
        return;
    case BandGroupSlot::JumpVhf:
        //VHF
        // [original inline comment from MeterManager.cs:13179]
        setGroup(m_shownSlice, BandGroup::Vhf);
        return;
    case BandGroupSlot::JumpGen:
        //SWL
        // [original inline comment from MeterManager.cs:13187]
        setGroup(m_shownSlice, BandGroup::Gen);
        return;
    case BandGroupSlot::Blank:
        // From Thetis MeterManager.cs:13192 [v2.10.3.15]: if (index > 10) return;
        return;
    case BandGroupSlot::Band:
        break;
    }
    if (slot.band == Band::WWV) {
        //WVV
        // dont need to set band panel, as will happen due to band change
        // [original inline comments from MeterManager.cs:13183-13185]
        // From Thetis MeterManager.cs:13183-13186 [v2.10.3.15]: WWV sets the
        // band for either mouse button; it has no band stack.
        emit bandClicked(static_cast<int>(slot.band));
        return;
    }
    // From Thetis MeterManager.cs:13210-13218 [v2.10.3.15]: a right-click
    // opens the band's stack (PopupBandstack).
    if (btn == Qt::RightButton) {
        emit bandStackRequested(static_cast<int>(slot.band));
    } else {
        emit bandClicked(static_cast<int>(slot.band));
    }
}

QString BandButtonItem::serialize() const
{
    return QStringLiteral("BANDBTNS|%1|%2|%3|%4|%5|%6|%7|%8|%9")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h)
        .arg(m_bindingId).arg(m_zOrder)
        .arg(columns()).arg(m_activeBand).arg(visibleBits())
        + serializeGridStyle()
        + layoutMarkerField();
}

QString BandButtonItem::layoutMarkerField() const
{
    // A record loaded without the marker is saved again as it was.
    return m_legacyVisibility ? QString() : QLatin1Char('|') + QLatin1String(kLayoutMarker);
}

bool BandButtonItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("BANDBTNS")) { return false; }
    m_x = parts[1].toFloat();
    m_y = parts[2].toFloat();
    m_w = parts[3].toFloat();
    m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt();
    m_zOrder = parts[6].toInt();
    if (parts.size() > 7) { setColumns(parts[7].toInt()); }
    if (parts.size() > 8) { setActiveBand(parts[8].toInt()); }
    deserializeVisibility(parts, 9, 16);
    deserializeGridStyle(parts, 10);
    return true;
}

void BandButtonItem::deserializeVisibility(const QStringList& parts, int bitsField, int markerField)
{
    uint32_t bits = visibleBits();
    if (parts.size() > bitsField) { bits = parts[bitsField].toUInt(); }
    // A record without the layout marker was saved before the groups: its
    // bits 0-14 keep their meaning, and the buttons added since are shown.
    const bool grouped = parts.size() > markerField
        && parts[markerField] == QLatin1String(kLayoutMarker);
    setVisibleBitsValue(bits);
    m_legacyVisibility = !grouped;
    rebuildButtons();
}

namespace {
// From Thetis setup.cs:25817 [v2.10.3.15]
constexpr int kBandGridMinColumns = 1;
constexpr int kBandGridMaxColumns = 15;
} // namespace

// From Thetis setup.cs:25816-25820 [v2.10.3.15] (BAND_BUTTONS: 1 to 15 columns). NereusBandButtonItem, a band grid
// of its own, takes the same range.
std::pair<int, int> BandButtonItem::columnRange() const
{
    return {kBandGridMinColumns, kBandGridMaxColumns};
}

} // namespace NereusSDR
