// =================================================================
// src/gui/setup/hardware/AntennaAlexAlex1Tab.cpp  (NereusSDR)
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
//   2026-09-25 - R-R3-49 (remote-window parity Task 13, plan C5): the
//                 LPF band edges are hidden until built (UnbuiltFeatures);
//                 HPF bypass on TX and on PureSignal and Disable 6 m LNA on
//                 TX stay shown, since plan Task 14 applies them. J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
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
//   2026-09-29 - R-R3-49 / R-IOS-18: the receive filter rows carry their
//                Setup description ids (version 13). J.J. Boyd (KG4VCF),
//                AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-49 / R-IOS-18: the five switches above the rows carry
//                their Setup description ids (version 13). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - The IMD warning text moves to imdWarningText() so the Setup
//                description can carry it. J.J. Boyd (KG4VCF), AI-assisted
//                via Anthropic Claude Code.
//   2026-09-29 - The BPF1 column shows on the ANAN-7000DLE / 8000DLE too
//                (comments only here; the gate is in AntennaAlexTab).
//                J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - updateBoardCapabilities swaps the Alex HPF group for the
//                BPF1 group and moves the five switches with it, as Thetis
//                setup.cs:6336-6360 and the per-model cases do. J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-09-29 - R-R3-46 / R-R3-49: the Alex-1 Filters tab's low-pass rows
//                and 6m/ByPass on RX select the low-pass as Thetis's
//                setAlexLPF does (radioHardwareVersion 10). J.J. Boyd
//                (KG4VCF), AI-assisted via Anthropic Claude Code.
// =================================================================
//
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
//   private void InitializeComponent() {
//     this.components = new System.ComponentModel.Container();
//     System.Windows.Forms.TabPage tpAlexAntCtrl;
//     ...
//     this.chkForceATTwhenOutPowerChanges_decreased = new CheckBoxTS();
//     this.chkEnableXVTRHF = new CheckBoxTS();
//     this.labelATTOnTX = new LabelTS();
// =================================================================

#include "AntennaAlexAlex1Tab.h"
#include "HardwareTransmitGate.h"

#include "core/AlexSettingsKeys.h"
#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "core/codec/AlexFilterMap.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

namespace NereusSDR {

// ── Band table helpers ────────────────────────────────────────────────────────

// Alex HPF band rows — 6 entries.
// Source: Thetis panelAlex1HPFControl (setup.designer.cs:23640-24420) [@501e3f5]
// Defaults derived from NumericUpDownTS.Value initialisation:
//   1.5 MHz HPF start: {18,0,0,65536} = 1.8 MHz; end: {6499999,0,0,393216} = 6.499999 MHz
//   6.5 MHz HPF start: {65,0,0,65536} = 6.5 MHz;  end: {9499999,0,0,393216} = 9.499999 MHz
//   9.5 MHz HPF start: {95,0,0,65536} = 9.5 MHz;  end: {12999999,0,0,393216} = 12.999999 MHz
//   13 MHz  HPF start: {13,0,0,0} = 13.0 MHz;    end: {19999999,0,0,393216} = 19.999999 MHz
//   20 MHz  HPF start: {20,0,0,0} = 20.0 MHz;    end: {49999999,0,0,393216} = 49.999999 MHz
//   6m Bypass   start: {50,0,0,0} = 50.0 MHz;    end: {6144,0,0,131072} = 61.44 MHz
const std::vector<AntennaAlexAlex1Tab::HpfBandEntry>& AntennaAlexAlex1Tab::hpfBands()
{
    static const std::vector<HpfBandEntry> bands = {
        { "1.5 MHz HPF",  alexKeys::kPreselector1_5MHz,   1.8,      6.499999 },  // udAlex1_5HPF*  [@501e3f5:23784-23825]
        { "6.5 MHz HPF",  alexKeys::kPreselector6_5MHz,   6.5,      9.499999 },  // udAlex6_5HPF*  [@501e3f5:23866-23907]
        { "9.5 MHz HPF",  alexKeys::kPreselector9_5MHz,   9.5,     12.999999 },  // udAlex9_5HPF*  [@501e3f5:23948-23989]
        { "13 MHz HPF",   alexKeys::kPreselector13MHz,   13.0,     19.999999 },  // udAlex13HPF*   [@501e3f5:24243-24049]
        { "20 MHz HPF",   alexKeys::kPreselector20MHz,   20.0,     49.999999 },  // udAlex20HPF*   [@501e3f5:24079-24019]
        { "6m Bypass",    alexKeys::kPreselector6mBP,    50.0,     61.44     },  // udAlex6BPF*    [@501e3f5:24272-24340]
    };
    return bands;
}

// Saturn / Orion MkII BPF1 band rows, 6 entries.
//
// The MkII boards (ANAN-7000DLE / 8000DLE / Anvelina Pro 3 / ANAN-G2 /
// ANAN-G2-1K / ANAN-G2E) put a BAND-PASS bank on the same relay bits the
// ANAN-100/200 boards use for the high-pass ladder above, so these rows need
// their own edges and their own labels.  This table used to be hpfBands(),
// which showed a G2 owner "1.5 MHz HPF / 1.8 - 6.499999" for what is really
// the 160m band-pass at 1.5 - 2.1 MHz.
//
// Source: Thetis panelBPF1Control BPF1 spinboxes, read through the BPF1_*
// getters at setup.cs:5193-5251 [v2.10.3.15].  Defaults derived from
// NumericUpDownTS.Value initialisation in setup.designer.cs [v2.10.3.15]:
//   160m   BPF start: {15,0,0,65536}       = 1.5 MHz;  end: {2099999,0,0,393216}  =  2.099999 MHz
//   80/60m BPF start: {21,0,0,65536}       = 2.1 MHz;  end: {5499999,0,0,393216}  =  5.499999 MHz
//   40/30m BPF start: {55,0,0,65536}       = 5.5 MHz;  end: {10999999,0,0,393216} = 10.999999 MHz
//   20/15m BPF start: {11,0,0,0}           = 11.0 MHz; end: {21999999,0,0,393216} = 21.999999 MHz
//   12/10m BPF start: {22,0,0,0}           = 22.0 MHz; end: {34999999,0,0,393216} = 34.999999 MHz
//   6m BPF/LNA start: {35,0,0,0}           = 35.0 MHz; end: {6144,0,0,131072}     = 61.44 MHz
//
// The slugs deliberately match hpfBands() so that BPF1 edges already persisted
// under hardware/<mac>/alex/bpf1/<slug>/{start,end} keep loading; only the
// labels and the shipped defaults change.  Both tables take the slugs from
// core/AlexSettingsKeys.h, which is also what SettingsHygiene sweeps.  The two
// sides carried independent spellings until 2026-07-25 and never matched.
const std::vector<AntennaAlexAlex1Tab::HpfBandEntry>& AntennaAlexAlex1Tab::bpf1Bands()
{
    static const std::vector<HpfBandEntry> bands = {
        { "160m BPF",     alexKeys::kPreselector1_5MHz,   1.5,      2.099999  },  // ud1_5BPF1*  [v2.10.3.15:24982-25023]
        { "80/60m BPF",   alexKeys::kPreselector6_5MHz,   2.1,      5.499999  },  // ud6_5BPF1*  [v2.10.3.15:25064-25105]
        { "40/30m BPF",   alexKeys::kPreselector9_5MHz,   5.5,     10.999999  },  // ud9_5BPF1*  [v2.10.3.15:25146-25187]
        { "20/17/15m BPF",alexKeys::kPreselector13MHz,   11.0,     21.999999  },  // ud13BPF1*   [v2.10.3.15:25440-25247]
        { "12/10m BPF",   alexKeys::kPreselector20MHz,   22.0,     34.999999  },  // ud20BPF1*   [v2.10.3.15:25277-25217]
        { "6m BPF/LNA",   alexKeys::kPreselector6mBP,    35.0,     61.44      },  // ud6BPF1*    [v2.10.3.15:25481-25522]
    };
    return bands;
}

// Alex LPF band rows — 7 entries.
// Source: Thetis tpAlexFilterControl LPF spinboxes (setup.designer.cs:23414-23435) [@501e3f5]
// Defaults:
//   160m: {0,0,0,0}=0.0 / {25,0,0,65536}=2.5
//   80m:  {2500001,0,0,393216}=2.500001 / {5,0,0,0}=5.0
//   40m:  {5000001,0,0,393216}=5.000001 / {8,0,0,0}=8.0
//   20m:  {8000001,0,0,393216}=8.000001 / {165,0,0,65536}=16.5
//   15m:  {16500001,0,0,393216}=16.500001 / {24000000,0,0,393216}=24.0
//   10m:  {24000001,0,0,393216}=24.000001 / {356,0,0,65536}=35.6
//   6m:   {35600001,0,0,393216}=35.600001 / {6144,0,0,131072}=61.44
const std::vector<AntennaAlexAlex1Tab::LpfBandEntry>& AntennaAlexAlex1Tab::lpfBands()
{
    static const std::vector<LpfBandEntry> bands = {
        { "160m",    "160m",   0.0,        2.5      },  // udAlex160mLPF* [@501e3f5:24803-24833]
        { "80m",     "80m",    2.500001,   5.0      },  // udAlex80mLPF*  [@501e3f5:24743-24773]
        { "60/40m",  "40m",    5.000001,   8.0      },  // udAlex40mLPF*  [@501e3f5:24683-24713]
        { "30/20m",  "20m",    8.000001,  16.5      },  // udAlex20mLPF*  [@501e3f5:24563-24623]
        { "17/15m",  "15m",   16.500001,  24.0      },  // udAlex15mLPF*  [@501e3f5:24593-24653]
        { "12/10m",  "10m",   24.000001,  35.6      },  // udAlex10mLPF*  [@501e3f5:24474-24444]
        { "6m",      "6m",    35.600001,  61.44     },  // udAlex6mLPF*   [@501e3f5:24533-24504]
    };
    return bands;
}

// ── makeFreqSpin ──────────────────────────────────────────────────────────────

// Builds a QDoubleSpinBox for MHz frequency entry with 6 decimal places.
// Range 0.0–200.0, step 0.001, 6 decimal places — mirrors Thetis NumericUpDownTS
// DecimalPlaces=6 / Increment=~0.001 (set via flags field 196608 = scale 3).
// Source: udAlex*HPFStart DecimalPlaces=6 (setup.designer.cs:23763) [@501e3f5]
QDoubleSpinBox* AntennaAlexAlex1Tab::makeFreqSpin(double defaultMhz, QWidget* parent)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(0.0, 200.0);
    spin->setDecimals(6);
    spin->setSingleStep(0.001);
    spin->setSuffix(QStringLiteral(" MHz"));
    spin->setValue(defaultMhz);
    return spin;
}

