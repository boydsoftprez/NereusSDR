// =================================================================
// src/gui/setup/hardware/AntennaAlexAlex2Tab.cpp  (NereusSDR)
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

#include "AntennaAlexAlex2Tab.h"

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace NereusSDR {

// ── Band table helpers ────────────────────────────────────────────────────────

// Alex-2 HPF band rows — 6 entries.
// Source: Thetis tpAlex2FilterControl (setup.designer.cs:25539-26857) [@501e3f5]
// Defaults decoded from NumericUpDownTS.Value initialisation:
//   1.5 MHz HPF: start {15,0,0,65536}=1.5  end {2099999,0,0,393216}=2.099999
//   6.5 MHz HPF: start {21,0,0,65536}=2.1  end {5499999,0,0,393216}=5.499999
//   9.5 MHz HPF: start {55,0,0,65536}=5.5  end {10999999,0,0,393216}=10.999999
//   13 MHz HPF:  start {11,0,0,0}=11.0     end {21999999,0,0,393216}=21.999999
//   20 MHz HPF:  start {22,0,0,0}=22.0     end {34999999,0,0,393216}=34.999999
//   6m BPF:      start {35,0,0,0}=35.0     end {6144,0,0,131072}=61.44
const std::vector<AntennaAlexAlex2Tab::HpfBandEntry>& AntennaAlexAlex2Tab::hpfBands()
{
    static const std::vector<HpfBandEntry> bands = {
        { "1.5 MHz HPF",  "1_5MHz",   1.5,       2.099999 },  // udAlex21_5HPF* [@501e3f5:26815-26857]
        { "6.5 MHz HPF",  "6_5MHz",   2.1,       5.499999 },  // udAlex26_5HPF* [@501e3f5:26730-26792]
        { "9.5 MHz HPF",  "9_5MHz",   5.5,      10.999999 },  // udAlex29_5HPF* [@501e3f5:26680-26726]
        { "13 MHz HPF",   "13MHz",   11.0,      21.999999 },  // udAlex213HPF*  [@501e3f5:26550-26643]
        { "20 MHz HPF",   "20MHz",   22.0,      34.999999 },  // udAlex220HPF*  [@501e3f5:26600-26650]
        { "6m Bypass",    "6mBP",    35.0,      61.44     },  // udAlex26BPF*   [@501e3f5:26415-26473]
    };
    return bands;
}

// Alex-2 LPF band rows — 7 entries.
// Source: Thetis tpAlex2FilterControl LPF spinboxes (setup.designer.cs:25740-26095) [@501e3f5]
// Defaults decoded from NumericUpDownTS.Value initialisation:
//   160m:  start {18,0,0,65536}=1.8         end {2,0,0,0}=2.0
//   80m:   start {35,0,0,65536}=3.5         end {4,0,0,0}=4.0
//   60/40m:start {5330,0,0,196608}=5.330    end {73,0,0,65536}=7.3
//   30/20m:start {101,0,0,65536}=10.1       end {1435,0,0,131072}=14.35
//   17/15m:start {18068,0,0,196608}=18.068  end {21450,0,0,196608}=21.45
//   12/10m:start {24890,0,0,196608}=24.890  end {297,0,0,65536}=29.7
//   6m:    start {50,0,0,0}=50.0            end {54,0,0,0}=54.0
const std::vector<AntennaAlexAlex2Tab::LpfBandEntry>& AntennaAlexAlex2Tab::lpfBands()
{
    static const std::vector<LpfBandEntry> bands = {
        { "160m",     "160m",   1.8,       2.0      },  // udAlex2160mLPF* [@501e3f5:25748-25786]
        { "80m",      "80m",    3.5,       4.0      },  // udAlex280mLPF*  [@501e3f5:25806-25836]
        { "60/40m",   "40m",    5.330,     7.3      },  // udAlex240mLPF*  [@501e3f5:25864-25896]
        { "30/20m",   "20m",   10.1,      14.35     },  // udAlex220mLPF*  [@501e3f5:26009-26037]
        { "17/15m",   "15m",   18.068,    21.45     },  // udAlex215mLPF*  [@501e3f5:25980-26008]
        { "12/10m",   "10m",   24.890,    29.7      },  // udAlex210mLPF*  [@501e3f5:26096-26130]
        { "6m/ByPass","6m",    50.0,      54.0      },  // udAlex26mLPF*   [@501e3f5:26038-26068]
    };
    return bands;
}

// ── makeFreqSpin ──────────────────────────────────────────────────────────────

// Builds a QDoubleSpinBox for MHz frequency entry with 6 decimal places.
// Mirrors Thetis NumericUpDownTS DecimalPlaces=6 (setup.designer.cs:25742) [@501e3f5]
QDoubleSpinBox* AntennaAlexAlex2Tab::makeFreqSpin(double defaultMhz, QWidget* parent)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(0.0, 200.0);
    spin->setDecimals(6);
    spin->setSingleStep(0.001);
    spin->setSuffix(QStringLiteral(" MHz"));
    spin->setValue(defaultMhz);
    return spin;
}

