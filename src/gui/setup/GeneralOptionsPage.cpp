// 2026-09-27: activate the validated Core transmit-region control.
// J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
// 2026-09-28: Extended is the Core's ExtendedTransmit setting (addendum
// G-42); the old per-computer ExtendedTxAllowed is ignored. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-29: a hosting desktop's Extended waits while another device
// holds transmit. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// 2026-09-29: Prevent transmitting on a different band is shown and is the
// Core's PreventTxOnDifferentBandToRx setting, gated as Extended. J.J. Boyd
// (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
// src/gui/setup/GeneralOptionsPage.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-09-29 - Prevent TX'ing on a different band is shown again as the
//                 Core's setting, compared against the device's other slices.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-27 - Task 25: retire the obsolete split-band checkbox and show
//                 enforced, disabled TX policy values pending Core policy.
//                 J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-23 - R-R3-21: the Step Attenuator and Auto Attenuate groups are
//                 disabled with a plain reason on a remote-station model.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-23 - R-R3-21 / R-R3-10: the Region combo (a Core setting) is disabled
//                 while a remote window does not have the Core's settings.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-23 - R-R3-46 / R-R3-21: in a remote window the Step Attenuator
//                 and Auto Attenuate groups show and write the Core's
//                 `stepAtt` object, enabled while the Core takes the
//                 window's edits and otherwise disabled with its plain
//                 reason. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 / R-R3-21: the Network Watchdog checkbox reaches the
//                 radio where it is (this window's, or the Core's from a
//                 remote window), is disabled with the Core's reason while
//                 its settings are unavailable, and says the Core needs
//                 updating when an older Core refuses it. J.J. Boyd (KG4VCF),
//                 with AI-assisted implementation via Anthropic Claude Code.
//   2026-09-24 - R-R3-49 fix wave: the tooltip says the box sets how long
//                 NereusSDR waits before it treats the radio as lost; the
//                 radio's safety timer stays on (operator decision).
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-25 - iPhone app plan Task 38 (R-IOS-04, D29): the Time Out
//                 Timers group from Thetis's Options-2 tab (groupBoxTS32:
//                 MOX, Ping and its host) plus the time-out for phones
//                 and tablets; all its settings are the Core's. J.J. Boyd
//                 (KG4VCF), with AI-assisted implementation via Anthropic
//                 Claude Code.
//   2026-09-26 - Merge of Tasks 38 and 39: an older Core, which stores the
//                 time-out settings and ignores them, shows the Time Out
//                 Timers group disabled with an "update the Core" reason;
//                 a disabled control on this page looks disabled in the
//                 dark theme. J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-26 - Trunk merge of remote transmit (R-R3-49): the page's own
//                 disabled-look rules dropped; Style::applyDarkPageStyle's
//                 shared darkPageDisabledRules() draws them now. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - Receiver and transmit gaps plan, Task 16: Receive Only is
//                 shown on every radio and reaches the keying gate
//                 (RadioModel::setRxOnly, the Core's from a remote window);
//                 turning it off asks first (chkGeneralRXOnly_CheckedChanged,
//                 setup.cs:6479 [v2.10.3.15]); on a radio with no
//                 transmitter it is checked and disabled with the reason.
//                 J.J. Boyd (KG4VCF), with AI-assisted implementation via
//                 Anthropic Claude Code.
//   2026-09-29 - R-R3-46 / R-R3-11: the RX2 row shows and sets the other
//                 receive ADC's own attenuator and RX2's own enable, and
//                 Auto Attenuate RX2 sets RX2's own Enable, Undo and Hold
//                 (Thetis chkRX2StepAtt, chkAutoATTRx2, chkAutoAttUndoRX2,
//                 nudAutoAttHoldRX2), locally and on the Core; disabled
//                 with the reason on a one-ADC radio or an older Core.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - The Region combo's IARU entries read "IARU Region 1".."IARU
//                 Region 3" (display text only; the saved value is
//                 unchanged). J.J. Boyd (KG4VCF), with AI-assisted
//                 implementation via Anthropic Claude Code.
//   2026-09-30 - Level Cal 2 review: the RX2 box stops at 31 dB, the
//                 second ADC's field (rx2StepAttMaxDb), except while linked.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
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
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/PureSignal.h"
#include "core/StepAttenuatorController.h"
#include "core/StepAttenuatorFacade.h"
#include "core/session/IStationLink.h"
#include "core/session/StationCapabilities.h"
#include "core/settings/SettingsProxy.h"
#include "core/safety/BandPlanGuard.h"

#include <algorithm>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHostAddress>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QCheckBox>
#include <QComboBox>
#include <QSignalBlocker>
#include <QSpinBox>

namespace NereusSDR {

namespace {

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
    NereusSDR::Style::applyDarkPageStyle(this);
    // R-R3-46: a remote window has no attenuator of its own; its groups
    // follow the Core's (the mirrored `stepAtt` object).
    m_stepAtt = (model && !model->ownsLocalDsp()) ? model->stepAttFacade() : nullptr;
    m_ctrl = (model && !m_stepAtt) ? model->stepAttController() : nullptr;

    buildHardwareConfigGroup();
    buildOptionsGroup();
    buildTimeOutGroup();
    buildStepAttGroup();
    buildAutoAttGroup();

    m_regionSettingsAvailable = model && model->ownsLocalDsp();
    if (model) {
        const auto refresh = [this]() {
            refreshRegionAvailability();
            refreshExtendedAvailability();
            refreshPreventDifferentBandAvailability();
        };
        connect(model, &RadioModel::coreOnAirChanged, this, refresh);
        connect(model, &RadioModel::stationLinkStateChanged, this, refresh);
        connect(model, &RadioModel::transmittingChanged, this, refresh);
        connect(&model->transmitModel(), &TransmitModel::moxChanged, this, refresh);
        connect(&model->transmitModel(), &TransmitModel::tuneChanged, this, refresh);
        connect(&model->transmitModel(), &TransmitModel::twoToneActiveChanged, this, refresh);
    }
    refreshRegionAvailability();
    refreshExtendedAvailability();
    refreshPreventDifferentBandAvailability();
    // Addendum G-42: every device shows the Core's Extended. A remote
    // window hears the Core's change (stationSettingChanged, empty for a
    // whole snapshot) and its own refused write; the Core's own page hears
    // a device's change it took (transmitGateSettingChanged).
    if (model) {
        const auto follow = [this](const QString& key) {
            if (key.isEmpty() || key == QLatin1String(RadioModel::kExtendedTransmitKey)) {
                syncExtendedFromSetting();
                refreshExtendedAvailability();
            }
            if (key.isEmpty() || key == QLatin1String(RadioModel::kPreventTxOnDifferentBandKey)) {
                syncPreventDifferentBandFromSetting();
                refreshPreventDifferentBandAvailability();
            }
        };
        connect(model, &RadioModel::stationSettingChanged, this, follow);
        connect(model, &RadioModel::transmitGateSettingChanged, this, follow);
        connect(model, &RadioModel::transmitHolderChanged, this,
                [this]() {
                    refreshExtendedAvailability();
                    refreshPreventDifferentBandAvailability();
                });
    }
    if (auto* proxy = dynamic_cast<SettingsProxy*>(AppSettings::instance().remoteBackend())) {
        connect(proxy, &SettingsProxy::valueRejected, this,
                [this](const QString& key, const QVariant&) {
            if (key == QLatin1String(RadioModel::kExtendedTransmitKey)) {
                syncExtendedFromSetting();
                refreshExtendedAvailability();
            }
            if (key == QLatin1String(RadioModel::kPreventTxOnDifferentBandKey)) {
                syncPreventDifferentBandFromSetting();
                refreshPreventDifferentBandAvailability();
            }
        });
    }

    // Task 16: the Receive Only checkbox follows the model's receive-only
    // state (Thetis console.RXOnly keeps SetupForm.RXOnly in step,
    // console.cs:15328-15332 [v2.10.3.15]), including a Core's change seen
    // from a remote window and a radio with no transmitter.
    if (model) {
        connect(model, &RadioModel::rxOnlyChanged, this,
                [this](bool) { syncReceiveOnly(); });
        connect(model, &RadioModel::currentRadioChanged,
                this, &GeneralOptionsPage::onCurrentRadioChanged);
        // R-R3-46 / R-R3-11: the RX2 row follows the radio's board and, in
        // a remote window, whether the Core sends RX2's value.
        connect(model, &RadioModel::currentRadioChanged,
                this, [this]() { refreshRx2StepAtt(); });
        connect(model, &RadioModel::stationLinkStateChanged,
                this, [this]() { refreshRx2StepAtt(); });
    }
    syncReceiveOnly();

    if (m_ctrl) {
        // hermes-filter-debug Bug 1: pull BOTH bounds from the controller —
        // HL2 uses the signed -28..+31 range (mi0bot setup.cs:16085-16086
        // [v2.10.3.13-beta2]).  The previous hardcoded `0` minimum clamped
        // any HL2 negative-dB value the user typed back to zero.
        const int minDb = m_ctrl->minAttenuation();
        const int maxDb = m_ctrl->maxAttenuation();
        m_spnRx1StepAttValue->setRange(minDb, maxDb);
        m_spnRx2StepAttValue->setRange(minDb, rx2StepAttMaxDb());
        connectController();
        // Issue #259 — pull the controller's already-restored state into
        // the widgets. Must run AFTER connectController() so that any
        // controller signal that fires later goes to wired slots; must run
        // AT construction so the cold-open case (page constructed after
        // RadioModel::loadSliceState has already triggered the controller's
        // loadSettings) doesn't show stale defaults. See the function-level
        // comment in initFromController() for the full lazy-construct trace.
        initFromController();
    }

