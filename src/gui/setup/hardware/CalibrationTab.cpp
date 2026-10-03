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
//   2026-09-23 - R-R3-46: a remote window loads the Core's radio's
//                 calibration; TX Display and Volts/Amps follow the transmit
//                 permission. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-24 - R-R3-49 / R-R3-21: the frequency calibration Start button
//                 is hidden until built (freq-cal). J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-46 / R-R3-49 (remote-window parity Task 13): TX
//                 Display Cal and Volts/Amps Calibration follow the transmit
//                 settings gate instead of the transmit permission.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - Volts/Amps Calibration takes Thetis's ranges and the
//                 model's defaults (btnAmpDefault), and is disabled with its
//                 reason where it cannot change the PA current reading.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - 6 m LNA spins take Thetis's 0..25 dB, step 1 and 13 dB;
//                 Rx2's is disabled with its reason. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-R3-49 (found bug): TX Display Cal holds Thetis's
//                 -100..100 dB (was -50..50) and carries its Setup
//                 description id (R-IOS-18). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-29 - R-R3-49 (lead's ruling): the correction factors take
//                 Thetis's 0..65 (was 0..2). J.J. Boyd (KG4VCF), AI-assisted
//                 via Anthropic Claude Code.
//   2026-09-29 - R-R3-49 (found bug): Log Volts/Amps to VALog.txt works:
//                 the controller reads the box (logVoltsAmps), the station's
//                 RadioModel logs through VoltsAmpsLog (Thetis console.cs
//                 LogVA). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-29 - Level Cal: Reset asks first (setup.cs:24332-24341
//                 [v2.10.3.15]) and resets the meter and display offsets
//                 through RadioModel; Start is shown disabled with its reason
//                 and no longer writes cal/triggerLevelCal. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - Level Cal: Start runs the level calibration on the Core
//                 (setup.cs:6516-6567 [v2.10.3.15]): it asks first, shows
//                 the run's progress with a Cancel, says when it is
//                 complete, and is disabled with its reason where the
//                 Core cannot run it. J.J. Boyd (KG4VCF), AI-assisted via
//                 Anthropic Claude Code.
//   2026-09-30 - Rx1 6m LNA carries its Setup description id (version 23).
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Level Cal 2: at the hosting desktop, Start names the
//                 desktop's own active slice when another device owns the
//                 station's. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                 Claude Code.
//   2026-09-30 - Level Cal 2 review: with no slice the desktop may
//                 change, Start is disabled with the ownership words.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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
#include "HardwareTransmitGate.h"
#include "gui/UnbuiltFeatures.h"

