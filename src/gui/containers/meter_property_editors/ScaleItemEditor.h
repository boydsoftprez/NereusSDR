#pragma once
// =================================================================
// src/gui/containers/meter_property_editors/ScaleItemEditor.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

#include "BaseItemEditor.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QSpinBox;
class QPushButton;

namespace NereusSDR {

class ScaleItem;

class ScaleItemEditor : public BaseItemEditor {
    Q_OBJECT
public:
    explicit ScaleItemEditor(QWidget* parent = nullptr);
    void setItem(MeterItem* item) override;

private:
    void buildTypeSpecific() override;

    QComboBox*      m_comboOrientation{nullptr};
    QDoubleSpinBox* m_spinMin{nullptr};
    QDoubleSpinBox* m_spinMax{nullptr};
    QSpinBox*       m_spinMajorTicks{nullptr};
    QSpinBox*       m_spinMinorTicks{nullptr};
    QPushButton*    m_btnTickColor{nullptr};
    QPushButton*    m_btnLabelColor{nullptr};
    QSpinBox*       m_spinFontSize{nullptr};
    // Phase B4 — ShowType centered title (Thetis clsScaleItem.ShowType)
    QCheckBox*      m_chkShowType{nullptr};
    QPushButton*    m_btnTitleColor{nullptr};
};

} // namespace NereusSDR