    // R-R3-46 / R-R3-21: a remote window shows the Core's values and is
    // usable only while the Core takes its edits; until then the two
    // groups are disabled with the object's plain reason. The Hardware
    // Configuration and Options groups are left as they are.
    if (m_stepAtt) {
        connectFacade();
        syncFromFacade();
        applyRadioHardwareAvailability();
    }
}

void GeneralOptionsPage::setStationSettingsAvailable(bool available, const QString& reason)
{
    m_regionSettingsAvailable = available;
    m_regionSettingsReason = reason;
    refreshExtendedAvailability();
    refreshPreventDifferentBandAvailability();
    // R-R3-49: the Network Watchdog is the Core's setting too; so is
    // Receive Only (Task 16). The radio-has-no-transmitter lock sits on top
    // of the Core's gate, so it is taken off first and put back after.
    setReceiveOnlyLocked(false, QString());
    gateStationControls({m_chkNetworkWDT, m_chkGeneralRXOnly},
                        available, reason);
    refreshRegionAvailability();
    syncReceiveOnly();
    // iPhone app plan Task 38: so is every transmit time-out. A Core older
    // than the time-out stores these settings and ignores them, so a window
    // on it shows the group disabled and says why.
    const IStationLink* link = model() ? model()->stationLink() : nullptr;
    const bool olderCore = available && link != nullptr && !link->transmitTimeOutAvailable();
    gateStationControls({m_grpTimeOut}, available && !olderCore,
                        olderCore ? timeOutNeedsNewerCoreText() : reason);
}

bool GeneralOptionsPage::regionEditAvailable()
{
    const RadioModel* radio = model();
    if (!radio || !m_regionSettingsAvailable) { return false; }
    if (radio->ownsLocalDsp()) { return !radio->stationOnAirRefusal(nullptr); }
    const IStationLink* link = radio->stationLink();
    return link && link->transmitSettingsAvailable(9) && !radio->isCoreOnAir();
}

void GeneralOptionsPage::refreshRegionAvailability()
{
    const bool enabled = regionEditAvailable();
    m_comboFRSRegion->setEnabled(enabled);
    QString reason;
    if (!enabled) {
        const bool onAir = model() && (model()->ownsLocalDsp()
            ? model()->stationOnAirRefusal(nullptr) : model()->isCoreOnAir());
        reason = !m_regionSettingsAvailable && !m_regionSettingsReason.isEmpty()
            ? m_regionSettingsReason
            : onAir ? RadioModel::onAirReason()
            : tr("Region selection is not available for transmit on this Core.");
    }
    m_comboFRSRegion->setToolTip(enabled ? tr("Select Region for your location") : reason);
    m_comboFRSRegion->setAccessibleDescription(reason);
}

bool GeneralOptionsPage::transmitGateEditAvailable(int version)
{
    const RadioModel* radio = model();
    if (!radio || !m_regionSettingsAvailable) { return false; }
    // Scoped review: a desktop hosting the Core waits, as the Core's other
    // devices do, while another device holds transmit.
    if (radio->ownsLocalDsp()) {
        return !radio->stationOnAirRefusal(nullptr) && radio->otherDeviceHoldsRefusal().isEmpty();
    }
    const IStationLink* link = radio->stationLink();
    // The Core takes the change only from a device it permits to transmit,
    // so without that the box is disabled rather than refused after a tick.
    return link && link->transmitSettingsAvailable(version) && link->transmitSettingsPermitted()
        && !radio->isCoreOnAir();
}

QString GeneralOptionsPage::transmitGateReason(int version, const QString& olderCoreText,
                                               const QString& unavailableText)
{
    const RadioModel* radio = model();
    const bool onAir = radio && (radio->ownsLocalDsp()
        ? radio->stationOnAirRefusal(nullptr) : radio->isCoreOnAir());
    const IStationLink* link = radio ? radio->stationLink() : nullptr;
    const bool olderCore = radio && !radio->ownsLocalDsp() && m_regionSettingsAvailable
        && link && !link->transmitSettingsAvailable(version);
    const bool notPermitted = radio && !radio->ownsLocalDsp() && link
        && !link->transmitSettingsPermitted();
    return !m_regionSettingsAvailable && !m_regionSettingsReason.isEmpty()
        ? m_regionSettingsReason
        : onAir ? RadioModel::onAirReason()
        : radio && radio->ownsLocalDsp() && !radio->otherDeviceHoldsRefusal().isEmpty()
          ? radio->otherDeviceHoldsRefusal()
        : olderCore ? olderCoreText
        : notPermitted ? link->transmitPermissionReason()
        : unavailableText;
}

bool GeneralOptionsPage::extendedEditAvailable()
{
    return transmitGateEditAvailable(12);
}

bool GeneralOptionsPage::preventDifferentBandEditAvailable()
{
    return transmitGateEditAvailable(kTransmitSettingsDifferentBandVersion);
}

void GeneralOptionsPage::syncPreventDifferentBandFromSetting()
{
    if (!m_chkPreventTXonDifferentBandToRX) { return; }
    const QSignalBlocker blocked(m_chkPreventTXonDifferentBandToRX);
    m_chkPreventTXonDifferentBandToRX->setChecked(RadioModel::preventTxOnDifferentBandSetting());
}

void GeneralOptionsPage::refreshPreventDifferentBandAvailability()
{
    if (!m_chkPreventTXonDifferentBandToRX) { return; }
    const bool enabled = preventDifferentBandEditAvailable();
    m_chkPreventTXonDifferentBandToRX->setEnabled(enabled);
    const QString reason = enabled ? QString() : transmitGateReason(
        kTransmitSettingsDifferentBandVersion,
        tr("This Core does not have Prevent transmitting on a different band. Update the "
           "Core to use it."),
        tr("Prevent transmitting on a different band is not available on this Core."));
    // Thetis's enabled tooltip (setup.designer.cs:9098-9107 [v2.10.3.15])
    // says "the RX band"; NereusSDR has no split, so ours names the active
    // slice, the one this device listens on.
    m_chkPreventTXonDifferentBandToRX->setToolTip(
        enabled ? tr("Refuse to transmit when the transmitting slice is not your active slice "
                     "and is on a different band from it")
                : reason);
    m_chkPreventTXonDifferentBandToRX->setAccessibleDescription(reason);
}

void GeneralOptionsPage::syncExtendedFromSetting()
{
    if (!m_chkExtended) { return; }
    const QSignalBlocker blocked(m_chkExtended);
    m_chkExtended->setChecked(RadioModel::extendedTransmitSetting());
}

void GeneralOptionsPage::refreshExtendedAvailability()
{
    if (!m_chkExtended) { return; }
    const bool enabled = extendedEditAvailable();
    m_chkExtended->setEnabled(enabled);
    const QString reason = enabled ? QString() : transmitGateReason(
        12, tr("This Core does not have Extended transmit. Update the Core to use it."),
        tr("Extended transmit is not available on this Core."));
    // From Thetis setup.designer.cs:8121 [v2.10.3.15] (the enabled tooltip).
    m_chkExtended->setToolTip(enabled ? tr("Enable extended TX (out of band)") : reason);
    m_chkExtended->setAccessibleDescription(reason);
}

QString GeneralOptionsPage::timeOutNeedsNewerCoreText()
{
    return tr("This Core does not have the transmit time-out. Update the Core to use these "
              "settings.");
}

// ---------------------------------------------------------------------------
// Task 16: Receive Only.
//
// Thetis's designer hides chkGeneralRXOnly (setup.designer.cs:8535-8544
// [v2.10.3.13], Visible=false), and comboRadioModel_SelectedIndexChanged
// shows it for every model (setup.cs:19878, 19911 and on [v2.10.3.15]);
// mi0bot-Thetis shows it for the HL2 (setup.cs:20199 [v2.10.3.13-beta2]).
// NereusSDR shows it on every radio too, and with no radio (the operator,
// 2026-09-25: a control that cannot run is shown disabled with its reason,
// never hidden). On a radio with no
// transmitter (BoardCapabilities::isRxOnlySku, the HL2 receive-only kit) it
// is checked and disabled with that reason: NereusSDR's own rule, since
// mi0bot-Thetis has no kit model, only the operator's toggle.
// ---------------------------------------------------------------------------
void GeneralOptionsPage::syncReceiveOnly()
{
    if (!m_chkGeneralRXOnly) {
        return;
    }
    RadioModel* radio = model();
    const bool on = radio ? radio->isRxOnly() : RadioModel::rxOnlySetting();
    const bool forced = radio && radio->isRxOnlyForced();
    {
        const QSignalBlocker blocker(m_chkGeneralRXOnly);
        m_chkGeneralRXOnly->setChecked(on);
    }
    setReceiveOnlyLocked(forced, RadioModel::rxOnlyForcedReason());
}

void GeneralOptionsPage::setReceiveOnlyLocked(bool locked, const QString& reason)
{
    static constexpr auto kSavedTooltip = "GeneralRxOnlyLockedTooltip";
    static constexpr auto kSavedDescription = "GeneralRxOnlyLockedDescription";
    static constexpr auto kSavedEnabled = "GeneralRxOnlyLockedEnabled";
    QCheckBox* box = m_chkGeneralRXOnly;
    if (!box) {
        return;
    }
    if (locked) {
        if (!box->property(kSavedTooltip).isValid()) {
            box->setProperty(kSavedTooltip, box->toolTip());
            box->setProperty(kSavedDescription, box->accessibleDescription());
            box->setProperty(kSavedEnabled, box->isEnabled());
        }
        box->setEnabled(false);
        box->setToolTip(reason);
        box->setAccessibleDescription(reason);
    } else if (box->property(kSavedTooltip).isValid()) {
        box->setEnabled(box->property(kSavedEnabled).toBool());
        box->setToolTip(box->property(kSavedTooltip).toString());
        box->setAccessibleDescription(box->property(kSavedDescription).toString());
        box->setProperty(kSavedTooltip, QVariant());
        box->setProperty(kSavedDescription, QVariant());
        box->setProperty(kSavedEnabled, QVariant());
    }
}

bool GeneralOptionsPage::confirmEnableTransmit()
{
    if (m_confirmEnableTransmit) {
        return m_confirmEnableTransmit();
    }
    // From Thetis setup.cs:6484-6490 [v2.10.3.15]:
    //   DialogResult dr = MessageBox.Show(
    //       "Unchecking Receive Only may \n" +
    //       "cause damage to your hardware.  Are you sure you want \n" +
    //       "to enable transmit?",
    //       "Warning: Enable Transmit?",
    //       MessageBoxButtons.YesNo,
    //       MessageBoxIcon.Warning, MessageBoxDefaultButton.Button2, Common.MB_TOPMOST); //MW0LGE_[2.9.0.7]);
    const QMessageBox::StandardButton answer = QMessageBox::warning(
        this, tr("Warning: Enable Transmit?"),
        tr("Unchecking Receive Only may cause damage to your hardware. "
           "Are you sure you want to enable transmit?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return answer == QMessageBox::Yes;
}

void GeneralOptionsPage::setEnableTransmitConfirmForTest(std::function<bool()> confirm)
{
    m_confirmEnableTransmit = std::move(confirm);
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
        // Task 16: a radio with no transmitter locks Receive Only on.
        syncReceiveOnly();

        // hermes-filter-debug Bug 1: re-range the step-att spinboxes when
        // the connected board changes (e.g. user switches HL2 ↔ ANAN-G2
        // without restarting Setup).  The controller min/max are pushed
        // by RadioModel::connectToRadio from boardCapabilities().attenuator
        // before this signal fires.
        if (m_ctrl && m_spnRx1StepAttValue && m_spnRx2StepAttValue) {
            const int minDb = m_ctrl->minAttenuation();
            const int maxDb = m_ctrl->maxAttenuation();
            m_spnRx1StepAttValue->setRange(minDb, maxDb);
            m_spnRx2StepAttValue->setRange(minDb, rx2StepAttMaxDb());
        }
    }
}

// ---------------------------------------------------------------------------
// Hardware Configuration group
// From Thetis setup.designer.cs:8045-8396 [v2.10.3.13] (tpGeneralHardware)
// Controls: comboFRSRegion, chkExtended, lblWarningRegionExtended,
//           chkGeneralRXOnly (shown on every radio), chkNetworkWDT
//           (default ON).
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
    m_comboFRSRegion->setProperty("nereusSetupId", "general.options.region");
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
        // Thetis shows "Region1".."Region3"; NereusSDR names them "IARU
        // Region 1".."IARU Region 3" (display text only: the saved
        // BandPlanRegion value is still the entry's number, 20..22).
        QStringLiteral("IARU Region 1"),
        QStringLiteral("IARU Region 2"),
        QStringLiteral("IARU Region 3"),
        QStringLiteral("Germany"),
    });
    // From Thetis setup.designer.cs:8113 [v2.10.3.13]
    m_comboFRSRegion->setToolTip(QStringLiteral("Select Region for your location"));
    m_comboFRSRegion->setEnabled(false);
    m_comboFRSRegion->setToolTip(tr("Region selection is not available for transmit on this Core."));

    // Display the policy the TX gate actually reads, not the legacy Region
    // text (which remains saved but cannot influence transmit).
    auto& s = AppSettings::instance();
    const int usRegion = static_cast<int>(safety::Region::UnitedStates);
    bool regionOk = false;
    const int regionIdx = s.value(QStringLiteral("BandPlanRegion"), usRegion).toString().toInt(&regionOk);
    m_comboFRSRegion->setCurrentIndex(regionOk && regionIdx >= 0 && regionIdx < m_comboFRSRegion->count()
                                          ? regionIdx : -1);

    connect(m_comboFRSRegion, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!regionEditAvailable() || index < 0 || index >= m_comboFRSRegion->count()) {
            const QSignalBlocker blocked(m_comboFRSRegion);
            bool storedOk = false;
            const int stored = AppSettings::instance().value(QStringLiteral("BandPlanRegion"), 8).toString().toInt(&storedOk);
            m_comboFRSRegion->setCurrentIndex(storedOk && stored >= 0 && stored < m_comboFRSRegion->count() ? stored : -1);
            refreshRegionAvailability();
            return;
        }
        AppSettings::instance().setValue(QStringLiteral("BandPlanRegion"), QString::number(index));
    });

    regionRow->addWidget(regionLabel);
    regionRow->addWidget(m_comboFRSRegion);
    regionRow->addStretch();
    vbox->addLayout(regionRow);

    // --- Extended checkbox ---
    // From Thetis setup.designer.cs:8065-8074 [v2.10.3.13]
    m_chkExtended = new QCheckBox(tr("Extended"), group);
    m_chkExtended->setObjectName(QStringLiteral("chkExtended"));
    m_chkExtended->setProperty("nereusSetupId", "general.options.extended");
    m_chkExtended->setEnabled(false);
    // Addendum G-42 (JJ's ruling 2026-09-28): the box shows and changes
    // the Core's ExtendedTransmit, which the Core's transmit gate reads.
    // The old per-computer ExtendedTxAllowed is never read, so a stale
    // saved tick neither shows nor turns Extended on.
    syncExtendedFromSetting();
    connect(m_chkExtended, &QCheckBox::toggled, this, [this](bool on) {
        if (!extendedEditAvailable()) {
            syncExtendedFromSetting();
            refreshExtendedAvailability();
            return;
        }
        // From Thetis setup.cs:19251-19260 [v2.10.3.15] (ChkExtended_CheckedChanged):
        //   console.Extended = chkExtended.Checked;
        // Its //MW0LGE_21d BandStackManager.RegionReset() has no NereusSDR
        // counterpart: the band stack does not follow the region.
        AppSettings::instance().setValue(QString::fromLatin1(RadioModel::kExtendedTransmitKey),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
        if (RadioModel* radio = model()) {
            radio->reportTransmitGateSettingChanged(
                QString::fromLatin1(RadioModel::kExtendedTransmitKey));
        }
    });
    vbox->addWidget(m_chkExtended);

    // --- Warning label ---
    // From Thetis setup.designer.cs:8045-8054 [v2.10.3.13]
    m_lblWarningRegionExtended = new QLabel(
        tr("Changing this setting will reset your band stack entries"), group);
    m_lblWarningRegionExtended->setObjectName(QStringLiteral("lblWarningRegionExtended"));
    m_lblWarningRegionExtended->setStyleSheet(QStringLiteral("color: red; font-weight: bold;"));
    m_lblWarningRegionExtended->hide();
    m_lblWarningRegionExtended->setWordWrap(true);
    vbox->addWidget(m_lblWarningRegionExtended);

    // --- Receive Only checkbox ---
    // From Thetis setup.designer.cs:8535-8544 [v2.10.3.13] (text and
    // tooltip). The designer hides it (Visible=false) and Thetis shows it
    // for every model (setup.cs:19878 and on [v2.10.3.15]); NereusSDR shows
    // it on every radio (Task 16, syncReceiveOnly).
    m_chkGeneralRXOnly = new QCheckBox(tr("Receive Only"), group);
    m_chkGeneralRXOnly->setObjectName(QStringLiteral("chkGeneralRXOnly"));
    m_chkGeneralRXOnly->setProperty("nereusSetupId", "general.options.rxOnly");
    m_chkGeneralRXOnly->setToolTip(QStringLiteral("Check to disable transmit functionality."));
    m_chkGeneralRXOnly->setChecked(RadioModel::rxOnlySetting());
    // Task 16. From Thetis setup.cs:6479-6502 [v2.10.3.15]
    // (chkGeneralRXOnly_CheckedChanged): the operator unchecking it
    // (chkGeneralRXOnly.Focused) is asked first, and No puts the check back;
    // then console.RXOnly = chkGeneralRXOnly.Checked. `clicked` is the
    // operator's click only, as Focused is in Thetis; the model's own
    // updates reach the box through syncReceiveOnly with signals blocked.
    connect(m_chkGeneralRXOnly, &QCheckBox::clicked, this, [this](bool on) {
        if (m_lblRxOnlyCore) {
            m_lblRxOnlyCore->setVisible(false);
        }
        if (!on && !confirmEnableTransmit()) {
            const QSignalBlocker blocker(m_chkGeneralRXOnly);
            m_chkGeneralRXOnly->setChecked(true);
            return;
        }
        if (RadioModel* radio = model()) {
            radio->setRxOnly(on);
        } else {
            AppSettings::instance().setValue(QStringLiteral("RxOnly"),
                                              on ? QStringLiteral("True") : QStringLiteral("False"));
        }
    });
    vbox->addWidget(m_chkGeneralRXOnly);

    // Task 16 / R-R3-21: an older Core keeps Receive Only to itself and
    // refuses a window's change; the box goes back and the page says why.
    m_lblRxOnlyCore = new QLabel(
        tr("The Core needs updating before this window can change Receive Only."), group);
    m_lblRxOnlyCore->setObjectName(QStringLiteral("lblRxOnlyCore"));
    m_lblRxOnlyCore->setWordWrap(true);
    m_lblRxOnlyCore->setVisible(false);
    vbox->addWidget(m_lblRxOnlyCore);

    // --- Network Watchdog checkbox (default ON) ---
    // From Thetis setup.designer.cs:8385-8395 [v2.10.3.13] — Checked=true
    m_chkNetworkWDT = new QCheckBox(tr("Network Watchdog"), group);
    m_chkNetworkWDT->setObjectName(QStringLiteral("chkNetworkWDT"));
    m_chkNetworkWDT->setProperty("nereusSetupId", "general.options.networkWatchdog");
    // Thetis's tooltip is "Resets software/firmware if network becomes
    // inactive." (setup.designer.cs:8442 [v2.10.3.15]). NereusSDR keeps the
    // radio's own safety timer on whatever this box says (operator decision
    // 2026-09-24, R-R3-49), so the tooltip says what the box does here.
    // That is shown only for P2 radios (byte 38 is always 1) and the Hermes
    // Lite 2 (its start packet never turns the timer off), so the tooltip
    // names only those; "protocol" is not an operator word, so P2 is "the
    // newer network link".
    m_chkNetworkWDT->setToolTip(
        tr("How long NereusSDR waits for data from the radio before it treats "
           "the radio as lost. On: three seconds. Off: it keeps waiting. On a "
           "Hermes Lite 2, or a radio on the newer network link, the radio's "
           "own safety timer stays on either way."));
    // Default ON — first-launch loads "True"
    m_chkNetworkWDT->setChecked(
        s.value(QStringLiteral("NetworkWatchdogEnabled"), QStringLiteral("True")).toString() == QStringLiteral("True"));
    // R-R3-49: a radio setting, applied where the radio is. RadioModel saves
    // it (a remote window's save goes to the Core, which applies it) and
    // applies it to this window's own radio.
    // From Thetis setup.cs:18024-18028 [v2.10.3.15]:
    //   if (initializing) return;
    //   NetworkIO.SetWatchdogTimer(Convert.ToInt32(chkNetworkWDT.Checked));
    connect(m_chkNetworkWDT, &QCheckBox::toggled, this, [this](bool on) {
        if (m_lblNetworkWDTCore) {
            m_lblNetworkWDTCore->setVisible(false);
        }
        if (RadioModel* radio = model()) {
            radio->setNetworkWatchdogEnabled(on);
        } else {
            AppSettings::instance().setValue(QStringLiteral("NetworkWatchdogEnabled"),
                                              on ? QStringLiteral("True") : QStringLiteral("False"));
        }
    });
    vbox->addWidget(m_chkNetworkWDT);

    // R-R3-49 / R-R3-21: an older Core keeps this setting to itself and
    // refuses a window's change; the box goes back and the page says why.
    m_lblNetworkWDTCore = new QLabel(
        tr("The Core needs updating before this window can change the network watchdog."), group);
    m_lblNetworkWDTCore->setObjectName(QStringLiteral("lblNetworkWDTCore"));
    m_lblNetworkWDTCore->setWordWrap(true);
    m_lblNetworkWDTCore->setVisible(false);
    vbox->addWidget(m_lblNetworkWDTCore);
    if (auto* proxy = dynamic_cast<SettingsProxy*>(AppSettings::instance().remoteBackend())) {
        connect(proxy, &SettingsProxy::valueRejected, this,
                [this](const QString& key, const QVariant& restored) {
            if (key == QLatin1String("RxOnly")) {
                // Task 16: the window goes back to what the Core has.
                const bool restoredOn = restored.isValid()
                                        && restored.toString() == QLatin1String("True");
                if (RadioModel* radio = model()) {
                    radio->applyRxOnlySetting(restoredOn);
                }
                syncReceiveOnly();
                if (m_lblRxOnlyCore) {
                    m_lblRxOnlyCore->setVisible(true);
                }
                return;
            }
            if (key != QLatin1String("NetworkWatchdogEnabled") || !m_chkNetworkWDT) {
                return;
            }
            const QSignalBlocker blocker(m_chkNetworkWDT);
            m_chkNetworkWDT->setChecked(!restored.isValid()
                                        || restored.toString() == QLatin1String("True"));
            m_lblNetworkWDTCore->setVisible(true);
        });
    }

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// Options group
// From Thetis setup.designer.cs:9050-9059 [v2.10.3.13] (grpGeneralOptions)
// Controls: chkPreventTXonDifferentBandToRX
//
// Phase 3M-4 Task 11 also folds in two PureSignal Info Bar checkboxes that
// Thetis hosts on a separate "Info Bar (below spectrum)" groupBoxTS23 inside
// tpOptions2 (setup.designer.cs:10567-10632 [v2.10.3.13]).  NereusSDR's
// shallower Setup IA puts them in the existing General Options group.
// ---------------------------------------------------------------------------

void GeneralOptionsPage::buildOptionsGroup()
{
    auto* group = new QGroupBox(tr("Options"), this);
    group->setObjectName(QStringLiteral("grpGeneralOptions"));
    auto* vbox = new QVBoxLayout(group);
    vbox->setSpacing(6);

    // From Thetis setup.designer.cs:9098-9107 [v2.10.3.15]
    // (chkPreventTXonDifferentBandToRX; Thetis's tooltip is "Prevent TX'ing
    // on a different band to the RX band", see
    // refreshPreventDifferentBandAvailability for ours).
    m_chkPreventTXonDifferentBandToRX = new QCheckBox(
        tr("Prevent TX'ing on a different band to the RX band"), group);
    m_chkPreventTXonDifferentBandToRX->setObjectName(QStringLiteral("chkPreventTXonDifferentBandToRX"));
    m_chkPreventTXonDifferentBandToRX->setProperty("nereusSetupId",
                                                   "general.options.preventDifferentBand");
    m_chkPreventTXonDifferentBandToRX->setEnabled(false);
    // JJ's ruling (2026-09-29): the box shows and changes the Core's
    // PreventTxOnDifferentBandToRx, which the Core's transmit gate reads,
    // gated as Extended (transmit permission, off the air).
    syncPreventDifferentBandFromSetting();
    connect(m_chkPreventTXonDifferentBandToRX, &QCheckBox::toggled, this, [this](bool on) {
        if (!preventDifferentBandEditAvailable()) {
            syncPreventDifferentBandFromSetting();
            refreshPreventDifferentBandAvailability();
            return;
        }
        // From Thetis setup.cs:24414-24416 [v2.10.3.15]
        // (chkPreventTXonDifferentBandToRX_CheckedChanged):
        //   console.PreventTXonDifferentBandToRXband = chkPreventTXonDifferentBandToRX.Checked;
        AppSettings::instance().setValue(
            QString::fromLatin1(RadioModel::kPreventTxOnDifferentBandKey),
            on ? QStringLiteral("True") : QStringLiteral("False"));
        if (RadioModel* radio = model()) {
            radio->reportTransmitGateSettingChanged(
                QString::fromLatin1(RadioModel::kPreventTxOnDifferentBandKey));
        }
    });
    vbox->addWidget(m_chkPreventTXonDifferentBandToRX);

    // ── Phase 3M-4 Task 11: PureSignal Info Bar checkboxes ─────────────────
    //
    // From Thetis setup.designer.cs:10567-10597 [v2.10.3.13] chkHideFeebackLevel:
    //   "Hide feedback level number" (designer text), tooltip: "Hide the
    //   feedback level from the info bar".  Thetis preserves the typo
    //   "Feeback" in the objectName — NereusSDR uses corrected spelling
    //   in user-visible text, source-cite preserves the typo for traceability.
    // The "Same as right-clicking the FB indicator" tooltip cue is NereusSDR-original
    // (Thetis has no banner-click hook explanation in tooltip).
    m_chkHideFeedback = new QCheckBox(tr("Hide feedback level"), group);
    m_chkHideFeedback->setObjectName(QStringLiteral("chkHideFeedbackLevel"));
    m_chkHideFeedback->setProperty("nereusSetupId", "general.options.hideFeedback");
    m_chkHideFeedback->setToolTip(
        tr("When checked, the bottom-banner FB indicator shows \"Feedback\" "
           "text instead of the numeric level. Same as right-clicking the FB indicator."));
    m_chkHideFeedback->setChecked(
        AppSettings::instance().value(QStringLiteral("HideFeedbackLevel"),
                                       QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkHideFeedback, &QCheckBox::toggled, this, [this](bool on) {
        AppSettings::instance().setValue(QStringLiteral("HideFeedbackLevel"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
        emit hideFeedbackLevelChanged(on);
    });
    vbox->addWidget(m_chkHideFeedback);

    // From Thetis setup.designer.cs:10619-10630 [v2.10.3.13] chkSwapREDBluePSAColours:
    //   "Swap red and blue PS-A feeback" (designer text — typo preserved
    //   here as a source-cite reference; NereusSDR uses corrected spelling
    //   "feedback colours" in the user-visible text).
    m_chkSwapRedBlue = new QCheckBox(
        tr("Swap red and blue PS-A feedback colors"), group);
    m_chkSwapRedBlue->setObjectName(QStringLiteral("chkSwapREDBluePSAColours"));
    m_chkSwapRedBlue->setProperty("nereusSetupId", "general.options.swapRedBlue");
    m_chkSwapRedBlue->setToolTip(
        tr("For users with red/blue color blindness or alternate display "
           "preferences. Same as left-clicking the FB indicator."));
    m_chkSwapRedBlue->setChecked(
        AppSettings::instance().value(QStringLiteral("InvertRedBluePsa"),
                                       QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkSwapRedBlue, &QCheckBox::toggled, this, [this](bool on) {
        AppSettings::instance().setValue(QStringLiteral("InvertRedBluePsa"),
                                          on ? QStringLiteral("True") : QStringLiteral("False"));
        emit invertRedBluePsaChanged(on);
    });
    vbox->addWidget(m_chkSwapRedBlue);

    // Bidirectional sync: when PureSignal flips state from another source
    // (e.g. PsaIndicatorWidget left/right click on the bottom-banner FB
    // label per ucInfoBar.cs:1042-1054 [v2.10.3.13]), reflect the change
    // here without echoing back through the toggled lambdas above.
    if (auto* radio = this->model()) {
        if (auto* ps = radio->pureSignal()) {
            connect(ps, &PureSignal::hideFeedbackChanged, this,
                    [this](bool on) {
                        if (m_chkHideFeedback &&
                            m_chkHideFeedback->isChecked() != on) {
                            QSignalBlocker block(m_chkHideFeedback);
                            m_chkHideFeedback->setChecked(on);
                        }
                    });
            connect(ps, &PureSignal::invertRedBlueChanged, this,
                    [this](bool on) {
                        if (m_chkSwapRedBlue &&
                            m_chkSwapRedBlue->isChecked() != on) {
                            QSignalBlocker block(m_chkSwapRedBlue);
                            m_chkSwapRedBlue->setChecked(on);
                        }
                    });
        }
    }

    // --- CPU meter rate ---
    // Controls the update rate of the CPU usage indicator in the chrome
    // title bar. Persists as GeneralCpuMeterUpdateRateHz (int, default 1).
    // Range: 1-30 Hz.  Thetis equivalent: toolStripStatusLabel_CPU timer,
    // which fires every 1 s by default (console.cs [v2.10.3.13]).
    {
        auto* rateRow = new QHBoxLayout;
        auto* rateLabel = new QLabel(tr("CPU meter rate:"), group);
        m_cpuMeterRateHz = new QSpinBox(group);
        m_cpuMeterRateHz->setProperty("nereusSetupId", "general.options.cpuMeterRate");
        m_cpuMeterRateHz->setRange(1, 30);
        m_cpuMeterRateHz->setSuffix(QStringLiteral(" Hz"));
        m_cpuMeterRateHz->setFixedWidth(80);
        m_cpuMeterRateHz->setToolTip(
            tr("Update rate for the CPU usage indicator in the title bar (1-30 Hz)."));

        // Restore persisted value; default 1 Hz (matches Thetis 1 s timer).
        m_cpuMeterRateHz->setValue(
            AppSettings::instance().value(
                QStringLiteral("GeneralCpuMeterUpdateRateHz"), 1).toInt());

        connect(m_cpuMeterRateHz, QOverload<int>::of(&QSpinBox::valueChanged),
                this, [this](int v) {
            AppSettings::instance().setValue(
                QStringLiteral("GeneralCpuMeterUpdateRateHz"), v);
            emit cpuMeterRateChanged(v);
        });

        rateRow->addWidget(rateLabel);
        rateRow->addWidget(m_cpuMeterRateHz);
        rateRow->addStretch();
        vbox->addLayout(rateRow);
    }

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// Time Out Timers group (iPhone app plan Task 38, R-IOS-04, D29)
// From Thetis setup.designer.cs:10154-10299 [v2.10.3.15] (groupBoxTS32 on
// tpOptions2, "Time Out Timers"): chkToTMox + udMoxToTSeconds +
// lblMoxTotSec, chkToTPing + udPingToTSeconds + lblPingTotSec, txtToTPingIP
// + btnPingDef. Handlers from setup.cs:28539-28583 [v2.10.3.15].
// NereusSDR adds the time-out for phones and tablets (D29): on by default
// at 180 s, the same range. Every value here is the Core's (Station scope);
// RadioModel reads them at each tick of the time-out, so a change applies
// at once, from key-down.
// ---------------------------------------------------------------------------

void GeneralOptionsPage::buildTimeOutGroup()
{
    // From Thetis setup.designer.cs:10169 [v2.10.3.15]:
    //   this.groupBoxTS32.Text = "Time Out Timers";
    auto* group = new QGroupBox(tr("Time Out Timers"), this);
    group->setObjectName(QStringLiteral("grpTimeOutTimers"));
    m_grpTimeOut = group;
    auto* grid = new QGridLayout(group);
    grid->setHorizontalSpacing(6);
    grid->setVerticalSpacing(6);

    const AppSettings& s = AppSettings::instance();
    const auto readBool = [&s](const QString& key, bool fallback) {
        return s.value(key, fallback ? QStringLiteral("True") : QStringLiteral("False")).toString()
            == QStringLiteral("True");
    };
    const auto writeBool = [](const QString& key, bool on) {
        AppSettings::instance().setValue(key, on ? QStringLiteral("True") : QStringLiteral("False"));
    };
    // One row: a checkbox, its seconds and "secs". Thetis's udMoxToTSeconds
    // and udPingToTSeconds: 30 to 1800, default 180 (setup.designer.cs:
    // 10279-10298 and 10226-10245 [v2.10.3.15]).
    const auto makeSeconds = [group, &s](const QString& key) {
        auto* spin = new QSpinBox(group);
        spin->setRange(TxTimeOutTimer::kMinimumSeconds, TxTimeOutTimer::kMaximumSeconds);
        spin->setFixedWidth(80);
        bool ok = false;
        const int seconds = s.value(key, TxTimeOutTimer::kDefaultSeconds).toInt(&ok);
        spin->setValue(ok ? seconds : TxTimeOutTimer::kDefaultSeconds);
        return spin;
    };

    // --- MOX ---
    // From Thetis setup.designer.cs:10256-10257 [v2.10.3.15]:
    //   this.chkToTMox.Text = "MOX";
    //   this.toolTip1.SetToolTip(this.chkToTMox, "Time out Mox after X seconds");
    m_chkToTMox = new QCheckBox(tr("MOX"), group);
    m_chkToTMox->setObjectName(QStringLiteral("chkToTMox"));
    m_chkToTMox->setProperty("nereusSetupId", "general.options.moxTimeoutEnabled");
    m_chkToTMox->setToolTip(tr("Time out Mox after X seconds"));
    m_chkToTMox->setChecked(readBool(QStringLiteral("MoxTimeOutEnabled"), false));
    m_udMoxToTSeconds = makeSeconds(QStringLiteral("MoxTimeOutSeconds"));
    m_udMoxToTSeconds->setObjectName(QStringLiteral("udMoxToTSeconds"));
    m_udMoxToTSeconds->setProperty("nereusSetupId", "general.options.moxTimeoutSeconds");
    // From Thetis setup.designer.cs:10293 [v2.10.3.15]
    m_udMoxToTSeconds->setToolTip(tr("Stop mox if it is enabled for this duration"));
    m_lblMoxTotSec = new QLabel(tr("secs"), group);
    m_lblMoxTotSec->setObjectName(QStringLiteral("lblMoxTotSec"));
    grid->addWidget(m_chkToTMox, 0, 0);
    grid->addWidget(m_udMoxToTSeconds, 0, 1);
    grid->addWidget(m_lblMoxTotSec, 0, 2);

    // --- Ping ---
    // From Thetis setup.designer.cs:10202-10204 [v2.10.3.15]:
    //   this.chkToTPing.Text = "Ping";
    //   SetToolTip(this.chkToTPing, "If ping fails for X seconds, then stop mox.\r\nNote: use cmd line to check you can ping this IP.");
    m_chkToTPing = new QCheckBox(tr("Ping"), group);
    m_chkToTPing->setObjectName(QStringLiteral("chkToTPing"));
    m_chkToTPing->setProperty("nereusSetupId", "general.options.pingTimeoutEnabled");
    m_chkToTPing->setToolTip(tr("If ping fails for X seconds, then stop mox.\n"
                                "Note: use cmd line to check you can ping this IP."));
    m_chkToTPing->setChecked(readBool(QStringLiteral("PingTimeOutEnabled"), false));
    m_udPingToTSeconds = makeSeconds(QStringLiteral("PingTimeOutSeconds"));
    m_udPingToTSeconds->setObjectName(QStringLiteral("udPingToTSeconds"));
    m_udPingToTSeconds->setProperty("nereusSetupId", "general.options.pingTimeoutSeconds");
    // From Thetis setup.designer.cs:10240 [v2.10.3.15]
    m_udPingToTSeconds->setToolTip(tr("If unable to ping for this long, stop mox"));
    m_lblPingTotSec = new QLabel(tr("secs"), group);
    m_lblPingTotSec->setObjectName(QStringLiteral("lblPingTotSec"));
    grid->addWidget(m_chkToTPing, 1, 0);
    grid->addWidget(m_udPingToTSeconds, 1, 1);
    grid->addWidget(m_lblPingTotSec, 1, 2);

    // From Thetis setup.designer.cs:10190-10191 [v2.10.3.15]:
    //   this.txtToTPingIP.Text = "8.8.8.8";
    //   SetToolTip(this.txtToTPingIP, "Try to ping this IP");
    m_txtToTPingIP = new QLineEdit(group);
    m_txtToTPingIP->setObjectName(QStringLiteral("txtToTPingIP"));
    m_txtToTPingIP->setProperty("nereusSetupId", "general.options.pingTimeoutHost");
    m_txtToTPingIP->setToolTip(tr("Try to ping this IP"));
    m_txtToTPingIP->setFixedWidth(120);
    m_txtToTPingIP->setText(
        s.value(QStringLiteral("PingTimeOutHost"), QStringLiteral("8.8.8.8")).toString());
    // From Thetis setup.designer.cs:10179-10180 [v2.10.3.15]:
    //   this.btnPingDef.Text = "Def";
    //   SetToolTip(this.btnPingDef, "Default value of 8.8.8.8 (Google DNS)");
    m_btnPingDef = new QPushButton(tr("Def"), group);
    m_btnPingDef->setObjectName(QStringLiteral("btnPingDef"));
    m_btnPingDef->setToolTip(tr("Default value of 8.8.8.8 (Google DNS)"));
    auto* hostRow = new QHBoxLayout;
    hostRow->addWidget(m_txtToTPingIP);
    hostRow->addWidget(m_btnPingDef);
    hostRow->addStretch();
    grid->addLayout(hostRow, 2, 0, 1, 3);

    // --- Phone and iPad (NereusSDR, D29) ---
    m_chkRemoteMoxTimeOut = new QCheckBox(tr("Phone and iPad"), group);
    m_chkRemoteMoxTimeOut->setObjectName(QStringLiteral("chkRemoteMoxTimeOut"));
    m_chkRemoteMoxTimeOut->setProperty("nereusSetupId", "general.options.remoteTimeoutEnabled");
    m_chkRemoteMoxTimeOut->setToolTip(
        tr("Stop a transmission from a phone or iPad after this many seconds. "
           "MOX and Ping above apply to this radio's own keys and to computers."));
    m_chkRemoteMoxTimeOut->setChecked(
        readBool(QStringLiteral("RemoteMoxTimeOutEnabled"), RadioModel::kRemoteMoxTimeOutDefault));
    m_udRemoteMoxTimeOutSeconds = makeSeconds(QStringLiteral("RemoteMoxTimeOutSeconds"));
    m_udRemoteMoxTimeOutSeconds->setObjectName(QStringLiteral("udRemoteMoxTimeOutSeconds"));
    m_udRemoteMoxTimeOutSeconds->setProperty("nereusSetupId", "general.options.remoteTimeoutSeconds");
    m_udRemoteMoxTimeOutSeconds->setToolTip(
        tr("Stop a transmission from a phone or iPad if it lasts this long"));
    m_lblRemoteMoxTotSec = new QLabel(tr("secs"), group);
    m_lblRemoteMoxTotSec->setObjectName(QStringLiteral("lblRemoteMoxTotSec"));
    grid->addWidget(m_chkRemoteMoxTimeOut, 3, 0);
    grid->addWidget(m_udRemoteMoxTimeOutSeconds, 3, 1);
    grid->addWidget(m_lblRemoteMoxTotSec, 3, 2);
    grid->setColumnStretch(3, 1);

    // From Thetis setup.cs:28539-28550 [v2.10.3.15]:
    //   private void chkToTMox_CheckedChanged(object sender, EventArgs e)
    //   {
    //       if (initializing) return;
    //       udMoxToTSeconds.Enabled = chkToTMox.Checked;
    //       lblMoxTotSec.Enabled = chkToTMox.Checked;
    //       TimeOutTimerManager.MoxTimeOut((int)udMoxToTSeconds.Value, chkToTMox.Checked);
    //   }
    //   private void udMoxToTSeconds_ValueChanged(object sender, EventArgs e)
    //   {
    //       chkToTMox_CheckedChanged(this, EventArgs.Empty);
    //   }
    // The values are saved; the time-out reads them at its next tick.
    const auto applyMox = [this, writeBool]() {
        m_udMoxToTSeconds->setEnabled(m_chkToTMox->isChecked());
        m_lblMoxTotSec->setEnabled(m_chkToTMox->isChecked());
        writeBool(QStringLiteral("MoxTimeOutEnabled"), m_chkToTMox->isChecked());
        AppSettings::instance().setValue(QStringLiteral("MoxTimeOutSeconds"),
                                         m_udMoxToTSeconds->value());
    };
    // From Thetis setup.cs:28552-28583 [v2.10.3.15]:
    //   private void chkToTPing_CheckedChanged(object sender, EventArgs e)
    //   {
    //       if (initializing) return;
    //       udPingToTSeconds.Enabled = chkToTPing.Checked;
    //       txtToTPingIP.Enabled = chkToTPing.Checked;
    //       btnPingDef.Enabled = chkToTPing.Checked;
    //       lblPingTotSec.Enabled = chkToTPing.Checked;
    //
    //       bool bIPOk = IPAddress.TryParse(txtToTPingIP.Text, out IPAddress address);
    //       if (bIPOk)
    //       {
    //           txtToTPingIP.BackColor = SystemColors.Window;
    //           TimeOutTimerManager.PingTimeOut(txtToTPingIP.Text, (int)udPingToTSeconds.Value, chkToTPing.Checked);
    //       }
    //       else
    //           txtToTPingIP.BackColor = Color.Red;
    //   }
    // udPingToTSeconds_ValueChanged and txtToTPingIP_TextChanged call it;
    // btnPingDef_Click sets txtToTPingIP.Text = "8.8.8.8". As in Thetis,
    // nothing is saved while the host does not parse.
    const auto applyPing = [this, writeBool]() {
        const bool on = m_chkToTPing->isChecked();
        m_udPingToTSeconds->setEnabled(on);
        m_txtToTPingIP->setEnabled(on);
        m_btnPingDef->setEnabled(on);
        m_lblPingTotSec->setEnabled(on);

        const QString host = m_txtToTPingIP->text().trimmed();
        const bool hostOk = !QHostAddress(host).isNull();
        m_txtToTPingIP->setStyleSheet(hostOk ? QString()
                                             : QStringLiteral("QLineEdit { background: red; }"));
        if (hostOk) {
            AppSettings::instance().setValue(QStringLiteral("PingTimeOutHost"), host);
            AppSettings::instance().setValue(QStringLiteral("PingTimeOutSeconds"),
                                             m_udPingToTSeconds->value());
            writeBool(QStringLiteral("PingTimeOutEnabled"), on);
        }
    };
    const auto applyRemote = [this, writeBool]() {
        const bool on = m_chkRemoteMoxTimeOut->isChecked();
        m_udRemoteMoxTimeOutSeconds->setEnabled(on);
        m_lblRemoteMoxTotSec->setEnabled(on);
        writeBool(QStringLiteral("RemoteMoxTimeOutEnabled"), on);
        AppSettings::instance().setValue(QStringLiteral("RemoteMoxTimeOutSeconds"),
                                         m_udRemoteMoxTimeOutSeconds->value());
    };

    // Thetis's "if (initializing) return;": the loaded values only set the
    // enabled state; nothing is written until the operator changes one.
    const auto showEnabled = [this]() {
        m_udMoxToTSeconds->setEnabled(m_chkToTMox->isChecked());
        m_lblMoxTotSec->setEnabled(m_chkToTMox->isChecked());
        const bool ping = m_chkToTPing->isChecked();
        m_udPingToTSeconds->setEnabled(ping);
        m_txtToTPingIP->setEnabled(ping);
        m_btnPingDef->setEnabled(ping);
        m_lblPingTotSec->setEnabled(ping);
        m_udRemoteMoxTimeOutSeconds->setEnabled(m_chkRemoteMoxTimeOut->isChecked());
        m_lblRemoteMoxTotSec->setEnabled(m_chkRemoteMoxTimeOut->isChecked());
    };
    showEnabled();

    connect(m_chkToTMox, &QCheckBox::toggled, this, applyMox);
    connect(m_udMoxToTSeconds, QOverload<int>::of(&QSpinBox::valueChanged), this, applyMox);
    connect(m_chkToTPing, &QCheckBox::toggled, this, applyPing);
    connect(m_udPingToTSeconds, QOverload<int>::of(&QSpinBox::valueChanged), this, applyPing);
    connect(m_txtToTPingIP, &QLineEdit::textChanged, this, applyPing);
    connect(m_btnPingDef, &QPushButton::clicked, this, [this]() {
        // From Thetis setup.cs:28580-28583 [v2.10.3.15]:
        //   txtToTPingIP.Text = "8.8.8.8";
        m_txtToTPingIP->setText(QStringLiteral("8.8.8.8"));
    });
    connect(m_chkRemoteMoxTimeOut, &QCheckBox::toggled, this, applyRemote);
    connect(m_udRemoteMoxTimeOutSeconds, QOverload<int>::of(&QSpinBox::valueChanged), this,
            applyRemote);

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// Step Attenuator group
// From Thetis setup.cs: grpHermesStepAttenuator
// ---------------------------------------------------------------------------

void GeneralOptionsPage::buildStepAttGroup()
{
    auto* group = new QGroupBox(QStringLiteral("Step Attenuator"), this);
    group->setObjectName(QStringLiteral("grpStepAttenuator"));
    auto* vbox  = new QVBoxLayout(group);
    vbox->setSpacing(6);

    // --- RX1 row ---
    auto* rx1Row = new QHBoxLayout;
    m_chkRx1StepAttEnable = new QCheckBox(QStringLiteral("RX1 Enable"), group);
    m_chkRx1StepAttEnable->setProperty("nereusSetupId", "general.options.rx1StepAttEnable");
    // From Thetis setup.cs: chkHermesStepAttenuator
    m_chkRx1StepAttEnable->setToolTip(QStringLiteral("Enable the step attenuator."));
    m_spnRx1StepAttValue = makeDbSpinBox(group);
    m_spnRx1StepAttValue->setProperty("nereusSetupId", "general.options.rx1StepAtt");
    m_spnRx1StepAttValue->setEnabled(false);
    rx1Row->addWidget(m_chkRx1StepAttEnable);
    rx1Row->addWidget(m_spnRx1StepAttValue);
    rx1Row->addStretch();
    vbox->addLayout(rx1Row);

    // --- RX2 row ---
    // R-R3-46 / R-R3-11: the value is the other receive ADC's own
    // attenuator (StepAttenuatorController::rx2AttenuatorDb, Thetis
    // RX2AttenuatorData / udRX2StepAttData) and RX2 Enable its own enable
    // (Thetis chkRX2StepAtt -> console.RX2StepAttEnabled), both usable on a
    // radio with a second receive ADC (refreshRx2StepAtt). The row was built
    // and hidden by issue #259 until the controller held RX2's state.
    auto* rx2Row = new QHBoxLayout;
    m_chkRx2StepAttEnable = new QCheckBox(QStringLiteral("RX2 Enable"), group);
    m_chkRx2StepAttEnable->setObjectName(QStringLiteral("chkRx2StepAttEnable"));
    m_chkRx2StepAttEnable->setProperty("nereusSetupId", "general.options.rx2StepAttEnable");
    // From Thetis setup.cs: chkRX2StepAtt
    m_chkRx2StepAttEnable->setToolTip(QStringLiteral("Enable the step attenuator."));
    m_chkRx2StepAttEnable->setEnabled(false);
    m_spnRx2StepAttValue = makeDbSpinBox(group);
    m_spnRx2StepAttValue->setObjectName(QStringLiteral("spnRx2StepAttValue"));
    m_spnRx2StepAttValue->setProperty("nereusSetupId", "general.options.rx2StepAtt");
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
    // From Thetis setup.cs:15730-15762 [v2.10.3.13] chkHermesStepAttenuator_
    // CheckedChanged. The RX1↔RX2 click-time mirror lives in that handler
    // (lines 15750-15760), gated on chk != null (sender is a CheckBoxTS, i.e.
    // a real user click) AND HasSteppedAttenuation(2) AND shared-ADC. Mirror
    // wiring is deferred until the controller carries independent RX2 state.
    //MW0LGE [2.9.0.6]  [original inline comment from setup.cs:15742 — "only if we click it"]
    connect(m_chkRx1StepAttEnable, &QCheckBox::toggled, this, [this](bool on) {
        m_spnRx1StepAttValue->setEnabled(on);
        if (m_ctrl) {
            m_ctrl->setStepAttEnabled(on);
        } else if (m_stepAtt) {
            m_stepAtt->setEnabled(on);
        }
        refreshRx2StepAtt();
    });

    // --- Spinbox → controller ---
    // From Thetis setup.cs:15765-15772 [v2.10.3.13] udHermesStepAttenuator
    // Data_ValueChanged → console.RX1AttenuatorData. (The model-gated
    // Maximum=61 branch at setup.cs:15773-15786 lives in BoardCapsTable
    // ::stepAttMaxDb in NereusSDR.)
    connect(m_spnRx1StepAttValue, &QSpinBox::valueChanged, this, [this](int dB) {
        if (m_ctrl) {
            m_ctrl->setAttenuation(dB, 0);
        } else if (m_stepAtt) {
            m_stepAtt->setAttenuationDb(dB);
        }
    });

    // R-R3-46 / R-R3-11: RX2's own enable (Thetis chkRX2StepAtt_Checked
    // Changed -> console.RX2StepAttEnabled; on one ADC the controller keeps
    // the two enables one, as Thetis's Setup mirrors them).
    connect(m_chkRx2StepAttEnable, &QCheckBox::toggled, this, [this](bool on) {
        if (m_ctrl) {
            m_ctrl->setRx2StepAttEnabled(on);
        } else if (m_stepAtt) {
            m_stepAtt->setRx2StepAttEnabled(on);
        }
        refreshRx2StepAtt();
    });

    // R-R3-46 / R-R3-11: RX2's value is the other ADC's own attenuator
    // (Thetis udRX2StepAttData -> console.RX2AttenuatorData).
    connect(m_spnRx2StepAttValue, &QSpinBox::valueChanged, this, [this](int dB) {
        if (m_ctrl) {
            m_ctrl->setRx2Attenuation(dB);
        } else if (m_stepAtt) {
            m_stepAtt->setRx2AttenuationDb(dB);
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
        group->setObjectName(QStringLiteral("grpAutoAttRx%1").arg(rx + 1));
        auto* vbox  = new QVBoxLayout(group);
        vbox->setSpacing(6);

        // Enable checkbox
        chkEnable = new QCheckBox(QStringLiteral("Enable"), group);
        if (rx == 0) chkEnable->setProperty("nereusSetupId", "general.options.autoAttEnable");
        if (rx == 1) chkEnable->setProperty("nereusSetupId", "general.options.rx2AutoAttEnable");
        // From Thetis setup.cs: chkAutoATTRx1 / chkAutoATTRx2
        chkEnable->setToolTip(
            QStringLiteral("Auto attenuate RX%1 on ADC overload").arg(rx + 1));
        vbox->addWidget(chkEnable);

        // Mode combo row. R-R3-46 / R-R3-11: RX1 only. Adaptive is a
        // NereusSDR extension of RX1's; Thetis's RX2 group has Enable, Undo
        // and Hold (chkAutoATTRx2, chkAutoAttUndoRX2, nudAutoAttHoldRX2).
        cmbMode = nullptr;
        if (rx == 0) {
            auto* modeRow = new QHBoxLayout;
            auto* modeLabel = new QLabel(QStringLiteral("Mode:"), group);
            cmbMode = makeModeCombo(group);
            cmbMode->setProperty("nereusSetupId", "general.options.autoAttMode");
            cmbMode->setEnabled(false);
            modeRow->addWidget(modeLabel);
            modeRow->addWidget(cmbMode);
            modeRow->addStretch();
            vbox->addLayout(modeRow);
        }

        // Undo/Decay checkbox
        chkUndo = new QCheckBox(QStringLiteral("Undo"), group);
        if (rx == 0) chkUndo->setProperty("nereusSetupId", "general.options.autoAttUndo");
        if (rx == 1) chkUndo->setProperty("nereusSetupId", "general.options.rx2AutoAttUndo");
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
            if (cmbMode) {
                cmbMode->setEnabled(on);
            }
            chkUndo->setEnabled(on);
            spnHold->setEnabled(on && chkUndo->isChecked());
        });

        // hold only enabled when undo is checked (and auto-att enabled)
        connect(chkUndo, &QCheckBox::toggled, this, [chkEnable, spnHold](bool on) {
            spnHold->setEnabled(on && chkEnable->isChecked());
        });

        // Mode change → relabel undo checkbox
        if (cmbMode) {
            connect(cmbMode, &QComboBox::currentIndexChanged, this, [chkUndo](int idx) {
                if (idx == static_cast<int>(AutoAttMode::Adaptive)) {
                    chkUndo->setText(QStringLiteral("Decay"));
                } else {
                    chkUndo->setText(QStringLiteral("Undo"));
                }
            });
        }

        // R-R3-46 / R-R3-11: RX2's own auto-attenuate settings, locally
        // and on the Core (Thetis chkAutoATTRx2 -> console.AutoAttRX2,
        // chkAutoAttUndoRX2 -> AutoAttUndoRX2, nudAutoAttHoldRX2 ->
        // AutoAttUndoDelayRX2, in whole seconds).
        if (rx == 1) {
            connect(chkEnable, &QCheckBox::toggled, this, [this](bool on) {
                if (m_ctrl) {
                    m_ctrl->setRx2AutoAttEnabled(on);
                } else if (m_stepAtt) {
                    m_stepAtt->setRx2AutoAttEnabled(on);
                }
            });
            connect(chkUndo, &QCheckBox::toggled, this, [this](bool on) {
                if (m_ctrl) {
                    m_ctrl->setRx2AutoAttUndo(on);
                } else if (m_stepAtt) {
                    m_stepAtt->setRx2AutoAttUndo(on);
                }
            });
            connect(spnHold, &QSpinBox::valueChanged, this, [this](int sec) {
                if (m_ctrl) {
                    m_ctrl->setRx2AutoUndoDelaySec(sec);
                } else if (m_stepAtt) {
                    m_stepAtt->setRx2AutoAttUndoDelayMs(sec * 1000);
                }
            });
        }

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

        // R-R3-46: a remote window writes the Core's object (RX1 only, as
        // the controller behind it is). The times are whole seconds, carried
        // in ms: Classic's undo delay, Adaptive's hold.
        if (m_stepAtt && rx == 0) {
            connect(chkEnable, &QCheckBox::toggled, this, [this](bool on) {
                m_stepAtt->setAutoAttEnabled(on);
            });
            connect(cmbMode, &QComboBox::currentIndexChanged, this, [this](int idx) {
                m_stepAtt->setAutoAttMode(idx);
            });
            connect(chkUndo, &QCheckBox::toggled, this, [this](bool on) {
                m_stepAtt->setAutoAttUndo(on);
            });
            connect(spnHold, &QSpinBox::valueChanged, this, [this, cmbMode](int sec) {
                if (cmbMode->currentIndex() == static_cast<int>(AutoAttMode::Adaptive)) {
                    m_stepAtt->setAutoAttHoldMs(sec * 1000);
                } else {
                    m_stepAtt->setAutoAttUndoDelayMs(sec * 1000);
                }
            });
        }

        contentLayout()->addWidget(group);

        // R-R3-46 / R-R3-11: the RX2 group is usable on a radio with a
        // second receive ADC (refreshRx2StepAtt sets it).
        if (rx == 1) {
            group->setEnabled(false);
        }
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

    // Attenuation changed → update RX1 spinbox. Controller is single-RX
    // (m_attDb backs RX1 only); the RX2 row is hidden until the controller
    // gains independent RX2 state.
    connect(m_ctrl, &StepAttenuatorController::attenuationChanged,
            this, [this](int dB) {
        QSignalBlocker blk(m_spnRx1StepAttValue);
        m_spnRx1StepAttValue->setValue(dB);
    });

    // Enable state changed → update RX1 checkbox + cascade RX1 spinbox
    // enabled state. Issue #259: this is what closes the loop for the
    // post-reload restore on the next live signal (e.g. an external
    // setStepAttEnabled), but the cold-open case is handled in the
    // constructor via initFromController() — see GeneralOptionsPage ctor
    // for the full rationale.
    connect(m_ctrl, &StepAttenuatorController::stepAttEnabledChanged,
            this, [this](bool on) {
        {
            QSignalBlocker blk(m_chkRx1StepAttEnable);
            m_chkRx1StepAttEnable->setChecked(on);
        }
        m_spnRx1StepAttValue->setEnabled(on);
        refreshRx2StepAtt();
    });

    // R-R3-46 / R-R3-11: the other ADC's own attenuator, enable and
    // auto-attenuate settings.
    connect(m_ctrl, &StepAttenuatorController::rx2AttenuationChanged,
            this, [this](int) { refreshRx2StepAtt(); });
    for (auto signal : {&StepAttenuatorController::rx2StepAttEnabledChanged,
                        &StepAttenuatorController::rx2AutoAttEnabledChanged,
                        &StepAttenuatorController::rx2AutoAttUndoChanged}) {
        connect(m_ctrl, signal, this, [this](bool) { refreshRx2StepAtt(); });
    }
    connect(m_ctrl, &StepAttenuatorController::rx2AutoUndoDelayChanged,
            this, [this](int) { refreshRx2StepAtt(); });
}

// ---------------------------------------------------------------------------
// initFromController — pull the controller's current state into the widgets
// at page construction time.
//
// Issue #259 — the SetupDialog (and every page in it) is constructed lazily
// on every Tools → Setup open, through MainWindow::createSetupDialog(), which
// remote-daemon R2 Task 20 made the single construction site for the twelve
// entry points that used to run `new SetupDialog` inline. So:
//
//   1. App launches, no SetupDialog yet.
//   2. RadioModel connects, StepAttenuatorController::loadSettings runs,
//      m_stepAttEnabled / m_attDb are restored, and the controller emits
//      stepAttEnabledChanged + attenuationChanged. The page does not exist
//      yet — nobody is listening.
//   3. User opens Setup. GeneralOptionsPage constructor runs, connect-
//      Controller() wires future signals, but nothing fires retroactively.
//      Widget state is the QCheckBox / QSpinBox default (unchecked / 0).
//
// initFromController() pulls the current controller state into the widgets
// once, at construction time, with signals blocked so the read does not
// loop back into the controller. From this point on, future user edits
// (toggled / valueChanged) and future controller signals (stepAttEnabled-
// Changed / attenuationChanged) keep the two sides in sync.
// ---------------------------------------------------------------------------
void GeneralOptionsPage::initFromController()
{
    if (!m_ctrl) {
        return;
    }

    const bool stepOn = m_ctrl->stepAttEnabled();
    const int  attDb  = m_ctrl->attenuatorDb();

    {
        QSignalBlocker blk(m_chkRx1StepAttEnable);
        m_chkRx1StepAttEnable->setChecked(stepOn);
    }
    {
        QSignalBlocker blk(m_spnRx1StepAttValue);
        m_spnRx1StepAttValue->setValue(attDb);
    }
    m_spnRx1StepAttValue->setEnabled(stepOn);
    refreshRx2StepAtt();

    // Auto-att group — same lazy-construct problem. Issue #259 PR #260
    // review fix: previously this only pulled enable + mode, leaving
    // chkAutoAttUndoRx1 + spnAutoAttHoldRx1 at their constructor defaults
    // even when the controller had restored real values from disk. Pull
    // all four fields and apply the mode-aware Undo↔Decay relabel + the
    // mode-aware hold-vs-delay spinbox value (the same handler the cmbMode
    // currentIndexChanged slot wires up in buildAutoAttGroup at line ~596).
    {
        QSignalBlocker blk(m_chkAutoAttRx1);
        m_chkAutoAttRx1->setChecked(m_ctrl->autoAttEnabled());
    }
    {
        QSignalBlocker blk(m_cmbAutoAttRx1Mode);
        m_cmbAutoAttRx1Mode->setCurrentIndex(static_cast<int>(m_ctrl->autoAttMode()));
    }
    const bool isAdaptive =
        (m_ctrl->autoAttMode() == AutoAttMode::Adaptive);
    {
        QSignalBlocker blk(m_chkAutoAttUndoRx1);
        m_chkAutoAttUndoRx1->setChecked(m_ctrl->autoAttUndo());
        // Match the cmbMode::currentIndexChanged handler in buildAutoAttGroup
        // (line ~596) — Adaptive uses "Decay", Classic uses "Undo".
        m_chkAutoAttUndoRx1->setText(isAdaptive
            ? QStringLiteral("Decay") : QStringLiteral("Undo"));
    }
    {
        QSignalBlocker blk(m_spnAutoAttHoldRx1);
        // In Adaptive mode the spinbox holds the seconds-of-hold; in
        // Classic mode it holds the undo-delay seconds. The
        // spnHold::valueChanged binding (buildAutoAttGroup line ~615)
        // dispatches setAutoAttHoldSeconds vs setAutoUndoDelaySec on
        // the same widget; mirror that here on the read side.
        m_spnAutoAttHoldRx1->setValue(isAdaptive
            ? m_ctrl->adaptiveHoldSeconds()
            : m_ctrl->autoUndoDelaySec());
    }
    const bool autoOn = m_ctrl->autoAttEnabled();
    m_cmbAutoAttRx1Mode->setEnabled(autoOn);
    m_chkAutoAttUndoRx1->setEnabled(autoOn);
    m_spnAutoAttHoldRx1->setEnabled(autoOn && m_chkAutoAttUndoRx1->isChecked());
}

// ---------------------------------------------------------------------------
// syncFromModel — restore UI state from controller on page show
// ---------------------------------------------------------------------------

void GeneralOptionsPage::syncFromModel()
{
    if (m_stepAtt) {
        syncFromFacade();
        return;
    }
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

void GeneralOptionsPage::reloadFeedbackPreferences()
{
    const auto& settings = AppSettings::instance();
    if (m_chkHideFeedback) {
        QSignalBlocker block(m_chkHideFeedback);
        m_chkHideFeedback->setChecked(
            settings.value(QStringLiteral("HideFeedbackLevel"), QStringLiteral("False"))
                .toString() == QStringLiteral("True"));
    }
    if (m_chkSwapRedBlue) {
        QSignalBlocker block(m_chkSwapRedBlue);
        m_chkSwapRedBlue->setChecked(
            settings.value(QStringLiteral("InvertRedBluePsa"), QStringLiteral("False"))
                .toString() == QStringLiteral("True"));
    }
}

// ---------------------------------------------------------------------------
// R-R3-46 / R-R3-21: a remote window's groups and the Core's `stepAtt` object
// ---------------------------------------------------------------------------

void GeneralOptionsPage::connectFacade()
{
    Q_ASSERT(m_stepAtt);
    using F = StepAttenuatorFacade;
    for (auto signal : {&F::enabledChanged, &F::autoAttEnabledChanged,
                        &F::autoAttUndoChanged, &F::adcLinkedChanged,
                        &F::rx2StepAttEnabledChanged, &F::rx2AutoAttEnabledChanged,
                        &F::rx2AutoAttUndoChanged}) {
        connect(m_stepAtt, signal, this, [this](bool) { syncFromFacade(); });
    }
    for (auto signal : {&F::attenuationDbChanged, &F::autoAttModeChanged,
                        &F::autoAttUndoDelayMsChanged, &F::autoAttHoldMsChanged,
                        &F::minDbChanged, &F::maxDbChanged, &F::rx2AttenuationDbChanged,
                        &F::rx2AutoAttUndoDelayMsChanged}) {
        connect(m_stepAtt, signal, this, [this](int) { syncFromFacade(); });
    }
    connect(m_stepAtt, &F::windowAvailabilityChanged,
            this, [this](bool) { applyRadioHardwareAvailability(); });
}

// The same widget state initFromController() gives a local window, read from
// the Core's values, with signals blocked so the read writes nothing back.
void GeneralOptionsPage::syncFromFacade()
{
    if (!m_stepAtt) {
        return;
    }
    const bool stepOn = m_stepAtt->enabled();
    {
        QSignalBlocker blk(m_spnRx1StepAttValue);
        m_spnRx1StepAttValue->setRange(m_stepAtt->minDb(), m_stepAtt->maxDb());
        m_spnRx1StepAttValue->setValue(m_stepAtt->attenuationDb());
    }
    {
        QSignalBlocker blk(m_spnRx2StepAttValue);
        m_spnRx2StepAttValue->setRange(m_stepAtt->minDb(), rx2StepAttMaxDb());
    }
    refreshRx2StepAtt();
    {
        QSignalBlocker blk(m_chkRx1StepAttEnable);
        m_chkRx1StepAttEnable->setChecked(stepOn);
    }
    m_spnRx1StepAttValue->setEnabled(stepOn);
    m_lblAdcLinked->setVisible(m_stepAtt->adcLinked());

    const bool isAdaptive = m_stepAtt->autoAttMode() == static_cast<int>(AutoAttMode::Adaptive);
    {
        QSignalBlocker blk(m_chkAutoAttRx1);
        m_chkAutoAttRx1->setChecked(m_stepAtt->autoAttEnabled());
    }
    {
        QSignalBlocker blk(m_cmbAutoAttRx1Mode);
        m_cmbAutoAttRx1Mode->setCurrentIndex(isAdaptive ? static_cast<int>(AutoAttMode::Adaptive)
                                                        : static_cast<int>(AutoAttMode::Classic));
    }
    {
        QSignalBlocker blk(m_chkAutoAttUndoRx1);
        m_chkAutoAttUndoRx1->setChecked(m_stepAtt->autoAttUndo());
        m_chkAutoAttUndoRx1->setText(isAdaptive ? QStringLiteral("Decay")
                                                : QStringLiteral("Undo"));
    }
    {
        QSignalBlocker blk(m_spnAutoAttHoldRx1);
        m_spnAutoAttHoldRx1->setValue(
            (isAdaptive ? m_stepAtt->autoAttHoldMs() : m_stepAtt->autoAttUndoDelayMs()) / 1000);
    }
    const bool autoOn = m_stepAtt->autoAttEnabled();
    m_cmbAutoAttRx1Mode->setEnabled(autoOn);
    m_chkAutoAttUndoRx1->setEnabled(autoOn);
    m_spnAutoAttHoldRx1->setEnabled(autoOn && m_chkAutoAttUndoRx1->isChecked());
}

void GeneralOptionsPage::applyRadioHardwareAvailability()
{
    if (!m_stepAtt) {
        return;
    }
    const bool available = m_stepAtt->windowAvailable();
    const QString reason = available ? QString() : m_stepAtt->windowUnavailableReason();
    for (const char* name : {"grpStepAttenuator", "grpAutoAttRx1"}) {
        if (auto* group = findChild<QGroupBox*>(QLatin1String(name))) {
            group->setEnabled(available);
            group->setToolTip(reason);
        }
    }
    // R-R3-46 / R-R3-11: RX2's auto-attenuate group also needs a second
    // receive ADC and a Core that sends RX2's settings (refreshRx2StepAtt).
    refreshRx2StepAtt();
}

// R-R3-46 / R-R3-11: the RX2 row. Its value is the other receive ADC's own
// attenuator; it is usable on a radio with a second receive ADC while the
// step attenuator is on (and, in a remote window, when the Core sends it).
// Otherwise it is shown disabled with the reason.
// Level Cal 2 review: the top of the RX2 box. RX2's own value stops at the
// second ADC's 0-31 dB field (StepAttenuatorController::rx2MaxAttenuation,
// kRx2StepAttMaxDb); linked (diversity) it is RX1's and takes RX1's range.
int GeneralOptionsPage::rx2StepAttMaxDb() const
{
    if (m_ctrl) {
        return m_ctrl->adcAttenuatorsLinked() ? m_ctrl->maxAttenuation()
                                              : m_ctrl->rx2MaxAttenuation();
    }
    if (m_stepAtt) {
        return m_stepAtt->adcLinked()
            ? m_stepAtt->maxDb()
            : std::min(m_stepAtt->maxDb(), StepAttenuatorController::kRx2StepAttMaxDb);
    }
    return StepAttenuatorController::kRx2StepAttMaxDb;
}

void GeneralOptionsPage::refreshRx2StepAtt()
{
    if (!m_spnRx2StepAttValue || !m_chkRx2StepAttEnable) {
        return;
    }
    const RadioModel* radio = model();
    const bool twoAdc = radio && radio->boardCapabilities().attenuator.present
        && radio->boardCapabilities().adcCount >= 2;
    bool stepOn = false;
    bool autoOn = false;
    bool undoOn = false;
    int undoSec = m_spnAutoAttHoldRx2 ? m_spnAutoAttHoldRx2->value() : 5;
    int dB = m_spnRx2StepAttValue->value();
    bool sent = true;
    bool available = true;
    QString unavailable;
    if (m_ctrl) {
        stepOn = m_ctrl->rx2StepAttEnabled();
        autoOn = m_ctrl->rx2AutoAttEnabled();
        undoOn = m_ctrl->rx2AutoAttUndo();
        undoSec = m_ctrl->rx2AutoUndoDelaySec();
        dB = m_ctrl->rx2AttenuatorDb();
    } else if (m_stepAtt) {
        stepOn = m_stepAtt->rx2StepAttEnabled();
        autoOn = m_stepAtt->rx2AutoAttEnabled();
        undoOn = m_stepAtt->rx2AutoAttUndo();
        undoSec = m_stepAtt->rx2AutoAttUndoDelayMs() / 1000;
        dB = m_stepAtt->rx2AttenuationDb();
        const IStationLink* link = radio ? radio->stationLink() : nullptr;
        sent = link != nullptr && link->adcAttenuatorsAvailable();
        available = m_stepAtt->windowAvailable();
        unavailable = m_stepAtt->windowUnavailableReason();
    }
    const QString reason =
        !available ? unavailable
        : !twoAdc  ? tr("This radio has one receiver input, so RX1's attenuator covers every slice.")
        : !sent    ? tr("This Core does not send the second receiver's attenuator. Updating the Core may help.")
                   : QString();
    const bool rx2Usable = reason.isEmpty();

    {
        QSignalBlocker blk(m_chkRx2StepAttEnable);
        m_chkRx2StepAttEnable->setChecked(stepOn);
    }
    m_chkRx2StepAttEnable->setEnabled(rx2Usable);
    m_chkRx2StepAttEnable->setToolTip(rx2Usable ? tr("Enable the step attenuator.") : reason);
    {
        QSignalBlocker blk(m_spnRx2StepAttValue);
        m_spnRx2StepAttValue->setMaximum(rx2StepAttMaxDb());
        m_spnRx2StepAttValue->setValue(dB);
    }
    m_spnRx2StepAttValue->setEnabled(rx2Usable && stepOn);
    // With RX2's own enable off the value waits beside its RX2 Enable box,
    // as RX1's does; the tooltip says what it is.
    m_spnRx2StepAttValue->setToolTip(
        !rx2Usable ? reason
                   : tr("Attenuation for slices on the second receiver input (EXT1 or EXT2)."));

    // Auto Attenuate RX2 (Thetis setupAttRXControls(2): Undo and Hold follow
    // Enable, Hold follows Undo).
    if (auto* group = findChild<QGroupBox*>(QStringLiteral("grpAutoAttRx2"))) {
        group->setEnabled(rx2Usable);
        group->setToolTip(reason);
    }
    if (m_chkAutoAttRx2 && m_chkAutoAttUndoRx2 && m_spnAutoAttHoldRx2) {
        {
            QSignalBlocker blk(m_chkAutoAttRx2);
            m_chkAutoAttRx2->setChecked(autoOn);
        }
        {
            QSignalBlocker blk(m_chkAutoAttUndoRx2);
            m_chkAutoAttUndoRx2->setChecked(undoOn);
        }
        {
            QSignalBlocker blk(m_spnAutoAttHoldRx2);
            m_spnAutoAttHoldRx2->setValue(undoSec);
        }
        m_chkAutoAttUndoRx2->setEnabled(autoOn);
        m_spnAutoAttHoldRx2->setEnabled(autoOn && undoOn);
    }
}

} // namespace NereusSDR