// ── buildHpfColumn ────────────────────────────────────────────────────────────

// Builds the grid of HPF band rows (Bypass | Start | End) inside box.
// Same shape is reused for Saturn BPF1 column.
// settingsPrefix is e.g. "alex/hpf" or "alex/bpf1".
// From Thetis panelAlex1HPFControl (setup.designer.cs:23640-24420) [@501e3f5]
void AntennaAlexAlex1Tab::buildHpfColumn(QGroupBox* box,
                                         const QString& settingsPrefix,
                                         const std::vector<HpfBandEntry>& bands,
                                         std::vector<HpfRowWidgets>& rows)
{
    auto* formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setHorizontalSpacing(6);
    formLayout->setVerticalSpacing(4);

    // Column headers
    auto* headerRow = new QHBoxLayout();
    auto* lblBypass = new QLabel(tr("Bypass"), box);
    auto* lblStart  = new QLabel(tr("Start"), box);
    auto* lblEnd    = new QLabel(tr("End"), box);
    lblBypass->setAlignment(Qt::AlignCenter);
    lblStart->setAlignment(Qt::AlignCenter);
    lblEnd->setAlignment(Qt::AlignCenter);
    headerRow->addWidget(lblBypass);
    headerRow->addWidget(lblStart);
    headerRow->addWidget(lblEnd);
    formLayout->addRow(QStringLiteral(""), new QWidget(box));  // spacer label

    rows.clear();
    rows.reserve(bands.size());

    for (const HpfBandEntry& band : bands) {
        HpfRowWidgets w;
        w.bypass = new QCheckBox(box);
        w.start  = makeFreqSpin(band.startMhz, box);
        w.end    = makeFreqSpin(band.endMhz, box);

        auto* rowWidget = new QWidget(box);
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);
        rowLayout->addWidget(w.bypass);
        rowLayout->addWidget(w.start);
        rowLayout->addWidget(w.end);

        formLayout->addRow(tr(band.label), rowWidget);

        const QString slug = QString::fromLatin1(band.slug);

        // Wire bypass checkbox
        const QString enabledKey = QStringLiteral("%1/%2/enabled").arg(settingsPrefix, slug);
        connect(w.bypass, &QCheckBox::toggled, this,
                [this, enabledKey](bool checked) { onHpfCheckChanged(checked, enabledKey); });

        // Wire start spinbox
        const QString startKey = QStringLiteral("%1/%2/start").arg(settingsPrefix, slug);
        connect(w.start, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, startKey](double v) { onHpfSpinChanged(v, startKey); });

        // Wire end spinbox
        const QString endKey = QStringLiteral("%1/%2/end").arg(settingsPrefix, slug);
        connect(w.end, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, endKey](double v) { onHpfSpinChanged(v, endKey); });

        rows.push_back(w);
    }

    auto* boxLayout = qobject_cast<QVBoxLayout*>(box->layout());
    if (!boxLayout) {
        boxLayout = new QVBoxLayout(box);
    }
    // Re-add column-header row then form
    auto* headerWidget = new QWidget(box);
    auto* hw = new QHBoxLayout(headerWidget);
    hw->setContentsMargins(0, 0, 0, 0);
    // (header is embedded in form — just add form directly)
    boxLayout->addLayout(formLayout);
    boxLayout->addStretch();
}

// ── Constructor ───────────────────────────────────────────────────────────────

