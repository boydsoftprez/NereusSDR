#pragma once

// =================================================================
// src/gui/setup/TransmitSetupPages.h  (NereusSDR)
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

#include "gui/SetupPage.h"
#include "models/Band.h"

#include <array>

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
// Phase 3M-1a H.4: Max Power, per-band Tune Power, ATT-on-TX, and
// ForceATTwhenPSAoff are wired. Other sections deferred to later phases.
// ---------------------------------------------------------------------------
class PowerPaPage : public SetupPage {
    Q_OBJECT
public:
    explicit PowerPaPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();
    void buildPowerGroup();         // H.4: Max Power slider + ATT-on-TX + Force-ATT
    void buildTunePowerGroup();     // H.4: Per-band tune-power spinboxes
    void buildSwrProtectionGroup();
    void buildExternalTxInhibitGroup();
    void buildBlockTxAntennaGroup();
    void buildHfPaGroup();

    // Section: Power (H.4 — wired)
    QSlider*   m_maxPowerSlider{nullptr};        // 0–100 W — wired to TransmitModel::setPower
    QSlider*   m_swrProtectionSlider{nullptr};   // SWR threshold (future)

    // Section: ATT-on-TX (H.4 — wired)
    // chkATTOnTX — Thetis setup.designer.cs:5926-5939 [v2.10.3.13] (tpAlexAntCtrl).
    // NereusSDR places it in Power & PA (better thematic grouping).
    QCheckBox* m_chkAttOnTx{nullptr};

    // ForceATTwhenPSAoff — Thetis setup.designer.cs:5660-5671 [v2.10.3.13].
    // //MW0LGE [2.9.0.7] added  [original inline comment from console.cs:29285]
    QCheckBox* m_chkForceAttWhenPsOff{nullptr};

    // Section: Per-band tune power (H.4 — wired)
    // NereusSDR extension: exposes tunePower_by_band[] (console.cs:12094 [v2.10.3.13])
    // per-band in the setup dialog. Thetis uses a single udTXTunePower (setup.cs:5262
    // [v2.10.3.13]) that updates one slot on band change; NereusSDR lets the user
    // view and edit all 14 slots simultaneously.
    // HF amateur + GEN/WWV/XVTR only (Band::SwlFirst == 14).  SWL bands
    // (Phase 3L extension) have no TX power slot — TX is HF-only on HL2.
    static constexpr int kBandCount = static_cast<int>(Band::SwlFirst);  // 14
    std::array<QSpinBox*, kBandCount> m_tunePwrSpins{};

    // Section: PA
    QLabel*    m_perBandGainLabel{nullptr};    // placeholder: future table
    QComboBox* m_fanControlCombo{nullptr};     // Off/Low/Med/High/Auto

    // Section: SWR Protection (Task 9)
    // grpSWRProtectionControl per setup.designer.cs:5793-5924 [v2.10.3.13]
    QCheckBox*      m_chkSWRProtection{nullptr};
    QDoubleSpinBox* m_udSwrProtectionLimit{nullptr};
    QCheckBox*      m_chkSWRTuneProtection{nullptr};
    QSpinBox*       m_udTunePowerSwrIgnore{nullptr};
    QCheckBox*      m_chkWindBackPowerSWR{nullptr};

    // Section: External TX Inhibit (Task 10)
    // grpExtTXInhibit per setup.designer.cs:46626-46657 [v2.10.3.13]
    QCheckBox* m_chkTXInhibit{nullptr};
    QCheckBox* m_chkTXInhibitReverse{nullptr};

    // Section: Block TX on RX antennas (Task 11)
    // chkBlockTxAnt2/3 per setup.designer.cs:6704-6724 [v2.10.3.13]
    // (NereusSDR-original labels — Thetis ships unlabelled column-header checkboxes)
    QCheckBox* m_chkBlockTxAnt2{nullptr};
    QCheckBox* m_chkBlockTxAnt3{nullptr};