// ── Constructor ───────────────────────────────────────────────────────────────

AntennaAlexAlex2Tab::AntennaAlexAlex2Tab(RadioModel* model, QWidget* parent)
    : QWidget(parent), m_model(model)
{
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    scroll->setWidget(content);

    auto* outerVBox = new QVBoxLayout(this);
    outerVBox->setContentsMargins(0, 0, 0, 0);
    outerVBox->addWidget(scroll);

    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(8, 8, 8, 8);
    contentLayout->setSpacing(6);

    // ── Top status bar ────────────────────────────────────────────────────────
    // Source: Thetis lblAlex2Active (setup.designer.cs:25583-25594) [@501e3f5]
    // Text = "Active" — shown when board model has Alex-2.
    // TODO: add hasAlex2 to BoardCapabilities; for now gate on board model.
    auto* statusFrame = new QFrame(content);
    statusFrame->setFrameShape(QFrame::StyledPanel);
    statusFrame->setStyleSheet(
        QStringLiteral("QFrame { background: palette(window); border: 1px solid palette(mid); "
                       "border-radius: 3px; }"));

    auto* statusLayout = new QHBoxLayout(statusFrame);
    statusLayout->setContentsMargins(8, 4, 8, 4);
    statusLayout->setSpacing(6);

    // LED dot indicator — STUB: Phase H wires to ep6 real-time status.
    // radAlex2*led controls in Thetis are Enabled=false (setup.designer.cs:25660) [@501e3f5]
    m_statusLed = new QLabel(statusFrame);
    m_statusLed->setFixedSize(10, 10);
    m_statusLed->setStyleSheet(
        QStringLiteral("QLabel { background: #666666; border-radius: 5px; }"));

    m_statusLabel = new QLabel(tr("Alex-2 board: Not detected"), statusFrame);
    m_statusLabel->setStyleSheet(
        QStringLiteral("QLabel { color: palette(mid); }"));

    // Right-aligned "Currently selected" — STUB for Phase H.
    auto* selectedLabel = new QLabel(
        tr("Currently selected: HPF \xe2\x80\x94 \xc2\xb7 LPF \xe2\x80\x94"),
        statusFrame);
    selectedLabel->setStyleSheet(
        QStringLiteral("QLabel { color: palette(mid); font-size: 10px; }"));
    selectedLabel->setToolTip(
        tr("Live filter selection — wired to ep6 status in Phase H."));

    statusLayout->addWidget(m_statusLed);
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(selectedLabel);
    contentLayout->addWidget(statusFrame);

    // ── Two-column body ───────────────────────────────────────────────────────
    auto* colLayout = new QHBoxLayout();
    colLayout->setContentsMargins(0, 0, 0, 0);
    colLayout->setSpacing(12);

    // ── Column 1: Alex-2 HPF Bands ───────────────────────────────────────────
    // Source: Thetis panelAlex2HPFActive + HPF controls (setup.designer.cs:25539-26500) [@501e3f5]
    auto* hpfGroup = new QGroupBox(tr("Alex-2 HPF Bands"), content);
    auto* hpfVBox  = new QVBoxLayout(hpfGroup);
    hpfVBox->setContentsMargins(8, 8, 8, 8);
    hpfVBox->setSpacing(4);

    // LED indicator row — STUB (Phase H wires active-band highlight).
    // Source: panelAlex2HPFActive radAlex2*HPFled controls [@501e3f5:25630-25660]
    // All Enabled=false in Thetis — these are runtime status indicators only.
    {
        auto* ledRow = new QWidget(hpfGroup);
        auto* ledLayout = new QHBoxLayout(ledRow);
        ledLayout->setContentsMargins(0, 2, 0, 4);
        ledLayout->setSpacing(2);
        const char* const hpfLedLabels[] = { "1.5", "6.5", "9.5", "13", "20", "6m" };
        for (const char* lbl : hpfLedLabels) {
            auto* ledContainer = new QWidget(ledRow);
            auto* ledContainerLayout = new QVBoxLayout(ledContainer);
            ledContainerLayout->setContentsMargins(0, 0, 0, 0);
            ledContainerLayout->setSpacing(2);
            auto* led = new QFrame(ledContainer);
            led->setFixedSize(10, 10);
            led->setStyleSheet(
                QStringLiteral("QFrame { background: #444444; border-radius: 5px; }"));
            led->setToolTip(tr("STUB — Phase H wires live ep6 status"));
            auto* ledLabel = new QLabel(QString::fromLatin1(lbl), ledContainer);
            ledLabel->setAlignment(Qt::AlignHCenter);
            ledLabel->setStyleSheet(QStringLiteral("QLabel { font-size: 8px; }"));
            ledContainerLayout->addWidget(led, 0, Qt::AlignHCenter);
            ledContainerLayout->addWidget(ledLabel, 0, Qt::AlignHCenter);
            ledLayout->addWidget(ledContainer);
        }
        hpfVBox->addWidget(ledRow);
    }

    // Master bypass toggle.
    // Source: Thetis chkAlex2HPFBypass Text="ByPass/55 MHz BPF" (setup.designer.cs:26479-26488) [@501e3f5]
    m_hpfBypass55 = new QCheckBox(tr("ByPass / 55 MHz BPF (master)"), hpfGroup);
    hpfVBox->addWidget(m_hpfBypass55);
    connect(m_hpfBypass55, &QCheckBox::toggled, this,
            [this](bool checked) {
                onMasterCheckChanged(checked, QStringLiteral("alex2/master/bypass55MhzBpf"));
            });

    // HPF band rows (6 rows: bypass + Start + End).
    // Source: chkAlex2*BPHPF + udAlex2*HPF{Start,End} (setup.designer.cs:26228-26490) [@501e3f5]
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
        const QString enabledKey = QStringLiteral("alex2/hpf/%1/enabled").arg(slug);
        const QString startKey   = QStringLiteral("alex2/hpf/%1/start").arg(slug);
        const QString endKey     = QStringLiteral("alex2/hpf/%1/end").arg(slug);

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
    colLayout->addWidget(hpfGroup, 1);

    // ── Column 2: Alex-2 LPF Bands ───────────────────────────────────────────
    // Source: Thetis panelAlex2LPFControl (setup.designer.cs:25595-26000) [@501e3f5]
    auto* lpfGroup = new QGroupBox(tr("Alex-2 LPF Bands"), content);
    auto* lpfVBox  = new QVBoxLayout(lpfGroup);
    lpfVBox->setContentsMargins(8, 8, 8, 8);
    lpfVBox->setSpacing(4);

    // LED indicator row — STUB (Phase H wires active-band highlight).
    // Source: panelAlex2LPFActive radAlex2*LPFled (setup.designer.cs:25660-25745) [@501e3f5]
    // All Enabled=false in Thetis — these are runtime status indicators only.
    {
        auto* ledRow = new QWidget(lpfGroup);
        auto* ledLayout = new QHBoxLayout(ledRow);
        ledLayout->setContentsMargins(0, 2, 0, 4);
        ledLayout->setSpacing(2);
        const char* const lpfLedLabels[] = { "160", "80", "60-40", "30-20", "17-15", "12-10", "6m" };
        for (const char* lbl : lpfLedLabels) {
            auto* ledContainer = new QWidget(ledRow);
            auto* ledContainerLayout = new QVBoxLayout(ledContainer);
            ledContainerLayout->setContentsMargins(0, 0, 0, 0);
            ledContainerLayout->setSpacing(2);
            auto* led = new QFrame(ledContainer);
            led->setFixedSize(10, 10);
            led->setStyleSheet(
                QStringLiteral("QFrame { background: #444444; border-radius: 5px; }"));
            led->setToolTip(tr("STUB — Phase H wires live ep6 status"));
            auto* ledLabel = new QLabel(QString::fromLatin1(lbl), ledContainer);
            ledLabel->setAlignment(Qt::AlignHCenter);
            ledLabel->setStyleSheet(QStringLiteral("QLabel { font-size: 8px; }"));
            ledContainerLayout->addWidget(led, 0, Qt::AlignHCenter);
            ledContainerLayout->addWidget(ledLabel, 0, Qt::AlignHCenter);
            ledLayout->addWidget(ledContainer);
        }
        lpfVBox->addWidget(ledRow);
    }

    // Note: no master toggles for LPF — RX-only path.
    // Source: Thetis panelAlex2LPFControl has no master toggle checkboxes [@501e3f5]
    auto* lpfNote = new QLabel(
        QStringLiteral("<i>(no master toggles \xe2\x80\x94 RX-only path)</i>"), lpfGroup);
    lpfNote->setWordWrap(true);
    lpfVBox->addWidget(lpfNote);

    // LPF band rows (7 rows: Start + End).
    // Source: udAlex2*LPF{Start,End} (setup.designer.cs:25748-26095) [@501e3f5]
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
        const QString startKey = QStringLiteral("alex2/lpf/%1/start").arg(slug);
        const QString endKey   = QStringLiteral("alex2/lpf/%1/end").arg(slug);

        connect(w.start, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, startKey](double v) { onLpfSpinChanged(v, startKey); });
        connect(w.end, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
                [this, endKey](double v) { onLpfSpinChanged(v, endKey); });

        m_lpfRows.push_back(w);
    }

    lpfVBox->addWidget(lpfFormWidget);
    lpfVBox->addStretch();
    colLayout->addWidget(lpfGroup, 1);

    contentLayout->addLayout(colLayout);
    contentLayout->addStretch();
}