AntennaAlexAlex1Tab::AntennaAlexAlex1Tab(RadioModel* model, QWidget* parent)
    : QWidget(parent), m_model(model)
{
    // Outer layout: scroll area + horizontal column layout.
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);

    auto* outerVBox = new QVBoxLayout(this);
    outerVBox->setContentsMargins(0, 0, 0, 0);
    outerVBox->addWidget(scroll);

    auto* colLayout = new QHBoxLayout(content);
    colLayout->setContentsMargins(8, 8, 8, 8);
    colLayout->setSpacing(12);

    // ── Column 1: Alex HPF Bands ──────────────────────────────────────────────
    // Source: Thetis panelAlex1HPFControl (setup.designer.cs:23635-24420) [@501e3f5]
    auto* hpfGroup = new QGroupBox(tr("Alex HPF Bands"), content);
    m_hpfGroup = hpfGroup;
    auto* hpfVBox  = new QVBoxLayout(hpfGroup);
    m_hpfVBox = hpfVBox;
    hpfVBox->setContentsMargins(8, 8, 8, 8);
    hpfVBox->setSpacing(4);

    // Master toggles — Source: setup.designer.cs:23635-23727 [@501e3f5]
    // chkAlexHPFBypass:    "ByPass/55 MHz HPF" [@501e3f5:24354]
    // chkDisableHPFonTX:   "HPF ByPass on TX"  [@501e3f5:23727]
    // chkDisableHPFonPSb:  "HPF ByPass on PS"  [@501e3f5:23673] — default Checked
    // chkDisable6mLNAonTX: "TX" — default Checked                [@501e3f5:23700]
    // chkDisable6mLNAonRX: "RX"                                  [@501e3f5:23714]
    m_hpfBypass        = new QCheckBox(tr("HPF Bypass (master)"), hpfGroup);
    m_hpfBypassOnTx    = new QCheckBox(tr("HPF Bypass on TX"), hpfGroup);
    m_hpfBypassOnPs    = new QCheckBox(tr("HPF Bypass on PureSignal feedback"), hpfGroup);
    m_disable6mLnaOnTx = new QCheckBox(tr("Disable 6m LNA on TX"), hpfGroup);
    m_disable6mLnaOnRx = new QCheckBox(tr("Disable 6m LNA on RX"), hpfGroup);

    // Thetis defaults: chkDisableHPFonPSb.Checked = true [@501e3f5:23676]
    //                  chkDisable6mLNAonTX.Checked = true [@501e3f5:23703]
    m_hpfBypassOnPs->setChecked(true);
    m_disable6mLnaOnTx->setChecked(true);

    for (QCheckBox* chk : { m_hpfBypass, m_hpfBypassOnTx, m_hpfBypassOnPs,
                             m_disable6mLnaOnTx, m_disable6mLnaOnRx }) {
        hpfVBox->addWidget(chk);
    }
    // R-R3-49 (remote-window parity Task 13): named for the tests. Plan
    // Task 14 applies these three (RadioModel::applyAlexHpfSwitchSettings),
    // so they are shown; only the LPF band edges below wait (plan C5).
    m_hpfBypassOnTx->setObjectName(QStringLiteral("alexHpfBypassOnTx"));
    m_hpfBypassOnPs->setObjectName(QStringLiteral("alexHpfBypassOnPs"));
    m_disable6mLnaOnTx->setObjectName(QStringLiteral("alexDisable6mLnaOnTx"));
    // Setup description version 13: the same five switches on the phone.
    m_hpfBypass->setProperty("nereusSetupId", QStringLiteral("hardware.alex1Filters.hpfBypass"));
    m_hpfBypassOnTx->setProperty("nereusSetupId",
                                 QStringLiteral("hardware.alex1Filters.hpfBypassOnTx"));
    m_hpfBypassOnPs->setProperty("nereusSetupId",
                                 QStringLiteral("hardware.alex1Filters.hpfBypassOnPs"));
    m_disable6mLnaOnTx->setProperty("nereusSetupId",
                                    QStringLiteral("hardware.alex1Filters.disable6mLnaOnTx"));
    m_disable6mLnaOnRx->setProperty("nereusSetupId",
                                    QStringLiteral("hardware.alex1Filters.disable6mLnaOnRx"));

    auto wireMaster = [this](QCheckBox* chk, const QString& key) {
        connect(chk, &QCheckBox::toggled, this,
                [this, key](bool checked) { onMasterCheckChanged(checked, key); });
    };
    wireMaster(m_hpfBypass,        QStringLiteral("alex/master/hpfBypass"));
    wireMaster(m_hpfBypassOnTx,    QStringLiteral("alex/master/hpfBypassOnTx"));
    wireMaster(m_disable6mLnaOnTx, QStringLiteral("alex/master/disable6mLnaOnTx"));
    wireMaster(m_disable6mLnaOnRx, QStringLiteral("alex/master/disable6mLnaOnRx"));

    // Phase 3M-4 Task 11: m_hpfBypassOnPs gets its own toggled handler that
    // shows an IMD warning dialog when un-checked (i.e. the user is choosing
    // to INCLUDE the BPFs during a PureSignal feedback path).  Cancel reverts
    // the toggle.
    // From Thetis setup.cs:29274-29292 [v2.10.3.13] (chkDisableHPFonPS_CheckedChanged).
    connect(m_hpfBypassOnPs, &QCheckBox::toggled, this, [this](bool checked) {
        if (!checked) {
            // From Thetis setup.cs:29278-29286 [v2.10.3.13]: warning dialog.
            // Multi-paragraph text reproduced below preserves Thetis spelling
            // ONLY in the source-cite reference.  User-visible text uses
            // the corrected spelling ("transmission", "including").
            // Thetis uses MessageBoxButtons.OKCancel with Button2 (Cancel)
            // as the default focus and Common.MB_TOPMOST flag.
            QMessageBox::StandardButton choice = QMessageBox::Cancel;
            if (m_imdAutoResult == ImdAutoResult::None) {
                QMessageBox box(QMessageBox::Warning,
                    tr("PureSignal Issue"),
                    imdWarningText(),
                    QMessageBox::Ok | QMessageBox::Cancel, this);
                box.setDefaultButton(QMessageBox::Cancel);          // Button2 per Thetis
                box.setWindowFlag(Qt::WindowStaysOnTopHint);        // MB_TOPMOST per Thetis
                choice = static_cast<QMessageBox::StandardButton>(box.exec());
            } else {
                choice = (m_imdAutoResult == ImdAutoResult::Ok)
                             ? QMessageBox::Ok
                             : QMessageBox::Cancel;
            }
            if (choice == QMessageBox::Cancel) {
                QSignalBlocker block(m_hpfBypassOnPs);
                m_hpfBypassOnPs->setChecked(true);
                return;
            }
        }
        // Persist via the master-check pattern + emit the live PureSignal hook.
        onMasterCheckChanged(checked, QStringLiteral("alex/master/hpfBypassOnPs"));
        emit hpfBypassOnPsChanged(checked);
    });

    // HPF band rows
    auto* hpfFormWidget = new QWidget(hpfGroup);
    auto* hpfFormLayout = new QFormLayout(hpfFormWidget);
    hpfFormLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hpfFormLayout->setHorizontalSpacing(6);
    hpfFormLayout->setVerticalSpacing(4);

    m_hpfRows.reserve(hpfBands().size());
    m_hpfLeds.reserve(hpfBands().size());
    for (const HpfBandEntry& band : hpfBands()) {
        HpfRowWidgets w;
        w.bypass = new QCheckBox(hpfFormWidget);
        w.start  = makeFreqSpin(band.startMhz, hpfFormWidget);
        w.end    = makeFreqSpin(band.endMhz,   hpfFormWidget);

        // Live-filter-active LED — lit when the current RX frequency
        // falls in this row's [start, end] range.
        auto* led = new QFrame(hpfFormWidget);
        led->setFixedSize(12, 12);
        setLedLit(led, false);
        m_hpfLeds.push_back(led);

        auto* rowWidget = new QWidget(hpfFormWidget);
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);
        rowLayout->addWidget(led);
        rowLayout->addWidget(w.bypass);
        rowLayout->addWidget(w.start);
        rowLayout->addWidget(w.end);

        hpfFormLayout->addRow(tr(band.label), rowWidget);

        const QString slug = QString::fromLatin1(band.slug);
        // radioHardwareVersion 8: named for the tests and the remote gate.
        w.bypass->setObjectName(QStringLiteral("alexHpfBypass_%1").arg(slug));
        w.start->setObjectName(QStringLiteral("alexHpfStart_%1").arg(slug));
        w.end->setObjectName(QStringLiteral("alexHpfEnd_%1").arg(slug));
        // Setup description version 13: the same rows on the phone.
        w.bypass->setProperty("nereusSetupId",
                              QStringLiteral("hardware.alex1Filters.hpf.%1.bypass").arg(slug));
        w.start->setProperty("nereusSetupId",
                             QStringLiteral("hardware.alex1Filters.hpf.%1.start").arg(slug));
        w.end->setProperty("nereusSetupId",
                           QStringLiteral("hardware.alex1Filters.hpf.%1.end").arg(slug));
        const QString enabledKey = QStringLiteral("alex/hpf/%1/enabled").arg(slug);
        const QString startKey   = QStringLiteral("alex/hpf/%1/start").arg(slug);
        const QString endKey     = QStringLiteral("alex/hpf/%1/end").arg(slug);

        connect(w.bypass, &QCheckBox::toggled, this,
                [this, enabledKey](bool checked) { onHpfCheckChanged(checked, enabledKey); });
        connect(w.start, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, startKey](double v) { onHpfSpinChanged(v, startKey); });
        connect(w.end, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, endKey](double v) { onHpfSpinChanged(v, endKey); });

        m_hpfRows.push_back(w);
    }

    hpfVBox->addWidget(hpfFormWidget);
    hpfVBox->addStretch();
    colLayout->addWidget(hpfGroup);

    // ── Column 2: Alex LPF Bands ──────────────────────────────────────────────
    // Source: Thetis tpAlexFilterControl LPF controls (setup.designer.cs:23414-23435) [@501e3f5]
    // Note: "TX-side filters always engaged when keyed". The one switch is
    // chkLPFBypass, 6m/ByPass on RX (setup.designer.cs:23484-23495
    // [v2.10.3.15]), which selects the 6 m low-pass while not keyed.
    auto* lpfGroup = new QGroupBox(tr("Alex LPF Bands"), content);
    auto* lpfVBox  = new QVBoxLayout(lpfGroup);
    lpfVBox->setContentsMargins(8, 8, 8, 8);
    lpfVBox->setSpacing(4);

    auto* lpfNote = new QLabel(
        QStringLiteral("<i>TX-side filters always engaged when keyed</i>"), lpfGroup);
    lpfNote->setWordWrap(true);
    lpfVBox->addWidget(lpfNote);

    auto* lpfFormWidget = new QWidget(lpfGroup);
    auto* lpfFormLayout = new QFormLayout(lpfFormWidget);
    lpfFormLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    lpfFormLayout->setHorizontalSpacing(6);
    lpfFormLayout->setVerticalSpacing(4);

    m_lpfRows.reserve(lpfBands().size());
    m_lpfLeds.reserve(lpfBands().size());
    for (const LpfBandEntry& band : lpfBands()) {
        LpfRowWidgets w;
        w.start = makeFreqSpin(band.startMhz, lpfFormWidget);
        w.end   = makeFreqSpin(band.endMhz,   lpfFormWidget);
        {
            // Each edge's own range, the spinner's Minimum / Maximum
            // (setup.designer.cs [v2.10.3.15], kAlexLpfEdgeLimits).
            const auto& lim = codec::alex::kAlexLpfEdgeLimits[m_lpfRows.size()];
            w.start->setRange(lim.startMin, lim.startMax);
            w.end->setRange(lim.endMin, lim.endMax);
            w.start->setValue(band.startMhz);
            w.end->setValue(band.endMhz);
        }
        // radioHardwareVersion 10: the band edges select the low-pass
        // (RadioModel::savedAlexLpfEdges → RadioConnection::setAlexLpfEdges,
        // as Thetis's setAlexLPF reads udAlex<band>LPFStart/End).
        w.start->setObjectName(QStringLiteral("alexLpfStart_%1").arg(QLatin1String(band.slug)));
        w.end->setObjectName(QStringLiteral("alexLpfEnd_%1").arg(QLatin1String(band.slug)));
        // Setup description version 17: the same rows on the phone.
        w.start->setProperty("nereusSetupId",
            QStringLiteral("hardware.alex1Filters.lpf.%1.start").arg(QLatin1String(band.slug)));
        w.end->setProperty("nereusSetupId",
            QStringLiteral("hardware.alex1Filters.lpf.%1.end").arg(QLatin1String(band.slug)));

        auto* led = new QFrame(lpfFormWidget);
        led->setFixedSize(12, 12);
        setLedLit(led, false);
        m_lpfLeds.push_back(led);

        auto* rowWidget = new QWidget(lpfFormWidget);
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);
        rowLayout->addWidget(led);
        rowLayout->addWidget(w.start);
        rowLayout->addWidget(w.end);

        lpfFormLayout->addRow(tr(band.label), rowWidget);

        const QString slug = QString::fromLatin1(band.slug);
        const QString startKey = QStringLiteral("alex/lpf/%1/start").arg(slug);
        const QString endKey   = QStringLiteral("alex/lpf/%1/end").arg(slug);

        const std::size_t row = m_lpfRows.size();
        connect(w.start, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, startKey, row](double v) {
                    onLpfSpinChanged(v, startKey);
                    adjustLpfNeighbours(row, /*isStart=*/true);
                });
        connect(w.end, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, endKey, row](double v) {
                    onLpfSpinChanged(v, endKey);
                    adjustLpfNeighbours(row, /*isStart=*/false);
                });

        m_lpfRows.push_back(w);
    }

    lpfVBox->addWidget(lpfFormWidget);

    // From Thetis setup.designer.cs:23484-23495 [v2.10.3.15] (chkLPFBypass)
    //   this.chkLPFBypass.Text = "6m/ByPass on RX";
    //   "Selects the 6m LPF during receive reguardless of frequency."
    // The tooltip's spelling is corrected here.
    m_lpfBypass = new QCheckBox(tr("6m/ByPass on RX"), lpfGroup);
    m_lpfBypass->setObjectName(QStringLiteral("alexLpfBypass"));
    m_lpfBypass->setToolTip(tr("Selects the 6m LPF during receive regardless of frequency."));
    m_lpfBypass->setProperty("nereusSetupId", QStringLiteral("hardware.alex1Filters.lpfBypass"));
    lpfVBox->addWidget(m_lpfBypass);
    connect(m_lpfBypass, &QCheckBox::toggled, this, &AntennaAlexAlex1Tab::onLpfBypassChanged);
    lpfVBox->addStretch();
    colLayout->addWidget(lpfGroup);

    // ── Column 3: Saturn BPF1 Bands ───────────────────────────────────────────
    // Source: spec §7; same band-edge shape as Alex HPF.
    // Shown in place of the Alex HPF group on the boards the Core programs
    // through BPF1 (usesBpf1Preselector); AntennaAlexTab::populate sets it.
    m_bpf1Group = new QGroupBox(tr("Saturn BPF1 Bands"), content);
    auto* bpf1VBox = new QVBoxLayout(m_bpf1Group);
    m_bpf1VBox = bpf1VBox;
    bpf1VBox->setContentsMargins(8, 8, 8, 8);
    bpf1VBox->setSpacing(4);

    auto* bpf1Note = new QLabel(
        QStringLiteral("<i>Separate BPF1 board edges (G8NJJ Saturn) — "
                       "fall back to Alex defaults if unset</i>"),
        m_bpf1Group);
    bpf1Note->setWordWrap(true);
    bpf1VBox->addWidget(bpf1Note);

    auto* bpf1FormWidget = new QWidget(m_bpf1Group);
    auto* bpf1FormLayout = new QFormLayout(bpf1FormWidget);
    bpf1FormLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    bpf1FormLayout->setHorizontalSpacing(6);
    bpf1FormLayout->setVerticalSpacing(4);

    m_bpf1Rows.reserve(bpf1Bands().size());
    for (const HpfBandEntry& band : bpf1Bands()) {
        HpfRowWidgets w;
        w.bypass = new QCheckBox(bpf1FormWidget);
        w.start  = makeFreqSpin(band.startMhz, bpf1FormWidget);
        w.end    = makeFreqSpin(band.endMhz,   bpf1FormWidget);

        auto* rowWidget = new QWidget(bpf1FormWidget);
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);
        rowLayout->addWidget(w.bypass);
        rowLayout->addWidget(w.start);
        rowLayout->addWidget(w.end);

        bpf1FormLayout->addRow(tr(band.label), rowWidget);

        const QString slug = QString::fromLatin1(band.slug);
        w.bypass->setObjectName(QStringLiteral("alexBpf1Bypass_%1").arg(slug));
        w.start->setObjectName(QStringLiteral("alexBpf1Start_%1").arg(slug));
        w.end->setObjectName(QStringLiteral("alexBpf1End_%1").arg(slug));
        w.bypass->setProperty("nereusSetupId",
                              QStringLiteral("hardware.alex1Filters.bpf1.%1.bypass").arg(slug));
        w.start->setProperty("nereusSetupId",
                             QStringLiteral("hardware.alex1Filters.bpf1.%1.start").arg(slug));
        w.end->setProperty("nereusSetupId",
                           QStringLiteral("hardware.alex1Filters.bpf1.%1.end").arg(slug));
        const QString enabledKey = QStringLiteral("alex/bpf1/%1/enabled").arg(slug);
        const QString startKey   = QStringLiteral("alex/bpf1/%1/start").arg(slug);
        const QString endKey     = QStringLiteral("alex/bpf1/%1/end").arg(slug);

        connect(w.bypass, &QCheckBox::toggled, this,
                [this, enabledKey](bool checked) { onBpf1CheckChanged(checked, enabledKey); });
        connect(w.start, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, startKey](double v) { onBpf1SpinChanged(v, startKey); });
        connect(w.end, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, endKey](double v) { onBpf1SpinChanged(v, endKey); });

        m_bpf1Rows.push_back(w);
    }

    bpf1VBox->addWidget(bpf1FormWidget);
    bpf1VBox->addStretch();
    colLayout->addWidget(m_bpf1Group);

    // Start hidden — caller must call updateBoardCapabilities()
    m_bpf1Group->setVisible(false);

    // Match stretch so columns share width equally
    colLayout->setStretchFactor(hpfGroup, 1);
    colLayout->setStretchFactor(lpfGroup, 1);
    colLayout->setStretchFactor(m_bpf1Group, 1);

    // ── Live LED driver ───────────────────────────────────────────────────
    // Subscribe to SliceModel::frequencyChanged on every current + future
    // slice so the LED tracks the active RX frequency. Thetis's Alex HPF /
    // LPF selection switches on the active RX VFO. Because slices can be
    // added after this tab is constructed (addSlice() fires on connect),
    // subscribe to sliceAdded too and attach a fresh subscription as new
    // slices arrive.
    // Source: Thetis console.cs:setAlexHPF / setAlexLPF on RX VFO change [@501e3f5]
    if (m_model) {
        auto subscribeToSlice = [this](SliceModel* slice) {
            if (!slice) { return; }
            m_currentFreqHz = slice->frequency();
            connect(slice, &SliceModel::frequencyChanged,
                    this, &AntennaAlexAlex1Tab::setCurrentFrequencyHz);
        };
        for (SliceModel* slice : m_model->slices()) {
            subscribeToSlice(slice);
        }
        connect(m_model, &RadioModel::sliceAdded, this,
                [this, subscribeToSlice](int sliceId) {
                    subscribeToSlice(m_model->sliceById(sliceId));
                });
        // The low-pass lamp follows the bits the radio has in use.
        connect(m_model, &RadioModel::alexLpfBitsChanged, this,
                [this]() { updateActiveLeds(); });
    }

    // Recompute when the master bypass or any per-row bypass toggles so
    // edits update the LED without a frequency change.
    connect(m_hpfBypass, &QCheckBox::toggled, this,
            [this](bool) { updateActiveLeds(); });
    for (HpfRowWidgets& row : m_hpfRows) {
        if (row.bypass) {
            connect(row.bypass, &QCheckBox::toggled, this,
                    [this](bool) { updateActiveLeds(); });
        }
        if (row.start) {
            connect(row.start,
                    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this](double) { updateActiveLeds(); });
        }
        if (row.end) {
            connect(row.end,
                    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this](double) { updateActiveLeds(); });
        }
    }
    for (LpfRowWidgets& row : m_lpfRows) {
        if (row.start) {
            connect(row.start,
                    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this](double) { updateActiveLeds(); });
        }
        if (row.end) {
            connect(row.end,
                    QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this](double) { updateActiveLeds(); });
        }
    }

    // Belt-and-suspenders: poll slice 0 frequency every 250 ms and
    // refresh the LED. Keeps the indicator alive even if the
    // SliceModel::frequencyChanged subscription misses events (e.g.
    // slice object swapped, or tuning via a code path that bypasses
    // SliceModel::setFrequency's change-notification guard).
    // Negligible cost — one double read + range comparison per tick.
    auto* pollTimer = new QTimer(this);
    pollTimer->setInterval(250);
    connect(pollTimer, &QTimer::timeout, this, [this]() {
        if (!m_model) { return; }
        const auto slices = m_model->slices();
        if (slices.isEmpty()) { return; }
        const double hz = slices.first()->frequency();
        if (!qFuzzyCompare(m_currentFreqHz, hz)) {
            m_currentFreqHz = hz;
            updateActiveLeds();
        }
    });
    pollTimer->start();

    // Initial paint.
    updateActiveLeds();
}

