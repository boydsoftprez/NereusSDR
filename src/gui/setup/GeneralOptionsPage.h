// =================================================================
// src/gui/setup/GeneralOptionsPage.h  (NereusSDR)
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

#pragma once

#include "gui/SetupPage.h"

class QCheckBox;
class QComboBox;
class QSpinBox;
class QLabel;

namespace NereusSDR {

class StepAttenuatorController;
struct RadioInfo;

class GeneralOptionsPage : public SetupPage {
    Q_OBJECT
public:
    explicit GeneralOptionsPage(RadioModel* model, QWidget* parent = nullptr);

    void syncFromModel() override;

    /// Show or hide the Receive Only checkbox.  Hidden by default per
    /// Thetis setup.designer.cs:8535-8544 [v2.10.3.13] (Visible=false).
    /// Called by the constructor on initial connect and by currentRadioChanged
    /// so reconnects to a different radio (e.g. full-TX board after an HL2-RX)
    /// update visibility correctly.  BoardCapabilities::isRxOnlySku
    /// (NereusSDR-original) is the authoritative source.
    void setReceiveOnlyVisible(bool visible);

signals:
    // Phase 3M-4 Task 11: PureSignal Info Bar checkboxes.
    // Mirror Thetis groupBoxTS23 controls on tpOptions2 ("Info Bar (below
    // spectrum)") setup.designer.cs:10560-10632 [v2.10.3.13].  RadioModel /
    // MainWindow routes both signals to PureSignal::setHideFeedback /
    // PureSignal::setInvertRedBlue so the live banner updates without a
    // Setup dialog close.

    // Mirror of Thetis chkHideFeebackLevel (typo "Feeback" preserved in
    // source-cite for traceability; corrected spelling used in user text).
    // From setup.designer.cs:10571 [v2.10.3.13].
    void hideFeedbackLevelChanged(bool on);

    // Mirror of Thetis chkSwapREDBluePSAColours.
    // From setup.designer.cs:10572 [v2.10.3.13].
    void invertRedBluePsaChanged(bool on);

    // Emitted when the CPU meter rate spinbox value changes.
    // Value is in Hz (1-30). Forwarded by SetupDialog →
    // MainWindow::setCpuTimerIntervalHz().
    void cpuMeterRateChanged(int hz);

private slots:
    // 3M-1a G.2 fixup: named slot mirrors HardwarePage::onCurrentRadioChanged.
    // Eliminates the capture-by-pointer shutdown race of the original lambda
    // and brings the two setup pages into stylistic parity.
    void onCurrentRadioChanged(const NereusSDR::RadioInfo& info);

private:
    void buildHardwareConfigGroup();
    void buildOptionsGroup();
    void buildStepAttGroup();
    void buildAutoAttGroup();
    void connectController();
    // Issue #259 — pull the controller's already-restored state into the
    // widgets at construction time so that lazy SetupDialog construction
    // (after RadioModel::loadSliceState has triggered the controller's
    // own loadSettings) doesn't surface stale defaults.
    void initFromController();

    StepAttenuatorController* m_ctrl{nullptr};

    // Hardware Configuration group
    // From Thetis setup.designer.cs:8045-8396 [v2.10.3.13] (tpGeneralHardware)
    QComboBox* m_comboFRSRegion{nullptr};
    QCheckBox* m_chkExtended{nullptr};
    QLabel*    m_lblWarningRegionExtended{nullptr};
    QCheckBox* m_chkGeneralRXOnly{nullptr};
    QCheckBox* m_chkNetworkWDT{nullptr};

    // Options group
    // From Thetis setup.designer.cs:9050-9059 [v2.10.3.13] (grpGeneralOptions)
    QCheckBox* m_chkPreventTXonDifferentBandToRX{nullptr};
    QSpinBox*  m_cpuMeterRateHz{nullptr};

    // Phase 3M-4 Task 11: PureSignal Info Bar checkboxes.
    // Mirror of Thetis groupBoxTS23 ("Info Bar (below spectrum)") controls
    // chkHideFeebackLevel + chkSwapREDBluePSAColours on tpOptions2
    // (setup.designer.cs:10560-10632 [v2.10.3.13]).  Thetis places these
    // on a dedicated Info Bar groupbox; NereusSDR's IA folds them into the
    // existing General Options group to keep the Setup tree shallow.
    QCheckBox* m_chkHideFeedback{nullptr};
    QCheckBox* m_chkSwapRedBlue{nullptr};

    // radio-mic-input Task 23 — Disable PTT / All-Mode Mic PTT / PTT Out Delay.
    // From Thetis setup.designer.cs:9217-9237 + 9176-9204 [v2.10.3.13+501e3f51]
    // (chkGenAllModeMicPTT, chkGeneralDisablePTT, udGenPTTOutDelay).
    QCheckBox* m_disablePttCheck{nullptr};
    QCheckBox* m_allModeMicPttCheck{nullptr};
    QSpinBox*  m_pttOutDelaySpin{nullptr};

    // Step Attenuator group
    QCheckBox* m_chkRx1StepAttEnable{nullptr};
    QSpinBox*  m_spnRx1StepAttValue{nullptr};
    QCheckBox* m_chkRx2StepAttEnable{nullptr};
    QSpinBox*  m_spnRx2StepAttValue{nullptr};
    QLabel*    m_lblAdcLinked{nullptr};

    // Auto Attenuate RX1
    QCheckBox* m_chkAutoAttRx1{nullptr};
    QComboBox* m_cmbAutoAttRx1Mode{nullptr};
    QCheckBox* m_chkAutoAttUndoRx1{nullptr};
    QSpinBox*  m_spnAutoAttHoldRx1{nullptr};

    // Auto Attenuate RX2
    QCheckBox* m_chkAutoAttRx2{nullptr};
    QComboBox* m_cmbAutoAttRx2Mode{nullptr};
    QCheckBox* m_chkAutoAttUndoRx2{nullptr};
    QSpinBox*  m_spnAutoAttHoldRx2{nullptr};
};

} // namespace NereusSDR
