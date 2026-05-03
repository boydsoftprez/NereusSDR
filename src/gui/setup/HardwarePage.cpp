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

#include "hardware/RadioInfoTab.h"
#include "hardware/AntennaAlexTab.h"
#include "hardware/OcOutputsTab.h"
#include "hardware/XvtrTab.h"
#include "hardware/PureSignalTab.h"
#include "hardware/DiversityTab.h"
#include "hardware/CalibrationTab.h"
#include "hardware/Hl2IoBoardTab.h"
#include "hardware/Hl2OptionsTab.h"
#include "hardware/BandwidthMonitorTab.h"

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/HardwareProfile.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

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
    contentLayout()->addWidget(m_tabs);

    // ── Create stub tab widgets ───────────────────────────────────────────────
    m_radioInfoTab    = new RadioInfoTab(model, this);
    m_antennaAlexTab  = new AntennaAlexTab(model, this);
    m_ocOutputsTab    = new OcOutputsTab(model, this);
    m_xvtrTab         = new XvtrTab(model, this);
    m_pureSignalTab   = new PureSignalTab(model, this);
    m_diversityTab    = new DiversityTab(model, this);
    m_paCalTab        = new CalibrationTab(model, this);
    m_hl2OptionsTab   = new Hl2OptionsTab(model, this);
    m_hl2IoTab        = new Hl2IoBoardTab(model, this);
    m_bwMonitorTab    = new BandwidthMonitorTab(model, this);

    // ── Add tabs — order mirrors Thetis Setup.cs Hardware Config tab strip ────
    m_radioInfoIdx   = m_tabs->addTab(m_radioInfoTab,   tr("Radio Info"));
    m_antennaAlexIdx = m_tabs->addTab(m_antennaAlexTab, tr("Antenna / ALEX"));
    m_ocOutputsIdx   = m_tabs->addTab(m_ocOutputsTab,   tr("OC Outputs"));
    m_xvtrIdx        = m_tabs->addTab(m_xvtrTab,        tr("XVTR"));
    m_pureSignalIdx  = m_tabs->addTab(m_pureSignalTab,  tr("PureSignal"));
    m_diversityIdx   = m_tabs->addTab(m_diversityTab,   tr("Diversity"));
    m_paCalIdx       = m_tabs->addTab(m_paCalTab,       tr("Calibration"));
    m_hl2OptionsIdx  = m_tabs->addTab(m_hl2OptionsTab,  tr("HL2 Options"));
    m_hl2IoIdx       = m_tabs->addTab(m_hl2IoTab,       tr("HL2 I/O"));
    m_bwMonitorIdx   = m_tabs->addTab(m_bwMonitorTab,   tr("Bandwidth Monitor"));

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
    wire(m_antennaAlexTab, QStringLiteral("antennaAlex"));
    wire(m_ocOutputsTab,   QStringLiteral("ocOutputs"));
    wire(m_xvtrTab,        QStringLiteral("xvtr"));
    wire(m_pureSignalTab,  QStringLiteral("pureSignal"));
    wire(m_diversityTab,   QStringLiteral("diversity"));
    wire(m_paCalTab,       QStringLiteral("paCalibration"));
    wire(m_hl2OptionsTab,  QStringLiteral("hl2Options"));
    wire(m_hl2IoTab,       QStringLiteral("hl2IoBoard"));
    wire(m_bwMonitorTab,   QStringLiteral("bandwidthMonitor"));

    // ── Listen for live radio connection so sub-tabs populate ─────────────────
    if (m_model) {
        connect(m_model, &RadioModel::currentRadioChanged,
                this, &HardwarePage::onCurrentRadioChanged);
        // If we're constructed while a radio is already connected, refresh
        // now. Otherwise we'd show empty fields until the next reconnect.
        if (m_model->isConnected() && m_model->connection()) {
            onCurrentRadioChanged(m_model->connection()->radioInfo());
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

    // ── Task 2.5 of P1 full-parity epic: PureSignal "Enable" → TransmitModel ─
    // When the user toggles the Setup → Hardware → PureSignal "Enable"
    // checkbox, also drive the TransmitModel::pureSig property so the
    // model→connection wiring (RadioModel::wireConnectionSignals) emits
    // setPuresignalRun on the wire bit.  TransmitModel::loadFromSettings
    // reads the same hardware/<mac>/pureSignal/enabled key on connect, so
    // persistence stays single-sourced from this writer.
    //
    // Source: Thetis PSForm.cs:240 [v2.10.3.13] — _psenabled = value
    // is the user-facing PS-enable toggle that drives prn->puresignal_run
    // via NetworkIO.SetPureSignal.
    if (m_model && tabKey == QLatin1String("pureSignal")
                && bareKey == QLatin1String("enabled")) {
        m_model->transmitModel().setPureSigEnabled(value.toBool());
    }
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

    m_tabs->setTabVisible(m_xvtrIdx,        caps.xvtrJackCount > 0);
    m_tabs->setTabVisible(m_pureSignalIdx,  caps.hasPureSignal);
    m_tabs->setTabVisible(m_diversityIdx,   caps.hasDiversityReceiver);
    // Calibration tab is always visible — its 4 remaining groups (Freq Cal,
    // Level Cal, HPSDR Diag, TX Display) apply to every board, and Group 5
    // (Volts/Amps Cal) is harmless on boards without integrated PA. PA-cal
    // spinboxes (formerly Group 6) moved to PA → Watt Meter in IA reshape
    // Phase 3, gated there on caps.hasPaProfile via the PA top-level
    // category gate in SetupDialog.cpp.
    m_tabs->setTabVisible(m_hl2OptionsIdx,  caps.hasIoBoardHl2);
    m_tabs->setTabVisible(m_hl2IoIdx,       caps.hasIoBoardHl2);
    m_tabs->setTabVisible(m_bwMonitorIdx,   caps.hasBandwidthMonitor);

    // Populate each tab with the new board info.
    m_radioInfoTab->populate(info, caps);
    m_antennaAlexTab->populate(info, caps);
    m_ocOutputsTab->populate(info, caps);
    m_xvtrTab->populate(info, caps);
    m_pureSignalTab->populate(info, caps);
    m_diversityTab->populate(info, caps);
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
        m_pureSignalTab->restoreSettings(  filterPrefix(all, QStringLiteral("pureSignal/")));
        m_diversityTab->restoreSettings(   filterPrefix(all, QStringLiteral("diversity/")));
        m_paCalTab->restoreSettings(       filterPrefix(all, QStringLiteral("paCalibration/")));
        m_hl2OptionsTab->restoreSettings(  filterPrefix(all, QStringLiteral("hl2Options/")));
        m_hl2IoTab->restoreSettings(       filterPrefix(all, QStringLiteral("hl2IoBoard/")));
        m_bwMonitorTab->restoreSettings(   filterPrefix(all, QStringLiteral("bandwidthMonitor/")));
    }
}

