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

class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QScrollArea;
class QTabWidget;
class QTableWidget;
class QTableWidgetItem;

namespace NereusSDR {

class RadioModel;
struct RadioInfo;
struct BoardCapabilities;
class AntennaAlexAlex1Tab;
class AntennaAlexAlex2Tab;

// AntennaAlexTab — parent "Antenna / ALEX" tab in Hardware Config.
//
// Hosts three sub-sub-tabs that mirror Thetis tcAlexControl:
//   0. Antenna Control — per-band RX/TX antenna selection + relay options
//      (Thetis tpAlexAntCtrl; Phase F will add proper antenna routing here)
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

signals:
    void settingChanged(const QString& key, const QVariant& value);

private slots:
    void onRxAntTableChanged(QTableWidgetItem* item);
    void onTxAntTableChanged(QTableWidgetItem* item);

private:
    void buildAntennaTable(QTableWidget* table,
                           const QStringList& colHeaders,
                           const QString& signalKeyPrefix);

    RadioModel*   m_model{nullptr};

    // Sub-sub-tab host
    // Source: Thetis tcAlexControl (setup.designer.cs:23385-23395) [@501e3f5]
    QTabWidget*          m_subTabs{nullptr};

    // Tab 0: Antenna Control
    QWidget*             m_antennaControlTab{nullptr};

    // Per-band RX antenna: 14 rows × 3 columns (ANT1 / ANT2 / ANT3)
    // Each cell is a radio-button-like exclusive selection stored via
    // Qt::CheckStateRole. We use QTableWidget + exclusive logic in itemChanged.
    QTableWidget* m_rxAntTable{nullptr};

    // Per-band TX antenna: same shape
    QTableWidget* m_txAntTable{nullptr};

    // ALEX bypass / relay checkboxes
    // Source: Thetis Setup.cs:2892-2898
    QCheckBox*    m_rxOutOnTx{nullptr};
    QCheckBox*    m_ext1OutOnTx{nullptr};
    QCheckBox*    m_ext2OutOnTx{nullptr};
    QCheckBox*    m_hfTrRelay{nullptr};
    QCheckBox*    m_bpf2Gnd{nullptr};
    QCheckBox*    m_enableXvtrHf{nullptr};

    // Tab 1: Alex-1 Filters
    // Source: Thetis tpAlexFilterControl (setup.designer.cs:23399-25538) [@501e3f5]
    AntennaAlexAlex1Tab* m_alex1Tab{nullptr};

    // Tab 2: Alex-2 Filters
    // Source: Thetis tpAlex2FilterControl (setup.designer.cs:25539-26857) [@501e3f5]
    AntennaAlexAlex2Tab* m_alex2FiltersTab{nullptr};

    bool m_updating{false};

    // Last seen capabilities — needed to route populate() to sub-tabs
    QString m_lastMac;
};

} // namespace NereusSDR
