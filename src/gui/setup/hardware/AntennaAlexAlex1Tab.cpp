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

#include "core/AppSettings.h"
#include "core/BoardCapabilities.h"
#include "core/HpsdrModel.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
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
        { "1.5 MHz HPF",  "1_5MHz",   1.8,      6.499999 },  // udAlex1_5HPF*  [@501e3f5:23784-23825]
        { "6.5 MHz HPF",  "6_5MHz",   6.5,      9.499999 },  // udAlex6_5HPF*  [@501e3f5:23866-23907]
        { "9.5 MHz HPF",  "9_5MHz",   9.5,     12.999999 },  // udAlex9_5HPF*  [@501e3f5:23948-23989]
        { "13 MHz HPF",   "13MHz",   13.0,     19.999999 },  // udAlex13HPF*   [@501e3f5:24243-24049]
        { "20 MHz HPF",   "20MHz",   20.0,     49.999999 },  // udAlex20HPF*   [@501e3f5:24079-24019]
        { "6m Bypass",    "6mBP",    50.0,     61.44     },  // udAlex6BPF*    [@501e3f5:24272-24340]
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

// ── buildLpfColumn ────────────────────────────────────────────────────────────

// Builds the LPF band rows (Start | End) inside box.
// From Thetis tpAlexFilterControl LPF controls (setup.designer.cs:23414-23435) [@501e3f5]
void AntennaAlexAlex1Tab::buildLpfColumn(QGroupBox* box,
                                         const std::vector<LpfBandEntry>& bands,
                                         std::vector<LpfRowWidgets>& rows)
{
    auto* formLayout = new QFormLayout();
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    formLayout->setHorizontalSpacing(6);
    formLayout->setVerticalSpacing(4);

    rows.clear();
    rows.reserve(bands.size());

    for (const LpfBandEntry& band : bands) {
        LpfRowWidgets w;
        w.start = makeFreqSpin(band.startMhz, box);
        w.end   = makeFreqSpin(band.endMhz, box);

        auto* rowWidget = new QWidget(box);
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);
        rowLayout->addWidget(w.start);
        rowLayout->addWidget(w.end);

        formLayout->addRow(tr(band.label), rowWidget);

        const QString slug = QString::fromLatin1(band.slug);
        const QString startKey = QStringLiteral("alex/lpf/%1/start").arg(slug);
        const QString endKey   = QStringLiteral("alex/lpf/%1/end").arg(slug);

        connect(w.start, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, startKey](double v) { onLpfSpinChanged(v, startKey); });
        connect(w.end, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, endKey](double v) { onLpfSpinChanged(v, endKey); });

        rows.push_back(w);
    }

    auto* boxLayout = qobject_cast<QVBoxLayout*>(box->layout());
    if (!boxLayout) {
        boxLayout = new QVBoxLayout(box);
    }
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
    auto* hpfVBox  = new QVBoxLayout(hpfGroup);
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

    auto wireMaster = [this](QCheckBox* chk, const QString& key) {
        connect(chk, &QCheckBox::toggled, this,
                [this, key](bool checked) { onMasterCheckChanged(checked, key); });
    };
    wireMaster(m_hpfBypass,        QStringLiteral("alex/master/hpfBypass"));
    wireMaster(m_hpfBypassOnTx,    QStringLiteral("alex/master/hpfBypassOnTx"));
    wireMaster(m_hpfBypassOnPs,    QStringLiteral("alex/master/hpfBypassOnPs"));
    wireMaster(m_disable6mLnaOnTx, QStringLiteral("alex/master/disable6mLnaOnTx"));
    wireMaster(m_disable6mLnaOnRx, QStringLiteral("alex/master/disable6mLnaOnRx"));

    // HPF band rows
    auto* hpfFormWidget = new QWidget(hpfGroup);
    auto* hpfFormLayout = new QFormLayout(hpfFormWidget);
    hpfFormLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    hpfFormLayout->setHorizontalSpacing(6);
    hpfFormLayout->setVerticalSpacing(4);

    m_hpfRows.reserve(hpfBands().size());
    for (const HpfBandEntry& band : hpfBands()) {
        HpfRowWidgets w;
        w.bypass = new QCheckBox(hpfFormWidget);
        w.start  = makeFreqSpin(band.startMhz, hpfFormWidget);
        w.end    = makeFreqSpin(band.endMhz,   hpfFormWidget);

        auto* rowWidget = new QWidget(hpfFormWidget);
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);
        rowLayout->addWidget(w.bypass);
        rowLayout->addWidget(w.start);
        rowLayout->addWidget(w.end);

        hpfFormLayout->addRow(tr(band.label), rowWidget);

        const QString slug = QString::fromLatin1(band.slug);
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
    // Note: "TX-side filters always engaged when keyed" — no master toggle in Thetis.
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
    for (const LpfBandEntry& band : lpfBands()) {
        LpfRowWidgets w;
        w.start = makeFreqSpin(band.startMhz, lpfFormWidget);
        w.end   = makeFreqSpin(band.endMhz,   lpfFormWidget);

        auto* rowWidget = new QWidget(lpfFormWidget);
        auto* rowLayout = new QHBoxLayout(rowWidget);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(4);
        rowLayout->addWidget(w.start);
        rowLayout->addWidget(w.end);

        lpfFormLayout->addRow(tr(band.label), rowWidget);

        const QString slug = QString::fromLatin1(band.slug);
        const QString startKey = QStringLiteral("alex/lpf/%1/start").arg(slug);
        const QString endKey   = QStringLiteral("alex/lpf/%1/end").arg(slug);

        connect(w.start, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, startKey](double v) { onLpfSpinChanged(v, startKey); });
        connect(w.end, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, endKey](double v) { onLpfSpinChanged(v, endKey); });

        m_lpfRows.push_back(w);
    }

    lpfVBox->addWidget(lpfFormWidget);
    lpfVBox->addStretch();
    colLayout->addWidget(lpfGroup);

    // ── Column 3: Saturn BPF1 Bands ───────────────────────────────────────────
    // Source: spec §7; same band-edge shape as Alex HPF.
    // Gated on Saturn / SaturnMKII board — hide for all other boards.
    // Note: Thetis shows BPF1 always; NereusSDR gates on capability per spec.
    m_bpf1Group = new QGroupBox(tr("Saturn BPF1 Bands"), content);
    auto* bpf1VBox = new QVBoxLayout(m_bpf1Group);
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

    m_bpf1Rows.reserve(hpfBands().size());
    for (const HpfBandEntry& band : hpfBands()) {
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
}