// ── Test helper ───────────────────────────────────────────────────────────────

#ifdef NEREUS_BUILD_TESTS
bool HardwarePage::isTabVisibleForTest(Tab t) const
{
    switch (t) {
        case Tab::RadioInfo:        return m_tabs->isTabVisible(m_radioInfoIdx);
        case Tab::AntennaAlex:      return m_tabs->isTabVisible(m_antennaAlexIdx);
        case Tab::OcOutputs:        return m_tabs->isTabVisible(m_ocOutputsIdx);
        case Tab::Xvtr:             return m_tabs->isTabVisible(m_xvtrIdx);
        case Tab::PureSignal:       return m_tabs->isTabVisible(m_pureSignalIdx);
        case Tab::Diversity:        return m_tabs->isTabVisible(m_diversityIdx);
        case Tab::Calibration:      return m_tabs->isTabVisible(m_paCalIdx);
        case Tab::Hl2Options:       return m_tabs->isTabVisible(m_hl2OptionsIdx);
        case Tab::Hl2IoBoard:       return m_tabs->isTabVisible(m_hl2IoIdx);
        case Tab::BandwidthMonitor: return m_tabs->isTabVisible(m_bwMonitorIdx);
    }
    return false;
}

QString HardwarePage::tabTextForTest(Tab t) const
{
    switch (t) {
        case Tab::RadioInfo:        return m_tabs->tabText(m_radioInfoIdx);
        case Tab::AntennaAlex:      return m_tabs->tabText(m_antennaAlexIdx);
        case Tab::OcOutputs:        return m_tabs->tabText(m_ocOutputsIdx);
        case Tab::Xvtr:             return m_tabs->tabText(m_xvtrIdx);
        case Tab::PureSignal:       return m_tabs->tabText(m_pureSignalIdx);
        case Tab::Diversity:        return m_tabs->tabText(m_diversityIdx);
        case Tab::Calibration:      return m_tabs->tabText(m_paCalIdx);
        case Tab::Hl2Options:       return m_tabs->tabText(m_hl2OptionsIdx);
        case Tab::Hl2IoBoard:       return m_tabs->tabText(m_hl2IoIdx);
        case Tab::BandwidthMonitor: return m_tabs->tabText(m_bwMonitorIdx);
    }
    return {};
}
#endif

} // namespace NereusSDR
