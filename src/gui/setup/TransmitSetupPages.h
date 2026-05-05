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
//   2026-05-03 — Phase 3M-3a-iii Task 14: added DexpVoxPage (mirrors
//                 Thetis tpDSPVOXDE 1:1 — setup.designer.cs:44763-45260
//                 [v2.10.3.13]).
//   2026-05-04 — Issue #175 Wave 1: TxProfilesPage placeholder body
//                 dropped (5 disabled-stub controls with no Thetis
//                 upstream).  Live TX-profile editor lives at Setup →
//                 Audio → TX Profile.
//   2026-05-04 — Issue #175 Wave 1 (cont): PowerPage source-first
//                 strip-out — dropped dead "SWR Protection" slider stub,
//                 per-band Tune Power 14-spinbox grid + helpers, Reset
//                 Tune Power Defaults button + slot, and Block-TX-on-RX
//                 antennas group.  TX TUN Meter combo items now
//                 mi0bot-verbatim.
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
#include "models/TransmitModel.h"   // DrivePowerSource enum (Issue #175 Task 8)
#include "core/HpsdrModel.h"        // HPSDRModel enum + spinbox helpers (Issue #175 Task 8)

class QSlider;
class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QButtonGroup;

namespace NereusSDR {

// ---------------------------------------------------------------------------
// Transmit > Power
// Corresponds to Thetis setup.cs Power tab (PA-specific groups live under
// the new top-level PA category — IA reshape Phase 2 / Phase 6, 2026-05-02).
// Phase 3M-1a H.4: Max Power, per-band Tune Power, ATT-on-TX, and
// ForceATTwhenPSAoff are wired. Other sections deferred to later phases.
// ---------------------------------------------------------------------------
class PowerPage : public SetupPage {
    Q_OBJECT
public:
    explicit PowerPage(RadioModel* model, QWidget* parent = nullptr);

    // Issue #175 Task 8 — reconfigure the udTXTunePower spinbox bounds /
    // step / decimals / suffix for the given SKU.  Mirrors mi0bot
    // setup.cs:20328-20331 [v2.10.3.13-beta2] HERMESLITE branch.
    // Called from the constructor and from RadioModel::currentRadioChanged.
    void applyHpsdrModel(HPSDRModel m);

private:
    void buildUI();
    void buildPowerGroup();         // H.4: Max Power slider + ATT-on-TX + Force-ATT
    void buildTuneGroup();          // Issue #175 Task 8: grpPATune (Tune source + meter + fixed-mode pwr)
    void buildSwrProtectionGroup();
    void buildExternalTxInhibitGroup();
    void buildHfPaGroup();

    // Issue #175 Task 8 helper — flip enabled state on the Fixed-mode
    // spinbox so it tracks the active drive source.
    void onTuneDriveSourceChanged(DrivePowerSource src);

    // Issue #175 PR #194 review fix — SKU-aware unit conversion for the
    // udTXTunePower Fixed-mode spinbox.  Storage is the int slider scale
    // (TransmitModel::tunePower(): 0..100 watts on non-HL2, 0..99 mi0bot
    // sub-step units on HL2).  Display unit polymorphs by SKU:
    //   - non-HL2: identity (display == stored, watts)
    //   - HL2:     dB attenuation, formulae from mi0bot setup.cs:5305-5311
    //              and 9395-9398 [v2.10.3.13-beta2]:
    //                stored→display: (stored / 3.0 - 33.0) / 2.0
    //                display→stored: (int)((33.0 + display * 2.0) * 3.0)
    // Both helpers consult the live TransmitModel::hpsdrModel() so the same
    // displayed-value cell maps correctly when the connected SKU changes.
    double tunePowerDisplayFromStored(int stored);
    int    tunePowerStoredFromDisplay(double display);

    // Section: Power (H.4 — wired)
    QSlider*   m_maxPowerSlider{nullptr};        // 0–100 W — wired to TransmitModel::setPower

    // Section: ATT-on-TX (H.4 — wired)
    // chkATTOnTX — Thetis setup.designer.cs:5926-5939 [v2.10.3.13] (tpAlexAntCtrl).
    // NereusSDR places it in Power (better thematic grouping).
    QCheckBox* m_chkAttOnTx{nullptr};

    // ForceATTwhenPSAoff — Thetis setup.designer.cs:5660-5671 [v2.10.3.13].
    // //MW0LGE [2.9.0.7] added  [original inline comment from console.cs:29285]
    QCheckBox* m_chkForceAttWhenPsOff{nullptr};