// ── Live LED implementation ───────────────────────────────────────────────────

void AntennaAlexAlex1Tab::setCurrentFrequencyHz(double freqHz)
{
    m_currentFreqHz = freqHz;
    updateActiveLeds();
}

// Clear all LEDs then light the single HPF + single LPF row whose
// [start, end] range contains the current frequency. HPF has two
// fallbacks: master-bypass engaged → light the 6m-bypass row (last);
// per-row bypass engaged on the matching row → also promote to the 6m-
// bypass LED. Mirrors Thetis console.cs:setAlexHPF / setAlexLPF [@501e3f5].
void AntennaAlexAlex1Tab::updateActiveLeds()
{
    if (m_hpfLeds.empty() || m_lpfLeds.empty()) { return; }

    for (QFrame* led : m_hpfLeds) { setLedLit(led, false); }
    for (QFrame* led : m_lpfLeds) { setLedLit(led, false); }

    const double freqMhz = m_currentFreqHz / 1.0e6;
    const int bypassRow = static_cast<int>(m_hpfLeds.size()) - 1;

    // HPF selection
    int hpfIdx = -1;
    const bool masterBypass = (m_hpfBypass && m_hpfBypass->isChecked());
    if (masterBypass) {
        hpfIdx = bypassRow;
    } else {
        for (std::size_t i = 0; i < m_hpfRows.size(); ++i) {
            const double startMhz =
                m_hpfRows[i].start ? m_hpfRows[i].start->value() : 0.0;
            const double endMhz =
                m_hpfRows[i].end   ? m_hpfRows[i].end->value()   : 0.0;
            if (freqMhz >= startMhz && freqMhz <= endMhz) {
                if (m_hpfRows[i].bypass && m_hpfRows[i].bypass->isChecked()) {
                    hpfIdx = bypassRow;
                } else {
                    hpfIdx = static_cast<int>(i);
                }
                break;
            }
        }
        if (hpfIdx == -1) {
            hpfIdx = bypassRow;
        }
    }

    // LPF selection. The lamp is the low-pass the radio has in use
    // (RadioModel::alexLpfBits, from the connection, or from the Core in a
    // remote window), as Thetis's setAlexLPF lights rad<band>LPFled with
    // the bits it writes (console.cs:7177-7243 [v2.10.3.15]). With no
    // radio, or a Core that does not send it, the tab's own rows and the
    // tuned frequency stand in, selected the same way.
    int lpfBits = m_model ? m_model->alexLpfBits() : -1;
    if (lpfBits < 0 && m_lpfRows.size() == codec::alex::kAlexLpfRowCount) {
        if (m_lpfBypass && m_lpfBypass->isChecked() && m_lpfBypass->isEnabled()) {
            lpfBits = 0x10;
        } else {
            codec::alex::AlexLpfEdges edges;
            for (std::size_t i = 0; i < m_lpfRows.size(); ++i) {
                edges.rows[i].startMhz = m_lpfRows[i].start ? m_lpfRows[i].start->value() : 0.0;
                edges.rows[i].endMhz   = m_lpfRows[i].end   ? m_lpfRows[i].end->value()   : 0.0;
            }
            lpfBits = codec::alex::selectAlexLpf(freqMhz, edges);
        }
    }
    int lpfIdx = -1;
    // Rows in lpfBands() order (160, 80, 40, 20, 15, 10, 6 m) and the bit
    // each one writes (console.cs:7177-7243 [v2.10.3.15]).
    static constexpr int kRowBits[] = {0x08, 0x04, 0x02, 0x01, 0x40, 0x20, 0x10};
    for (int i = 0; i < int(std::size(kRowBits)); ++i) {
        if (lpfBits == kRowBits[i]) {
            lpfIdx = i;
            break;
        }
    }

    if (hpfIdx >= 0 && static_cast<std::size_t>(hpfIdx) < m_hpfLeds.size()) {
        setLedLit(m_hpfLeds[hpfIdx], true);
    }
    if (lpfIdx >= 0 && static_cast<std::size_t>(lpfIdx) < m_lpfLeds.size()) {
        setLedLit(m_lpfLeds[lpfIdx], true);
    }

    m_activeHpfIndex = hpfIdx;
    m_activeLpfIndex = lpfIdx;
}

