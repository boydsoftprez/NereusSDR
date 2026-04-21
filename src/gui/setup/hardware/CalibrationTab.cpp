// =================================================================
// src/gui/setup/hardware/CalibrationTab.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.cs
//     (tpGeneralCalibration group: udHPSDRFreqCorrectFactor, chkUsing10MHzRef,
//      udHPSDRFreqCorrectFactor10MHz, btnHPSDRFreqCalReset10MHz,
//      udGeneralCalFreq1, btnGeneralCalFreqStart, udGeneralCalFreq2,
//      udGeneralCalLevel, btnGeneralCalLevelStart, btnResetLevelCal,
//      ud6mLNAGainOffset, ud6mRx2LNAGainOffset, udTXDisplayCalOffset
//      -- lines 5137-5144; 6470-6525; 13966-13967; 14036-14050; 14325-14333;
//         17243-17248; 18315-18317; 22690-22706),
//      original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs
//     (CalibrateFreq, CalibrateLevel, RXCalibrationOffset, CalibratedPAPower
//      -- lines 9764-9844; 9844-10215; 21022-21086; 6691-6724),
//      original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 -- Original PaCalibrationTab implementation.
//   2026-04-20 -- Renamed PaCalibrationTab -> CalibrationTab; expanded to 5
//                  group boxes matching Thetis General -> Calibration 1:1;
//                  backed by CalibrationController model (Phase 3P-G).
//                  J.J. Boyd (KG4VCF), with AI-assisted transformation via
//                  Anthropic Claude Code.
// =================================================================

// --- From setup.cs ---

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

// --- From console.cs ---

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
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
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines.
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
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

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

#include "CalibrationTab.h"

#include "core/BoardCapabilities.h"
#include "core/CalibrationController.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

#ifdef NEREUS_BUILD_TESTS
#include <QLayout>
#endif