// ── updateBoardCapabilities ───────────────────────────────────────────────────

// Updates board-detection status bar.
// Gate: OrionMKII / Saturn / SaturnMKII → hasAlex2 = true.
// Thetis: lblAlex2Active text is "Active" when board has Alex-2 (setup.designer.cs:25583) [@501e3f5]
// TODO: add hasAlex2 cap field to BoardCapabilities in a future phase so this
// is driven by per-board data rather than board-model inference.
void AntennaAlexAlex2Tab::updateBoardCapabilities(bool hasAlex2)
{
    m_hasAlex2 = hasAlex2;

    if (hasAlex2) {
        m_statusLed->setStyleSheet(
            QStringLiteral("QLabel { background: #00cc00; border-radius: 5px; }"));
        m_statusLabel->setText(tr("Alex-2 board: Active"));
        m_statusLabel->setStyleSheet(
            QStringLiteral("QLabel { color: #00aa00; font-weight: bold; }"));
    } else {
        m_statusLed->setStyleSheet(
            QStringLiteral("QLabel { background: #666666; border-radius: 5px; }"));
        m_statusLabel->setText(tr("Alex-2 board: Not detected"));
        m_statusLabel->setStyleSheet(
            QStringLiteral("QLabel { color: palette(mid); }"));
    }
}

