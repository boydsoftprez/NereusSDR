#pragma once
// =================================================================
// src/gui/setup/hardware/PaCalibrationTab.h  (NereusSDR)
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

// PaCalibrationTab.h
//
// "PA Calibration" sub-tab of HardwarePage.
// Surfaces per-band target power + gain correction tables, a PA profile
// selector, a step-attenuator calibration button (gated on caps), and an
// auto-calibrate trigger (cold this phase).
//
// Source: Thetis Setup.cs:
//   - grpGainByBandPA (line 1796): per-band PA gain group box
//   - nudPAProfileGain_ValueChanged (line 24135): per-band gain correction
//   - nudMaxPowerForBandPA (line 24048): per-band max/target power
//   - comboPAProfile / updatePAProfileCombo() (lines 1973, 23784-23786)
//   - chkPA160..chkPA10 enable flags (line 9969)
//   - udPACalPower (line 12278): cal power spinbox

#include <QVariant>
#include <QWidget>

class QComboBox;
class QGroupBox;
class QPushButton;
class QScrollArea;
class QTableWidget;
class QVBoxLayout;

namespace NereusSDR {

class RadioModel;
struct RadioInfo;
struct BoardCapabilities;

class PaCalibrationTab : public QWidget {
    Q_OBJECT
public:
    explicit PaCalibrationTab(RadioModel* model, QWidget* parent = nullptr);
    void populate(const RadioInfo& info, const BoardCapabilities& caps);
    void restoreSettings(const QMap<QString, QVariant>& settings);

signals:
    void settingChanged(const QString& key, const QVariant& value);

private:
    RadioModel*  m_model{nullptr};

    // Per-band target power table — 14 rows × 1 column "Target (W)"
    QTableWidget* m_targetPowerTable{nullptr};

    // Per-band gain correction table — 14 rows × 1 column "Correction (dB)"
    QTableWidget* m_gainCorrectionTable{nullptr};

    // PA profile combo
    QComboBox*    m_paProfileCombo{nullptr};

    // Step attenuator cal button — hidden unless caps.hasStepAttenuatorCal
    QPushButton*  m_stepAttenCalButton{nullptr};

    // Auto-calibrate trigger (cold)
    QPushButton*  m_autoCalibrateButton{nullptr};
};

} // namespace NereusSDR