namespace NereusSDR {

// Helper: create a QDoubleSpinBox with common settings
static QDoubleSpinBox* makeSpinBox(double min, double max, double val,
                                   double step, int decimals, QWidget* parent)
{
    auto* sb = new QDoubleSpinBox(parent);
    sb->setRange(min, max);
    sb->setValue(val);
    sb->setSingleStep(step);
    sb->setDecimals(decimals);
    return sb;
}

// -- Constructor ---------------------------------------------------------------

CalibrationTab::CalibrationTab(RadioModel* model, QWidget* parent)
    : QWidget(parent), m_model(model)
{
    // Wire to CalibrationController if RadioModel exposes it.
    // RadioModel::calibrationController() is added in this phase.
    if (m_model) {
        m_calCtrl = &m_model->calibrationControllerMutable();
        connect(m_calCtrl, &CalibrationController::changed,
                this, &CalibrationTab::onControllerChanged);
    }

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(8);

    // We lay out the 5 group boxes in a 2-column grid using two QVBoxLayouts
    // inside a QHBoxLayout (top half: col 0 = Freq Cal + Level Cal,
    // col 1 = HPSDR Diag + TX Display Cal; bottom: PA Current full-width).
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    auto* scrollWidget = new QWidget(scrollArea);
    auto* mainLayout   = new QVBoxLayout(scrollWidget);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    scrollArea->setWidget(scrollWidget);
    outerLayout->addWidget(scrollArea);

    auto* topRow  = new QHBoxLayout;
    auto* leftCol = new QVBoxLayout;
    auto* rightCol= new QVBoxLayout;
    topRow->addLayout(leftCol, 1);
    topRow->addLayout(rightCol, 1);
    mainLayout->addLayout(topRow);

    // =========================================================================
    // Group 1: Freq Cal
    // Source: setup.cs:6470-6511 btnGeneralCalFreqStart + udGeneralCalFreq1 [@501e3f5]
    // =========================================================================
    auto* freqCalGroup = new QGroupBox(tr("Freq Cal"), scrollWidget);
    auto* freqCalForm  = new QFormLayout(freqCalGroup);

    // Source: setup.cs udGeneralCalFreq1 — default 10 000 000 Hz (10 MHz) [@501e3f5]
    m_freqCalFreqSpin = makeSpinBox(0.0, 30e6, 10e6, 1000.0, 0, freqCalGroup);
    m_freqCalFreqSpin->setSuffix(tr(" Hz"));
    freqCalForm->addRow(tr("Frequency:"), m_freqCalFreqSpin);

    // Source: setup.cs:6470-6472 btnGeneralCalFreqStart_Click -- triggers
    //   calibration routine on a background thread [@501e3f5]
    m_freqCalStartBtn = new QPushButton(tr("Start"), freqCalGroup);
    m_freqCalStartBtn->setToolTip(
        tr("Start frequency calibration. Requires radio powered on.\n"
           "Calibration logic deferred — emits calFreqStartRequested."));
    freqCalForm->addRow(QString(), m_freqCalStartBtn);

    // Source: setup.cs:6471 helptext above freq cal controls
    //   "Larger FFT sizes / lower sample rates give increased accuracy." [@501e3f5]
    auto* freqCalHelpLabel = new QLabel(
        tr("<i>Larger FFT sizes / lower sample rates give increased accuracy.</i>"),
        freqCalGroup);
    freqCalHelpLabel->setWordWrap(true);
    freqCalForm->addRow(freqCalHelpLabel);

    leftCol->addWidget(freqCalGroup);

    // =========================================================================
    // Group 2: Level Cal
    // Source: setup.cs:6482-6525 btnGeneralCalLevelStart + udGeneralCalFreq2
    //   + udGeneralCalLevel + btnResetLevelCal + ud6mLNAGainOffset [@501e3f5]
    // =========================================================================
    auto* levelCalGroup = new QGroupBox(tr("Level Cal"), scrollWidget);
    auto* levelCalForm  = new QFormLayout(levelCalGroup);

    // Source: setup.cs udGeneralCalFreq2 -- default 14 100 000 Hz (14.1 MHz) [@501e3f5]
    m_levelCalFreqSpin = makeSpinBox(0.0, 30e6, 14.1e6, 1000.0, 0, levelCalGroup);
    m_levelCalFreqSpin->setSuffix(tr(" Hz"));
    levelCalForm->addRow(tr("Frequency:"), m_levelCalFreqSpin);

    // Source: setup.cs udGeneralCalLevel -- default -73 dBm [@501e3f5]
    m_levelCalLevelSpin = makeSpinBox(-200.0, 0.0, -73.0, 1.0, 1, levelCalGroup);
    m_levelCalLevelSpin->setSuffix(tr(" dBm"));
    levelCalForm->addRow(tr("Level (dBm):"), m_levelCalLevelSpin);

    // Source: setup.cs:17243-17248 ud6mLNAGainOffset -> console.RX6mGainOffset_RX1 [@501e3f5]
    m_rx1LnaSpin = makeSpinBox(-30.0, 30.0, 0.0, 0.1, 1, levelCalGroup);
    m_rx1LnaSpin->setSuffix(tr(" dB"));
    levelCalForm->addRow(tr("Rx1 6m LNA:"), m_rx1LnaSpin);

    // Source: setup.cs:18315-18317 ud6mRx2LNAGainOffset -> console.RX6mGainOffset_RX2 [@501e3f5]
    m_rx2LnaSpin = makeSpinBox(-30.0, 30.0, 0.0, 0.1, 1, levelCalGroup);
    m_rx2LnaSpin->setSuffix(tr(" dB"));
    levelCalForm->addRow(tr("Rx2 6m LNA:"), m_rx2LnaSpin);

    auto* levelBtnRow = new QHBoxLayout;
    // Source: setup.cs:6482-6521 btnGeneralCalLevelStart_Click [@501e3f5]
    m_levelCalStartBtn = new QPushButton(tr("Start"), levelCalGroup);
    m_levelCalStartBtn->setToolTip(
        tr("Start level calibration. Requires calibrated signal at the specified frequency."));
    levelBtnRow->addWidget(m_levelCalStartBtn);
    // Source: setup.cs:24226 btnResetLevelCal_Click -- resets calibration offset to 0 [@501e3f5]
    m_levelCalResetBtn = new QPushButton(tr("Reset"), levelCalGroup);
    m_levelCalResetBtn->setToolTip(tr("Reset level calibration offset to 0 dB."));
    levelBtnRow->addWidget(m_levelCalResetBtn);
    levelBtnRow->addStretch();
    levelCalForm->addRow(levelBtnRow);

    leftCol->addWidget(levelCalGroup);
    leftCol->addStretch();

    // =========================================================================
    // Group 3: HPSDR Freq Cal Diagnostic
    // Source: setup.cs:5137-5144; 14036-14050; 22690-22706 [@501e3f5]
    // =========================================================================
    auto* hpsdrGroup = new QGroupBox(tr("HPSDR Freq Cal Diagnostic"), scrollWidget);
    auto* hpsdrForm  = new QFormLayout(hpsdrGroup);

    // Source: setup.cs:5137-5144 udHPSDRFreqCorrectFactor -- default 1.0, 9 decimal places [@501e3f5]
    m_freqFactorSpin = makeSpinBox(0.0, 2.0, 1.0, 0.000000001, 9, hpsdrGroup);
    m_freqFactorSpin->setToolTip(
        tr("HPSDR frequency correction factor applied to NCO phase-word.\n"
           "Default 1.0 = no correction. Set via auto-calibration or manually."));

    auto* factorRow = new QHBoxLayout;
    factorRow->addWidget(m_freqFactorSpin, 1);
    // Source: setup.cs:13966-13967 btnHPSDRFreqCalReset -- sets factor to 1.0 [@501e3f5]
    m_freqFactorResetBtn = new QPushButton(tr("Reset"), hpsdrGroup);
    m_freqFactorResetBtn->setToolTip(tr("Reset correction factor to 1.0 (no correction)."));
    factorRow->addWidget(m_freqFactorResetBtn);
    hpsdrForm->addRow(tr("Correction factor:"), factorRow);

    // Source: setup.cs:22690-22696 chkUsing10MHzRef_CheckedChanged [@501e3f5]
    m_use10MhzCheck = new QCheckBox(tr("Using external 10 MHz ref"), hpsdrGroup);
    m_use10MhzCheck->setToolTip(
        tr("When checked, uses the 10 MHz factor instead of the standard factor.\n"
           "Also disables the Freq Cal Start button (cannot auto-cal with 10 MHz ref)."));
    hpsdrForm->addRow(m_use10MhzCheck);

    // Source: setup.cs:22704 udHPSDRFreqCorrectFactor10MHz -- default 1.0 [@501e3f5]
    m_freqFactor10MSpin = makeSpinBox(0.0, 2.0, 1.0, 0.000000001, 9, hpsdrGroup);
    m_freqFactor10MSpin->setToolTip(
        tr("Correction factor used when external 10 MHz reference is selected."));
    m_freqFactor10MSpin->setEnabled(false); // enabled only when m_use10MhzCheck is checked

    auto* factor10MRow = new QHBoxLayout;
    factor10MRow->addWidget(m_freqFactor10MSpin, 1);
    // Source: setup.cs:22701 btnHPSDRFreqCalReset10MHz -- sets 10 MHz factor to 1.0 [@501e3f5]
    m_freqFactor10MResetBtn = new QPushButton(tr("Reset"), hpsdrGroup);
    m_freqFactor10MResetBtn->setToolTip(tr("Reset 10 MHz correction factor to 1.0."));
    m_freqFactor10MResetBtn->setEnabled(false);
    factor10MRow->addWidget(m_freqFactor10MResetBtn);
    hpsdrForm->addRow(tr("10 MHz factor:"), factor10MRow);

    rightCol->addWidget(hpsdrGroup);

    // =========================================================================
    // Group 4: TX Display Cal
    // Source: setup.cs:14325-14333 udTXDisplayCalOffset [@501e3f5]
    // =========================================================================
    auto* txDisplayGroup = new QGroupBox(tr("TX Display Cal"), scrollWidget);
    auto* txDisplayForm  = new QFormLayout(txDisplayGroup);

    // Source: setup.cs:14325-14328 udTXDisplayCalOffset -> Display.TXDisplayCalOffset [@501e3f5]
    m_txDisplayOffsetSpin = makeSpinBox(-50.0, 50.0, 0.0, 0.1, 1, txDisplayGroup);
    m_txDisplayOffsetSpin->setSuffix(tr(" dB"));
    m_txDisplayOffsetSpin->setToolTip(
        tr("TX display calibration offset in dB. Applied to TX spectrum display."));
    txDisplayForm->addRow(tr("Offset:"), m_txDisplayOffsetSpin);

    rightCol->addWidget(txDisplayGroup);
    rightCol->addStretch();

    // =========================================================================
    // Group 5: PA Current (A) calculation (preserved from PaCalibrationTab)
    // Source: console.cs:6691-6724 CalibratedPAPower -- sensitivity + offset [@501e3f5]
    // =========================================================================
    auto* paCurrentGroup = new QGroupBox(tr("PA Current (A) calculation"), scrollWidget);
    auto* paCurrentForm  = new QFormLayout(paCurrentGroup);

    m_paSensSpin = makeSpinBox(-100.0, 100.0, 1.0, 0.1, 4, paCurrentGroup);
    m_paSensSpin->setToolTip(
        tr("PA current sensor sensitivity (volts/amp). Hardware-specific constant."));
    paCurrentForm->addRow(tr("Sensitivity:"), m_paSensSpin);

    m_paOffsetSpin = makeSpinBox(-10.0, 10.0, 0.0, 0.01, 4, paCurrentGroup);
    m_paOffsetSpin->setToolTip(
        tr("PA current sensor offset (amps). Hardware-specific constant."));
    paCurrentForm->addRow(tr("Offset:"), m_paOffsetSpin);

    auto* paBtnRow = new QHBoxLayout;
    m_paDefaultBtn = new QPushButton(tr("Default"), paCurrentGroup);
    m_paDefaultBtn->setToolTip(tr("Restore PA current sensitivity/offset to board defaults."));
    paBtnRow->addWidget(m_paDefaultBtn);
    paBtnRow->addStretch();
    paCurrentForm->addRow(paBtnRow);

    // Source: console.cs:27457-27463 chkLogVoltsAmps -> console.LogVA [@501e3f5]
    m_logVoltsAmpsCheck = new QCheckBox(tr("Log Volts/Amps to VALog.txt"), paCurrentGroup);
    paCurrentForm->addRow(m_logVoltsAmpsCheck);

    mainLayout->addWidget(paCurrentGroup);

    // =========================================================================
    // Connections: UI -> CalibrationController
    // =========================================================================

    // Freq Cal: freq spinbox changes are not persisted (transient cal param)
    connect(m_freqCalStartBtn, &QPushButton::clicked, this, [this]() {
        emit settingChanged(QStringLiteral("cal/triggerFreqCal"),
                            m_freqCalFreqSpin->value());
    });

    // Level Cal: Start / Reset
    connect(m_levelCalStartBtn, &QPushButton::clicked, this, [this]() {
        emit settingChanged(QStringLiteral("cal/triggerLevelCal"),
                            m_levelCalFreqSpin->value());
    });
    connect(m_levelCalResetBtn, &QPushButton::clicked, this, [this]() {
        if (m_calCtrl) { m_calCtrl->setLevelOffsetDb(0.0); m_calCtrl->save(); }
        emit settingChanged(QStringLiteral("cal/levelOffset"), 0.0);
    });

    // 6m LNA offsets
    connect(m_rx1LnaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (m_updatingFromModel || !m_calCtrl) { return; }
        m_calCtrl->setRx1_6mLnaOffset(v);
        m_calCtrl->save();
        emit settingChanged(QStringLiteral("cal/rx1_6mLna"), v);
    });
    connect(m_rx2LnaSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (m_updatingFromModel || !m_calCtrl) { return; }
        m_calCtrl->setRx2_6mLnaOffset(v);
        m_calCtrl->save();
        emit settingChanged(QStringLiteral("cal/rx2_6mLna"), v);
    });

    // HPSDR Freq Cal Diagnostic
    connect(m_freqFactorSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (m_updatingFromModel || !m_calCtrl) { return; }
        m_calCtrl->setFreqCorrectionFactor(v);
        m_calCtrl->save();
        emit settingChanged(QStringLiteral("cal/freqFactor"), v);
    });
    connect(m_freqFactorResetBtn, &QPushButton::clicked, this, [this]() {
        // Source: setup.cs:13966-13967 udHPSDRFreqCorrectFactor.Value = 1.0 [@501e3f5]
        if (m_calCtrl) { m_calCtrl->setFreqCorrectionFactor(1.0); m_calCtrl->save(); }
        QSignalBlocker sb(m_freqFactorSpin);
        m_freqFactorSpin->setValue(1.0);
        emit settingChanged(QStringLiteral("cal/freqFactor"), 1.0);
    });
    connect(m_use10MhzCheck, &QCheckBox::toggled, this, [this](bool checked) {
        // Source: setup.cs:22690-22694 chkUsing10MHzRef_CheckedChanged
        //   btnGeneralCalFreqStart.Enabled = !chkUsing10MHzRef.Checked;
        //   udHPSDRFreqCorrectFactor10MHz.Enabled = chkUsing10MHzRef.Checked; [@501e3f5]
        m_freqCalStartBtn->setEnabled(!checked);
        m_freqFactor10MSpin->setEnabled(checked);
        m_freqFactor10MResetBtn->setEnabled(checked);
        if (m_updatingFromModel || !m_calCtrl) { return; }
        m_calCtrl->setUsing10MHzRef(checked);
        m_calCtrl->save();
        emit settingChanged(QStringLiteral("cal/using10M"), checked);
    });
    connect(m_freqFactor10MSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (m_updatingFromModel || !m_calCtrl) { return; }
        m_calCtrl->setFreqCorrectionFactor10M(v);
        m_calCtrl->save();
        emit settingChanged(QStringLiteral("cal/freqFactor10M"), v);
    });
    connect(m_freqFactor10MResetBtn, &QPushButton::clicked, this, [this]() {
        // Source: setup.cs:22701 udHPSDRFreqCorrectFactor10MHz.Value = 1.0 [@501e3f5]
        if (m_calCtrl) { m_calCtrl->setFreqCorrectionFactor10M(1.0); m_calCtrl->save(); }
        QSignalBlocker sb(m_freqFactor10MSpin);
        m_freqFactor10MSpin->setValue(1.0);
        emit settingChanged(QStringLiteral("cal/freqFactor10M"), 1.0);
    });

    // TX Display Cal
    connect(m_txDisplayOffsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (m_updatingFromModel || !m_calCtrl) { return; }
        m_calCtrl->setTxDisplayOffsetDb(v);
        m_calCtrl->save();
        emit settingChanged(QStringLiteral("cal/txDisplayOffset"), v);
    });

    // PA Current
    connect(m_paSensSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (m_updatingFromModel || !m_calCtrl) { return; }
        m_calCtrl->setPaCurrentSensitivity(v);
        m_calCtrl->save();
        emit settingChanged(QStringLiteral("cal/paSens"), v);
    });
    connect(m_paOffsetSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (m_updatingFromModel || !m_calCtrl) { return; }
        m_calCtrl->setPaCurrentOffset(v);
        m_calCtrl->save();
        emit settingChanged(QStringLiteral("cal/paOffset"), v);
    });
    connect(m_paDefaultBtn, &QPushButton::clicked, this, [this]() {
        if (!m_calCtrl) { return; }
        // Restore to model defaults (1.0 sensitivity, 0.0 offset)
        m_calCtrl->setPaCurrentSensitivity(1.0);
        m_calCtrl->setPaCurrentOffset(0.0);
        m_calCtrl->save();
        syncFromController();
        emit settingChanged(QStringLiteral("cal/paDefaultRestored"), true);
    });
    connect(m_logVoltsAmpsCheck, &QCheckBox::toggled, this, [this](bool checked) {
        // Source: console.cs:27460-27463 chkLogVoltsAmps_CheckedChanged -> console.LogVA [@501e3f5]
        emit settingChanged(QStringLiteral("cal/logVoltsAmps"), checked);
    });

    // Sync from controller if already available
    if (m_calCtrl) {
        syncFromController();
    }
}

