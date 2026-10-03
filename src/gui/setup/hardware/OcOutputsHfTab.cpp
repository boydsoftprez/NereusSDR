// =================================================================
// src/gui/setup/hardware/OcOutputsHfTab.cpp  (NereusSDR)
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
//   2026-09-23 - R-R3-46: TX pins, pin actions, external PA and reset
//                 follow the transmit permission. J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-23 - R-R3-21: Penny Ext Control reads and saves the radio's
//                 own key through PennyLaneController. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-46: "Allow hot switching" follows the transmit
//                permission. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-24 - R-R3-49: hot switching, USB BCD output and External PA
//                 control are hidden until they are applied
//                 (UnbuiltFeatures).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-46 / R-R3-49 (remote-window parity Task 13): the TX
//                 pins and the reset follow the transmit settings gate and
//                 the radio being on the air; the pin actions follow the
//                 gate alone. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-28 - 2 m as its own band (R-IOS-26, R-R3-49). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
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

#include "OcOutputsHfTab.h"
#include "HardwareTransmitGate.h"
#include "gui/UnbuiltFeatures.h"

#include "core/AppSettings.h"
#include "core/OcMatrix.h"
#include "core/accessories/PennyLaneController.h"
#include "gui/ComboStyle.h"
#include "models/Band.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

namespace NereusSDR {

// Human-readable labels for each TXPinAction value.
// Mirrors Thetis enums.cs:443-457 TXPinActions [@501e3f5].
static const char* kActionLabels[7] = {
    "MOX",             // TXPinAction::Mox
    "Tune",            // TXPinAction::Tune
    "TwoTone",         // TXPinAction::TwoTone
    "MOX+Tune",        // TXPinAction::MoxTune
    "MOX+TwoTone",     // TXPinAction::MoxTwoTone
    "Tune+TwoTone",    // TXPinAction::TuneTwoTone
    "MOX+Tune+TwoTone" // TXPinAction::MoxTuneTwoTone
};

// Returns true if the band should be greyed out (GEN/WWV have no OC sense)
static bool bandIsGrey(Band b)
{
    return b == Band::GEN || b == Band::WWV;
}

// The rows, top to bottom: 160m .. 6m, 2m, GEN, WWV, XVTR. 2 m has OC
// outputs of its own, as in Thetis (setup.cs:13087-13105 [v2.10.3.15],
// setBandABitMask(Band.B2M, ...)), and sits after 6 m as Thetis orders its
// bands (R-IOS-26). The matrices are indexed by the per-band state slot.
static constexpr std::array<Band, kPerBandStateCount> kRowOrder{
    Band::Band160m, Band::Band80m, Band::Band60m, Band::Band40m, Band::Band30m,
    Band::Band20m,  Band::Band17m, Band::Band15m, Band::Band12m, Band::Band10m,
    Band::Band6m,   Band::Band2m,  Band::GEN,     Band::WWV,     Band::XVTR,
};

// ── Constructor ──────────────────────────────────────────────────────────────

OcOutputsHfTab::OcOutputsHfTab(RadioModel* model, OcMatrix* ocMatrix,
                                QWidget* parent)
    : QWidget(parent)
    , m_model(model)
    , m_ocMatrix(ocMatrix)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(6);

