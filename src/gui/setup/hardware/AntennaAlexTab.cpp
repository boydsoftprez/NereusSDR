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
//   2026-09-23 - R-R3-46: forwards the transmit permission to Antenna
//                 Control. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-24 - R-R3-46: forwards the transmit permission to Alex-1
//                Filters too. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-25 - R-R3-49 / R-R3-46 (parity Task 12): Antenna Control's
//                transmit half now follows whether the Core takes it (the
//                Alex facade), so only Alex-1 Filters gets the transmit
//                permission. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//                Claude Code.
//   2026-09-26 - R-R3-46 / R-R3-49 (parity Task 14): forwards the Alex-1
//                high-pass switches' availability. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-49: the Alex Filters tabs' receive filter rows
//                (per-row bypass and edges, Alex-2 master bypass) select the
//                receive high-pass as Thetis's setAlexHPF /
//                setBPF1ForOrionIISaturn / setAlex2HPF do (radioHardwareVersion
//                8). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - The BPF1 rows follow codec::alex::usesBpf1Preselector, so
//                the ANAN-7000DLE / 8000DLE (OrionMKII) show them as Thetis
//                does. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - The Alex-1 panels follow the bank the Core programs
//                (usesBpf1Preselector, console.cs:6827-6837): BPF1 with the
//                switches, or Alex HPF with them. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-46 / R-R3-49: the Alex-1 Filters tab's low-pass rows
//                and 6m/ByPass on RX select the low-pass as Thetis's
//                setAlexLPF does (radioHardwareVersion 10). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-30 - Keeps G8NJJ's setup.cs:6244 comment beside the Alex-2 gate
//                cite (CI tag preservation). J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
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
#include "core/codec/AlexFilterMap.h"
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

    // Phase 3M-4 Task 11: pass-through for the IMD-warning-gated HPF Bypass
    // on PureSignal feedback toggle.  Lets SetupDialog wire this directly to
    // the PureSignal coordinator without reaching down through three layers
    // of setting key strings.
    connect(m_alex1Tab, &AntennaAlexAlex1Tab::hpfBypassOnPsChanged,
            this,       &AntennaAlexTab::hpfBypassOnPsChanged);

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

    // Show the BPF1 group, with the five HPF / 6 m LNA switches, in place of
    // the Alex HPF group when the Core programs the board's receive filter
    // through BPF1 (codec::alex::usesBpf1Preselector, the selector
    // computeRxPreselector uses): OrionMKII, Saturn, SaturnMKII, HermesC10.
    // From Thetis console.cs:6827-6837 [v2.10.3.15] (setAlex1HPF):
    //   if ((HardwareSpecific.Hardware == HPSDRHW.OrionMKII) || (HardwareSpecific.Hardware == HPSDRHW.Saturn)
    //      || (HardwareSpecific.Hardware == HPSDRHW.HermesC10))  //N1GP G2E added (HermesC10) //DK1HLM
    //   { setBPF1ForOrionIISaturn(freq); } else { setAlexHPF(freq); }
    // From Thetis setup.cs:6336-6360 [v2.10.3.15] (the panel list by model):
    //   HardwareSpecific.Model != HPSDRModel.ANAN_G2E && //N1GP G2E added
    //   HardwareSpecific.Model != HPSDRModel.REDPITAYA)//DH1KLM
    //   { panelBPFControl.Visible = false; panelAlex1HPFControl.Visible = true; ... }
    // Thetis's panel list matches the programmed bank for every model but the
    // plain ORIONMKII: it is on the OrionMKII board, so Thetis programs BPF1
    // yet shows the HPF panel, whose rows then do nothing. The tab shows the
    // rows that take effect, so the ORION MKII shows BPF1.
    const bool bpfPanel = codec::alex::usesBpf1Preselector(caps.board);
    m_alex1Tab->updateBoardCapabilities(bpfPanel);

    // Gate Alex-2 board status on caps.hasAlex2 (Phase 3P-I-b T8).
    // From Thetis setup.cs:6228-6264 [v2.10.3.13]: tpAlex2FilterControl
    // is visible only for BPF2-capable boards (ANAN7000D family +
    // OrionMKII + Saturn).
    //DH1KLM  [REDPITAYA-class SKU attribution in setup.cs:6256/6261]
    // G8NJJ. will need more work ofr high power PA
    //   [original inline comment from setup.cs:6244, on the ANAN_G2_1K branch]
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

// ── setTransmitPermitted (R-R3-46) ────────────────────────────────────────────

void AntennaAlexTab::setTransmitPermitted(bool permitted, const QString& reason)
{
    // Parity Task 12: Antenna Control's transmit half follows the Alex
    // facade's transmit edit availability instead. radioHardwareVersion 10:
    // the Alex-1 low-pass rows are no longer transmit-gated; they follow
    // setLpfRowsAvailable, as the high-pass rows follow
    // setHpfSwitchesAvailable.
    Q_UNUSED(permitted);
    Q_UNUSED(reason);
}

void AntennaAlexTab::setHpfSwitchesAvailable(bool available, const QString& reason)
{
    m_alex1Tab->setHpfSwitchesAvailable(available, reason);
}

void AntennaAlexTab::setHpfRowsAvailable(bool available, const QString& reason)
{
    m_alex1Tab->setHpfRowsAvailable(available, reason);
    m_alex2FiltersTab->setHpfRowsAvailable(available, reason);
}

void AntennaAlexTab::setLpfRowsAvailable(bool available, const QString& reason)
{
    m_alex1Tab->setLpfRowsAvailable(available, reason);
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
