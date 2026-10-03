// 2026-09-27: activate the validated Core transmit-region control.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-09-28: Extended is the Core's ExtendedTransmit setting (addendum
// G-42). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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
//   2026-09-23 - R-R3-21 / R-R3-10: the Region combo (a Core setting) is disabled
//                 while a remote window does not have the Core's settings.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-23 - R-R3-46 / R-R3-21: in a remote window the Step Attenuator
//                 and Auto Attenuate groups follow the Core's `stepAtt`
//                 object. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 / R-R3-21: the Network Watchdog checkbox is the
//                 Core's in a remote window (gated, older-Core note).
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 38 (R-IOS-04, D29): the Time Out
//                 Timers group (Thetis groupBoxTS32) plus the time-out for
//                 phones and tablets. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-26 - Merge of Tasks 38 and 39: timeOutNeedsNewerCoreText (the
//                 older-Core gate) and disabled controls that look
//                 disabled. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 16: Receive Only is
//                 never hidden and follows RadioModel::isRxOnly.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-30 - Level Cal 2 review: rx2StepAttMaxDb, the top of the RX2
//                 box. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
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

#include <functional>

#include "gui/SetupPage.h"

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QLabel;

namespace NereusSDR {

class StepAttenuatorController;
class StepAttenuatorFacade;
struct RadioInfo;

class GeneralOptionsPage : public SetupPage {
    Q_OBJECT
public:
    explicit GeneralOptionsPage(RadioModel* model, QWidget* parent = nullptr);

    void syncFromModel() override;
    void reloadFeedbackPreferences();

    /// Task 16: the question asked before Receive Only is turned off
    /// (setup.cs:6484 [v2.10.3.15]). Tests answer it instead of a message
    /// box; true means Yes.
    void setEnableTransmitConfirmForTest(std::function<bool()> confirm);

    // R-R3-21 / R-R3-10: the Region is the Core's setting (where the radio is),
    // so its combo is disabled while the Core's settings are unavailable;
    // so is the Network Watchdog checkbox (R-R3-49). The rest of the page is
    // this computer's.
    void setStationSettingsAvailable(bool available, const QString& reason) override;
    /// Merge of Tasks 38 and 39: the reason the Time Out Timers group is
    /// disabled on a Core older than the transmit time-out.
    static QString timeOutNeedsNewerCoreText();

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
    void refreshRegionAvailability();
    bool regionEditAvailable();
    // Addendum G-42: Extended shows the Core's ExtendedTransmit and is
    // usable where the Core takes the change (this computer's own radio
    // off the air, or a Core at transmitSettingsVersion 12 off the air).
    void refreshExtendedAvailability();
    bool extendedEditAvailable();
    void syncExtendedFromSetting();
    // JJ's ruling (2026-09-29): Prevent transmitting on a different band
    // is the Core's PreventTxOnDifferentBandToRx, gated as Extended at
    // transmitSettingsVersion 14 (kTransmitSettingsDifferentBandVersion).
    void refreshPreventDifferentBandAvailability();
    bool preventDifferentBandEditAvailable();
    void syncPreventDifferentBandFromSetting();
    // The shared gate of the Core's transmit-gate settings: usable where the
    // Core takes the change (this computer's own radio off the air with
    // nobody else holding transmit, or a Core at `version` that permits
    // this device to transmit, off the air), and the plain reason if not.
    bool transmitGateEditAvailable(int version);
    QString transmitGateReason(int version, const QString& olderCoreText,
                               const QString& unavailableText);
    bool m_regionSettingsAvailable{false};
    QString m_regionSettingsReason;
    void buildHardwareConfigGroup();
    void buildOptionsGroup();
    // iPhone app plan Task 38: Thetis's Time Out Timers group
    // (setup.designer.cs:10154-10299 [v2.10.3.15]) plus the time-out for
    // phones and tablets.
    void buildTimeOutGroup();
    void buildStepAttGroup();
    void buildAutoAttGroup();
    void connectController();
    // Issue #259 — pull the controller's already-restored state into the
    // widgets at construction time so that lazy SetupDialog construction
    // (after RadioModel::loadSliceState has triggered the controller's
    // own loadSettings) doesn't surface stale defaults.
    void initFromController();

    // R-R3-46 / R-R3-21: a remote window's groups show and write the Core's
    // `stepAtt` object (m_ctrl stays null there), enabled while the Core
    // takes the window's edits.
    void connectFacade();
    void syncFromFacade();
    // R-R3-46 / R-R3-11: the RX2 row (the other ADC's own attenuator):
    // value, and enabled or disabled with its reason.
    void refreshRx2StepAtt();
    // Level Cal 2 review: the top of the RX2 box (31 dB, RX1's while linked).
    int rx2StepAttMaxDb() const;
    void applyRadioHardwareAvailability();

    // Task 16: Receive Only follows the model; a radio with no transmitter
    // locks it on (disabled, with the reason).
    void syncReceiveOnly();
    void setReceiveOnlyLocked(bool locked, const QString& reason);
    bool confirmEnableTransmit();

    StepAttenuatorController* m_ctrl{nullptr};
    StepAttenuatorFacade* m_stepAtt{nullptr};

    // Hardware Configuration group
    // From Thetis setup.designer.cs:8045-8396 [v2.10.3.13] (tpGeneralHardware)
    QComboBox* m_comboFRSRegion{nullptr};
    QCheckBox* m_chkExtended{nullptr};
    QLabel*    m_lblWarningRegionExtended{nullptr};
    QCheckBox* m_chkGeneralRXOnly{nullptr};
    // Task 16: shown when an older Core refuses a window's change.
    QLabel*    m_lblRxOnlyCore{nullptr};
    std::function<bool()> m_confirmEnableTransmit;
    QCheckBox* m_chkNetworkWDT{nullptr};
    // R-R3-49: shown when an older Core refuses a window's change.
    QLabel*    m_lblNetworkWDTCore{nullptr};

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

    // Time Out Timers group (iPhone app plan Task 38). From Thetis
    // groupBoxTS32 on tpOptions2 (setup.designer.cs:10154-10299
    // [v2.10.3.15]); the phone and iPad row is NereusSDR's (D29).
    QGroupBox* m_grpTimeOut{nullptr};
    QCheckBox* m_chkToTMox{nullptr};
    QSpinBox*  m_udMoxToTSeconds{nullptr};
    QLabel*    m_lblMoxTotSec{nullptr};
    QCheckBox* m_chkToTPing{nullptr};
    QSpinBox*  m_udPingToTSeconds{nullptr};
    QLabel*    m_lblPingTotSec{nullptr};
    QLineEdit* m_txtToTPingIP{nullptr};
    QPushButton* m_btnPingDef{nullptr};
    QCheckBox* m_chkRemoteMoxTimeOut{nullptr};
    QSpinBox*  m_udRemoteMoxTimeOutSeconds{nullptr};
    QLabel*    m_lblRemoteMoxTotSec{nullptr};

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
