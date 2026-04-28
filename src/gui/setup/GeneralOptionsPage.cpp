// =================================================================
// src/gui/setup/GeneralOptionsPage.cpp  (NereusSDR)
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

#include "GeneralOptionsPage.h"
#include "models/RadioModel.h"
#include "core/BoardCapabilities.h"
#include "core/StepAttenuatorController.h"
#include "core/AppSettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>

namespace NereusSDR {

namespace {

void applyDarkStyle(QWidget* w)
{
    w->setStyleSheet(QStringLiteral(
        "QGroupBox { color: #8090a0; font-size: 11px;"
        "  border: 1px solid #203040; border-radius: 4px;"
        "  margin-top: 8px; padding-top: 4px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QLabel { color: #c8d8e8; }"
        "QComboBox { background: #1a2a3a; color: #c8d8e8; border: 1px solid #203040;"
        "  border-radius: 3px; padding: 2px 6px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #1a2a3a; color: #c8d8e8;"
        "  selection-background-color: #00b4d8; }"
        "QSpinBox { background: #1a2a3a; color: #c8d8e8;"
        "  border: 1px solid #203040; border-radius: 3px; padding: 1px 4px; }"
        // Up/down buttons: rely on Fusion + app-level dark palette
        // (see main.cpp / AppTheme.h). Styling the subcontrols here
        // would erase the native arrow images.
        "QCheckBox { color: #c8d8e8; }"
        "QCheckBox::indicator { width: 14px; height: 14px; background: #1a2a3a;"
        "  border: 1px solid #203040; border-radius: 2px; }"
        "QCheckBox::indicator:checked { background: #00b4d8; border-color: #00b4d8; }"
    ));
}

// Helper: create a dB spinbox (0-31, suffix " dB", width 80).
QSpinBox* makeDbSpinBox(QWidget* parent)
{
    auto* spn = new QSpinBox(parent);
    spn->setRange(0, 31);
    spn->setSuffix(QStringLiteral(" dB"));
    spn->setFixedWidth(80);
    return spn;
}

// Helper: create an auto-att mode combo (Classic/Adaptive, width 100).
QComboBox* makeModeCombo(QWidget* parent)
{
    auto* cmb = new QComboBox(parent);
    cmb->addItem(QStringLiteral("Classic"));
    cmb->addItem(QStringLiteral("Adaptive"));
    cmb->setFixedWidth(100);
    // NereusSDR native — Classic mirrors Thetis bump+stack, Adaptive adds
    // 1 dB/tick attack with hold/decay and per-band floor memory.
    cmb->setToolTip(QStringLiteral(
        "Classic: bump ATT on red overload, stack-based undo.\n"
        "Adaptive: 1 dB/tick attack, configurable hold/decay, per-band memory."));
    return cmb;
}

// Helper: create hold-seconds spinbox (1-3600, default 5, suffix " sec").
QSpinBox* makeHoldSpinBox(QWidget* parent)
{
    auto* spn = new QSpinBox(parent);
    spn->setRange(1, 3600);
    spn->setValue(5);
    spn->setSuffix(QStringLiteral(" sec"));
    spn->setFixedWidth(80);
    return spn;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// GeneralOptionsPage
// ---------------------------------------------------------------------------

GeneralOptionsPage::GeneralOptionsPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Options"), model, parent)
{
    applyDarkStyle(this);
    m_ctrl = model ? model->stepAttController() : nullptr;

    buildHardwareConfigGroup();
    buildOptionsGroup();
    buildStepAttGroup();
    buildAutoAttGroup();

    // 3M-1a G.2: wire Receive Only checkbox visibility from caps.isRxOnlySku.
    // Hidden by default (see buildHardwareConfigGroup); shown only for RX-only
    // SKUs (HL2-RX, etc.).  Named slot mirrors HardwarePage::onCurrentRadioChanged.
    // Cite: Thetis setup.designer.cs:8535-8544 [v2.10.3.13] (Visible=false default);
    //       BoardCapabilities::isRxOnlySku (NereusSDR-original).
    if (model) {
        // Initial state: apply caps of the already-connected radio (if any).
        setReceiveOnlyVisible(model->boardCapabilities().isRxOnlySku);

        // Live updates: reconnects to a different radio (e.g. HL2-RX → ANAN-G2)
        // must flip visibility without reopening Setup.
        connect(model, &RadioModel::currentRadioChanged,
                this, &GeneralOptionsPage::onCurrentRadioChanged);
    }

    if (m_ctrl) {
        // Re-range setup spinboxes from board capabilities (may be 61 dB).
        int maxDb = m_ctrl->maxAttenuation();
        m_spnRx1StepAttValue->setRange(0, maxDb);
        m_spnRx2StepAttValue->setRange(0, maxDb);
        connectController();
    }
}

// ---------------------------------------------------------------------------
// setReceiveOnlyVisible
// ---------------------------------------------------------------------------
// 3M-1a G.2: public setter so RadioModel (via currentRadioChanged) and the
// constructor can show/hide the RX-only checkbox without direct access to
// the private m_chkGeneralRXOnly member.
// Cite: Thetis setup.designer.cs:8535-8544 [v2.10.3.13] (Visible=false default);
//       BoardCapabilities::isRxOnlySku (NereusSDR-original).

void GeneralOptionsPage::setReceiveOnlyVisible(bool visible)
{
    if (m_chkGeneralRXOnly) {
        m_chkGeneralRXOnly->setVisible(visible);
    }
}

// ---------------------------------------------------------------------------
// onCurrentRadioChanged — named slot, mirrors HardwarePage::onCurrentRadioChanged
// ---------------------------------------------------------------------------
// 3M-1a G.2 fixup: replaces the lambda that captured 'model' by pointer.
// The named slot form is auto-disconnected when 'this' dies (Qt::AutoConnection),
// with no shutdown-race on pointer capture.  m_model is the SetupPage base member.

void GeneralOptionsPage::onCurrentRadioChanged(const NereusSDR::RadioInfo& /*info*/)
{
    if (model()) {
        setReceiveOnlyVisible(model()->boardCapabilities().isRxOnlySku);
    }
}

// ---------------------------------------------------------------------------
// Hardware Configuration group
// From Thetis setup.designer.cs:8045-8396 [v2.10.3.13] (tpGeneralHardware)
// Controls: comboFRSRegion, chkExtended, lblWarningRegionExtended,
//           chkGeneralRXOnly (hidden), chkNetworkWDT (default ON).
// ---------------------------------------------------------------------------

void GeneralOptionsPage::buildHardwareConfigGroup()
{
    auto* group = new QGroupBox(tr("Hardware Configuration"), this);
    group->setObjectName(QStringLiteral("grpHardwareConfig"));
    auto* vbox = new QVBoxLayout(group);
    vbox->setSpacing(6);

    // --- Region combo ---
    // From Thetis setup.designer.cs:8080-8114 [v2.10.3.13]
    auto* regionRow = new QHBoxLayout;
    auto* regionLabel = new QLabel(tr("Region:"), group);
    m_comboFRSRegion = new QComboBox(group);
    m_comboFRSRegion->setObjectName(QStringLiteral("comboFRSRegion"));
    // From Thetis setup.designer.cs:8084-8108 [v2.10.3.13] — 24 entries
    m_comboFRSRegion->addItems({
        QStringLiteral("Australia"),
        QStringLiteral("Europe"),
        QStringLiteral("India"),
        QStringLiteral("Italy"),
        QStringLiteral("Israel"),
        QStringLiteral("Japan"),
        QStringLiteral("Spain"),
        QStringLiteral("United Kingdom"),
        QStringLiteral("United States"),
        QStringLiteral("Norway"),
        QStringLiteral("Denmark"),
        QStringLiteral("Sweden"),
        QStringLiteral("Latvia"),
        QStringLiteral("Slovakia"),
        QStringLiteral("Bulgaria"),
        QStringLiteral("Greece"),
        QStringLiteral("Hungary"),
        QStringLiteral("Netherlands"),
        QStringLiteral("France"),
        QStringLiteral("Russia"),
        QStringLiteral("Region1"),
        QStringLiteral("Region2"),
        QStringLiteral("Region3"),
        QStringLiteral("Germany"),
    });
    // From Thetis setup.designer.cs:8113 [v2.10.3.13]
    m_comboFRSRegion->setToolTip(QStringLiteral("Select Region for your location"));

    // Restore persisted value; default "United States"
    auto& s = AppSettings::instance();
    const QString savedRegion = s.value(QStringLiteral("Region"),
                                        QStringLiteral("United States")).toString();
    const int regionIdx = m_comboFRSRegion->findText(savedRegion);
    m_comboFRSRegion->setCurrentIndex(regionIdx >= 0 ? regionIdx : m_comboFRSRegion->findText(QStringLiteral("United States")));

    connect(m_comboFRSRegion, &QComboBox::currentTextChanged, this, [](const QString& text) {
        AppSettings::instance().setValue(QStringLiteral("Region"), text);
    });

    regionRow->addWidget(regionLabel);
    regionRow->addWidget(m_comboFRSRegion);
    regionRow->addStretch();
    vbox->addLayout(regionRow);

    // --- Extended checkbox ---
    // From Thetis setup.designer.cs:8065-8074 [v2.10.3.13]
    m_chkExtended = new QCheckBox(tr("Extended"), group);
    m_chkExtended->setObjectName(QStringLiteral("chkExtended"));
    m_chkExtended->setToolTip(QStringLiteral("Enable extended TX (out of band)"));
    m_chkExtended->setChecked(
        s.value(QStringLiteral("ExtendedTxAllowed"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkExtended, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("ExtendedTxAllowed"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    vbox->addWidget(m_chkExtended);

    // --- Warning label ---
    // From Thetis setup.designer.cs:8045-8054 [v2.10.3.13]
    m_lblWarningRegionExtended = new QLabel(
        tr("Changing this setting will reset your band stack entries"), group);
    m_lblWarningRegionExtended->setObjectName(QStringLiteral("lblWarningRegionExtended"));
    m_lblWarningRegionExtended->setStyleSheet(QStringLiteral("color: red; font-weight: bold;"));
    m_lblWarningRegionExtended->setWordWrap(true);
    vbox->addWidget(m_lblWarningRegionExtended);

    // --- Receive Only checkbox (hidden by default) ---
    // From Thetis setup.designer.cs:8535-8544 [v2.10.3.13] — Visible=false
    m_chkGeneralRXOnly = new QCheckBox(tr("Receive Only"), group);
    m_chkGeneralRXOnly->setObjectName(QStringLiteral("chkGeneralRXOnly"));
    m_chkGeneralRXOnly->setToolTip(QStringLiteral("Check to disable transmit functionality."));
    m_chkGeneralRXOnly->setChecked(
        s.value(QStringLiteral("RxOnly"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    m_chkGeneralRXOnly->setVisible(false);  // per-board visibility set via setReceiveOnlyVisible() — 3M-1a G.2
    connect(m_chkGeneralRXOnly, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("RxOnly"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    vbox->addWidget(m_chkGeneralRXOnly);

    // --- Network Watchdog checkbox (default ON) ---
    // From Thetis setup.designer.cs:8385-8395 [v2.10.3.13] — Checked=true
    m_chkNetworkWDT = new QCheckBox(tr("Network Watchdog"), group);
    m_chkNetworkWDT->setObjectName(QStringLiteral("chkNetworkWDT"));
    m_chkNetworkWDT->setToolTip(QStringLiteral("Resets software/firmware if network becomes inactive."));
    // Default ON — first-launch loads "True"
    m_chkNetworkWDT->setChecked(
        s.value(QStringLiteral("NetworkWatchdogEnabled"), QStringLiteral("True")).toString() == QStringLiteral("True"));
    connect(m_chkNetworkWDT, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("NetworkWatchdogEnabled"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    vbox->addWidget(m_chkNetworkWDT);

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// Options group
// From Thetis setup.designer.cs:9050-9059 [v2.10.3.13] (grpGeneralOptions)
// Controls: chkPreventTXonDifferentBandToRX
// ---------------------------------------------------------------------------

void GeneralOptionsPage::buildOptionsGroup()
{
    auto* group = new QGroupBox(tr("Options"), this);
    group->setObjectName(QStringLiteral("grpGeneralOptions"));
    auto* vbox = new QVBoxLayout(group);
    vbox->setSpacing(6);

    // From Thetis setup.designer.cs:9050-9059 [v2.10.3.13]
    // Note: tooltip is NereusSDR-original — Thetis has no tooltip on this control.
    m_chkPreventTXonDifferentBandToRX = new QCheckBox(
        tr("Prevent TX'ing on a different band to the RX band"), group);
    m_chkPreventTXonDifferentBandToRX->setObjectName(QStringLiteral("chkPreventTXonDifferentBandToRX"));
    m_chkPreventTXonDifferentBandToRX->setToolTip(
        QStringLiteral("When checked, MOX is rejected if the TX VFO is on a different band than the RX VFO"));
    m_chkPreventTXonDifferentBandToRX->setChecked(
        AppSettings::instance().value(QStringLiteral("PreventTxOnDifferentBandToRx"),
                                       QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkPreventTXonDifferentBandToRX, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("PreventTxOnDifferentBandToRx"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    vbox->addWidget(m_chkPreventTXonDifferentBandToRX);

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// Step Attenuator group
// From Thetis setup.cs: grpHermesStepAttenuator
// ---------------------------------------------------------------------------

void GeneralOptionsPage::buildStepAttGroup()
{
    auto* group = new QGroupBox(QStringLiteral("Step Attenuator"), this);
    auto* vbox  = new QVBoxLayout(group);
    vbox->setSpacing(6);

    // --- RX1 row ---
    auto* rx1Row = new QHBoxLayout;
    m_chkRx1StepAttEnable = new QCheckBox(QStringLiteral("RX1 Enable"), group);
    // From Thetis setup.cs: chkHermesStepAttenuator
    m_chkRx1StepAttEnable->setToolTip(QStringLiteral("Enable the step attenuator."));
    m_spnRx1StepAttValue = makeDbSpinBox(group);
    m_spnRx1StepAttValue->setEnabled(false);
    rx1Row->addWidget(m_chkRx1StepAttEnable);
    rx1Row->addWidget(m_spnRx1StepAttValue);
    rx1Row->addStretch();
    vbox->addLayout(rx1Row);

    // --- RX2 row ---
    auto* rx2Row = new QHBoxLayout;
    m_chkRx2StepAttEnable = new QCheckBox(QStringLiteral("RX2 Enable"), group);
    // From Thetis setup.cs: chkHermesStepAttenuator (RX2 mirror)
    m_chkRx2StepAttEnable->setToolTip(QStringLiteral("Enable the step attenuator."));
    m_spnRx2StepAttValue = makeDbSpinBox(group);
    m_spnRx2StepAttValue->setEnabled(false);
    rx2Row->addWidget(m_chkRx2StepAttEnable);
    rx2Row->addWidget(m_spnRx2StepAttValue);
    rx2Row->addStretch();
    vbox->addLayout(rx2Row);

    // --- ADC linked label ---
    m_lblAdcLinked = new QLabel(QStringLiteral("ADC linked — both RX share the same ADC"), group);
    m_lblAdcLinked->setStyleSheet(QStringLiteral("color: #ff4444; font-weight: bold;"));
    m_lblAdcLinked->setVisible(false);
    vbox->addWidget(m_lblAdcLinked);

    // --- Enable/disable cascade ---
    connect(m_chkRx1StepAttEnable, &QCheckBox::toggled, this, [this](bool on) {
        m_spnRx1StepAttValue->setEnabled(on);
        if (m_ctrl) {
            m_ctrl->setStepAttEnabled(on);
        }
    });
    connect(m_chkRx2StepAttEnable, &QCheckBox::toggled, this, [this](bool on) {
        m_spnRx2StepAttValue->setEnabled(on);
        // Controller is single-RX for step-att enable; RX2 is future expansion.
    });

    // --- Spinbox → controller ---
    connect(m_spnRx1StepAttValue, &QSpinBox::valueChanged, this, [this](int dB) {
        if (m_ctrl) {
            m_ctrl->setAttenuation(dB, 0);
        }
    });
    connect(m_spnRx2StepAttValue, &QSpinBox::valueChanged, this, [this](int dB) {
        if (m_ctrl) {
            m_ctrl->setAttenuation(dB, 1);
        }
    });

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// Auto Attenuate groups (RX1 + RX2)
// From Thetis setup.cs: groupBoxTS47
// ---------------------------------------------------------------------------

void GeneralOptionsPage::buildAutoAttGroup()
{
    // --- Helper lambda to build one auto-att group ---
    auto buildOneRx = [this](const QString& title, int rx,
                             QCheckBox*& chkEnable, QComboBox*& cmbMode,
                             QCheckBox*& chkUndo, QSpinBox*& spnHold)
    {
        auto* group = new QGroupBox(title, this);
        auto* vbox  = new QVBoxLayout(group);
        vbox->setSpacing(6);

        // Enable checkbox
        chkEnable = new QCheckBox(QStringLiteral("Enable"), group);
        // From Thetis setup.cs: chkAutoATTRx1 / chkAutoATTRx2
        chkEnable->setToolTip(
            QStringLiteral("Auto attenuate RX%1 on ADC overload").arg(rx + 1));
        vbox->addWidget(chkEnable);

        // Mode combo row
        auto* modeRow = new QHBoxLayout;
        auto* modeLabel = new QLabel(QStringLiteral("Mode:"), group);
        cmbMode = makeModeCombo(group);
        cmbMode->setEnabled(false);
        modeRow->addWidget(modeLabel);
        modeRow->addWidget(cmbMode);
        modeRow->addStretch();
        vbox->addLayout(modeRow);

        // Undo/Decay checkbox
        chkUndo = new QCheckBox(QStringLiteral("Undo"), group);
        // From Thetis setup.cs: chkAutoATTRx1Undo concept
        chkUndo->setToolTip(
            QStringLiteral("Undo the changes made after the hold period."));
        chkUndo->setEnabled(false);
        vbox->addWidget(chkUndo);

        // Hold seconds row
        auto* holdRow = new QHBoxLayout;
        auto* holdLabel = new QLabel(QStringLiteral("Hold:"), group);
        spnHold = makeHoldSpinBox(group);
        spnHold->setEnabled(false);
        holdRow->addWidget(holdLabel);
        holdRow->addWidget(spnHold);
        holdRow->addStretch();
        vbox->addLayout(holdRow);

        // --- Enable/disable cascade ---
        // mode + undo + hold only enabled when auto-att enabled
        connect(chkEnable, &QCheckBox::toggled, this, [cmbMode, chkUndo, spnHold](bool on) {
            cmbMode->setEnabled(on);
            chkUndo->setEnabled(on);
            spnHold->setEnabled(on && chkUndo->isChecked());
        });

        // hold only enabled when undo is checked (and auto-att enabled)
        connect(chkUndo, &QCheckBox::toggled, this, [chkEnable, spnHold](bool on) {
            spnHold->setEnabled(on && chkEnable->isChecked());
        });

        // Mode change → relabel undo checkbox
        connect(cmbMode, &QComboBox::currentIndexChanged, this, [chkUndo](int idx) {
            if (idx == static_cast<int>(AutoAttMode::Adaptive)) {
                chkUndo->setText(QStringLiteral("Decay"));
            } else {
                chkUndo->setText(QStringLiteral("Undo"));
            }
        });

        // --- Wire to controller (RX1 only — controller is single-RX) ---
        if (m_ctrl && rx == 0) {
            connect(chkEnable, &QCheckBox::toggled, this, [this](bool on) {
                m_ctrl->setAutoAttEnabled(on);
            });
            connect(cmbMode, &QComboBox::currentIndexChanged, this, [this](int idx) {
                m_ctrl->setAutoAttMode(static_cast<AutoAttMode>(idx));
            });
            connect(chkUndo, &QCheckBox::toggled, this, [this](bool on) {
                m_ctrl->setAutoAttUndo(on);
            });
            connect(spnHold, &QSpinBox::valueChanged, this, [this, cmbMode](int sec) {
                if (cmbMode->currentIndex() == static_cast<int>(AutoAttMode::Adaptive)) {
                    m_ctrl->setAutoAttHoldSeconds(static_cast<double>(sec));
                } else {
                    m_ctrl->setAutoUndoDelaySec(sec);
                }
            });
        }

        contentLayout()->addWidget(group);
    };

    buildOneRx(QStringLiteral("Auto Attenuate RX1"), 0,
               m_chkAutoAttRx1, m_cmbAutoAttRx1Mode,
               m_chkAutoAttUndoRx1, m_spnAutoAttHoldRx1);

    buildOneRx(QStringLiteral("Auto Attenuate RX2"), 1,
               m_chkAutoAttRx2, m_cmbAutoAttRx2Mode,
               m_chkAutoAttUndoRx2, m_spnAutoAttHoldRx2);
}

// ---------------------------------------------------------------------------
// connectController — wire signals from the controller back to UI
// ---------------------------------------------------------------------------

void GeneralOptionsPage::connectController()
{
    Q_ASSERT(m_ctrl);

    // ADC-linked label visibility
    connect(m_ctrl, &StepAttenuatorController::adcLinkedChanged,
            m_lblAdcLinked, &QLabel::setVisible);

    // Attenuation changed → update RX1 spinbox (controller is single-RX)
    connect(m_ctrl, &StepAttenuatorController::attenuationChanged,
            this, [this](int dB) {
        QSignalBlocker blk(m_spnRx1StepAttValue);
        m_spnRx1StepAttValue->setValue(dB);
    });
}

// ---------------------------------------------------------------------------
// syncFromModel — restore UI state from controller on page show
// ---------------------------------------------------------------------------

void GeneralOptionsPage::syncFromModel()
{
    if (!m_ctrl) {
        return;
    }

    // Sync auto-att enable/mode from controller accessors
    {
        QSignalBlocker blk(m_chkAutoAttRx1);
        m_chkAutoAttRx1->setChecked(m_ctrl->autoAttEnabled());
    }
    {
        QSignalBlocker blk(m_cmbAutoAttRx1Mode);
        m_cmbAutoAttRx1Mode->setCurrentIndex(static_cast<int>(m_ctrl->autoAttMode()));
    }

    // Sync attenuation value
    {
        QSignalBlocker blk(m_spnRx1StepAttValue);
        m_spnRx1StepAttValue->setValue(m_ctrl->attenuatorDb());
    }

    // Re-cascade enable states
    bool autoOn = m_chkAutoAttRx1->isChecked();
    m_cmbAutoAttRx1Mode->setEnabled(autoOn);
    m_chkAutoAttUndoRx1->setEnabled(autoOn);
    m_spnAutoAttHoldRx1->setEnabled(autoOn && m_chkAutoAttUndoRx1->isChecked());
}

} // namespace NereusSDR
