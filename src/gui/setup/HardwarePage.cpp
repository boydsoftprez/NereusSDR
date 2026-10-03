// =================================================================
// src/gui/setup/HardwarePage.cpp  (NereusSDR)
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
//   2026-09-23 - R-R3-46: in a remote window the tabs show the Core's
//                 radio; edits stay off the Core's raw hardware keys.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-23 - R-R3-46: a remote window's receive settings write through
//                 to the Core, which applies them (radioHardwareVersion 2);
//                 transmit fields follow the transmit permission.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49: the XVTR and Bandwidth Monitor tabs stay hidden
//                 (UnbuiltFeatures) until their features are built.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-24 - R-R3-49: the Diversity tab is removed (DiversityTab
//                 deleted); saved diversity/* values stay in the file.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-25 - R-R3-46 / R-R3-49 (remote-window parity Task 13): the OC
//                 transmit pins close on the air in both windows (Thetis
//                 UpdateForHotSwitch); the pin actions and transmit
//                 calibration follow transmitSettingsVersion 8, User Dig Out
//                 the transmit settings gate. J.J. Boyd (KG4VCF),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-26 - R-R3-46 / R-R3-49 (remote-window parity Task 14): HL2
//                 Options' I2C tool and Pin Control and the Alex-1 tab's three
//                 transmit high-pass switches follow radioHardwareVersion 7.
//                 J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-49: the Alex Filters tabs' receive filter rows
//                (per-row bypass and edges, Alex-2 master bypass) select the
//                receive high-pass as Thetis's setAlexHPF /
//                setBPF1ForOrionIISaturn / setAlex2HPF do (radioHardwareVersion
//                8). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-46 / R-R3-49: the Alex-1 Filters tab's low-pass rows
//                and 6m/ByPass on RX select the low-pass as Thetis's
//                setAlexLPF does (radioHardwareVersion 10). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - HL2 Options' Enable CL2, CL2 frequency and External 10 MHz
//                follow radioHardwareVersion 11 in a remote window.
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - HL2 Options' Swap audio channels follows
//                radioHardwareVersion 13 in a remote window. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
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

#include "HardwarePage.h"
#include "gui/UnbuiltFeatures.h"

#include "hardware/RadioInfoTab.h"
#include "hardware/AntennaAlexTab.h"
#include "hardware/OcOutputsTab.h"
#include "hardware/XvtrTab.h"
#include "hardware/CalibrationTab.h"
#include "hardware/Hl2IoBoardTab.h"
#include "hardware/Hl2OptionsTab.h"
#include "hardware/BandwidthMonitorTab.h"

#include "core/AppSettings.h"
#include "core/accessories/AlexAntennaFacade.h"
#include "core/BoardCapabilities.h"
#include "core/HardwareProfile.h"
#include "core/RadioDiscovery.h"
#include "core/session/IStationLink.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

