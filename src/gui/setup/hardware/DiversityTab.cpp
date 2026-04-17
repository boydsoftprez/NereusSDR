// =================================================================
// src/gui/setup/hardware/DiversityTab.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/DiversityForm.cs
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

// DiversityTab.cpp
//
// Source: Thetis DiversityForm.cs
//   - chkLockAngle "Lock Phase" (line 1216): phase lock toggle
//   - chkLockR "Lock Gain" (line 1228): gain lock toggle
//   - udGainMulti NumericUpDown (lines 1177-1206): gain multiplier control
//   - labelTS4 "Phase" / labelTS3 "Gain" (lines 1240/1293): axis labels
//   - udR1/udR2 range.Maximum tied to udGainMulti.Maximum (lines 170-171)
//   - trackBarPhase1 / trackBarR1 referenced but commented-out (lines 182-183)
//
// NereusSDR maps the phase concept to a -180..+180 degree integer slider and
// the gain concept to a -60..+20 dB integer slider. The reference ADC combo
// and null-signal preset button are NereusSDR-specific surface for the
// DiversityForm analog gain/phase controls.

#include "DiversityTab.h"

#include "core/BoardCapabilities.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace NereusSDR {

DiversityTab::DiversityTab(RadioModel* model, QWidget* parent)
    : QWidget(parent), m_model(model)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);

    m_stack = new QStackedWidget(this);
    outerLayout->addWidget(m_stack);

    // ── Page 0: unsupported notice ────────────────────────────────────────────
    auto* unsupportedPage = new QWidget(m_stack);
    auto* unsupportedLayout = new QVBoxLayout(unsupportedPage);
    unsupportedLayout->addWidget(
        new QLabel(tr("Board does not support diversity reception."), unsupportedPage));
    unsupportedLayout->addStretch();
    m_stack->addWidget(unsupportedPage);  // index 0

    // ── Page 1: Diversity controls ────────────────────────────────────────────
    auto* controlsPage   = new QWidget(m_stack);
    auto* controlsLayout = new QVBoxLayout(controlsPage);
    controlsLayout->setSpacing(8);

    // ── Enable group ──────────────────────────────────────────────────────────
    auto* enableGroup = new QGroupBox(tr("Diversity"), controlsPage);
    auto* enableForm  = new QFormLayout(enableGroup);
    enableForm->setLabelAlignment(Qt::AlignRight);

    // Enable diversity checkbox
    // Source: DiversityForm.cs — chkLockAngle (line 1216) / chkLockR (line 1228)
    //   are per-axis locks; DiversityForm itself is the enable surface.
    m_enableCheck = new QCheckBox(tr("Enable diversity receiver"), enableGroup);
    enableForm->addRow(m_enableCheck);

    // Reference ADC combo
    m_referenceAdcCombo = new QComboBox(enableGroup);
    m_referenceAdcCombo->addItem(QStringLiteral("ADC0"), 0);
    m_referenceAdcCombo->addItem(QStringLiteral("ADC1"), 1);
    enableForm->addRow(tr("Reference ADC:"), m_referenceAdcCombo);

    controlsLayout->addWidget(enableGroup);

    // ── Phase group ───────────────────────────────────────────────────────────
    // Source: DiversityForm.cs labelTS4 "Phase" (line 1240); trackBarPhase1
    //   commented out (line 183). Range -180..+180 degrees (full rotation).
    auto* phaseGroup  = new QGroupBox(tr("Phase"), controlsPage);
    auto* phaseLayout = new QVBoxLayout(phaseGroup);

    auto* phaseRow  = new QWidget(phaseGroup);
    auto* phaseHBox = new QHBoxLayout(phaseRow);
    phaseHBox->setContentsMargins(0, 0, 0, 0);

    m_phaseSlider = new QSlider(Qt::Horizontal, phaseRow);
    m_phaseSlider->setRange(-180, 180);
    m_phaseSlider->setValue(0);
    m_phaseSlider->setTickInterval(45);
    m_phaseSlider->setTickPosition(QSlider::TicksBelow);

    m_phaseValueLabel = new QLabel(QStringLiteral("0°"), phaseRow);
    m_phaseValueLabel->setMinimumWidth(48);

    phaseHBox->addWidget(m_phaseSlider);
    phaseHBox->addWidget(m_phaseValueLabel);
    phaseLayout->addWidget(phaseRow);
    controlsLayout->addWidget(phaseGroup);

    // ── Gain group ────────────────────────────────────────────────────────────
    // Source: DiversityForm.cs labelTS3 "Gain" (line 1293); udGainMulti
    //   (lines 1177-1206) for gain multiplier. -60..+20 dB maps to the
    //   amplitude ratio available on two ADC paths.
    auto* gainGroup  = new QGroupBox(tr("Gain"), controlsPage);
    auto* gainLayout = new QVBoxLayout(gainGroup);

    auto* gainRow  = new QWidget(gainGroup);
    auto* gainHBox = new QHBoxLayout(gainRow);
    gainHBox->setContentsMargins(0, 0, 0, 0);

    m_gainSlider = new QSlider(Qt::Horizontal, gainRow);
    m_gainSlider->setRange(-60, 20);
    m_gainSlider->setValue(0);
    m_gainSlider->setTickInterval(10);
    m_gainSlider->setTickPosition(QSlider::TicksBelow);

    m_gainValueLabel = new QLabel(QStringLiteral("0 dB"), gainRow);
    m_gainValueLabel->setMinimumWidth(56);

    gainHBox->addWidget(m_gainSlider);
    gainHBox->addWidget(m_gainValueLabel);
    gainLayout->addWidget(gainRow);
    controlsLayout->addWidget(gainGroup);

    // ── Null signal preset button ─────────────────────────────────────────────
    m_nullSignalButton = new QPushButton(tr("Null Signal Preset"), controlsPage);
    m_nullSignalButton->setToolTip(
        tr("Sets phase and gain to preset values that approximate signal null."));
    controlsLayout->addWidget(m_nullSignalButton);

    controlsLayout->addStretch();
    m_stack->addWidget(controlsPage);  // index 1

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_enableCheck, &QCheckBox::toggled, this, [this](bool checked) {
        emit settingChanged(QStringLiteral("diversity/enabled"), checked);
    });

    connect(m_referenceAdcCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        emit settingChanged(QStringLiteral("diversity/referenceAdc"),
                            m_referenceAdcCombo->itemData(idx));
    });

    connect(m_phaseSlider, &QSlider::valueChanged, this, [this](int value) {
        m_phaseValueLabel->setText(QStringLiteral("%1°").arg(value));
        emit settingChanged(QStringLiteral("diversity/phaseDeg"), value);
    });

    connect(m_gainSlider, &QSlider::valueChanged, this, [this](int value) {
        m_gainValueLabel->setText(QStringLiteral("%1 dB").arg(value));
        emit settingChanged(QStringLiteral("diversity/gainDb"), value);
    });

    // Null signal preset: stub values (phase 0°, gain -30 dB) pending real HL2 test
    connect(m_nullSignalButton, &QPushButton::clicked, this, [this]() {
        QSignalBlocker pbPhase(m_phaseSlider);
        QSignalBlocker pbGain(m_gainSlider);
        m_phaseSlider->setValue(0);
        m_gainSlider->setValue(-30);
        m_phaseValueLabel->setText(QStringLiteral("0°"));
        m_gainValueLabel->setText(QStringLiteral("-30 dB"));
        emit settingChanged(QStringLiteral("diversity/phaseDeg"), 0);
        emit settingChanged(QStringLiteral("diversity/gainDb"), -30);
    });
}

