#pragma once

// =================================================================
// src/gui/containers/meter_property_editors/NeedleScalePwrItemEditor.h  (NereusSDR)
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

#include "BaseItemEditor.h"

class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QFontComboBox;
class QTableWidget;

namespace NereusSDR {

class NeedleScalePwrItem;

// Phase 3G-6 block 4 — per-item property editor for NeedleScalePwrItem.
// Exposes full Thetis parity: colors, font, marks, power, direction,
// geometry, and the calibration map (fully editable table).
class NeedleScalePwrItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit NeedleScalePwrItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    void refreshCalTable(NeedleScalePwrItem* nspi);
    void commitCalTable(NeedleScalePwrItem* nspi);

    QPushButton*    m_btnLowColour{nullptr};
    QPushButton*    m_btnHighColour{nullptr};
    QFontComboBox*  m_fontCombo{nullptr};
    QDoubleSpinBox* m_spinFontSize{nullptr};
    QCheckBox*      m_chkFontBold{nullptr};
    QSpinBox*       m_spinMarks{nullptr};
    QCheckBox*      m_chkShowMarkers{nullptr};
    QDoubleSpinBox* m_spinMaxPower{nullptr};
    QCheckBox*      m_chkDarkMode{nullptr};
    QComboBox*      m_comboDirection{nullptr};
    QDoubleSpinBox* m_spinOffsetX{nullptr};
    QDoubleSpinBox* m_spinOffsetY{nullptr};
    QDoubleSpinBox* m_spinLengthFactor{nullptr};
    QTableWidget*   m_calTable{nullptr};
};

} // namespace NereusSDR