// Simple LED repaint. Lit = green dot, unlit = dark grey. Mirrors
// AntennaAlexAlex2Tab::setLedLit — no frame-shape or border so the
// stylesheet background applies cleanly across platforms.
void AntennaAlexAlex1Tab::setLedLit(QFrame* led, bool lit)
{
    if (!led) { return; }
    if (lit) {
        led->setStyleSheet(
            QStringLiteral("QFrame { background: #00cc44; border-radius: 5px; }"));
    } else {
        led->setStyleSheet(
            QStringLiteral("QFrame { background: #444444; border-radius: 5px; }"));
    }
}

// ── imdWarningText ────────────────────────────────────────────────────────────

// From Thetis setup.cs:29440-29449 [v2.10.3.15] (chkDisableHPFonPS_CheckedChanged):
// the warning shown before HPF Bypass on PureSignal feedback is cleared
// (Thetis's "tranmission" and "inlcuding" corrected in the user-visible text).
QString AntennaAlexAlex1Tab::imdWarningText()
{
    return tr("Including the BPFs during a PureSignal transmission may "
              "produce passive Inter-Modulation Distortion in the "
              "inductors of the bandpass filters.\n\n"
              "You will NOT be able to observe this degraded performance "
              "on the panadapter because PS is correcting to the distorted "
              "feedback and the panadapter is \"seeing\" that same "
              "distorted feedback. It can only be observed with an "
              "external spectrum analyzer.\n\n"
              "Please ensure you understand the implications of including "
              "the BPFs when transmitting a PureSignal based signal. "
              "It is not recommended.");
}