// ── populate ──────────────────────────────────────────────────────────────────

void DiversityTab::populate(const RadioInfo& /*info*/, const BoardCapabilities& caps)
{
    // Require diversity hardware AND at least 2 ADCs
    if (!caps.hasDiversityReceiver || caps.adcCount < 2) {
        m_stack->setCurrentIndex(0);
        return;
    }

    m_stack->setCurrentIndex(1);

    // Populate reference ADC combo from the actual ADC count
    {
        QSignalBlocker blocker(m_referenceAdcCombo);
        m_referenceAdcCombo->clear();
        for (int i = 0; i < caps.adcCount; ++i) {
            m_referenceAdcCombo->addItem(
                QStringLiteral("ADC%1").arg(i), i);
        }
    }
}

// ── restoreSettings ───────────────────────────────────────────────────────────

void DiversityTab::restoreSettings(const QMap<QString, QVariant>& settings)
{
    auto it = settings.constFind(QStringLiteral("enabled"));
    if (it != settings.constEnd()) {
        QSignalBlocker blocker(m_enableCheck);
        m_enableCheck->setChecked(it.value().toBool());
    }

    it = settings.constFind(QStringLiteral("phaseDeg"));
    if (it != settings.constEnd()) {
        QSignalBlocker blocker(m_phaseSlider);
        const int val = it.value().toInt();
        m_phaseSlider->setValue(val);
        m_phaseValueLabel->setText(QStringLiteral("%1°").arg(val));
    }

    it = settings.constFind(QStringLiteral("gainDb"));
    if (it != settings.constEnd()) {
        QSignalBlocker blocker(m_gainSlider);
        const int val = it.value().toInt();
        m_gainSlider->setValue(val);
        m_gainValueLabel->setText(QStringLiteral("%1 dB").arg(val));
    }

    it = settings.constFind(QStringLiteral("referenceAdc"));
    if (it != settings.constEnd()) {
        QSignalBlocker blocker(m_referenceAdcCombo);
        const int adcIdx = it.value().toInt();
        for (int i = 0; i < m_referenceAdcCombo->count(); ++i) {
            if (m_referenceAdcCombo->itemData(i).toInt() == adcIdx) {
                m_referenceAdcCombo->setCurrentIndex(i);
                break;
            }
        }
    }
}

} // namespace NereusSDR
