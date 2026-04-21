#pragma once

// =================================================================
// src/gui/setup/hardware/AntennaAlexAlex2Tab.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/setup.designer.cs (~lines 25539-26857, tpAlex2FilterControl)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-20 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                (KG4VCF), with AI-assisted transformation via Anthropic
//                Claude Code. Sub-sub-tab under Hardware → Antenna/ALEX.
//                Auto-hides on boards without Alex-2 (HL2, Hermes,
//                Angelia). Live LED indicators are stubs; Phase H wires
//                them to ep6 status feed.
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

// AntennaAlexAlex2Tab — "Alex-2 Filters" sub-sub-tab under Hardware → Antenna/ALEX.
//
// Two columns:
//   1. Alex-2 HPF Bands — master bypass toggle + 6 HPF rows (bypass + Start/End)
//      + 7 LED status indicators (STUB — Phase H wires them)
//   2. Alex-2 LPF Bands — 7 LPF rows (Start/End, no master toggles)
//      + 7 LED status indicators (STUB — Phase H wires them)
//
// Per-MAC persistence under hardware/<mac>/alex2/{hpf,lpf}/<band>/{enabled,start,end}
// and hardware/<mac>/alex2/master/bypass55MhzBpf.
//
// Board gate: OrionMKII, Saturn, SaturnMKII boards have Alex-2 capability;
//   HL2/Hermes/HermesII/Angelia do not.  Call updateBoardCapabilities() after
//   board capabilities are known.
// TODO: add hasAlex2 cap field to BoardCapabilities in a future phase.
//
// Source: Thetis Console/setup.designer.cs:25539-26857 (tpAlex2FilterControl) [@501e3f5]
class AntennaAlexAlex2Tab : public QWidget {
    Q_OBJECT
public:
    explicit AntennaAlexAlex2Tab(RadioModel* model, QWidget* parent = nullptr);

    // Call when board capabilities are known. Shows status and optionally hides tab.
    // From Thetis: tpAlex2FilterControl visible/enabled gated on board model.
    // Gate: OrionMKII / Saturn / SaturnMKII → hasAlex2 = true.
    void updateBoardCapabilities(bool hasAlex2);

    // Populate controls from AppSettings for the given MAC address.
    // Call when a radio is connected.
    void restoreSettings(const QString& macAddress);

    // Test seam — returns whether the "Active" status is showing (hasAlex2=true).
    // Always compiled (NEREUS_BUILD_TESTS is set on NereusSDRObjs globally).
    bool isAlex2Active() const;

signals:
    void settingChanged(const QString& key, const QVariant& value);

private slots:
    void onHpfCheckChanged(bool checked, const QString& settingsKey);
    void onHpfSpinChanged(double value, const QString& settingsKey);
    void onLpfSpinChanged(double value, const QString& settingsKey);
    void onMasterCheckChanged(bool checked, const QString& settingsKey);

private:
    // Band descriptor for HPF row construction.
    // From Thetis tpAlex2FilterControl (setup.designer.cs:25539-26857) [@501e3f5]
    struct HpfBandEntry {
        const char* label;      // e.g. "1.5 MHz HPF"
        const char* slug;       // e.g. "1_5MHz" (AppSettings key fragment)
        double      startMhz;   // Thetis default start
        double      endMhz;     // Thetis default end
    };

    // Band descriptor for LPF row construction.
    // From Thetis tpAlex2FilterControl LPF controls (setup.designer.cs:25539-26000) [@501e3f5]
    struct LpfBandEntry {
        const char* label;      // e.g. "160m"
        const char* slug;       // e.g. "160m"
        double      startMhz;   // Thetis default start
        double      endMhz;     // Thetis default end
    };

    // Per-band widget set (HPF rows).
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

    static QDoubleSpinBox* makeFreqSpin(double defaultMhz, QWidget* parent);

    RadioModel* m_model{nullptr};
    QString     m_currentMac;

    // Top status bar — shows board detection.
    // From Thetis lblAlex2Active (setup.designer.cs:25583-25594) [@501e3f5]
    QLabel*    m_statusLabel{nullptr};
    QLabel*    m_statusLed{nullptr};
    bool       m_hasAlex2{false};

    // Column 1 — Alex-2 HPF master toggle
    // From Thetis chkAlex2HPFBypass (setup.designer.cs:26474-26488) [@501e3f5]
    // Text: "ByPass/55 MHz BPF"
    QCheckBox* m_hpfBypass55{nullptr};

    // Column 1 — Alex-2 HPF rows (6 rows)
    std::vector<HpfRowWidgets> m_hpfRows;

    // Column 2 — Alex-2 LPF rows (7 rows)
    std::vector<LpfRowWidgets> m_lpfRows;

    // Static band tables
    static const std::vector<HpfBandEntry>& hpfBands();
    static const std::vector<LpfBandEntry>& lpfBands();
};

} // namespace NereusSDR