    // ── Row 1: Master toggles ───────────────────────────────────────────────
    // Source: Thetis setup.designer.cs tpOCHFControl master checkboxes [@501e3f5]
    //
    // Issue #174 cleanup: removed the redundant "N2ADR Filter (HERCULES)"
    // checkbox.  It wrote to a global key (hardware/oc/n2adrFilter) that
    // had no consumer — the actual N2ADR control lives on the HL2 I/O
    // Board tab and writes to a per-MAC key (hardware/<mac>/hl2IoBoard/
    // n2adrFilter).  The dual surface confused users into toggling the
    // dead checkbox and concluding N2ADR was broken.  An italic hint
    // pointer is shown in its place to preserve discoverability.
    {
        auto* row = new QHBoxLayout();

        m_pennyExtCtrl = new QCheckBox(tr("Penny Ext Control enabled"), this);
        m_pennyExtCtrl->setToolTip(tr("Enable the Penelope/Hermes open-collector external control outputs"));
        row->addWidget(m_pennyExtCtrl);

        auto* n2adrHint = new QLabel(
            tr("N2ADR Filter (HL2): Hardware → HL2 I/O Board"),
            this);
        n2adrHint->setStyleSheet(QStringLiteral(
            "QLabel { color: #888; font-style: italic; font-size: 10px; }"));
        n2adrHint->setToolTip(tr(
            "On Hermes Lite 2, N2ADR filter board control is configured "
            "on the HL2 I/O Board tab."));
        row->addWidget(n2adrHint);

        m_allowHotSwitching = new QCheckBox(tr("Allow hot switching"), this);
        m_allowHotSwitching->setToolTip(tr("Allow OC output lines to switch while transmitting"));
        m_allowHotSwitching->setObjectName(QStringLiteral("ocAllowHotSwitching"));
        row->addWidget(m_allowHotSwitching);
        // R-R3-49: saved but not applied; hidden until it is.
        UnbuiltFeatures::hideUnlessBuilt(m_allowHotSwitching, UnbuiltFeature::OcExtras);
        // R-R3-46: OC lines switching while transmitting is a transmit setting.
        m_transmitWidgets.append(m_allowHotSwitching);

        row->addStretch();

        auto* resetBtn = new QPushButton(tr("Reset OC defaults"), this);
        resetBtn->setToolTip(tr("Reset all OC matrix pin assignments and pin actions to Thetis defaults"));
        row->addWidget(resetBtn);
        connect(resetBtn, &QPushButton::clicked, this, &OcOutputsHfTab::onResetClicked);
        // R-R3-46 / R-R3-49 (parity Task 13): the reset clears the TX pins
        // too, so it follows setTransmitPinsPermitted.
        m_resetButton = resetBtn;

        outerLayout->addLayout(row);
    }

    // ── Row 2: RX/TX matrix side by side ────────────────────────────────────
    // Source: Thetis setup.designer.cs chkPenOCrcv* + chkPenOCxmit* [@501e3f5]
    {
        auto* matrixRow = new QHBoxLayout();

        auto* rxGroup = new QGroupBox(tr("RX OC Pins per Band"), this);
        rxGroup->setStyleSheet(QStringLiteral(
            "QGroupBox { color: #6699ff; font-weight: bold; }"
        ));
        buildMatrixGrid(rxGroup, /*tx=*/false);
        matrixRow->addWidget(rxGroup, 1);

        auto* txGroup = new QGroupBox(tr("TX OC Pins per Band"), this);
        txGroup->setStyleSheet(QStringLiteral(
            "QGroupBox { color: #ff6655; font-weight: bold; }"
        ));
        buildMatrixGrid(txGroup, /*tx=*/true);
        matrixRow->addWidget(txGroup, 1);
        m_txGroup = txGroup;

        outerLayout->addLayout(matrixRow);
    }

