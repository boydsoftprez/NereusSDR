#pragma once

// =================================================================
// src/gui/applets/TxChainSection.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs - original licence from Thetis
//   source is included below.
//
// Extracted from src/gui/applets/TxApplet.h: the LEV, EQ and CFC buttons
// and the TX Profile combo.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-20 - A copy of this row in a container is told the outer
//                 margins the applet's column gives the row (outerMargins);
//                 the applet's own row is told none.
//                 Brian Sutton, AI-assisted via OpenCode.
//   2026-10-19 - Extracted from TxApplet.{h,cpp} as part of splitting the
//                 TX applet into per-concern section objects. Behaviour,
//                 appearance, object names and tooltips are unchanged.
//                 Brian Sutton, AI-assisted via OpenAI Codex.
// =================================================================

//=================================================================
// console.cs
//=================================================================
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

// =================================================================
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   Layout pattern from AetherSDR `src/gui/TxApplet.{h,cpp}`.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.

#include <QMargins>
#include <QString>
#include <QWidget>

class QComboBox;
class QPushButton;

namespace NereusSDR {

// The TX chain rows of the TX applet, as their own object: the LEV, EQ and
// CFC buttons, and the TX Profile combo.
//
// Extracted from TxApplet; the object names, the tooltips, the styles and the
// row spacing are unchanged, so the panel looks the same and the tests that
// find these widgets by name still find them.
//
// The section owns its widgets. The applet keeps the wiring: what each
// button means to the model, the profile list, and the dialogs the
// right-click opens.
class TxChainSection : public QWidget
{
    Q_OBJECT
public:
    // rowSpacing is the gap the applet's own column puts between rows, so the
    // rows inside this section keep the same gap they had there.
    explicit TxChainSection(QWidget* parent, int rowSpacing, const QMargins& outerMargins);

    QPushButton* levButton() const { return m_levBtn; }
    QPushButton* eqButton() const { return m_eqBtn; }
    QPushButton* cfcButton() const { return m_cfcBtn; }
    QComboBox*   profileCombo() const { return m_profileCombo; }

private:
    void buildUI(int rowSpacing);
    // The margins this section gives itself.  The applet's own rows
    // give none, because the applet's column already has them; a copy
    // in a container has no column of its own, so it gives itself the
    // margins the column would have given it.
    QMargins m_outerMargins;

    QPushButton* m_levBtn = nullptr;
    QPushButton* m_eqBtn = nullptr;
    QPushButton* m_cfcBtn = nullptr;
    QComboBox*   m_profileCombo = nullptr;
};

} // namespace NereusSDR
