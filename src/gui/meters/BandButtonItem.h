#pragma once

// =================================================================
// src/gui/meters/BandButtonItem.h  (NereusSDR)
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
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-26 - Thetis band groups (GEN, HF, VHF) from BandGroups.h, a
//                group per slice, and visibility bits kept by meaning.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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

#include "BandGroups.h"
#include "ButtonBoxItem.h"

#include <QHash>

namespace NereusSDR {

// Band selection grid, ported from Thetis clsBandButtonBox
// (MeterManager.cs:12814+ [v2.10.3.15]).
//
// The 15 buttons show one band group at a time (BandGroups.h): GEN, HF or
// VHF, each with jump buttons to the others. Each slice keeps its own
// group. A slice's group follows its band when the band moves to another
// group (Thetis BandChanged, MeterManager.cs:12876-12890), and a jump
// button changes it. The group is not saved, as in Thetis, where it comes
// from the band (Initialise, MeterManager.cs:12822-12843).
//
// bandClicked and bandStackRequested carry the band's number
// (static_cast<int>(Band)). For 160m to XVTR it equals the old button index
// and the per-band state slot.
//
// Visibility bits (visibleBits) are kept by meaning, never by position:
// bit 0-14 is the per-band state slot (160m .. XVTR, 2m) as in every grid
// saved before the groups, and bits 15 and up were added after them (the SWL
// bands, then the three jumps). A record saved with the groups ends with a
// layout marker (kLayoutMarker). One saved without it had only bits 0-14:
// the added buttons are shown, and the record is saved again as it was
// (no marker) until its visibility is set.
class BandButtonItem : public ButtonBoxItem {
    Q_OBJECT

public:
    explicit BandButtonItem(QObject* parent = nullptr);

    // The band lit on the grid, as its per-band state slot (Band.h,
    // uiIndexFromBand), or -1 for none. It is the band of the slice the grid
    // shows (setSliceBand), on that slice's group.
    void setActiveBand(int index);
    int activeBand() const { return m_activeBand; }

    // The slice the grid shows and its band: shows the slice's group, which
    // moves to the band's group when the band moves to another group.
    void setSliceBand(int sliceIndex, Band band);
    // No slice: no band is lit; the group stays.
    void clearSliceBand();
    int shownSlice() const { return m_shownSlice; }

    // Each slice's group. A slice the grid has not seen shows the group of
    // its band once it is set, and HF until then. -1 is the grid with no
    // slice yet (a new grid, a preview).
    void setGroup(int sliceIndex, BandGroup group);
    BandGroup group(int sliceIndex) const;
    BandGroup shownGroup() const { return group(m_shownSlice); }
    // What button `index` holds in the shown group.
    BandGroupSlot slotAt(int index) const;
    // Where `band` is in the shown group, or -1.
    int buttonForBand(Band band) const;

    // A band's own reason it cannot be chosen (empty: it can), kept with
    // the band through group changes. A grid that is unavailable as a whole
    // keeps its reason on every button.
    void setBandUnavailableReason(Band band, const QString& reason);

    // The visibility bit of a band or jump, -1 for a blank.
    static int visibleBitFor(const BandGroupSlot& slot);
    void setVisibleBits(uint32_t bits) override;

    Layer renderLayer() const override { return Layer::OverlayDynamic; }
    QString serialize() const override;
    bool deserialize(const QString& data) override;

    // The column counts this grid offers (Thetis setup.cs).
    std::pair<int, int> columnRange() const override;

    // The marker that ends a record saved with the band groups.
    static constexpr const char* kLayoutMarker = "BG1";

signals:
    // The band's number (static_cast<int>(Band)).
    void bandClicked(int band);
    // From Thetis PopupBandstack (MeterManager.cs:13210-13218)
    void bandStackRequested(int band);

protected:
    // Reads the visibility at parts[bitsField] and the layout marker at
    // parts[markerField] (a record without the marker had bits 0-14 only).
    void deserializeVisibility(const QStringList& parts, int bitsField, int markerField);
    // "|" + kLayoutMarker, or nothing for a record loaded without it.
    QString layoutMarkerField() const;

private:
    void onButtonClicked(int index, Qt::MouseButton button);
    void rebuildButtons();
    void applyBandAvailability();

    int m_activeBand{-1};
    int m_shownSlice{-1};
    bool m_hasLitBand{false};
    // Loaded from a record without the layout marker: bits 15 and up are
    // not in it, so their buttons are shown.
    bool m_legacyVisibility{false};
    Band m_litBand{Band::GEN};
    QHash<int, BandGroup> m_groups;
    QHash<int, Band> m_lastBands;
    QHash<int, QString> m_bandReasons;      // by band number
    QHash<int, QString> m_gatedButtons;     // button -> the band reason it shows
};

} // namespace NereusSDR
