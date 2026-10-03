#pragma once

// =================================================================
// src/gui/setup/hardware/AntennaAlexAntennaControlTab.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.designer.cs (~lines 5981-7000+,
//     grpAlexAntCtrl + panelAlexTXAntControl + panelAlexRXAntControl)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Sub-sub-tab under Hardware → Antenna/ALEX.
//                Per-band antenna assignment + Block-TX safety; backed
//                by AlexController model (Phase 3P-F Task 1).
//   2026-09-23 - R-R3-46: remote window source and transmit
//                 permission. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-25 - R-R3-49 / R-R3-46 (parity Task 12): a remote window's
//                 transmit half writes the Core and follows whether the Core
//                 takes it. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================

//=================================================================
// setup.designer.cs
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
//
// === Verbatim Thetis Console/setup.designer.cs header (lines 1-50) ===
// namespace Thetis { using System.Windows.Forms; partial class Setup {
//   #region Windows Form Designer generated code
//   private void InitializeComponent() {
//     this.components = new System.ComponentModel.Container();
//     System.Windows.Forms.TabPage tpAlexAntCtrl;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS3;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS4;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS6;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS9;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS10;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS12;
//     System.ComponentModel.ComponentResourceManager resources = ...;
//     this.chkForceATTwhenOutPowerChanges_decreased = new CheckBoxTS();
//     this.chkUndoAutoATTTx = new CheckBoxTS();
//     this.chkAutoATTTXPsOff = new CheckBoxTS();
//     this.lblTXattBand = new LabelTS();
//     this.chkForceATTwhenOutPowerChanges = new CheckBoxTS();
//     this.chkForceATTwhenPSAoff = new CheckBoxTS();
//     this.chkEnableXVTRHF = new CheckBoxTS();
//     this.chkBPF2Gnd = new CheckBoxTS();
//     this.chkDisableRXOut = new CheckBoxTS();
//     this.chkEXT2OutOnTx = new CheckBoxTS();
//     this.chkEXT1OutOnTx = new CheckBoxTS();
//     this.labelATTOnTX = new LabelTS();
// =================================================================

#include <QWidget>
#include <QVector>
#include <array>

#include "models/Band.h"

class QBoxLayout;
class QCheckBox;
class QButtonGroup;
class QLabel;
class QRadioButton;
class QVBoxLayout;

namespace NereusSDR {

class RadioModel;
class AlexController;
class AlexAntennaFacade;
struct RadioInfo;

// AntennaAlexAntennaControlTab — "Antenna Control" sub-sub-tab under Hardware → Antenna/ALEX.
//
// Ports Thetis grpAlexAntCtrl / panelAlexTXAntControl / panelAlexRXAntControl
// (setup.designer.cs:5981-7000+) [@501e3f5].
//
// Three rows:
//   1. Block-TX safety strip — two checkboxes + italic warning label
//   2. TX Antenna per band   — 14 rows × 3 radio buttons (Ant 1/2/3)
//   3. RX1 / RX2 per band    — 14 rows × 6 radio buttons (RX1: 1/2/3 + RX-only: 1/2/3)
//   4. TX-bypass strip       — 5 checkboxes, SKU-gated visibility (Phase 3P-I-b T7)
//
// UI bindings are backed by AlexController (Phase 3P-F Task 1). Changes call
// AlexController setters directly; antennaChanged/blockTxChanged signals
// push model state back to the UI.
//
// NereusSDR spin: 14 bands (Band160m … XVTR) vs Thetis's 12.
class AntennaAlexAntennaControlTab : public QWidget {
    Q_OBJECT

public:
    explicit AntennaAlexAntennaControlTab(RadioModel* model, QWidget* parent = nullptr);

    // Expose a test accessor so unit tests can verify the controller reference.
    AlexController& controller();

    // Parity Task 12 (R-R3-49 / R-R3-46): in a remote window the transmit
    // half (TX antenna grid, Block-TX switches and the four TX relay
    // switches) goes to the Core and follows whether the Core takes it (the
    // Alex facade's transmit edit availability), disabled with its reason
    // when not. It never waits for the radio to leave the air, as in
    // Thetis. A local window's is always live.

#ifdef NEREUS_BUILD_TESTS
    QRadioButton* rxButtonForTest(Band band, int ant) const;
    QRadioButton* rxOnlyButtonForTest(Band band, int ant) const;
    QRadioButton* txButtonForTest(Band band, int ant) const;
    QCheckBox* useTxAntForRxForTest() const { return m_chkUseTxAntForRx; }
    QCheckBox* blockTxAnt2ForTest() const { return m_blockTxAnt2; }
    QCheckBox* blockTxAnt3ForTest() const { return m_blockTxAnt3; }
    QCheckBox* rxOutOnTxForTest() const { return m_chkRxOutOnTx; }
    QCheckBox* ext1OutOnTxForTest() const { return m_chkExt1OutOnTx; }
    QCheckBox* ext2OutOnTxForTest() const { return m_chkExt2OutOnTx; }
    QCheckBox* rxOutOverrideForTest() const { return m_chkRxOutOverride; }
    QWidget* txGridForTest() const { return m_txGridGroup; }
#endif

private slots:
    void onAntennaChanged(NereusSDR::Band band);
    void onBlockTxChanged();

private:
    void buildBlockTxStrip(QVBoxLayout* outerLayout);
    void buildTxGrid(QBoxLayout* outerLayout);
    void buildRxGrid(QBoxLayout* outerLayout);
    void buildTxBypassStrip(QVBoxLayout* outerLayout);
    // Phase 3F Sub-Epic E Tasks 11-13: antenna conflict policy radio group.
    void buildConflictPolicyGroup(QVBoxLayout* outerLayout);
    void applySkuProfile();  // reads current HPSDRModel + refreshes labels/visibility

