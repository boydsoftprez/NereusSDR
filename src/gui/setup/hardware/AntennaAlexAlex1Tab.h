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
//   2026-09-24 - R-R3-46: transmit permission for the TX low-pass table
//                and TX master switches. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-26 - R-R3-46 / R-R3-49 (remote-window parity Task 14): HPF
//                bypass on TX, HPF bypass on PureSignal and Disable 6 m LNA
//                on TX follow whether the Core takes them (radioHardwareVersion
//                7), with no on-air rule in either window, as Thetis sets them
//                with no MOX check. J.J. Boyd (KG4VCF), AI-assisted via
//                Anthropic Claude Code.
//   2026-09-28 - R-R3-46 / R-R3-49: the Alex Filters tabs' receive filter rows
//                (per-row bypass and edges, Alex-2 master bypass) select the
//                receive high-pass as Thetis's setAlexHPF /
//                setBPF1ForOrionIISaturn / setAlex2HPF do (radioHardwareVersion
//                8). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - updateBoardCapabilities swaps Alex HPF Bands for Saturn
//                BPF1 Bands with the five switches (Thetis setup.cs:6336-6360);
//                isAlexHpfVisible() for the tests. J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-46 / R-R3-49: the Alex-1 Filters tab's low-pass rows
//                and 6m/ByPass on RX select the low-pass as Thetis's
//                setAlexLPF does (radioHardwareVersion 10). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
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
class QFrame;
class QGroupBox;
class QLabel;
class QVBoxLayout;

namespace NereusSDR {

class RadioModel;

// AntennaAlexAlex1Tab — "Alex-1 Filters" sub-sub-tab under Hardware → Antenna/ALEX.
//
// Three columns:
//   1. Alex HPF Bands — 5 master toggles + 6 HPF rows (bypass + Start/End)
//   2. Alex LPF Bands — 7 LPF rows (Start/End, no master toggles)
//   3. Saturn BPF1 Bands — same as HPF column; on Thetis's BPF-panel models it
//      replaces column 1 and takes the 5 master toggles (updateBoardCapabilities)
//
// Per-MAC persistence under hardware/<mac>/alex/{hpf,lpf,bpf1}/<band>/{enabled,start,end}
// and hardware/<mac>/alex/master/{hpfBypass,...}.
//
// Source: Thetis Console/setup.designer.cs:23385-25538 (tpAlexFilterControl) [@501e3f5]
class AntennaAlexAlex1Tab : public QWidget {
    Q_OBJECT
public:
    explicit AntennaAlexAlex1Tab(RadioModel* model, QWidget* parent = nullptr);

    // Call when the board is known. bpfPanel (codec::alex::
    // usesBpf1Preselector, the bank the Core programs) shows Saturn BPF1 Bands in place of Alex HPF Bands and moves the five
    // switches into it; false does the reverse.
    // From Thetis setup.cs:6336-6360 [v2.10.3.15]:
    //   HardwareSpecific.Model != HPSDRModel.ANAN_G2E && //N1GP G2E added
    //   HardwareSpecific.Model != HPSDRModel.REDPITAYA)//DH1KLM
    void updateBoardCapabilities(bool bpfPanel);

    // Populate controls from AppSettings for the given MAC address.
    // Call when a radio is connected.
    void restoreSettings(const QString& macAddress);

    // R-R3-46 / R-R3-49 (parity Task 14): the three TX master switches (HPF
    // bypass on TX and on PureSignal feedback, 6 m LNA off on TX) follow
    // whether the Core takes them (radioHardwareVersion 7), disabled with
    // `reason` when it does not. Always available locally. No on-air rule:
    // Thetis's setters apply them at once with no MOX check
    // (console.cs:18719-18803 [v2.10.3.15]).
    void setHpfSwitchesAvailable(bool available, const QString& reason);