    // ── Row 3: Pin actions + USB BCD + Ext PA + Live OC state ───────────────
    {
        auto* bottomRow = new QHBoxLayout();

        // ── TX Pin Action mapping ────────────────────────────────────────────
        // Source: Thetis setup.designer.cs grpTransmitPinActionHF [@501e3f5]
        // Each of the 7 pins gets ONE action (radio-button semantic via exclusive
        // QButtonGroup per pin row, but we use individual QCheckBoxes with mutual
        // exclusion handled in toggled() to match Thetis's single-select behaviour).
        {
            auto* actionGroup = new QGroupBox(tr("TX Pin Action mapping"), this);
            auto* grid = new QVBoxLayout(actionGroup);
            grid->setSpacing(2);

            // Header row: action column labels
            {
                auto* hdrRow = new QHBoxLayout();
                hdrRow->addWidget(new QLabel(tr("Pin"), actionGroup), 1);
                for (int a = 0; a < kActionCount; ++a) {
                    auto* lbl = new QLabel(QString::fromUtf8(kActionLabels[a]), actionGroup);
                    lbl->setAlignment(Qt::AlignCenter);
                    lbl->setWordWrap(true);
                    hdrRow->addWidget(lbl, 2);
                }
                grid->addLayout(hdrRow);
            }

            // One row per pin
            for (int pin = 0; pin < kPinCount; ++pin) {
                auto* pinRow = new QHBoxLayout();
                auto* pinLbl = new QLabel(tr("Pin %1").arg(pin + 1), actionGroup);
                pinRow->addWidget(pinLbl, 1);

                for (int a = 0; a < kActionCount; ++a) {
                    auto* cb = new QCheckBox(actionGroup);
                    cb->setToolTip(tr("Pin %1: %2").arg(pin + 1)
                                        .arg(QString::fromUtf8(kActionLabels[a])));
                    // Mutual-exclusion: only one action active per pin
                    connect(cb, &QCheckBox::toggled, this,
                            [this, pin, a](bool checked) {
                                if (m_syncing || !checked) { return; }
                                QSignalBlocker outerBlocker(this);
                                // Uncheck sibling actions on this pin
                                for (int other = 0; other < kActionCount; ++other) {
                                    if (other != a && m_actionChecks[pin][other]) {
                                        QSignalBlocker b(m_actionChecks[pin][other]);
                                        m_actionChecks[pin][other]->setChecked(false);
                                    }
                                }
                                // Write to matrix
                                m_ocMatrix->setPinAction(pin,
                                    static_cast<OcMatrix::TXPinAction>(a));
                            });
                    m_actionChecks[pin][a] = cb;
                    pinRow->addWidget(cb, 2, Qt::AlignHCenter);
                }
                grid->addLayout(pinRow);
            }

            bottomRow->addWidget(actionGroup, 3);
            m_actionGroup = actionGroup;
        }

        // ── USB BCD output ───────────────────────────────────────────────────
        // Source: Thetis setup.designer.cs grpUSBBCD [@501e3f5]
        {
            auto* bcdGroup = new QGroupBox(tr("USB BCD output"), this);
            bcdGroup->setObjectName(QStringLiteral("ocUsbBcdGroup"));
            // R-R3-49: saved but not applied; hidden until it is.
            UnbuiltFeatures::hideUnlessBuilt(bcdGroup, UnbuiltFeature::OcExtras);
            auto* bcdLayout = new QVBoxLayout(bcdGroup);

            m_usbBcdEnabled = new QCheckBox(tr("Enable BCD"), bcdGroup);
            bcdLayout->addWidget(m_usbBcdEnabled);

            auto* fmtRow = new QHBoxLayout();
            fmtRow->addWidget(new QLabel(tr("Format:"), bcdGroup));
            m_usbBcdFormat = new QComboBox(bcdGroup);
            applyComboStyle(m_usbBcdFormat);
            m_usbBcdFormat->setMinimumWidth(120);
            m_usbBcdFormat->setMaximumWidth(160);
            m_usbBcdFormat->addItem(tr("4-bit binary"));
            m_usbBcdFormat->addItem(tr("BCD"));
            fmtRow->addWidget(m_usbBcdFormat);
            bcdLayout->addLayout(fmtRow);

            m_usbBcdInvert = new QCheckBox(tr("Invert"), bcdGroup);
            bcdLayout->addWidget(m_usbBcdInvert);

            bcdLayout->addStretch();

            // Wire BCD controls → AppSettings (direct; not routed via OcMatrix)
            connect(m_usbBcdEnabled, &QCheckBox::toggled, this, [this](bool v) {
                if (m_syncing) { return; }
                AppSettings::instance().setValue(
                    QStringLiteral("hardware/oc/usbBcd/enabled"), v);
            });
            connect(m_usbBcdFormat, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, [this](int idx) {
                        if (m_syncing) { return; }
                        AppSettings::instance().setValue(
                            QStringLiteral("hardware/oc/usbBcd/format"), idx);
                    });
            connect(m_usbBcdInvert, &QCheckBox::toggled, this, [this](bool v) {
                if (m_syncing) { return; }
                AppSettings::instance().setValue(
                    QStringLiteral("hardware/oc/usbBcd/invert"), v);
            });

            bottomRow->addWidget(bcdGroup, 1);
        }

        // ── External PA control ──────────────────────────────────────────────
        // Source: Thetis setup.designer.cs grpExtPAControlHF [@501e3f5]
        {
            auto* paGroup = new QGroupBox(tr("External PA control"), this);
            paGroup->setObjectName(QStringLiteral("ocExternalPaGroup"));
            // R-R3-49: saved but not applied; hidden until it is.
            UnbuiltFeatures::hideUnlessBuilt(paGroup, UnbuiltFeature::OcExtras);
            auto* paLayout = new QVBoxLayout(paGroup);

            auto* modelRow = new QHBoxLayout();
            modelRow->addWidget(new QLabel(tr("PA model:"), paGroup));
            m_extPaModel = new QComboBox(paGroup);
            applyComboStyle(m_extPaModel);
            m_extPaModel->setMinimumWidth(120);
            m_extPaModel->setMaximumWidth(160);
            m_extPaModel->addItem(tr("None"));
            // Future entries (Phase H) will populate from a PA registry
            modelRow->addWidget(m_extPaModel);
            paLayout->addLayout(modelRow);

            auto* delayRow = new QHBoxLayout();
            delayRow->addWidget(new QLabel(tr("Bias delay (ms):"), paGroup));
            m_biasDelayMs = new QSpinBox(paGroup);
            m_biasDelayMs->setRange(0, 100);
            m_biasDelayMs->setValue(5);
            m_biasDelayMs->setSuffix(tr(" ms"));
            delayRow->addWidget(m_biasDelayMs);
            paLayout->addLayout(delayRow);

            paLayout->addStretch();

            // Wire PA controls → AppSettings
            connect(m_extPaModel, QOverload<int>::of(&QComboBox::currentIndexChanged),
                    this, [this](int idx) {
                        if (m_syncing) { return; }
                        AppSettings::instance().setValue(
                            QStringLiteral("hardware/oc/extPa/model"), idx);
                    });
            connect(m_biasDelayMs, QOverload<int>::of(&QSpinBox::valueChanged),
                    this, [this](int v) {
                        if (m_syncing) { return; }
                        AppSettings::instance().setValue(
                            QStringLiteral("hardware/oc/extPa/biasDelayMs"), v);
                    });

            bottomRow->addWidget(paGroup, 1);
            m_transmitWidgets.append(paGroup);
        }

        // ── Live OC pin state ────────────────────────────────────────────────
        // Phase H wires these to the actual ep6 OC byte from the radio.
        // For now: 7 grey LED stubs, one per pin.
        {
            auto* ledGroup = new QGroupBox(tr("Live OC pin state"), this);
            auto* ledLayout = new QVBoxLayout(ledGroup);

            auto* ledRow = new QHBoxLayout();
            for (int pin = 0; pin < kPinCount; ++pin) {
                auto* pinCol = new QVBoxLayout();
                auto* led = new QFrame(ledGroup);
                led->setFixedSize(12, 12);
                // Grey stub — Phase H will set to lit-green when OC pin is high
                led->setStyleSheet(QStringLiteral(
                    "background: rgba(255,255,255,0.1);"
                    "border: 1px solid rgba(255,255,255,0.2);"
                    "border-radius: 6px;"));
                led->setToolTip(tr("OC pin %1: shows the last OC byte sent to the radio").arg(pin + 1));
                m_leds[pin] = led;
                pinCol->addWidget(led, 0, Qt::AlignHCenter);
                pinCol->addWidget(new QLabel(tr("%1").arg(pin + 1), ledGroup), 0, Qt::AlignHCenter);
                ledRow->addLayout(pinCol);
            }
            ledLayout->addLayout(ledRow);

            auto* noteLbl = new QLabel(
                tr("<i>Reflects the last C&amp;C OC byte sent to the radio</i>"),
                ledGroup);
            noteLbl->setWordWrap(true);
            ledLayout->addWidget(noteLbl);
            ledLayout->addStretch();

            bottomRow->addWidget(ledGroup, 1);
        }

        outerLayout->addLayout(bottomRow);
    }

