#pragma once

// =================================================================
// src/gui/setup/hardware/AntennaAlexAlex1Tab.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.designer.cs (~lines 23385-25538, tpAlexFilterControl)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Sub-sub-tab under Hardware → Antenna/ALEX.
//                Saturn BPF1 panel auto-hides on non-Saturn boards.
// =================================================================

//=================================================================
// setup.designer.cs
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
//
// === Verbatim Thetis Console/setup.designer.cs header (lines 1-50) ===
// namespace Thetis { using System.Windows.Forms; partial class Setup {
//   #region Windows Form Designer generated code
//   private void InitializeComponent() {
//     this.components = new System.ComponentModel.Container();
//     System.Windows.Forms.TabPage tpAlexAntCtrl;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS3;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS4;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS6;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS9;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS10;
//     System.Windows.Forms.NumericUpDownTS numericUpDownTS12;
//     System.ComponentModel.ComponentResourceManager resources = ...;
//     this.chkForceATTwhenOutPowerChanges_decreased = new CheckBoxTS();
//     this.chkUndoAutoATTTx = new CheckBoxTS();
//     this.chkAutoATTTXPsOff = new CheckBoxTS();
//     this.lblTXattBand = new LabelTS();
//     this.chkForceATTwhenOutPowerChanges = new CheckBoxTS();
//     this.chkForceATTwhenPSAoff = new CheckBoxTS();
//     this.chkEnableXVTRHF = new CheckBoxTS();
//     this.chkBPF2Gnd = new CheckBoxTS();
//     this.chkDisableRXOut = new CheckBoxTS();
//     this.chkEXT2OutOnTx = new CheckBoxTS();
//     this.chkEXT1OutOnTx = new CheckBoxTS();
//     this.labelATTOnTX = new LabelTS();
// =================================================================

#include <QWidget>
#include <vector>

class QCheckBox;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;

namespace NereusSDR {

class RadioModel;

// AntennaAlexAlex1Tab — "Alex-1 Filters" sub-sub-tab under Hardware → Antenna/ALEX.
//
// Three columns:
//   1. Alex HPF Bands — 5 master toggles + 6 HPF rows (bypass + Start/End)
//   2. Alex LPF Bands — 7 LPF rows (Start/End, no master toggles)
//   3. Saturn BPF1 Bands — same as HPF column, gated on Saturn/SaturnMKII board
//
// Per-MAC persistence under hardware/<mac>/alex/{hpf,lpf,bpf1}/<band>/{enabled,start,end}
// and hardware/<mac>/alex/master/{hpfBypass,...}.
//
// Source: Thetis Console/setup.designer.cs:23385-25538 (tpAlexFilterControl) [@501e3f5]
class AntennaAlexAlex1Tab : public QWidget {
    Q_OBJECT
public:
    explicit AntennaAlexAlex1Tab(RadioModel* model, QWidget* parent = nullptr);

    // Call when board capabilities are known. Shows/hides Saturn BPF1 column.
    // From Thetis: BPF1 panel always visible in Thetis; we gate on Saturn/SaturnMKII.
    void updateBoardCapabilities(bool isSaturnBoard);

    // Populate controls from AppSettings for the given MAC address.
    // Call when a radio is connected.
    void restoreSettings(const QString& macAddress);

    // Test seam — returns whether the Saturn BPF1 groupbox is visible.
    // Always compiled (NEREUS_BUILD_TESTS is set on NereusSDRObjs globally). Used by
    // tst_alex1_filters_tab to verify the Saturn/non-Saturn capability gate.
    bool isSaturnBpf1Visible() const;

signals:
    void settingChanged(const QString& key, const QVariant& value);

private slots:
    void onHpfCheckChanged(bool checked, const QString& settingsKey);
    void onHpfSpinChanged(double value, const QString& settingsKey);
    void onLpfSpinChanged(double value, const QString& settingsKey);
    void onBpf1CheckChanged(bool checked, const QString& settingsKey);
    void onBpf1SpinChanged(double value, const QString& settingsKey);
    void onMasterCheckChanged(bool checked, const QString& settingsKey);

private:
    // Band descriptor for HPF/BPF1 row construction.
    // From Thetis panelAlex1HPFControl (setup.designer.cs:23640-24420) [@501e3f5]
    struct HpfBandEntry {
        const char* label;      // e.g. "1.5 MHz HPF"
        const char* slug;       // e.g. "1_5MHz" (AppSettings key fragment)
        double      startMhz;   // Thetis default start
        double      endMhz;     // Thetis default end
    };

    // Band descriptor for LPF row construction.
    // From Thetis tpAlexFilterControl LPF controls (setup.designer.cs:23414-23435) [@501e3f5]
    struct LpfBandEntry {
        const char* label;      // e.g. "160m"
        const char* slug;       // e.g. "160m"
        double      startMhz;   // Thetis default start
        double      endMhz;     // Thetis default end
    };

    // Per-band widget set (HPF and BPF1 rows).
    struct HpfRowWidgets {
        QCheckBox*      bypass{nullptr};
        QDoubleSpinBox* start{nullptr};
        QDoubleSpinBox* end{nullptr};
    };

    // Per-band widget set (LPF rows — no bypass checkbox).
    struct LpfRowWidgets {
        QDoubleSpinBox* start{nullptr};
        QDoubleSpinBox* end{nullptr};
    };

    void buildHpfColumn(QGroupBox* box, const QString& settingsPrefix,
                        const std::vector<HpfBandEntry>& bands,
                        std::vector<HpfRowWidgets>& rows);

    void buildLpfColumn(QGroupBox* box,
                        const std::vector<LpfBandEntry>& bands,
                        std::vector<LpfRowWidgets>& rows);

    static QDoubleSpinBox* makeFreqSpin(double defaultMhz, QWidget* parent);

    RadioModel* m_model{nullptr};
    QString     m_currentMac;

    // Column 1 — Alex HPF master toggles
    // From Thetis panelAlex1HPFControl (setup.designer.cs:23635-24420) [@501e3f5]
    QCheckBox* m_hpfBypass{nullptr};           // chkAlexHPFBypass  [@501e3f5:24354]
    QCheckBox* m_hpfBypassOnTx{nullptr};       // chkDisableHPFonTX [@501e3f5:23727]
    QCheckBox* m_hpfBypassOnPs{nullptr};       // chkDisableHPFonPSb [@501e3f5:23673]
    QCheckBox* m_disable6mLnaOnTx{nullptr};    // chkDisable6mLNAonTX [@501e3f5:23700]
    QCheckBox* m_disable6mLnaOnRx{nullptr};    // chkDisable6mLNAonRX [@501e3f5:23714]

    // Column 1 — Alex HPF rows (6 rows: 1.5/6.5/9.5/13/20MHz + 6m bypass)
    std::vector<HpfRowWidgets> m_hpfRows;

    // Column 2 — Alex LPF rows (7 rows: 160m/80m/40m/20m/15m/10m/6m)
    std::vector<LpfRowWidgets> m_lpfRows;

    // Column 3 — Saturn BPF1 rows (same 6-row shape as HPF)
    QGroupBox*                 m_bpf1Group{nullptr};
    std::vector<HpfRowWidgets> m_bpf1Rows;

    // Static band tables
    static const std::vector<HpfBandEntry>& hpfBands();
    static const std::vector<LpfBandEntry>& lpfBands();
};

} // namespace NereusSDR
