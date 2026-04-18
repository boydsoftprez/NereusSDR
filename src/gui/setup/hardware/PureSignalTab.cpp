// =================================================================
// src/gui/setup/hardware/PureSignalTab.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/PSForm.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
// =================================================================

/*  PSForm.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2000-2025 Original authors
Copyright (C) 2020-2025 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
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

#include "PureSignalTab.h"

#include "core/BoardCapabilities.h"
#include "core/RadioDiscovery.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QSlider>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace NereusSDR {

PureSignalTab::PureSignalTab(RadioModel* model, QWidget* parent)
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
        new QLabel(tr("Board does not support PureSignal."), unsupportedPage));
    unsupportedLayout->addStretch();
    m_stack->addWidget(unsupportedPage);  // index 0

    // ── Page 1: PureSignal controls ───────────────────────────────────────────
    auto* controlsPage = new QWidget(m_stack);
    auto* controlsLayout = new QVBoxLayout(controlsPage);
    controlsLayout->setSpacing(8);

    // ── Enable + Feedback source group ────────────────────────────────────────
    auto* mainGroup = new QGroupBox(tr("PureSignal"), controlsPage);
    auto* mainForm  = new QFormLayout(mainGroup);
    mainForm->setLabelAlignment(Qt::AlignRight);

    // Enable PureSignal checkbox — cold this phase
    // Source: PSForm.cs state flag _OFF (line 94); no PS start fired in Phase 3I
    m_enableCheck = new QCheckBox(
        tr("Enable PureSignal (inactive until Phase 3I-4)"), mainGroup);
    mainForm->addRow(m_enableCheck);

    // Feedback source combo
    // Source: PSForm.cs checkLoopback_CheckedChanged (line 846) —
    //   cmaster.PSLoopback = checkLoopback.Checked
    m_feedbackSourceCombo = new QComboBox(mainGroup);
    m_feedbackSourceCombo->addItem(tr("Internal"),           0);
    m_feedbackSourceCombo->addItem(tr("External loopback"),  1);
    mainForm->addRow(tr("Feedback source:"), m_feedbackSourceCombo);

    controlsLayout->addWidget(mainGroup);

    // ── Calibration options group ─────────────────────────────────────────────
    auto* calGroup = new QGroupBox(tr("Calibration"), controlsPage);
    auto* calForm  = new QFormLayout(calGroup);
    calForm->setLabelAlignment(Qt::AlignRight);

    // Auto-calibrate on band change
    // Source: PSForm.cs AutoCalEnabled property (lines 272-278)
    m_autoCalOnBandChangeCheck = new QCheckBox(
        tr("Auto-calibrate on band change"), calGroup);
    calForm->addRow(m_autoCalOnBandChangeCheck);

    // Preserve calibration across restarts
    // Source: PSForm.cs _restoreON flag (line 93) used by btnPSRestore_Click
    m_preserveCalCheck = new QCheckBox(
        tr("Preserve calibration across restarts"), calGroup);
    calForm->addRow(m_preserveCalCheck);

    controlsLayout->addWidget(calGroup);

    // ── RX feedback attenuator group ──────────────────────────────────────────
    auto* attenGroup  = new QGroupBox(tr("RX Feedback"), controlsPage);
    auto* attenLayout = new QVBoxLayout(attenGroup);

    auto* attenRow   = new QWidget(attenGroup);
    auto* attenHBox  = new QHBoxLayout(attenRow);
    attenHBox->setContentsMargins(0, 0, 0, 0);

    // RX feedback attenuator slider 0..31 dB
    // Source: PSForm.cs AutoAttenuate / _autoattenuate (lines 291-311) —
    //   console.ATTOnTX driven by attenuate state; range 0..31 from HPSDR
    //   step-attenuator spec (31 dB max in 1 dB steps).
    m_rxFeedbackAttenSlider = new QSlider(Qt::Horizontal, attenRow);
    m_rxFeedbackAttenSlider->setRange(0, 31);
    m_rxFeedbackAttenSlider->setValue(0);
    m_rxFeedbackAttenSlider->setTickInterval(5);
    m_rxFeedbackAttenSlider->setTickPosition(QSlider::TicksBelow);

    m_rxAttenValueLabel = new QLabel(QStringLiteral("0 dB"), attenRow);
    m_rxAttenValueLabel->setMinimumWidth(48);

    attenHBox->addWidget(m_rxFeedbackAttenSlider);
    attenHBox->addWidget(m_rxAttenValueLabel);

    attenLayout->addWidget(new QLabel(tr("Attenuator (0–31 dB):"), attenGroup));
    attenLayout->addWidget(attenRow);

    controlsLayout->addWidget(attenGroup);
    controlsLayout->addStretch();

    m_stack->addWidget(controlsPage);  // index 1

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_enableCheck, &QCheckBox::toggled, this, [this](bool checked) {
        emit settingChanged(QStringLiteral("pureSignal/enabled"), checked);
    });

    connect(m_feedbackSourceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        emit settingChanged(QStringLiteral("pureSignal/feedbackSource"),
                            m_feedbackSourceCombo->itemData(idx));
    });

    connect(m_autoCalOnBandChangeCheck, &QCheckBox::toggled, this, [this](bool checked) {
        emit settingChanged(QStringLiteral("pureSignal/autoCalOnBandChange"), checked);
    });

    connect(m_preserveCalCheck, &QCheckBox::toggled, this, [this](bool checked) {
        emit settingChanged(QStringLiteral("pureSignal/preserveCalibration"), checked);
    });

    connect(m_rxFeedbackAttenSlider, &QSlider::valueChanged, this, [this](int value) {
        m_rxAttenValueLabel->setText(QStringLiteral("%1 dB").arg(value));
        emit settingChanged(QStringLiteral("pureSignal/rxFeedbackAtten"), value);
    });
}

// ── populate ──────────────────────────────────────────────────────────────────

void PureSignalTab::populate(const RadioInfo& /*info*/, const BoardCapabilities& caps)
{
    if (!caps.hasPureSignal) {
        m_stack->setCurrentIndex(0);
        return;
    }

    m_stack->setCurrentIndex(1);

    // Cap the slider range to the board attenuator range if present.
    // Source: PSForm.cs AutoAttenuate range — hardware attenuator 0..31 dB.
    if (caps.attenuator.present) {
        QSignalBlocker blocker(m_rxFeedbackAttenSlider);
        int maxDb = qBound(0, caps.attenuator.maxDb, 31);
        m_rxFeedbackAttenSlider->setRange(0, maxDb);
        m_rxAttenValueLabel->setText(
            QStringLiteral("%1 dB").arg(m_rxFeedbackAttenSlider->value()));
    }
}