    // Section: PA Control (Task 11)
    // chkHFTRRelay per setup.designer.cs:5780-5791 [v2.10.3.13]
    QCheckBox* m_chkHFTRRelay{nullptr};
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

    // ── Phase 3M-3a-ii Batch 5: Compression section removed.
    // CPDR + CESSB controls now live on Setup → DSP → CFC (CESSB group)
    // and on the dashboard / TxApplet [PROC] toggle.  The orphan
    // m_compressorToggle / m_gainSlider / m_cessbToggle disabled stubs
    // violated master-design meta rule §2.2.1 (no half-shipped
    // placeholders) and have been removed.
};

// ---------------------------------------------------------------------------
// Transmit > Speech Processor — TX dashboard (NereusSDR-spin)
//
// No direct Thetis equivalent.  The Thetis "Speech Proc" tile lives on the
// main console as a single CPDR enable+gain pair (console.cs CPDR controls
// [v2.10.3.13]), and the per-stage controls (Phrot / CFC / CESSB / Leveler
// / ALC) live across multiple Setup → DSP tabs.  NereusSDR repurposes this
// page as a one-stop overview that shows the live state of every TXA
// speech-chain stage and cross-links to where each stage is configured.
//
// Stage names cited from WDSP TXA pipeline (txa[ch] members in
// `wdsp/TXA.c:create_txa` [v2.10.3.13]):
//   eqp (TX EQ) / leveler / cfcomp+cfir (CFC) / compressor (CPDR) /
//   phrot (Phase Rotator) / amsq (AM Squelch / DEXP) / alc.
//
// 3M-3a-i Batch 5 surface; the per-stage Setup pages it cross-links into
// land in 3M-3a-ii (CFC / Phrot / CESSB) and 3M-3a-iii (VOX / DEXP).
// ---------------------------------------------------------------------------
class SpeechProcessorPage : public SetupPage {
    Q_OBJECT
public:
    explicit SpeechProcessorPage(RadioModel* model, QWidget* parent = nullptr);

signals:
    /// Emitted when a cross-link button is clicked.  MainWindow listens and
    /// re-targets the active SetupDialog page via SetupDialog::selectPage().
    /// `category` is informational (always "DSP" for current cross-links);
    /// `page` is the SetupDialog leaf-item label (e.g. "AGC/ALC", "CFC",
    /// "VOX/DEXP").
    void openSetupRequested(const QString& category, const QString& page);

private:
    void buildUI();
    void buildActiveProfileSection();
    void buildStageStatusSection();
    void buildQuickNotesSection();

    // Helper: build one "Stage  ●  state    [Open ... ]" row inside the
    // Stage Status section's grid layout.  Returns the status QLabel so the
    // caller can wire it to a model signal for live updates.
    QLabel* addStageRow(class QGridLayout* grid, int row,
                         const QString& stageName,
                         const QString& initialState,
                         bool initiallyOn,
                         const QString& buttonText,
                         const QString& buttonTooltip,
                         const QString& linkPage,           // empty → button is a placeholder
                         const QString& futurePhaseTag);    // empty → no "(3M-3a-X)" suffix

    // Section: Active Profile
    QLabel*      m_activeProfileLabel{nullptr};
    QPushButton* m_manageProfileBtn{nullptr};

    // Section: Stage Status — one status QLabel per stage, kept for live updates.
    QLabel* m_txEqStatusLabel{nullptr};
    QLabel* m_levelerStatusLabel{nullptr};
    QLabel* m_alcStatusLabel{nullptr};       // static "always-on" (no signal)
    QLabel* m_phrotStatusLabel{nullptr};     // future phase
    QLabel* m_cfcStatusLabel{nullptr};       // future phase
    QLabel* m_cessbStatusLabel{nullptr};     // future phase
    QLabel* m_amSqDexpStatusLabel{nullptr};  // future phase

    // Cross-link buttons (Open TX EQ Editor / Open AGC-ALC / Open CFC / Open VOX-DEXP)
    QPushButton* m_openTxEqBtn{nullptr};
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