// ── restoreSettings ───────────────────────────────────────────────────────────

void AntennaAlexAlex2Tab::restoreSettings(const QString& macAddress)
{
    m_currentMac = macAddress;
    if (macAddress.isEmpty()) { return; }

    auto& settings = AppSettings::instance();

    // Restore master bypass toggle
    {
        const bool val = settings.hardwareValue(macAddress,
            QStringLiteral("alex2/master/bypass55MhzBpf"), QStringLiteral("False"))
            .toString() == QStringLiteral("True");
        QSignalBlocker b(m_hpfBypass55);
        m_hpfBypass55->setChecked(val);
    }

    // Restore HPF band rows
    const auto& hpf = hpfBands();
    for (std::size_t i = 0; i < m_hpfRows.size() && i < hpf.size(); ++i) {
        const QString slug = QString::fromLatin1(hpf[i].slug);
        {
            QSignalBlocker b(m_hpfRows[i].bypass);
            const bool v = settings.hardwareValue(macAddress,
                QStringLiteral("alex2/hpf/%1/enabled").arg(slug), QStringLiteral("False"))
                .toString() == QStringLiteral("True");
            m_hpfRows[i].bypass->setChecked(v);
        }
        {
            QSignalBlocker b(m_hpfRows[i].start);
            const double v = settings.hardwareValue(macAddress,
                QStringLiteral("alex2/hpf/%1/start").arg(slug),
                hpf[i].startMhz).toDouble();
            m_hpfRows[i].start->setValue(v);
        }
        {
            QSignalBlocker b(m_hpfRows[i].end);
            const double v = settings.hardwareValue(macAddress,
                QStringLiteral("alex2/hpf/%1/end").arg(slug),
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
                QStringLiteral("alex2/lpf/%1/start").arg(slug),
                lpf[i].startMhz).toDouble();
            m_lpfRows[i].start->setValue(v);
        }
        {
            QSignalBlocker b(m_lpfRows[i].end);
            const double v = settings.hardwareValue(macAddress,
                QStringLiteral("alex2/lpf/%1/end").arg(slug),
                lpf[i].endMhz).toDouble();
            m_lpfRows[i].end->setValue(v);
        }
    }
}

