// =================================================================
// src/gui/setup/GeneralOptionsPage.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs
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

// src/gui/setup/GeneralOptionsPage.h
//
// Setup → General → Options page.
// Step attenuator enable/value per RX, auto-attenuate controls
// (enable, Classic/Adaptive mode, undo/decay, hold seconds).
//
// Porting from Thetis setup.cs: grpHermesStepAttenuator, groupBoxTS47.

#pragma once

#include "gui/SetupPage.h"

class QCheckBox;
class QComboBox;
class QSpinBox;
class QLabel;

namespace NereusSDR {

class StepAttenuatorController;

class GeneralOptionsPage : public SetupPage {
    Q_OBJECT
public:
    explicit GeneralOptionsPage(RadioModel* model, QWidget* parent = nullptr);

    void syncFromModel() override;

private:
    void buildStepAttGroup();
    void buildAutoAttGroup();
    void connectController();

    StepAttenuatorController* m_ctrl{nullptr};

    // Step Attenuator group
    QCheckBox* m_chkRx1StepAttEnable{nullptr};
    QSpinBox*  m_spnRx1StepAttValue{nullptr};
    QCheckBox* m_chkRx2StepAttEnable{nullptr};
    QSpinBox*  m_spnRx2StepAttValue{nullptr};
    QLabel*    m_lblAdcLinked{nullptr};

    // Auto Attenuate RX1
    QCheckBox* m_chkAutoAttRx1{nullptr};
    QComboBox* m_cmbAutoAttRx1Mode{nullptr};
    QCheckBox* m_chkAutoAttUndoRx1{nullptr};
    QSpinBox*  m_spnAutoAttHoldRx1{nullptr};

    // Auto Attenuate RX2
    QCheckBox* m_chkAutoAttRx2{nullptr};
    QComboBox* m_cmbAutoAttRx2Mode{nullptr};
    QCheckBox* m_chkAutoAttUndoRx2{nullptr};
    QSpinBox*  m_spnAutoAttHoldRx2{nullptr};
};

} // namespace NereusSDR
