// =================================================================
// src/gui/applets/DigitalApplet.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-18 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Layout ports Thetis setup.cs DIGx tab (digital-mode slider rows) + console.cs VAC wiring. All controls NYI — wired in later phase.
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

//
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

#include "AppletWidget.h"

class QComboBox;
class QSlider;
class QPushButton;
class QLabel;

namespace NereusSDR {

// VAC/VAX digital audio routing applet — NYI shell (Phase 3-VAX).
//
// Controls:
//   1. VAC 1 enable       — QPushButton("VAC 1", green, checkable)
//   2. VAC 1 device combo — QComboBox (device list, empty)
//   3. VAC 2 enable       — QPushButton("VAC 2", green, checkable)
//   4. VAC 2 device combo — QComboBox (device list, empty)
//   5. Sample rate combo  — QComboBox: "48000", "96000", "192000"
//   6. Stereo/Mono toggle — QPushButton("Stereo", blue, checkable)
//   7. Buffer size combo  — QComboBox: "256", "512", "1024", "2048"
//
// Additional: TX/RX gain sliders (x2), rigctld channel combo.
// All controls disabled (NYI). Body margins: (4,2,4,2), spacing 2.
class DigitalApplet : public AppletWidget {
    Q_OBJECT

public:
    explicit DigitalApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("DIG"); }
    QString appletTitle() const override { return QStringLiteral("Digital / VAC"); }
    void syncFromModel() override;

private:
    void buildUI();
    void buildSliderRow(QVBoxLayout* root, const QString& label,
                        QSlider*& sliderOut, QLabel*& valueLblOut);

    // Control 1 — VAC 1 enable
    QPushButton* m_vac1Btn        = nullptr;
    // Control 2 — VAC 1 device
    QComboBox*   m_vac1DevCombo   = nullptr;

    // Control 3 — VAC 2 enable
    QPushButton* m_vac2Btn        = nullptr;
    // Control 4 — VAC 2 device
    QComboBox*   m_vac2DevCombo   = nullptr;

    // Control 5 — Sample rate
    QComboBox*   m_sampleRateCombo = nullptr;

    // Control 6 — Stereo/Mono
    QPushButton* m_stereoBtn      = nullptr;

    // Control 7 — Buffer size
    QComboBox*   m_bufferSizeCombo = nullptr;

    // Additional — RX/TX gain sliders
    QSlider*     m_rxGainSlider   = nullptr;
    QLabel*      m_rxGainLbl      = nullptr;
    QSlider*     m_txGainSlider   = nullptr;
    QLabel*      m_txGainLbl      = nullptr;

    // Additional — rigctld channel combo
    QComboBox*   m_rigctldCombo   = nullptr;
};

} // namespace NereusSDR
