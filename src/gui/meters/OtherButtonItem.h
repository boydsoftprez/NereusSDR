#pragma once

// =================================================================
// src/gui/meters/OtherButtonItem.h  (NereusSDR)
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
//   2026-09-30 - The Power button is removed (maintainer decision): never
//                 drawn, and a saved layout's Power bit is dropped on load.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25 - Thetis button numbers (bitfield group * 32 + bit), 384
//                 slots in 12 groups, every button's visibility and the
//                 button order saved; earlier records load through the old
//                 numbering. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
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

#include "ButtonBoxItem.h"
#include "gui/NereusGuiExport.h"
#include "gui/UnbuiltFeatures.h"

#include <QVector>

#include <optional>

namespace NereusSDR {

// Miscellaneous control buttons + 31 macro slots.
// Ported from Thetis clsOtherButtons (MeterManager.cs:8225+).
//
// Button ids are Thetis's: bitfield group * 32 + bit (MeterManager.cs:8908-8916
// [v2.10.3.15]), 12 groups of 32 slots, bit 31 of each group never used
// (MeterManager.cs:8301-8307 [v2.10.3.15]). The grid has one slot per id;
// slot p draws id buttonMap()[p] (MeterManager.cs:39138-39152, :9398-9401
// [v2.10.3.15]).
class NEREUS_GUI_EXPORT OtherButtonItem : public ButtonBoxItem {
    Q_OBJECT

public:
    // Thetis numbers from ucOtherButtonsOptionsGrid.cs:529-755 [v2.10.3.15];
    // only the ids NereusSDR has an action for are named. The catalog
    // (OtherButtonCatalog) maps the earlier NereusSDR numbers to these.
    // Power intentionally omitted in NereusSDR (maintainer decision
    // 2026-09-30): the id keeps its Thetis index (and its saved visibility
    // bit), but the button is never drawn and a saved one is dropped.
    enum class ButtonId {
        Power = 0, Rx2 = 1, Mon = 2, Tun = 3, Mox = 4, TwoTon = 5, Dup = 6, PsA = 7,
        Xpa = 8, Rec = 9, Play = 10,
        Anf = 37, Snb = 41, Mnf = 42,
        Avg = 96, PeakHold = 97, Ctun = 98,
        Spectrum = 99, Panadapter = 100, Scope = 101, Scope2 = 102, Phase = 103,
        Waterfall = 104, Histogram = 105, Panafall = 106, Panascope = 107,
        Spectrascope = 108, DisplayOff = 109,
        Vac1 = 128, Vac2 = 129, Mute = 130, Bin = 132, SubRx = 133, PanSwap = 134,
        Macro0 = 352
    };

    // From Thetis ucOtherButtonsOptionsGrid.cs:523 [v2.10.3.15] (MAX_BITFIELD_GROUP)
    static constexpr int kGroupCount = 12;
    // From Thetis MeterManager.cs:8301 [v2.10.3.15] (32 * MAX_BITFIELD_GROUP)
    static constexpr int kSlotCount = 32 * kGroupCount;
    // From Thetis ucOtherButtonsOptionsGrid.cs:524 [v2.10.3.15] (MACRO_BUTTONS_PERGROUP)
    static constexpr int kMacroCount = 31;

    enum class MacroType { Off, On, Toggle, Led, ContainerVis, Cat };

    struct MacroSettings {
        MacroType type{MacroType::Toggle};
        QString onText;
        QString offText;
        QString catCommand;
        int containerVisibleId{-1};
    };

    explicit OtherButtonItem(QObject* parent = nullptr);

    void setButtonState(ButtonId id, bool on);
    bool buttonState(ButtonId id) const;

    // R-R3-21: the grid position of a button id, or -1.
    int indexOf(ButtonId id) const;
    // R-R3-21: dimmed, a click changes nothing and says `reason`.
    void setButtonAvailable(ButtonId id, bool available, const QString& reason = QString());
    bool isButtonAvailable(ButtonId id) const;
    // Drawn: saved visible, offered and its feature built.
    bool isButtonShown(ButtonId id) const;

    // The saved visibility of one id (Thetis _visible_bits). Bit 31 of a
    // group is never a button.
    void setButtonVisible(ButtonId id, bool visible);
    bool isButtonVisible(ButtonId id) const;
    // The 12 group bitfields, each non-negative with bit 31 clear.
    QVector<qint32> visibleGroups() const { return m_groups; }
    void setVisibleGroups(const QVector<qint32>& groups);
    // Only this id visible.
    void setOnlyVisible(ButtonId id);

    // The earlier mask: bit n set when the button NereusSDR numbered n
    // (0-31) is visible. Saved in field 8 so an older build loads the same
    // buttons; setting it changes only those buttons.
    void setVisibleBits(uint32_t legacyBits) override;

    // Slot -> id, kSlotCount entries. Setting an empty map, or one that is
    // not an order of every id, restores the identity order.
    QVector<short> buttonMap() const { return m_map; }
    void setButtonMap(const QVector<short>& map);
    void resetButtonMap();

    // True when NereusSDR has an action for this id (a named button other
    // than Power, or a macro). Any other id is never drawn; its saved
    // visibility is kept.
    bool isOffered(ButtonId id) const;

    // R-R3-49: the unbuilt feature a button waits for, or nullopt for a
    // button whose feature NereusSDR has. Such a button is not drawn until
    // the feature is built; its saved visibility is kept.
    static std::optional<UnbuiltFeature> unbuiltFeatureFor(ButtonId id);
    MacroSettings& macroSettings(int macroIndex);

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    QString serialize() const override;
    bool deserialize(const QString& data) override;

signals:
    void otherButtonClicked(int buttonId);
    void macroTriggered(int macroIndex);

private:
    void onButtonClicked(int index, Qt::MouseButton button);

    static bool isValidId(int id);
    // Sets slot `index`'s saved and drawn visibility for the id the map
    // puts there (its caption and colours move with the id).
    void applySlot(int index);
    void applyAllSlots();
    // field 8's mask from the current visibility.
    void updateLegacyBits();
    QString labelFor(int id) const;

    QVector<qint32> m_groups;
    QVector<short> m_map;        // slot -> id
    QVector<int> m_slotOfId;     // id -> slot
    QVector<MacroSettings> m_macroSettings;
};

} // namespace NereusSDR