// -- onControllerChanged -------------------------------------------------------

void CalibrationTab::onControllerChanged()
{
    syncFromController();
}

// -- syncFromController --------------------------------------------------------

void CalibrationTab::syncFromController()
{
    if (!m_calCtrl) { return; }

    m_updatingFromModel = true;

    {
        QSignalBlocker sb1(m_freqFactorSpin);
        m_freqFactorSpin->setValue(m_calCtrl->freqCorrectionFactor());
    }
    {
        QSignalBlocker sb2(m_freqFactor10MSpin);
        m_freqFactor10MSpin->setValue(m_calCtrl->freqCorrectionFactor10M());
    }
    {
        QSignalBlocker sb3(m_use10MhzCheck);
        m_use10MhzCheck->setChecked(m_calCtrl->using10MHzRef());
    }
    // Enable/disable 10 MHz controls to match state
    // Source: setup.cs:22693-22694 [@501e3f5]
    m_freqCalStartBtn->setEnabled(!m_calCtrl->using10MHzRef());
    m_freqFactor10MSpin->setEnabled(m_calCtrl->using10MHzRef());
    m_freqFactor10MResetBtn->setEnabled(m_calCtrl->using10MHzRef());

    {
        QSignalBlocker sb4(m_rx1LnaSpin);
        m_rx1LnaSpin->setValue(m_calCtrl->rx1_6mLnaOffset());
    }
    {
        QSignalBlocker sb5(m_rx2LnaSpin);
        m_rx2LnaSpin->setValue(m_calCtrl->rx2_6mLnaOffset());
    }
    {
        QSignalBlocker sb6(m_txDisplayOffsetSpin);
        m_txDisplayOffsetSpin->setValue(m_calCtrl->txDisplayOffsetDb());
    }
    {
        QSignalBlocker sb7(m_paSensSpin);
        m_paSensSpin->setValue(m_calCtrl->paCurrentSensitivity());
    }
    {
        QSignalBlocker sb8(m_paOffsetSpin);
        m_paOffsetSpin->setValue(m_calCtrl->paCurrentOffset());
    }

    m_updatingFromModel = false;
}

