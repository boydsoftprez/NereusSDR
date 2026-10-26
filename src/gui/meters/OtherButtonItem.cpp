// =================================================================
// src/gui/meters/OtherButtonItem.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/ucOtherButtonsOptionsGrid.cs (button
//     numbers and table sizes), original licence from Thetis source is
//     included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-24 - R-R3-49 / R-R3-21: VAX 1 / VAX 2 captions, buttons with no
//                 NereusSDR feature hidden through UnbuiltFeatures, lit and
//                 available state per button id. J.J. Boyd (KG4VCF), with
//                 AI-assisted implementation via Anthropic Claude Code.
//   2026-09-27 - A11 / R-R3-49 (parity Task 31): the DUP button is built
//                 (display duplex), no longer hidden with the status bar's
//                 FDX. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-30 - The Power button is removed (maintainer decision): never
//                 drawn, and a saved layout's Power bit is dropped on load.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - Saves and restores the grid styling (border, margin,
//                 corner radius, height ratio, fade on RX/TX) after the
//                 older fields. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-10-25 - Thetis button numbers (bitfield group * 32 + bit), 384
//                 slots in 12 groups, every button's visibility and the
//                 button order saved; earlier records load through the old
//                 numbering. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-10-26 - Reorder: Alt-drag a button onto another to move it there,
//                 with Shift to swap the two; the order is saved. The
//                 dragged button follows the pointer in NereusSDR colours.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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

// --- From ucOtherButtonsOptionsGrid.cs ---