    // Sync a single row's TX radio buttons from the model (no signal side-effects).
    void syncTxRow(int row);
    // Sync a single row's RX1 and RX-only radio buttons from the model.
    void syncRxRow(int row);
    // Update TX radio button enabled states for blocked ports.
    void updateTxBlockedStates();
    // R-R3-46: re-read every row and switch (a remote window's source is the
    // Core's `alexAntennas` object).
    void syncAllFromSource();
    // Parity Task 12: a remote window's four TX relay switches show the
    // Core's values again (after an edit, or its refusal).
    void syncTxRelaysFromSource();
    // Parity Task 12: a remote window's transmit half follows the Alex
    // facade's transmit edit availability.
    void applyTransmitEditAvailability();

    // Where the tab reads its values: the window's own AlexController
    // locally, the Core's `alexAntennas` object in a remote window.
    int txAntOf(Band band) const;
    int rxAntOf(Band band) const;
    int rxOnlyAntOf(Band band) const;
    bool blockTxAnt2Now() const;
    bool blockTxAnt3Now() const;
    bool rxOutOnTxNow() const;
    bool ext1OutOnTxNow() const;
    bool ext2OutOnTxNow() const;
    bool rxOutOverrideNow() const;
    bool useTxAntForRxNow() const;

    RadioModel*      m_model{nullptr};
    AlexController*  m_alex{nullptr};
    // R-R3-46: a remote window's source and sink for the receive settings
    // (nullptr locally).
    AlexAntennaFacade* m_remoteAlex{nullptr};

    QWidget* m_txGridGroup{nullptr};
    QWidget* m_blockTxFrame{nullptr};

    // Block-TX safety strip
    QCheckBox* m_blockTxAnt2{nullptr};
    QCheckBox* m_blockTxAnt3{nullptr};

    // Per-band TX antenna: one QButtonGroup of 3 QRadioButton per row.
    // Indexed by band int (0..13).  HF amateur + GEN/WWV/XVTR only;
    // SWL bands (Phase 3L extension) inherit ham antenna routing.
    static constexpr int kBandCount = kPerBandStateCount;  // 15: per-band state slots, 2 m at 14

    std::array<QButtonGroup*, kBandCount> m_txGroups{};
    // [band][ant-1] where ant-1 in [0,2]
    std::array<std::array<QRadioButton*, 3>, kBandCount> m_txButtons{};

    // Per-band RX1 antenna: button group per row
    std::array<QButtonGroup*, kBandCount> m_rx1Groups{};
    std::array<std::array<QRadioButton*, 3>, kBandCount> m_rx1Buttons{};

    // Per-band RX-only antenna: button group per row
    std::array<QButtonGroup*, kBandCount> m_rxOnlyGroups{};
    std::array<std::array<QRadioButton*, 3>, kBandCount> m_rxOnlyButtons{};

    // Column sub-headers for RX-only grid — retargeted per SKU.
    // See SkuUiProfile::rxOnlyLabels.
    std::array<QLabel*, 3> m_rxOnlyColumnLabels{};

    // TX-bypass flag checkboxes (Phase 3P-I-b T7).
    // Source: Thetis setup.cs:6268-6273 (default Alex) / 6201-6250 (SKU overrides)
    // [v2.10.3.13 @501e3f5]. Visibility driven by SkuUiProfile; state bound to
    // AlexController.
    QCheckBox* m_chkRxOutOnTx     {nullptr};  // "RX Bypass on TX"
    QCheckBox* m_chkExt1OutOnTx   {nullptr};  // "Ext 1 on TX"
    QCheckBox* m_chkExt2OutOnTx   {nullptr};  // "Ext 2 on TX"
    QCheckBox* m_chkRxOutOverride {nullptr};  // "Disable RX Bypass relay"
    QCheckBox* m_chkUseTxAntForRx {nullptr};  // "Use TX antenna for RX"
};

} // namespace NereusSDR