    // ── Wire master toggles → AppSettings ────────────────────────────────────
    // Issue #174: removed n2adrFilter writer — see Row 1 cleanup notes.
    // R-R3-21: the checkbox mirrors PennyLaneController, which saves under
    // the radio's own hardware/<mac>/penny/extCtrlEnabled. It used to write
    // a global hardware/oc/pennyExtCtrl that nothing read (the controller
    // adopts that old value once; see PennyLaneController::load).
    if (m_model) {
        PennyLaneController& penny = m_model->pennyLaneControllerMutable();
        {
            QSignalBlocker block(m_pennyExtCtrl);
            m_pennyExtCtrl->setChecked(penny.extCtrlEnabled());
        }
        connect(&penny, &PennyLaneController::extCtrlEnabledChanged,
                m_pennyExtCtrl, [this](bool on) {
            QSignalBlocker block(m_pennyExtCtrl);
            m_pennyExtCtrl->setChecked(on);
        });
        connect(m_pennyExtCtrl, &QCheckBox::toggled, this, [this](bool v) {
            if (m_syncing || !m_model) { return; }
            PennyLaneController& p = m_model->pennyLaneControllerMutable();
            p.setExtCtrlEnabled(v);
            p.save();
        });
    }
    connect(m_allowHotSwitching, &QCheckBox::toggled, this, [this](bool v) {
        if (m_syncing) { return; }
        AppSettings::instance().setValue(
            QStringLiteral("hardware/oc/allowHotSwitching"), v);
    });