    // radioHardwareVersion 8: the HPF and BPF1 rows (Bypass, Start, End)
    // follow whether the Core takes them, disabled with `reason` when it
    // does not. Always available locally. No on-air rule: Thetis's per-row
    // setters apply at once with no MOX check (console.cs:18823-19040
    // [v2.10.3.15]).
    void setHpfRowsAvailable(bool available, const QString& reason);

    // radioHardwareVersion 10: the low-pass rows (Start, End) and
    // 6m/ByPass on RX follow whether the Core takes them, disabled with
    // `reason` when it does not. Always available locally. No on-air rule:
    // Thetis's spinner and check box handlers have no MOX check
    // (setup.cs:15888-15994, 18832-18835 [v2.10.3.15]). A receive-only
    // Core takes them too, so they do not follow the transmit permission.
    void setLpfRowsAvailable(bool available, const QString& reason);

    // The reason 6m/ByPass on RX is disabled on the radios Thetis hides it
    // on (codec::alex::lpfBypassAvailable).
    static QString lpfBypassNotOnThisRadioReason();

    // Test seam — returns whether the Saturn BPF1 groupbox is visible.
    // Always compiled (NEREUS_BUILD_TESTS is set on NereusSDRLib globally). Used by
    // tst_alex1_filters_tab to verify the Saturn/non-Saturn capability gate.
    bool isSaturnBpf1Visible() const;
    // True when the Alex HPF Bands group is shown (not on BPF-panel models).
    bool isAlexHpfVisible() const;

    // Live-LED test seams — mirrors AntennaAlexAlex2Tab.
    // Returns the index of the currently-highlighted HPF / LPF row, or -1
    // when no row matches. HPF index 5 (the 6m-bypass row) doubles as the
    // Thetis setAlexHPF fallback when master bypass is engaged or no band
    // row matches.
    int  activeHpfLedForTest() const { return m_activeHpfIndex; }
    int  activeLpfLedForTest() const { return m_activeLpfIndex; }

    // Drives the LED selection from a frequency (Hz). Exposed for unit
    // tests and called internally on SliceModel::frequencyChanged.
    // Source: Thetis console.cs:setAlexHPF / setAlexLPF range match [@501e3f5]
    void setCurrentFrequencyHz(double freqHz);

signals:
    void settingChanged(const QString& key, const QVariant& value);

    // Phase 3M-4 Task 11: HPF Bypass on PureSignal feedback toggle.
    // Emitted whenever m_hpfBypassOnPs changes after the IMD warning gate
    // resolves (or instantly when re-checking).  RadioModel routes this to
    // the live PureSignal feedback path so the BPF/HPF safety state takes
    // effect without requiring a Setup dialog close.
    // From Thetis setup.cs:29274-29292 [v2.10.3.13] (chkDisableHPFonPS_CheckedChanged
    // — setter cascades to console.DisableHPFonPS).
    void hpfBypassOnPsChanged(bool checked);

private slots:
    void onHpfCheckChanged(bool checked, const QString& settingsKey);
    void onHpfSpinChanged(double value, const QString& settingsKey);
    void onLpfSpinChanged(double value, const QString& settingsKey);
    void onLpfBypassChanged(bool checked);
    void onBpf1CheckChanged(bool checked, const QString& settingsKey);
    void onBpf1SpinChanged(double value, const QString& settingsKey);
    void onMasterCheckChanged(bool checked, const QString& settingsKey);

public:
    // Test seam — controls whether the IMD warning modal dialog is suppressed.
    // When enabled, the handler treats every confirmation as "OK" (i.e. the
    // toggle is allowed to land unchecked) without invoking QMessageBox::exec().
    // tst_setup_deltas uses the cancel-emulation seam below to verify the
    // revert path; the OK-emulation default exercises the persist+emit path.
    enum class TestImdResult { ConfirmOk, ConfirmCancel };
    void setImdWarningResultForTest(TestImdResult result);

    // The IMD warning the tab shows before HPF Bypass on PureSignal feedback
    // is cleared. The Setup description carries the same text as that row's
    // `confirm` (tst_setup_description_parity compares them).
    static QString imdWarningText();