namespace NereusSDR {

// ── Construction ──────────────────────────────────────────────────────────────

HardwarePage::HardwarePage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Hardware Config"), model, parent)
    , m_model(model)
{
    // Replace the default SetupPage content area with a plain tab widget so
    // the sub-tabs fill the whole right pane. We insert the QTabWidget into
    // the inherited contentLayout() so SetupPage's title header is preserved.
    m_tabs = new QTabWidget(this);
    m_tabs->setTabPosition(QTabWidget::North);
    contentLayout()->setContentsMargins(0, 0, 0, 0);
    // R-R3-46: why a remote window cannot change these, when it cannot.
    m_remote = model != nullptr && !model->ownsLocalDsp();
    m_remoteNotice = new QLabel(this);
    m_remoteNotice->setObjectName(QStringLiteral("hardwareConfigUnavailable"));
    m_remoteNotice->setWordWrap(true);
    m_remoteNotice->setMargin(8);
    m_remoteNotice->hide();
    contentLayout()->addWidget(m_remoteNotice);
    contentLayout()->addWidget(m_tabs);

    // ── Create stub tab widgets ───────────────────────────────────────────────
    m_radioInfoTab    = new RadioInfoTab(model, this);
    m_antennaAlexTab  = new AntennaAlexTab(model, this);
    m_ocOutputsTab    = new OcOutputsTab(model, this);
    m_xvtrTab         = new XvtrTab(model, this);
    m_paCalTab        = new CalibrationTab(model, this);
    m_hl2OptionsTab   = new Hl2OptionsTab(model, this);
    m_hl2IoTab        = new Hl2IoBoardTab(model, this);
    m_bwMonitorTab    = new BandwidthMonitorTab(model, this);

    // ── Add tabs — order mirrors Thetis Setup.cs Hardware Config tab strip ────
    // Note: Setup → Hardware → PureSignal tab retired in Phase 3M-4 Task 14
    // (no Thetis equivalent; PsForm at Tools > PureSignal is the entire PS
    // control surface — design §4.2).
    m_radioInfoIdx   = m_tabs->addTab(m_radioInfoTab,   tr("Radio Info"));
    m_antennaAlexIdx = m_tabs->addTab(m_antennaAlexTab, tr("Antenna / ALEX"));
    m_ocOutputsIdx   = m_tabs->addTab(m_ocOutputsTab,   tr("OC Outputs"));
    m_xvtrIdx        = m_tabs->addTab(m_xvtrTab,        tr("XVTR"));
    m_paCalIdx       = m_tabs->addTab(m_paCalTab,       tr("Calibration"));
    m_hl2OptionsIdx  = m_tabs->addTab(m_hl2OptionsTab,  tr("HL2 Options"));
    m_hl2IoIdx       = m_tabs->addTab(m_hl2IoTab,       tr("HL2 I/O"));
    m_bwMonitorIdx   = m_tabs->addTab(m_bwMonitorTab,   tr("Bandwidth Monitor"));
    // R-R3-49: tabs whose feature is not built yet stay hidden, before and
    // after a radio is known (onCurrentRadioChanged ANDs the same check).
    m_tabs->setTabVisible(m_xvtrIdx, UnbuiltFeatures::isBuilt(UnbuiltFeature::Transverters));
    m_tabs->setTabVisible(m_bwMonitorIdx,
                          UnbuiltFeatures::isBuilt(UnbuiltFeature::BandwidthMonitor));

    // ── Wire per-tab settingChanged → write-through persistence (Task 21) ─────
    // Lambda helper: generic connect for any tab type that has settingChanged.
    auto wire = [this](auto* tab, const QString& tabKey) {
        connect(tab,
                &std::remove_pointer_t<decltype(tab)>::settingChanged,
                this,
                [this, tabKey](const QString& key, const QVariant& value) {
                    onTabSettingChanged(tabKey, key, value);
                });
    };
    wire(m_radioInfoTab,   QStringLiteral("radioInfo"));

    // Task 3.6: forward ANAN-8000DLE volts/amps toggle from RadioInfoTab up
    // through HardwarePage so SetupDialog can route it to MainWindow.
    connect(m_radioInfoTab, &RadioInfoTab::anan8000DleVoltsAmpsChanged,
            this,           &HardwarePage::anan8000DleVoltsAmpsChanged);

    wire(m_antennaAlexTab, QStringLiteral("antennaAlex"));

    // Phase 3M-4 Task 11: pass-through for the IMD-warning-gated HPF Bypass
    // on PureSignal feedback toggle.  Originates in AntennaAlexAlex1Tab,
    // bubbles through AntennaAlexTab; HardwarePage re-emits so SetupDialog
    // can wire it to the live PureSignal coordinator without traversing the
    // settingChanged key namespace.
    connect(m_antennaAlexTab, &AntennaAlexTab::hpfBypassOnPsChanged,
            this,             &HardwarePage::hpfBypassOnPsChanged);
    wire(m_ocOutputsTab,   QStringLiteral("ocOutputs"));
    wire(m_xvtrTab,        QStringLiteral("xvtr"));
    wire(m_paCalTab,       QStringLiteral("paCalibration"));
    wire(m_hl2OptionsTab,  QStringLiteral("hl2Options"));
    wire(m_hl2IoTab,       QStringLiteral("hl2IoBoard"));
    wire(m_bwMonitorTab,   QStringLiteral("bandwidthMonitor"));

    // ── Listen for live radio connection so sub-tabs populate ─────────────────
    if (m_remote) {
        if (AlexAntennaFacade* alex = m_model->alexAntennaFacade()) {
            connect(alex, &AlexAntennaFacade::windowAvailabilityChanged,
                    this, [this](bool) { applyRemoteAvailability(); });
        }
        applyRemoteAvailability();
    }

    // R-R3-46 / R-R3-49 (parity Task 13): the OC transmit pins close while
    // the radio is on the air, in a local window and a remote one alike.
    if (m_model) {
        connect(m_model, &RadioModel::coreOnAirChanged,
                this, [this](bool) { applyTransmitHardwareGates(); });
    }
    applyTransmitHardwareGates();

    if (m_model) {
        connect(m_model, &RadioModel::currentRadioChanged,
                this, &HardwarePage::onCurrentRadioChanged);
        // If we're constructed while a radio is already connected, refresh
        // now. Otherwise we'd show empty fields until the next reconnect.
        if (m_model->isConnected() && m_model->connection()) {
            onCurrentRadioChanged(m_model->connection()->radioInfo());
        } else if (!m_model->ownsLocalDsp()
                   && !m_model->currentRadioInfo().macAddress.isEmpty()) {
            // R-R3-46: a remote window has no connection of its own; the
            // Core's radio is the model's stored radio info.
            onCurrentRadioChanged(m_model->currentRadioInfo());
        }
    }
}

HardwarePage::~HardwarePage() = default;

// ── filterPrefix ─────────────────────────────────────────────────────────────

// static
QMap<QString, QVariant> HardwarePage::filterPrefix(const QMap<QString, QVariant>& map,
                                                     const QString& prefix)
{
    QMap<QString, QVariant> result;
    for (auto it = map.constBegin(); it != map.constEnd(); ++it) {
        if (it.key().startsWith(prefix)) {
            result.insert(it.key().mid(prefix.size()), it.value());
        }
    }
    return result;
}

// ── onTabSettingChanged ───────────────────────────────────────────────────────

void HardwarePage::onTabSettingChanged(const QString& tabKey,
                                        const QString& key,
                                        const QVariant& value)
{
    if (m_currentMac.isEmpty()) { return; } // no radio connected yet
    // R-R3-46: in a remote window the tabs show the Core's radio and write
    // through to its settings for that radio, as a local window writes its
    // own. The Core then reloads the controllers that hold them (its
    // hardware apply step), so the radio changes now and the Core's later
    // saves keep the change. Against a Core that does not offer that, the
    // edit is dropped (the tabs are disabled with the reason).
    if (m_remote && !remoteEditsAvailable()) { return; }

    // The tab emits keys like "radioInfo/sampleRate"; strip the leading tabKey/
    // prefix if already included, or compose it.
    // Tab signals use bare keys (e.g. "sampleRate") OR fully-prefixed keys
    // (e.g. "radioInfo/sampleRate"). Normalise: always store as <tabKey>/<key>.
    QString bareKey = key;
    const QString tabPrefix = tabKey + QLatin1Char('/');
    if (bareKey.startsWith(tabPrefix)) {
        bareKey = bareKey.mid(tabPrefix.size());
    }
    const QString fullKey = QStringLiteral("%1/%2").arg(tabKey, bareKey);

    AppSettings::instance().setHardwareValue(m_currentMac, fullKey, value);
    AppSettings::instance().save();

    // Note: Phase 3M-4 Task 14 retired the Setup → Hardware → PureSignal tab.
    // The previous Task-2.5-of-P1-full-parity bridge that drove
    // TransmitModel::setPureSigEnabled from the tab's settingChanged signal
    // is gone with the tab — no other tab emits tabKey == "pureSignal".
    // Persistence of the user PS-enable proxy now lives outside this page
    // (PsForm + General Options); TransmitModel::loadFromSettings still
    // reads the same hardware/<mac>/pureSignal/enabled key on connect.
}

// ── onCurrentRadioChanged ─────────────────────────────────────────────────────

void HardwarePage::onCurrentRadioChanged(const RadioInfo& info)
{
    m_currentMac = info.macAddress;

    // Use HardwareProfile for capability lookup (Phase 3I-RP).
    const BoardCapabilities& caps = *m_model->hardwareProfile().caps;

    // Radio Info is always visible.
    // Remaining tabs are shown only when the connected board supports them.
    m_tabs->setTabVisible(m_antennaAlexIdx, caps.hasAlexFilters);

    // OC Outputs tab — visible for any board with OC pins OR for HL2 (whose
    // I/O board pattern reuses the OcMatrix for N2ADR Filter pin assignments).
    // mi0bot exposes tpPennyCtrl on HL2 with title "Hermes Lite Control"
    // (setup.cs:20232 [v2.10.3.13-beta2]).
    const bool ocTabRelevant = (caps.ocOutputCount > 0) || caps.hasIoBoardHl2;
    m_tabs->setTabVisible(m_ocOutputsIdx, ocTabRelevant);
    m_tabs->setTabText(m_ocOutputsIdx,
        caps.hasIoBoardHl2 ? tr("Hermes Lite Control") : tr("OC Outputs"));

    m_tabs->setTabVisible(m_xvtrIdx,        caps.xvtrJackCount > 0
                                            && UnbuiltFeatures::isBuilt(UnbuiltFeature::Transverters));
    // Calibration tab is always visible — its 4 remaining groups (Freq Cal,
    // Level Cal, HPSDR Diag, TX Display) apply to every board, and Group 5
    // (Volts/Amps Cal) is harmless on boards without integrated PA. PA-cal
    // spinboxes (formerly Group 6) moved to PA → Watt Meter in IA reshape
    // Phase 3, gated there on caps.hasPaProfile via the PA top-level
    // category gate in SetupDialog.cpp.
    m_tabs->setTabVisible(m_hl2OptionsIdx,  caps.hasIoBoardHl2);
    m_tabs->setTabVisible(m_hl2IoIdx,       caps.hasIoBoardHl2);
    m_tabs->setTabVisible(m_bwMonitorIdx,   caps.hasBandwidthMonitor
                                            && UnbuiltFeatures::isBuilt(UnbuiltFeature::BandwidthMonitor));

    // Populate each tab with the new board info.
    m_radioInfoTab->populate(info, caps);
    m_antennaAlexTab->populate(info, caps);
    m_ocOutputsTab->populate(info, caps);
    m_xvtrTab->populate(info, caps);
    m_paCalTab->populate(info, caps);
    m_hl2OptionsTab->populate(info, caps);
    m_hl2IoTab->populate(info, caps);
    m_bwMonitorTab->populate(info, caps);

    // Restore persisted settings for this radio's MAC (Task 21).
    if (!m_currentMac.isEmpty()) {
        const auto all = AppSettings::instance().hardwareValues(m_currentMac);
        m_radioInfoTab->restoreSettings(   filterPrefix(all, QStringLiteral("radioInfo/")));
        m_antennaAlexTab->restoreSettings( filterPrefix(all, QStringLiteral("antennaAlex/")));
        m_ocOutputsTab->restoreSettings(   filterPrefix(all, QStringLiteral("ocOutputs/")));
        m_xvtrTab->restoreSettings(        filterPrefix(all, QStringLiteral("xvtr/")));
        m_paCalTab->restoreSettings(       filterPrefix(all, QStringLiteral("paCalibration/")));
        m_hl2OptionsTab->restoreSettings(  filterPrefix(all, QStringLiteral("hl2Options/")));
        m_hl2IoTab->restoreSettings(       filterPrefix(all, QStringLiteral("hl2IoBoard/")));
        m_bwMonitorTab->restoreSettings(   filterPrefix(all, QStringLiteral("bandwidthMonitor/")));
    }
}

// ── Remote availability and transmit permission (R-R3-46) ────────────────────

bool HardwarePage::remoteEditsAvailable() const
{
    const AlexAntennaFacade* alex = m_model ? m_model->alexAntennaFacade() : nullptr;
    return alex != nullptr && alex->windowAvailable();
}

void HardwarePage::applyRemoteAvailability()
{
    if (!m_remote) {
        return;
    }
    const bool available = remoteEditsAvailable();
    const AlexAntennaFacade* alex = m_model ? m_model->alexAntennaFacade() : nullptr;
    const QString reason = available || alex == nullptr ? QString()
                                                        : alex->windowUnavailableReason();
    m_tabs->setEnabled(available);
    m_tabs->setToolTip(reason);
    m_remoteNotice->setText(reason);
    m_remoteNotice->setVisible(!available && !reason.isEmpty());
}

void HardwarePage::setTransmitPermitted(bool permitted, const QString& reason)
{
    m_antennaAlexTab->setTransmitPermitted(permitted, reason);
    m_ocOutputsTab->setTransmitPermitted(permitted, reason);
    m_hl2OptionsTab->setTransmitPermitted(permitted, reason);
    m_hl2IoTab->setTransmitPermitted(permitted, reason);
}

void HardwarePage::setTransmitSettingsPermitted(bool permitted, const QString& reason)
{
    m_ocOutputsTab->setUserDigOutPermitted(permitted, reason);
    applyTransmitHardwareGates();
}

void HardwarePage::setTransmitSettingsPermittedAt(int /*version*/, bool /*permitted*/,
                                                  const QString& /*reason*/)
{
    // The dialog pushes each version on every link change; the gates here
    // read the link and the on-air state themselves (below), since the
    // version 8 push also closes on the air and part of version 8 does not.
    applyTransmitHardwareGates();
}

void HardwarePage::applyTransmitHardwareGates()
{
    if (!m_ocOutputsTab || !m_paCalTab || !m_hl2IoTab) {
        return;
    }
    // R-R3-46 / R-R3-49 (parity Task 13): a local window changes its own
    // radio; a remote one only a Core at transmitSettingsVersion 8.
    const IStationLink* link = m_model ? m_model->stationLink() : nullptr;
    const bool offered = !m_remote || (link != nullptr && link->transmitSettingsAvailable(8));
    const QString notOffered = IStationLink::transmitSettingsUnavailableReason();
    // Thetis greys the TX pin boxes while MOX is on unless OC hot switching
    // is allowed, which NereusSDR does not build (the box is hidden), so
    // they close on the air in both windows. The pin actions, TX Display
    // Cal and Volts/Amps Calibration have no such rule in Thetis.
    // From Thetis setup.cs:21944 [v2.10.3.15] UpdateForHotSwitch
    //   bool enable = !tx || (tx && chkAllowHotSwitching.Checked);
    const bool onAir = m_model != nullptr && m_model->isCoreOnAir();
    m_ocOutputsTab->setTransmitPinsPermitted(offered && !onAir,
                                             offered ? RadioModel::onAirReason() : notOffered);
    m_ocOutputsTab->setPinActionsPermitted(offered, notOffered);
    m_paCalTab->setTransmitCalibrationPermitted(offered, notOffered);
    // The N2ADR switch: a Core at version 8 applies its whole preset, so
    // its tooltip no longer says it moves the receive filters only.
    m_hl2IoTab->setCoreAppliesWholeN2adrPreset(m_remote && offered);
    // R-R3-46 / R-R3-49 (parity Task 14): a remote window reaches the
    // Core's I2C bus and output pins, and changes the three transmit
    // high-pass switches, only on a Core at radioHardwareVersion 7. The
    // tab itself closes a write and Pin Control on the air; the three
    // switches have no on-air rule (Thetis sets them with no MOX check).
    const bool hardwareOffered =
        !m_remote || (link != nullptr && link->radioHardwareAvailable(7));
    if (m_hl2OptionsTab) {
        m_hl2OptionsTab->setIoBoardControlAvailable(
            hardwareOffered, IStationLink::ioBoardI2cUnavailableReason());
    }
    if (m_antennaAlexTab) {
        m_antennaAlexTab->setHpfSwitchesAvailable(
            hardwareOffered, IStationLink::alexHpfSwitchesUnavailableReason());
        // radioHardwareVersion 8: the receive filter rows, which the Core
        // applies to its radio at once (RadioModel::savedAlexHpfEdges). No
        // on-air rule: Thetis's per-row setters have no MOX check.
        m_antennaAlexTab->setHpfRowsAvailable(
            !m_remote || (link != nullptr && link->radioHardwareAvailable(8)),
            IStationLink::alexHpfRowsUnavailableReason());
        // radioHardwareVersion 10: the low-pass rows and 6m/ByPass on RX.
        // No on-air rule: Thetis's spinner and check box handlers have no
        // MOX check.
        m_antennaAlexTab->setLpfRowsAvailable(
            !m_remote || (link != nullptr && link->radioHardwareAvailable(10)),
            IStationLink::alexLpfRowsUnavailableReason());
    }
    // radioHardwareVersion 11: HL2 Options' clock options (Enable CL2, CL2
    // frequency, External 10 MHz), which the Core sends to its radio. No
    // on-air rule: mi0bot's handlers have no MOX check.
    if (m_hl2OptionsTab) {
        m_hl2OptionsTab->setClockControlAvailable(
            !m_remote || (link != nullptr && link->radioHardwareAvailable(11)),
            IStationLink::hl2ClockUnavailableReason());
        // radioHardwareVersion 13: HL2 Options' Swap audio channels, which
        // the Core applies to the receive audio it sends its radio. No
        // on-air rule: mi0bot's handler has no MOX check.
        m_hl2OptionsTab->setSwapAudioAvailable(
            !m_remote || (link != nullptr && link->radioHardwareAvailable(13)),
            IStationLink::hl2SwapAudioUnavailableReason());
    }
}

bool HardwarePage::showAntennaTab()
{
    if (m_antennaAlexIdx < 0 || !m_tabs->isTabVisible(m_antennaAlexIdx)) {
        return false;
    }
    m_tabs->setCurrentIndex(m_antennaAlexIdx);
    return true;
}

// ── Test helper ───────────────────────────────────────────────────────────────

#ifdef NEREUS_BUILD_TESTS
QString HardwarePage::currentTabText() const
{
    return m_tabs->tabText(m_tabs->currentIndex());
}

bool HardwarePage::isTabVisibleForTest(Tab t) const
{
    switch (t) {
        case Tab::RadioInfo:        return m_tabs->isTabVisible(m_radioInfoIdx);
        case Tab::AntennaAlex:      return m_tabs->isTabVisible(m_antennaAlexIdx);
        case Tab::OcOutputs:        return m_tabs->isTabVisible(m_ocOutputsIdx);
        case Tab::Xvtr:             return m_tabs->isTabVisible(m_xvtrIdx);
        case Tab::Calibration:      return m_tabs->isTabVisible(m_paCalIdx);
        case Tab::Hl2Options:       return m_tabs->isTabVisible(m_hl2OptionsIdx);
        case Tab::Hl2IoBoard:       return m_tabs->isTabVisible(m_hl2IoIdx);
        case Tab::BandwidthMonitor: return m_tabs->isTabVisible(m_bwMonitorIdx);
    }
    return false;
}

QWidget* HardwarePage::tabWidgetForTest(Tab t) const
{
    switch (t) {
        case Tab::RadioInfo:        return m_radioInfoTab;
        case Tab::AntennaAlex:      return m_antennaAlexTab;
        case Tab::OcOutputs:        return m_ocOutputsTab;
        case Tab::Xvtr:             return m_xvtrTab;
        case Tab::Calibration:      return m_paCalTab;
        case Tab::Hl2Options:       return m_hl2OptionsTab;
        case Tab::Hl2IoBoard:       return m_hl2IoTab;
        case Tab::BandwidthMonitor: return m_bwMonitorTab;
    }
    return nullptr;
}

bool HardwarePage::remoteEditsAvailableForTest() const
{
    return !m_remote || remoteEditsAvailable();
}

QString HardwarePage::tabTextForTest(Tab t) const
{
    switch (t) {
        case Tab::RadioInfo:        return m_tabs->tabText(m_radioInfoIdx);
        case Tab::AntennaAlex:      return m_tabs->tabText(m_antennaAlexIdx);
        case Tab::OcOutputs:        return m_tabs->tabText(m_ocOutputsIdx);
        case Tab::Xvtr:             return m_tabs->tabText(m_xvtrIdx);
        case Tab::Calibration:      return m_tabs->tabText(m_paCalIdx);
        case Tab::Hl2Options:       return m_tabs->tabText(m_hl2OptionsIdx);
        case Tab::Hl2IoBoard:       return m_tabs->tabText(m_hl2IoIdx);
        case Tab::BandwidthMonitor: return m_tabs->tabText(m_bwMonitorIdx);
    }
    return {};
}
#endif

} // namespace NereusSDR
