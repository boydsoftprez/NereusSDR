// =================================================================
// src/gui/setup/hardware/PaCalibrationTab.cpp  (NereusSDR)
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

// PaCalibrationTab.cpp
//
// Source: Thetis Setup.cs
//   - grpGainByBandPA / nudPAProfileGain_ValueChanged (lines 1796, 24135):
//       per-band gain correction numeric-up-down per PA profile
//   - nudMaxPowerForBandPA (line 24048): per-band target/max power
//   - comboPAProfile / updatePAProfileCombo() (lines 1963-1982, 23784-23786):
//       profile selector dropdown populated from saved PAProfile entries
//   - chkPA160..chkPA10 (line 9969): per-band PA enable flags fed into cal run
//   - chkPANewCal (line 9982): selects CalibratePAGain2 vs CalibratePAGain
//   - udPACalPower (line 12278): calibration power spinbox
//
// PA Calibration is COLD this phase — buttons emit settingChanged but do
// NOT invoke console.CalibratePAGain(). Backend hookup deferred to Phase 3I-1.

#include "PaCalibrationTab.h"

#include "core/BoardCapabilities.h"
#include "core/RadioDiscovery.h"
#include "models/Band.h"
#include "models/RadioModel.h"

#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

namespace NereusSDR {

// ── Band row labels (14-band) ─────────────────────────────────────────────────
static QStringList paCalBandLabels()
{
    QStringList labels;
    labels.reserve(static_cast<int>(Band::Count));
    for (int i = 0; i < static_cast<int>(Band::Count); ++i) {
        labels << bandLabel(static_cast<Band>(i));
    }
    return labels;
}

// ── Constructor ───────────────────────────────────────────────────────────────

PaCalibrationTab::PaCalibrationTab(RadioModel* model, QWidget* parent)
    : QWidget(parent), m_model(model)
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(8, 8, 8, 8);
    outerLayout->setSpacing(8);

    const QStringList bands = paCalBandLabels();
    const int bandCount = bands.size();

    // ── PA Profile combo ──────────────────────────────────────────────────────
    // Source: Setup.cs comboPAProfile / updatePAProfileCombo() (line 1963)
    auto* profileGroup = new QGroupBox(tr("PA Profile"), this);
    auto* profileHBox  = new QHBoxLayout(profileGroup);

    profileHBox->addWidget(new QLabel(tr("Profile:"), profileGroup));
    m_paProfileCombo = new QComboBox(profileGroup);
    m_paProfileCombo->addItem(QStringLiteral("ANAN-100D-default"));
    m_paProfileCombo->addItem(QStringLiteral("ANAN-200D-default"));
    m_paProfileCombo->addItem(tr("Custom"));
    profileHBox->addWidget(m_paProfileCombo);
    profileHBox->addStretch();

    outerLayout->addWidget(profileGroup);

    // ── Per-band tables in a scroll area ─────────────────────────────────────
    auto* tablesScroll = new QScrollArea(this);
    tablesScroll->setWidgetResizable(true);

    auto* tablesContainer = new QWidget(tablesScroll);
    auto* tablesLayout    = new QHBoxLayout(tablesContainer);
    tablesLayout->setSpacing(12);

    // Per-band target power table (14 rows × 1 col)
    // Source: Setup.cs nudMaxPowerForBandPA (line 24048) — per-band max/target power
    auto* targetGroup = new QGroupBox(tr("Target Power"), tablesContainer);
    auto* targetVBox  = new QVBoxLayout(targetGroup);

    m_targetPowerTable = new QTableWidget(bandCount, 1, targetGroup);
    m_targetPowerTable->setHorizontalHeaderLabels({tr("Target (W)")});
    m_targetPowerTable->setVerticalHeaderLabels(bands);
    m_targetPowerTable->horizontalHeader()->setStretchLastSection(true);
    m_targetPowerTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_targetPowerTable->setMinimumHeight(300);

    // Default target power 100 W per band
    for (int row = 0; row < bandCount; ++row) {
        auto* item = new QTableWidgetItem(QStringLiteral("100"));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_targetPowerTable->setItem(row, 0, item);
    }
    targetVBox->addWidget(m_targetPowerTable);
    tablesLayout->addWidget(targetGroup);

    // Per-band gain correction table (14 rows × 1 col)
    // Source: Setup.cs nudPAProfileGain_ValueChanged (line 24135) —
    //   per-band gain correction (dB) applied to PA profile
    auto* gainGroup = new QGroupBox(tr("Gain Correction"), tablesContainer);
    auto* gainVBox  = new QVBoxLayout(gainGroup);

    m_gainCorrectionTable = new QTableWidget(bandCount, 1, gainGroup);
    m_gainCorrectionTable->setHorizontalHeaderLabels({tr("Correction (dB)")});
    m_gainCorrectionTable->setVerticalHeaderLabels(bands);
    m_gainCorrectionTable->horizontalHeader()->setStretchLastSection(true);
    m_gainCorrectionTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_gainCorrectionTable->setMinimumHeight(300);

    // Default correction 0 dB per band
    for (int row = 0; row < bandCount; ++row) {
        auto* item = new QTableWidgetItem(QStringLiteral("0.0"));
        item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_gainCorrectionTable->setItem(row, 0, item);
    }
    gainVBox->addWidget(m_gainCorrectionTable);
    tablesLayout->addWidget(gainGroup);

