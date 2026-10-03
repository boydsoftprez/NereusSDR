// =================================================================
// src/gui/LevelCalGridFollowGuard.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis [v2.10.3.15]:
//   Project Files/Source/Console/console.cs
//     CalibrateLevel (9872-9876, 10230-10231): the grid's minimum stops
//     following the noise floor while the level calibration runs
//
// Original licence from the Thetis source file is included below,
// verbatim, with // --- From [filename] --- marker per
// CLAUDE.md "Byte-for-byte headers and multi-file attribution".
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29 - Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Level Cal: the run lives on the Core, so the
//                window that shows the grid saves, turns off and restores
//                its noise floor follow as the Core's run starts and ends.
//   2026-09-30 - Level Cal fix wave: while it holds the follow off, the
//                saved value stays the user's (the hold callback), so a
//                quit or a crash mid-run never leaves the follow off.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

// --- From console.cs ---
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
// ApacheLabs G2E support added throughout Thetis in various files, all changes marked  //N1GP G2E added
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
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Final modifictions by MW0LGE Richard Samphire - 19th April 2026
// Nothing further added by him after this date, and his repo is now in archive https://github.com/ramdor/Thetis
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#pragma once

#include <QMetaObject>
#include <QObject>

#include <functional>
#include <optional>

namespace NereusSDR {

class RadioModel;

// Watches RadioModel's level calibration state. When a run starts it saves
// whether the grid's minimum follows the noise floor and turns that off;
// when the run ends (or the session drops it) it puts the saved value back.
// MainWindow owns one and points it at the spectrum's setting.
class LevelCalGridFollowGuard : public QObject {
    Q_OBJECT
public:
    explicit LevelCalGridFollowGuard(RadioModel* model, QObject* parent = nullptr);

    // How the guard reads and writes the grid's noise floor follow, and
    // (optional) how it keeps the saved setting at the user's value while
    // it holds the follow off: hold(the user's value) as the run starts,
    // hold(std::nullopt) once the value is put back.
    void setAccess(std::function<bool()> read, std::function<void(bool)> write,
                   std::function<void(std::optional<bool>)> hold = {});

private:
    void onStateChanged();

    RadioModel* m_model = nullptr;
    std::function<bool()> m_read;
    std::function<void(bool)> m_write;
    std::function<void(std::optional<bool>)> m_hold;
    bool m_holding = false;
    bool m_saved = false;
};

} // namespace NereusSDR