// ── updateBoardCapabilities ───────────────────────────────────────────────────

// Shows the BPF1 group in place of the Alex HPF group on the boards the Core
// programs through BPF1 (usesBpf1Preselector, decided in
// AntennaAlexTab::populate) and
// moves the five HPF / 6 m LNA switches into whichever group is shown.
// From Thetis setup.cs:6336-6360 [v2.10.3.15]: other models get
//   panelBPFControl.Visible = false; panelAlex1HPFControl.Visible = true;
//   and the switches reparented to panelAlex1HPFControl.
//   HardwareSpecific.Model != HPSDRModel.ANAN_G2E && //N1GP G2E added
//   HardwareSpecific.Model != HPSDRModel.REDPITAYA)//DH1KLM
// From Thetis setup.cs:20208-20220 [v2.10.3.15] (7000D; the other BPF-panel
//   cases match): panelAlex1HPFControl.Visible = false;
//   panelBPFControl.Visible = true; the switches reparented to panelBPFControl.
void AntennaAlexAlex1Tab::updateBoardCapabilities(bool bpfPanel)
{
    m_bpf1Group->setVisible(bpfPanel);
    m_hpfGroup->setVisible(!bpfPanel);

    QVBoxLayout* target = bpfPanel ? m_bpf1VBox : m_hpfVBox;
    QGroupBox* parent = bpfPanel ? m_bpf1Group : m_hpfGroup;
    // BPF1 keeps its note label first; the switches follow it.
    int at = bpfPanel ? 1 : 0;
    for (QCheckBox* chk : { m_hpfBypass, m_hpfBypassOnTx, m_hpfBypassOnPs,
                             m_disable6mLnaOnTx, m_disable6mLnaOnRx }) {
        if (chk->parentWidget() != parent) {
            const bool hidden = chk->isHidden();
            chk->setParent(parent);
            target->insertWidget(at, chk);
            chk->setHidden(hidden);
        }
        ++at;
    }
}

// ── restoreSettings ───────────────────────────────────────────────────────────