    // ── Wire OcMatrix::changed() → UI re-sync ────────────────────────────────
    if (m_ocMatrix) {
        connect(m_ocMatrix, &OcMatrix::changed,
                this, &OcOutputsHfTab::onMatrixChanged);
        syncFromMatrix();
    }

    // ── Phase 3P-H Task 5b: live OC pin state wiring ────────────────────────
    // Plan Task 14 fix wave (R-R3-49): the row shows the byte the connection
    // composed (RadioModel::bandOutputsByte), not one computed here. It was
    // OcMatrix::maskFor(pan 1's band, MOX), which in a cross-band split is
    // the other slice's pins, and ignores the HL2's receive bypass. Thetis
    // shows the bits UpdateExtCtrl returned:
    //   UpdateOCLedStrip(_mox, bits) (console.cs:29106-29107 [v2.10.3.15]).
    // In a remote window the model carries the Core's byte, so the row
    // shows what the radio gets there too.
    if (m_model) {
        connect(m_model, &RadioModel::bandOutputsChanged,
                this, &OcOutputsHfTab::onLiveStateChanged);
        connect(m_model, &RadioModel::connectionStateChanged,
                this, &OcOutputsHfTab::onLiveStateChanged);
    }
    // Initial paint.
    onLiveStateChanged();
}

// ── buildMatrixGrid ───────────────────────────────────────────────────────────

