#pragma once

// =================================================================
// src/gui/setup/hardware/AntennaAlexTab.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.designer.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-04-20 — Refactored into parent QTabWidget hosting three sub-sub-tabs:
//                 Antenna Control (existing content), Alex-1 Filters (Task 8),
//                 Alex-2 Filters (placeholder for Task 9). J.J. Boyd (KG4VCF).
//   2026-04-20 — Replaced Alex-2 Filters placeholder with real AntennaAlexAlex2Tab
//                 (Task 9). J.J. Boyd (KG4VCF).
//   2026-09-23 - R-R3-46: forwards the transmit permission to Antenna
//                 Control. J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude
//                 Code.
//   2026-09-26 - R-R3-46 / R-R3-49 (parity Task 14): forwards the Alex-1
//                high-pass switches' availability. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-49: the Alex Filters tabs' receive filter rows
//                (per-row bypass and edges, Alex-2 master bypass) select the
//                receive high-pass as Thetis's setAlexHPF /
//                setBPF1ForOrionIISaturn / setAlex2HPF do (radioHardwareVersion
//                8). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-46 / R-R3-49: the Alex-1 Filters tab's low-pass rows
//                and 6m/ByPass on RX select the low-pass as Thetis's
//                setAlexLPF does (radioHardwareVersion 10). J.J. Boyd
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

#include <QVariant>
#include <QWidget>

class QTabWidget;

namespace NereusSDR {

class RadioModel;
struct RadioInfo;
struct BoardCapabilities;
class AntennaAlexAntennaControlTab;
class AntennaAlexAlex1Tab;
class AntennaAlexAlex2Tab;

// AntennaAlexTab — parent "Antenna / ALEX" tab in Hardware Config.
//
// Hosts three sub-sub-tabs that mirror Thetis tcAlexControl:
//   0. Antenna Control — per-band TX/RX/RX-only antenna selection + Block-TX safety
//      (Thetis tpAlexAntCtrl; Phase 3P-F Task 3 — AntennaAlexAntennaControlTab)
//   1. Alex-1 Filters  — HPF + LPF + Saturn BPF1 band-edge editors (Task 8)
//   2. Alex-2 Filters  — RX2 board HPF + LPF panels with LED status stubs (Task 9)
//
// Source: Thetis tcAlexControl (setup.designer.cs:23385-23395) [@501e3f5]
class AntennaAlexTab : public QWidget {
    Q_OBJECT
public:
    explicit AntennaAlexTab(RadioModel* model, QWidget* parent = nullptr);
    void populate(const RadioInfo& info, const BoardCapabilities& caps);
    void restoreSettings(const QMap<QString, QVariant>& settings);

    // R-R3-46: the Alex-1 Filters tab's transmit fields follow the transmit
    // permission with its reason. Always permitted locally. (Antenna
    // Control's transmit half follows the Alex facade, parity Task 12.)
    void setTransmitPermitted(bool permitted, const QString& reason);
    // R-R3-46 / R-R3-49 (parity Task 14): see
    // AntennaAlexAlex1Tab::setHpfSwitchesAvailable.
    void setHpfSwitchesAvailable(bool available, const QString& reason);
    // radioHardwareVersion 8: the Alex-1 and Alex-2 Filters tabs' receive
    // filter rows (each row's Bypass, Start and End, and Alex-2's master
    // bypass) follow whether the Core takes them, disabled with `reason`
    // when it does not. Always available locally.
    void setHpfRowsAvailable(bool available, const QString& reason);
    // radioHardwareVersion 10: see AntennaAlexAlex1Tab::setLpfRowsAvailable.
    void setLpfRowsAvailable(bool available, const QString& reason);

signals:
    void settingChanged(const QString& key, const QVariant& value);

    // Phase 3M-4 Task 11: pass-through for AntennaAlexAlex1Tab::hpfBypassOnPsChanged.
    // RadioModel/SetupDialog routes this to PureSignal::setHpfBypassOnPs (NereusSDR-
    // original setter that mirrors Thetis console.DisableHPFonPS path).
    // From Thetis setup.cs:29274-29292 [v2.10.3.13].
    void hpfBypassOnPsChanged(bool checked);

private:
    RadioModel*   m_model{nullptr};

    // Sub-sub-tab host
    // Source: Thetis tcAlexControl (setup.designer.cs:23385-23395) [@501e3f5]
    QTabWidget*   m_subTabs{nullptr};

    // Tab 0: Antenna Control (real impl — Phase 3P-F Task 3)
    // Source: Thetis tpAlexAntCtrl (setup.designer.cs:5981-7000) [@501e3f5]
    AntennaAlexAntennaControlTab* m_antennaControlTab{nullptr};

    // Tab 1: Alex-1 Filters
    // Source: Thetis tpAlexFilterControl (setup.designer.cs:23399-25538) [@501e3f5]
    AntennaAlexAlex1Tab* m_alex1Tab{nullptr};

    // Tab 2: Alex-2 Filters
    // Source: Thetis tpAlex2FilterControl (setup.designer.cs:25539-26857) [@501e3f5]
    AntennaAlexAlex2Tab* m_alex2FiltersTab{nullptr};

    // Last seen MAC — passed to sub-tabs in populate() for per-MAC settings restore.
    QString m_lastMac;
};

} // namespace NereusSDR
