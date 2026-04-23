// =================================================================
// src/gui/setup/hardware/AntennaAlexTab.cpp  (NereusSDR)
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
//   2026-04-20 — Refactored into parent QTabWidget hosting three sub-sub-tabs:
//                 Antenna Control (existing table content), Alex-1 Filters (Task 8),
//                 Alex-2 Filters (placeholder for Task 9). J.J. Boyd (KG4VCF).
//   2026-04-20 — Replaced Alex-2 Filters placeholder with real AntennaAlexAlex2Tab
//                 (Task 9). J.J. Boyd (KG4VCF).
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

#include "AntennaAlexTab.h"
#include "AntennaAlexAntennaControlTab.h"
#include "AntennaAlexAlex1Tab.h"
#include "AntennaAlexAlex2Tab.h"

#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "models/RadioModel.h"

#include <QTabWidget>
#include <QVBoxLayout>

namespace NereusSDR {

// ── Constructor ───────────────────────────────────────────────────────────────

AntennaAlexTab::AntennaAlexTab(RadioModel* model, QWidget* parent)
    : QWidget(parent), m_model(model)
{
    // Top-level layout holds the sub-tab widget that mirrors Thetis tcAlexControl.
    // Source: Thetis tcAlexControl (setup.designer.cs:23385-23395) [@501e3f5]
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);

    m_subTabs = new QTabWidget(this);
    m_subTabs->setTabPosition(QTabWidget::North);
    outerLayout->addWidget(m_subTabs);

    // ── Tab 0: Antenna Control ────────────────────────────────────────────────
    // Source: Thetis tpAlexAntCtrl (setup.designer.cs:5981-7000) [@501e3f5]
    // Phase 3P-F Task 3: replaced placeholder with real AntennaAlexAntennaControlTab.
    m_antennaControlTab = new AntennaAlexAntennaControlTab(model, m_subTabs);
    m_subTabs->addTab(m_antennaControlTab, tr("Antenna Control"));

    // ── Tab 1: Alex-1 Filters ─────────────────────────────────────────────────
    // Source: Thetis tpAlexFilterControl (setup.designer.cs:23399-25538) [@501e3f5]
    m_alex1Tab = new AntennaAlexAlex1Tab(model, m_subTabs);
    m_subTabs->addTab(m_alex1Tab, tr("Alex-1 Filters"));

    // Forward Alex-1 settingChanged under the "alex1/" prefix so HardwarePage
    // routes it to the correct AppSettings namespace.
    connect(m_alex1Tab, &AntennaAlexAlex1Tab::settingChanged,
            this, [this](const QString& key, const QVariant& value) {
                emit settingChanged(QStringLiteral("alex1/") + key, value);
            });

    // ── Tab 2: Alex-2 Filters ─────────────────────────────────────────────────
    // Source: Thetis tpAlex2FilterControl (setup.designer.cs:25539-26857) [@501e3f5]
    m_alex2FiltersTab = new AntennaAlexAlex2Tab(model, m_subTabs);
    m_subTabs->addTab(m_alex2FiltersTab, tr("Alex-2 Filters"));

    // Forward Alex-2 settingChanged under the "alex2/" prefix so HardwarePage
    // routes it to the correct AppSettings namespace.
    connect(m_alex2FiltersTab, &AntennaAlexAlex2Tab::settingChanged,
            this, [this](const QString& key, const QVariant& value) {
                emit settingChanged(QStringLiteral("alex2/") + key, value);
            });

    // Antenna Control signals are now handled internally by AntennaAlexAntennaControlTab
    // via AlexController bindings. No parent-level wiring needed for Tab 0.
    // Phase 3P-F Task 3: placeholder connections removed.
}

// ── populate ──────────────────────────────────────────────────────────────────

void AntennaAlexTab::populate(const RadioInfo& info, const BoardCapabilities& caps)
{
    // If the board has no ALEX, HardwarePage hides this whole tab.
    // Antenna port capability gating (antennaInputCount) is now handled
    // internally by AntennaAlexAntennaControlTab via AlexController.
    // Phase 3P-F Task 3: removed old placeholder table gating code.

    // Gate Saturn BPF1 column on board type.
    // Saturn = ANAN-G2 / G2-1K (G8NJJ). SaturnMKII = MkII board revision.
    // From spec §7: "auto-hide on non-Saturn boards" [@501e3f5]
    const bool isSaturn = (caps.board == HPSDRHW::Saturn || caps.board == HPSDRHW::SaturnMKII);
    m_alex1Tab->updateBoardCapabilities(isSaturn);

    // Gate Alex-2 board status on caps.hasAlex2 (Phase 3P-I-b T8).
    // From Thetis setup.cs:6228-6264 [v2.10.3.13]: tpAlex2FilterControl
    // is visible only for BPF2-capable boards (ANAN7000D family +
    // OrionMKII + Saturn).
    //DH1KLM  [REDPITAYA-class SKU attribution in setup.cs:6256/6261]
    m_alex2FiltersTab->updateBoardCapabilities(caps.hasAlex2);
    const int alex2Idx = m_subTabs->indexOf(m_alex2FiltersTab);
    if (alex2Idx >= 0) {
        m_subTabs->setTabVisible(alex2Idx, caps.hasAlex2);
    }

    // Restore Alex-1 and Alex-2 filter settings from per-MAC AppSettings.
    m_lastMac = info.macAddress;
    m_alex1Tab->restoreSettings(info.macAddress);
    m_alex2FiltersTab->restoreSettings(info.macAddress);
}

// ── restoreSettings ───────────────────────────────────────────────────────────

void AntennaAlexTab::restoreSettings(const QMap<QString, QVariant>& /*settings*/)
{
    // Antenna Control per-band state is now owned by AlexController (Phase 3P-F Task 1)
    // and restored at connect time via RadioModel::connectToRadio() → m_alexController.load().
    // The old placeholder checkbox restore (rxOutOnTx / hfTrRelay / etc.) and the
    // old per-band RX table restore are no longer needed here.
    //
    // Alex-1 Filters tab uses per-MAC AppSettings directly (different key namespace).
    // Restore path: HardwarePage → populate() → m_alex1Tab->restoreSettings(mac).
    // No additional action needed here for the filtered map variant.
}

} // namespace NereusSDR
