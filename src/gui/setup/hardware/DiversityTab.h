#pragma once
// =================================================================
// src/gui/setup/hardware/DiversityTab.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/DiversityForm.cs
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

// DiversityTab.h
//
// "Diversity" sub-tab of HardwarePage.
// Exposes enable toggle, phase/gain sliders, reference ADC selector,
// and a "null signal" preset button.
//
// Source: Thetis DiversityForm.cs — chkLockAngle / chkLockR (lines 1216/1228),
// udGainMulti NumericUpDown (line 1177), labelTS4 "Phase" / labelTS3 "Gain"
// (lines 1240/1293), trackBarR1/trackBarPhase1 commented-out sliders (lines 182-183),
// udR1/udR2 values (lines 170-171).

#include <QVariant>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QSlider;
class QStackedWidget;

namespace NereusSDR {

class RadioModel;
struct RadioInfo;
struct BoardCapabilities;

class DiversityTab : public QWidget {
    Q_OBJECT
public:
    explicit DiversityTab(RadioModel* model, QWidget* parent = nullptr);
    void populate(const RadioInfo& info, const BoardCapabilities& caps);
    void restoreSettings(const QMap<QString, QVariant>& settings);

signals:
    void settingChanged(const QString& key, const QVariant& value);

private:
    RadioModel*      m_model{nullptr};

    // Two pages: 0 = unsupported notice, 1 = controls
    QStackedWidget*  m_stack{nullptr};

    // Controls page widgets
    QCheckBox*       m_enableCheck{nullptr};
    QSlider*         m_phaseSlider{nullptr};
    QLabel*          m_phaseValueLabel{nullptr};
    QSlider*         m_gainSlider{nullptr};
    QLabel*          m_gainValueLabel{nullptr};
    QComboBox*       m_referenceAdcCombo{nullptr};
    QPushButton*     m_nullSignalButton{nullptr};
};

} // namespace NereusSDR