void AntennaAlexAlex1Tab::restoreSettings(const QString& macAddress)
{
    m_currentMac = macAddress;
    if (macAddress.isEmpty()) { return; }

    auto& settings = AppSettings::instance();

    // Restore HPF master toggles
    struct MasterEntry { const char* key; QCheckBox* widget; bool defaultVal; };
    const MasterEntry masterEntries[] = {
        { "alex/master/hpfBypass",         m_hpfBypass,        false },
        { "alex/master/hpfBypassOnTx",     m_hpfBypassOnTx,    false },
        { "alex/master/hpfBypassOnPs",     m_hpfBypassOnPs,    true  },  // Thetis default: true
        { "alex/master/disable6mLnaOnTx",  m_disable6mLnaOnTx, true  },  // Thetis default: true
        { "alex/master/disable6mLnaOnRx",  m_disable6mLnaOnRx, false },
    };
    for (const MasterEntry& e : masterEntries) {
        const QString defaultStr = e.defaultVal ? QStringLiteral("True") : QStringLiteral("False");
        const bool val = settings.hardwareValue(macAddress, QString::fromLatin1(e.key),
                                                defaultStr).toString() == QStringLiteral("True");
        QSignalBlocker blocker(e.widget);
        e.widget->setChecked(val);
    }

    // Restore HPF band rows
    const auto& hpf = hpfBands();
    for (std::size_t i = 0; i < m_hpfRows.size() && i < hpf.size(); ++i) {
        const QString slug = QString::fromLatin1(hpf[i].slug);
        {
            QSignalBlocker b(m_hpfRows[i].bypass);
            const bool v = settings.hardwareValue(macAddress,
                QStringLiteral("alex/hpf/%1/enabled").arg(slug), QStringLiteral("False"))
                .toString() == QStringLiteral("True");
            m_hpfRows[i].bypass->setChecked(v);
        }
        {
            QSignalBlocker b(m_hpfRows[i].start);
            const double v = settings.hardwareValue(macAddress,
                QStringLiteral("alex/hpf/%1/start").arg(slug),
                hpf[i].startMhz).toDouble();
            m_hpfRows[i].start->setValue(v);
        }
        {
            QSignalBlocker b(m_hpfRows[i].end);
            const double v = settings.hardwareValue(macAddress,
                QStringLiteral("alex/hpf/%1/end").arg(slug),
                hpf[i].endMhz).toDouble();
            m_hpfRows[i].end->setValue(v);
        }
    }

    // Restore LPF band rows
    const auto& lpf = lpfBands();
    for (std::size_t i = 0; i < m_lpfRows.size() && i < lpf.size(); ++i) {
        const QString slug = QString::fromLatin1(lpf[i].slug);
        {
            QSignalBlocker b(m_lpfRows[i].start);
            const double v = settings.hardwareValue(macAddress,
                QStringLiteral("alex/lpf/%1/start").arg(slug),
                lpf[i].startMhz).toDouble();
            m_lpfRows[i].start->setValue(codec::alex::clampAlexLpfEdge(
                static_cast<int>(i), /*isEnd=*/false, v, lpf[i].startMhz));
        }
        {
            QSignalBlocker b(m_lpfRows[i].end);
            const double v = settings.hardwareValue(macAddress,
                QStringLiteral("alex/lpf/%1/end").arg(slug),
                lpf[i].endMhz).toDouble();
            m_lpfRows[i].end->setValue(codec::alex::clampAlexLpfEdge(
                static_cast<int>(i), /*isEnd=*/true, v, lpf[i].endMhz));
        }
    }

    // 6m/ByPass on RX, unchecked and disabled on the radios Thetis hides
    // it on (codec::alex::lpfBypassAvailable, setup.cs:6190-6205
    // [v2.10.3.15]); the Core applies it off there too.
    m_lpfBypassOnThisRadio = !m_model
        || codec::alex::lpfBypassAvailable(m_model->hardwareProfile().model);
    {
        QSignalBlocker b(m_lpfBypass);
        const bool v = settings.hardwareValue(macAddress,
            QString::fromLatin1(alexKeys::kLpfBypass), QStringLiteral("False"))
            .toString() == QStringLiteral("True");
        m_lpfBypass->setChecked(v && m_lpfBypassOnThisRadio);
    }
    applyLpfGates();
    updateActiveLeds();

    // Restore BPF1 band rows.  Slugs deliberately match the HPF rows so that
    // edges persisted before bpf1Bands() existed still load; the defaults,
    // however, come from the BPF1 table, because the MkII band-pass bank has
    // entirely different crossovers to the legacy high-pass ladder.
    const auto& bpf1 = bpf1Bands();
    for (std::size_t i = 0; i < m_bpf1Rows.size() && i < bpf1.size(); ++i) {
        const QString slug = QString::fromLatin1(bpf1[i].slug);
        {
            QSignalBlocker b(m_bpf1Rows[i].bypass);
            const bool v = settings.hardwareValue(macAddress,
                QStringLiteral("alex/bpf1/%1/enabled").arg(slug), QStringLiteral("False"))
                .toString() == QStringLiteral("True");
            m_bpf1Rows[i].bypass->setChecked(v);
        }
        {
            QSignalBlocker b(m_bpf1Rows[i].start);
            const double v = settings.hardwareValue(macAddress,
                QStringLiteral("alex/bpf1/%1/start").arg(slug),
                bpf1[i].startMhz).toDouble();
            m_bpf1Rows[i].start->setValue(v);
        }
        {
            QSignalBlocker b(m_bpf1Rows[i].end);
            const double v = settings.hardwareValue(macAddress,
                QStringLiteral("alex/bpf1/%1/end").arg(slug),
                bpf1[i].endMhz).toDouble();
            m_bpf1Rows[i].end->setValue(v);
        }
    }
}

// ── Change slots ──────────────────────────────────────────────────────────────

void AntennaAlexAlex1Tab::onHpfCheckChanged(bool checked, const QString& settingsKey)
{
    if (!m_currentMac.isEmpty()) {
        AppSettings::instance().setHardwareValue(
            m_currentMac, settingsKey, checked ? QStringLiteral("True") : QStringLiteral("False"));
        AppSettings::instance().save();
    }
    // radioHardwareVersion 8: the receive filter rows reach the radio at
    // once, as Thetis's per-row bypass setters re-select the high-pass
    // (console.cs:18823-18833 [v2.10.3.15]). In a remote window the save
    // goes to the Core, which applies it (scheduleRemoteHardwareApply).
    if (m_model) {
        m_model->applyAlexHpfSwitchSettings();
    }
    emit settingChanged(settingsKey, checked);
}

void AntennaAlexAlex1Tab::onHpfSpinChanged(double value, const QString& settingsKey)
{
    if (!m_currentMac.isEmpty()) {
        AppSettings::instance().setHardwareValue(m_currentMac, settingsKey, value);
        AppSettings::instance().save();
    }
    // radioHardwareVersion 8: the receive filter rows reach the radio at
    // once, as Thetis's per-row bypass setters re-select the high-pass
    // (console.cs:18823-18833 [v2.10.3.15]). In a remote window the save
    // goes to the Core, which applies it (scheduleRemoteHardwareApply).
    if (m_model) {
        m_model->applyAlexHpfSwitchSettings();
    }
    emit settingChanged(settingsKey, value);
}

void AntennaAlexAlex1Tab::onLpfSpinChanged(double value, const QString& settingsKey)
{
    if (!m_currentMac.isEmpty()) {
        AppSettings::instance().setHardwareValue(m_currentMac, settingsKey, value);
        AppSettings::instance().save();
    }
    // radioHardwareVersion 10: the connection stores the edges for its next
    // low-pass selection; nothing is re-selected now, keyed or not, as
    // Thetis's spinner handlers only move the neighbouring edge and
    // setAlexLPF reads them when next called (setup.cs:15888-15994
    // [v2.10.3.15]). In a remote window the save goes to the Core.
    if (m_model) {
        m_model->applyAlexHpfSwitchSettings();
    }
    emit settingChanged(settingsKey, value);
}

// From Thetis setup.cs:15888-15994 [v2.10.3.15]
//   udAlex160mLPFStart: if (Start >= End + 0.000001) End = Start + 0.000001;
//   udAlex160mLPFEnd:   if (End <= Start) Start = End - 0.000001;
//                       else if (End >= udAlex80mLPFStart) 80m Start = End + 0.000001;
//   udAlex<b>LPFStart:  if (Start <= <prev>End) <prev>End = Start - 0.000001;
//   udAlex<b>LPFEnd:    if (End >= <next>Start) <next>Start = End + 0.000001;
//   (80, 40, 20, 15 and 10 m ends; udAlex6mLPFEnd has no handler.)
// The rule itself is codec::alex::alexLpfNeighbourMove, which the Core also
// runs on a write from any window or the phone. Setting the neighbour runs
// its own handler, which saves it and takes the next step, as Thetis's Value
// set raises its ValueChanged.
void AntennaAlexAlex1Tab::adjustLpfNeighbours(std::size_t row, bool isStart)
{
    if (row >= m_lpfRows.size()
        || m_lpfRows.size() != static_cast<std::size_t>(codec::alex::kAlexLpfRowCount)) {
        return;
    }
    codec::alex::AlexLpfRows rows{};
    for (std::size_t i = 0; i < m_lpfRows.size(); ++i) {
        if (!m_lpfRows[i].start || !m_lpfRows[i].end) {
            return;
        }
        rows[i] = {m_lpfRows[i].start->value(), m_lpfRows[i].end->value()};
    }
    const std::optional<codec::alex::AlexLpfEdgeMove> move =
        codec::alex::alexLpfNeighbourMove(rows, static_cast<int>(row), !isStart);
    if (!move) {
        return;
    }
    const LpfRowWidgets& target = m_lpfRows[static_cast<std::size_t>(move->row)];
    (move->isEnd ? target.end : target.start)->setValue(move->mhz);
}