    // Section: grpPATune "Tune" group (Issue #175 Task 8)
    // From mi0bot-Thetis setup.designer.cs:47874-47930 [v2.10.3.13-beta2].
    // Three drive-source radios + TX TUN Meter combo + Fixed-mode tune power
    // spinbox.  Spinbox bounds polymorph by SKU via applyHpsdrModel().
    QGroupBox*       m_grpPATune{nullptr};
    QRadioButton*    m_radFixedDrive{nullptr};
    QRadioButton*    m_radDriveSlider{nullptr};
    QRadioButton*    m_radTuneSlider{nullptr};
    QButtonGroup*    m_tuneDriveButtons{nullptr};
    QComboBox*       m_comboTxTunMeter{nullptr};
    QDoubleSpinBox*  m_fixedTunePwrSpin{nullptr};

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

    // Section: PA Control (Task 11)
    // chkHFTRRelay per setup.designer.cs:5780-5791 [v2.10.3.13]
    QCheckBox* m_chkHFTRRelay{nullptr};
};

// ---------------------------------------------------------------------------
// Transmit > TX Profiles
//
// Issue #175 Wave 1 follow-up — page body reduced to a single explanatory
// label.  The live TX-profile editor lives at Setup → Audio → TX Profile
// (TxProfileSetupPage, fully wired to MicProfileManager).  Thetis ships
// grpTXProfile (combo + Save/Delete) directly on tpTransmit at mi0bot
// setup.designer.cs:47829-47836 [v2.10.3.13-beta2]; no dedicated "TX
// Profiles" tab page exists upstream.  Leaf retained in SetupDialog tree
// per JJ's direction (IA reshape decisions deferred to a separate audit).
// ---------------------------------------------------------------------------
class TxProfilesPage : public SetupPage {
    Q_OBJECT
public:
    explicit TxProfilesPage(RadioModel* model, QWidget* parent = nullptr);

private:
    void buildUI();
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

// ---------------------------------------------------------------------------
// Transmit > DEXP/VOX
//
// Phase 3M-3a-iii Task 14: full Setup page mirroring Thetis tpDSPVOXDE 1:1
// (setup.designer.cs:44763-45260 [v2.10.3.13]).  Three QGroupBox containers
// laid out left-to-right:
//   1. grpDEXPVOX           ("VOX / DEXP")               — line 44843
//   2. grpDEXPLookAhead     ("Audio LookAhead")          — line 44763
//   3. grpScf               ("Side-Channel Trigger Filter") — line 45165
//
// 14 controls total bound bidirectionally to TransmitModel via QSignalBlocker
// guards.  Object names (chkVOXEnable, udDEXPThreshold, …) match the Thetis
// Designer verbatim so that downstream code (PhoneCwApplet right-click in
// Task 15) can target controls via findChild() if ever needed.
// ---------------------------------------------------------------------------
class DexpVoxPage : public SetupPage {
    Q_OBJECT
public:
    explicit DexpVoxPage(RadioModel* model, QWidget* parent = nullptr);

private:
    // ── grpDEXPVOX ───────────────────────────────────────────────────────────
    QCheckBox*      m_chkVOXEnable{nullptr};
    QCheckBox*      m_chkDEXPEnable{nullptr};
    QSpinBox*       m_udDEXPThreshold{nullptr};         // dBV     int (range -80..0)
    QDoubleSpinBox* m_udDEXPHysteresisRatio{nullptr};   // dB      1 dp (range 0..10)
    QDoubleSpinBox* m_udDEXPExpansionRatio{nullptr};    // dB      1 dp (range 0..30)
    QSpinBox*       m_udDEXPAttack{nullptr};            // ms      int (range 2..100)
    QSpinBox*       m_udDEXPHold{nullptr};              // ms      int (range 1..2000, step 10)
    QSpinBox*       m_udDEXPRelease{nullptr};           // ms      int (range 2..1000)
    QSpinBox*       m_udDEXPDetTau{nullptr};            // ms      int (range 1..100)

    // ── grpDEXPLookAhead ─────────────────────────────────────────────────────
    QCheckBox*      m_chkDEXPLookAheadEnable{nullptr};
    QSpinBox*       m_udDEXPLookAhead{nullptr};         // ms      int (range 10..999)

    // ── grpScf ───────────────────────────────────────────────────────────────
    QCheckBox*      m_chkSCFEnable{nullptr};
    QSpinBox*       m_udSCFLowCut{nullptr};             // Hz      int (range 100..10000, step 10)
    QSpinBox*       m_udSCFHighCut{nullptr};            // Hz      int (range 100..10000, step 10)
};

} // namespace NereusSDR
