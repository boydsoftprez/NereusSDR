#pragma once

// =================================================================
// src/gui/setup/hardware/OcOutputsHfTab.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.designer.cs:tpOCHFControl
//   (~lines 13658-13670 + nested groupboxes for chkPenOC{rcv,xmit}*,
//    grpTransmitPinActionHF, grpUSBBCD, grpExtPAControlHF, etc.)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Sub-sub-tab under Hardware → OC Outputs.
//                Persistence via OcMatrix model (Phase 3P-D Task 1).
//                NereusSDR spin: 14 bands (incl. GEN/WWV/XVTR) vs
//                Thetis's 12; GEN/WWV rows greyed by default.
//   2026-04-21 — Phase 3P-H Task 5b: Live OC pin-state LED row now
//                reflects OcMatrix::maskFor(currentBand, isTx) for
//                the current PanadapterModel band and TransmitModel
//                MOX state.
//   2026-09-23 - R-R3-46: transmit permission. J.J. Boyd (KG4VCF), AI-
//                 assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-46 / R-R3-49 (remote-window parity Task 13): the TX
//                 pins, pin actions and resets follow their own gates.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
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
//
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
// made by him, the copyright holder for those portions (Richard Samphire) reserves his       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "models/Band.h"

#include <QList>
#include <QWidget>
#include <array>
#include <vector>

class QCheckBox;
class QComboBox;
class QFrame;
class QGroupBox;
class QLabel;
class QPushButton;
class QScrollArea;
class QSpinBox;

namespace NereusSDR {

class RadioModel;
class OcMatrix;

// OcOutputsHfTab — HF sub-sub-tab under Hardware → OC Outputs.
//
// Ports Thetis setup.designer.cs:tpOCHFControl + nested groupboxes [@501e3f5].
// Variant: thetis-samphire.
//
// Layout (top → bottom):
//   Row 1: Master toggles — Penny Ext Control, N2ADR Filter (HERCULES),
//           Allow hot switching, [spacer], Reset OC defaults button
//   Row 2: RX OC matrix (blue) side-by-side with TX OC matrix (red)
//           14 bands × 7 pins each — backed by OcMatrix::pinEnabled/setPin
//   Row 3: TX Pin Action map | USB BCD | Ext PA | Live OC pin state
//
// NereusSDR spin vs Thetis:
//   - 14 bands (adds GEN/WWV/XVTR) — GEN and WWV rows greyed (no OC sense)
//   - State persisted via OcMatrix (per-MAC AppSettings) not directly via
//     AppSettings keys from this widget
//   - Live OC LED stubs — Phase H wires to ep6 status feed
class OcOutputsHfTab : public QWidget {
    Q_OBJECT

public:
    explicit OcOutputsHfTab(RadioModel* model, OcMatrix* ocMatrix,
                            QWidget* parent = nullptr);

    // Test seam: expose matrix state for unit tests
    bool rxPinCheckedForTest(int bandIdx, int pin) const;
    bool txPinCheckedForTest(int bandIdx, int pin) const;

    // Phase 3P-H Task 5b test seams.
    // Current OC byte displayed by the live LED row. Plan Task 14 fix wave
    // (R-R3-49): the byte the connection composed (RadioModel::
    // bandOutputsByte, the Core's in a remote window), 0 when none is known.
    quint8 currentOcByteForTest() const { return m_currentOcByte; }
    bool   livePinLitForTest(int pin) const;

    // Drives the live-LED row with an explicit OC byte; used by tests and
    // internally from onLiveStateChanged(). Exposed so test fixtures can
    // inject a byte without spinning a full RadioModel + band change.
    void setCurrentOcByte(quint8 byte);

    // R-R3-46: the external PA group and "Allow hot switching" (both hidden
    // until built) follow the transmit permission with its reason. Always
    // permitted locally.
    void setTransmitPermitted(bool permitted, const QString& reason);
    // R-R3-46 / R-R3-49 (parity Task 13): the TX pin matrix and Reset OC
    // defaults (which clears the TX pins too), closed while the radio is on
    // the air in either window, and in a remote window while its Core does
    // not take them (HardwarePage decides).
    void setTransmitPinsPermitted(bool permitted, const QString& reason);
    // R-R3-46 / R-R3-49 (parity Task 13): the TX pin actions, which Thetis
    // changes while transmitting; closed only in a remote window whose Core
    // does not take them.
    void setPinActionsPermitted(bool permitted, const QString& reason);

private slots:
    void onMatrixChanged();
    void onResetClicked();
    // Phase 3P-H Task 5b; plan Task 14 fix wave: show the composed byte.
    void onLiveStateChanged();

private:
    void buildMatrixGrid(QGroupBox* group, bool tx);
    void syncFromMatrix();
    // Phase 3P-H Task 5b: repaint the 7 pin-state LEDs from m_currentOcByte.
    void repaintLiveLeds();

    RadioModel* m_model{nullptr};
    OcMatrix*   m_ocMatrix{nullptr};

    // Master toggles (AppSettings keys under hardware/<mac>/oc/)
    // Issue #174: m_n2adrFilter removed — see OcOutputsHfTab.cpp:113.
    QList<QWidget*> m_transmitWidgets;
    QWidget*     m_txGroup{nullptr};
    QWidget*     m_actionGroup{nullptr};
    QPushButton* m_resetButton{nullptr};
    QCheckBox* m_pennyExtCtrl{nullptr};
    QCheckBox* m_allowHotSwitching{nullptr};

    // RX / TX matrix grids: [per-band state slot][pinIdx 0-6] (Band.h:
    // 160m .. XVTR at their numbers, 2 m at 14)
    static constexpr int kBandCount = kPerBandStateCount;
    static constexpr int kPinCount  = 7;

    std::array<std::array<QCheckBox*, kPinCount>, kBandCount> m_rxPins{};
    std::array<std::array<QCheckBox*, kPinCount>, kBandCount> m_txPins{};

    // TX pin action: [pinIdx 0-6] — one QCheckBox per (pin, action) pair
    // Layout: kPinCount rows × kActionCount cols
    static constexpr int kActionCount = 7;
    std::array<std::array<QCheckBox*, kActionCount>, kPinCount> m_actionChecks{};

    // USB BCD
    QCheckBox* m_usbBcdEnabled{nullptr};
    QComboBox* m_usbBcdFormat{nullptr};
    QCheckBox* m_usbBcdInvert{nullptr};

    // External PA
    QComboBox* m_extPaModel{nullptr};
    QSpinBox*  m_biasDelayMs{nullptr};

    // Live OC pin state LEDs: [pinIdx 0-6]
    std::array<QFrame*, kPinCount> m_leds{};

    // Guard against feedback loops between matrix changed() and checkbox toggled()
    bool m_syncing{false};

    // Phase 3P-H Task 5b: the OC byte the live-LED row shows; bit N == 1
    // means pin N lit. Plan Task 14 fix wave: this is the byte the
    // connection composed (RadioModel::bandOutputsByte), refreshed on
    // RadioModel::bandOutputsChanged and connectionStateChanged; 0 (nothing
    // lit) until a byte is known. The tab computes no byte of its own.
    quint8 m_currentOcByte{0};
};

} // namespace NereusSDR