#include "core/BoardCapabilities.h"
#include "core/CalibrationController.h"
#include "core/RadioDiscovery.h"
#include "core/SliceOwnership.h"
#include "core/session/IStationLink.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
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
    // col 1 = HPSDR Diag + TX Display Cal; bottom: Volts/Amps Cal full-width).
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
        tr("Start frequency calibration. The radio must be on."));
    freqCalForm->addRow(QString(), m_freqCalStartBtn);
    m_freqCalStartBtn->setObjectName(QStringLiteral("freqCalStartButton"));
    // R-R3-49 (freq-cal): nothing handles the frequency calibration request
    // yet; the button's row is hidden until it is built.
    UnbuiltFeatures::hideUnlessBuilt(m_freqCalStartBtn, UnbuiltFeature::FrequencyCalibration);

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
    m_levelCalFreqSpin->setObjectName(QStringLiteral("levelCalFrequencySpin"));
    levelCalForm->addRow(tr("Frequency:"), m_levelCalFreqSpin);

    // Source: setup.cs udGeneralCalLevel -- default -73 dBm [@501e3f5]
    m_levelCalLevelSpin = makeSpinBox(-200.0, 0.0, -73.0, 1.0, 1, levelCalGroup);
    m_levelCalLevelSpin->setSuffix(tr(" dBm"));
    m_levelCalLevelSpin->setObjectName(QStringLiteral("levelCalLevelSpin"));
    levelCalForm->addRow(tr("Level (dBm):"), m_levelCalLevelSpin);

    // Source: setup.cs:17243-17248 ud6mLNAGainOffset -> console.RX6mGainOffset_RX1 [@501e3f5]
    // From Thetis setup.designer.cs:12089-12116 [v2.10.3.15]: 0..25 dB,
    // step 1, one decimal, 13 dB.
    m_rx1LnaSpin = makeSpinBox(0.0, 25.0, 13.0, 1.0, 1, levelCalGroup);
    m_rx1LnaSpin->setObjectName(QStringLiteral("rx1SixMeterLnaSpin"));
    // Setup description version 23 describes this box to remote windows.
    m_rx1LnaSpin->setProperty("nereusSetupId", "hardware.calibration.rx1_6mLna");
    m_rx1LnaSpin->setSuffix(tr(" dB"));
    levelCalForm->addRow(tr("Rx1 6m LNA:"), m_rx1LnaSpin);

    // Source: setup.cs:18315-18317 ud6mRx2LNAGainOffset -> console.RX6mGainOffset_RX2 [@501e3f5]
    // From Thetis setup.designer.cs:12047-12074 [v2.10.3.15]: 0..25 dB,
    // step 1, one decimal, 13 dB. Thetis applies it to RX2's own receive
    // calibration (RXCalibrationOffset(2), console.cs:21068-21075 //DH1KLM); NereusSDR
    // has one receive calibration for the station, which the Rx1 value
    // enters, so this one is shown disabled with the reason.
    m_rx2LnaSpin = makeSpinBox(0.0, 25.0, 13.0, 1.0, 1, levelCalGroup);
    m_rx2LnaSpin->setObjectName(QStringLiteral("rx2SixMeterLnaSpin"));
    m_rx2LnaSpin->setSuffix(tr(" dB"));
    m_rx2LnaSpin->setEnabled(false);
    m_rx2LnaSpin->setToolTip(
        tr("Not used: every receiver shares the Rx1 calibration."));
    levelCalForm->addRow(tr("Rx2 6m LNA:"), m_rx2LnaSpin);

    auto* levelBtnRow = new QHBoxLayout;
    // From Thetis setup.cs:6516-6538 [v2.10.3.15] btnGeneralCalLevelStart_Click:
    // the run (console.cs CalibrateLevel) goes on the Core that owns the
    // radio (LevelCalibrationService); refreshLevelCalControls() sets
    // whether it can start and why not.
    m_levelCalStartBtn = new QPushButton(tr("Start"), levelCalGroup);
    m_levelCalStartBtn->setObjectName(QStringLiteral("levelCalStartButton"));
    levelBtnRow->addWidget(m_levelCalStartBtn);
    // From Thetis setup.cs:24332-24341 [v2.10.3.15] btnResetLevelCal_Click:
    // asks first, then console.ResetLevelCalibration() puts the meter and
    // display offsets back to the radio's defaults.
    m_levelCalResetBtn = new QPushButton(tr("Reset"), levelCalGroup);
    m_levelCalResetBtn->setObjectName(QStringLiteral("levelCalResetButton"));
    m_levelCalResetBtn->setToolTip(
        tr("Put the receive level calibration back to this radio's defaults."));
    levelBtnRow->addWidget(m_levelCalResetBtn);
    // Thetis shows the run in its progress window (progress.cs, a bar and
    // Abort); here the bar and Cancel sit in the group, with the run's
    // last word under them.
    m_levelCalCancelBtn = new QPushButton(tr("Cancel"), levelCalGroup);
    m_levelCalCancelBtn->setObjectName(QStringLiteral("levelCalCancelButton"));
    levelBtnRow->addWidget(m_levelCalCancelBtn);
    levelBtnRow->addStretch();
    levelCalForm->addRow(levelBtnRow);
    m_levelCalProgress = new QProgressBar(levelCalGroup);
    m_levelCalProgress->setObjectName(QStringLiteral("levelCalProgressBar"));
    m_levelCalProgress->setRange(0, 100);
    m_levelCalProgress->setValue(0);
    levelCalForm->addRow(m_levelCalProgress);
    m_levelCalStatusLabel = new QLabel(levelCalGroup);
    m_levelCalStatusLabel->setObjectName(QStringLiteral("levelCalStatusLabel"));
    m_levelCalStatusLabel->setWordWrap(true);
    levelCalForm->addRow(m_levelCalStatusLabel);

    leftCol->addWidget(levelCalGroup);
    leftCol->addStretch();

    // =========================================================================
    // Group 3: HPSDR Freq Cal Diagnostic
    // Source: setup.cs:5137-5144; 14036-14050; 22690-22706 [@501e3f5]
    // =========================================================================
    auto* hpsdrGroup = new QGroupBox(tr("HPSDR Freq Cal Diagnostic"), scrollWidget);
    auto* hpsdrForm  = new QFormLayout(hpsdrGroup);

    // Source: setup.cs:5137-5144 udHPSDRFreqCorrectFactor -- default 1.0, 9 decimal places [@501e3f5]
    // Lead's ruling (R-R3-49): Thetis's range, 0 to 65 (was 0 to 2).
    // From Thetis setup.designer.cs:11983 [v2.10.3.15] udHPSDRFreqCorrectFactor
    //   Maximum = 65; Minimum = 0
    m_freqFactorSpin = makeSpinBox(0.0, 65.0, 1.0, 0.000000001, 9, hpsdrGroup);
    m_freqFactorSpin->setObjectName(QStringLiteral("freqCorrectionFactorSpin"));
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
    // From Thetis setup.designer.cs:11928 [v2.10.3.15] udHPSDRFreqCorrectFactor10MHz
    //   Maximum = 65; Minimum = 0
    m_freqFactor10MSpin = makeSpinBox(0.0, 65.0, 1.0, 0.000000001, 9, hpsdrGroup);
    m_freqFactor10MSpin->setObjectName(QStringLiteral("freqCorrectionFactor10MSpin"));
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
    m_txDisplayGroup = txDisplayGroup;
    auto* txDisplayForm  = new QFormLayout(txDisplayGroup);

    // Source: setup.cs:14325-14328 udTXDisplayCalOffset -> Display.TXDisplayCalOffset [@501e3f5]
    // R-R3-49 (found bug): the range was -50..50; Thetis's box holds
    // -100..100 in 0.1 dB steps, one decimal:
    // From Thetis setup.designer.cs:11863 [v2.10.3.15] udTXDisplayCalOffset
    //   DecimalPlaces = 1; Increment = 0.1; Maximum = 100; Minimum = -100
    m_txDisplayOffsetSpin = makeSpinBox(-100.0, 100.0, 0.0, 0.1, 1, txDisplayGroup);
    m_txDisplayOffsetSpin->setProperty("nereusSetupId", "hardware.calibration.txDisplayOffset");
    m_txDisplayOffsetSpin->setSuffix(tr(" dB"));
    m_txDisplayOffsetSpin->setToolTip(
        tr("TX display calibration offset in dB. Applied to TX spectrum display."));
    txDisplayForm->addRow(tr("Offset:"), m_txDisplayOffsetSpin);

    rightCol->addWidget(txDisplayGroup);
    rightCol->addStretch();

    // =========================================================================
    // Group 5: Volts/Amps Calibration -- Thetis groupBoxTS27 equivalent.
    // Source: Thetis setup.designer.cs:11672-11677 (groupBoxTS27 contents:
    //         chkLogVoltsAmps + btnAmpDefault + udAmpSens + udAmpVoff)
    //         + console.cs:24893 _amp_voff = 360.0f default [v2.10.3.13]
    // Was previously labelled "PA Current (A) calculation" -- relabelled
    // 2026-05-02 to match Thetis intent (V/A calibration constants for
    // PA volts/amps computation).  The CalibrationController model-layer
    // API (paCurrentSensitivity / paCurrentOffset) is intentionally NOT
    // renamed in the same pass -- separate audit, since persistence keys
    // ride on those names.
    // =========================================================================
    auto* vaCalGroup = new QGroupBox(tr("Volts/Amps Calibration"), scrollWidget);
    m_vaCalGroup = vaCalGroup;
    auto* vaCalForm  = new QFormLayout(vaCalGroup);

    // Source: Thetis udAmpSens setup.designer.cs:11672-11677 [v2.10.3.13];
    //         setup.cs:24255 udAmpSens_ValueChanged -> console.AmpSens.
    //         Range from udAmpSens setup.designer.cs:11782-11807 [v2.10.3.15]:
    //         0.001..5000, step 1. Three decimals (Thetis shows one) so the
    //         ANAN-G2 defaults 66.23 and 0.001 show as they apply.
    m_ampSensSpin = makeSpinBox(0.001, 5000.0, 120.0, 1.0, 3, vaCalGroup);
    m_ampSensSpin->setRange(0.001, 5000.0);
    m_ampSensSpin->setToolTip(
        tr("Amp sensitivity for PA volts/amps calculation. Hardware-specific constant."));
    vaCalForm->addRow(tr("Sensitivity:"), m_ampSensSpin);

    // Source: Thetis udAmpVoff setup.designer.cs:11672-11677 [v2.10.3.13];
    //         setup.cs:24247 udAmpVoff_ValueChanged -> console.AmpVoff.
    //         Default upstream is 360.0f (console.cs:24893 _amp_voff).
    //         Range from udAmpVoff setup.designer.cs:11812-11837 [v2.10.3.15]:
    //         0..5000, step 1.
    m_ampVoffSpin = makeSpinBox(0.0, 5000.0, 360.0, 1.0, 3, vaCalGroup);
    m_ampVoffSpin->setToolTip(
        tr("Amp voltage offset for PA volts/amps calculation. Hardware-specific constant."));
    vaCalForm->addRow(tr("Offset:"), m_ampVoffSpin);

    auto* vaBtnRow = new QHBoxLayout;
    // Source: Thetis btnAmpDefault setup.designer.cs:11672-11677 [v2.10.3.13]
    //         -- restores AmpSens / AmpVoff to factory defaults.
    m_ampDefaultBtn = new QPushButton(tr("Default"), vaCalGroup);
    m_ampDefaultBtn->setToolTip(tr("Restore amp sensitivity / voltage offset to board defaults."));
    vaBtnRow->addWidget(m_ampDefaultBtn);
    vaBtnRow->addStretch();
    vaCalForm->addRow(vaBtnRow);

    // Source: console.cs:27457-27463 chkLogVoltsAmps -> console.LogVA [@501e3f5]
    // Upstream inline attribution preserved verbatim (console.cs:27453):
    //   chkVFOBLock.Enabled = false; //[2.10.3.7]MW0LGE
    m_logVoltsAmpsCheck = new QCheckBox(tr("Log Volts/Amps to VALog.txt"), vaCalGroup);
    m_logVoltsAmpsCheck->setObjectName(QStringLiteral("logVoltsAmpsCheck"));
    vaCalForm->addRow(m_logVoltsAmpsCheck);

    mainLayout->addWidget(vaCalGroup);

    // Note: the per-board PA forward-power cal spinbox group (PaCalibrationGroup)
    // previously lived here as Group 6.  Migrated to PA → Watt Meter
    // (PaWattMeterPage) on 2026-05-02 as part of Setup IA reshape Phase 3A —
    // see docs/architecture/2026-05-02-p1-full-parity-plan.md.

    // =========================================================================
    // Connections: UI -> CalibrationController
    // =========================================================================

    // Freq Cal: freq spinbox changes are not persisted (transient cal param)
    connect(m_freqCalStartBtn, &QPushButton::clicked, this, [this]() {
        emit settingChanged(QStringLiteral("cal/triggerFreqCal"),
                            m_freqCalFreqSpin->value());
    });

    // Level Cal: the question and messages, as message boxes.
    m_levelCalConfirm = [this]() {
        return QMessageBox::question(
                   this, tr("Level Calibration Check"),
                   tr("Is the calibrated signal present at the correct frequency?"),
                   QMessageBox::Yes | QMessageBox::No)
               == QMessageBox::Yes;
    };
    m_levelCalTell = [this](const QString& title, const QString& text, bool warning) {
        if (warning) {
            QMessageBox::warning(this, title, text);
        } else {
            QMessageBox::information(this, title, text);
        }
    };

    // Level Cal: Start / Cancel / Reset
    connect(m_levelCalStartBtn, &QPushButton::clicked,
            this, &CalibrationTab::startLevelCalibration);
    connect(m_levelCalCancelBtn, &QPushButton::clicked, this, [this]() {
        if (!m_model) { return; }
        const QString reason = m_model->requestCancelLevelCalibration();
        if (!reason.isEmpty() && m_levelCalTell) {
            m_levelCalTell(tr("Level Calibration"), reason, true);
        }
    });
    if (m_model) {
        connect(m_model, &RadioModel::levelCalStateChanged,
                this, &CalibrationTab::refreshLevelCalControls);
        connect(m_model, &RadioModel::stationLinkStateChanged,
                this, &CalibrationTab::refreshLevelCalControls);
        // Level Cal 2: whose the station's active slice is decides whether
        // the hosting desktop's Start can run.
        connect(m_model, &RadioModel::activeSliceChanged,
                this, [this](int) { refreshLevelCalControls(); });
        if (SliceOwnership* owners = m_model->sliceOwnership()) {
            connect(owners, &SliceOwnership::markChanged, this,
                    [this](int, const QByteArray&, const QByteArray&) { refreshLevelCalControls(); });
            connect(owners, &SliceOwnership::activeChanged,
                    this, &CalibrationTab::refreshLevelCalControls);
            connect(owners, &SliceOwnership::listenersChanged,
                    this, [this](int) { refreshLevelCalControls(); });
        }
        // The Core refused a start this window sent.
        connect(m_model, &RadioModel::levelCalibrationRefused, this, [this](const QString& reason) {
            m_levelCalStartedHere = false;
            if (m_levelCalTell) {
                m_levelCalTell(tr("Level Calibration"), reason, true);
            }
        });
    }
    // From Thetis setup.cs:24332-24341 [v2.10.3.15]: a Yes/No question with
    // No as the default; Yes runs console.ResetLevelCalibration()
    // (console.cs:46868-46886), which RadioModel ports.
    connect(m_levelCalResetBtn, &QPushButton::clicked, this, [this]() {
        if (!m_model) { return; }
        const QMessageBox::StandardButton answer = QMessageBox::question(
            this, tr("Level Defaults"),
            tr("Do you want to reset Level Calibration back to defaults?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) { return; }
        const QString reason = m_model->requestResetLevelCalibration();
        if (!reason.isEmpty()) {
            QMessageBox::warning(this, tr("Level Defaults"), reason);
        }
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

    // Volts/Amps Calibration -- Thetis groupBoxTS27 wiring.
    // Note: the model-layer setters are still named setPaCurrent{Sensitivity,Offset}
    // pending a separate audit of the persistence keys; the UI-side names match
    // Thetis (udAmpSens / udAmpVoff / btnAmpDefault).
    connect(m_ampSensSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (m_updatingFromModel || !m_calCtrl) { return; }
        m_calCtrl->setPaCurrentSensitivity(v);
        m_calCtrl->save();
        emit settingChanged(QStringLiteral("cal/paSens"), v);
    });
    connect(m_ampVoffSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, [this](double v) {
        if (m_updatingFromModel || !m_calCtrl) { return; }
        m_calCtrl->setPaCurrentOffset(v);
        m_calCtrl->save();
        emit settingChanged(QStringLiteral("cal/paOffset"), v);
    });
    connect(m_ampDefaultBtn, &QPushButton::clicked, this, [this]() {
        if (!m_calCtrl) { return; }
        // From Thetis setup.cs:24346-24352 [v2.10.3.15] btnAmpDefault_Click:
        // the radio model's factory values (GetDefaultVoltCalibration).
        m_calCtrl->restoreDefaultVoltCalibration();
        m_calCtrl->save();
        syncFromController();
        emit settingChanged(QStringLiteral("cal/paDefaultRestored"), true);
    });
    connect(m_logVoltsAmpsCheck, &QCheckBox::toggled, this, [this](bool checked) {
        // Source: console.cs:27460-27463 chkLogVoltsAmps_CheckedChanged -> console.LogVA [@501e3f5]
        // R-R3-49 (found bug): nothing read the box. The station's
        // controller now logs (RadioModel's VoltsAmpsLog); a remote
        // window's change reaches the Core as the stored key.
        if (!m_updatingFromModel && m_calCtrl) {
            m_calCtrl->setLogVoltsAmps(checked);
        }
        emit settingChanged(QStringLiteral("cal/logVoltsAmps"), checked);
    });
    if (m_calCtrl) {
        // The log turns itself off after an hour; the box follows.
        connect(m_calCtrl, &CalibrationController::logVoltsAmpsChanged, this, [this](bool on) {
            QSignalBlocker blocker(m_logVoltsAmpsCheck);
            m_logVoltsAmpsCheck->setChecked(on);
        });
    }

    // Sync from controller if already available
    if (m_calCtrl) {
        syncFromController();
    }
    refreshLevelCalControls();

    // Note: the PaCalibrationGroup live-rebuild on paCalProfileChanged was moved
    // to PaWattMeterPage on 2026-05-02 (Setup IA reshape Phase 3A) when the
    // group itself migrated to PA → Watt Meter.
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
        QSignalBlocker sb7(m_ampSensSpin);
        m_ampSensSpin->setValue(m_calCtrl->paCurrentSensitivity());
    }
    {
        QSignalBlocker sb8(m_ampVoffSpin);
        m_ampVoffSpin->setValue(m_calCtrl->paCurrentOffset());
    }

    {
        // R-R3-49: the Volts/Amps log box, as stored.
        QSignalBlocker sb9(m_logVoltsAmpsCheck);
        m_logVoltsAmpsCheck->setChecked(m_calCtrl->logVoltsAmps());
    }

    m_updatingFromModel = false;
}

// -- populate ------------------------------------------------------------------

void CalibrationTab::populate(const RadioInfo& info, const BoardCapabilities& caps)
{
    // Volts/Amps Calibration changes the PA current reading only on a radio
    // with a current sensor (Thetis HasAmps), and not on the HL2, whose
    // reading uses a fixed sense chain (mi0bot convertToAmps, MI0BOT: HL2
    // current). Elsewhere the values are shown, disabled, with the reason.
    // The transmit-settings gate (setTransmitCalibrationPermitted) disables
    // the group around them on its own.
    QString ampsReason;
    if (!caps.hasPaAmpsTelemetry) {
        ampsReason = tr("This radio does not report PA current.");
    } else if (m_model && m_model->hardwareProfile().model == HPSDRModel::HERMESLITE) {
        ampsReason = tr("The Hermes Lite 2 reads its PA current with fixed scaling. "
                        "These values do not change it.");
    }
    const bool ampsCalApplies = ampsReason.isEmpty();
    HardwareTransmitGate::apply(m_ampSensSpin, ampsCalApplies, ampsReason);
    HardwareTransmitGate::apply(m_ampVoffSpin, ampsCalApplies, ampsReason);
    HardwareTransmitGate::apply(m_ampDefaultBtn, ampsCalApplies, ampsReason);

    // R-R3-46: a remote window has no connect of its own to load the
    // controller, so it reads the Core's radio's saved calibration here
    // (reading only: nothing is written back until an edit).
    if (m_calCtrl && m_model && !m_model->ownsLocalDsp() && !info.macAddress.isEmpty()) {
        m_calCtrl->setMacAddress(info.macAddress);
        m_calCtrl->load();
    }
    // A remote window resets and runs through its Core; a Core that cannot
    // take the request leaves the button disabled with the reason.
    refreshLevelCalControls();
    // Load per-radio calibration settings from controller (set by RadioModel at connect).
    if (m_calCtrl) {
        syncFromController();
    }
    // Note: PaCalibrationGroup repopulation on radio swap is now handled by
    // PaWattMeterPage (Setup IA reshape Phase 3A, 2026-05-02).
}

// -- setTransmitCalibrationPermitted (R-R3-46, R-R3-49) ------------------------

void CalibrationTab::setTransmitCalibrationPermitted(bool permitted, const QString& reason)
{
    HardwareTransmitGate::apply(m_txDisplayGroup, permitted, reason);
    HardwareTransmitGate::apply(m_vaCalGroup, permitted, reason);
}

// -- restoreSettings -----------------------------------------------------------

void CalibrationTab::restoreSettings(const QMap<QString, QVariant>& /*settings*/)
{
    // Calibration settings are loaded via CalibrationController::load() on connect.
    // Nothing to do here — kept for API parity with other tab types.
}

// -- Level Cal: the run -------------------------------------------------------

void CalibrationTab::refreshLevelCalControls()
{
    if (!m_model) {
        const QString noRadio = tr("No radio to calibrate.");
        for (QPushButton* b : {m_levelCalStartBtn, m_levelCalCancelBtn, m_levelCalResetBtn}) {
            b->setEnabled(false);
            b->setToolTip(noRadio);
        }
        return;
    }
    const bool runAvailable = m_model->levelCalibrationRunAvailable();
    const bool resetAvailable = m_model->levelCalibrationResetAvailable();
    const bool running = m_model->levelCalRunning();
    const QString runningReason = tr("A level calibration is running.");

    // From Thetis setup.cs:6525-6526 and 6557-6558 [v2.10.3.15]: Start and
    // Reset are off while the run goes and back on when it ends.
    // Level Cal 2: at the hosting desktop, a station active slice the
    // desktop may not change, with no slice of its own, disables Start
    // with the ownership words (RadioModel::levelCalHostSlice).
    QString hostRefusal;
    m_model->levelCalHostSlice(&hostRefusal);
    m_levelCalStartBtn->setEnabled(runAvailable && !running && hostRefusal.isEmpty());
    m_levelCalStartBtn->setToolTip(
        !runAvailable           ? IStationLink::levelCalibrationRunUnavailableReason()
        : running               ? runningReason
        : !hostRefusal.isEmpty() ? hostRefusal
                                : tr("Measure a signal of the level and frequency above and "
                                     "set the receive level calibration from it."));
    m_levelCalResetBtn->setEnabled(resetAvailable && !running);
    m_levelCalResetBtn->setToolTip(
        !resetAvailable ? IStationLink::levelCalibrationResetUnavailableReason()
        : running       ? runningReason
                        : tr("Put the receive level calibration back to this radio's defaults."));
    m_levelCalCancelBtn->setEnabled(runAvailable && running);
    m_levelCalCancelBtn->setToolTip(
        !runAvailable ? IStationLink::levelCalibrationRunUnavailableReason()
        : running     ? tr("Stop the level calibration and put the receiver back.")
                      : tr("Nothing to stop: no level calibration is running."));

    m_levelCalProgress->setValue(m_model->levelCalPercent());
    m_levelCalStatusLabel->setText(running ? QString() : m_model->levelCalMessage());

    const bool ended = m_levelCalWasRunning && !running;
    m_levelCalWasRunning = running;
    if (ended && m_levelCalStartedHere) {
        m_levelCalStartedHere = false;
        // From Thetis setup.cs:6556 and 6561-6567 [v2.10.3.15]:
        // showCalibrateDone("Level Calibration complete.") under
        // "Calibration", only when the run finished.
        if (m_model->levelCalSucceeded() && m_levelCalTell) {
            m_levelCalTell(tr("Calibration"), tr("Level Calibration complete."), false);
        }
    }
}

void CalibrationTab::startLevelCalibration()
{
    if (!m_model || !m_model->levelCalibrationRunAvailable() || m_model->levelCalRunning()) {
        return;
    }
    // From Thetis setup.cs:6518-6523 [v2.10.3.15]
    if (!m_levelCalConfirm()) {
        return; //MW0LGE_[2.9.0.6] double check we want to do this, prevents accidental click from changing config
    }
    // Thetis calibrates RX1 on VFO A; here the active slice (-1).
    // Level Cal 2: a remote window names its own active slice
    // (RadioModel::requestStartLevelCalibration). At the hosting desktop -1
    // is the station's active slice while the station may change it; else
    // the desktop's own active slice; with neither Start is disabled with
    // the reason (refreshLevelCalControls), and refused here the same way.
    QString hostRefusal;
    const int sliceId = m_model->levelCalHostSlice(&hostRefusal);
    if (!hostRefusal.isEmpty()) {
        if (m_levelCalTell) {
            m_levelCalTell(tr("Level Calibration"), hostRefusal, true);
        }
        return;
    }
    m_levelCalStartedHere = true;
    const QString reason = m_model->requestStartLevelCalibration(
        static_cast<float>(m_levelCalLevelSpin->value()), m_levelCalFreqSpin->value(), sliceId);
    if (!reason.isEmpty()) {
        m_levelCalStartedHere = false;
        if (m_levelCalTell) {
            m_levelCalTell(tr("Level Calibration"), reason, true);
        }
    }
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

void CalibrationTab::setLevelCalPromptsForTest(
    std::function<bool()> confirm,
    std::function<void(const QString& title, const QString& text, bool warning)> tell)
{
    m_levelCalConfirm = std::move(confirm);
    m_levelCalTell = std::move(tell);
}
#endif

} // namespace NereusSDR
