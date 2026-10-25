// =================================================================
// src/gui/setup/GeneralFiltersPage.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.designer.cs (upstream has no top-of-file header — project-level LICENSE applies)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-10-24 - Setup > General > Filters with Thetis's "Limit filter
//                edges to sidebands" checkbox (setup.designer.cs:12426-12437,
//                setup.cs:22378-22381 [v2.10.3.15]), the Core's
//                LimitFiltersToSidebands setting. J.J. Boyd (KG4VCF), with
//                AI-assisted transformation via Claude Code.
// =================================================================

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

#pragma once

#include "gui/SetupPage.h"
#include "gui/NereusGuiExport.h"

class QCheckBox;
class QLabel;

namespace NereusSDR {

// Setup > General > Filters, Thetis's tpFilters tab (setup.designer.cs:12305
// -12313 [v2.10.3.15], in tcGeneral after Options and Calibration). Only its
// "Limit filter edges to sidebands" checkbox is built; the tab's RX1/RX2
// filter controls and snap frequencies are not. The box shows and changes
// RadioModel's limitFiltersToSidebands, the Core's setting in a remote
// window.
class NEREUS_GUI_EXPORT GeneralFiltersPage : public SetupPage {
    Q_OBJECT
public:
    explicit GeneralFiltersPage(RadioModel* model, QWidget* parent = nullptr);

    void syncFromModel() override;

    QCheckBox* limitFilterEdgesCheckBox() const { return m_chkLimitFilterEdgesToSidebands; }

private:
    void syncLimitFromModel();

    QCheckBox* m_chkLimitFilterEdgesToSidebands{nullptr};
    QLabel*    m_lblLimitCore{nullptr};
};

} // namespace NereusSDR