// From Thetis setup.cs:18832-18835 [v2.10.3.15] (chkLPFBypass_CheckedChanged)
//   if (initializing) return;
//   console.LPFBypass = chkLPFBypass.Checked;
// The LPFBypass setter re-selects the low-pass at once (console.cs:
// 18775-18790 [v2.10.3.15]); RadioModel::applyAlexHpfSwitchSettings hands
// the saved value to the connection, which does the same. In a remote
// window the save goes to the Core, which applies it there.
void AntennaAlexAlex1Tab::onLpfBypassChanged(bool checked)
{
    const QString key = QString::fromLatin1(alexKeys::kLpfBypass);
    if (!m_currentMac.isEmpty()) {
        AppSettings::instance().setHardwareValue(
            m_currentMac, key, checked ? QStringLiteral("True") : QStringLiteral("False"));
        AppSettings::instance().save();
    }
    if (m_model) {
        m_model->applyAlexHpfSwitchSettings();
    }
    updateActiveLeds();
    emit settingChanged(key, checked);
}

void AntennaAlexAlex1Tab::onBpf1CheckChanged(bool checked, const QString& settingsKey)
{
    if (!m_currentMac.isEmpty()) {
        AppSettings::instance().setHardwareValue(
            m_currentMac, settingsKey, checked ? QStringLiteral("True") : QStringLiteral("False"));
        AppSettings::instance().save();
    }
    // radioHardwareVersion 8: the receive filter rows reach the radio at
    // once, as Thetis's per-row bypass setters re-select the high-pass
    // (console.cs:18823-18833 [v2.10.3.15]). In a remote window the save
    // goes to the Core, which applies it (scheduleRemoteHardwareApply).
    if (m_model) {
        m_model->applyAlexHpfSwitchSettings();
    }
    emit settingChanged(settingsKey, checked);
}

void AntennaAlexAlex1Tab::onBpf1SpinChanged(double value, const QString& settingsKey)
{
    if (!m_currentMac.isEmpty()) {
        AppSettings::instance().setHardwareValue(m_currentMac, settingsKey, value);
        AppSettings::instance().save();
    }
    // radioHardwareVersion 8: the receive filter rows reach the radio at
    // once, as Thetis's per-row bypass setters re-select the high-pass
    // (console.cs:18823-18833 [v2.10.3.15]). In a remote window the save
    // goes to the Core, which applies it (scheduleRemoteHardwareApply).
    if (m_model) {
        m_model->applyAlexHpfSwitchSettings();
    }
    emit settingChanged(settingsKey, value);
}

void AntennaAlexAlex1Tab::setHpfSwitchesAvailable(bool available, const QString& reason)
{
    for (QWidget* w : std::initializer_list<QWidget*>{
             m_hpfBypassOnTx, m_hpfBypassOnPs, m_disable6mLnaOnTx}) {
        HardwareTransmitGate::apply(w, available, reason);
    }
}

void AntennaAlexAlex1Tab::setHpfRowsAvailable(bool available, const QString& reason)
{
    for (const std::vector<HpfRowWidgets>* rows : {&m_hpfRows, &m_bpf1Rows}) {
        for (const HpfRowWidgets& row : *rows) {
            for (QWidget* w : std::initializer_list<QWidget*>{row.bypass, row.start, row.end}) {
                HardwareTransmitGate::apply(w, available, reason);
            }
        }
    }
}

void AntennaAlexAlex1Tab::setLpfRowsAvailable(bool available, const QString& reason)
{
    m_lpfRowsAvailable = available;
    m_lpfRowsReason = reason;
    applyLpfGates();
}

QString AntennaAlexAlex1Tab::lpfBypassNotOnThisRadioReason()
{
    return RadioModel::lpfBypassUnavailableReason();
}

void AntennaAlexAlex1Tab::applyLpfGates()
{
    for (const LpfRowWidgets& row : m_lpfRows) {
        HardwareTransmitGate::apply(row.start, m_lpfRowsAvailable, m_lpfRowsReason);
        HardwareTransmitGate::apply(row.end, m_lpfRowsAvailable, m_lpfRowsReason);
    }
    // Shown disabled with its reason where Thetis hides it (setup.cs:
    // 6190-6205 [v2.10.3.15]); the Core's own gate otherwise.
    if (!m_lpfBypassOnThisRadio) {
        HardwareTransmitGate::apply(m_lpfBypass, false, lpfBypassNotOnThisRadioReason());
    } else {
        HardwareTransmitGate::apply(m_lpfBypass, m_lpfRowsAvailable, m_lpfRowsReason);
    }
}

void AntennaAlexAlex1Tab::onMasterCheckChanged(bool checked, const QString& settingsKey)
{
    if (!m_currentMac.isEmpty()) {
        AppSettings::instance().setHardwareValue(
            m_currentMac, settingsKey, checked ? QStringLiteral("True") : QStringLiteral("False"));
        AppSettings::instance().save();
    }
    // Plan Task 14 and its fix wave (R-R3-49): the high-pass switches reach
    // the radio at once, as Thetis's setters re-apply the high-pass
    // (console.cs:18719-18803 Disable6mLNAonRX / Disable6mLNAonTX /
    // DisableHPFonTX / DisableHPFonPS / AlexHPFBypass [v2.10.3.15]).
    // In a remote window the save above goes to the Core, which applies it
    // there (scheduleRemoteHardwareApply); the window's own model has no
    // radio and does nothing.
    if (m_model && settingsKey.startsWith(QLatin1String("alex/master/"))) {
        m_model->applyAlexHpfSwitchSettings();
    }
    emit settingChanged(settingsKey, checked);
}

// ── Test seam ─────────────────────────────────────────────────────────────────

// Always compiled — NEREUS_BUILD_TESTS is set on NereusSDRLib globally.
// Used by tst_alex1_filters_tab to verify the Saturn/non-Saturn capability gate.
// isVisible() returns false if the widget itself is not shown (e.g. in tests
// where no parent window is displayed). Use !isHidden() which reflects only
// the explicit show/hide state set via setVisible(), not ancestry visibility.
bool AntennaAlexAlex1Tab::isSaturnBpf1Visible() const
{
    return m_bpf1Group && !m_bpf1Group->isHidden();
}

bool AntennaAlexAlex1Tab::isAlexHpfVisible() const
{
    return m_hpfGroup && !m_hpfGroup->isHidden();
}

// Phase 3M-4 Task 11 — IMD warning dialog auto-confirm seam for tests.
// Lets tst_setup_deltas verify both the "Cancel reverts" and the
// "OK persists" paths without driving a real modal dialog.
void AntennaAlexAlex1Tab::setImdWarningResultForTest(TestImdResult result)
{
    m_imdAutoResult = (result == TestImdResult::ConfirmOk)
                          ? ImdAutoResult::Ok
                          : ImdAutoResult::Cancel;
}

} // namespace NereusSDR