    // Test seam — bypass-the-dialog wire for the m_hpfBypassOnPs checkbox.
    // Drives QCheckBox::toggled exactly as the user would.  Tests must call
    // setImdWarningResultForTest() first to seed the auto-confirm path.
    QCheckBox* hpfBypassOnPsCheckboxForTest() const { return m_hpfBypassOnPs; }

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

    // Per-band widget set (LPF rows; 6m/ByPass on RX is one box for all).
    struct LpfRowWidgets {
        QDoubleSpinBox* start{nullptr};
        QDoubleSpinBox* end{nullptr};
    };

    void buildHpfColumn(QGroupBox* box, const QString& settingsPrefix,
                        const std::vector<HpfBandEntry>& bands,
                        std::vector<HpfRowWidgets>& rows);

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
    QCheckBox* m_lpfBypass{nullptr};           // chkLPFBypass [v2.10.3.15:23484]

    // The low-pass gates: the Core takes the rows (setLpfRowsAvailable)
    // and the radio has 6m/ByPass on RX (restoreSettings).
    bool    m_lpfRowsAvailable{true};
    QString m_lpfRowsReason;
    bool    m_lpfBypassOnThisRadio{true};
    void applyLpfGates();

    // Thetis's udAlex<band>LPFStart/End_ValueChanged: keep the rows
    // contiguous by moving the neighbouring edge (setup.cs:15888-15994
    // [v2.10.3.15]). `row` indexes lpfBands().
    void adjustLpfNeighbours(std::size_t row, bool isStart);

    // Column 3 — Saturn BPF1 rows (same 6-row shape as HPF)
    QGroupBox*                 m_bpf1Group{nullptr};
    QGroupBox*                 m_hpfGroup{nullptr};
    QVBoxLayout*               m_hpfVBox{nullptr};
    QVBoxLayout*               m_bpf1VBox{nullptr};
    std::vector<HpfRowWidgets> m_bpf1Rows;

    // Live-LED indicators, one per HPF/LPF row (same ordering as
    // hpfBands() / lpfBands()). Added 2026-04-21 for UX parity with
    // Alex-2. BPF1 rows intentionally get no LED — that panel is
    // Saturn-only and is wired by the same codec path as Alex-1 HPF.
    std::vector<QFrame*> m_hpfLeds;
    std::vector<QFrame*> m_lpfLeds;

    // Latest-known RX frequency (Hz). Used by updateActiveLeds() and
    // also when a spinbox mutates so range-edits re-highlight without
    // waiting for the next frequencyChanged emission.
    double m_currentFreqHz{0.0};

    // Cached indices of currently-highlighted rows (-1 = none lit).
    int m_activeHpfIndex{-1};
    int m_activeLpfIndex{-1};

    // Phase 3M-4 Task 11: IMD warning dialog auto-result for tests.
    // When std::nullopt the handler shows a real QMessageBox::exec() modal.
    // When set, the handler skips the modal and treats it as the indicated
    // outcome (ConfirmOk = proceed unchecked; ConfirmCancel = revert).
    enum class ImdAutoResult { None, Ok, Cancel };
    ImdAutoResult m_imdAutoResult{ImdAutoResult::None};

    // Recompute LED highlight from m_currentFreqHz and current spinbox
    // values. Source: Thetis console.cs:setAlexHPF / setAlexLPF —
    // master-bypass fallback → per-band bypass → first-match range [@501e3f5]
    void updateActiveLeds();

    // Restyle an LED frame lit / unlit.
    static void setLedLit(QFrame* led, bool lit);

    // Static band tables
    static const std::vector<HpfBandEntry>& hpfBands();
    static const std::vector<LpfBandEntry>& lpfBands();

    // Saturn / Orion MkII BPF1 band rows.  Separate from hpfBands() because
    // the MkII boards carry a BAND-PASS bank on the same relay bits, with
    // entirely different crossovers (see AlexFilterMap).
    static const std::vector<HpfBandEntry>& bpf1Bands();
};

} // namespace NereusSDR
