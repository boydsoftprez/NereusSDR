#pragma once

// =================================================================
// src/gui/meters/OverlayTokens.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs (clsTextOverlay parseText,
//   GetPlaceholders, formatNumber, the custom text readings and
//   clsMMIO.VariableValueType), original licence from Thetis source is
//   included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-26 - Text overlay token table: Thetis parseText's precision,
//                 reading, time and date, newline and meter data source
//                 tokens, case-insensitive, with slice-letter tokens in
//                 place of the VFO and receiver ones, and a translator
//                 for Thetis overlay text. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
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

// Text overlay tokens. A token is a name between two percent signs, matched
// without regard to case (%SliceA_Freq% is %slicea_freq%). Text that is not
// a known token stays as it is: a lone %, %%, %1 or an unknown %name%.
//
// Offered tokens (offeredOverlayTokens(), the editor's "%" list):
//   %nl%                 a new line
//   %precis=N%           N decimal places (0 to 20) for every number in the
//                        line; shows nothing. Without it a number shows at
//                        least one and up to six decimals.
//   %value%              the item's own reading (one decimal unless
//                        %precis=N%; "--" with no reading)
//   %group%              the container's shown display group
//   %time_utc%           UTC time, HH:mm:ss
//   %time_loc%           local time, HH:mm:ss
//   %date_utc%           UTC date, ddd d MMM yyyy
//   %date_loc%           local date, ddd d MMM yyyy
//   %time_utc_int%       UTC time as a number, hours*10000 + minutes*100 +
//                        seconds (09:05:07 is 90507)
//   %time_loc_int%       local time as that number
//   %date_utc_int%       UTC date as a number, yyyyMMdd (20261025)
//   %date_loc_int%       local date as that number
//   %mox%                MOX while transmitting, else nothing
//   %tx_slice%           the letter of the slice transmit comes off
//   %sliceX_freq%        slice X's frequency in MHz, as 14.074.000
//   %sliceX_mode%        slice X's mode
//   %sliceX_filter%      slice X's filter width
//   %sliceX_band%        slice X's band
//   %sliceX_step%        slice X's tune step in Hz
//   %sliceX_open%        "Slice X on" while slice X is open, else nothing
//                        (X is A to E; a slice that is not open shows
//                        nothing)
//   %swr%, %pwr%, ...    a reading by its name (thetisReadingName), from the
//                        entry's slice and readings
// Also resolved, not listed: %<ID>.<name>% for a meter data source value
// (the 4-character ID and the value's name), and names the item sets with
// TextOverlayItem::setVariable.
//
// Not carried: the QSO timer, CAT variable and Discord tokens.

#include "gui/NereusGuiExport.h"
#include "gui/meters/VfoDisplayItem.h"

#include <QChar>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace NereusSDR {

// What one line resolves against.
struct OverlayContext {
    // Latest value per MeterBinding id (MeterWidget::lastBindingValues).
    QHash<int, double> readings;
    // One state per open slice; VfoSliceState::index is the slice (0 = A).
    QVector<VfoSliceState> slices;
    // The slice letter transmit comes off; null for none.
    QChar txSlice;
    bool mox{false};
    QDateTime nowUtc;
    QDateTime nowLocal;
    // NereusSDR: the item's own values by lowercase name (%value%, %group%
    // and setVariable names), already as text.
    QHash<QString, QString> named;
};

// The text with every known token replaced. One pass, left to right: a
// token's value is never read again as tokens.
NEREUS_GUI_EXPORT QString resolveOverlayTokens(const QString& text, const OverlayContext& context);

// The decimal places the first %precis=N% in the text asks for (clamped to
// 0..20), or -1 when it has none.
NEREUS_GUI_EXPORT int overlayTokenPrecision(const QString& text);

// Thetis overlay text with its VFO and receiver tokens named by slice:
// %vfoa% becomes %sliceA_freq%, %vfob% %sliceB_freq%, %rx2% %sliceB_open%,
// and the band, mode and filter forms the same way. Everything else stays.
NEREUS_GUI_EXPORT QString translateThetisOverlayText(const QString& text);

// The tokens the editor offers, each with its percent signs.
NEREUS_GUI_EXPORT QStringList offeredOverlayTokens();

} // namespace NereusSDR
