#pragma once

// =================================================================
// src/gui/setup/DspSetupPages.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
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

// src/gui/setup/DspSetupPages.h
//
// Nine DSP setup pages for the NereusSDR settings dialog.
// All controls are present as real widgets but disabled (NYI — no WDSP
// wiring yet).  Each class corresponds to one leaf item in the DSP
// category of the tree navigation.
//
// Pages:
//   AgcAlcSetupPage   — AGC / ALC / Leveler
//   NrAnfSetupPage    — Noise Reduction / ANF / EMNR
//   NbSnbSetupPage    — NB1 / NB2 / SNB
//   CwSetupPage       — Keyer / Timing / APF
//   AmSamSetupPage    — AM TX / SAM / Squelch
//   FmSetupPage       — FM RX / FM TX
//   VoxDexpSetupPage  — VOX / DEXP
//   CfcSetupPage      — CFC / Profile
//   MnfSetupPage      — Manual Notch Filter

#include "core/WdspTypes.h"   // NrSlot
#include "gui/SetupPage.h"

#include <QCheckBox>

namespace NereusSDR {

class RadioModel;
enum class AGCMode : int;

// ── AGC / ALC ─────────────────────────────────────────────────────────────────

class AgcAlcSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit AgcAlcSetupPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void updateCustomGating(AGCMode mode);

    QComboBox*  m_agcModeCombo{nullptr};
    QSpinBox*   m_agcAttack{nullptr};
    QSpinBox*   m_agcDecay{nullptr};
    QSpinBox*   m_agcHang{nullptr};
    QSlider*    m_agcSlope{nullptr};
    QSpinBox*   m_agcMaxGain{nullptr};
    QSpinBox*   m_agcFixedGain{nullptr};
    QSlider*    m_agcHangThresh{nullptr};
    QCheckBox*  m_autoAgcChk{nullptr};
    QSpinBox*   m_autoAgcOffset{nullptr};

    // ── Phase 3M-3a-i Batch 2 Task 2 (D): TX Leveler + TX ALC controls ──
    // Wired bidirectionally to RadioModel::transmitModel() — see
    // DspSetupPages.cpp for the connect() block.
    QCheckBox*  m_txLevelerOnChk{nullptr};
    QSpinBox*   m_txLevelerTopSpin{nullptr};
    QSpinBox*   m_txLevelerDecaySpin{nullptr};
    QSpinBox*   m_txAlcMaxGainSpin{nullptr};
    QSpinBox*   m_txAlcDecaySpin{nullptr};
};

// ── NR / ANF ──────────────────────────────────────────────────────────────────

class NrAnfSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit NrAnfSetupPage(RadioModel* model, QWidget* parent = nullptr);

    // Programmatically select a sub-tab by NrSlot. Used by MainWindow to
    // route "More Settings…" popup clicks to the correct filter's sub-tab.
    void selectSubtab(NereusSDR::NrSlot slot);

private:
    QTabWidget* m_tabs{nullptr};  // owned by content layout
};

// ── NB / SNB ──────────────────────────────────────────────────────────────────

class NbSnbSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit NbSnbSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

// ── CW ───────────────────────────────────────────────────────────────────────

class CwSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit CwSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

// ── AM / SAM ─────────────────────────────────────────────────────────────────

class AmSamSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit AmSamSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

// ── FM ───────────────────────────────────────────────────────────────────────

class FmSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit FmSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

// ── VOX / DEXP ───────────────────────────────────────────────────────────────

class VoxDexpSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit VoxDexpSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

// ── CFC ──────────────────────────────────────────────────────────────────────

class CfcSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit CfcSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

// ── MNF ──────────────────────────────────────────────────────────────────────

class MnfSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit MnfSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

} // namespace NereusSDR
