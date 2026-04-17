#pragma once

// =================================================================
// src/gui/setup/TransmitSetupPages.h  (NereusSDR)
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

#include "gui/SetupPage.h"

class QSlider;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Transmit > Power & PA
// Corresponds to Thetis setup.cs PA / Power tab.
// All controls NYI (disabled).
// ---------------------------------------------------------------------------
class PowerPaPage : public SetupPage {
    Q_OBJECT
public:
    explicit PowerPaPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: Power
    QSlider* m_maxPowerSlider{nullptr};        // 0–100 W
    QSlider* m_swrProtectionSlider{nullptr};   // SWR threshold

    // Section: PA
    QLabel*    m_perBandGainLabel{nullptr};    // placeholder: future table
    QComboBox* m_fanControlCombo{nullptr};     // Off/Low/Med/High/Auto
};

// ---------------------------------------------------------------------------
// Transmit > TX Profiles
// ---------------------------------------------------------------------------
class TxProfilesPage : public SetupPage {
    Q_OBJECT
public:
    explicit TxProfilesPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: Profile
    QLabel*      m_profileListLabel{nullptr};  // placeholder for future list
    QLineEdit*   m_nameEdit{nullptr};
    QPushButton* m_newBtn{nullptr};
    QPushButton* m_deleteBtn{nullptr};
    QPushButton* m_copyBtn{nullptr};

    // Section: Compression
    QCheckBox* m_compressorToggle{nullptr};
    QSlider*   m_gainSlider{nullptr};
    QCheckBox* m_cessbToggle{nullptr};
};

// ---------------------------------------------------------------------------
// Transmit > Speech Processor
// ---------------------------------------------------------------------------
class SpeechProcessorPage : public SetupPage {
    Q_OBJECT
public:
    explicit SpeechProcessorPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: Compressor
    QSlider*   m_gainSlider{nullptr};
    QCheckBox* m_cessbToggle{nullptr};

    // Section: Phase Rotator
    QSpinBox* m_stagesSpin{nullptr};
    QSlider*  m_cornerFreqSlider{nullptr};

    // Section: CFC
    QCheckBox* m_cfcToggle{nullptr};
    QLabel*    m_cfcProfileLabel{nullptr};  // placeholder
};

// ---------------------------------------------------------------------------
// Transmit > PureSignal
// ---------------------------------------------------------------------------
class PureSignalPage : public SetupPage {
    Q_OBJECT
public:
    explicit PureSignalPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();

    // Section: PureSignal
    QCheckBox* m_enableToggle{nullptr};
    QCheckBox* m_autoCalToggle{nullptr};
    QComboBox* m_feedbackDdcCombo{nullptr};  // DDC selection
    QSlider*   m_attentionSlider{nullptr};
    QLabel*    m_infoLabel{nullptr};         // status/info placeholder
};

} // namespace NereusSDR
