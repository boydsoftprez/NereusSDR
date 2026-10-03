// =================================================================
// src/gui/setup/hardware/OcOutputsSwlTab.cpp  (NereusSDR)
// =================================================================
//
// Ported from mi0bot-Thetis source:
//   Project Files/Source/Console/setup.designer.cs:tpOCSWLControl,
//                                                  grpExtCtrlSWL,
//                                                  btnCtrlSWLReset
//   (mi0bot v2.10.3.13-beta2 / @c26a8a4)
//
// See OcOutputsSwlTab.h for the full design + scope rationale.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — New for Phase 3L HL2 Filter visibility.  Replaces
//                Phase 3P-D Task 2 placeholder QLabel.
//                J.J. Boyd (KG4VCF), with AI-assisted transformation
//                via Anthropic Claude Code.
//   2026-09-23 - R-R3-46: TX pins and reset follow the transmit
//                 permission. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-25 - R-R3-46 / R-R3-49 (remote-window parity Task 13): the TX
//                 pins, pin actions and resets follow their own gates.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
//
//=================================================================
// setup.designer.cs (mi0bot fork)
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

#include "OcOutputsSwlTab.h"
#include "HardwareTransmitGate.h"

#include "core/OcMatrix.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace NereusSDR {

// From mi0bot setup.designer.cs:14362-14364, 20414-20426
// [v2.10.3.13-beta2] — tpOCSWLControl is the third sub-tab inside
// grpTransmitPinActionSWL (HF / VHF / SWL).
OcOutputsSwlTab::OcOutputsSwlTab(RadioModel* model, OcMatrix* ocMatrix,
                                 QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_ocMatrix(ocMatrix)
{
    auto* outer = new QVBoxLayout(this);
    outer->setSpacing(8);
    outer->setContentsMargins(8, 8, 8, 8);

    // Top row: RX matrix (left) + TX matrix (right), side by side.
    // From mi0bot grpExtCtrlSWL contents — split conceptually into RX
    // (chkOCrcv*) + TX (chkOCxmit*) for clarity vs the upstream single
    // group with both prefixes interleaved (setup.designer.cs:20968+).
    auto* matrixRow = new QHBoxLayout();
    matrixRow->setSpacing(10);

    auto* rxGroup = new QGroupBox(tr("SWL — RX OC matrix"), this);
    buildMatrixGrid(rxGroup, /*tx=*/false);
    matrixRow->addWidget(rxGroup, /*stretch=*/1);

    auto* txGroup = new QGroupBox(tr("SWL — TX OC matrix"), this);
    buildMatrixGrid(txGroup, /*tx=*/true);
    matrixRow->addWidget(txGroup, /*stretch=*/1);
    m_txGroup = txGroup;

    outer->addLayout(matrixRow, /*stretch=*/1);

    // Bottom row: SWL-only reset button.
    // From mi0bot setup.designer.cs:btnCtrlSWLReset [v2.10.3.13-beta2]
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();
    m_resetButton = new QPushButton(tr("Reset SWL OC pins"), this);
    m_resetButton->setToolTip(tr(
        "Clear all 13 SWL band entries × 7 pins (RX + TX).  "
        "Does not affect HF amateur band entries on the HF sub-tab."));
    connect(m_resetButton, &QPushButton::clicked,
            this, &OcOutputsSwlTab::onResetClicked);
    btnRow->addWidget(m_resetButton);
    outer->addLayout(btnRow);

    // Wire OcMatrix change notifications.  OcMatrix::changed() fires
    // whenever any band/pin entry mutates (HF or SWL or pin-action),
    // so we re-sync our SWL portion of the state defensively.
    if (m_ocMatrix) {
        connect(m_ocMatrix, &OcMatrix::changed,
                this, &OcOutputsSwlTab::onMatrixChanged);
        syncFromMatrix();
    }
}

// ── Static helpers ───────────────────────────────────────────────────────────

Band OcOutputsSwlTab::swlBandForRow(int row)
{
    // From mi0bot enums.cs:310-322 [v2.10.3.13-beta2] order:
    //   row 0 = B120M, row 12 = B11M.
    const int idx = static_cast<int>(Band::SwlFirst) + row;
    if (idx < static_cast<int>(Band::SwlFirst) ||
        idx > static_cast<int>(Band::SwlLast)) {
        return Band::SwlFirst;  // safe fallback (matches band 120m)
    }
    return static_cast<Band>(idx);
}

// ── buildMatrixGrid ──────────────────────────────────────────────────────────

void OcOutputsSwlTab::buildMatrixGrid(QGroupBox* group, bool tx)
{
    auto* grid = new QVBoxLayout(group);
    grid->setSpacing(1);
    grid->setContentsMargins(4, 4, 4, 4);

    // Column header row: "Band" | P1..P7
    {
        auto* hdrRow = new QHBoxLayout();
        auto* bandHdr = new QLabel(tr("Band"), group);
        bandHdr->setFixedWidth(52);
        hdrRow->addWidget(bandHdr);
        for (int pin = 0; pin < kSwlMatrixPinCount; ++pin) {
            auto* lbl = new QLabel(tr("P%1").arg(pin + 1), group);
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setFixedWidth(28);
            hdrRow->addWidget(lbl);
        }
        hdrRow->addStretch();
        grid->addLayout(hdrRow);
    }

    auto& dest = tx ? m_txPins : m_rxPins;

    for (int row = 0; row < kSwlMatrixBandCount; ++row) {
        const Band band = swlBandForRow(row);

        auto* bandRow = new QHBoxLayout();
        bandRow->setSpacing(0);

        auto* bandLbl = new QLabel(bandLabel(band), group);
        bandLbl->setFixedWidth(52);
        bandRow->addWidget(bandLbl);

        for (int pin = 0; pin < kSwlMatrixPinCount; ++pin) {
            auto* cb = new QCheckBox(group);
            cb->setFixedWidth(28);
            cb->setToolTip(tr("%1 OC pin %2, SWL band %3")
                               .arg(tx ? tr("TX") : tr("RX"))
                               .arg(pin + 1)
                               .arg(bandLabel(band)));
            dest[row][pin] = cb;

            // On user toggle, push the change to the shared OcMatrix.
            // Captures band/pin/tx by value for the lambda lifetime.
            connect(cb, &QCheckBox::toggled, this,
                    [this, band, pin, tx](bool checked) {
                        if (m_syncing) { return; }
                        if (m_ocMatrix) {
                            m_ocMatrix->setPin(band, pin, tx, checked);
                        }
                    });

            bandRow->addWidget(cb);
        }
        bandRow->addStretch();
        grid->addLayout(bandRow);
    }

    grid->addStretch();
}

// ── syncFromMatrix ───────────────────────────────────────────────────────────

void OcOutputsSwlTab::syncFromMatrix()
{
    if (!m_ocMatrix) { return; }

    m_syncing = true;
    for (int row = 0; row < kSwlMatrixBandCount; ++row) {
        const Band band = swlBandForRow(row);
        for (int pin = 0; pin < kSwlMatrixPinCount; ++pin) {
            if (m_rxPins[row][pin]) {
                m_rxPins[row][pin]->setChecked(
                    m_ocMatrix->pinEnabled(band, pin, /*tx=*/false));
            }
            if (m_txPins[row][pin]) {
                m_txPins[row][pin]->setChecked(
                    m_ocMatrix->pinEnabled(band, pin, /*tx=*/true));
            }
        }
    }
    m_syncing = false;
}

// ── onMatrixChanged ──────────────────────────────────────────────────────────

void OcOutputsSwlTab::onMatrixChanged()
{
    syncFromMatrix();
}

// ── onResetClicked ───────────────────────────────────────────────────────────

void OcOutputsSwlTab::onResetClicked()
{
    if (!m_ocMatrix) { return; }
    for (int row = 0; row < kSwlMatrixBandCount; ++row) {
        const Band band = swlBandForRow(row);
        for (int pin = 0; pin < kSwlMatrixPinCount; ++pin) {
            if (m_ocMatrix->pinEnabled(band, pin, /*tx=*/false)) {
                m_ocMatrix->setPin(band, pin, /*tx=*/false, false);
            }
            if (m_ocMatrix->pinEnabled(band, pin, /*tx=*/true)) {
                m_ocMatrix->setPin(band, pin, /*tx=*/true, false);
            }
        }
    }
    // setPin emits OcMatrix::changed which routes back into syncFromMatrix.
}

// ── Test seams ───────────────────────────────────────────────────────────────

bool OcOutputsSwlTab::rxPinCheckedForTest(int swlBandRow, int pin) const
{
    if (swlBandRow < 0 || swlBandRow >= kSwlMatrixBandCount) { return false; }
    if (pin < 0 || pin >= kSwlMatrixPinCount) { return false; }
    return m_rxPins[swlBandRow][pin] &&
           m_rxPins[swlBandRow][pin]->isChecked();
}

bool OcOutputsSwlTab::txPinCheckedForTest(int swlBandRow, int pin) const
{
    if (swlBandRow < 0 || swlBandRow >= kSwlMatrixBandCount) { return false; }
    if (pin < 0 || pin >= kSwlMatrixPinCount) { return false; }
    return m_txPins[swlBandRow][pin] &&
           m_txPins[swlBandRow][pin]->isChecked();
}

void OcOutputsSwlTab::setTransmitPinsPermitted(bool permitted, const QString& reason)
{
    HardwareTransmitGate::apply(m_txGroup, permitted, reason);
    HardwareTransmitGate::apply(m_resetButton, permitted, reason);
}

} // namespace NereusSDR