// ── Change slots ──────────────────────────────────────────────────────────────

void AntennaAlexAlex2Tab::onHpfCheckChanged(bool checked, const QString& settingsKey)
{
    if (!m_currentMac.isEmpty()) {
        AppSettings::instance().setHardwareValue(
            m_currentMac, settingsKey,
            checked ? QStringLiteral("True") : QStringLiteral("False"));
        AppSettings::instance().save();
    }
    emit settingChanged(settingsKey, checked);
}

void AntennaAlexAlex2Tab::onHpfSpinChanged(double value, const QString& settingsKey)
{
    if (!m_currentMac.isEmpty()) {
        AppSettings::instance().setHardwareValue(m_currentMac, settingsKey, value);
        AppSettings::instance().save();
    }
    emit settingChanged(settingsKey, value);
}

void AntennaAlexAlex2Tab::onLpfSpinChanged(double value, const QString& settingsKey)
{
    if (!m_currentMac.isEmpty()) {
        AppSettings::instance().setHardwareValue(m_currentMac, settingsKey, value);
        AppSettings::instance().save();
    }
    emit settingChanged(settingsKey, value);
}

void AntennaAlexAlex2Tab::onMasterCheckChanged(bool checked, const QString& settingsKey)
{
    if (!m_currentMac.isEmpty()) {
        AppSettings::instance().setHardwareValue(
            m_currentMac, settingsKey,
            checked ? QStringLiteral("True") : QStringLiteral("False"));
        AppSettings::instance().save();
    }
    emit settingChanged(settingsKey, checked);
}

// ── Test seam ─────────────────────────────────────────────────────────────────

// Always compiled — NEREUS_BUILD_TESTS is set on NereusSDRObjs globally.
// Returns whether the status bar reflects an "Active" (hasAlex2=true) board.
bool AntennaAlexAlex2Tab::isAlex2Active() const
{
    return m_hasAlex2;
}

} // namespace NereusSDR
