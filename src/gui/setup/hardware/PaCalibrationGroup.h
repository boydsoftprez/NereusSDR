// =================================================================
// src/gui/setup/hardware/PaCalibrationGroup.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.cs
//     (ud10PA1W..ud10PA10W, ud100PA10W..ud100PA100W, ud200PA20W..ud200PA200W
//      spinbox blocks driving CalibratedPAPower lookup -- lines 5404-5594),
//      original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs
//     (CalibratedPAPower + PowerKernel -- per-board PA forward-power
//      calibration -- lines 6691-6768),
//      original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-05-02 -- Original implementation. Per-board PA forward-power
//                  cal-point spinbox group; populates ud{10|100|200}PA{N}W
//                  equivalent based on PaCalBoardClass. Section 3.3 of
//                  P1 full-parity epic. J.J. Boyd (KG4VCF), with
//                  AI-assisted transformation via Anthropic Claude Code.
// =================================================================

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

// --- From console.cs ---

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

#pragma once

#include "core/PaCalProfile.h"

#include <QGroupBox>
#include <QMetaObject>
#include <QString>
#include <array>

class QDoubleSpinBox;
class QLabel;

namespace NereusSDR {

class CalibrationController;

// PaCalibrationGroup -- per-board-class PA forward-power calibration UI.
//
// Hosts 10 spinboxes (cal points 1..10) labelled per class:
//   Anan10     "1 W"   .. "10 W"   range 0.0..15.0  step 0.1
//   Anan100    "10 W"  .. "100 W"  range 0.0..150.0 step 1.0
//   Anan8000   "20 W"  .. "200 W"  range 0.0..300.0 step 1.0
//   HermesLite "0.5 W" .. "5 W"    range 0.0..7.5   step 0.05
//                  (placeholder; revisited in 3M-2)
//   None       group is hidden, no spinboxes
//
// Spinbox edits route through CalibrationController::setPaCalPoint, which
// owns per-MAC persistence (Section 3.2 of the P1 full-parity epic).
//
// Source: Thetis setup.cs:5404-5594 ud{10|100|200}PA{N}W spinbox blocks
// [v2.10.3.13]; CalibratedPAPower / PowerKernel console.cs:6691-6768
// [v2.10.3.13].
class PaCalibrationGroup : public QGroupBox {
    Q_OBJECT
public:
    explicit PaCalibrationGroup(QWidget* parent = nullptr);

    // Bind to the controller and (re)build the spinbox set for boardClass.
    // Safe to call repeatedly when board class changes (radio swap or first
    // connect). Hides the whole group when boardClass == PaCalBoardClass::None.
    void populate(CalibrationController* controller, PaCalBoardClass boardClass);

#ifdef NEREUS_BUILD_TESTS
    // Test seams.
    int     spinBoxCountForTest() const;            // 0 (None) / 10 (any other)
    QString labelTextForTest(int idx) const;        // idx 1..10 -- e.g. "10 W"
    double  spinValueForTest(int idx) const;        // idx 1..10
    void    setSpinValueForTest(int idx, double v); // idx 1..10
#endif

private:
    void rebuildLayout(PaCalBoardClass cls);   // tear down + recreate spinboxes
    void onSpinChanged(int idx, double v);     // -> m_controller->setPaCalPoint
    void syncFromProfile();                    // controller -> spinboxes (echo-guarded)

    CalibrationController*          m_controller{nullptr};
    PaCalBoardClass                 m_boardClass{PaCalBoardClass::None};
    std::array<QDoubleSpinBox*, 11> m_spins{};   // index 0 unused -- kept for 1:1 parity with PaCalProfile.watts
    std::array<QLabel*, 11>         m_labels{};
    bool                            m_updatingFromController{false};

    // Connections to the bound controller -- disconnected on rebind so we
    // don't double-fire on radio swaps.
    QMetaObject::Connection m_pointConn;
    QMetaObject::Connection m_profileConn;
};

}  // namespace NereusSDR