    tablesScroll->setWidget(tablesContainer);
    outerLayout->addWidget(tablesScroll);

    // ── Calibration action buttons ────────────────────────────────────────────
    auto* buttonRow   = new QWidget(this);
    auto* buttonHBox  = new QHBoxLayout(buttonRow);
    buttonHBox->setContentsMargins(0, 0, 0, 0);

    // Step attenuator calibration (shown only when caps.hasStepAttenuatorCal)
    // Source: Setup.cs udPACalPower (line 12278) — visible = !b when using new cal
    m_stepAttenCalButton = new QPushButton(tr("Step Attenuator Calibration…"), buttonRow);
    m_stepAttenCalButton->setVisible(false); // shown in populate() if caps present
    buttonHBox->addWidget(m_stepAttenCalButton);

    // Auto-calibrate (cold — no backend action this phase)
    // Source: Setup.cs chkPANewCal / CalibratePAGain (line 9982) — cold stub
    m_autoCalibrateButton = new QPushButton(tr("Auto-Calibrate PA"), buttonRow);
    m_autoCalibrateButton->setToolTip(
        tr("Triggers PA gain calibration. Backend hookup deferred to Phase 3I-1."));
    buttonHBox->addWidget(m_autoCalibrateButton);
    buttonHBox->addStretch();

    outerLayout->addWidget(buttonRow);

    // ── Connections ───────────────────────────────────────────────────────────
    connect(m_paProfileCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int /*idx*/) {
        emit settingChanged(QStringLiteral("paCalibration/profile"),
                            m_paProfileCombo->currentText());
    });

    connect(m_targetPowerTable, &QTableWidget::itemChanged, this,
            [this](QTableWidgetItem* item) {
        if (!item) { return; }
        QString key = QStringLiteral("paCalibration/targetPower/%1")
                          .arg(item->row());
        emit settingChanged(key, item->text().toDouble());
    });

    connect(m_gainCorrectionTable, &QTableWidget::itemChanged, this,
            [this](QTableWidgetItem* item) {
        if (!item) { return; }
        QString key = QStringLiteral("paCalibration/gainCorrection/%1")
                          .arg(item->row());
        emit settingChanged(key, item->text().toDouble());
    });

    connect(m_stepAttenCalButton, &QPushButton::clicked, this, [this]() {
        emit settingChanged(QStringLiteral("paCalibration/triggerStepAttenCal"), true);
    });

    connect(m_autoCalibrateButton, &QPushButton::clicked, this, [this]() {
        emit settingChanged(QStringLiteral("paCalibration/triggerAutoCal"), true);
    });
}

// ── populate ──────────────────────────────────────────────────────────────────

void PaCalibrationTab::populate(const RadioInfo& /*info*/, const BoardCapabilities& caps)
{
    // Show/hide step-attenuator calibration button
    // Source: Setup.cs udPACalPower.Visible (line 12278) — gated on cal feature
    m_stepAttenCalButton->setVisible(caps.hasStepAttenuatorCal);

    // Select the appropriate default profile based on board name
    // Source: Setup.cs comboPAProfile default (line 1963)
    if (caps.hasPaProfile) {
        QSignalBlocker blocker(m_paProfileCombo);
        m_paProfileCombo->setCurrentIndex(0); // ANAN-100D-default as default
    }
}

// ── restoreSettings ───────────────────────────────────────────────────────────

void PaCalibrationTab::restoreSettings(const QMap<QString, QVariant>& settings)
{
    // Restore PA profile combo
    auto it = settings.constFind(QStringLiteral("profile"));
    if (it != settings.constEnd()) {
        const int idx = it.value().toInt();
        QSignalBlocker blocker(m_paProfileCombo);
        if (idx >= 0 && idx < m_paProfileCombo->count()) {
            m_paProfileCombo->setCurrentIndex(idx);
        }
    }

    // Restore per-band target power: keys like "targetPower[<bandKeyName>]"
    {
        QSignalBlocker blocker(m_targetPowerTable);
        for (auto mit = settings.constBegin(); mit != settings.constEnd(); ++mit) {
            if (!mit.key().startsWith(QStringLiteral("targetPower["))) { continue; }
            const QString bandName = mit.key().mid(12, mit.key().size() - 13);
            for (int row = 0; row < m_targetPowerTable->rowCount(); ++row) {
                if (bandKeyName(static_cast<Band>(row)) != bandName) { continue; }
                if (auto* item = m_targetPowerTable->item(row, 0)) {
                    item->setText(mit.value().toString());
                }
                break;
            }
        }
    }

    // Restore per-band gain correction: keys like "gainCorrection[<bandKeyName>]"
    {
        QSignalBlocker blocker(m_gainCorrectionTable);
        for (auto mit = settings.constBegin(); mit != settings.constEnd(); ++mit) {
            if (!mit.key().startsWith(QStringLiteral("gainCorrection["))) { continue; }
            const QString bandName = mit.key().mid(15, mit.key().size() - 16);
            for (int row = 0; row < m_gainCorrectionTable->rowCount(); ++row) {
                if (bandKeyName(static_cast<Band>(row)) != bandName) { continue; }
                if (auto* item = m_gainCorrectionTable->item(row, 0)) {
                    item->setText(mit.value().toString());
                }
                break;
            }
        }
    }
}

} // namespace NereusSDR