void OcOutputsHfTab::buildMatrixGrid(QGroupBox* group, bool tx)
{
    auto* scroll = new QScrollArea(group);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* inner = new QWidget();
    auto* grid = new QVBoxLayout(inner);
    grid->setSpacing(1);
    grid->setContentsMargins(2, 2, 2, 2);

    // Column header row: "Band" + "P1" … "P7"
    {
        auto* hdrRow = new QHBoxLayout();
        auto* bandHdr = new QLabel(tr("Band"), inner);
        bandHdr->setFixedWidth(52);
        hdrRow->addWidget(bandHdr);
        for (int pin = 0; pin < kPinCount; ++pin) {
            auto* lbl = new QLabel(tr("P%1").arg(pin + 1), inner);
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setFixedWidth(28);
            hdrRow->addWidget(lbl);
        }
        grid->addLayout(hdrRow);
    }

    // One row per band
    auto& dest = tx ? m_txPins : m_rxPins;

    for (const Band band : kRowOrder) {
        const int bi = perBandStateSlot(band);
        bool grey = bandIsGrey(band);

        auto* bandRow = new QHBoxLayout();
        bandRow->setSpacing(0);

        auto* bandLbl = new QLabel(bandLabel(band), inner);
        bandLbl->setFixedWidth(52);
        if (grey) {
            bandLbl->setEnabled(false);
        }
        bandRow->addWidget(bandLbl);

        for (int pin = 0; pin < kPinCount; ++pin) {
            auto* cb = new QCheckBox(inner);
            cb->setFixedWidth(28);
            cb->setEnabled(!grey);
            cb->setToolTip(tr("%1 OC pin %2, band %3")
                               .arg(tx ? tr("TX") : tr("RX"))
                               .arg(pin + 1)
                               .arg(bandLabel(band)));
            dest[bi][pin] = cb;

            // Capture band/pin/tx for the lambda (by value)
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
    scroll->setWidget(inner);

    auto* groupLayout = new QVBoxLayout(group);
    groupLayout->addWidget(scroll);
}

// ── syncFromMatrix ────────────────────────────────────────────────────────────

void OcOutputsHfTab::syncFromMatrix()
{
    if (!m_ocMatrix) { return; }

    m_syncing = true;

    // Sync RX / TX matrices
    for (int bi = 0; bi < kBandCount; ++bi) {
        auto band = bandFromPerBandStateSlot(bi);
        for (int pin = 0; pin < kPinCount; ++pin) {
            if (m_rxPins[bi][pin]) {
                m_rxPins[bi][pin]->setChecked(m_ocMatrix->pinEnabled(band, pin, false));
            }
            if (m_txPins[bi][pin]) {
                m_txPins[bi][pin]->setChecked(m_ocMatrix->pinEnabled(band, pin, true));
            }
        }
    }

    // Sync pin action checkboxes — one column active per pin row
    for (int pin = 0; pin < kPinCount; ++pin) {
        int activeAction = static_cast<int>(m_ocMatrix->pinAction(pin));
        for (int a = 0; a < kActionCount; ++a) {
            if (m_actionChecks[pin][a]) {
                m_actionChecks[pin][a]->setChecked(a == activeAction);
            }
        }
    }

    m_syncing = false;
}

// ── onMatrixChanged ───────────────────────────────────────────────────────────

void OcOutputsHfTab::onMatrixChanged()
{
    syncFromMatrix();
    // The live row follows the connection: a pin edit reaches the byte it
    // composes (and, on Protocol 2, is sent at once), which then arrives
    // here through bandOutputsChanged.
}

// ── onLiveStateChanged (Phase 3P-H Task 5b) ──────────────────────────────────

// Shows the OC byte the connection composed. Plan Task 14 fix wave: this
// no longer computes a byte from the matrix, a band and MOX; nothing is lit
// until a byte is known.
void OcOutputsHfTab::onLiveStateChanged()
{
    if (!m_model || !m_model->bandOutputsKnown()) { setCurrentOcByte(0); return; }
    setCurrentOcByte(static_cast<quint8>(m_model->bandOutputsByte()));
}

// ── setCurrentOcByte / repaintLiveLeds (Phase 3P-H Task 5b) ──────────────────

void OcOutputsHfTab::setCurrentOcByte(quint8 byte)
{
    m_currentOcByte = byte;
    repaintLiveLeds();
}

bool OcOutputsHfTab::livePinLitForTest(int pin) const
{
    if (pin < 0 || pin >= kPinCount) { return false; }
    return (m_currentOcByte >> pin) & 0x01;
}

void OcOutputsHfTab::repaintLiveLeds()
{
    for (int pin = 0; pin < kPinCount; ++pin) {
        if (!m_leds[pin]) { continue; }
        const bool lit = (m_currentOcByte >> pin) & 0x01;
        if (lit) {
            m_leds[pin]->setStyleSheet(QStringLiteral(
                "background: #00cc44;"
                "border: 1px solid #33dd55;"
                "border-radius: 6px;"));
        } else {
            m_leds[pin]->setStyleSheet(QStringLiteral(
                "background: rgba(255,255,255,0.1);"
                "border: 1px solid rgba(255,255,255,0.2);"
                "border-radius: 6px;"));
        }
    }
}

// ── onResetClicked ────────────────────────────────────────────────────────────

void OcOutputsHfTab::onResetClicked()
{
    if (!m_ocMatrix) { return; }

    auto reply = QMessageBox::question(
        this, tr("Reset OC defaults"),
        tr("Reset all OC pin assignments and TX pin actions to Thetis defaults?\n\n"
           "All custom band/pin mappings will be cleared."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        m_ocMatrix->resetDefaults();  // fires changed() → syncFromMatrix()
    }
}

// ── Test seams ───────────────────────────────────────────────────────────────

bool OcOutputsHfTab::rxPinCheckedForTest(int bandIdx, int pin) const
{
    if (bandIdx < 0 || bandIdx >= kBandCount) { return false; }
    if (pin < 0 || pin >= kPinCount) { return false; }
    auto* cb = m_rxPins[bandIdx][pin];
    return cb ? cb->isChecked() : false;
}

bool OcOutputsHfTab::txPinCheckedForTest(int bandIdx, int pin) const
{
    if (bandIdx < 0 || bandIdx >= kBandCount) { return false; }
    if (pin < 0 || pin >= kPinCount) { return false; }
    auto* cb = m_txPins[bandIdx][pin];
    return cb ? cb->isChecked() : false;
}

void OcOutputsHfTab::setTransmitPermitted(bool permitted, const QString& reason)
{
    for (QWidget* w : std::as_const(m_transmitWidgets)) {
        HardwareTransmitGate::apply(w, permitted, reason);
    }
}

void OcOutputsHfTab::setTransmitPinsPermitted(bool permitted, const QString& reason)
{
    HardwareTransmitGate::apply(m_txGroup, permitted, reason);
    HardwareTransmitGate::apply(m_resetButton, permitted, reason);
}

void OcOutputsHfTab::setPinActionsPermitted(bool permitted, const QString& reason)
{
    HardwareTransmitGate::apply(m_actionGroup, permitted, reason);
}

} // namespace NereusSDR
