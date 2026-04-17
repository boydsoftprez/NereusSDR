// =================================================================
// src/gui/setup/TransmitSetupPages.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs
//
// Original Thetis copyright and license (preserved per GNU GPL):
//
//   Thetis is a C# implementation of a Software Defined Radio.
//   Copyright (C) 2004-2009  FlexRadio Systems
//   Copyright (C) 2010-2020  Doug Wigley (W5WC)
//   Copyright (C) 2019-2026  Richard Samphire (MW0LGE) — heavily modified
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License
//   as published by the Free Software Foundation; either version 2
//   of the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//
// Dual-Licensing Statement (applies ONLY to Richard Samphire MW0LGE's
// contributions — preserved verbatim from Thetis LICENSE-DUAL-LICENSING):
//
//   For any code originally written by Richard Samphire MW0LGE, or for
//   any modifications made by him, the copyright holder for those
//   portions (Richard Samphire) reserves the right to use, license, and
//   distribute such code under different terms, including closed-source
//   and proprietary licences, in addition to the GNU General Public
//   License granted in LICENCE. Nothing in this statement restricts any
//   rights granted to recipients under the GNU GPL.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. Structural template follows AetherSDR
//                 (ten9876/AetherSDR) Qt6 conventions.
// =================================================================

#include "TransmitSetupPages.h"
#include "gui/StyleConstants.h"

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
        "QSpinBox::up-button, QSpinBox::down-button,"
        "QDoubleSpinBox::up-button, QDoubleSpinBox::down-button"
        "  { background: #203040; border: none; }"
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
    contentLayout()->addStretch();
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
