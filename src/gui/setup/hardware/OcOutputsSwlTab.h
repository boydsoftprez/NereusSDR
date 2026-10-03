#pragma once

// =================================================================
// src/gui/setup/hardware/OcOutputsSwlTab.h  (NereusSDR)
// =================================================================
//
// Ported from mi0bot-Thetis source:
//   Project Files/Source/Console/setup.designer.cs:tpOCSWLControl,
//                                                  grpExtCtrlSWL,
//                                                  btnCtrlSWLReset
//   (mi0bot v2.10.3.13-beta2 / @c26a8a4)
//
// SWL band per-pin RX/TX matrix for the HL2 N2ADR Filter board.  The
// HF amateur counterpart lives in OcOutputsHfTab.  When the user
// enables the N2ADR Filter master toggle on the HF tab, mi0bot's
// chkHERCULES handler (setup.cs:14324-14368) writes pin-7 entries to
// all 13 SWL bands here in addition to the 10 amateur bands on the
// HF tab.  Without this widget being real (it was a QLabel placeholder
// at OcOutputsTab.cpp:90-102 prior to Phase 3L), the user can see
// the ham-band auto-fill but not the SWL auto-fill — roughly 30%
// of what N2ADR is doing is invisible.
//
// Scope kept narrow vs HF tab:
//   - 13 SWL bands (Band::SwlFirst..SwlLast = 120m, 90m, 61m, 49m,
//     41m, 31m, 25m, 22m, 19m, 16m, 14m, 13m, 11m) × 7 pins
//   - RX matrix + TX matrix side by side, backed by OcMatrix
//   - Reset SWL-only button (clears just the 13 SWL band entries)
//   - NO Ext PA Control group (mi0bot grpExtPAControlSWL deferred —
//     SWL bands rarely have ext PA; can ship as follow-up if asked)
//   - NO Transmit Pin Action combos (mi0bot groupBoxTS19 deferred —
//     pin actions are global in OcMatrix, edited from the HF tab)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — New for Phase 3L HL2 Filter visibility brainstorm.
//                Replaces OcOutputsTab.cpp:90-102 QLabel stub
//                ("SWL band plan — coming in a follow-up commit") that
//                was scaffolded in Phase 3P-D Task 2.
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Anthropic Claude Code.
//   2026-09-23 - R-R3-46: transmit permission. J.J. Boyd (KG4VCF), AI-
//                 assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-46 / R-R3-49 (remote-window parity Task 13): the TX
//                 pins, pin actions and resets follow their own gates.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
//
// === Verbatim mi0bot setup.designer.cs header (Thetis upstream) ===
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
// =================================================================

#include <QWidget>
#include <array>

#include "models/Band.h"

class QCheckBox;
class QGroupBox;
class QPushButton;

namespace NereusSDR {

class RadioModel;
class OcMatrix;

// Number of SWL bands handled by this tab (13 — Band::SwlFirst..SwlLast).
// From mi0bot enums.cs:310-322 [v2.10.3.13-beta2].
constexpr int kSwlMatrixBandCount = kSwlBandCount;
// Number of OC pins per band (mirrors HF tab).
constexpr int kSwlMatrixPinCount  = 7;

class OcOutputsSwlTab : public QWidget {
    Q_OBJECT

public:
    explicit OcOutputsSwlTab(RadioModel* model, OcMatrix* ocMatrix,
                             QWidget* parent = nullptr);

    // Test seam: read the underlying matrix for unit assertions.
    bool rxPinCheckedForTest(int swlBandRow, int pin) const;
    bool txPinCheckedForTest(int swlBandRow, int pin) const;

    // R-R3-46 / R-R3-49 (parity Task 13): the TX pin matrix and the reset
    // (which clears TX pins too), closed while the radio is on the air in
    // either window, and in a remote window while its Core does not take
    // them (HardwarePage decides).
    void setTransmitPinsPermitted(bool permitted, const QString& reason);

private slots:
    void onMatrixChanged();
    void onResetClicked();

private:
    void buildMatrixGrid(QGroupBox* group, bool tx);
    void syncFromMatrix();

    // Maps a 0-based SWL row index (0..12) to the corresponding Band
    // enum value.  Row 0 = Band120m, row 12 = Band11m.
    static Band swlBandForRow(int row);

    RadioModel* m_model{nullptr};
    OcMatrix*   m_ocMatrix{nullptr};

    // RX / TX matrix grids: [swlBandRow 0-12][pinIdx 0-6]
    std::array<std::array<QCheckBox*, kSwlMatrixPinCount>,
               kSwlMatrixBandCount> m_rxPins{};
    std::array<std::array<QCheckBox*, kSwlMatrixPinCount>,
               kSwlMatrixBandCount> m_txPins{};

    QPushButton* m_resetButton{nullptr};
    QWidget*     m_txGroup{nullptr};

    // Re-entrancy guard for syncFromMatrix → setChecked → onMatrixChanged
    // ping-pong.  Mirrors OcOutputsHfTab::m_syncing.
    bool m_syncing{false};
};

} // namespace NereusSDR
