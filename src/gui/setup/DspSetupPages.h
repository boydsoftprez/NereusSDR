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
//   CfcSetupPage      — CFC / Profile
//   MnfSetupPage      — Manual Notch Filter
//
// (VoxDexpSetupPage placeholder removed in 3M-3a-iii Task 16 — the wired
//  page lives at Setup → Transmit → DEXP/VOX (DexpVoxPage from Task 14).)

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

    // P1 full-parity §4.2 — gate the "Sidetone Volume" row on
    // BoardCapabilities.hasSidetoneGenerator.  HL2 firmware generates the
    // CW sidetone in hardware (with its own volume control routed via the
    // radio's audio path); standard P1 boards rely on host-generated
    // software sidetone.  When false, the row is hidden so users on
    // non-HL2 boards don't see a control that does nothing on their
    // hardware.  Default: hidden (no flag set yet → no radio connected).
    void setHasSidetoneGenerator(bool on);

    // Test seam — returns the Sidetone Volume row's visibility state.
    // Used by tst_board_capability_flag_wiring to assert the gate works
    // without a fully-shown widget (isVisibleTo(parent) requires a top-
    // level shown ancestor, which is awkward in a unit test).
    bool sidetoneRowVisibleForTest() const;

private:
    QWidget* m_sidetoneRow = nullptr;
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

// (VoxDexpSetupPage placeholder removed in 3M-3a-iii Task 16 — wired page
//  lives at Setup → Transmit → DEXP/VOX (DexpVoxPage from Task 14).)

// ── CFC ──────────────────────────────────────────────────────────────────────
//
// Phase 3M-3a-ii Batch 5: full implementation of the Setup → DSP → CFC page.
// Mirrors Thetis tpDSPCFC tab layout 1:1 (setup.Designer.cs grpPhRot at
// 46162-46280 [v2.10.3.13]).  Hosts three group boxes:
//   1. Phase Rotator   — chkPHROTEnable / chkPHROTReverse / udPhRotFreq /
//                        udPHROTStages, bidirectional with TransmitModel
//                        phase-rotator-* properties.
//   2. CFC             — Enable / Post-EQ Enable / PreComp / PostEqGain
//                        global controls + a [Configure CFC bands…] button
//                        that emits openCfcDialogRequested (Batch 6 wires
//                        the per-band TxCfcDialog).
//   3. CESSB           — Enable toggle + explanatory label per WDSP
//                        TXA.c:846-855 [v2.10.3.13] (CESSB gated on CPDR
//                        via the bp2 ladder).

class CfcSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit CfcSetupPage(RadioModel* model, QWidget* parent = nullptr);

signals:
    /// Emitted when the [Configure CFC bands…] button is clicked.  Batch 6
    /// will wire the parent SetupDialog to route this to the modeless
    /// TxCfcDialog (10-band CFC editor).  For Batch 5 the signal lands but
    /// no dialog launches.
    void openCfcDialogRequested();

private:
    // ── Phase Rotator group ──────────────────────────────────────────────────
    QCheckBox* m_phRotEnableChk{nullptr};
    QCheckBox* m_phRotReverseChk{nullptr};
    QSpinBox*  m_phRotFreqSpin{nullptr};
    QSpinBox*  m_phRotStagesSpin{nullptr};

    // ── CFC group ────────────────────────────────────────────────────────────
    QCheckBox* m_cfcEnableChk{nullptr};
    QCheckBox* m_cfcPostEqEnableChk{nullptr};
    QSpinBox*  m_cfcPrecompSpin{nullptr};
    QSpinBox*  m_cfcPostEqGainSpin{nullptr};
    class QPushButton* m_cfcBandsBtn{nullptr};

    // ── CESSB group ──────────────────────────────────────────────────────────
    QCheckBox* m_cessbEnableChk{nullptr};
};

// ── MNF ──────────────────────────────────────────────────────────────────────

class MnfSetupPage : public SetupPage {
    Q_OBJECT
public:
    explicit MnfSetupPage(RadioModel* model, QWidget* parent = nullptr);
};

} // namespace NereusSDR