// ── updateBoardCapabilities ───────────────────────────────────────────────────

// Shows/hides the Saturn BPF1 column based on the connected board.
// Gate: Saturn (ANAN-G2 / G2-1K) or SaturnMKII only.
// From Thetis spec §7 — "spin from Thetis", Thetis shows always; we gate on capability.
void AntennaAlexAlex1Tab::updateBoardCapabilities(bool isSaturnBoard)
{
    m_bpf1Group->setVisible(isSaturnBoard);
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
            m_lpfRows[i].start->setValue(v);
        }
        {
            QSignalBlocker b(m_lpfRows[i].end);
            const double v = settings.hardwareValue(macAddress,
                QStringLiteral("alex/lpf/%1/end").arg(slug),
                lpf[i].endMhz).toDouble();
            m_lpfRows[i].end->setValue(v);
        }
    }

    // Restore BPF1 band rows (uses same band slugs as HPF)
    for (std::size_t i = 0; i < m_bpf1Rows.size() && i < hpf.size(); ++i) {
        const QString slug = QString::fromLatin1(hpf[i].slug);
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
                hpf[i].startMhz).toDouble();
            m_bpf1Rows[i].start->setValue(v);
        }
        {
            QSignalBlocker b(m_bpf1Rows[i].end);
            const double v = settings.hardwareValue(macAddress,
                QStringLiteral("alex/bpf1/%1/end").arg(slug),
                hpf[i].endMhz).toDouble();
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
    emit settingChanged(settingsKey, checked);
}

void AntennaAlexAlex1Tab::onHpfSpinChanged(double value, const QString& settingsKey)
{
    if (!m_currentMac.isEmpty()) {
        AppSettings::instance().setHardwareValue(m_currentMac, settingsKey, value);
        AppSettings::instance().save();
    }
    emit settingChanged(settingsKey, value);
}

void AntennaAlexAlex1Tab::onLpfSpinChanged(double value, const QString& settingsKey)
{
    if (!m_currentMac.isEmpty()) {
        AppSettings::instance().setHardwareValue(m_currentMac, settingsKey, value);
        AppSettings::instance().save();
    }
    emit settingChanged(settingsKey, value);
}

void AntennaAlexAlex1Tab::onBpf1CheckChanged(bool checked, const QString& settingsKey)
{
    if (!m_currentMac.isEmpty()) {
        AppSettings::instance().setHardwareValue(
            m_currentMac, settingsKey, checked ? QStringLiteral("True") : QStringLiteral("False"));
        AppSettings::instance().save();
    }
    emit settingChanged(settingsKey, checked);
}

void AntennaAlexAlex1Tab::onBpf1SpinChanged(double value, const QString& settingsKey)
{
    if (!m_currentMac.isEmpty()) {
        AppSettings::instance().setHardwareValue(m_currentMac, settingsKey, value);
        AppSettings::instance().save();
    }
    emit settingChanged(settingsKey, value);
}

void AntennaAlexAlex1Tab::onMasterCheckChanged(bool checked, const QString& settingsKey)
{
    if (!m_currentMac.isEmpty()) {
        AppSettings::instance().setHardwareValue(
            m_currentMac, settingsKey, checked ? QStringLiteral("True") : QStringLiteral("False"));
        AppSettings::instance().save();
    }
    emit settingChanged(settingsKey, checked);
}

// ── Test seam ─────────────────────────────────────────────────────────────────

// Always compiled — NEREUS_BUILD_TESTS is set on NereusSDRObjs globally.
// Used by tst_alex1_filters_tab to verify the Saturn/non-Saturn capability gate.
// isVisible() returns false if the widget itself is not shown (e.g. in tests
// where no parent window is displayed). Use !isHidden() which reflects only
// the explicit show/hide state set via setVisible(), not ancestry visibility.
bool AntennaAlexAlex1Tab::isSaturnBpf1Visible() const
{
    return m_bpf1Group && !m_bpf1Group->isHidden();
}

} // namespace NereusSDR