// -- populate ------------------------------------------------------------------

void CalibrationTab::populate(const RadioInfo& /*info*/, const BoardCapabilities& /*caps*/)
{
    // Load per-radio calibration settings from controller (set by RadioModel at connect).
    if (m_calCtrl) {
        syncFromController();
    }
}

// -- restoreSettings -----------------------------------------------------------

void CalibrationTab::restoreSettings(const QMap<QString, QVariant>& /*settings*/)
{
    // Calibration settings are loaded via CalibrationController::load() on connect.
    // Nothing to do here — kept for API parity with other tab types.
}

// -- groupBoxCountForTest ------------------------------------------------------

#ifdef NEREUS_BUILD_TESTS
int CalibrationTab::groupBoxCountForTest() const
{
    int count = 0;
    // Walk all child widgets of this widget
    for (QObject* child : children()) {
        // QGroupBoxes may be nested inside QScrollArea/QWidget; do a recursive walk
        if (qobject_cast<QGroupBox*>(child)) {
            ++count;
        }
    }
    // The group boxes are inside the scroll widget, so recurse one more level
    for (QObject* child : children()) {
        if (auto* sa = qobject_cast<QScrollArea*>(child)) {
            for (QObject* inner : sa->widget()->children()) {
                if (qobject_cast<QGroupBox*>(inner)) {
                    ++count;
                }
            }
        }
    }
    return count;
}
#endif

} // namespace NereusSDR
