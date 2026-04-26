// =================================================================
// src/gui/setup/TransmitSetupPages.cpp  (NereusSDR)
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

#include "TransmitSetupPages.h"
#include "gui/StyleConstants.h"
#include "core/AppSettings.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QPushButton>

namespace NereusSDR {

namespace {

void applyDarkStyle(QWidget* w)
{
    w->setStyleSheet(QStringLiteral(
        "QGroupBox { color: #8090a0; font-size: 11px;"
        "  border: 1px solid #203040; border-radius: 4px;"
        "  margin-top: 8px; padding-top: 4px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }"
        "QLabel { color: #c8d8e8; }"
        "QComboBox { background: #1a2a3a; color: #c8d8e8; border: 1px solid #203040;"
        "  border-radius: 3px; padding: 2px 6px; }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: #1a2a3a; color: #c8d8e8;"
        "  selection-background-color: #00b4d8; }"
        "QSlider::groove:horizontal { background: #1a2a3a; height: 4px; border-radius: 2px; }"
        "QSlider::handle:horizontal { background: #00b4d8; width: 12px; margin: -4px 0;"
        "  border-radius: 6px; }"
        "QSlider::sub-page:horizontal { background: #00b4d8; border-radius: 2px; }"
        "QSpinBox, QDoubleSpinBox { background: #1a2a3a; color: #c8d8e8;"
        "  border: 1px solid #203040; border-radius: 3px; padding: 1px 4px; }"
        // Up/down buttons: rely on Fusion + app-level dark palette
        // (see main.cpp / AppTheme.h). Styling the subcontrols here
        // would erase the native arrow images.
        "QCheckBox { color: #c8d8e8; }"
        "QCheckBox::indicator { width: 14px; height: 14px; background: #1a2a3a;"
        "  border: 1px solid #203040; border-radius: 2px; }"
        "QCheckBox::indicator:checked { background: #00b4d8; border-color: #00b4d8; }"
        "QLineEdit { background: #1a2a3a; color: #c8d8e8; border: 1px solid #203040;"
        "  border-radius: 3px; padding: 2px 6px; }"
        "QPushButton { background: #1a2a3a; color: #c8d8e8; border: 1px solid #203040;"
        "  border-radius: 3px; padding: 3px 12px; }"
        "QPushButton:hover { background: #203040; }"
        "QPushButton:pressed { background: #00b4d8; color: #0f0f1a; }"
    ));
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// PowerPaPage
// ---------------------------------------------------------------------------

PowerPaPage::PowerPaPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Power & PA"), model, parent)
{
    buildUI();
}

void PowerPaPage::buildUI()
{
    applyDarkStyle(this);

    // --- Section: Power ---
    auto* pwrGroup = new QGroupBox(QStringLiteral("Power"), this);
    auto* pwrForm  = new QFormLayout(pwrGroup);
    pwrForm->setSpacing(6);

    m_maxPowerSlider = new QSlider(Qt::Horizontal, pwrGroup);
    m_maxPowerSlider->setRange(0, 100);
    m_maxPowerSlider->setValue(100);
    m_maxPowerSlider->setEnabled(false);  // NYI
    m_maxPowerSlider->setToolTip(QStringLiteral("Maximum TX power (0–100 W) — not yet implemented"));
    pwrForm->addRow(QStringLiteral("Max Power (W):"), m_maxPowerSlider);

    m_swrProtectionSlider = new QSlider(Qt::Horizontal, pwrGroup);
    m_swrProtectionSlider->setRange(10, 50);  // SWR * 10 for integer slider
    m_swrProtectionSlider->setValue(30);      // 3.0:1
    m_swrProtectionSlider->setEnabled(false); // NYI
    m_swrProtectionSlider->setToolTip(QStringLiteral("SWR protection threshold — not yet implemented"));
    pwrForm->addRow(QStringLiteral("SWR Protection:"), m_swrProtectionSlider);

    contentLayout()->addWidget(pwrGroup);

    // --- Section: PA ---
    auto* paGroup = new QGroupBox(QStringLiteral("PA"), this);
    auto* paForm  = new QFormLayout(paGroup);
    paForm->setSpacing(6);

    m_perBandGainLabel = new QLabel(QStringLiteral("(Per-band gain table — not yet implemented)"), paGroup);
    m_perBandGainLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic; }"));
    m_perBandGainLabel->setEnabled(false);
    paForm->addRow(QStringLiteral("Band Gain:"), m_perBandGainLabel);

    m_fanControlCombo = new QComboBox(paGroup);
    m_fanControlCombo->addItems({QStringLiteral("Off"), QStringLiteral("Low"),
                                 QStringLiteral("Med"), QStringLiteral("High"),
                                 QStringLiteral("Auto")});
    m_fanControlCombo->setCurrentText(QStringLiteral("Auto"));
    m_fanControlCombo->setEnabled(false);  // NYI
    m_fanControlCombo->setToolTip(QStringLiteral("PA fan control — not yet implemented"));
    paForm->addRow(QStringLiteral("Fan Control:"), m_fanControlCombo);

    contentLayout()->addWidget(paGroup);

    buildSwrProtectionGroup();
    buildExternalTxInhibitGroup();
    buildBlockTxAntennaGroup();
    buildHfPaGroup();

    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// PowerPaPage helpers (Tasks 9-11)
// ---------------------------------------------------------------------------

// Task 9 — SWR Protection
// From Thetis setup.designer.cs:5793-5924 [v2.10.3.13]
void PowerPaPage::buildSwrProtectionGroup()
{
    auto& s = AppSettings::instance();

    auto* group = new QGroupBox(tr("SWR Protection"), this);
    group->setObjectName(QStringLiteral("grpSWRProtectionControl"));
    auto* layout = new QFormLayout(group);
    layout->setSpacing(6);

    // chkSWRProtection — From Thetis setup.designer.cs:5913-5924 [v2.10.3.13]
    m_chkSWRProtection = new QCheckBox(tr("Enable Protection SWR >"), group);
    m_chkSWRProtection->setObjectName(QStringLiteral("chkSWRProtection"));
    // From Thetis setup.designer.cs:5922 [v2.10.3.13]
    m_chkSWRProtection->setToolTip(tr("Show a visual SWR warning in the spectral area"));
    m_chkSWRProtection->setChecked(
        s.value(QStringLiteral("SwrProtectionEnabled"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkSWRProtection, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("SwrProtectionEnabled"), on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    layout->addRow(QString(), m_chkSWRProtection);

    // udSwrProtectionLimit — From Thetis setup.designer.cs:5832-5860 [v2.10.3.13]
    // Min=1.0, Max=5.0, Increment=0.1, DecimalPlaces=1, Default=2.0 (Value=20, 65536→one decimal)
    m_udSwrProtectionLimit = new QDoubleSpinBox(group);
    m_udSwrProtectionLimit->setObjectName(QStringLiteral("udSwrProtectionLimit"));
    m_udSwrProtectionLimit->setRange(1.0, 5.0);
    m_udSwrProtectionLimit->setSingleStep(0.1);
    m_udSwrProtectionLimit->setDecimals(1);
    m_udSwrProtectionLimit->setValue(
        s.value(QStringLiteral("SwrProtectionLimit"), QStringLiteral("2.0")).toDouble());
    connect(m_udSwrProtectionLimit, &QDoubleSpinBox::valueChanged, this, [](double v) {
        AppSettings::instance().setValue(QStringLiteral("SwrProtectionLimit"), QString::number(v, 'f', 1));
    });
    layout->addRow(tr("SWR Limit:"), m_udSwrProtectionLimit);

    // chkSWRTuneProtection — From Thetis setup.designer.cs:5901-5911 [v2.10.3.13]
    m_chkSWRTuneProtection = new QCheckBox(tr("Ignore when Tune Pwr <"), group);
    m_chkSWRTuneProtection->setObjectName(QStringLiteral("chkSWRTuneProtection"));
    // From Thetis setup.designer.cs:5909 [v2.10.3.13]
    m_chkSWRTuneProtection->setToolTip(tr("Disables SWR Protection during Tune."));
    m_chkSWRTuneProtection->setChecked(
        s.value(QStringLiteral("SwrTuneProtectionEnabled"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkSWRTuneProtection, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("SwrTuneProtectionEnabled"), on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    layout->addRow(QString(), m_chkSWRTuneProtection);

    // udTunePowerSwrIgnore — From Thetis setup.designer.cs:5872-5899 [v2.10.3.13]
    // Min=5, Max=50, Increment=1, Default=35
    m_udTunePowerSwrIgnore = new QSpinBox(group);
    m_udTunePowerSwrIgnore->setObjectName(QStringLiteral("udTunePowerSwrIgnore"));
    m_udTunePowerSwrIgnore->setRange(5, 50);
    m_udTunePowerSwrIgnore->setSingleStep(1);
    m_udTunePowerSwrIgnore->setValue(
        s.value(QStringLiteral("TunePowerSwrIgnore"), QStringLiteral("35")).toInt());
    connect(m_udTunePowerSwrIgnore, &QSpinBox::valueChanged, this, [](int v) {
        AppSettings::instance().setValue(QStringLiteral("TunePowerSwrIgnore"), QString::number(v));
    });
    layout->addRow(tr("Tune Pwr (W):"), m_udTunePowerSwrIgnore);

    // chkWindBackPowerSWR — From Thetis setup.designer.cs:5809-5820 [v2.10.3.13]
    m_chkWindBackPowerSWR = new QCheckBox(tr("Reduce Pwr if protected"), group);
    m_chkWindBackPowerSWR->setObjectName(QStringLiteral("chkWindBackPowerSWR"));
    // From Thetis setup.designer.cs:5818 [v2.10.3.13]
    m_chkWindBackPowerSWR->setToolTip(tr("Winds back the power if high swr protection kicks in"));
    m_chkWindBackPowerSWR->setChecked(
        s.value(QStringLiteral("WindBackPowerSwr"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkWindBackPowerSWR, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("WindBackPowerSwr"), on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    layout->addRow(QString(), m_chkWindBackPowerSWR);

    contentLayout()->addWidget(group);
}

// Task 10 — External TX Inhibit
// From Thetis setup.designer.cs:46626-46657 [v2.10.3.13]
void PowerPaPage::buildExternalTxInhibitGroup()
{
    auto& s = AppSettings::instance();

    auto* group = new QGroupBox(tr("External TX Inhibit"), this);
    group->setObjectName(QStringLiteral("grpExtTXInhibit"));
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(6);

    // chkTXInhibit — From Thetis setup.designer.cs:46637-46646 [v2.10.3.13]
    m_chkTXInhibit = new QCheckBox(tr("Update with TX Inhibit state"), group);
    m_chkTXInhibit->setObjectName(QStringLiteral("chkTXInhibit"));
    // From Thetis setup.designer.cs:46645 [v2.10.3.13]
    m_chkTXInhibit->setToolTip(tr("Thetis will update on TX inhibit state change"));
    m_chkTXInhibit->setChecked(
        s.value(QStringLiteral("TxInhibitMonitorEnabled"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkTXInhibit, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TxInhibitMonitorEnabled"), on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    layout->addWidget(m_chkTXInhibit);

    // chkTXInhibitReverse — From Thetis setup.designer.cs:46648-46657 [v2.10.3.13]
    m_chkTXInhibitReverse = new QCheckBox(tr("Reversed logic"), group);
    m_chkTXInhibitReverse->setObjectName(QStringLiteral("chkTXInhibitReverse"));
    // From Thetis setup.designer.cs:46656 [v2.10.3.13]
    m_chkTXInhibitReverse->setToolTip(tr("Reverse the input state logic"));
    m_chkTXInhibitReverse->setChecked(
        s.value(QStringLiteral("TxInhibitMonitorReversed"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkTXInhibitReverse, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("TxInhibitMonitorReversed"), on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    layout->addWidget(m_chkTXInhibitReverse);

    contentLayout()->addWidget(group);
}

// Task 11a — Block TX on RX antennas
// NereusSDR-original label and tooltip — Thetis ships these as unlabelled
// column-header checkboxes per setup.designer.cs:6704-6724 [v2.10.3.13];
// we add accessible copy. AppSettings keys mirror AlexController (Task 3P-F).
void PowerPaPage::buildBlockTxAntennaGroup()
{
    auto& s = AppSettings::instance();

    auto* group = new QGroupBox(tr("Block TX on RX antennas"), this);
    group->setObjectName(QStringLiteral("grpBlockTxAntennas"));
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(6);

    // chkBlockTxAnt2 — From Thetis setup.designer.cs:6715-6724 [v2.10.3.13]
    // NereusSDR-original label/tooltip (Thetis has no text on these checkboxes)
    m_chkBlockTxAnt2 = new QCheckBox(tr("Block TX on Ant 2"), group);
    m_chkBlockTxAnt2->setObjectName(QStringLiteral("chkBlockTxAnt2"));
    m_chkBlockTxAnt2->setToolTip(tr("When checked, the radio cannot transmit on Antenna 2"));
    m_chkBlockTxAnt2->setChecked(
        s.value(QStringLiteral("AlexAnt2RxOnly"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkBlockTxAnt2, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("AlexAnt2RxOnly"), on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    layout->addWidget(m_chkBlockTxAnt2);

    // chkBlockTxAnt3 — From Thetis setup.designer.cs:6704-6713 [v2.10.3.13]
    // NereusSDR-original label/tooltip (Thetis has no text on these checkboxes)
    m_chkBlockTxAnt3 = new QCheckBox(tr("Block TX on Ant 3"), group);
    m_chkBlockTxAnt3->setObjectName(QStringLiteral("chkBlockTxAnt3"));
    m_chkBlockTxAnt3->setToolTip(tr("When checked, the radio cannot transmit on Antenna 3"));
    m_chkBlockTxAnt3->setChecked(
        s.value(QStringLiteral("AlexAnt3RxOnly"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkBlockTxAnt3, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("AlexAnt3RxOnly"), on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    layout->addWidget(m_chkBlockTxAnt3);

    contentLayout()->addWidget(group);
}

// Task 11b — PA Control (Disable HF PA)
// From Thetis setup.designer.cs:5780-5791 [v2.10.3.13]
void PowerPaPage::buildHfPaGroup()
{
    auto& s = AppSettings::instance();

    auto* group = new QGroupBox(tr("PA Control"), this);
    group->setObjectName(QStringLiteral("grpHfPaControl"));
    auto* layout = new QVBoxLayout(group);
    layout->setSpacing(6);

    // chkHFTRRelay — From Thetis setup.designer.cs:5780-5791 [v2.10.3.13]
    m_chkHFTRRelay = new QCheckBox(tr("Disable HF PA"), group);
    m_chkHFTRRelay->setObjectName(QStringLiteral("chkHFTRRelay"));
    // From Thetis setup.designer.cs:5789 [v2.10.3.13]
    m_chkHFTRRelay->setToolTip(tr("Disables HF PA."));
    m_chkHFTRRelay->setChecked(
        s.value(QStringLiteral("DisableHfPa"), QStringLiteral("False")).toString() == QStringLiteral("True"));
    connect(m_chkHFTRRelay, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("DisableHfPa"), on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    layout->addWidget(m_chkHFTRRelay);

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// TxProfilesPage
// ---------------------------------------------------------------------------

TxProfilesPage::TxProfilesPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("TX Profiles"), model, parent)
{
    buildUI();
}

void TxProfilesPage::buildUI()
{
    applyDarkStyle(this);

    // --- Section: Profile ---
    auto* profGroup = new QGroupBox(QStringLiteral("Profile"), this);
    auto* profLayout = new QVBoxLayout(profGroup);
    profLayout->setSpacing(6);

    m_profileListLabel = new QLabel(QStringLiteral("(Profile list — not yet implemented)"), profGroup);
    m_profileListLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic; "
        " background: #1a2a3a; border: 1px solid #203040; "
        " border-radius: 3px; padding: 8px; }"));
    m_profileListLabel->setMinimumHeight(80);
    m_profileListLabel->setEnabled(false);
    m_profileListLabel->setAlignment(Qt::AlignCenter);
    profLayout->addWidget(m_profileListLabel);

    auto* nameRow = new QHBoxLayout();
    nameRow->addWidget(new QLabel(QStringLiteral("Name:"), profGroup));
    m_nameEdit = new QLineEdit(profGroup);
    m_nameEdit->setPlaceholderText(QStringLiteral("Profile name"));
    m_nameEdit->setEnabled(false);  // NYI
    m_nameEdit->setToolTip(QStringLiteral("TX profile name — not yet implemented"));
    nameRow->addWidget(m_nameEdit, 1);
    profLayout->addLayout(nameRow);

    auto* btnRow = new QHBoxLayout();
    m_newBtn = new QPushButton(QStringLiteral("New"), profGroup);
    m_newBtn->setEnabled(false);  // NYI
    m_newBtn->setToolTip(QStringLiteral("New TX profile — not yet implemented"));
    m_newBtn->setAutoDefault(false);
    btnRow->addWidget(m_newBtn);

    m_deleteBtn = new QPushButton(QStringLiteral("Delete"), profGroup);
    m_deleteBtn->setEnabled(false);  // NYI
    m_deleteBtn->setToolTip(QStringLiteral("Delete TX profile — not yet implemented"));
    m_deleteBtn->setAutoDefault(false);
    btnRow->addWidget(m_deleteBtn);

    m_copyBtn = new QPushButton(QStringLiteral("Copy"), profGroup);
    m_copyBtn->setEnabled(false);  // NYI
    m_copyBtn->setToolTip(QStringLiteral("Copy TX profile — not yet implemented"));
    m_copyBtn->setAutoDefault(false);
    btnRow->addWidget(m_copyBtn);

    btnRow->addStretch();
    profLayout->addLayout(btnRow);

    contentLayout()->addWidget(profGroup);

    // --- Section: Compression ---
    auto* compGroup = new QGroupBox(QStringLiteral("Compression"), this);
    auto* compForm  = new QFormLayout(compGroup);
    compForm->setSpacing(6);

    m_compressorToggle = new QCheckBox(QStringLiteral("Enable compressor"), compGroup);
    m_compressorToggle->setEnabled(false);  // NYI
    m_compressorToggle->setToolTip(QStringLiteral("TX speech compressor — not yet implemented"));
    compForm->addRow(QString(), m_compressorToggle);

    m_gainSlider = new QSlider(Qt::Horizontal, compGroup);
    m_gainSlider->setRange(0, 20);
    m_gainSlider->setValue(0);
    m_gainSlider->setEnabled(false);  // NYI
    m_gainSlider->setToolTip(QStringLiteral("Compressor gain (dB) — not yet implemented"));
    compForm->addRow(QStringLiteral("Gain (dB):"), m_gainSlider);

    m_cessbToggle = new QCheckBox(QStringLiteral("CESSB"), compGroup);
    m_cessbToggle->setEnabled(false);  // NYI
    m_cessbToggle->setToolTip(QStringLiteral("Controlled Envelope SSB — not yet implemented"));
    compForm->addRow(QString(), m_cessbToggle);

    contentLayout()->addWidget(compGroup);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// SpeechProcessorPage
// ---------------------------------------------------------------------------

SpeechProcessorPage::SpeechProcessorPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Speech Processor"), model, parent)
{
    buildUI();
}

void SpeechProcessorPage::buildUI()
{
    applyDarkStyle(this);

    // --- Section: Compressor ---
    auto* compGroup = new QGroupBox(QStringLiteral("Compressor"), this);
    auto* compForm  = new QFormLayout(compGroup);
    compForm->setSpacing(6);

    m_gainSlider = new QSlider(Qt::Horizontal, compGroup);
    m_gainSlider->setRange(0, 20);
    m_gainSlider->setValue(0);
    m_gainSlider->setEnabled(false);  // NYI
    m_gainSlider->setToolTip(QStringLiteral("Speech compressor gain (dB) — not yet implemented"));
    compForm->addRow(QStringLiteral("Gain (dB):"), m_gainSlider);

    m_cessbToggle = new QCheckBox(QStringLiteral("CESSB enable"), compGroup);
    m_cessbToggle->setEnabled(false);  // NYI
    m_cessbToggle->setToolTip(QStringLiteral("Controlled Envelope SSB — not yet implemented"));
    compForm->addRow(QString(), m_cessbToggle);

    contentLayout()->addWidget(compGroup);

    // --- Section: Phase Rotator ---
    auto* prGroup = new QGroupBox(QStringLiteral("Phase Rotator"), this);
    auto* prForm  = new QFormLayout(prGroup);
    prForm->setSpacing(6);

    m_stagesSpin = new QSpinBox(prGroup);
    m_stagesSpin->setRange(0, 8);
    m_stagesSpin->setValue(0);
    m_stagesSpin->setEnabled(false);  // NYI
    m_stagesSpin->setToolTip(QStringLiteral("Phase rotator stages — not yet implemented"));
    prForm->addRow(QStringLiteral("Stages:"), m_stagesSpin);

    m_cornerFreqSlider = new QSlider(Qt::Horizontal, prGroup);
    m_cornerFreqSlider->setRange(50, 3000);
    m_cornerFreqSlider->setValue(300);
    m_cornerFreqSlider->setEnabled(false);  // NYI
    m_cornerFreqSlider->setToolTip(QStringLiteral("Phase rotator corner frequency (Hz) — not yet implemented"));
    prForm->addRow(QStringLiteral("Corner Freq (Hz):"), m_cornerFreqSlider);

    contentLayout()->addWidget(prGroup);

    // --- Section: CFC ---
    auto* cfcGroup = new QGroupBox(QStringLiteral("CFC"), this);
    auto* cfcForm  = new QFormLayout(cfcGroup);
    cfcForm->setSpacing(6);

    m_cfcToggle = new QCheckBox(QStringLiteral("Enable CFC"), cfcGroup);
    m_cfcToggle->setEnabled(false);  // NYI
    m_cfcToggle->setToolTip(QStringLiteral("Continuous Frequency Compression — not yet implemented"));
    cfcForm->addRow(QString(), m_cfcToggle);

    m_cfcProfileLabel = new QLabel(QStringLiteral("(CFC profile — not yet implemented)"), cfcGroup);
    m_cfcProfileLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic; }"));
    m_cfcProfileLabel->setEnabled(false);
    cfcForm->addRow(QStringLiteral("Profile:"), m_cfcProfileLabel);

    contentLayout()->addWidget(cfcGroup);
    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// PureSignalPage
// ---------------------------------------------------------------------------

PureSignalPage::PureSignalPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("PureSignal"), model, parent)
{
    buildUI();
}

void PureSignalPage::buildUI()
{
    applyDarkStyle(this);

    // --- Section: PureSignal ---
    auto* psGroup = new QGroupBox(QStringLiteral("PureSignal"), this);
    auto* psForm  = new QFormLayout(psGroup);
    psForm->setSpacing(6);

    m_enableToggle = new QCheckBox(QStringLiteral("Enable PureSignal"), psGroup);
    m_enableToggle->setEnabled(false);  // NYI — requires Phase 3I-4
    m_enableToggle->setToolTip(QStringLiteral("PureSignal linearization — not yet implemented (Phase 3I-4)"));
    psForm->addRow(QString(), m_enableToggle);

    m_autoCalToggle = new QCheckBox(QStringLiteral("Auto-calibrate"), psGroup);
    m_autoCalToggle->setEnabled(false);  // NYI
    m_autoCalToggle->setToolTip(QStringLiteral("PureSignal auto-calibration — not yet implemented"));
    psForm->addRow(QString(), m_autoCalToggle);

    m_feedbackDdcCombo = new QComboBox(psGroup);
    // From Thetis setup.cs: PS feedback DDC choices depend on board DDC count
    m_feedbackDdcCombo->addItems({QStringLiteral("DDC 0"), QStringLiteral("DDC 1"),
                                  QStringLiteral("DDC 2"), QStringLiteral("DDC 3"),
                                  QStringLiteral("DDC 4"), QStringLiteral("DDC 5"),
                                  QStringLiteral("DDC 6"), QStringLiteral("DDC 7")});
    m_feedbackDdcCombo->setEnabled(false);  // NYI
    m_feedbackDdcCombo->setToolTip(QStringLiteral("Feedback DDC selection — not yet implemented"));
    psForm->addRow(QStringLiteral("Feedback DDC:"), m_feedbackDdcCombo);

    m_attentionSlider = new QSlider(Qt::Horizontal, psGroup);
    m_attentionSlider->setRange(0, 31);
    m_attentionSlider->setValue(0);
    m_attentionSlider->setEnabled(false);  // NYI
    m_attentionSlider->setToolTip(QStringLiteral("PureSignal feedback attention (dB) — not yet implemented"));
    psForm->addRow(QStringLiteral("Attention (dB):"), m_attentionSlider);

    m_infoLabel = new QLabel(QStringLiteral("Status: Not active"), psGroup);
    m_infoLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #607080; font-style: italic; }"));
    m_infoLabel->setEnabled(false);
    psForm->addRow(QStringLiteral("Status:"), m_infoLabel);

    contentLayout()->addWidget(psGroup);
    contentLayout()->addStretch();
}

} // namespace NereusSDR