/*  ucOtherButtonsOptionsGrid.cs

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

#include "OtherButtonCatalog.h"
#include "core/LogCategories.h"

#include <QMouseEvent>
#include <QPainter>
#include <QStringList>

#include <algorithm>

namespace NereusSDR {

namespace {

// The record's id scheme (field 15): 2 = Thetis numbers. A record without
// it (or with another value) is read from field 8, the earlier numbering.
constexpr int kThetisIdScheme = 2;
constexpr int kSchemeField = 15;
constexpr int kFirstGroupField = 16;
constexpr int kMapField = kFirstGroupField + OtherButtonItem::kGroupCount;  // 28
// The earlier mask holds the buttons NereusSDR numbered 0-31.
constexpr int kLegacyMaskBits = 32;
// The earlier numbers 32 and 33 (Spectrascope, Off) were never in the mask
// and never hidden by it: they count as visible in an earlier record.
constexpr int kLegacyUnmaskedFirst = 32;
constexpr int kLegacyUnmaskedLast = 33;
// From Thetis MeterManager.cs:8902-8906 [v2.10.3.15]: bit 31 is skipped.
constexpr qint32 kGroupBits = 0x7FFFFFFF;

// From Thetis clsOtherButtons (MeterManager.cs:8225+), by NereusSDR's
// earlier numbers 0-33. Power ("PWR") intentionally omitted in NereusSDR
// (maintainer decision 2026-09-30): its label stays, never drawn.
const char* const kCoreLabels[] = {
    "PWR", "RX2", "MON", "TUN", "MOX", "2TON", "DUP", "PS",
    "PLAY", "REC", "ANF", "SNB", "MNF", "AVG", "PEAK", "CTUN",
    // R-R3-49: VAC1 / VAC2 read VAX 1 / VAX 2, NereusSDR's name for them.
    "VAX 1", "VAX 2", "MUTE", "BIN", "SUB", "SWAP", "XPA",
    "SPEC", "PAN", "SCP", "SCP2", "PHS",
    "WF", "HIST", "PANF", "PANS", "SPCS", "OFF"
};
constexpr int kCoreLabelCount = static_cast<int>(sizeof(kCoreLabels) / sizeof(kCoreLabels[0]));

// NereusSDR-original drag face (Thetis's lime / dark red outline,
// MeterManager.cs:39797-39815 [v2.10.3.15], is not ported): the accent outline over a
// button it can drop on, the border colour elsewhere.
const QColor kDragTargetOutline{0x00, 0xb4, 0xd8};  // STYLEGUIDE accent
const QColor kDragNoTargetOutline{0x20, 0x50, 0x70}; // STYLEGUIDE border
constexpr qreal kDragFaceOpacity = 0.85;
constexpr qreal kDragOutlineWidth = 2.0;

bool isMacroId(int id)
{
    const int macro = id - static_cast<int>(OtherButtonItem::ButtonId::Macro0);
    return macro >= 0 && macro < OtherButtonItem::kMacroCount;
}

} // namespace

OtherButtonItem::OtherButtonItem(QObject* parent)
    : ButtonBoxItem(parent)
{
    setButtonCount(kSlotCount);
    setColumns(6);
    setCornerRadius(3.0f);

    m_groups = QVector<qint32>(kGroupCount, 0);
    m_map.resize(kSlotCount);
    m_slotOfId.resize(kSlotCount);
    for (int i = 0; i < kSlotCount; ++i) {
        m_map[i] = static_cast<short>(i);
        m_slotOfId[i] = i;
    }
    m_macroSettings.resize(kMacroCount);
    for (int i = 0; i < kSlotCount; ++i) {
        setupButton(i, labelFor(i));
        // Macro buttons light blue, the rest green (an earlier NereusSDR
        // choice); the colour moves with its id when the order changes.
        button(i).onColour = isMacroId(i) ? QColor(0x00, 0x70, 0xc0) : QColor(0x00, 0x60, 0x40);
    }

    // A new grid shows what it showed before Thetis numbers: every core
    // button but Power (the earlier all-ones mask, Power dropped), the
    // macro buttons off. Thetis starts with all off (MeterManager.cs:8303-8307).
    for (int old = 1; old <= kLegacyUnmaskedLast; ++old) {
        const int id = static_cast<int>(OtherButtonCatalog::legacyToButtonId(old));
        m_groups[id / 32] |= qint32(1) << (id % 32);
    }
    applyAllSlots();

    connect(this, &ButtonBoxItem::buttonClicked, this, &OtherButtonItem::onButtonClicked);
}

bool OtherButtonItem::isValidId(int id)
{
    // From Thetis MeterManager.cs:8902-8916 [v2.10.3.15]: bit 31 is never a slot.
    // bit 32 is skipped [original inline comment from MeterManager.cs:8905]
    return id >= 0 && id < kSlotCount && (id % 32) != 31;
}

QString OtherButtonItem::labelFor(int id) const
{
    const int old = OtherButtonCatalog::buttonIdToLegacy(static_cast<ButtonId>(id));
    if (old >= 0 && old < kCoreLabelCount) { return QString::fromLatin1(kCoreLabels[old]); }
    if (isMacroId(id)) {
        return QStringLiteral("M%1").arg(id - static_cast<int>(ButtonId::Macro0));
    }
    return OtherButtonCatalog::info(static_cast<ButtonId>(id)).caption;
}

void OtherButtonItem::applySlot(int index)
{
    const int id = m_map[index];
    ButtonState& state = m_buttons[index];
    state.visible = isButtonVisible(static_cast<ButtonId>(id));
    // skip 32nd bit in any group and any that are not defined
    // [original inline comment from MeterManager.cs:8990]
    // An id with no NereusSDR action is never drawn. R-R3-49: a button whose
    // feature NereusSDR does not have is not drawn either (layout time only;
    // the saved visibility is untouched).
    bool hidden = !isOffered(static_cast<ButtonId>(id));
    if (!hidden) {
        if (const auto feature = unbuiltFeatureFor(static_cast<ButtonId>(id))) {
            hidden = !UnbuiltFeatures::isBuilt(*feature);
        }
    }
    state.hiddenUntilBuilt = hidden;
}

void OtherButtonItem::applyAllSlots()
{
    for (int i = 0; i < kSlotCount; ++i) { applySlot(i); }
    invalidateShownLayout();
    updateLegacyBits();
}

void OtherButtonItem::updateLegacyBits()
{
    uint32_t bits = 0;
    for (int old = 0; old < kLegacyMaskBits; ++old) {
        if (isButtonVisible(OtherButtonCatalog::legacyToButtonId(old))) { bits |= 1u << old; }
    }
    setVisibleBitsValue(bits);
}

void OtherButtonItem::setButtonState(ButtonId id, bool on)
{
    const int index = indexOf(id);
    if (index >= 0) { m_buttons[index].on = on; }
}

bool OtherButtonItem::buttonState(ButtonId id) const
{
    const int index = indexOf(id);
    return index >= 0 && button(index).on;
}

int OtherButtonItem::indexOf(ButtonId id) const
{
    const int value = static_cast<int>(id);
    if (value < 0 || value >= m_slotOfId.size()) { return -1; }
    return m_slotOfId[value];
}

void OtherButtonItem::setButtonAvailable(ButtonId id, bool available, const QString& reason)
{
    ButtonBoxItem::setButtonAvailable(indexOf(id), available, reason);
}

bool OtherButtonItem::isButtonAvailable(ButtonId id) const
{
    return ButtonBoxItem::isButtonAvailable(indexOf(id));
}

bool OtherButtonItem::isButtonShown(ButtonId id) const
{
    return ButtonBoxItem::isButtonShown(indexOf(id));
}

void OtherButtonItem::setButtonVisible(ButtonId id, bool visible)
{
    const int value = static_cast<int>(id);
    if (!isValidId(value)) { return; }
    const qint32 bit = qint32(1) << (value % 32);
    qint32& group = m_groups[value / 32];
    group = visible ? (group | bit) : (group & ~bit);
    applySlot(m_slotOfId[value]);
    invalidateShownLayout();
    updateLegacyBits();
}

bool OtherButtonItem::isButtonVisible(ButtonId id) const
{
    const int value = static_cast<int>(id);
    if (!isValidId(value)) { return false; }
    // From Thetis MeterManager.cs:8902-8906 [v2.10.3.15]
    return (m_groups[value / 32] & (qint32(1) << (value % 32))) != 0;
}

void OtherButtonItem::setVisibleGroups(const QVector<qint32>& groups)
{
    for (int g = 0; g < kGroupCount; ++g) {
        // groups of 32 bits, 32nd bit is never used to prevent -ve ints in storage
        // [original inline comment from MeterManager.cs:8303]
        m_groups[g] = g < groups.size() ? (groups[g] & kGroupBits) : 0;
    }
    applyAllSlots();
}

void OtherButtonItem::setOnlyVisible(ButtonId id)
{
    m_groups.fill(0);
    const int value = static_cast<int>(id);
    if (isValidId(value)) { m_groups[value / 32] = qint32(1) << (value % 32); }
    applyAllSlots();
}

void OtherButtonItem::setVisibleBits(uint32_t legacyBits)
{
    for (int old = 0; old < kLegacyMaskBits; ++old) {
        const int id = static_cast<int>(OtherButtonCatalog::legacyToButtonId(old));
        const qint32 bit = qint32(1) << (id % 32);
        qint32& group = m_groups[id / 32];
        group = ((legacyBits >> old) & 1u) ? (group | bit) : (group & ~bit);
    }
    applyAllSlots();
}

void OtherButtonItem::setButtonMap(const QVector<short>& map)
{
    if (map.isEmpty()) {
        resetButtonMap();
        return;
    }
    // Every id once, in some order; anything else keeps the identity order.
    bool valid = map.size() == kSlotCount;
    QVector<bool> seen(kSlotCount, false);
    for (int i = 0; valid && i < map.size(); ++i) {
        const int id = map[i];
        valid = id >= 0 && id < kSlotCount && !seen[id];
        if (valid) { seen[id] = true; }
    }
    if (!valid) {
        qCWarning(lcMeter) << "OtherButtonItem: ignoring a button order that is not one of every button";
        resetButtonMap();
        return;
    }
    // Each button's state (lit, available, colours) moves with its id.
    const QVector<ButtonState> byId = [this] {
        QVector<ButtonState> states(kSlotCount);
        for (int slot = 0; slot < kSlotCount; ++slot) { states[m_map[slot]] = m_buttons[slot]; }
        return states;
    }();
    m_map = map;
    for (int slot = 0; slot < kSlotCount; ++slot) {
        m_slotOfId[m_map[slot]] = slot;
        m_buttons[slot] = byId[m_map[slot]];
    }
    applyAllSlots();
}

void OtherButtonItem::resetButtonMap()
{
    QVector<short> identity(kSlotCount);
    for (int i = 0; i < kSlotCount; ++i) { identity[i] = static_cast<short>(i); }
    if (m_map == identity) { return; }
    setButtonMap(identity);
}

bool OtherButtonItem::isOffered(ButtonId id) const
{
    const int value = static_cast<int>(id);
    if (isMacroId(value)) { return true; }
    // Power is not a NereusSDR button (maintainer decision 2026-09-30).
    if (id == ButtonId::Power) { return false; }
    const int old = OtherButtonCatalog::buttonIdToLegacy(id);
    return old >= 0 && old < kCoreLabelCount;
}

std::optional<UnbuiltFeature> OtherButtonItem::unbuiltFeatureFor(ButtonId id)
{
    if (isMacroId(static_cast<int>(id))) {
        // R-R3-49: the macro buttons are built after R4.
        return UnbuiltFeature::MacroButtons;
    }
    switch (id) {
    // Thetis's two-receiver layout (RX2 on, sub receiver, pan swap) has no
    // place among slices A to D.
    case ButtonId::Rx2:
    case ButtonId::SubRx:
    case ButtonId::PanSwap:
        return UnbuiltFeature::TwoReceiverLayout;
    // AVG is built after R4 with the display work.
    case ButtonId::Avg:
        return UnbuiltFeature::DisplayAveraging;
    case ButtonId::Play:
    case ButtonId::Rec:
        return UnbuiltFeature::Voice;           // the voice keyer
    case ButtonId::Xpa:
        return UnbuiltFeature::OcExtras;        // external PA
    case ButtonId::Spectrum:
    case ButtonId::Panadapter:
    case ButtonId::Scope:
    case ButtonId::Scope2:
    case ButtonId::Phase:
    case ButtonId::Waterfall:
    case ButtonId::Histogram:
    case ButtonId::Panafall:
    case ButtonId::Panascope:
    case ButtonId::Spectrascope:
    case ButtonId::DisplayOff:
        return UnbuiltFeature::DisplayMode;
    default:
        return std::nullopt;
    }
}

OtherButtonItem::MacroSettings& OtherButtonItem::macroSettings(int macroIndex)
{
    return m_macroSettings[macroIndex];
}

void OtherButtonItem::onButtonClicked(int index, Qt::MouseButton btn)
{
    if (btn != Qt::LeftButton) { return; }
    if (index < 0 || index >= m_map.size()) { return; }
    //use map to convert index [original inline comment from MeterManager.cs:9397]
    const int mapped = m_map[index];
    // From Thetis MeterManager.cs:9404-9407 [v2.10.3.15]
    if (!isValidId(mapped)) { return; }
    if (isMacroId(mapped)) {
        emit macroTriggered(mapped - static_cast<int>(ButtonId::Macro0));
    } else {
        emit otherButtonClicked(mapped);
    }
}

void OtherButtonItem::paint(QPainter& p, int widgetW, int widgetH)
{
    ButtonBoxItem::paint(p, widgetW, widgetH);
    if (m_dragIndex < 0 || !m_dragMoved || !ButtonBoxItem::isButtonShown(m_dragIndex)) { return; }
    // The dragged button follows the pointer, centred on it, the way
    // Thetis moves its drag rectangle (MeterManager.cs:39797-39804 [v2.10.3.15]).
    QRectF face = shownButtonRect(m_dragIndex, widgetW, widgetH);
    if (!face.isValid()) { return; }
    face.moveCenter(m_dragPoint);
    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    if (m_dragTarget >= 0 && m_dragTarget != m_dragIndex) {
        const QRectF target = shownButtonRect(m_dragTarget, widgetW, widgetH);
        p.setPen(QPen(kDragTargetOutline, kDragOutlineWidth));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(target, cornerRadius(), cornerRadius());
    }
    p.setOpacity(kDragFaceOpacity);
    paintButton(p, m_dragIndex, face);
    p.setOpacity(1.0);
    p.setPen(QPen(m_dragTarget >= 0 ? kDragTargetOutline : kDragNoTargetOutline, kDragOutlineWidth));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(face, cornerRadius(), cornerRadius());
    p.restore();
}

bool OtherButtonItem::handleMousePress(QMouseEvent* event, int widgetW, int widgetH)
{
    // From Thetis MeterManager.cs:9322-9344 [v2.10.3.15]
    if (inputFaded()) { return false; }

    const int index = buttonAt(event->position(), widgetW, widgetH);
    if (index == -1) { return false; }

    m_dragTarget = -1;
    m_dragMoved = false;
    if (event->modifiers().testFlag(Qt::AltModifier)) {
        m_dragIndex = index;
        m_dragPoint = event->position();
        return true;
    }
    m_dragIndex = -1;

    return ButtonBoxItem::handleMousePress(event, widgetW, widgetH);
}

bool OtherButtonItem::handleMouseMove(QMouseEvent* event, int widgetW, int widgetH)
{
    if (m_dragIndex < 0) { return ButtonBoxItem::handleMouseMove(event, widgetW, widgetH); }
    if (event->buttons() == Qt::NoButton) {
        // The release went elsewhere (outside this grid): the drag ends
        // with nothing moved.
        m_dragIndex = -1;
        m_dragTarget = -1;
        m_dragMoved = false;
        ButtonBoxItem::handleMouseMove(event, widgetW, widgetH);
        return true;
    }
    // From Thetis MeterManager.cs:9546-9550 [v2.10.3.15] (MoveButton): the
    // button under the pointer is the target, -1 when there is none.
    m_dragPoint = event->position();
    m_dragMoved = true;
    m_dragTarget = buttonAt(m_dragPoint, widgetW, widgetH);
    ButtonBoxItem::handleMouseMove(event, widgetW, widgetH);
    return true;
}

bool OtherButtonItem::handleMouseRelease(QMouseEvent* event, int widgetW, int widgetH)
{
    if (m_dragIndex < 0) { return ButtonBoxItem::handleMouseRelease(event, widgetW, widgetH); }

    const int from = m_dragIndex;
    m_dragIndex = -1;
    m_dragMoved = false;
    // From Thetis MeterManager.cs:9348-9349 [v2.10.3.15]: nothing while the
    // fade covers the grid.
    if (inputFaded()) {
        m_dragTarget = -1;
        return true;
    }
    // The button under the pointer at the release (Thetis keeps the last
    // one its paint saw, MeterManager.cs:39817 [v2.10.3.15]).
    const int to = buttonAt(event->position(), widgetW, widgetH);
    m_dragTarget = -1;

    // From Thetis MeterManager.cs:9353-9389 [v2.10.3.15]
    if (to != -1 && from != to) {
        QVector<short> map = m_map;
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            std::swap(map[to], map[from]);
        } else {
            const short moving = map[from];

            if (from < to) {
                std::copy(map.begin() + from + 1, map.begin() + to + 1, map.begin() + from);
                map[to] = moving;
            } else {
                std::copy_backward(map.begin() + to, map.begin() + from, map.begin() + from + 1);
                map[to] = moving;
            }
        }
        //Debug.Print($"{_dragging_index} to {_last_draged_to}");
        // [original inline comment from MeterManager.cs:9381]
        setButtonMap(map);
    }
    return true;
}

QString OtherButtonItem::serialize() const
{
    // Fields 0-14 as before (field 8 the earlier mask), then the id scheme,
    // the 12 group bitfields (Thetis buttonbox_other_buttons_bitfield_0..11)
    // and the map, empty for the identity order (Thetis buttonbox_button_map,
    // MeterManager.cs:28579-28583 [v2.10.3.15]).
    QString out = QStringLiteral("OTHERBTNS|%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(m_x).arg(m_y).arg(m_w).arg(m_h)
        .arg(m_bindingId).arg(m_zOrder)
        .arg(columns()).arg(visibleBits())
        + serializeGridStyle();
    out += QLatin1Char('|') + QString::number(kThetisIdScheme);
    for (const qint32 group : m_groups) { out += QLatin1Char('|') + QString::number(group); }
    out += QLatin1Char('|');
    bool identity = true;
    for (int i = 0; identity && i < m_map.size(); ++i) { identity = m_map[i] == i; }
    if (!identity) {
        QStringList ids;
        ids.reserve(m_map.size());
        for (const short id : m_map) { ids.append(QString::number(id)); }
        out += ids.join(QLatin1Char(','));
    }
    return out;
}

bool OtherButtonItem::deserialize(const QString& data)
{
    const QStringList parts = data.split(QLatin1Char('|'));
    if (parts.size() < 7 || parts[0] != QLatin1String("OTHERBTNS")) { return false; }
    m_x = parts[1].toFloat(); m_y = parts[2].toFloat();
    m_w = parts[3].toFloat(); m_h = parts[4].toFloat();
    m_bindingId = parts[5].toInt(); m_zOrder = parts[6].toInt();
    if (parts.size() > 7) { setColumns(parts[7].toInt()); }

    const bool thetisIds = parts.size() > kSchemeField
        && parts[kSchemeField].toInt() == kThetisIdScheme;
    if (thetisIds) {
        QVector<qint32> groups(kGroupCount, 0);
        for (int g = 0; g < kGroupCount; ++g) {
            const int field = kFirstGroupField + g;
            if (field >= parts.size()) { break; }
            bool ok = false;
            const qint64 value = parts[field].toLongLong(&ok);
            groups[g] = (ok && value >= 0) ? static_cast<qint32>(value & kGroupBits) : 0;
        }
        // A layout saved with a Power button loads without it.
        groups[0] &= ~(qint32(1) << static_cast<int>(ButtonId::Power));
        QVector<short> map;
        if (parts.size() > kMapField && !parts[kMapField].isEmpty()) {
            const QStringList ids = parts[kMapField].split(QLatin1Char(','));
            map.reserve(ids.size());
            for (const QString& id : ids) {
                bool ok = false;
                const int value = id.toInt(&ok);
                map.append(ok ? static_cast<short>(value) : short(-1));
            }
        }
        setButtonMap(map);
        setVisibleGroups(groups);
    } else if (parts.size() > 8) {
        // An earlier record: field 8 holds the buttons NereusSDR numbered
        // 0-31, read through the old numbering; 32 and 33 were never in
        // that mask and were always visible. A layout saved with a Power
        // button loads without it.
        resetButtonMap();
        m_groups.fill(0);
        for (int old = kLegacyUnmaskedFirst; old <= kLegacyUnmaskedLast; ++old) {
            const int id = static_cast<int>(OtherButtonCatalog::legacyToButtonId(old));
            m_groups[id / 32] |= qint32(1) << (id % 32);
        }
        const uint32_t power = 1u << OtherButtonCatalog::buttonIdToLegacy(ButtonId::Power);
        setVisibleBits(parts[8].toUInt() & ~power);
    }
    deserializeGridStyle(parts, 9);
    return true;
}

} // namespace NereusSDR
