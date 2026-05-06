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
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"
#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/PureSignal.h"
#include "core/StepAttenuatorController.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
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
        // hermes-filter-debug Bug 1: pull BOTH bounds from the controller —
        // HL2 uses the signed -28..+31 range (mi0bot setup.cs:16085-16086
        // [v2.10.3.13-beta2]).  The previous hardcoded `0` minimum clamped
        // any HL2 negative-dB value the user typed back to zero.
        const int minDb = m_ctrl->minAttenuation();
        const int maxDb = m_ctrl->maxAttenuation();
        m_spnRx1StepAttValue->setRange(minDb, maxDb);
        m_spnRx2StepAttValue->setRange(minDb, maxDb);
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

        // hermes-filter-debug Bug 1: re-range the step-att spinboxes when
        // the connected board changes (e.g. user switches HL2 ↔ ANAN-G2
        // without restarting Setup).  The controller min/max are pushed
        // by RadioModel::connectToRadio from boardCapabilities().attenuator
        // before this signal fires.
        if (m_ctrl && m_spnRx1StepAttValue && m_spnRx2StepAttValue) {
            const int minDb = m_ctrl->minAttenuation();
            const int maxDb = m_ctrl->maxAttenuation();
            m_spnRx1StepAttValue->setRange(minDb, maxDb);
            m_spnRx2StepAttValue->setRange(minDb, maxDb);
        }
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

    // ── Phase 3M-4 Task 11: PureSignal Info Bar checkboxes ─────────────────
    //
    // From Thetis setup.designer.cs:10567-10597 [v2.10.3.13] chkHideFeebackLevel:
    //   "Hide feedback level number" (designer text), tooltip: "Hide the
    //   feedback level from the info bar".  Thetis preserves the typo
    //   "Feeback" in the objectName — NereusSDR uses corrected spelling
    //   in user-visible text, source-cite preserves the typo for traceability.
    // The "Mirror of FB-label right-click" tooltip cue is NereusSDR-original
    // (Thetis has no banner-click hook explanation in tooltip).
    m_chkHideFeedback = new QCheckBox(tr("Hide feedback level"), group);
    m_chkHideFeedback->setObjectName(QStringLiteral("chkHideFeedbackLevel"));
    m_chkHideFeedback->setToolTip(
        tr("When checked, the bottom-banner FB indicator shows \"Feedback\" "
           "text instead of the numeric level. Mirror of FB-label right-click."));
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
        tr("Swap red and blue PS-A feedback colours"), group);
    m_chkSwapRedBlue->setObjectName(QStringLiteral("chkSwapREDBluePSAColours"));
    m_chkSwapRedBlue->setToolTip(
        tr("For users with red/blue color blindness or alternate display "
           "preferences. Mirror of FB-label left-click."));
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

    // radio-mic-input Task 23 controls.
    // Three new controls bind directly to TransmitModel; persistence is
    // per-MAC inside TransmitModel::loadFromSettings/persistToSettings.
    // Per project rule "no source cites in user-visible strings" tooltips
    // stay plain English; Thetis cites live only in these inline comments.

    if (RadioModel* rm = model()) {
        TransmitModel& tm = rm->transmitModel();

        // Disable PTT.
        // From Thetis setup.cs:6535-6539 [v2.10.3.13+501e3f51]
        //   chkGeneralDisablePTT_CheckedChanged: console.DisablePTT = chkGeneralDisablePTT.Checked
        // Designer: setup.designer.cs:9228-9237 [v2.10.3.13+501e3f51]
        //   Text="Disable PTT", tooltip "Disable Push To Talk detection."
        m_disablePttCheck = new QCheckBox(tr("Disable PTT"), group);
        m_disablePttCheck->setObjectName(QStringLiteral("disablePttCheck"));
        m_disablePttCheck->setToolTip(QStringLiteral(
            "Block all PTT sources from engaging the radio. Mirrors Thetis "
            "Setup -> General -> Options -> Disable PTT."));
        m_disablePttCheck->setChecked(tm.disablePTT());
        connect(m_disablePttCheck, &QCheckBox::toggled,
                &tm, &TransmitModel::setDisablePTT);
        connect(&tm, &TransmitModel::disablePTTChanged,
                m_disablePttCheck, &QCheckBox::setChecked);
        vbox->addWidget(m_disablePttCheck);

        // All-Mode Mic PTT.
        // From Thetis setup.cs:12127-12130 [v2.10.3.13+501e3f51]
        //   chkGenAllModeMicPTT_CheckedChanged: console.AllModeMicPTT = chkGenAllModeMicPTT.Checked
        // Designer: setup.designer.cs:9217-9226 [v2.10.3.13+501e3f51]
        //   Text="All Mode Mic PTT", tooltip "If checked, the Mic PTT is no
        //   longer limited to just voice modes."
        m_allModeMicPttCheck = new QCheckBox(tr("All-Mode Mic PTT"), group);
        m_allModeMicPttCheck->setObjectName(QStringLiteral("allModeMicPttCheck"));
        m_allModeMicPttCheck->setToolTip(QStringLiteral(
            "Allow mic-jack PTT to drive transmit in non-voice modes (CW etc.). "
            "Mirrors Thetis Setup -> General -> Options -> All Mode Mic PTT."));
        m_allModeMicPttCheck->setChecked(tm.allModeMicPTT());
        connect(m_allModeMicPttCheck, &QCheckBox::toggled,
                &tm, &TransmitModel::setAllModeMicPTT);
        connect(&tm, &TransmitModel::allModeMicPTTChanged,
                m_allModeMicPttCheck, &QCheckBox::setChecked);
        vbox->addWidget(m_allModeMicPttCheck);

        // PTT Out Delay.
        // From Thetis setup.cs:14302-14305 [v2.10.3.13+501e3f51]
        //   udGenPTTOutDelay_ValueChanged: console.PTTOutDelay = (int)udGenPTTOutDelay.Value
        // Range from setup.designer.cs:9176-9204 [v2.10.3.13+501e3f51]:
        //   Min=0, Max=500, Increment=1.  Tooltip "Delay between PTT Out
        //   drops to RX On."
        auto* pttRow = new QHBoxLayout;
        auto* pttLabel = new QLabel(tr("PTT Out Delay:"), group);
        m_pttOutDelaySpin = new QSpinBox(group);
        m_pttOutDelaySpin->setObjectName(QStringLiteral("pttOutDelaySpin"));
        m_pttOutDelaySpin->setRange(0, 500);
        m_pttOutDelaySpin->setSuffix(QStringLiteral(" ms"));
        m_pttOutDelaySpin->setFixedWidth(80);
        m_pttOutDelaySpin->setToolTip(QStringLiteral(
            "Delay between local TX engagement and the rear-panel PTT-OUT "
            "relay closing. Mirrors Thetis Setup -> General -> Options -> "
            "PTT Out Delay."));
        m_pttOutDelaySpin->setValue(tm.pttOutDelayMs());
        connect(m_pttOutDelaySpin, &QSpinBox::valueChanged,
                &tm, &TransmitModel::setPttOutDelayMs);
        connect(&tm, &TransmitModel::pttOutDelayMsChanged,
                m_pttOutDelaySpin, &QSpinBox::setValue);
        pttRow->addWidget(pttLabel);
        pttRow->addWidget(m_pttOutDelaySpin);
        pttRow->addStretch();
        vbox->addLayout(pttRow);
    }

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
    // Issue #259: RX2 step-att UI is constructed (m_chkRx2StepAttEnable +
    // m_spnRx2StepAttValue) and added to the layout, then hidden until the
    // controller gains independent RX2 state (m_stepAttEnabledRx2 / m_attDbRx2)
    // plus its own rx2Enabled / rx2Value / rx2Band/<band> persistence schema.
    // Hiding rather than removing keeps the existing widget members alive so
    // the connectController() / initFromController() blocks below stay
    // structurally identical to the eventual RX2-enabled wiring. The Thetis
    // contract — independent RX1/RX2 storage with a click-time
    // RX1↔RX2 mirror when nRX1ADCinUse == nRX2ADCinUse (setup.cs:15741-15760
    // [v2.10.3.13]) — is the follow-up implementation target.
    auto* rx2Row = new QHBoxLayout;
    m_chkRx2StepAttEnable = new QCheckBox(QStringLiteral("RX2 Enable"), group);
    m_chkRx2StepAttEnable->setToolTip(QStringLiteral("Enable the step attenuator."));
    m_chkRx2StepAttEnable->setVisible(false);
    m_spnRx2StepAttValue = makeDbSpinBox(group);
    m_spnRx2StepAttValue->setEnabled(false);
    m_spnRx2StepAttValue->setVisible(false);
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
        }
    });

    // --- Spinbox → controller ---
    // From Thetis setup.cs:15765-15772 [v2.10.3.13] udHermesStepAttenuator
    // Data_ValueChanged → console.RX1AttenuatorData. (The model-gated
    // Maximum=61 branch at setup.cs:15773-15786 lives in BoardCapsTable
    // ::stepAttMaxDb in NereusSDR.)
    connect(m_spnRx1StepAttValue, &QSpinBox::valueChanged, this, [this](int dB) {
        if (m_ctrl) {
            m_ctrl->setAttenuation(dB, 0);
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

        // Issue #259: hide the Auto-Att RX2 group alongside the hidden
        // RX2 step-att row. The controller is single-RX (auto-att is
        // RX1-only too); independent RX2 auto-att lands with the
        // controller-side RX2 refactor. From Thetis groupBoxTS47 the
        // Auto-Att RX1 / RX2 boxes are independent — same Phase 3F
        // follow-up scope as RX2 step-att.
        if (rx == 1) {
            group->setVisible(false);
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
    });
}

// ---------------------------------------------------------------------------
// initFromController — pull the controller's current state into the widgets
// at page construction time.
//
// Issue #259 — the SetupDialog (and every page in it) is constructed lazily
// on every Tools → Setup open via `new SetupDialog(m_radioModel, this)` at
// the seven call sites in MainWindow.cpp. So:
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