// ── restoreSettings ───────────────────────────────────────────────────────────

void PureSignalTab::restoreSettings(const QMap<QString, QVariant>& settings)
{
    auto it = settings.constFind(QStringLiteral("enabled"));
    if (it != settings.constEnd()) {
        QSignalBlocker blocker(m_enableCheck);
        m_enableCheck->setChecked(it.value().toBool());
    }

    it = settings.constFind(QStringLiteral("feedbackSource"));
    if (it != settings.constEnd()) {
        const int src = it.value().toInt();
        QSignalBlocker blocker(m_feedbackSourceCombo);
        for (int i = 0; i < m_feedbackSourceCombo->count(); ++i) {
            if (m_feedbackSourceCombo->itemData(i).toInt() == src) {
                m_feedbackSourceCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    it = settings.constFind(QStringLiteral("autoCalOnBandChange"));
    if (it != settings.constEnd()) {
        QSignalBlocker blocker(m_autoCalOnBandChangeCheck);
        m_autoCalOnBandChangeCheck->setChecked(it.value().toBool());
    }

    it = settings.constFind(QStringLiteral("preserveCalibration"));
    if (it != settings.constEnd()) {
        QSignalBlocker blocker(m_preserveCalCheck);
        m_preserveCalCheck->setChecked(it.value().toBool());
    }

    it = settings.constFind(QStringLiteral("rxFeedbackAtten"));
    if (it != settings.constEnd()) {
        QSignalBlocker blocker(m_rxFeedbackAttenSlider);
        const int val = it.value().toInt();
        m_rxFeedbackAttenSlider->setValue(val);
        m_rxAttenValueLabel->setText(QStringLiteral("%1 dB").arg(val));
    }
}

} // namespace NereusSDR
